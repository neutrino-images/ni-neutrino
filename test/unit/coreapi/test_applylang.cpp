/*
 * test_applylang.cpp - the language groups run once per change and at startup
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

#include <config.h>

#include "support/catch.hpp"
#include "support/fakes.h"
#include "support/phaseenv.h"
#include "support/applytest.h"

#include <neutrinoMessages.h>

#include "coreapi/base/apply.h"
#include "coreapi/base/deps.h"
#include "coreapi/box/apply_lang.h"
#include "coreapi/settings/choicesources.h"
#include "coreapi/settings/settings.h"

#include <system/settings.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <vector>

extern SNeutrinoSettings g_settings;

using namespace coreapi;

namespace
{

// The members the cases write, put back so the rest of the suite finds them as they were.
struct KeptSettings
{
	std::string language, timezone, lang0, lang1, lang2;
	KeptSettings()
		: language(g_settings.language), timezone(g_settings.timezone), lang0(g_settings.pref_lang[0]),
		  lang1(g_settings.pref_lang[1]), lang2(g_settings.pref_lang[2]) {}
	~KeptSettings()
	{
		g_settings.language = language;
		g_settings.timezone = timezone;
		g_settings.pref_lang[0] = lang0;
		g_settings.pref_lang[1] = lang1;
		g_settings.pref_lang[2] = lang2;
	}
};

struct LanguageBox
{
	KeptSettings kept;
	PhaseEnvironment env;
	FakeLocalization &out;

	explicit LanguageBox(ApplyPhase phase) : env(phase), out(env.fake<FakeLocalization>("localization"))
	{
		resetSentLanguage();
	}
	~LanguageBox() { resetSentLanguage(); }
};

} // namespace

TEST_CASE("the language groups are registered by the one hook", "[apply][language]")
{
	ApplyFresh fresh;
	registerApplyGroups();

	REQUIRE(groupOf("language") == &kLanguageApplyGroup);
	REQUIRE(groupOf("timezone") == &kTimezoneApplyGroup);
	REQUIRE(kLanguageApplyGroup.phase == ApplyPhase::Framebuffer);
	REQUIRE(kTimezoneApplyGroup.phase == ApplyPhase::Framebuffer);
	for (int i = 0; i < 3; ++i)
		REQUIRE(groupOf(("pref_lang_" + applyNumber(i)).c_str()) == &kGuideLanguageApplyGroup);
	REQUIRE(kGuideLanguageApplyGroup.phase == ApplyPhase::Sectionsd);

	// Read where they are used.
	REQUIRE(groupOf("auto_lang") == NULL);
}

TEST_CASE("startup loads the language, links the zone once each", "[apply][language]")
{
	ApplyFresh fresh;
	LanguageBox box(ApplyPhase::Framebuffer);
	registerApplyGroups();

	g_settings.language = "deutsch";
	g_settings.timezone = "(GMT+01:00) Amsterdam, Berlin, Bern, Rome, Vienna";
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);

	REQUIRE(box.out.languages.size() == 1);
	REQUIRE(box.out.languages[0] == "deutsch");
	REQUIRE(box.out.count("timezone") == 1);
	// The guide is not up yet.
	REQUIRE(box.out.count("guide") == 0);
}

/* The program loads the catalog itself before the first phase, falling back when
   the box has none, and menus hold its texts by then. */
TEST_CASE("the language the program loaded at startup is not loaded a second time", "[apply][language]")
{
	ApplyFresh fresh;
	LanguageBox box(ApplyPhase::Framebuffer);
	registerApplyGroups();

	g_settings.language = "english";
	noteLanguageLoaded("english");
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	REQUIRE(box.out.count("language") == 0);
	REQUIRE(box.out.count("timezone") == 1);

	g_settings.language = "deutsch";
	REQUIRE(applyKey("language") == Status::Ok);
	REQUIRE(box.out.languages.size() == 1);
	REQUIRE(box.out.languages[0] == "deutsch");
}

