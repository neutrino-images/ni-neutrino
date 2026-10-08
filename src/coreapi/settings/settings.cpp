/*
 * settings.cpp - reading and writing the box settings
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "settings.h"

#include "coreapi/base/apply.h"
#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/eventbus.h"
#include "couple.h"
#include "settingstable.h"

#include <algorithm>
#include <mutex>
#include <set>
#include <vector>

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/stat.h>
#include <sys/statfs.h>

namespace coreapi
{
namespace settings
{

namespace
{

std::string decimal(long value)
{
	char out[32];
	snprintf(out, sizeof(out), "%ld", value);
	return std::string(out);
}

// The kinds whose value travels as text: the rest are numbers.
bool holdsText(const Descriptor &d)
{
	return d.type == ValueType::String || d.type == ValueType::Color;
}

/* Strict, because the whole of the value is what was offered: a parse that
   stopped at the first character it did not like would take "1x" as 1 and report
   a setting nobody asked for as set. */
bool parseNumber(const std::string &v, long &out)
{
	if (v.empty())
		return false;
	errno = 0;
	char *end = NULL;
	long n = strtol(v.c_str(), &end, 10);
	if (errno != 0 || end == v.c_str() || *end != '\0')
		return false;
	out = n;
	return true;
}

/* What the settings file can carry on one line. The struct holds a value as text
   of no length in particular and the file has no length either, so this ceiling
   is one this layer picks: the longest value any row holds is a path. */
const size_t kMaxValueBytes = 4096;

/* What a value may be whatever kind the row is. A zero byte is the end of the
   string to every call under this, so a value carrying one is a different value
   from the one that was offered. */
Result<void> allowedAtAll(const std::string &value)
{
	if (value.find('\0') != std::string::npos)
		return fail(Status::InvalidArgument, ErrorCode::ValueHasZeroByte,
			    "the value carries a zero byte, which the calls below it would read as its end");
	if (value.size() > kMaxValueBytes)
		return fail(Status::InvalidArgument, ErrorCode::ValueTooLong,
			    "the value is longer than " + decimal((long) kMaxValueBytes) + " bytes");
	return ok();
}

/* What text may be, which is what one line of the settings file gives back
   unchanged. The program writes a setting as its key, a separator and the value,
   ends the line there, and reads it back by splitting at the first separator and
   cutting the rest off at the first number sign. Neither side escapes anything,
   so three kinds of byte do not survive the round trip:

   the byte that ends a line ends the value, and what follows it is read as a
   setting of its own under whatever key it names, which is any key the program
   loads, both pin rows and every credential among them;

   a number sign takes the rest of the line with it;

   a space at either end is written and read back while nothing that shows the
   value can show it, so two settings that differ look the same.

   The other control bytes go with the first. The separator needs no rule of its
   own: the split is at the first one, so a later one is part of the value. */
Result<void> allowedAsText(const std::string &value)
{
	for (size_t i = 0; i < value.size(); ++i)
	{
		const unsigned char c = (unsigned char) value[i];
		if (c < 0x20 || c == 0x7f)
			return fail(Status::InvalidArgument, ErrorCode::BadString,
				    "the setting takes one line of text and no control byte in it");
		if (c == '#')
			return fail(Status::InvalidArgument, ErrorCode::BadString,
				    "the setting cannot hold a number sign, which is where the value is cut off when it is read back");
	}

	if (!value.empty() && (value[0] == ' ' || value[value.size() - 1] == ' '))
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the setting does not keep a space at either end of its value");

	return ok();
}

/* The file system types the recording and update screens refuse a folder on: flash,
   which wears out under a recording, and memory, which a restart empties. */
const long kFsRamfs = 0x858458f6L;
const long kFsTmpfs = 0x1021994L;
const long kFsJffs2 = 0x72b6L;

bool endsWithNoCase(const std::string &name, const std::string &ending)
{
	if (name.size() < ending.size())
		return false;
	const size_t at = name.size() - ending.size();
	for (size_t i = 0; i < ending.size(); ++i)
	{
		if (tolower((unsigned char) name[at + i]) != tolower((unsigned char) ending[i]))
			return false;
	}
	return true;
}

bool hasExtension(const std::string &name, const char *list)
{
	std::string rest = list;
	while (!rest.empty())
	{
		const size_t comma = rest.find(',');
		const std::string one = rest.substr(0, comma);
		if (!one.empty() && endsWithNoCase(name, "." + one))
			return true;
		if (comma == std::string::npos)
			break;
		rest = rest.substr(comma + 1);
	}
	return false;
}

} // anonymous namespace

bool fileSystemAllowed(MustExist level, long fs_type)
{
	const bool memory = fs_type == kFsRamfs || fs_type == kFsTmpfs;
	if (fs_type == kFsJffs2)
		return level == MustExist::No || level == MustExist::Yes;
	return !(memory && level == MustExist::YesNotTmpfs);
}

namespace
{

/* The folder test the screens made through the folder browser: it exists, and the
   file system under it is one the rule allows. A failed statfs refuses, as the
   screen's own test did. */
Result<void> existsAsRequired(const TextRule &rule, const std::string &value)
{
	struct stat st;
	if (stat(value.c_str(), &st) != 0)
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the setting names a place that does not exist on the box");

	if (rule.kind == TextKind::Directory && !S_ISDIR(st.st_mode))
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the setting names a folder and this is not one");
	if (rule.kind == TextKind::File && !S_ISREG(st.st_mode))
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the setting names a file and this is not one");

	if (rule.must_exist == MustExist::Yes)
		return ok();

	struct statfs fs;
	if (statfs(value.c_str(), &fs) != 0)
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the file system under the setting's place cannot be told");
	if (!fileSystemAllowed(rule.must_exist, (long) fs.f_type))
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the setting cannot be on this kind of file system");
	return ok();
}

/* A list of texts and a list of records travel as one text, a line to each text
   or record and a tab between the members of a record. The two separators are
   the control bytes the text rule above refuses inside a value, so nothing a
   member can hold is mistaken for one, and the text a list is read as is the
   text it is written back from. An empty text is the empty list: no list can
   have an empty text in it, since an empty line would be indistinguishable
   from the end of the text. */
const size_t kMaxListItems = 1000;

bool splitOn(const std::string &text, char sep, std::vector<std::string> &out)
{
	out.clear();
	size_t from = 0;
	for (;;)
	{
		const size_t at = text.find(sep, from);
		if (at == std::string::npos)
		{
			out.push_back(text.substr(from));
			return true;
		}
		out.push_back(text.substr(from, at - from));
		from = at + 1;
	}
}

std::string joinOn(const std::vector<std::string> &items, char sep)
{
	std::string out;
	for (size_t i = 0; i < items.size(); ++i)
	{
		if (i > 0)
			out += sep;
		out += items[i];
	}
	return out;
}

/* One member of a record, held to what its field says it is. The text rule of a
   String is the one every row's text is held to. */
Result<void> allowedAsMember(const RecordField &f, const std::string &v)
{
	Result<void> shape = allowedAtAll(v);
	if (!shape.ok())
		return shape;

	if (f.type == ValueType::String)
		return allowedAsText(v);

	long n = 0;
	if (!parseNumber(v, n))
		return fail(Status::InvalidArgument, ErrorCode::NotANumber,
			    "a member of the record takes a whole number");
	if (f.type == ValueType::Bool)
	{
		if (n != 0 && n != 1)
			return fail(Status::InvalidArgument, ErrorCode::OutOfRange,
				    "a member of the record takes 0 or 1");
		return ok();
	}
	if (n < f.min || n > f.max)
		return fail(Status::InvalidArgument, ErrorCode::OutOfRange,
			    "a member of the record takes " + decimal(f.min) + " to " + decimal(f.max));
	/* Nought stays legal where the bounds allow it: a record that holds no key
	   keeps that as nought, which no remote control sends. Every other code is held
	   to what a key row is held to, so that a member cannot store a code the input
	   layer never delivers. */
	if (f.type == ValueType::Key && n != 0 && !keySource().known(n))
		return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
			    "a member of the record takes the code of a key a remote control can send, or no key");
	return ok();
}

