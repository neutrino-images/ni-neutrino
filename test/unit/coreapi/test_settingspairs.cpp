/*
 * test_settingspairs.cpp - tests for settings that come in pairs
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

#include "support/catch.hpp"

#include "coreapi/base/schema.h"
#include "coreapi/settings/settingstable.h"

#include "support/counts.h"
#include "support/fakes.h"

// What the load names its key fallbacks by, which the scan cannot read a value of
// and the compiler can.
#include <driver/rcinput.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

/* What the program says about its own settings, read out of its source by the scripts
   beside this file and compared against what the table declares.

   The table is written by hand off the load site and off the screen, and the first five
   rows written that way carried four errors between them, every one of them a plausible
   value and none of them visible to anything that ran: a key paired with the wrong
   field, a bound read off the wrong argument and a label naming a different locale all
   compile, all load, and all read as correct on the page.

   The scripts refuse to print a short answer, so a regex that stopped matching stops the
   build rather than leaving these cases comparing nothing. The counts below are asserted
   here as well, because a check that ran over an empty map looks exactly like a pass. */

using namespace coreapi;

namespace
{

/* Keys the program stores but does not load and save in the one pass the map is read
   from, so no pair for them exists to be found. The first two are loaded through a
   conditional rather than an assignment; the third is loaded through one as well and is
   saved from a value another object holds. Named here rather than left to fail, and
   short on purpose: a list that grows is a check being talked out of its job. */
const char *const kUnpaired[] =
{
	"cpufreq",
	"standby_cpufreq",
	"osd_resolution"
};

bool isUnpaired(const std::string &key)
{
	for (size_t i = 0; i < sizeof(kUnpaired) / sizeof(kUnpaired[0]); ++i)
	{
		if (key == kUnpaired[i])
			return true;
	}
	return false;
}

/* Whether the settings file names this row's key at all, which is what every case below
   the first one really asks.

   Two answers go into it. A row whose value is not in the member it is named after is
   usually not in that file under any key either: a bit of a mask is stored under the
   mask's own key, and a value a daemon keeps is in the daemon's own file. But one of
   them is: the size the box draws at lives in another object of the program, and the
   program writes that object's copy to its own file under this row's key.

   What keeps this from being a way out of the cases below is the case at the foot of
   this file: every row it excludes has to have a key the program really does not
   read. */
/* An element of an array, a list and a list of records are in the file under keys
   the load builds: out of the element's number, or out of a count and the number of
   the entry. So no pair for one is in the map below and no read names the row's own
   key. Each is held to a scan of its own, in test_settingselements.cpp. */
bool storedUnderBuiltKeys(const Descriptor &d)
{
	return d.field.origin == FieldOrigin::Element || d.type == ValueType::List ||
	       d.type == ValueType::Records;
}

bool inSettingsFile(const Descriptor &d)
{
	if (storedUnderBuiltKeys(d))
		return false;
	return valueIsInNamedMember(d.field) || isUnpaired(d.key);
}

std::vector<std::string> split(const std::string &line)
{
	std::vector<std::string> out;
	std::string cur;
	for (size_t i = 0; i < line.size(); ++i)
	{
		if (line[i] == '\t')
		{
			out.push_back(cur);
			cur.clear();
		}
		else
			cur += line[i];
	}
	out.push_back(cur);
	return out;
}

// The files are written by make before the suite runs. A missing one is a
// failure of this case and not something to work around, so nothing here
// invents an empty answer for it.
std::vector<std::vector<std::string> > readTable(const char *path)
{
	std::vector<std::vector<std::string> > rows;
	FILE *f = fopen(path, "r");
	if (f == NULL)
		return rows;

	std::string line;
	int c;
	while ((c = fgetc(f)) != EOF)
	{
		if (c == '\n')
		{
			if (!line.empty())
				rows.push_back(split(line));
			line.clear();
		}
		else
			line += (char) c;
	}
	if (!line.empty())
		rows.push_back(split(line));

	fclose(f);
	return rows;
}

// One bound the screens state. Several rows can name the same field, because
// one setting is offered on more than one screen where the boxes differ.
struct Offered
{
	std::string min;
	std::string max;
	std::string label;
	std::string where;
};

/* One default the program falls back to. A key loaded in two conditional arms
   has one of these for each, so a row is held to whichever arm the build it is
   written for takes rather than to only the first. kind is int, str, expr or
   none, and the last two are what no scan can turn into a value. */
struct Fallback
{
	std::string kind;
	std::string value;
};

struct Loaded
{
	std::string             field;
	std::vector<Fallback>   defaults;
};

const std::map<std::string, Loaded> &loaded()
{
	static std::map<std::string, Loaded> m;
	static bool done = false;
	if (!done)
	{
		std::vector<std::vector<std::string> > rows = readTable(COREAPI_PAIRS_FILE);
		for (size_t i = 0; i < rows.size(); ++i)
		{
			if (rows[i].size() != 4)
				continue;
			Loaded &l = m[rows[i][0]];
			l.field = rows[i][1];
			Fallback f;
			f.kind = rows[i][2];
			f.value = rows[i][3];
			l.defaults.push_back(f);
		}
		done = true;
	}
	return m;
}

/* The same pairs, but only the arm of each conditional this build compiles, so a
   key loaded differently on another model has the one fallback this model gets. */
const std::map<std::string, Loaded> &loadedHere()
{
	static std::map<std::string, Loaded> m;
	static bool done = false;
	if (!done)
	{
		std::vector<std::vector<std::string> > rows = readTable(COREAPI_PAIRS_BUILT_FILE);
		for (size_t i = 0; i < rows.size(); ++i)
		{
			if (rows[i].size() != 4)
				continue;
			Loaded &l = m[rows[i][0]];
			l.field = rows[i][1];
			Fallback f;
			f.kind = rows[i][2];
			f.value = rows[i][3];
			l.defaults.push_back(f);
		}
		done = true;
	}
	return m;
}

struct LoadValue
{
	const char *key;
	long        value;
};

// Written by the build from the load's own fallbacks, one entry for each that is a key code.
const LoadValue kLoadValues[] =
{
#include COREAPI_LOAD_VALUES_INC
	{ NULL, 0 }
};

std::string fallbacks(const std::vector<Fallback> &v)
{
	std::string s;
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (i > 0)
			s += ", ";
		s += v[i].kind + " " + v[i].value;
	}
	return s;
}