TEST_CASE("a key of the language groups sends that group only and only a changed value", "[apply][language]")
{
	ApplyFresh fresh;
	LanguageBox box(ApplyPhase::Framebuffer);
	registerApplyGroups();
	g_settings.language = "deutsch";
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.out.forget();

	g_settings.timezone = "(GMT) London";
	REQUIRE(applyKey("timezone") == Status::Ok);
	REQUIRE(box.out.calls.size() == 1);
	REQUIRE(box.out.calls[0] == "timezone");
	box.out.forget();

	// The same zone again links nothing.
	REQUIRE(applyKey("timezone") == Status::Ok);
	REQUIRE(applyKey("language") == Status::Ok);
	REQUIRE(box.out.calls.empty());
}

TEST_CASE("a catalog that is not installed is tried again by the next run", "[apply][language]")
{
	ApplyFresh fresh;
	LanguageBox box(ApplyPhase::Framebuffer);
	registerApplyGroups();
	g_settings.language = "klingon";
	box.out.language_answer = Status::NotFound;
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::NotFound);

	box.out.language_answer = Status::Ok;
	REQUIRE(applyKey("language") == Status::Ok);
	REQUIRE(box.out.count("language") == 2);
}

namespace
{

std::vector<std::string> guideNames(const char *a, const char *b, const char *c)
{
	std::vector<std::string> v;
	v.push_back(a);
	v.push_back(b);
	v.push_back(c);
	return v;
}

}

/* The guide keeps its own list of languages in a file, and the three settings are
   never empty, so a push at startup would rewrite that file at every start. */
TEST_CASE("the guide languages are not pushed at startup", "[apply][language]")
{
	ApplyFresh fresh;
	LanguageBox box(ApplyPhase::Sectionsd);
	registerApplyGroups();
	g_settings.pref_lang[0] = "German";
	g_settings.pref_lang[1] = "English";
	g_settings.pref_lang[2] = "none";
	noteGuideLanguagesLoaded(guideNames("German", "English", "none"));
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	REQUIRE(runPhase(ApplyPhase::Sectionsd) == Status::Ok);
	REQUIRE(box.out.guide.empty());
	REQUIRE(box.out.count("guide") == 0);
}

/* A web or MCP write comes in before the guide is up and is answered Busy; the phase
   is what makes it good, with the languages the write left. */
TEST_CASE("a guide language written before the guide is up is pushed once by its phase", "[apply][language]")
{
	ApplyFresh fresh;
	LanguageBox box(ApplyPhase::Sectionsd);
	registerApplyGroups();
	noteGuideLanguagesLoaded(guideNames("German", "English", "none"));
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);

	g_settings.pref_lang[0] = "German";
	g_settings.pref_lang[1] = "French";
	g_settings.pref_lang[2] = "none";
	REQUIRE(applyKey("pref_lang_1") == Status::Busy);
	REQUIRE(box.out.guide.empty());

	REQUIRE(runPhase(ApplyPhase::Sectionsd) == Status::Ok);
	REQUIRE(box.out.guide.size() == 1);
	REQUIRE(box.out.guide[0] == guideNames("German", "French", "none"));
}

TEST_CASE("a change of a guide language after startup is pushed once, with all three", "[apply][language]")
{
	ApplyFresh fresh;
	LanguageBox box(ApplyPhase::Sectionsd);
	registerApplyGroups();
	g_settings.pref_lang[0] = "German";
	g_settings.pref_lang[1] = "English";
	g_settings.pref_lang[2] = "none";
	noteGuideLanguagesLoaded(guideNames("German", "English", "none"));
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	REQUIRE(runPhase(ApplyPhase::Sectionsd) == Status::Ok);

	g_settings.pref_lang[1] = "French";
	REQUIRE(applyKey("pref_lang_1") == Status::Ok);
	REQUIRE(box.out.guide.size() == 1);
	REQUIRE(box.out.guide[0] == guideNames("German", "French", "none"));

	// The same languages again are not sent again.
	REQUIRE(applyKey("pref_lang_2") == Status::Ok);
	REQUIRE(box.out.guide.size() == 1);
}

