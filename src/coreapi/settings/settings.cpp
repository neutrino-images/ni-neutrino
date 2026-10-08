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

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/eventbus.h"
#include "couple.h"
#include "settingstable.h"

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
Result<void> allowedByRow(const Descriptor &d, long value)
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
			if (value < d.min || value > d.max)
				return fail(Status::InvalidArgument, ErrorCode::OutOfRange,
					    "the setting takes " + decimal(d.min) + " to " + decimal(d.max) + also);
			return ok();
		}

		case ValueType::Enum:
			// Held to what the row offers by the caller, which asks the one function
			// that also leaves out what the box lacks.
			return ok();

		case ValueType::Key:
			if (value < d.min || value > d.max)
				return fail(Status::InvalidArgument, ErrorCode::OutOfRange,
					    "the setting takes the code of a key of the remote control");
			// Inside the bounds is still no key where the code is one the input layer
			// never delivers, such as a release.
			if (!keySource().known(value))
				return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
					    "the setting takes the code of a key a remote control can send, or no key");
			return ok();

		case ValueType::String:
		case ValueType::Color:
		case ValueType::List:
		case ValueType::Records:
			break;
	}

	return fail(Status::Internal, ErrorCode::BadTable,
		    "the setting is not of a kind a number is offered for");
}

/* The values a row offers on this box. One function, because the write is held to the
   same set a read answers with: written twice, a caller could be offered a value the
   write turns down. False is a row that offers no set at all and a set the box has no
   entry of. */
bool valuesOffered(const Descriptor &d, std::vector<SettingChoice> &out)
{
	if (d.type != ValueType::Enum)
		return false;

	if (d.values == NULL || d.value_count == 0)
		return false;

	std::vector<SettingChoice> listed;
	listed.reserve(d.value_count);
	for (size_t i = 0; i < d.value_count; ++i)
	{
		const EnumValue &e = d.values[i];
		if (!entryOffered(e))
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

/* Asked last, once the value has passed every rule of its own: a value no row could
   take is wrong whatever the other settings hold, and that answer is the one a caller
   can act on without changing anything else. */
Result<void> allowedByConditions(const Descriptor &d, const BatchOverlay *batch)
{
	// The cast only fits the lookup's untyped context; both readers take it back as const.
	ValueLookup after_batch = { readNumberAfterBatch, readTextAfterBatch,
	                            const_cast<BatchOverlay *>(batch) };
	if (!conditionsHold(d, after_batch))
		return fail(Status::Conflict, ErrorCode::SettingConditionNotMet,
			    "the settings this one depends on do not allow it to be set");
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
				return one;
		}
		records.push_back(members);
	}
	out.records.swap(records);
	return ok();
}

/* Every rule set() holds a value to that is about the value and the row alone: what a write
   takes without asking another setting or touching the store. One function for both, so
   that check() cannot pass a value set() would refuse for itself. */
Result<void> checkValue(const std::string &key, const std::string &value, Checked &out)
{
	const Descriptor *d = findRow(key);
	if (d == NULL)
		return fail(Status::NotFound, ErrorCode::UnknownSetting,
			    "no setting is declared under that key");

	// Ahead of every rule about the value: a held row takes none.
	if (lockedNow(key))
		return fail(Status::Conflict, ErrorCode::SettingLocked,
			    "the box's parental lock fixes this setting");
	/* Not the lock, and worded apart from it: the screen that owns this setting still starts
	   or stops a program, or changes other settings, when it is switched, and a write of the
	   value alone would leave the box half changed. The schema marks such a row held. */
	if (heldNow(key))
		return fail(Status::Conflict, ErrorCode::SettingLocked,
			    "this setting is held: the screen that owns it still applies its effect on the box itself, which a write from here would not");

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
	if (d->secret && value.empty())
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

		/* An identifier is text here and a number where the program keeps it, so the one
		   spelling that survives is the one a channel is named by everywhere else. A value the
		   store cannot read would travel as far as the field, be dropped there and read back as
		   whatever the field already held, on a thread with nobody left to answer. */
		if (d->field.origin == FieldOrigin::ChannelIdField)
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

		Result<void> allowed = allowedByRow(*d, number);
		if (!allowed.ok())
			return fail(allowed.error());

		/* Held to what the box offers, which leaves out the entries it lacks. Every value is
		   refused while the box cannot say what it has: taking one then would write a number
		   the box cannot show, which is a picture nobody gets back from with the remote
		   control. */
		if (d->type == ValueType::Enum)
		{
			std::vector<SettingChoice> offered;
			if (!valuesOffered(*d, offered))
				return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
					    "the setting does not offer that value");

			bool listed = false;
			for (size_t i = 0; !listed && i < offered.size(); ++i)
				listed = offered[i].value == number;
			if (!listed)
				return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
					    "the setting does not offer that value");
		}

		out.number = number;
	}

	out.row = here;
	return ok();
}

} // anonymous namespace

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