const std::set<std::string> &keysRead()
{
	static std::set<std::string> s;
	static bool loaded = false;
	if (!loaded)
	{
		std::vector<std::vector<std::string> > rows = readTable(COREAPI_KEYS_FILE);
		for (size_t i = 0; i < rows.size(); ++i)
			s.insert(rows[i][0]);
		loaded = true;
	}
	return s;
}

const std::set<std::string> &localeNames()
{
	static std::set<std::string> s;
	static bool loaded = false;
	if (!loaded)
	{
		std::vector<std::vector<std::string> > rows = readTable(COREAPI_LOCALES_FILE);
		for (size_t i = 0; i < rows.size(); ++i)
			s.insert(rows[i][0]);
		loaded = true;
	}
	return s;
}

/* Sites a screen builds from the declaration, counted toward the figures below
   and compared against nothing. Filled by the readers that follow. */
size_t derivedLabels = 0;

const std::map<std::string, std::vector<Offered> > &offered()
{
	static std::map<std::string, std::vector<Offered> > m;
	static bool loaded = false;
	if (!loaded)
	{
		std::vector<std::vector<std::string> > rows = readTable(COREAPI_BOUNDS_FILE);
		for (size_t i = 0; i < rows.size(); ++i)
		{
			if (rows[i].size() != 5)
				continue;
			// built from the declaration: no bounds on the screen to compare
			if (rows[i][1] == "DERIVED")
				continue;
			Offered o;
			o.min = rows[i][1];
			o.max = rows[i][2];
			o.label = rows[i][3];
			o.where = rows[i][4];
			m[rows[i][0]].push_back(o);
		}
		loaded = true;
	}
	return m;
}

/* One label a screen states beside a setting it offers as neither a range nor a
   table of choices. The same field can be stated on more than one screen, as a
   range can. */
struct Stated
{
	std::string label;
	std::string where;
};

