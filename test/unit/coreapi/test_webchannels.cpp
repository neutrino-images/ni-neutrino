/*
 * test_webchannels.cpp - the web channel files the box loads from its own folders
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

#include "coreapi/settings/settings.h"
#include "coreapi/webchannels.h"

#include <system/settings.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include <string>
#include <vector>

extern SNeutrinoSettings g_settings;

using namespace coreapi;

namespace
{

// A folder of its own per case, removed with everything put into it.
struct ListFolder
{
	std::string path;
	std::vector<std::string> made;

	ListFolder()
	{
		char dir[] = "/tmp/webchanXXXXXX";
		REQUIRE(mkdtemp(dir) != NULL);
		path = dir;
	}
	~ListFolder()
	{
		for (size_t i = made.size(); i > 0; --i)
		{
			if (unlink(made[i - 1].c_str()) != 0)
				rmdir(made[i - 1].c_str());
		}
		rmdir(path.c_str());
	}

	std::string file(const std::string &name, const char *content)
	{
		const std::string at = path + "/" + name;
		FILE *f = fopen(at.c_str(), "w");
		REQUIRE(f != NULL);
		fputs(content, f);
		fclose(f);
		made.push_back(at);
		return at;
	}

	std::string folder(const std::string &name)
	{
		const std::string at = path + "/" + name;
		REQUIRE(mkdir(at.c_str(), 0755) == 0);
		made.push_back(at);
		return at;
	}
};

struct KeptSwitches
{
	int tv, radio;
	KeptSwitches() : tv(g_settings.webtv_xml_auto), radio(g_settings.webradio_xml_auto) {}
	~KeptSwitches()
	{
		g_settings.webtv_xml_auto = tv;
		g_settings.webradio_xml_auto = radio;
	}
};

std::vector<std::string> one(const std::string &dir)
{
	return std::vector<std::string>(1, dir);
}

} // namespace

TEST_CASE("autoloading takes the list files of a folder that are not empty, in the alphabet", "[webchannels]")
{
	ListFolder dir;
	const std::string b = dir.file("b.m3u", "#EXTM3U\n");
	const std::string a = dir.file("a.xml", "<x/>");
	const std::string c = dir.file("c.tv", "x");
	dir.file("empty.xml", "");
	dir.file("notes.txt", "x");
	dir.file("xml", "x");

	const std::vector<std::string> got = webchannels::autoloadIn(one(dir.path), std::vector<std::string>());
	REQUIRE(got.size() == 3);
	CHECK(got[0] == a);
	CHECK(got[1] == b);
	CHECK(got[2] == c);
	CHECK(webchannels::autoloadIn(one("/nonexistent/webtv"), std::vector<std::string>()).empty());
}

TEST_CASE("a name the user lists or another folder gave is autoloaded once", "[webchannels]")
{
	ListFolder dir;
	const std::string first = dir.folder("var");
	const std::string second = dir.folder("shipped");
	dir.file("var/own.xml", "<x/>");
	const std::string both = dir.file("var/both.xml", "<x/>");
	dir.file("shipped/both.xml", "<x/>");
	const std::string extra = dir.file("shipped/extra.xml", "<x/>");

	std::vector<std::string> dirs;
	dirs.push_back(first);
	dirs.push_back(second);
	// Listed by the user from somewhere else, by an address or by hand.
	std::vector<std::string> listed;
	listed.push_back("/media/hdd/own.xml");
	listed.push_back("http://example.com/list.m3u");

	const std::vector<std::string> got = webchannels::autoloadIn(dirs, listed);
	REQUIRE(got.size() == 2);
	CHECK(got[0] == both);
	CHECK(got[1] == extra);
}

TEST_CASE("a folder named twice is read once", "[webchannels]")
{
	ListFolder dir;
	const std::string a = dir.file("a.xml", "<x/>");
	std::vector<std::string> dirs;
	dirs.push_back(dir.path);
	dirs.push_back(dir.path + "/");

	const std::vector<std::string> got = webchannels::autoloadIn(dirs, std::vector<std::string>());
	REQUIRE(got.size() == 1);
	CHECK(got[0] == a);
}

TEST_CASE("each kind reads its own two folders", "[webchannels]")
{
	const std::vector<std::string> tv = webchannels::autoloadDirs(false);
	const std::vector<std::string> radio = webchannels::autoloadDirs(true);
	REQUIRE(tv.size() == 2);
	REQUIRE(radio.size() == 2);
	CHECK(tv[0] == WEBTVDIR_VAR);
	CHECK(tv[1] == WEBTVDIR);
	CHECK(radio[0] == WEBRADIODIR_VAR);
	CHECK(radio[1] == WEBRADIODIR);
}

TEST_CASE("nothing is autoloaded while the switch of the kind is off", "[webchannels]")
{
	KeptSwitches kept;
	g_settings.webtv_xml_auto = 0;
	g_settings.webradio_xml_auto = 0;

	CHECK(webchannels::autoloaded(false).empty());
	CHECK(webchannels::autoloaded(true).empty());
	std::vector<SettingChoice> none;
	CHECK_FALSE(webchannels::webtvAutoloaded(none));
	CHECK_FALSE(webchannels::webradioAutoloaded(none));

	Result<std::vector<SettingChoice> > asked = settings::choices("webtv_xml");
	REQUIRE_FALSE(asked.ok());
	CHECK(asked.error().code == ErrorCode::ChoicesUnavailable);
}

/* The menu and the web read the files off the list rows, so both have to name the
   same provider the channel daemon's folders come from. */
TEST_CASE("the list rows offer what the channel daemon autoloads beside them", "[webchannels]")
{
	KeptSwitches kept;
	REQUIRE(settings::findRow("webtv_xml") != NULL);
	REQUIRE(settings::findRow("webradio_xml") != NULL);
	CHECK(settings::findRow("webtv_xml")->choices_from == &webchannels::webtvAutoloaded);
	CHECK(settings::findRow("webradio_xml")->choices_from == &webchannels::webradioAutoloaded);

	g_settings.webtv_xml_auto = 1;
	const std::vector<std::string> files = webchannels::autoloaded(false);
	Result<std::vector<SettingChoice> > asked = settings::choices("webtv_xml");
	REQUIRE(asked.ok() == !files.empty());
	for (size_t i = 0; asked.ok() && i < files.size(); ++i)
	{
		REQUIRE(i < asked.value().size());
		CHECK(asked.value()[i].text == files[i]);
		CHECK(asked.value()[i].label == files[i]);
	}
}

TEST_CASE("a write of the list is not held to what is autoloaded", "[webchannels]")
{
	CHECK(settings::check("webtv_xml", "http://example.com/list.m3u\n/media/hdd/own.xml").ok());
	CHECK(settings::check("webradio_xml", "").ok());
}
