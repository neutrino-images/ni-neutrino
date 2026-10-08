/*
 * test_settingselements.cpp - tests for the rows of arrays, nested structs, lists and flag files
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

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/schema.h"
#include "coreapi/settings/menuspec.h"
#include "coreapi/settings/settings.h"
#include "coreapi/settings/settingsfield.h"
#include "coreapi/settings/settingstable.h"
#include "support/counts.h"
#include "support/fakes.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <driver/neutrino_msg_t.h>

#include <sys/stat.h>
#include <unistd.h>

/* What an array, a list and a nested struct of the settings are held to.

   The keys of an array's elements are built by the load out of the element's
   number, so the scans that read a literal key out of the source cannot see them,
   and a row naming the wrong element or the wrong key would read as right on every
   page there is. Two things hold them here: the load's own loops and tables, read
   out of the source and compared with each row, and a write of each element
   through the layer, with a look at which member of the struct moved. */

using namespace coreapi;

namespace
{

bool fixtureSaved = false;
bool fixtureSave() { fixtureSaved = true; return true; }

struct RealStore
{
	SNeutrinoSettings     values;
	FakeCommandSink       sink;
	InstalledSink         installed;
	ClearedSettingsSource cleared;

	RealStore() : values(SNeutrinoSettings()), installed(&sink)
	{
		fixtureSaved = false;
		installRealSettingsSource(&values, fixtureSave);
	}

	~RealStore() { installRealSettingsSource(NULL, NULL); }
};

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

std::vector<std::vector<std::string> > readLines(const char *path)
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

// One fallback the load states for an element.
struct Fallback
{
	std::string kind;
	std::string value;
};

// A loop that builds the key from the number: the format and the fallbacks.
struct Loop
{
	std::string           format;
	std::vector<Fallback> fallbacks;
};

// A table entry: the key it names and the fallback.
struct Entry
{
	std::string key;
	Fallback    fallback;
};

struct Scan
{
	// The loops by member, one for each format the member is loaded under.
	std::map<std::string, std::vector<Loop> > loops;
	// The table entries by member and enumerator.
	std::map<std::string, std::map<std::string, Entry> > entries;
};

struct EnumeratorValue
{
	const char *member;
	const char *name;
	long        value;
};

const EnumeratorValue kEnumerators[] =
{
#include COREAPI_ELEMENT_NAMES_INC
	{ NULL, NULL, 0 }
};

long enumeratorValue(const std::string &member, const std::string &name, bool &found)
{
	for (size_t i = 0; kEnumerators[i].member != NULL; ++i)
	{
		if (member == kEnumerators[i].member && name == kEnumerators[i].name)
		{
			found = true;
			return kEnumerators[i].value;
		}
	}
	found = false;
	return 0;
}

const Scan &scan()
{
	static Scan s;
	static bool done = false;
	if (done)
		return s;

	std::vector<std::vector<std::string> > rows = readLines(COREAPI_ELEMENTS_FILE);
	for (size_t i = 0; i < rows.size(); ++i)
	{
		const std::vector<std::string> &r = rows[i];
		if (r.size() >= 5 && r[0] == "L")
		{
			Fallback f;
			f.kind = r[3];
			f.value = r[4];
			std::vector<Loop> &v = s.loops[r[1]];
			size_t at = 0;
			while (at < v.size() && v[at].format != r[2])
				++at;
			if (at == v.size())
			{
				v.push_back(Loop());
				v.back().format = r[2];
			}
			v[at].fallbacks.push_back(f);
		}
		else if (r.size() >= 6 && r[0] == "T")
		{
			Entry e;
			e.key = r[3];
			e.fallback.kind = r[4];
			e.fallback.value = r[5];
			s.entries[r[1]][r[2]] = e;
		}
	}
	done = true;
	return s;
}

// The key a format names for an element: the number written where %d stands.
std::string formatted(const std::string &format, long index)
{
	char buf[160];
	snprintf(buf, sizeof(buf), format.c_str(), (int) index);
	return buf;
}

bool isElement(const Descriptor &d)
{
	return d.field.origin == FieldOrigin::Element;
}

size_t elementRowCount()
{
	size_t n = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		if (isElement(settingsTable()[i]))
			++n;
	}
	return n;
}

} // namespace

/* The load's own statement of each element, against the row for it: the key and the
   fallback. A row for an element the load does not load, or under another key than the
   one it loads it under, is a row nothing ever reads back. */