const std::map<std::string, std::vector<Stated> > &stated()
{
	static std::map<std::string, std::vector<Stated> > m;
	static bool loaded = false;
	if (!loaded)
	{
		std::vector<std::vector<std::string> > rows = readTable(COREAPI_LABELS_FILE);
		for (size_t i = 0; i < rows.size(); ++i)
		{
			if (rows[i].size() != 3)
				continue;
			// built from the declaration: no label on the screen to compare
			if (rows[i][1] == "DERIVED")
			{
				++derivedLabels;
				continue;
			}
			Stated t;
			t.label = rows[i][1];
			t.where = rows[i][2];
			m[rows[i][0]].push_back(t);
		}
		loaded = true;
	}
	return m;
}

std::string states(const std::vector<Stated> &v)
{
	std::string s;
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (i > 0)
			s += ", ";
		s += v[i].where + " " + v[i].label;
	}
	return s;
}

std::string sites(const std::vector<Offered> &v)
{
	std::string s;
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (i > 0)
			s += ", ";
		s += v[i].where + " " + v[i].min + ".." + v[i].max + " " + v[i].label;
	}
	return s;
}

} // namespace

TEST_CASE("the map read out of the program is the size the program has", "[settingspairs]")
{
	INFO("key to field pairs read: " << loaded().size());
	REQUIRE(loaded().size() > 0);
	recordCount("key to field pairs read out of the program", loaded().size());

	size_t comparable = 0;
	for (std::map<std::string, Loaded>::const_iterator it = loaded().begin();
	     it != loaded().end(); ++it)
	{
		for (size_t j = 0; j < it->second.defaults.size(); ++j)
		{
			if (it->second.defaults[j].kind == "int" || it->second.defaults[j].kind == "str")
			{
				++comparable;
				break;
			}
		}
	}
	INFO("keys whose default the scan can compare: " << comparable);
	recordCount("keys whose default the scan can compare", comparable);

	INFO("keys read anywhere in the tree: " << keysRead().size());
	recordCount("keys read anywhere in the tree", keysRead().size());

	recordCount("fields the bound scan read", offered().size());

	INFO("locale names the program has: " << localeNames().size());
	recordCount("locale names the program has", localeNames().size());

	size_t pairings = 0;
	for (std::map<std::string, std::vector<Stated> >::const_iterator it = stated().begin();
	     it != stated().end(); ++it)
		pairings += it->second.size();
	pairings += derivedLabels;
	INFO("labels stated beside a field: " << pairings << " over " << stated().size() << " fields");
	recordCount("labels stated beside a field", pairings);
	recordCount("fields with a stated label", stated().size());
}

TEST_CASE("every declared row names the field the program loads its key into", "[settingspairs]")
{
	size_t checked = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.field.name == NULL)
			continue;

		INFO("row " << d.key << " points at g_settings." << d.field.name);
		if (isUnpaired(d.key) || !inSettingsFile(d))
			continue;

		std::map<std::string, Loaded>::const_iterator it = loaded().find(d.key);
		CHECK(it != loaded().end());
		if (it == loaded().end())
			continue;

		++checked;
		CHECK(it->second.field == std::string(d.field.name));
	}

	INFO("rows compared against the load and save pass: " << checked);
	recordCount("rows compared against the load and save pass", checked);
}

/* The weaker question, and the only one that can be asked of a setting the
   program reads outside the one pass the map above comes from. Font sizes are
   read from the screens that use them, so a row for one has no save beside its
   load to pair with, but its key is still a key the program reads. */
TEST_CASE("every declared key is one the program reads", "[settingspairs]")
{
	size_t checked = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (!inSettingsFile(d))
			continue;
		++checked;
		INFO("row " << d.key);
		CHECK(keysRead().count(d.key) == 1);
	}

	INFO("keys compared: " << checked);
	recordCount("keys compared against what the program reads", checked);
}

/* The default, which is the third thing on the line the pairing already reads and the
   widest check there is: the program states one for nearly every setting, against the
   few dozen whose range a screen states. Two of the three errors that last survived a
   whole run were defaults. A default the program writes as an expression cannot be
   compared and is named rather than passed over, because a row nobody can check is a
   fact about the check and not about the row. */
