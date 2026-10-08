/*
 * test_settingsappliers.cpp - tests for applying settings to the GUI
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

#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

/* What the seam between a written setting and the box acts on, and how much of the
   table that is.

   The lists in src/gui/settings_appliers.cpp were transcribed from the notifiers by
   hand. Nothing here can link that object, so the lists are read as text by
   extract-applied.sh, which follows the registration to the applier, the applier to its
   notifier and the notifier to the options it branches on.

   The number that matters is rows and not sections. A section with an applier is a
   section where some settings are acted on, which reads as coverage and is not: eight of
   the sixteen carry an applier and the rows those eight declare outnumber the rows
   anything acts on by an order of magnitude. */

using namespace coreapi;

namespace
{

std::vector<std::string> splitTabs(const std::string &line)
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

// Written by make before the suite runs. A missing file is a failure of these
// cases and not something to invent an empty answer for.
const std::vector<std::vector<std::string> > &records()
{
	static std::vector<std::vector<std::string> > rows;
	static bool read = false;
	if (read)
		return rows;
	read = true;

	FILE *f = fopen(COREAPI_APPLIED_FILE, "r");
	if (f == NULL)
		return rows;

	std::string line;
	int c;
	while ((c = fgetc(f)) != EOF)
	{
		if (c == '\n')
		{
			if (!line.empty())
				rows.push_back(splitTabs(line));
			line.clear();
		}
		else
			line += (char) c;
	}
	if (!line.empty())
		rows.push_back(splitTabs(line));
	fclose(f);
	return rows;
}

// registered	section
const std::set<std::string> &registered()
{
	static std::set<std::string> out;
	static bool read = false;
	if (!read)
	{
		read = true;
		for (size_t i = 0; i < records().size(); ++i)
		{
			if (records()[i].size() == 2 && records()[i][0] == "registered")
				out.insert(records()[i][1]);
		}
	}
	return out;
}

// listed	section	key	locale	label
struct Listed
{
	std::string section;
	std::string key;
	std::string locale;
	std::string label;
};

const std::vector<Listed> &listed()
{
	static std::vector<Listed> out;
	static bool read = false;
	if (!read)
	{
		read = true;
		for (size_t i = 0; i < records().size(); ++i)
		{
			const std::vector<std::string> &r = records()[i];
			if (r.size() != 5 || r[0] != "listed")
				continue;
			Listed l;
			l.section = r[1];
			l.key = r[2];
			l.locale = r[3];
			l.label = r[4];
			out.push_back(l);
		}
	}
	return out;
}

// acts	section	locale	label	where
struct Acts
{
	std::string section;
	std::string label;
	std::string where;
};

const std::vector<Acts> &actsOn()
{
	static std::vector<Acts> out;
	static bool read = false;
	if (!read)
	{
		read = true;
		for (size_t i = 0; i < records().size(); ++i)
		{
			const std::vector<std::string> &r = records()[i];
			if (r.size() != 5 || r[0] != "acts")
				continue;
			Acts a;
			a.section = r[1];
			a.label = r[3];
			a.where = r[4];
			out.push_back(a);
		}
	}
	return out;
}

// Whether the section of a row has one at all, which is what makes the row part
// of the denominator the honest figure is over.
bool hasApplier(const char *section)
{
	return section != NULL && registered().count(section) == 1;
}

std::string sectionOf(const Descriptor &d)
{
	return d.section != NULL ? std::string(d.section) : std::string();
}

} // namespace

/* The scan behind the three cases below. A file that stopped matching leaves
   every one of them comparing nothing and passing, which is the shape of
   failure this whole guard exists for, so what it read is counted first. */
TEST_CASE("the seam read out of the source is the size the source has", "[settingsappliers]")
{
	INFO("records read: " << records().size());
	REQUIRE(records().size() > 0);

	recordCount("applier sections registered", registered().size());
	recordCount("keys an applier lists", listed().size());
	recordCount("options an applier or its notifier branches on", actsOn().size());
}

/* A key in a list that no row of that section declares is a key apply() answers
   true for and nothing acts on, which is the defect the key routing was written
   to close, one level up. The locale beside it has to be the row's own label
   too: the key is what routes and the locale is what the notifier is asked
   with, and a pair that names two different rows applies the wrong one. */