/* Read off the declaration: every row naming a file or folder either defaults to an
   absolute name or is a directory whose empty default means "beside another one",
   and the few relative names are listed above. */
bool holdsPath(const Descriptor &d)
{
	// The lists of texts hold files and addresses the box reads from.
	if (d.type == ValueType::List)
		return true;
	if (d.type != ValueType::String || d.key == NULL)
		return false;
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

bool heldNow(const std::string &key)
{
	return heldUntilApplied(key.c_str());
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

Result<void> check(const std::string &key, const std::string &value)
{
	Checked unused;
	return checkValue(key, value, unused);
}

Result<void> set(const std::string &key, const std::string &value, const BatchOverlay *batch, bool onLoop)
{
	Checked c;
	Result<void> checked = checkValue(key, value, c);
	if (!checked.ok())
		return checked;

	Result<void> allowed = allowedByConditions(c.row, batch);
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

	/* Saved here rather than left to the caller: a value the store took and
	   nobody saved is gone the next time the program writes its file. */
	Status saved = onLoop ? settingsSource().persistNow() : settingsSource().persist();
	if (saved != Status::Ok)
		return fail(saved, ErrorCode::SettingNotWritten,
			    "the setting was taken and not saved");

	/* Nothing is applied from here. The value is not in the program's own settings yet: the
	   store holds it and the save above is what carries it there, so whoever applies the change
	   is asked by whatever carries the write. An applier run here would read the value the box
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

void refuseByCondition(Refusals &refused, const std::string &key, const char *message)
{
	if (!alreadyRefused(refused, key))
		refused.push_back(std::make_pair(key,
			Error(Status::Conflict, ErrorCode::SettingConditionNotMet, message)));
}

} // anonymous namespace

void settleBatch(BatchOverlay &batch, Refusals &refused)
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
		for (size_t i = 0; i < made.values.size(); ++i)
		{
			const Descriptor *row = findRow(made.values[i].first);
			if (row == NULL)
				continue;
			if (!allowedByConditions(*row, &made).ok())
				failing.push_back(made.values[i].first);
		}
		if (failing.empty())
		{
			batch = made;
			return;
		}

		bool progress = false;
		for (size_t f = 0; f < failing.size(); ++f)
		{
			if (eraseMember(named, failing[f]))
			{
				refuseByCondition(refused, failing[f],
					"the settings this one depends on do not allow it to be set");
				progress = true;
				continue;
			}

			/* Not named by the caller: a coupling put it. It is reported under its own key, and
			   what asked for it is refused with it, since one without the other is the state the
			   coupling exists to prevent. */
			refuseByCondition(refused, failing[f],
				"the settings this one depends on do not allow it to be set");
			for (size_t a = 0; a < added.size(); ++a)
			{
				if (added[a].key != failing[f])
					continue;
				if (eraseMember(named, added[a].trigger))
				{
					refuseByCondition(refused, added[a].trigger,
						"the setting it brings with it cannot be set");
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
			batch = made;
			return;
		}
	}
}

void writeBatch(const std::vector<std::pair<std::string, std::string> > &members, Refusals &failed,
		bool onLoop)
{
	/* The three passes of one write, so every caller that writes more than one setting gets
	   the couplings and the settling and none can leave them out. The first refuses what is
	   wrong on its own, and only what passed goes into the batch, since a value that never
	   lands must not allow another one. */
	BatchOverlay batch;
	for (size_t i = 0; i < members.size(); ++i)
	{
		Result<void> checked = check(members[i].first, members[i].second);
		if (!checked.ok())
		{
			failed.push_back(std::make_pair(members[i].first, checked.error()));
			continue;
		}
		batch.values.push_back(members[i]);
	}

	settleBatch(batch, failed);

	for (size_t i = 0; i < batch.values.size(); ++i)
	{
		Result<void> done = set(batch.values[i].first, batch.values[i].second, &batch, onLoop);
		if (!done.ok())
			failed.push_back(std::make_pair(batch.values[i].first, done.error()));
	}
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

Result<void> clearSecret(const std::string &key)
{
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

void announceSettingsChanged()
{
	Event e;
	e.type = EventType::SettingsChanged;
	EventBus::instance().publish(e);
}

} // namespace settings
} // namespace coreapi