/* A secret row answers no value of its own, and its declared default is one: the
   tables state the default the program falls back to because a check holds them
   to it, so the withholding is here rather than there. */
void withhold(Descriptor &d)
{
	if (!d.secret)
		return;
	d.default_string = "";
	d.default_int = 0;
}

/* Whether the row allows the value, which is not the question the store asks.
   That one is whether the field can hold it, and it is narrower. Both have to
   refuse what they refuse and neither stands in for the other, because a row's
   bounds can be wider than its field and a field is wider than most rows'
   bounds. */
Result<void> allowedByRow(const Descriptor &d, long value, const ValueLookup *now)
{
	switch (d.type)
	{
		case ValueType::Bool:
			/* The two values the type itself has, rather than the row's
			   bounds: nothing holds a Bool row to declaring any, and the rows
			   of other kinds beside it leave both at zero. */
			if (value != 0 && value != 1)
				return fail(Status::InvalidArgument, ErrorCode::OutOfRange,
					    "the setting takes 0 or 1");
			return ok();

		case ValueType::Int:
		{
			// Every value the row names in words is taken beside the bounds.
			std::string also;
			for (size_t i = 0; d.values != NULL && i < d.value_count; ++i)
			{
				if (value == d.values[i].value)
					return ok();
				also += (i == 0 ? " or " : ", ") + decimal(d.values[i].value);
			}
			const Bounds b = boundsNow(d, now);
			if (value < b.min || value > b.max)
				return fail(Status::InvalidArgument, ErrorCode::OutOfRange,
					    "the setting takes " + decimal(b.min) + " to " + decimal(b.max) + also);
			return ok();
		}

		case ValueType::Enum:
			// Held to what the row offers by the caller, which asks the one function
			// that also leaves out what the box lacks.
			return ok();

		case ValueType::Key:
		{
			const Bounds b = boundsNow(d, now);
			if (value < b.min || value > b.max)
				return fail(Status::InvalidArgument, ErrorCode::OutOfRange,
					    "the setting takes the code of a key of the remote control");
			// Inside the bounds is still no key where the code is one the input layer
			// never delivers, such as a release.
			if (!keySource().known(value))
				return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
					    "the setting takes the code of a key a remote control can send, or no key");
			return ok();
		}

		case ValueType::String:
		case ValueType::Color:
		case ValueType::List:
		case ValueType::Records:
			break;
	}

	return fail(Status::Internal, ErrorCode::BadTable,
		    "the setting is not of a kind a number is offered for");
}

/* The default and the stored value of a number row: writing either again is no new pick,
   whatever the row offers now. The store is read only when the default does not match. */
bool keptValue(const Descriptor &d, long number)
{
	if (number == defaultInt(d))
		return true;
	long stored = 0;
	return settingsSource().readInt(d.key, stored) == Status::Ok && stored == number;
}

// Once per row, since a screen asks again on every redraw.
void saidNameless(const char *key)
{
	static std::mutex lock;
	static std::set<std::string> told;
	std::lock_guard<std::mutex> hold(lock);
	if (told.insert(key).second)
		fprintf(stderr, "coreapi: the list of %s has entries without text and they are left out\n", key);
}

/* The list a row takes from its provider, with each label resolved the way an own list's
   is. False where the provider cannot say and where it says nothing, since a list of no
   entry offers nothing to choose and the two are answered alike. */
bool providedChoices(const Descriptor &d, std::vector<SettingChoice> &out)
{
	std::vector<SettingChoice> listed;
	if (!d.choices_from(listed) || listed.empty())
		return false;

	std::vector<SettingChoice> kept;
	kept.reserve(listed.size());
	bool nameless = false;
	for (size_t i = 0; i < listed.size(); ++i)
	{
		SettingChoice &one = listed[i];
		/* A String entry stands for its text. One without it is a provider that filled the
		   number and the label like a number row's, and every write would be refused for it
		   without a word, so it is left out and said once. */
		if (d.type == ValueType::String && one.text.empty())
		{
			nameless = true;
			continue;
		}
		std::string words;
		if (!one.label_key.empty() && resolveLabel(one.label_key.c_str(), words))
			one.label = words;
		else if (one.label.empty() && d.type == ValueType::String)
			one.label = one.text;
		kept.push_back(one);
	}
	if (nameless)
		saidNameless(d.key);
	if (kept.empty())
		return false;
	out.swap(kept);
	return true;
}

ValueLookup lookupAfter(const BatchOverlay *batch);

/* The values a row offers on this box. One function, because the write is held to the
   same set a read answers with: written twice, a caller could be offered a value the
   write turns down. False is a row that offers no set at all and a set the box has no
   entry of. The entries' own conditions are judged on batch first and the store after it;
   ignoreWhen leaves them out, for the pass that has no batch yet. */
bool valuesOffered(const Descriptor &d, std::vector<SettingChoice> &out,
		   const BatchOverlay *batch = NULL, bool ignoreWhen = false)
{
	const ValueLookup now = lookupAfter(batch);
	if (d.choices_from != NULL)
		return providedChoices(d, out);

	if (d.type != ValueType::Enum)
		return false;

	if (d.values == NULL || d.value_count == 0)
		return false;

	std::vector<SettingChoice> listed;
	listed.reserve(d.value_count);
	for (size_t i = 0; i < d.value_count; ++i)
	{
		const EnumValue &e = d.values[i];
		if (ignoreWhen ? (e.available != NULL && !e.available()) : !entryOffered(e, now))
			continue;
		SettingChoice one;
		one.value = e.value;
		if (e.label_text != NULL)
			one.label = e.label_text;
		else
		{
			/* The text and not the name of it, because that is what the other kind
			   answers with and a caller must not have to tell the two apart. A name
			   the catalog carries nothing under leaves the text empty. */
			if (e.label_key != NULL)
				one.label_key = e.label_key;
			resolveLabel(e.label_key, one.label);
		}
		listed.push_back(one);
	}
	if (listed.empty())
		return false;
	out.swap(listed);
	return true;
}