TEST_CASE("every element row is the element the program loads under its key", "[settingselements]")
{
	size_t compared = 0;
	size_t defaults = 0;
	std::string uncheckable;

	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (!isElement(d))
			continue;

		const std::string member = d.field.name;
		const long index = d.field.extra->index;
		INFO("row " << d.key << " is element " << index << " of " << member);

		std::vector<Fallback> fallbacks;
		bool keyed = false;

		// A table names each element, by the enumerator of the array it stands at.
		std::map<std::string, std::map<std::string, Entry> >::const_iterator t = scan().entries.find(member);
		if (t != scan().entries.end())
		{
			for (std::map<std::string, Entry>::const_iterator e = t->second.begin(); e != t->second.end(); ++e)
			{
				bool found = false;
				if (enumeratorValue(member, e->first, found) != index || !found)
					continue;
				CHECK(e->second.key == std::string(d.key));
				keyed = e->second.key == std::string(d.key);
				fallbacks.push_back(e->second.fallback);
			}
		}

		// A loop names every element with one format.
		std::map<std::string, std::vector<Loop> >::const_iterator l = scan().loops.find(member);
		if (l != scan().loops.end())
		{
			for (size_t k = 0; k < l->second.size(); ++k)
			{
				if (formatted(l->second[k].format, index) != std::string(d.key))
					continue;
				keyed = true;
				fallbacks.insert(fallbacks.end(), l->second[k].fallbacks.begin(), l->second[k].fallbacks.end());
			}
		}

		CHECK(keyed);
		if (!keyed)
			continue;
		++compared;

		bool comparable = false;
		bool agrees = false;
		for (size_t f = 0; f < fallbacks.size(); ++f)
		{
			if (d.field.read_text != NULL && d.field.origin == FieldOrigin::Element &&
			    d.type == ValueType::String)
			{
				if (fallbacks[f].kind == "str")
				{
					comparable = true;
					agrees = agrees || (d.default_string != NULL && fallbacks[f].value == d.default_string);
				}
				else if (fallbacks[f].kind == "int" && d.default_string != NULL)
				{
					// An identifier is a number to the load and text here.
					comparable = true;
					unsigned long long id = 0;
					agrees = agrees || (readChannelIdText(d.default_string, id) &&
					                    (unsigned long long) strtoll(fallbacks[f].value.c_str(), NULL, 10) == id);
				}
			}
			else if (fallbacks[f].kind == "int")
			{
				comparable = true;
				agrees = agrees || strtol(fallbacks[f].value.c_str(), NULL, 10) == defaultInt(d);
			}
		}

		if (!comparable)
		{
			uncheckable += (uncheckable.empty() ? "" : ", ");
			uncheckable += d.key;
			continue;
		}
		++defaults;
		INFO("row " << d.key << " declares " << (d.type == ValueType::String ? std::string(d.default_string) : std::to_string(defaultInt(d))));
		CHECK(agrees);
	}

	INFO("element rows compared: " << compared << ", their defaults: " << defaults
	     << "; rows whose default the program writes as an expression: " << uncheckable);
	recordCount("element rows compared against the load", compared);
	recordCount("element defaults compared against the load", defaults);
	REQUIRE(compared > 0);
}

/* The other direction: every element the load's tables list has a row, bar the ones
   that are left out on purpose. A table entry with no row is a setting nobody can
   reach, and the check that the struct is covered counts members and not elements. */
TEST_CASE("every element a table of the load lists has a row", "[settingselements]")
{
	// Nothing in the tree reads it, so a row would offer a setting that does nothing.
	std::set<std::string> omitted;
	omitted.insert("lcd_setting LCD_EPGMODE");

	size_t compared = 0;
	for (size_t i = 0; kEnumerators[i].member != NULL; ++i)
	{
		const EnumeratorValue &e = kEnumerators[i];
		if (omitted.count(std::string(e.member) + " " + e.name))
			continue;

		bool have = false;
		for (size_t r = 0; r < settingsTableCount() && !have; ++r)
		{
			const Descriptor &d = settingsTable()[r];
			have = isElement(d) && std::string(d.field.name) == e.member && d.field.extra->index == e.value;
		}
		INFO(e.member << " " << e.name << " has no row");
		CHECK(have);
		++compared;
	}

	INFO("table entries held to a row: " << compared);
	recordCount("table entries of the load with a row", compared);
	REQUIRE(compared > 0);
}

/* A list is stored as a count and an entry to each, and a list of records as the keys
   of its own kind of record. The count, or the first key of a record, is a literal the
   load reads, which is what is held to the row here: a list nobody loads is a row for
   nothing. One line to each such row, so that a new one has to say how it is stored. */
TEST_CASE("every list and list of records is one the program loads", "[settingselements]")
{
	static const struct { const char *row; const char *read; } kStored[] =
	{
		{ "webtv_xml", "webtv_xml_count" },
		{ "webradio_xml", "webradio_xml_count" },
		{ "xmltv_xml", "xmltv_xml_count" },
		{ "usermenu", "usermenu_key_red" },
		{ "timer_remotebox_ip", "timer_remotebox_ip_count" }
	};

	std::set<std::string> read;
	std::vector<std::vector<std::string> > keys = readLines(COREAPI_KEYS_FILE);
	for (size_t i = 0; i < keys.size(); ++i)
		read.insert(keys[i][0]);
	REQUIRE(!read.empty());

	size_t compared = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.type != ValueType::List && d.type != ValueType::Records)
			continue;

		const char *loaded = NULL;
		for (size_t k = 0; k < sizeof(kStored) / sizeof(kStored[0]); ++k)
			if (std::string(d.key) == kStored[k].row)
				loaded = kStored[k].read;
		INFO("row " << d.key << " says how it is stored");
		REQUIRE(loaded != NULL);
		INFO("the load reads " << loaded);
		CHECK(read.count(loaded) == 1);
		++compared;
	}

	recordCount("lists and lists of records held to a read of the load", compared);
	CHECK(compared == sizeof(kStored) / sizeof(kStored[0]));
}

/* Every element of every array has a row, not only the ones a table of the load lists.
   The denominator check counts an array as covered once any element names it, and the
   check above reads only the arrays that a table lists, so a loop's last slot could go
   and nothing would say so. The extent comes from the array itself, through the row. */
TEST_CASE("every element of every array has a row", "[settingselements]")
{
	// Nothing in the tree reads it, so a row would offer a setting that does nothing.
	std::set<std::string> omitted;
	omitted.insert("lcd_setting LCD_EPGMODE");

	std::map<std::string, std::set<long> > have;
	std::map<std::string, size_t> extent;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (!isElement(d))
			continue;
		have[d.field.name].insert(d.field.extra->index);
		extent[d.field.name] = d.field.extra->extent;
	}
	REQUIRE(!have.empty());

	size_t compared = 0;
	for (std::map<std::string, size_t>::const_iterator m = extent.begin(); m != extent.end(); ++m)
	{
		for (size_t index = 0; index < m->second; ++index)
		{
			bool left_out = false;
			for (size_t e = 0; kEnumerators[e].member != NULL; ++e)
				if (m->first == kEnumerators[e].member && kEnumerators[e].value == (long) index &&
				    omitted.count(m->first + " " + kEnumerators[e].name))
					left_out = true;
			if (left_out)
				continue;
			INFO("element " << index << " of " << m->first << " has no row");
			CHECK(have[m->first].count((long) index) == 1);
			++compared;
		}
	}

	INFO("elements held to a row: " << compared << " in " << extent.size() << " arrays");
	recordCount("elements of arrays held to a row", compared);
	recordCount("arrays held to a row for every element", extent.size());
	CHECK(extent.size() >= 14);
}