TEST_CASE("every key an applier lists is a row its own section declares", "[settingsappliers]")
{
	std::map<std::string, const Descriptor *> byKey;
	for (size_t i = 0; i < settingsTableCount(); ++i)
		byKey[settingsTable()[i].key] = &settingsTable()[i];

	size_t checked = 0;
	for (size_t i = 0; i < listed().size(); ++i)
	{
		const Listed &l = listed()[i];
		INFO("the " << l.section << " applier lists " << l.key << " under " << l.locale);

		std::map<std::string, const Descriptor *>::const_iterator it = byKey.find(l.key);
		// A key behind a build condition this build does not take is read by
		// the scan and linked by nothing, so it is passed over rather than
		// failed: the list answers for every box and the table for one.
		if (it == byKey.end())
			continue;

		++checked;
		CHECK(sectionOf(*it->second) == l.section);
		CHECK(it->second->label_key != NULL);
		if (it->second->label_key != NULL)
			CHECK(std::string(it->second->label_key) == l.label);
	}

	recordCount("listed keys compared against the table", checked);
}

/* What the comparison below found. A row whose value is not in the member it is
   named after must not be listed, and the direction is the opposite of the rule
   for the others: the notifier's branch reads that member, the write went where
   the value really lives and left the member alone, so running the notifier
   would apply whatever the screen last left in it. Refused rather than passed
   over, so adding such a key to a list is a failure. */
struct ActedMatch
{
	size_t checked;
	size_t elsewhere;
	size_t pending;
	std::string wrong;

	ActedMatch() : checked(0), elsewhere(0), pending(0) {}
};

/* Rows a notifier acts on that the applier does not list yet, and why, with the stream
   that is to settle each. The front display's brightnesses: the screen edits each through
   a copy of its own and writes it into the setting when the item is focused, so applying
   the setting would apply whatever the copy last held. Stream S5 takes the screen over and
   then lists them; the figure below is held to what is named here, so a row that is
   listed has to come off this list and a name that no longer matches a row fails. */
struct Pending
{
	const char *key;
	const char *owner;
};

static const Pending kPending[] =
{
	{ "lcd_brightness", "S5" },
	{ "lcd_standbybrightness", "S5" },
	{ "lcd_deepbrightness", "S5" }
};

static bool isPending(const std::string &key)
{
	for (size_t i = 0; i < sizeof(kPending) / sizeof(kPending[0]); ++i)
		if (key == kPending[i].key)
			return true;
	return false;
}

static ActedMatch matchActed(const std::vector<Acts> &acts, const std::set<std::string> &listedKeys)
{
	ActedMatch m;
	for (size_t i = 0; i < acts.size(); ++i)
	{
		const Acts &a = acts[i];
		for (size_t j = 0; j < settingsTableCount(); ++j)
		{
			const Descriptor &d = settingsTable()[j];
			if (d.label_key == NULL || sectionOf(d) != a.section || std::string(d.label_key) != a.label)
				continue;

			const bool isListed = listedKeys.count(a.section + "\t" + std::string(d.key)) == 1;
			if (!valueIsInNamedMember(d.field))
			{
				++m.elsewhere;
				if (isListed)
					m.wrong += " " + std::string(d.key) + "(listed, value is elsewhere)"
					           " [" + a.label + ", acted on by " + a.where + "]";
				continue;
			}

			++m.checked;
			if (isPending(d.key))
			{
				// Pending means not listed; a pending row that is listed is a list to correct.
				if (isListed)
					m.wrong += " " + std::string(d.key) + "(listed, and still pending)";
				else
					++m.pending;
				continue;
			}
			if (!isListed)
				m.wrong += " " + std::string(d.key) + "(acted on, not listed)"
				           " [" + a.label + ", acted on by " + a.where + "]";
		}
	}
	return m;
}

/* The other direction, and the one a hand written list loses first: a notifier
   that gains a branch for a setting the section declares leaves the list short,
   and the row is written, saved and never applied with nothing to say so. */