TEST_CASE("the guide is told the codes of the named languages and none for none", "[language]")
{
	std::map<std::string, std::string> codes;
	codes["deu"] = "German";
	codes["ger"] = "German";
	codes["eng"] = "English";
	codes["fra"] = "French";

	std::vector<std::string> names;
	names.push_back("German");
	names.push_back("none");
	names.push_back("");
	names.push_back("English");
	names.push_back("Klingon");

	const std::vector<std::string> got = guideLanguageCodes(names, codes);
	REQUIRE(got.size() == 3);
	CHECK(got[0] == "deu");
	CHECK(got[1] == "ger");
	CHECK(got[2] == "eng");

	CHECK(guideLanguageCodes(std::vector<std::string>(), codes).empty());
}

TEST_CASE("a web batch writing the language, the zone and a guide language runs each group once", "[apply][language]")
{
	ApplyFresh fresh;
	LanguageBox box(ApplyPhase::Sectionsd);
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, applyNothingToSave);

	g_settings.language = "deutsch";
	noteGuideLanguagesLoaded(guideNames(g_settings.pref_lang[0].c_str(), g_settings.pref_lang[1].c_str(),
					    g_settings.pref_lang[2].c_str()));
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	REQUIRE(runPhase(ApplyPhase::Sectionsd) == Status::Ok);
	box.out.forget();

	REQUIRE(settings::set("language", "english").ok());
	REQUIRE(settings::set("timezone", "(GMT) London").ok());
	REQUIRE(settings::set("pref_lang_0", "English").ok());
	REQUIRE(settings::set("pref_lang_2", "German").ok());
	REQUIRE(box.out.calls.empty());
	applyPendingSettings();

	REQUIRE(box.out.count("language") == 1);
	REQUIRE(box.out.languages[0] == "english");
	REQUIRE(box.out.count("timezone") == 1);
	REQUIRE(box.out.count("guide") == 1);
	REQUIRE(events.sent.empty());

	installRealSettingsSource(NULL, NULL);
}

namespace
{

// A directory of the case's own, removed with everything the case put in it.
struct ScratchDir
{
	std::string path;
	std::vector<std::string> files;
	ScratchDir()
	{
		char tmpl[] = "/tmp/coreapi-choices-XXXXXX";
		path = mkdtemp(tmpl) ? tmpl : "";
	}
	~ScratchDir()
	{
		for (size_t i = 0; i < files.size(); ++i)
			unlink(files[i].c_str());
		for (size_t i = dirs.size(); i > 0; --i)
			rmdir(dirs[i - 1].c_str());
		rmdir(path.c_str());
	}
	std::vector<std::string> dirs;
	std::string sub(const std::string &name)
	{
		const std::string p = path + "/" + name;
		mkdir(p.c_str(), 0700);
		dirs.push_back(p);
		return p;
	}
	void put(const std::string &full, const std::string &content)
	{
		std::ofstream f(full.c_str());
		f << content;
		files.push_back(full);
	}
};

std::vector<std::string> labelsOf(const std::vector<SettingChoice> &list)
{
	std::vector<std::string> out;
	for (size_t i = 0; i < list.size(); ++i)
	{
		CHECK(list[i].label_key.empty());
		out.push_back(list[i].label);
	}
	return out;
}

} // namespace