namespace
{

// Every element row of one array, in the order of the table.
std::vector<const Descriptor *> rowsOf(const std::string &member)
{
	std::vector<const Descriptor *> v;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (isElement(d) && member == d.field.name)
			v.push_back(&d);
	}
	return v;
}

std::string decimalText(long v)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%ld", v);
	return buf;
}

} // namespace

/* A write to one element moves that element. Every number element of every array is
   written in turn with a value no other element holds, and the whole array is read
   back through the rows: an element row that carried another element's index, or the
   index of an array beside it, would show as a neighbour that moved. */
TEST_CASE("writing an element moves that element and no other", "[settingselements]")
{
	RealStore store;
	std::set<std::string> members;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		if (isElement(settingsTable()[i]) && settingsTable()[i].field.read_number != NULL)
			members.insert(settingsTable()[i].field.name);
	}
	REQUIRE(!members.empty());

	size_t written = 0;
	for (std::set<std::string>::const_iterator m = members.begin(); m != members.end(); ++m)
	{
		std::vector<const Descriptor *> rows = rowsOf(*m);
		for (size_t i = 0; i < rows.size(); ++i)
			if (rows[i]->field.read_number != NULL)
				rows[i]->field.write_number(store.values, 0);

		for (size_t i = 0; i < rows.size(); ++i)
		{
			if (rows[i]->field.read_number == NULL)
				continue;
			INFO("element " << rows[i]->field.extra->index << " of " << *m << " is row " << rows[i]->key);
			rows[i]->field.write_number(store.values, 1);
			++written;
			for (size_t k = 0; k < rows.size(); ++k)
			{
				if (rows[k]->field.read_number == NULL)
					continue;
				INFO("neighbour " << rows[k]->key);
				CHECK(rows[k]->field.read_number(store.values) == (k == i ? 1 : 0));
			}
			rows[i]->field.write_number(store.values, 0);
		}
	}

	INFO("elements written: " << written);
	recordCount("number elements written and read back among their neighbours", written);
	CHECK(written > 0);
}

/* The same for the text elements, and the identifier ones, which are carried as the
   text a channel is named by. */
TEST_CASE("writing a text element moves that element and no other", "[settingselements]")
{
	RealStore store;
	std::set<std::string> members;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (isElement(d) && d.field.read_text != NULL)
			members.insert(d.field.name);
	}
	REQUIRE(!members.empty());

	size_t written = 0;
	for (std::set<std::string>::const_iterator m = members.begin(); m != members.end(); ++m)
	{
		std::vector<const Descriptor *> rows = rowsOf(*m);
		const bool ids = rows[0]->field.origin == FieldOrigin::Element &&
		                 std::string(rows[0]->default_string) == "0" && *m != "ci_pincode";
		for (size_t i = 0; i < rows.size(); ++i)
			rows[i]->field.write_text(store.values, ids ? "0" : "");

		for (size_t i = 0; i < rows.size(); ++i)
		{
			INFO("element " << rows[i]->field.extra->index << " of " << *m << " is row " << rows[i]->key);
			rows[i]->field.write_text(store.values, ids ? "1f" : "x");
			++written;
			for (size_t k = 0; k < rows.size(); ++k)
			{
				std::string got;
				rows[k]->field.read_text(store.values, got);
				INFO("neighbour " << rows[k]->key);
				CHECK(got == (k == i ? (ids ? "1f" : "x") : (ids ? "0" : "")));
			}
			rows[i]->field.write_text(store.values, ids ? "0" : "");
		}
	}

	INFO("text elements written: " << written);
	recordCount("text elements written and read back among their neighbours", written);
	CHECK(written > 0);
}

/* A write through the layer lands in the member the key's element stands for. One
   row of each array that has an ordinary row, written by its key and looked at in the
   struct itself rather than through the row's own functions. */
TEST_CASE("a write by key lands in the element of the array it names", "[settingselements]")
{
	RealStore store;

	REQUIRE(settings::set("timing.menu", "17").ok());
	REQUIRE(settings::set("timing.static_messages", "99").ok());
	REQUIRE(settings::set("personalize_media", "2").ok());
	REQUIRE(settings::set("pref_lang_1", "French").ok());
	REQUIRE(settings::set("pref_subs_2", "German").ok());
	REQUIRE(settings::set("mode_icons_flag3", "/tmp/flag").ok());
	REQUIRE(settings::set("ci_op_2", "1").ok());
	REQUIRE(settings::set("lcd_show_volume", "2").ok());
	applyPendingSettings();

	CHECK(store.values.timing[SNeutrinoSettings::TIMING_MENU] == 17);
	CHECK(store.values.timing[SNeutrinoSettings::TIMING_STATIC_MESSAGES] == 99);
	CHECK(store.values.timing[SNeutrinoSettings::TIMING_CHANLIST] != 17);
	CHECK(store.values.personalize[SNeutrinoSettings::P_MAIN_MEDIA] == 2);
	CHECK(store.values.pref_lang[1] == "French");
	CHECK(store.values.pref_lang[0] != "French");
	CHECK(store.values.pref_subs[2] == "German");
	CHECK(store.values.mode_icons_flag[3] == "/tmp/flag");
	CHECK(store.values.ci_op[2] == 1);
	CHECK(store.values.ci_op[1] == 0);
	CHECK(store.values.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] == 2);

	// And it reads back as the key it was written under.
	Result<std::string> got = settings::get("timing.menu");
	REQUIRE(got.ok());
	CHECK(got.value() == "17");
}