TEST_CASE("every declared default is the one the program falls back to", "[settingspairs]")
{
	size_t checked = 0;
	std::string uncheckable;

	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.field.name == NULL || !inSettingsFile(d))
			continue;

		std::map<std::string, Loaded>::const_iterator it = loaded().find(d.key);
		if (it == loaded().end())
			continue;

		const std::vector<Fallback> &v = it->second.defaults;
		bool agrees = false;
		bool comparable = false;

		for (size_t j = 0; j < v.size(); ++j)
		{
			/* An identifier is a number where the program loads it and text
			   here, so the two are compared as the number both of them mean.
			   Read as a spelling against a fallback that was never written as
			   one, it would be a row nothing could check. */
			if (d.field.origin == FieldOrigin::ChannelIdField)
			{
				if (v[j].kind != "int")
					continue;
				comparable = true;
				unsigned long long declared = 0;
				if (d.default_string != NULL &&
				    readChannelIdText(d.default_string, declared) &&
				    (unsigned long long) strtoll(v[j].value.c_str(), NULL, 10) == declared)
					agrees = true;
			}
			// A String carries its default as text and every other kind as a
			// number, so only the matching sort of fallback answers for it.
			else if (d.type == ValueType::String)
			{
				if (v[j].kind != "str")
					continue;
				comparable = true;
				if (d.default_string != NULL && v[j].value == std::string(d.default_string))
					agrees = true;
			}
			else
			{
				if (v[j].kind != "int")
					continue;
				comparable = true;
				if (strtol(v[j].value.c_str(), NULL, 10) == d.default_int)
					agrees = true;
			}
		}

		if (!comparable)
		{
			uncheckable += (uncheckable.empty() ? "" : ", ");
			uncheckable += d.key + std::string(" [") + fallbacks(v) + "]";
			continue;
		}

		std::string declared;
		if (d.field.origin == FieldOrigin::ChannelIdField)
			declared = std::string("id ") + (d.default_string != NULL ? d.default_string : "");
		else if (d.type == ValueType::String)
			declared = std::string("str ") + (d.default_string != NULL ? d.default_string : "");
		else
		{
			char n[32];
			snprintf(n, sizeof(n), "int %ld", d.default_int);
			declared = n;
		}

		++checked;
		INFO("row " << d.key << " declares " << declared
		     << ", the program falls back to " << fallbacks(v));
		CHECK(agrees);
	}

	INFO("defaults compared: " << checked
	     << "; rows whose default the program writes as an expression: " << uncheckable);
	recordCount("defaults compared against the program", checked);
}

/* The case above lets a row agree with any arm of the load, which is what the
   table written for every model needs and is also why it cannot see a row whose
   constant is the arm of another model: the loader has the other arm too. This
   holds every row to the one arm this build compiles, with the row's own function
   applied where it has one, which is the number a read of an unset key and a
   reset answer on this box. */
TEST_CASE("every declared default is the one this build's own load falls back to", "[settingspairs]")
{
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	size_t checked = 0;

	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.field.name == NULL || !inSettingsFile(d))
			continue;
		if (d.field.origin == FieldOrigin::ChannelIdField)
			continue;

		std::map<std::string, Loaded>::const_iterator it = loadedHere().find(d.key);
		if (it == loadedHere().end() || it->second.defaults.size() != 1)
			continue;

		const Fallback &f = it->second.defaults[0];
		if (d.type == ValueType::String)
		{
			if (f.kind != "str")
				continue;
			++checked;
			INFO("row " << d.key << " declares " << (d.default_string != NULL ? d.default_string : "")
			     << ", this build falls back to " << f.value);
			CHECK(d.default_string != NULL);
			if (d.default_string != NULL)
				CHECK(f.value == std::string(d.default_string));
		}
		else
		{
			if (f.kind != "int")
				continue;
			++checked;
			INFO("row " << d.key << " declares " << defaultInt(d)
			     << ", this build falls back to " << f.value);
			CHECK(strtol(f.value.c_str(), NULL, 10) == defaultInt(d));
		}
	}

	INFO("defaults compared against this build's arm: " << checked);
	recordCount("defaults compared against this build's own load", checked);

	// The ones the load writes as a key code: the compiler has given each its number.
	size_t named = 0;
	for (size_t v = 0; kLoadValues[v].key != NULL; ++v)
	{
		const Descriptor *d = NULL;
		for (size_t i = 0; i < settingsTableCount() && d == NULL; ++i)
		{
			if (std::string(settingsTable()[i].key) == kLoadValues[v].key)
				d = &settingsTable()[i];
		}
		if (d == NULL || d->field.name == NULL || !inSettingsFile(*d))
			continue;
		if (d->type == ValueType::String || d->field.origin == FieldOrigin::ChannelIdField)
			continue;

		++named;
		INFO("row " << d->key << " declares " << (int32_t) defaultInt(*d)
		     << ", this build falls back to " << (int32_t) kLoadValues[v].value);
		CHECK((int32_t) kLoadValues[v].value == (int32_t) defaultInt(*d));
	}
	INFO("defaults named as key codes compared against this build's arm: " << named);
	recordCount("key code defaults compared against this build's own load", named);
}