TEST_CASE("the catalogs of the directories are listed once each by their name", "[language][choices]")
{
	ScratchDir dir;
	REQUIRE(!dir.path.empty());
	const std::string shipped = dir.sub("shipped"), extra = dir.sub("extra");
	dir.put(shipped + "/english.locale", "x");
	dir.put(shipped + "/deutsch.locale", "x");
	dir.put(shipped + "/readme.txt", "x");
	dir.put(extra + "/english.locale", "x");
	dir.put(extra + "/polski.locale", "x");

	std::vector<std::string> dirs;
	dirs.push_back(shipped);
	dirs.push_back(extra);
	dirs.push_back(dir.path + "/missing");
	std::vector<SettingChoice> got;
	REQUIRE(localesIn(dirs, got));
	const std::vector<std::string> names = labelsOf(got);
	REQUIRE(names.size() == 3);
	CHECK(names[0] == "deutsch");
	CHECK(names[1] == "english");
	CHECK(names[2] == "polski");
	// A text row stores the name, so the entry has to carry it as its text.
	for (size_t i = 0; i < got.size(); ++i)
		CHECK(got[i].text == got[i].label);

	// Nothing to offer leaves what the caller had.
	std::vector<SettingChoice> kept(1);
	kept[0].label = "kept";
	std::vector<std::string> none;
	none.push_back(dir.path + "/missing");
	CHECK_FALSE(localesIn(none, kept));
	REQUIRE(kept.size() == 1);
	CHECK(kept[0].label == "kept");
}

TEST_CASE("the preferred languages offer none and the names of the box's table once each in the alphabet", "[language][choices]")
{
	ScratchDir dir;
	REQUIRE(!dir.path.empty());
	dir.put(dir.path + "/iso-639.tab",
		"## a comment\n"
		"nno\tnno\tnn\tNorwegian\n"
		"nob\tnob\tnb\tNorwegian\n"
		"ger\tdeu\tde\tGerman\n"
		"aar\taar\taa\tOld High Dutch\n");

	std::vector<SettingChoice> got;
	REQUIRE(languagesFrom(dir.path + "/iso-639.tab", got));
	const std::vector<std::string> names = labelsOf(got);
	REQUIRE(names.size() == 4);
	CHECK(names[0] == "none");
	CHECK(names[1] == "German");
	CHECK(names[2] == "Norwegian");
	CHECK(names[3] == "Old High Dutch");
	CHECK(got[2].text == "Norwegian");

	std::vector<SettingChoice> kept(1);
	CHECK_FALSE(languagesFrom(dir.path + "/absent.tab", kept));
	CHECK(kept.size() == 1);

	// The three rows offer them to a screen.
	const char *const rows[] = { "pref_lang_0", "pref_lang_1", "pref_lang_2" };
	for (size_t i = 0; i < 3; ++i)
	{
		INFO(rows[i]);
		const Descriptor *d = settings::findRow(rows[i]);
		REQUIRE(d != NULL);
		CHECK(d->choices_from == languageNames);
	}
}

TEST_CASE("the zones of the list are offered when their zone file is installed", "[language][choices]")
{
	ScratchDir dir;
	REQUIRE(!dir.path.empty());
	dir.sub("usr");
	dir.sub("usr/share");
	dir.sub("usr/share/zoneinfo");
	const std::string europe = dir.sub("usr/share/zoneinfo/Europe");
	dir.put(europe + "/Berlin", "tz");
	dir.put(dir.path + "/timezone.xml",
		"<?xml version=\"1.0\"?>\n<zones>\n"
		"<zone name=\"Berlin time\" zone=\"Europe/Berlin\"/>\n"
		"<zone name=\"Nowhere time\" zone=\"Europe/Nowhere\"/>\n"
		"</zones>\n");

	std::vector<SettingChoice> got;
	REQUIRE(timezonesFrom(dir.path + "/timezone.xml", dir.path, got));
	const std::vector<std::string> names = labelsOf(got);
	REQUIRE(names.size() == 1);
	CHECK(names[0] == "Berlin time");
	CHECK(got[0].text == "Berlin time");

	std::vector<SettingChoice> kept(1);
	CHECK_FALSE(timezonesFrom(dir.path + "/absent.xml", dir.path, kept));
	CHECK(kept.size() == 1);
}