/* The bounds of an element row are the screen's. A value outside them is refused
   before the struct is touched, and the elements that take a bound of their own say
   so. */
TEST_CASE("an element row is held to its own bounds", "[settingselements]")
{
	RealStore store;

	CHECK_FALSE(settings::set("timing.menu", "241").ok());
	CHECK_FALSE(settings::set("timing.menu", "-1").ok());
	// The infobar's timeouts take minus one, which the other timeouts do not.
	CHECK(settings::set("timing.infobar_tv", "-1").ok());
	CHECK_FALSE(settings::set("timing.infobar_tv", "-2").ok());
	// A personalize entry is one of three, and a key of the table of five is one of five.
	CHECK_FALSE(settings::set("personalize_media", "3").ok());
	CHECK_FALSE(settings::set("personalize_feat_key_fav", "5").ok());
	CHECK(settings::set("personalize_feat_key_fav", "4").ok());
	// The two protected entries are offered as protected or not and nothing between.
	CHECK_FALSE(settings::set("personalize_settings", "1").ok());
	CHECK(settings::set("personalize_settings", "2").ok());
}

/* A widget edits an int through a pointer, and an element row hands out the address
   of its own element and not of the array or of another element. */
TEST_CASE("an element row and a nested row hand out the address of the member they name", "[settingselements]")
{
	SNeutrinoSettings values;

	Result<MenuItemSpec> timing = menuItem("timing.epg");
	REQUIRE(timing.ok());
	REQUIRE(timing.value().int_pointer != NULL);
	CHECK(timing.value().int_pointer(values) == &values.timing[SNeutrinoSettings::TIMING_EPG]);

	Result<MenuItemSpec> gradient = menuItem("menu_SubHead_gradient");
	REQUIRE(gradient.ok());
	REQUIRE(gradient.value().int_pointer != NULL);
	CHECK(gradient.value().int_pointer(values) == &values.theme.menu_SubHead_gradient);

	Result<MenuItemSpec> corners = menuItem("rounded_corners");
	REQUIRE(corners.ok());
	CHECK(corners.value().int_pointer(values) == &values.theme.rounded_corners);
}

/* A member of the theme struct is a setting under the key the theme files give it,
   and a write lands in the theme and nowhere else. */
TEST_CASE("a theme row writes the member of the theme it names", "[settingselements]")
{
	RealStore store;

	REQUIRE(settings::set("menu_Head_gradient", "5").ok());
	REQUIRE(settings::set("progressbar_timescale_green", "100").ok());
	REQUIRE(settings::set("progressbar_design_channellist", "-2").ok());
	REQUIRE(settings::set("infobar_gradient_top_direction", "1").ok());
	applyPendingSettings();

	CHECK(store.values.theme.menu_Head_gradient == 5);
	CHECK(store.values.theme.menu_SubHead_gradient != 5);
	CHECK(store.values.theme.progressbar_timescale_green == 100);
	CHECK(store.values.theme.progressbar_timescale_red != 100);
	CHECK(store.values.theme.progressbar_design_channellist == -2);
	CHECK(store.values.theme.infobar_gradient_top_direction == 1);

	// A time scale is a percentage.
	CHECK_FALSE(settings::set("progressbar_timescale_green", "101").ok());
	CHECK_FALSE(settings::set("progressbar_timescale_green", "-1").ok());
	// A gradient is one of the seven the screen lists, which have holes in no place but the end.
	CHECK_FALSE(settings::set("menu_Head_gradient", "7").ok());
	// And the design of the general progress bar leaves out the one the channel list adds.
	CHECK_FALSE(settings::set("progressbar_design", "-2").ok());
	CHECK(settings::set("progressbar_design", "-1").ok());
}

/* A list of texts is one setting written whole. It reads as the texts a line each, and
   what was written reads back before the loop has taken it. */
TEST_CASE("a list of texts reads and writes as a line to each", "[settingselements]")
{
	RealStore store;
	store.values.webtv_xml.push_back("/a.xml");
	store.values.webtv_xml.push_back("http://x/b.xml");

	Result<std::string> got = settings::get("webtv_xml");
	REQUIRE(got.ok());
	CHECK(got.value() == "/a.xml\nhttp://x/b.xml");

	REQUIRE(settings::set("webtv_xml", "/c.xml\n/d.xml\n/e.xml").ok());
	got = settings::get("webtv_xml");
	REQUIRE(got.ok());
	CHECK(got.value() == "/c.xml\n/d.xml\n/e.xml");
	// Not yet in the struct: the loop carries it in.
	CHECK(store.values.webtv_xml.size() == 2);

	applyPendingSettings();
	REQUIRE(store.values.webtv_xml.size() == 3);
	CHECK(*store.values.webtv_xml.begin() == "/c.xml");
	CHECK(store.values.webradio_xml.empty());
	CHECK(fixtureSaved);

	// An empty text is an empty list.
	REQUIRE(settings::set("webtv_xml", "").ok());
	applyPendingSettings();
	CHECK(store.values.webtv_xml.empty());
}