// The value batch writes under key, NULL when it writes none.
const std::string *batchValue(const BatchOverlay *batch, const Descriptor &row)
{
	if (batch == NULL)
		return NULL;
	for (size_t i = 0; i < batch->values.size(); ++i)
	{
		if (batch->values[i].first == row.key)
			return &batch->values[i].second;
	}
	return NULL;
}

/* The two readers a condition is judged with. False is "cannot say", which the evaluator
   takes as holding, so a store that fails to read never refuses a write. A credential is
   read from the store itself and not through get(), which withholds it: what a caller
   cannot see must not be what refuses its write. */
bool readNumberAfterBatch(const char *key, long *value, void *context)
{
	const Descriptor *row = findRow(key);
	if (row == NULL || holdsText(*row) || row->type == ValueType::List ||
	    row->type == ValueType::Records)
		return false;

	const std::string *sent = batchValue(static_cast<const BatchOverlay *>(context), *row);
	if (sent != NULL)
		return parseNumber(*sent, *value);

	long stored = 0;
	Status s = settingsSource().readInt(row->key, stored);
	if (s == Status::NotFound)
		stored = defaultInt(*row);
	else if (s != Status::Ok)
		return false;
	*value = stored;
	return true;
}

bool readTextAfterBatch(const char *key, std::string *value, void *context)
{
	const Descriptor *row = findRow(key);
	if (row == NULL || !holdsText(*row))
		return false;

	const std::string *sent = batchValue(static_cast<const BatchOverlay *>(context), *row);
	if (sent != NULL)
	{
		*value = *sent;
		return true;
	}

	std::string stored;
	Status s = settingsSource().readString(row->key, stored);
	if (s == Status::NotFound)
		stored = row->default_string != NULL ? row->default_string : "";
	else if (s != Status::Ok)
		return false;
	value->swap(stored);
	return true;
}

ValueLookup lookupAfter(const BatchOverlay *batch)
{
	// The cast only fits the lookup's untyped context; both readers take it back as const.
	ValueLookup after_batch = { readNumberAfterBatch, readTextAfterBatch,
	                            const_cast<BatchOverlay *>(batch) };
	return after_batch;
}

/* Asked last, once the value has passed every rule of its own: a value no row could
   take is wrong whatever the other settings hold, and that answer is the one a caller
   can act on without changing anything else. The entry of an enum that the value names
   is judged here too, on the batch and then the store, because the settings its own
   condition reads can be written in the same batch. value is NULL where there is none. */
void addOnce(std::vector<std::string> &keys, const char *key)
{
	if (key != NULL && std::find(keys.begin(), keys.end(), key) == keys.end())
		keys.push_back(key);
}

// The settings read by the conditions of d that do not hold, a group that fails naming all it reads.
std::vector<std::string> failingKeys(const Descriptor &d, const ValueLookup &lookup)
{
	std::vector<std::string> keys;
	for (size_t i = 0; d.conditions != NULL && i < d.condition_count; ++i)
	{
		const Condition &c = d.conditions[i];
		if (conditionsHold(&c, 1, lookup))
			continue;
		if (!conditionIsGroup(c))
			addOnce(keys, c.key);
		for (size_t m = 0; conditionIsGroup(c) && c.any_of != NULL && m < c.any_count; ++m)
			addOnce(keys, c.any_of[m].key);
	}
	return keys;
}

Result<void> allowedByConditions(const Descriptor &d, const BatchOverlay *batch, const std::string *value = NULL)
{
	const ValueLookup after_batch = lookupAfter(batch);
	// A batch has been through the couplings, and for such a pair theirs is the answer.
	if (!(batch != NULL && judgedByCoupling(d)) && !conditionsHold(d, after_batch))
		return Result<void>::failure(conditionRefusal("the settings this one depends on do not allow it to be set",
							      failingKeys(d, after_batch)));

	long number = 0;
	if (value != NULL && d.type == ValueType::Enum && d.choices_from == NULL && d.values != NULL &&
	    parseNumber(*value, number))
	{
		for (size_t i = 0; i < d.value_count; ++i)
		{
			const EnumValue &e = d.values[i];
			if (e.value == number && e.when_count > 0 && !conditionsHold(e.when, e.when_count, after_batch) &&
			    !keptValue(d, number))
				return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
					    "the setting does not offer that value");
		}
	}
	return ok();
}

// What passed checkValue(), so set() writes the row and number that were checked.
struct Checked
{
	Descriptor  row;
	long        number;
	// What a text row stores, which is the value for a plain text and its one
	// spelling for a colour, so what is read back is the same whoever wrote it.
	std::string text;
	// What a list or a list of records was read into, so set() writes what was checked.
	std::vector<std::string>  list;
	std::vector<RecordValues> records;

	Checked() : row(), number(0), text(), list(), records() {}
};

/* The text of a list or of a list of records, held to what its row says each
   text or member is, and read into out. Nothing is stored. */
Result<void> checkCollection(const Descriptor &d, const std::string &value, Checked &out)
{
	std::vector<std::string> lines;
	if (!value.empty())
		splitOn(value, '\n', lines);

	if (lines.size() > kMaxListItems)
		return fail(Status::InvalidArgument, ErrorCode::OutOfRange,
			    "the setting takes at most " + decimal((long) kMaxListItems) + " entries");

	// A list whose records the screens hold addresses of takes exactly the records it has.
	if (d.type == ValueType::Records && d.field.extra->fixed_count)
	{
		std::vector<RecordValues> current;
		const Status s = settingsSource().readRecords(d.key, current);
		if (s != Status::Ok && s != Status::NotFound)
			return fail(s, ErrorCode::SettingUnreadable, "the setting could not be read");
		if (lines.size() != current.size())
			return fail(Status::InvalidArgument, ErrorCode::OutOfRange,
				    "the setting takes exactly its " + decimal((long) current.size()) +
				    " records: adding and removing one is done on the box's screen");
	}

	for (size_t i = 0; i < lines.size(); ++i)
	{
		if (lines[i].empty())
			return fail(Status::InvalidArgument, ErrorCode::BadString,
				    "the setting takes one entry to a line and no empty line");
		Result<void> shape = allowedAtAll(lines[i]);
		if (!shape.ok())
			return shape;
	}

	if (d.type == ValueType::List)
	{
		for (size_t i = 0; i < lines.size(); ++i)
		{
			Result<void> text = allowedAsText(lines[i]);
			if (!text.ok())
				return text;
		}
		out.list.swap(lines);
		return ok();
	}

	const FieldExtra *x = d.field.extra;
	std::vector<RecordValues> records;
	std::vector<RecordValues> stored;
	bool storedRead = false;
	for (size_t i = 0; i < lines.size(); ++i)
	{
		RecordValues members;
		splitOn(lines[i], '\t', members);
		if (members.size() != x->record_field_count)
			return fail(Status::InvalidArgument, ErrorCode::BadString,
				    "each record takes " + decimal((long) x->record_field_count) +
				    " members with a tab between them");
		for (size_t m = 0; m < members.size(); ++m)
		{
			Result<void> one = allowedAsMember(x->record_fields[m], members[m]);
			if (!one.ok())
			{
				/* A key the input layer no longer delivers passes again where the record
				   already holds it at that place, as a number row's stored value does. */
				if (x->record_fields[m].type != ValueType::Key || one.error().code != ErrorCode::NotAListedValue)
					return one;
				if (!storedRead)
				{
					storedRead = true;
					if (settingsSource().readRecords(d.key, stored) != Status::Ok)
						stored.clear();
				}
				if (i >= stored.size() || m >= stored[i].size() || stored[i][m] != members[m])
					return one;
			}
		}
		records.push_back(members);
	}
	out.records.swap(records);
	return ok();
}