/* The weaker question about a label, and the one that can be asked of every row
   rather than only of the ones a number chooser offers. A label naming nothing
   shows as the key itself on the screen, which reads as a missing translation
   and not as a wrong row. */
TEST_CASE("every declared label names a locale the program has", "[settingspairs]")
{
	size_t checked = 0;
	size_t nameless = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];

		if (d.label_key == NULL)
		{
			++nameless;
		}
		else
		{
			++checked;
			INFO("row " << d.key << " declares label " << d.label_key);
			CHECK(localeNames().count(d.label_key) == 1);
		}

		if (d.hint_key != NULL)
		{
			INFO("row " << d.key << " declares hint " << d.hint_key);
			CHECK(localeNames().count(d.hint_key) == 1);
		}

		// The other shape a row may take carries a label of its own.
		if (d.field.otherwise != NULL && d.field.otherwise->label_key != NULL)
		{
			++checked;
			INFO("row " << d.key << " declares label " << d.field.otherwise->label_key << " for its other shape");
			CHECK(localeNames().count(d.field.otherwise->label_key) == 1);
		}
		if (d.field.otherwise != NULL && d.field.otherwise->hint_key != NULL)
		{
			INFO("row " << d.key << " declares hint " << d.field.otherwise->hint_key << " for its other shape");
			CHECK(localeNames().count(d.field.otherwise->hint_key) == 1);
		}
	}

	INFO("labels compared: " << checked << ", rows the program has no name for: " << nameless);
	recordCount("labels compared against the locale names", checked);
}

// The same question of the text each heading of the menu is titled with.
TEST_CASE("every heading of the menu names a locale the program has", "[settingspairs]")
{
	size_t headed = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.group == NULL)
			continue;
		++headed;
		INFO("row " << d.key << " under " << d.group->label_key);
		CHECK(localeNames().count(d.group->label_key) == 1);
	}
	CHECK(headed > 0);
}

/* The other half of allowing a row to say it has no name: one that says so
   while a screen does name the field is a name thrown away, and a frontend
   would then show the key where the box shows a word. This reads the ranges the
   screens offer; the case beside it in the choices file reads the choosers. */
TEST_CASE("a row with no label is one no number chooser names", "[settingspairs]")
{
	size_t nameless = 0;
	std::string named;

	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.label_key != NULL || d.field.name == NULL)
			continue;

		++nameless;
		std::map<std::string, std::vector<Offered> >::const_iterator it =
			offered().find(d.field.name);
		if (it == offered().end())
			continue;

		named += (named.empty() ? "" : ", ");
		named += std::string(d.key) + " " + sites(it->second);
	}

	INFO("rows with no label that a number chooser does name: " << named);
	CHECK(named.empty());

	INFO("rows the program has no name for: " << nameless);
	REQUIRE(nameless > 0);
}

/* The label, which the pairing cannot see either. One of the five rows named a
   locale that exists and belongs to a different item, which reads as right on
   the page and is wrong on the screen. */
TEST_CASE("every declared label is the one the screen offers", "[settingspairs]")
{
	size_t checked = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.field.name == NULL || d.label_key == NULL)
			continue;

		std::map<std::string, std::vector<Offered> >::const_iterator it =
			offered().find(d.field.name);
		if (it == offered().end())
			continue;

		bool agrees = false;
		bool comparable = false;
		for (size_t j = 0; j < it->second.size(); ++j)
		{
			if (it->second[j].label == "?")
				continue;
			comparable = true;
			if (it->second[j].label == std::string(d.label_key))
				agrees = true;
		}

		if (!comparable)
			continue;

		++checked;
		INFO("row " << d.key << " declares label " << d.label_key
		     << ", the screens offer " << sites(it->second));
		CHECK(agrees);
	}

	// A tree whose number choosers all come from their rows has nothing here to compare.
	if (checked == 0)
		SUCCEED("no screen builds a number chooser for a declared row by hand");
	INFO("labels compared: " << checked);
	recordCount("labels compared against a number chooser", checked);
}