TEST_CASE("a list of texts refuses what its file cannot give back", "[settingselements]")
{
	RealStore store;

	// An empty line is the end of the text and not an entry.
	CHECK_FALSE(settings::set("webtv_xml", "/a.xml\n\n/b.xml").ok());
	CHECK_FALSE(settings::set("webtv_xml", "/a.xml\n").ok());
	// One entry is one line of text, so a number sign cuts nothing off.
	CHECK_FALSE(settings::set("webtv_xml", "/a.xml#x").ok());
	CHECK_FALSE(settings::set("webtv_xml", " /a.xml").ok());
	CHECK_FALSE(settings::set("webtv_xml", std::string("/a") + '\0' + "b").ok());
	CHECK_FALSE(settings::set("webtv_xml", std::string("/a\tb")).ok());

	std::string many;
	for (int i = 0; i < 1001; ++i)
		many += (i == 0 ? "" : "\n") + std::string("/f") + decimalText(i);
	CHECK_FALSE(settings::set("webtv_xml", many).ok());

	// Nothing of the refused was held.
	applyPendingSettings();
	CHECK(store.values.webtv_xml.empty());
}

namespace
{

// A user menu of n buttons as the program loads it: the four coloured names, then none.
void fillUsermenu(SNeutrinoSettings &values, int n)
{
	const char *const names[] = { "red", "green", "yellow", "blue" };
	for (int i = 0; i < n; ++i)
	{
		SNeutrinoSettings::usermenu_t *u = new SNeutrinoSettings::usermenu_t;
		u->key = 10 + i;
		u->title = "old";
		u->items = "1";
		if (i < 4)
			u->name = names[i];
		values.usermenu.push_back(u);
	}
}

// Three good buttons and the record under test, which is the fourth.
std::string withThree(const std::string &fourth)
{
	return "A\t1\t2\nB\t2\t6\nC\t3\t7\n" + fourth;
}

} // namespace

/* A list of records carries a line to each record and a tab between its members, each
   held to what its field says. */
TEST_CASE("a list of records reads and writes as a line to each record", "[settingselements]")
{
	RealStore store;
	fillUsermenu(store.values, 5);

	REQUIRE(settings::set("usermenu",
	                      "Red\t1\t2,3,4\n"
	                      "Green\t2\t6\n"
	                      "Yellow\t3\t7,35,freeze\n"
	                      "Blue\t4\t27\n"
	                      "Five\t5\t9").ok());
	applyPendingSettings();

	REQUIRE(store.values.usermenu.size() == 5);
	CHECK(store.values.usermenu[0]->title == "Red");
	CHECK(store.values.usermenu[0]->key == 1);
	CHECK(store.values.usermenu[0]->items == "2,3,4");
	CHECK(store.values.usermenu[0]->name == "red");
	CHECK(store.values.usermenu[3]->name == "blue");
	CHECK(store.values.usermenu[4]->title == "Five");
	CHECK(store.values.usermenu[4]->items == "9");

	Result<std::string> got = settings::get("usermenu");
	REQUIRE(got.ok());
	CHECK(got.value() == "Red\t1\t2,3,4\nGreen\t2\t6\nYellow\t3\t7,35,freeze\nBlue\t4\t27\nFive\t5\t9");

	// A title may be empty and the items too.
	REQUIRE(settings::set("usermenu", "A\t1\t2\nB\t2\t6\nC\t3\t7\n\t7\t\nE\t8\t1").ok());
	applyPendingSettings();
	REQUIRE(store.values.usermenu.size() == 5);
	CHECK(store.values.usermenu[3]->title.empty());
	CHECK(store.values.usermenu[3]->key == 7);
}

TEST_CASE("a list of records refuses a record its fields do not describe", "[settingselements]")
{
	RealStore store;
	fillUsermenu(store.values, 4);

	// Each is the fourth record of an otherwise good menu, so it is the record that is refused.
	CHECK(settings::set("usermenu", withThree("D\t4\t2")).ok());

	// Too few members and too many.
	CHECK_FALSE(settings::set("usermenu", withThree("Red\t1")).ok());
	CHECK_FALSE(settings::set("usermenu", withThree("Red\t1\t2\t3")).ok());
	// A key is a number, and one of the codes the remote has.
	CHECK_FALSE(settings::set("usermenu", withThree("Red\tx\t2")).ok());
	CHECK_FALSE(settings::set("usermenu", withThree("Red\t-3\t2")).ok());
	// A member is a line of text like any other.
	CHECK_FALSE(settings::set("usermenu", withThree("Re#d\t1\t2")).ok());
	CHECK_FALSE(settings::set("usermenu", withThree(" Red\t1\t2")).ok());
	// An empty line is no record.
	CHECK_FALSE(settings::set("usermenu", "A\t1\t2\n\nB\t2\t6\nC\t3\t7\nD\t4\t2").ok());

	// Only the good write was held.
	applyPendingSettings();
	CHECK(store.values.usermenu[0]->title == "A");
	CHECK(store.values.usermenu[3]->title == "D");
}

/* The screens hold the address of a record, or of a text of one, while they are open, and a
   write arrives from inside their message loop. So a write carries exactly the records there
   are: a count that differs would add or free one. Adding and removing stay on the screen. */
TEST_CASE("a write of a list of records carries exactly the records there are", "[settingselements]")
{
	RealStore store;
	fillUsermenu(store.values, 5);

	CHECK_FALSE(settings::set("usermenu", "").ok());
	CHECK_FALSE(settings::set("usermenu", "A\t1\t2").ok());
	CHECK_FALSE(settings::set("usermenu", "A\t1\t2\nB\t2\t6\nC\t3\t7\nD\t4\t8").ok());
	CHECK_FALSE(settings::set("usermenu", "A\t1\t2\nB\t2\t6\nC\t3\t7\nD\t4\t8\nE\t5\t9\nF\t6\t1").ok());
	CHECK(settings::set("usermenu", "A\t1\t2\nB\t2\t6\nC\t3\t7\nD\t4\t8\nE\t5\t9").ok());
	applyPendingSettings();
	CHECK(store.values.usermenu.size() == 5);
	CHECK(store.values.usermenu[4]->title == "E");
}