/* Every rule set() holds a value to that is about the value and the row alone: what a write
   takes without asking another setting or touching the store. One function for both, so
   that check() cannot pass a value set() would refuse for itself. */
Result<void> checkValue(const std::string &key, const std::string &value, Checked &out, bool cleared = false,
			bool deferWhen = false, const BatchOverlay *batch = NULL)
{
	const Descriptor *d = findRow(key);
	if (d == NULL)
		return fail(Status::NotFound, ErrorCode::UnknownSetting,
			    "no setting is declared under that key");

	// Ahead of every rule about the value: a row the lock fixes takes none.
	if (lockedNow(key))
		return fail(Status::Conflict, ErrorCode::SettingLocked,
			    "the box's parental lock fixes this setting");
	// Held to the shape the box offers it in, and refused where the box lacks it.
	Descriptor here;
	if (!rowOnThisBox(*d, here))
		return fail(Status::Conflict, ErrorCode::SettingNotOnThisBox,
			    "this box does not have what the setting controls");
	d = &here;

	/* A credential reads as nothing, so a form that redraws itself from what it read offers
	   nothing back here, and taking that would wipe the value the read protected. Refused for
	   every kind rather than only for text, so the answer says why: a number row would
	   otherwise turn an emptied field down as a bad number. */
	if (d->secret && value.empty() && !cleared)
		return fail(Status::InvalidArgument, ErrorCode::EmptyCredential,
			    "the setting is a credential and is not cleared by writing nothing");

	// A collection is held to its own rules, member by member.
	if (d->type == ValueType::List || d->type == ValueType::Records)
	{
		Result<void> collected = checkCollection(*d, value, out);
		if (!collected.ok())
			return collected;
		out.row = here;
		return ok();
	}

	// The store is not touched until the value has passed the row, so a request
	// refused anywhere here leaves every setting as it was.
	Result<void> shape = allowedAtAll(value);
	if (!shape.ok())
		return fail(shape.error());

	if (d->type == ValueType::Color)
	{
		/* Not held to the rule for text: the number sign that opens a colour is where the
		   settings file cuts a value off, and a colour never goes through that file. A text
		   of another length is a colour of another row. */
		unsigned char channels[4];
		if (!readColorText(value, colorChannels(*d), channels))
			return fail(Status::InvalidArgument, ErrorCode::BadString,
				    colorChannels(*d) == 4
				    ? "the setting takes a colour written #rrggbbaa, in hexadecimal"
				    : "the setting takes a colour written #rrggbb, in hexadecimal");
		out.text = colorText(channels, colorChannels(*d));
	}
	else if (d->type == ValueType::String)
	{
		out.text = value;
		Result<void> text = allowedAsText(value);
		if (!text.ok())
			return fail(text.error());

		if (d->text != NULL)
		{
			Result<void> rule = holdsTextRule(*d->text, value);
			/* The store is asked only for a value the rule refused, since what is already
			   stored passes again however the place is mounted now. */
			if (!rule.ok())
			{
				std::string stored;
				if (settingsSource().readString(d->key, stored) == Status::Ok)
					rule = holdsTextRule(*d->text, value, &stored);
			}
			if (!rule.ok())
				return fail(rule.error());
		}

		if (d->choices_from != NULL)
		{
			/* The default counts as offered: a row reads as its default until something is
			   stored, so refusing it would make the way back to it a write that fails. The
			   stored value is read only once the list has refused. */
			const std::string fallback = d->default_string != NULL ? d->default_string : "";
			Result<void> offered = holdsOffered(d->choices_from, value, &fallback);
			if (!offered.ok())
			{
				std::string stored;
				if (settingsSource().readString(d->key, stored) == Status::Ok && stored == value)
					offered = ok();
			}
			if (!offered.ok())
				return fail(offered.error());
		}

		/* An identifier is text here and a number where the program keeps it, so the one
		   spelling that survives is the one a channel is named by everywhere else. A value the
		   store cannot read would travel as far as the field, be dropped there and read back as
		   whatever the field already held, on a thread with nobody left to answer. */
		if (d->field.origin == FieldOrigin::ChannelIdField
		    || (d->field.extra != NULL && d->field.extra->channel_id))
		{
			unsigned long long id = 0;
			if (!readChannelIdText(value, id))
				return fail(Status::InvalidArgument, ErrorCode::BadString,
					    "the setting takes the hexadecimal spelling a channel is named by, of one to sixteen digits");
		}
	}
	else
	{
		long number = 0;
		if (!parseNumber(value, number))
			return fail(Status::InvalidArgument, ErrorCode::NotANumber,
				    "the setting takes a whole number");

		// A bound that follows another setting reads the value the same write gives it.
		const ValueLookup after_batch = lookupAfter(batch);
		Result<void> allowed = allowedByRow(*d, number, batch != NULL ? &after_batch : NULL);
		if (!allowed.ok() && boundsVary(*d) && number >= d->min && number <= d->max)
		{
			/* Inside the constants and outside what the box states now: the default is the
			   way back and the stored value is what a bound that has moved since leaves
			   behind, so writing either again is no new pick. The stored value is read only
			   on a refusal. */
			long stored = 0;
			if (number == defaultInt(*d) ||
			    (settingsSource().readInt(d->key, stored) == Status::Ok && stored == number))
				allowed = ok();
		}
		if (!allowed.ok())
			return fail(allowed.error());

		/* Held to what the box offers, which leaves out the entries it lacks. While the box
		   cannot say what it has, only the default and the stored value pass: any other
		   could be a number the box cannot show. */
		if (d->type == ValueType::Enum)
		{
			std::vector<SettingChoice> offered;
			bool listed = false;
			if (valuesOffered(*d, offered, NULL, deferWhen))
			{
				for (size_t i = 0; !listed && i < offered.size(); ++i)
					listed = offered[i].value == number;
			}
			/* The default and the stored value pass again, as for a number: what a box
			   that lacks the entry now falls back to or still holds is the way back. */
			if (!listed && !keptValue(*d, number))
				return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
					    "the setting does not offer that value");
		}
		else if (d->choices_from != NULL)
		{
			// As for text: the default is offered, and the stored number is read only on a refusal.
			const long fallback = defaultInt(*d);
			Result<void> offered = holdsOfferedNumber(d->choices_from, number, &fallback);
			if (!offered.ok())
			{
				long stored = 0;
				if (settingsSource().readInt(d->key, stored) == Status::Ok && stored == number)
					offered = ok();
			}
			if (!offered.ok())
				return fail(offered.error());
		}

		out.number = number;
	}

	out.row = here;
	return ok();
}

} // anonymous namespace

ValueLookup currentValues()
{
	ValueLookup now = { readNumberAfterBatch, readTextAfterBatch, NULL };
	return now;
}