/* The rest of the rows, which neither the case above nor the one in the choices file
   reaches: a setting offered through a forwarder, a string chooser or one of the text
   inputs has a label nothing compares, and a wrong one there is a locale that exists,
   belonging to the item beside it on the screen. That is the error the first five rows
   carried.

   The label is read out of the argument each widget carries the setting in and not out
   of the whole call, because a forwarder takes whether it is active in the argument
   before the one it shows. Read whole, seven rows here pair a label with a setting the
   screen only asks about. */
TEST_CASE("every declared label is the one the screen states beside its field", "[settingspairs]")
{
	size_t checked = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.field.name == NULL || d.label_key == NULL)
			continue;

		std::map<std::string, std::vector<Stated> >::const_iterator it =
			stated().find(d.field.name);
		if (it == stated().end())
			continue;

		bool agrees = false;
		for (size_t j = 0; j < it->second.size(); ++j)
		{
			if (it->second[j].label == std::string(d.label_key))
				agrees = true;
		}

		++checked;
		INFO("row " << d.key << " declares label " << d.label_key
		     << ", the screens state " << states(it->second));
		CHECK(agrees);
	}

	INFO("labels compared: " << checked);
	recordCount("labels compared against a stated field", checked);
}

/* The names a choice offers, which no check above reads: a row's own label is
   compared against the screen, and the words beside each of its values are not.
   One naming nothing shows on screen as the name itself. */
TEST_CASE("every value a choice offers names a locale the program has", "[settingspairs]")
{
	size_t checked = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		for (size_t v = 0; v < d.value_count; ++v)
		{
			++checked;
			INFO("row " << d.key << " offers " << d.values[v].value
			     << " as " << (d.values[v].label_key ? d.values[v].label_key : "(none)"));
			// An entry with a text of its own, such as a format name, has no locale to name.
			if (d.values[v].label_key == NULL)
			{
				CHECK(d.values[v].label_text != NULL);
				continue;
			}
			CHECK(localeNames().count(d.values[v].label_key) == 1);
		}
	}

	INFO("choices compared: " << checked);
	recordCount("choice value names compared", checked);
}

/* The rows the three cases above step over, held to what they do say rather than left
   out of every check at once.

   Each of them declares that its value is not in the settings file, and the one thing
   that can be asked of that declaration here is whether it is true: a row whose key the
   program does read is one the cases above should have held to the file. The rest of
   what each kind promises is a question about running code and is asked where code
   runs.

   The count is asserted for the reason every count here is: a table that declared none
   of these would satisfy every line above by looking at nothing. */
TEST_CASE("a row whose value is not in the settings file is not in it", "[settingspairs]")
{
	size_t checked = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (inSettingsFile(d) || storedUnderBuiltKeys(d))
			continue;

		++checked;
		INFO("row " << d.key << " says its value is not in the settings file");
		CHECK(keysRead().count(d.key) == 0);
		CHECK(loaded().find(d.key) == loaded().end());

		/* And it still names a member, because that is what the cases reading
		   the screens find its bounds, its label and its choices under. */
		INFO("row " << d.key << " is found under g_settings." << (d.field.name ? d.field.name : "(none)"));
		CHECK(d.field.name != NULL);
	}

	INFO("rows whose value is not in the settings file: " << checked);
	recordCount("rows whose value is not in the settings file", checked);
	REQUIRE(checked > 0);
}

/* ------------------------------------------------------------ the headings of the menus

   What settings-groups.txt holds is said in scan/groups.awk. A row's heading is held to
   the screens that build it: one of them has to file it there, by the text its widget is
   introduced with, by the titled separator above it, or by the widget's title, which is
   what a submenu inside a submenu carries of the one that leads to it. A row standing on
   the section's own page has to be built in the menu that offers the section, under no
   titled separator. A row no screen builds from the declaration has nothing to be held to
   and is not compared. */