/* A button with no key reads as nought and nought writes it back, so that what a read
   answered can be sent again unchanged. The program keeps no key as a code of its own. */
TEST_CASE("a user menu button without a key survives a read followed by a write", "[settingselements]")
{
	RealStore store;
	fillUsermenu(store.values, 5);
	for (int i = 0; i < 5; ++i)
	{
		store.values.usermenu[i]->key = i == 4 ? RC_NOKEY : 10 + i;
		store.values.usermenu[i]->title = std::string("T") + char('0' + i);
	}

	Result<std::string> got = settings::get("usermenu");
	REQUIRE(got.ok());
	CHECK(got.value() == "T0\t10\t1\nT1\t11\t1\nT2\t12\t1\nT3\t13\t1\nT4\t0\t1");

	REQUIRE(settings::set("usermenu", got.value()).ok());
	applyPendingSettings();
	REQUIRE(store.values.usermenu.size() == 5);
	CHECK(store.values.usermenu[4]->key == RC_NOKEY);
	CHECK(store.values.usermenu[3]->key == 13);
	// Written again as read.
	CHECK(settings::get("usermenu").value() == got.value());
}

/* A write changes the records that are there and no others. The addresses of the records
   and of their texts are the same after an accepted write as before, and a refused write
   leaves them alone. A slot a screen emptied and has not yet tidied up is skipped by a read,
   so the nth record is the nth slot that holds one, and the empty slot stays empty. */
TEST_CASE("a user menu write changes the records in place", "[settingselements]")
{
	RealStore store;
	fillUsermenu(store.values, 6);
	delete store.values.usermenu[4];
	store.values.usermenu[4] = NULL;

	std::vector<SNeutrinoSettings::usermenu_t *> before = store.values.usermenu;
	std::vector<std::string *> titles;
	for (size_t i = 0; i < before.size(); ++i)
		titles.push_back(before[i] != NULL ? &before[i]->title : NULL);

	Result<std::string> got = settings::get("usermenu");
	REQUIRE(got.ok());
	// Five records over six slots, one of them empty.
	CHECK(std::count(got.value().begin(), got.value().end(), '\n') == 4);

	CHECK_FALSE(settings::set("usermenu", "A\t1\t2\nB\t2\t6\nC\t3\t7\nD\t4\t8").ok());
	REQUIRE(settings::set("usermenu", "A\t1\t2\nB\t2\t6\nC\t3\t7\nD\t4\t8\nF\t6\t9").ok());
	applyPendingSettings();

	REQUIRE(store.values.usermenu.size() == before.size());
	for (size_t i = 0; i < before.size(); ++i)
	{
		INFO("slot " << i);
		CHECK(store.values.usermenu[i] == before[i]);
		if (before[i] != NULL)
			CHECK(&store.values.usermenu[i]->title == titles[i]);
	}
	CHECK(store.values.usermenu[4] == NULL);
	CHECK(store.values.usermenu[0]->title == "A");
	CHECK(store.values.usermenu[3]->title == "D");
	CHECK(store.values.usermenu[5]->title == "F");
	CHECK(store.values.usermenu[3]->name == "blue");
}

/* A write the loop finds to be for another count than there is, because a screen added or took
   away a record between the check and the drain, changes nothing: it has no record to put a
   record on, and the answer has long gone. */
TEST_CASE("a records write for a count the list no longer has changes nothing", "[settingselements]")
{
	SNeutrinoSettings values;
	fillUsermenu(values, 4);
	timer_remotebox_item b;
	b.port = 80;
	b.rbname = "box";
	b.rbaddress = "10.0.0.2";
	b.enabled = true;
	b.online = false;
	values.timer_remotebox_ip.push_back(b);

	std::vector<RecordValues> menu(3, RecordValues(3, "x"));
	for (size_t i = 0; i < menu.size(); ++i)
		menu[i][1] = "5";
	const FieldExtra *x = settings::findRow("usermenu")->field.extra;
	x->write_records(values, menu);
	CHECK(values.usermenu.size() == 4);
	CHECK(values.usermenu[0]->title == "old");

	std::vector<RecordValues> boxes(2, RecordValues(6, "x"));
	const FieldExtra *y = settings::findRow("timer_remotebox_ip")->field.extra;
	y->write_records(values, boxes);
	REQUIRE(values.timer_remotebox_ip.size() == 1);
	CHECK(values.timer_remotebox_ip[0].rbname == "box");
}

/* The remote boxes carry a password, which makes the whole list a credential: a read
   answers nothing and the write replaces it whole and cannot be nothing. The screen of
   the timers reads the list out of the struct, so what a read of it through the layer
   withholds is not what the screen shows. */