// Linear over a few hundred rows, for the reason the store's own lookup is:
// what an index would save is less than building it costs for a request that
// reads a handful of settings.
const Descriptor *findRow(const std::string &key)
{
	const Descriptor *t = settingsTable();
	const size_t n = settingsTableCount();
	for (size_t i = 0; i < n; ++i)
	{
		if (t[i].key != NULL && key == t[i].key)
			return &t[i];
	}
	return NULL;
}

Result<std::vector<Descriptor> > schema()
{
	const Descriptor *t = settingsTable();
	const size_t n = settingsTableCount();
	std::vector<Descriptor> out;
	out.reserve(n);
	for (size_t i = 0; t != NULL && i < n; ++i)
	{
		// A row the box lacks stays in, as it is declared, so a frontend
		// knows the key and can say why it offers nothing for it.
		Descriptor here;
		rowOnThisBox(t[i], here);
		withhold(here);
		out.push_back(here);
	}
	return ok(std::move(out));
}

/* Names the declaration cannot show: a relative name the box writes a file under,
   appended to another directory, and a file or a flag whose default is none and
   which is therefore a path only once somebody gives it one. */
const char *const kRelativePaths[] =
{
	"recordingmenu.filename_template",
	"glcd_font",
	"glcd_background_image",
	"mode_icons_flag5",
	"mode_icons_flag6",
	"mode_icons_flag7"
};

/* Read off the declaration. A row with a text rule is a path exactly when the rule says
   it names a folder or a file; a list of plugin names is not one. A row without one either defaults to an
   absolute name or is a directory whose empty default means "beside another one", and
   the few relative names are listed above. */
bool holdsPath(const Descriptor &d)
{
	// The lists of texts hold files and addresses the box reads from.
	if (d.type == ValueType::List)
		return true;
	if (d.type != ValueType::String || d.key == NULL)
		return false;
	if (d.text != NULL)
		return d.text->kind == TextKind::Directory || d.text->kind == TextKind::File;
	if (d.default_string != NULL && d.default_string[0] == '/')
		return true;
	for (size_t i = 0; i < sizeof(kRelativePaths) / sizeof(kRelativePaths[0]); ++i)
	{
		if (strcmp(d.key, kRelativePaths[i]) == 0)
			return true;
	}
	const size_t n = strlen(d.key);
	return n >= 3 && strcmp(d.key + n - 3, "dir") == 0;
}

Result<void> holdsOffered(ChoiceSource from, const std::string &text, const std::string *always)
{
	// A value the caller knows the row holds, such as its default, needs no entry.
	if (always != NULL && *always == text)
		return ok();

	std::vector<SettingChoice> listed;
	// A box that cannot say what it has holds the row to nothing, as a rule of text would.
	if (!from(listed))
		return ok();

	/* An entry without text stands for nothing a text row can store, as in the list a read
	   answers, so it neither matches nor counts as an offer. */
	bool offered = false;
	for (size_t i = 0; i < listed.size(); ++i)
	{
		if (listed[i].text.empty())
			continue;
		offered = true;
		if (listed[i].text == text)
			return ok();
	}
	if (!offered)
		return ok();
	return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
		    "the setting does not offer that value");
}

Result<void> holdsOfferedNumber(ChoiceSource from, long number, const long *always)
{
	if (always != NULL && *always == number)
		return ok();

	std::vector<SettingChoice> listed;
	if (!from(listed) || listed.empty())
		return ok();

	for (size_t i = 0; i < listed.size(); ++i)
	{
		if (listed[i].value == number)
			return ok();
	}
	return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
		    "the setting does not offer that value");
}

Result<void> holdsTextRule(const TextRule &rule, const std::string &value, const std::string *current)
{
	switch (textFault(rule, value))
	{
		case TextFault::None:
			break;
		case TextFault::TooShort:
			return fail(Status::InvalidArgument, ErrorCode::BadString,
				    "the setting takes at least " + decimal((long) rule.min_length) + " characters");
		case TextFault::TooLong:
			return fail(Status::InvalidArgument, ErrorCode::BadString,
				    "the setting takes at most " + decimal((long) rule.max_length) + " characters");
		case TextFault::BadCharacter:
			return fail(Status::InvalidArgument, ErrorCode::BadString,
				    std::string("the setting takes only these characters: ") + rule.allowed);
	}

	/* What is already stored is not asked about again, so a client that sends the
	   whole section back for an edit elsewhere is not refused for a disk that is
	   not mounted right now. Only a changed value has to name something there. */
	if (current != NULL && *current == value)
		return ok();

	if (value.empty())
	{
		if (rule.allow_empty || (rule.must_exist == MustExist::No && rule.extensions == NULL))
			return ok();
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the setting names a place and cannot be empty");
	}

	if (rule.extensions != NULL && !hasExtension(value, rule.extensions))
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    std::string("the setting takes a file ending in one of: ") + rule.extensions);

	if (rule.must_exist != MustExist::No)
		return existsAsRequired(rule, value);
	return ok();
}

bool sectionHoldsSecret(const std::string &section)
{
	const Descriptor *t = settingsTable();
	const size_t n = settingsTableCount();
	for (size_t i = 0; t != NULL && i < n; ++i)
	{
		if (t[i].secret && t[i].section != NULL && section == t[i].section)
			return true;
	}
	return false;
}

Result<std::vector<std::string> > sections()
{
	const Descriptor *t = settingsTable();
	const size_t n = settingsTableCount();

	std::vector<std::string> out;
	for (size_t i = 0; i < n; ++i)
	{
		if (t[i].section == NULL)
			continue;
		bool seen = false;
		for (size_t j = 0; j < out.size() && !seen; ++j)
			seen = out[j] == t[i].section;
		if (!seen)
			out.push_back(t[i].section);
	}
	return ok(std::move(out));
}

bool lockedNow(const std::string &key)
{
	if (!heldByParentalLock(key.c_str()))
		return false;
	bool locked = true;
	if (systemSource().parentalLocked(locked) != Status::Ok)
		return true;
	return locked;
}

bool conditionsHoldNow(const std::string &key)
{
	const Descriptor *d = findRow(key);
	if (d == NULL)
		return true;
	return allowedByConditions(*d, NULL).ok();
}

Result<Descriptor> describe(const std::string &key)
{
	const Descriptor *d = findRow(key);
	if (d == NULL)
		return fail(Status::NotFound, ErrorCode::UnknownSetting,
			    "no setting is declared under that key");
	Descriptor out;
	rowOnThisBox(*d, out);
	withhold(out);
	return ok(out);
}