namespace
{

struct GroupSite
{
	std::vector<std::string> intro;
	std::string sep;
	std::string title;
	std::string where;
};

const std::map<std::string, std::vector<GroupSite> > &groupSites()
{
	static std::map<std::string, std::vector<GroupSite> > m;
	static bool loaded = false;
	if (!loaded)
	{
		std::vector<std::vector<std::string> > rows = readTable(COREAPI_GROUPS_FILE);
		for (size_t i = 0; i < rows.size(); ++i)
		{
			if (rows[i].size() != 6 || rows[i][0] != "S")
				continue;
			GroupSite s;
			const std::string &intro = rows[i][2];
			for (size_t at = 0; at <= intro.size();)
			{
				size_t bar = intro.find('|', at);
				if (bar == std::string::npos)
					bar = intro.size();
				s.intro.push_back(intro.substr(at, bar - at));
				at = bar + 1;
			}
			s.sep = rows[i][3];
			s.title = rows[i][4];
			s.where = rows[i][5];
			m[rows[i][1]].push_back(s);
		}
		loaded = true;
	}
	return m;
}

// The heading each menu that offers a whole section is introduced with.
struct SectionPage
{
	const char *section;
	const char *heading;
};

const SectionPage kSectionPages[] =
{
	{ "audio", "mainsettings.audio" },
	{ "cam", "ci.settings" },
	{ "display", "lcdmenu.head" },
	{ "general", "languagesetup.head" },
	{ "hdd", "hdd_settings" },
	{ "keybindings", "mainsettings.keybinding" },
	{ "network", "mainsettings.network" },
	{ "osd", "mainsettings.osd" },
	{ "parental", "parentallock.parentallock" },
	{ "player", "mainmenu.media" },
	{ "recording", "mainsettings.recording" },
	{ "update", "flashupdate.settings" },
	{ "video", "mainsettings.video" },
	{ "weather", "miscsettings.onlineservices" },
};

bool isPage(const std::string &section, const std::string &heading)
{
	for (size_t i = 0; i < sizeof(kSectionPages) / sizeof(kSectionPages[0]); ++i)
	{
		if (section == kSectionPages[i].section && heading == kSectionPages[i].heading)
			return true;
	}
	return false;
}

/* Rows whose heading the scan cannot read off the screen that builds them, each with why.
   Held here rather than left uncompared, so that one more such row is a decision and not
   something nobody noticed; a row listed here that the scan has learnt to read fails, so
   the list cannot outlive its reason. */
struct UnreadHeading
{
	const char *key;
	const char *why;
};

const UnreadHeading kUnreadHeadings[] =
{
	{ "analog_out", "offered only by the audio selection while something plays, no settings menu" },
	{ "hdd_fs", "a drive's own menu, titled with the drive's name at run time" },
	{ "lcd4l_brightness", "the display type submenu inside the LCD4Linux menu, opened by an action and not a widget" },
	{ "lcd4l_brightness_standby", "the display type submenu inside the LCD4Linux menu, opened by an action and not a widget" },
	{ "lcd4l_skin", "the display type submenu inside the LCD4Linux menu, opened by an action and not a widget" },
	{ "lcd4l_skin_radio", "the display type submenu inside the LCD4Linux menu, opened by an action and not a widget" },
	{ "personalize_pincode", "built into a widget the function is handed under a name of its own" },
	{ "personalize_pinstatus", "built into a widget the function is handed under a name of its own" },
	{ "shutdown_block_while_recording", "the energy menu is introduced with a text its caller passes in" },
	{ "shutdown_count", "the energy menu is introduced with a text its caller passes in" },
	{ "shutdown_real", "the energy menu is introduced with a text its caller passes in" },
	{ "shutdown_real_rcdelay", "the energy menu is introduced with a text its caller passes in" },
	{ "sleeptimer_min", "the energy menu is introduced with a text its caller passes in" },
	{ "softupdate_proxypassword", "the proxy menu is introduced with a text chosen by its caller" },
	{ "softupdate_proxyserver", "the proxy menu is introduced with a text chosen by its caller" },
	{ "softupdate_proxyusername", "the proxy menu is introduced with a text chosen by its caller" },
};

const char *unreadWhy(const std::string &key)
{
	for (size_t i = 0; i < sizeof(kUnreadHeadings) / sizeof(kUnreadHeadings[0]); ++i)
	{
		if (key == kUnreadHeadings[i].key)
			return kUnreadHeadings[i].why;
	}
	return NULL;
}

bool siteFiles(const GroupSite &s, const Descriptor &d)
{
	if (d.group != NULL)
	{
		const std::string heading = d.group->label_key;
		return std::find(s.intro.begin(), s.intro.end(), heading) != s.intro.end() ||
		       s.sep == heading || s.title == heading;
	}
	if (s.sep != "-")
		return false;
	for (size_t i = 0; i < s.intro.size(); ++i)
	{
		if (isPage(d.section, s.intro[i]))
			return true;
	}
	return isPage(d.section, s.title);
}

} // anonymous namespace