TEST_CASE("a list of records that holds a password is a credential", "[settingselements]")
{
	RealStore store;
	timer_remotebox_item b;
	b.port = 80;
	b.user = "root";
	b.pass = "swordfish";
	b.rbname = "box";
	b.rbaddress = "10.0.0.2";
	b.enabled = true;
	b.online = true;
	store.values.timer_remotebox_ip.push_back(b);

	Result<std::string> got = settings::get("timer_remotebox_ip");
	REQUIRE(got.ok());
	CHECK(got.value().empty());

	Result<Descriptor> d = settings::describe("timer_remotebox_ip");
	REQUIRE(d.ok());
	CHECK(d.value().secret);
	CHECK(std::string(d.value().default_string) == "");

	// Not the whole list over a form that read nothing.
	Result<void> empty = settings::set("timer_remotebox_ip", "");
	REQUIRE_FALSE(empty.ok());
	CHECK(empty.error().code == ErrorCode::EmptyCredential);

	/* The timer screen holds iterators into the list and the address of the texts of a box
	   while it is open, so a write carries exactly the boxes there are and changes each in
	   place: the same elements at the same addresses afterwards, and none added or freed. */
	const timer_remotebox_item *at = &store.values.timer_remotebox_ip[0];
	const std::string *name_at = &store.values.timer_remotebox_ip[0].rbname;
	CHECK_FALSE(settings::set("timer_remotebox_ip",
	                          "1\t10.0.0.3\tnew\tuser\tsecret\t8080\n1\t10.0.0.4\tother\tuser\tsecret\t80").ok());
	applyPendingSettings();
	REQUIRE(store.values.timer_remotebox_ip.size() == 1);
	CHECK(store.values.timer_remotebox_ip[0].rbaddress == "10.0.0.2");

	REQUIRE(settings::set("timer_remotebox_ip", "1\t10.0.0.3\tnew\tuser\tsecret\t8080").ok());
	applyPendingSettings();
	REQUIRE(store.values.timer_remotebox_ip.size() == 1);
	CHECK(&store.values.timer_remotebox_ip[0] == at);
	CHECK(&store.values.timer_remotebox_ip[0].rbname == name_at);
	const timer_remotebox_item &n = store.values.timer_remotebox_ip[0];
	CHECK(n.rbaddress == "10.0.0.3");
	CHECK(n.rbname == "new");
	CHECK(n.user == "user");
	CHECK(n.pass == "secret");
	CHECK(n.port == 8080);
	CHECK(n.enabled);
	// Whether it can be reached is found out by the screen and not said by a write.
	CHECK(n.online);

	CHECK_FALSE(settings::set("timer_remotebox_ip", "2\t10.0.0.3\tnew\tuser\tsecret\t8080").ok());
	CHECK_FALSE(settings::set("timer_remotebox_ip", "1\t10.0.0.3\tnew\tuser\tsecret\t0").ok());
	CHECK_FALSE(settings::set("timer_remotebox_ip", "1\t10.0.0.3\tnew\tuser\tsecret\t65536").ok());
}

// The usermenu and the remote boxes are made of the members their rows say, in that order.
TEST_CASE("a row of records names the members of a record", "[settingselements]")
{
	Result<Descriptor> usermenu = settings::describe("usermenu");
	REQUIRE(usermenu.ok());
	REQUIRE(usermenu.value().type == ValueType::Records);
	REQUIRE(usermenu.value().field.extra != NULL);
	REQUIRE(usermenu.value().field.extra->record_field_count == 3);
	CHECK(std::string(usermenu.value().field.extra->record_fields[0].name) == "title");
	CHECK(std::string(usermenu.value().field.extra->record_fields[1].name) == "key");
	CHECK(usermenu.value().field.extra->record_fields[1].type == ValueType::Int);
	CHECK(std::string(usermenu.value().field.extra->record_fields[2].name) == "items");

	Result<Descriptor> boxes = settings::describe("timer_remotebox_ip");
	REQUIRE(boxes.ok());
	REQUIRE(boxes.value().field.extra->record_field_count == 6);
	size_t secrets = 0;
	for (size_t i = 0; i < boxes.value().field.extra->record_field_count; ++i)
		if (boxes.value().field.extra->record_fields[i].secret)
			++secrets;
	CHECK(secrets == 1);
}

namespace
{

// A path under a directory made for the case and removed with it.
struct TempDir
{
	std::string path;

	TempDir()
	{
		char tmpl[] = "/tmp/coreapi-flag-XXXXXX";
		const char *made = mkdtemp(tmpl);
		path = made != NULL ? made : "";
	}

	~TempDir()
	{
		if (!path.empty())
		{
			unlink((path + "/.flag").c_str());
			rmdir(path.c_str());
		}
	}

	bool exists(const char *name) const
	{
		struct stat st;
		return stat((path + "/" + name).c_str(), &st) == 0;
	}
};

/* The table a flag file case installs: one row, whose path is the directory made for
   the case. A row of the shipped table names a fixed directory, which no case may
   write to. */
struct FlagTable
{
	std::string path;
	Descriptor  rows[1];

	explicit FlagTable(const std::string &p) : path(p)
	{
		rows[0] = boolRow("flag_case")
			.section("hdd")
			.defaultValue(0)
			.field(FieldRef{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, path.c_str(),
			                 FieldOrigin::FlagFile, NULL, NULL, NULL });
		setSettingsTable(rows, 1);
	}

	~FlagTable() { setSettingsTable(NULL, 0); }
};

} // namespace

/* A flag that is a file's existence: read from the file, written by making it and
   removing it, and what was written reads back at once and is on disc only when the
   loop has carried it in. */
TEST_CASE("a flag file row is the existence of its file", "[settingselements]")
{
	TempDir dir;
	REQUIRE(!dir.path.empty());
	FlagTable table(dir.path + "/.flag");
	RealStore store;
	REQUIRE(descriptorIsSane(table.rows[0]));

	Result<std::string> got = settings::get("flag_case");
	REQUIRE(got.ok());
	CHECK(got.value() == "0");

	REQUIRE(settings::set("flag_case", "1").ok());
	got = settings::get("flag_case");
	REQUIRE(got.ok());
	CHECK(got.value() == "1");
	// The loop has not carried it in.
	CHECK_FALSE(dir.exists(".flag"));

	applyPendingSettings();
	CHECK(dir.exists(".flag"));
	got = settings::get("flag_case");
	REQUIRE(got.ok());
	CHECK(got.value() == "1");

	REQUIRE(settings::set("flag_case", "0").ok());
	applyPendingSettings();
	CHECK_FALSE(dir.exists(".flag"));

	// Taking away a file that is not there is what was asked for.
	REQUIRE(settings::set("flag_case", "0").ok());
	applyPendingSettings();
	CHECK_FALSE(dir.exists(".flag"));

	// A file made by something else is read as it is.
	FILE *f = fopen((dir.path + "/.flag").c_str(), "w");
	REQUIRE(f != NULL);
	fclose(f);
	got = settings::get("flag_case");
	REQUIRE(got.ok());
	CHECK(got.value() == "1");

	// Two states and no third.
	CHECK_FALSE(settings::set("flag_case", "2").ok());
}