TEST_CASE("every option a notifier acts on is one its applier lists", "[settingsappliers]")
{
	std::set<std::string> listedKeys;
	for (size_t i = 0; i < listed().size(); ++i)
		listedKeys.insert(listed()[i].section + "\t" + listed()[i].key);

	ActedMatch m = matchActed(actsOn(), listedKeys);
	INFO("rows wrongly listed or left out:" << m.wrong);
	CHECK(m.wrong.empty());

	recordCount("acted options matched to a declared row", m.checked);
	// Every name on the pending list is a row a notifier acts on, so none is left behind.
	recordCount("rows a notifier acts on that wait for a stream", m.pending);
	CHECK(m.pending == sizeof(kPending) / sizeof(kPending[0]));
	/* Counted, or a table that stopped declaring any of them would satisfy the
	   comparison by never matching a row. Rows whose value lives elsewhere are
	   no longer required of the tree: the proof for that path is the fixture
	   below. */
	recordCount("acted options whose row writes another member", m.elsewhere);
	REQUIRE(m.checked > 0);
}

/* The path for a row whose value is not in its named member needs a notifier
   branch on such a row to be exercised, and the tree no longer has one. So the
   same comparison is run over a branch written here, on rows the table does
   declare: the three bits of the audio pid mask and the daemon's safety time. */
TEST_CASE("a notifier branch on a row whose value lives elsewhere is refused when listed", "[settingsappliers]")
{
	std::vector<Acts> acts;
	Acts a;
	a.section = "recording";
	a.label = "recordingmenu.apids_std";
	a.where = "fixture";
	acts.push_back(a);
	a.label = "timersettings.record_safety_time_before";
	acts.push_back(a);

	std::set<std::string> none;
	ActedMatch clean = matchActed(acts, none);
	INFO("wrong:" << clean.wrong);
	REQUIRE(clean.elsewhere == 2);
	REQUIRE(clean.checked == 0);
	CHECK(clean.wrong.empty());

	std::set<std::string> listedAnyway;
	listedAnyway.insert("recording\trecording_audio_pids_std");
	ActedMatch broken = matchActed(acts, listedAnyway);
	REQUIRE(broken.elsewhere == 2);
	CHECK(broken.wrong.find("recording_audio_pids_std(listed, value is elsewhere)") != std::string::npos);
	// Names which notifier branch to look at, which a bare key does not.
	CHECK(broken.wrong.find("[recordingmenu.apids_std, acted on by fixture]") != std::string::npos);
}

/* And the other side of the same comparison, on a row whose value is in its
   member: acted on and not listed is a failure, and listed is not. */
TEST_CASE("a notifier branch on an ordinary row must be listed", "[settingsappliers]")
{
	std::vector<Acts> acts;
	Acts a;
	a.section = "recording";
	a.label = "recordingmenu.fill_warn";
	a.where = "fixture";
	acts.push_back(a);

	std::set<std::string> none;
	ActedMatch missing = matchActed(acts, none);
	REQUIRE(missing.checked == 1);
	CHECK(missing.wrong.find("recording_fill_warning(acted on, not listed)") != std::string::npos);
	CHECK(missing.wrong.find("[recordingmenu.fill_warn, acted on by fixture]") != std::string::npos);

	std::set<std::string> listedKeys;
	listedKeys.insert("recording\trecording_fill_warning");
	ActedMatch fine = matchActed(acts, listedKeys);
	REQUIRE(fine.checked == 1);
	CHECK(fine.wrong.empty());
}

/* The figure the build states about this seam. Sections are the wrong unit:
   the eight that carry an applier declare the great majority of the table
   between them and what any applier acts on is a small part of that. Counted
   here rather than printed by a scan, so that a fall in either of them is a
   failure and not a line scrolling past. */
TEST_CASE("what an applier acts on is counted in rows and not in sections", "[settingsappliers]")
{
	std::set<std::string> listedKeys;
	for (size_t i = 0; i < listed().size(); ++i)
		listedKeys.insert(listed()[i].section + "\t" + listed()[i].key);

	size_t sectioned = 0;
	size_t acted = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (!hasApplier(d.section))
			continue;
		++sectioned;
		if (listedKeys.count(sectionOf(d) + "\t" + std::string(d.key)) == 1)
			++acted;
	}

	INFO("rows in a section with an applier: " << sectioned);
	INFO("rows an applier acts on: " << acted);
	REQUIRE(acted < sectioned);

	recordCount("rows this build links", settingsTableCount());
	recordCount("rows in a section with an applier", sectioned);
	recordCount("rows an applier acts on", acted);
}