TEST_CASE("every row stands under the heading a screen files it under", "[settingspairs][groups]")
{
	const std::map<std::string, std::vector<GroupSite> > &sites = groupSites();
	REQUIRE_FALSE(sites.empty());

	size_t compared = 0, unread = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		std::map<std::string, std::vector<GroupSite> >::const_iterator at = sites.find(d.key);
		if (at == sites.end())
			continue;
		++compared;
		bool filed = false;
		std::string seen;
		for (size_t s = 0; s < at->second.size(); ++s)
		{
			filed = filed || siteFiles(at->second[s], d);
			seen += " " + at->second[s].where;
		}
		const char *why = unreadWhy(d.key);
		INFO("row " << d.key << " of " << d.section << " under "
		     << (d.group != NULL ? d.group->label_key : "no heading") << ", built at" << seen);
		if (why != NULL)
		{
			++unread;
			INFO("listed as unread: " << why);
			CHECK_FALSE(filed);
			continue;
		}
		CHECK(filed);
	}

	// Every row listed as unread is still one a screen builds.
	for (size_t u = 0; u < sizeof(kUnreadHeadings) / sizeof(kUnreadHeadings[0]); ++u)
	{
		INFO("listed as unread: " << kUnreadHeadings[u].key);
		CHECK(sites.count(kUnreadHeadings[u].key) == 1);
	}

	INFO("rows compared: " << compared << ", of them listed as unread: " << unread);
	/* Below this the scan or the table has stopped matching. A build compares some 380 to
	   480, by how many of the rows the screens build it links. */
	CHECK(compared > 300);
}

/* The order of a section's headings, held to the order in which the screens lead to them.
   A file that leads to two or more headings of one section, by a forwarder or a titled
   separator, has to do so in the order the headings state. */
TEST_CASE("a section's headings stand in the order the screens lead to them", "[settingspairs][groups]")
{
	std::map<std::string, std::vector<const MenuGroup *> > bySection;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.group == NULL)
			continue;
		std::vector<const MenuGroup *> &list = bySection[d.section];
		if (std::find(list.begin(), list.end(), d.group) == list.end())
			list.push_back(d.group);
	}
	REQUIRE_FALSE(bySection.empty());

	std::vector<std::vector<std::string> > rows = readTable(COREAPI_GROUPS_FILE);
	std::map<std::string, std::vector<std::string> > byFile;
	std::vector<std::string> files;
	for (size_t i = 0; i < rows.size(); ++i)
	{
		if (rows[i].size() != 3 || rows[i][0] != "H")
			continue;
		const std::string file = rows[i][2].substr(0, rows[i][2].rfind(':'));
		if (byFile.find(file) == byFile.end())
			files.push_back(file);
		byFile[file].push_back(rows[i][1]);
	}
	REQUIRE_FALSE(files.empty());

	size_t held = 0;
	for (std::map<std::string, std::vector<const MenuGroup *> >::const_iterator s = bySection.begin(); s != bySection.end(); ++s)
	{
		for (size_t f = 0; f < files.size(); ++f)
		{
			const std::vector<std::string> &led = byFile[files[f]];
			std::vector<const MenuGroup *> met;
			for (size_t h = 0; h < led.size(); ++h)
			{
				for (size_t g = 0; g < s->second.size(); ++g)
				{
					if (led[h] == s->second[g]->label_key &&
					    std::find(met.begin(), met.end(), s->second[g]) == met.end())
						met.push_back(s->second[g]);
				}
			}
			for (size_t m = 1; m < met.size(); ++m)
			{
				INFO(files[f] << " leads to " << met[m - 1]->label_key << " before " << met[m]->label_key
				     << " of " << s->first);
				CHECK(met[m - 1]->order < met[m]->order);
				++held;
			}
		}
	}
	INFO("pairs of headings held to a screen: " << held);
	// The tree holds some forty such pairs; none says the scan has stopped reading.
	CHECK(held > 20);
}