namespace
{

// What the store holds for a row, a credential included. Never handed to a caller outside.
Result<std::string> readStored(const Descriptor *d)
{
	if (d->type == ValueType::List)
	{
		std::vector<std::string> items;
		Status s = settingsSource().readList(d->key, items);
		if (s == Status::NotFound)
			return ok(std::string(d->default_string != NULL ? d->default_string : ""));
		if (s != Status::Ok)
			return fail(s, ErrorCode::SettingUnreadable, "the setting could not be read");
		return ok(joinOn(items, '\n'));
	}

	if (d->type == ValueType::Records)
	{
		std::vector<RecordValues> records;
		Status s = settingsSource().readRecords(d->key, records);
		if (s == Status::NotFound)
			return ok(std::string(d->default_string != NULL ? d->default_string : ""));
		if (s != Status::Ok)
			return fail(s, ErrorCode::SettingUnreadable, "the setting could not be read");
		std::vector<std::string> lines;
		for (size_t i = 0; i < records.size(); ++i)
			lines.push_back(joinOn(records[i], '\t'));
		return ok(joinOn(lines, '\n'));
	}

	if (holdsText(*d))
	{
		std::string value;
		Status s = settingsSource().readString(d->key, value);
		// A store that never held the key answers what the row declares,
		// which is what the program's own load puts there.
		if (s == Status::NotFound)
			return ok(std::string(d->default_string != NULL ? d->default_string : ""));
		if (s != Status::Ok)
			return fail(s, ErrorCode::SettingUnreadable,
				    "the setting could not be read");
		return ok(std::move(value));
	}

	long value = 0;
	Status s = settingsSource().readInt(d->key, value);
	if (s == Status::NotFound)
		value = defaultInt(*d);
	else if (s != Status::Ok)
		return fail(s, ErrorCode::SettingUnreadable,
			    "the setting could not be read");
	return ok(decimal(value));
}

} // anonymous namespace

Result<std::string> get(const std::string &key)
{
	const Descriptor *d = findRow(key);
	if (d == NULL)
		return fail(Status::NotFound, ErrorCode::UnknownSetting,
			    "no setting is declared under that key");

	/* A credential is answered with nothing rather than with what is stored, and the store is
	   not asked at all. Empty and not an error: the schema says the row is secret, while an
	   error could not be told from a store that failed and would leave a frontend unable to
	   draw the field at all. */
	if (d->secret)
		return ok(std::string());

	return readStored(d);
}

Result<void> checkCleared(const std::string &key)
{
	Checked unused;
	return checkValue(key, std::string(), unused, true);
}

Result<void> check(const std::string &key, const std::string &value)
{
	Checked unused;
	return checkValue(key, value, unused);
}

namespace
{

/* Whether the checked value is the one the store holds, a missing entry counting as its
   default. The text is the checked one, so a colour and a list compare in the spelling they
   are stored in. A credential never counts: answering ok to a guess of it where a different
   write would be refused would tell the guess apart. */
bool sameAsStored(const Checked &c)
{
	if (c.row.secret)
		return false;
	std::string text;
	if (c.row.type == ValueType::List)
		text = joinOn(c.list, '\n');
	else if (c.row.type == ValueType::Records)
	{
		std::vector<std::string> lines;
		for (size_t i = 0; i < c.records.size(); ++i)
			lines.push_back(joinOn(c.records[i], '\t'));
		text = joinOn(lines, '\n');
	}
	else if (holdsText(c.row))
		text = c.text;
	else
		text = decimal(c.number);
	const Result<std::string> was = readStored(&c.row);
	return was.ok() && was.value() == text;
}

// For a member nobody has checked yet: one a check refuses is not the stored value.
bool holdsStored(const std::string &key, const std::string &value)
{
	Checked c;
	return checkValue(key, value, c, false, true).ok() && sameAsStored(c);
}

/* set() up to the store and no further: the value is held and nobody is asked to carry it
   in yet, so a batch can hold all of its members before one drain applies them together.
   A value the store holds already is answered ok without a write, unless the caller asks
   for it to be written all the same, and unchanged says which happened. */
Result<void> store(const std::string &key, const std::string &value, const BatchOverlay *batch,
		   bool *unchanged = NULL, bool writeUnchanged = false)
{
	Checked c;
	const bool cleared = batch != NULL && std::find(batch->cleared.begin(), batch->cleared.end(), key) != batch->cleared.end();
	// The entries' own conditions wait for the batch, which the first pass of a batch has not got.
	Result<void> checked = checkValue(key, value, c, cleared, true, batch);
	if (!checked.ok())
		return checked;

	/* The conditions are asked first and their answer is dropped for a value the store holds:
	   writing back what a row holds is no change for them to judge, and a client that sends a
	   whole section back must not be refused for a row it did not touch. */
	Result<void> allowed = allowedByConditions(c.row, batch, &value);
	if (!writeUnchanged && sameAsStored(c))
	{
		if (unchanged != NULL)
			*unchanged = true;
		return ok();
	}
	if (!allowed.ok())
		return allowed;

	Status written = Status::Ok;
	if (c.row.type == ValueType::List)
		written = settingsSource().writeList(c.row.key, c.list);
	else if (c.row.type == ValueType::Records)
		written = settingsSource().writeRecords(c.row.key, c.records);
	else if (holdsText(c.row))
		written = settingsSource().writeString(c.row.key, c.text);
	else
		written = settingsSource().writeInt(c.row.key, c.number);
	if (written != Status::Ok)
		return fail(written, ErrorCode::SettingNotWritten,
			    "the setting could not be written");
	return ok();
}

/* Saved here rather than left to the caller: a value the store took and nobody saved is
   gone the next time the program writes its file. */
Status persistStored(bool onLoop)
{
	return onLoop ? settingsSource().persistNow() : settingsSource().persist();
}

/* Every member is held before one save carries them in, so each group runs once on the
   batch as a whole and never on a half of it, and the file is written once. */
void storeAll(const BatchOverlay &batch, Refusals &failed, bool onLoop, bool writeUnchanged = false)
{
	std::vector<std::string> stored;
	for (size_t i = 0; i < batch.values.size(); ++i)
	{
		bool unchanged = false;
		Result<void> done = store(batch.values[i].first, batch.values[i].second, &batch, &unchanged, writeUnchanged);
		if (!done.ok())
			failed.push_back(std::make_pair(batch.values[i].first, done.error()));
		else if (!unchanged)
			stored.push_back(batch.values[i].first);
	}
	if (stored.empty())
		return;

	Status saved = persistStored(onLoop);
	if (saved == Status::Ok)
		return;
	for (size_t i = 0; i < stored.size(); ++i)
		failed.push_back(std::make_pair(stored[i],
			Error(saved, ErrorCode::SettingNotWritten, "the setting was taken and not saved")));
}

} // anonymous namespace

Result<void> set(const std::string &key, const std::string &value, const BatchOverlay *batch, bool onLoop,
		 const std::string &who)
{
	WriterScope writer(who.empty() ? currentWriter() : who);
	bool unchanged = false;
	Result<void> stored = store(key, value, batch, &unchanged);
	if (!stored.ok())
		return stored;
	// Nothing to save, post or apply for a value the store holds.
	if (unchanged)
		return ok();

	Status saved = persistStored(onLoop);
	if (saved != Status::Ok)
		return fail(saved, ErrorCode::SettingNotWritten,
			    "the setting was taken and not saved");

	/* Nothing is applied from here. The value is not in the program's own settings yet: the
	   store holds it and the save above is what carries it there, so whoever applies the change
	   is asked by whatever carries the write. An apply run here would read the value the box
	   was running on before. */
	return ok();
}