/* A menu draws a flag file as the flag it is and moves it through the two calls that
   reach the file, because the row has no member for a widget to edit. */
TEST_CASE("a menu reads and writes a flag file row through the file", "[settingselements]")
{
	TempDir dir;
	REQUIRE(!dir.path.empty());
	FlagTable table(dir.path + "/.flag");
	SNeutrinoSettings values;

	Result<MenuItemSpec> spec = menuItem("flag_case");
	REQUIRE(spec.ok());
	CHECK(spec.value().type == ValueType::Bool);
	CHECK(spec.value().int_pointer == NULL);

	long v = 7;
	REQUIRE(menuValueRead(spec.value(), values, v));
	CHECK(v == 0);
	REQUIRE(menuValueWrite(spec.value(), values, 1));
	CHECK(dir.exists(".flag"));
	REQUIRE(menuValueRead(spec.value(), values, v));
	CHECK(v == 1);
	REQUIRE(menuValueWrite(spec.value(), values, 0));
	CHECK_FALSE(dir.exists(".flag"));
	// Two states and no third, and a directory that is not there is a write that failed.
	CHECK_FALSE(menuValueWrite(spec.value(), values, 2));
	FlagTable missing(dir.path + "/no/such/dir/.flag");
	Result<MenuItemSpec> lost = menuItem("flag_case");
	REQUIRE(lost.ok());
	CHECK_FALSE(menuValueWrite(lost.value(), values, 1));
}

/* A flag file whose screen also starts or stops a program, or rewrites other settings, would
   be half done by a write of the file alone, so such a row is read-only until the stream that
   owns it moves the effect into something that applies it. The ones that only touch a file
   are not. */
TEST_CASE("the flag files whose screen does more than touch a file are read-only", "[settingselements]")
{
	// A row may ask the box whether it has what the row controls.
	FakeSystemSource box;
	InstalledSystemSource installed_box(&box);
	size_t held = 0, plain = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.field.origin != FieldOrigin::FlagFile)
			continue;
		const std::string key = d.key;
		const bool does_more = key.compare(0, 12, "flag_daemon_") == 0 || key.compare(0, 10, "flag_camd_") == 0 ||
		                       key == "flag_scart_osd_fix";
		INFO("row " << key);
		// Held is its own state: the parental lock is not what fixes these rows, and the box's
		// own screen does not take them for locked.
		CHECK(settings::heldNow(key) == does_more);
		CHECK_FALSE(settings::lockedNow(key));
		if (does_more)
		{
			++held;
			// Refused as every fixed row is, before anything of the value is looked at, and
			// worded as held and not as the parental lock.
			Result<void> r = settings::check(key, "1");
			REQUIRE_FALSE(r.ok());
			CHECK(r.error().code == ErrorCode::SettingLocked);
			CHECK(r.error().message.find("held") != std::string::npos);
			CHECK(r.error().message.find("parental") == std::string::npos);
		}
		else
			++plain;
	}
	CHECK(held == 21);
	CHECK(plain >= 1);
	recordCount("flag file rows held read-only", held);
}

/* A menu does not grey a held row. The screen that owns it applies its effect itself, so
   only a write through the layer is held off; the menu spec is as for any row. */
TEST_CASE("a held flag row is not greyed in a menu", "[settingselements]")
{
	FakeSystemSource box;
	box.caps.has_scart_osd_fix = true;
	InstalledSystemSource installed_box(&box);
	REQUIRE(settings::heldNow("flag_scart_osd_fix"));

	Result<MenuItemSpec> spec = menuItem("flag_scart_osd_fix");
	REQUIRE(spec.ok());
	CHECK_FALSE(spec.value().locked);

	// And with the parental lock on, a row it holds is greyed as before: the two are apart.
	box.parental_locked = true;
	Result<MenuItemSpec> lock = menuItem("parentallock_prompt");
	REQUIRE(lock.ok());
	CHECK(lock.value().locked);
	Result<MenuItemSpec> still = menuItem("flag_scart_osd_fix");
	REQUIRE(still.ok());
	CHECK_FALSE(still.value().locked);
}

/* The three kinds of row the tables now hold are sane as they are written, and the
   shipped rows of each are there. */
TEST_CASE("the shipped rows of the new kinds are sane", "[settingselements]")
{
	size_t lists = 0, records = 0, flags = 0, nested = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		INFO("row " << d.key);
		CHECK(descriptorIsSane(d));
		if (d.type == ValueType::List)
			++lists;
		if (d.type == ValueType::Records)
			++records;
		if (d.field.origin == FieldOrigin::FlagFile)
		{
			++flags;
			CHECK(d.field.name[0] == '/');
			// Each is offered only where what it stands for is, bar the display's, which sit in a build of their own.
			if (std::string(d.key).compare(0, 9, "flag_lcd4") != 0)
				CHECK(d.field.available != NULL);
		}
		if (d.field.origin != FieldOrigin::ColorBytes &&
		    std::string(d.field.name != NULL ? d.field.name : "").compare(0, 6, "theme.") == 0)
			++nested;
	}
	CHECK(lists == 3);
	CHECK(records == 2);
	// The hard disk power, the SCART picture fix, thirteen daemons and seven softcams.
	CHECK(flags == 22);
	// The twenty six numbers and choices of the theme; its colours are the colour rows' own.
	CHECK(nested == 26);
	CHECK(elementRowCount() > 100);
	recordCount("list, records and flag file rows", lists + records + flags);
	recordCount("rows of the theme struct", nested);
}