namespace
{

bool alreadyRefused(const Refusals &refused, const std::string &key)
{
	for (size_t i = 0; i < refused.size(); ++i)
	{
		if (refused[i].first == key)
			return true;
	}
	return false;
}

bool eraseMember(BatchOverlay &batch, const std::string &key)
{
	for (size_t i = 0; i < batch.values.size(); ++i)
	{
		if (batch.values[i].first == key)
		{
			batch.values.erase(batch.values.begin() + i);
			return true;
		}
	}
	return false;
}

/* What is left of a settled batch. A member the caller named that the store holds already is
   done and goes out of it, so no write, save or apply is made for it; a menu keeps it, because
   its screen has put the value into the program's settings and the key is what the drain
   applies. What a coupling added stays: the write skips it where it is unchanged. */
void leaveSettled(BatchOverlay &batch, const BatchOverlay &made, const BatchOverlay &named, bool keepUnchanged)
{
	batch = made;
	if (keepUnchanged)
		return;
	for (size_t i = batch.values.size(); i-- > 0;)
	{
		const std::string &key = batch.values[i].first;
		bool asked = false;
		for (size_t n = 0; n < named.values.size() && !asked; ++n)
			asked = named.values[n].first == key;
		if (asked && holdsStored(key, batch.values[i].second))
			batch.values.erase(batch.values.begin() + i);
	}
}

void refuseByCondition(Refusals &refused, const std::string &key, const Error &why)
{
	if (!alreadyRefused(refused, key))
		refused.push_back(std::make_pair(key, why));
}

void refuseByCondition(Refusals &refused, const std::string &key, const char *message)
{
	refuseByCondition(refused, key, conditionRefusal(message, std::vector<std::string>()));
}

} // anonymous namespace

void settleBatch(BatchOverlay &batch, Refusals &refused, bool keepUnchanged)
{
	/* What the caller named is kept apart from what the couplings made of it. Every round
	   makes the couplings over again from the survivors, so a member that fell takes what it
	   brought with it, and an addition that cannot land takes the member that brought it. The
	   members only ever leave, so this ends. */
	BatchOverlay named = batch;

	/* A round that takes nothing out ends the loop, and every other round takes out a member of
	   what the caller named, so the rounds cannot outnumber the members. The bound is that with
	   room for every coupling to refuse in its own round. It is here for a coupling that comes
	   to feed itself: that ends in a refusal of everything the caller named, never in a hang. */
	const size_t bound = (named.values.size() + 1) * (couplingCount() + 1);
	for (size_t round = 0;; ++round)
	{
		if (round > bound)
		{
			for (size_t i = 0; i < named.values.size(); ++i)
				refuseByCondition(refused, named.values[i].first,
					"the settings written together could not be settled");
			batch.values.clear();
			return;
		}

		BatchOverlay made = named;
		Refusals refusedByCouple;
		std::vector<Addition> added;
		applyCouplings(made, refusedByCouple, added);
		if (!refusedByCouple.empty())
		{
			// A refusal leaves the round half made, so it is made again without them.
			for (size_t i = 0; i < refusedByCouple.size(); ++i)
			{
				eraseMember(named, refusedByCouple[i].first);
				if (!alreadyRefused(refused, refusedByCouple[i].first))
					refused.push_back(refusedByCouple[i]);
			}
			continue;
		}

		/* Judged round by round against what is still in, because dropping one member can take
		   away what another one leaned on. */
		std::vector<std::string> failing;
		std::vector<Error> why;
		for (size_t i = 0; i < made.values.size(); ++i)
		{
			const Descriptor *row = findRow(made.values[i].first);
			if (row == NULL)
				continue;
			Result<void> allowed = allowedByConditions(*row, &made, &made.values[i].second);
			if (allowed.ok())
				continue;
			if (keepUnchanged || !holdsStored(made.values[i].first, made.values[i].second))
			{
				failing.push_back(made.values[i].first);
				// An entry's own condition names no setting of the row's.
				why.push_back(allowed.error().code == ErrorCode::SettingConditionNotMet ? allowed.error() :
					conditionRefusal("the settings this one depends on do not allow it to be set",
							 std::vector<std::string>()));
			}
		}
		if (failing.empty())
		{
			leaveSettled(batch, made, named, keepUnchanged);
			return;
		}

		bool progress = false;
		for (size_t f = 0; f < failing.size(); ++f)
		{
			if (eraseMember(named, failing[f]))
			{
				refuseByCondition(refused, failing[f], why[f]);
				progress = true;
				continue;
			}

			/* Not named by the caller: a coupling put it. It is reported under its own key, and
			   what asked for it is refused with it, since one without the other is the state the
			   coupling exists to prevent. */
			refuseByCondition(refused, failing[f], why[f]);
			for (size_t a = 0; a < added.size(); ++a)
			{
				if (added[a].key != failing[f])
					continue;
				if (eraseMember(named, added[a].trigger))
				{
					refuseByCondition(refused, added[a].trigger,
						conditionRefusal("the setting it brings with it cannot be set",
								 std::vector<std::string>(1, failing[f])));
					progress = true;
				}
			}
		}

		/* An addition nobody is recorded as owning cannot be traced to a member, so it alone
		   is left out rather than looping on it. */
		if (!progress)
		{
			for (size_t f = 0; f < failing.size(); ++f)
				eraseMember(made, failing[f]);
			leaveSettled(batch, made, named, keepUnchanged);
			return;
		}
	}
}

void writeBatch(const std::vector<std::pair<std::string, std::string> > &members, Refusals &failed,
		bool onLoop, const std::string &who)
{
	WriterScope writer(who.empty() ? currentWriter() : who);
	/* The three passes of one write, so every caller that writes more than one setting gets
	   the couplings and the settling and none can leave them out. The first refuses what is
	   wrong on its own, and only what passed goes into the batch, since a value that never
	   lands must not allow another one. A bound that follows another setting reads the request
	   there; the third pass judges it again on what passed. */
	BatchOverlay asked;
	asked.values = members;
	BatchOverlay batch;
	for (size_t i = 0; i < members.size(); ++i)
	{
		Checked unused;
		Result<void> checked = checkValue(members[i].first, members[i].second, unused, false, true, &asked);
		if (!checked.ok())
		{
			failed.push_back(std::make_pair(members[i].first, checked.error()));
			continue;
		}
		batch.values.push_back(members[i]);
	}

	settleBatch(batch, failed);

	storeAll(batch, failed, onLoop);
}

Status menuChanged(const std::string &key)
{
	const Descriptor *d = findRow(key);
	if (d == NULL)
		return applyKey(key);
	Result<std::string> now = readStored(d);
	if (!now.ok())
		return applyKey(key);

	BatchOverlay batch;
	batch.values.push_back(std::make_pair(key, now.value()));
	Refusals refused;
	settleBatch(batch, refused, true);

	// A coupling that only restates what is stored has nothing to write and nothing to save.
	bool more = false;
	for (size_t i = 0; i < batch.values.size() && !more; ++i)
	{
		if (batch.values[i].first == key)
			continue;
		const Descriptor *row = findRow(batch.values[i].first);
		if (row == NULL)
			continue;
		Result<std::string> was = readStored(row);
		more = !was.ok() || was.value() != batch.values[i].second;
	}
	if (!more)
		return applyKey(key);

	/* The key goes in with what it brought, so the one drain runs each group once on all of
	   them, saves, and tells the open menus which settings moved. */
	Refusals failed;
	storeAll(batch, failed, true, true);
	for (size_t i = 0; i < failed.size(); ++i)
		std::fprintf(stderr, "coreapi: %s, coupled to %s, was not written: %s\n", failed[i].first.c_str(),
			     key.c_str(), failed[i].second.message.c_str());
	return failed.empty() ? Status::Ok : Status::Internal;
}

Result<void> resetDefaults(const std::vector<std::string> &requested, Refusals &refused, bool onLoop)
{
	/* Every key is known before anything is written, so a misspelt one leaves the rest as it
	   was and a caller never has to guess how far a reset got. */
	for (size_t i = 0; i < requested.size(); ++i)
	{
		if (findRow(requested[i]) == NULL)
			return fail(Status::NotFound, ErrorCode::UnknownSetting,
				    "no setting is declared under that key");
	}

	// One half of a pair is not a setting that can be written, so its partner is reset with it.
	std::vector<std::string> keys = requested;
	addPartners(keys);

	std::vector<std::pair<std::string, std::string> > members;
	for (size_t i = 0; i < keys.size(); ++i)
	{
		Descriptor here;
		const Descriptor *d = findRow(keys[i]);
		rowOnThisBox(*d, here);
		withhold(here);

		std::string value;
		// Text, a colour and the two kinds of list all declare their default as text.
		if (holdsText(here) || here.type == ValueType::List || here.type == ValueType::Records)
			value = here.default_string != NULL ? here.default_string : "";
		else
			value = decimal(defaultInt(here));
		members.push_back(std::make_pair(keys[i], value));
	}

	writeBatch(members, refused, onLoop);
	return ok();
}

Result<void> clearSecret(const std::string &key, const std::string &who)
{
	WriterScope writer(who.empty() ? currentWriter() : who);
	const Descriptor *d = findRow(key);
	if (d == NULL)
		return fail(Status::NotFound, ErrorCode::UnknownSetting,
			    "no setting is declared under that key");

	/* Only a credential, because this is the one call that writes a value no other call may
	   write. Widening it to every row would make it a second way to write a setting, one that
	   goes round the three rules the write beside it holds a value to. */
	if (!d->secret)
		return fail(Status::InvalidArgument, ErrorCode::NotACredential,
			    "the setting is not a credential and is not cleared here");

	/* Written as text whatever the row declares. A credential is a string on every row that
	   carries one, and a number row marked secret would have no empty value to write: there is
	   no such row, and if one were added the store would answer for it. */
	if (d->type != ValueType::String)
		return fail(Status::InvalidArgument, ErrorCode::NotACredential,
			    "the setting is a credential of a kind that has nothing to clear");

	// Emptying is a write like any other to a row whose rule wants more, such as a pin.
	if (d->text != NULL)
	{
		Result<void> rule = holdsTextRule(*d->text, std::string());
		if (!rule.ok())
			return fail(rule.error());
	}

	Status written = settingsSource().writeString(d->key, std::string());
	if (written != Status::Ok)
		return fail(written, ErrorCode::SettingNotWritten,
			    "the setting could not be written");

	// Saved here for the reason the write beside it saves: a value the store
	// took and nobody saved is gone the next time the program writes its file.
	Status saved = settingsSource().persist();
	if (saved != Status::Ok)
		return fail(saved, ErrorCode::SettingNotWritten,
			    "the setting was taken and not saved");

	return ok();
}

Result<std::vector<SettingChoice> > choices(const std::string &key)
{
	const Descriptor *d = findRow(key);
	if (d == NULL)
		return fail(Status::NotFound, ErrorCode::UnknownSetting,
			    "no setting is declared under that key");

	Descriptor here;
	if (!rowOnThisBox(*d, here))
		return fail(Status::Conflict, ErrorCode::SettingNotOnThisBox,
			    "this box does not have what the setting controls");

	std::vector<SettingChoice> out;
	if (!valuesOffered(here, out))
		return fail(Status::InvalidArgument, ErrorCode::ChoicesUnavailable,
			    "the setting offers no set of values this box can state");

	return ok(std::move(out));
}

bool resolveLabel(const char *key, std::string &out)
{
	if (key == NULL)
		return false;
	return localeSource().text(key, out) == Status::Ok;
}

namespace
{

// The number after the last underscore of key, -1 where it does not end in one; stem is the rest.
long slotOf(const char *key, std::string &stem)
{
	const char *bar = strrchr(key, '_');
	if (bar == NULL || bar[1] == '\0')
		return -1;
	for (const char *c = bar + 1; *c != '\0'; ++c)
	{
		if (!isdigit((unsigned char) *c))
			return -1;
	}
	stem.assign(key, bar - key);
	return strtol(bar + 1, NULL, 10);
}

} // anonymous namespace

bool rowLabel(const Descriptor &d, std::string &out)
{
	if (!resolveLabel(d.label_key, out))
		return false;
	if (d.key == NULL)
		return true;
	std::string stem;
	const long slot = slotOf(d.key, stem);
	if (slot < 0)
		return true;
	// Read off the table: a sibling under the same stem and the same label is what makes it a slot.
	const Descriptor *table = settingsTable();
	const size_t count = settingsTableCount();
	for (size_t i = 0; i < count; ++i)
	{
		std::string other;
		if (table[i].key == NULL || strcmp(table[i].key, d.key) == 0 || table[i].label_key == NULL ||
		    strcmp(table[i].label_key, d.label_key) != 0 || slotOf(table[i].key, other) < 0 || other != stem)
			continue;
		char n[24];
		snprintf(n, sizeof(n), " %ld", slot + 1);
		out += n;
		return true;
	}
	return true;
}

void announceSettingsChanged()
{
	Event e;
	e.type = EventType::SettingsChanged;
	EventBus::instance().publish(e);
}

Snapshot snapshot()
{
	Snapshot out;
	const Descriptor *t = settingsTable();
	const size_t n = settingsTableCount();
	for (size_t i = 0; t != NULL && i < n; ++i)
	{
		if (t[i].key == NULL)
			continue;
		/* Read past the rule that answers a credential with nothing: a file that changed only
		   a key is a change its group has to put in force. The snapshot never leaves here. */
		const Result<std::string> v = readStored(&t[i]);
		if (v.ok())
			out[t[i].key] = v.value();
	}
	return out;
}

std::vector<std::string> changedSince(const Snapshot &before)
{
	std::vector<std::string> changed;
	const Snapshot now = snapshot();
	for (Snapshot::const_iterator it = now.begin(); it != now.end(); ++it)
	{
		const Snapshot::const_iterator was = before.find(it->first);
		if (was == before.end() || was->second != it->second)
			changed.push_back(it->first);
	}
	return changed;
}

Status applyChangedSince(const Snapshot &before)
{
	return applyBatch(changedSince(before));
}

Status applyReplaced(const std::function<void()> &replace)
{
	const Snapshot before = snapshot();
	replace();
	return applyChangedSince(before);
}

} // namespace settings
} // namespace coreapi
