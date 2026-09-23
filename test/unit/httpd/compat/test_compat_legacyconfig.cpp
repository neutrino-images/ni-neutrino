/*
 * test_compat_legacyconfig.cpp - tests for the legacy configuration reader
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

// compat/ is not compiled with --disable-legacy-api (src/httpd/Makefile.am).
#include <config.h>

#ifndef DISABLE_LEGACY_API

#include "httpd/compat/legacyconfig.h"

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

using httpd::compat::CStringList;
using httpd::compat::legacyWebsiteMainConfig;

namespace
{

// A config file this suite writes and removes itself, the way
// test_mysystem.cpp's Spawned does for a script. mkstemp() rather than a
// fixed name: two cases running the same binary twice in a row must not
// read a leftover from the first.
struct TempConfigFile
{
	bool        ready;
	std::string path;

	explicit TempConfigFile(const std::string &body) : ready(false)
	{
		char tmpl[] = "/tmp/coreapi_legacyconfig_XXXXXX";
		int fd = mkstemp(tmpl);
		if (fd < 0)
			return;
		path = tmpl;
		ssize_t n = write(fd, body.data(), body.size());
		ready = (close(fd) == 0) && (n == (ssize_t) body.size());
	}

	~TempConfigFile()
	{
		if (!path.empty())
			unlink(path.c_str());
	}

private:
	TempConfigFile(const TempConfigFile &);
	TempConfigFile &operator=(const TempConfigFile &);
};

// The one thing CControlAPI::init() does with either value: PLUGIN_DIRS[i]
// becomes exactly this, and yExecuteScript then calls access() on it. A
// value that makes this equal "/" + script is the bug; nothing else about
// the string matters here.
std::string asSearchedForScript(const std::string &plugin_dir, const std::string &script)
{
	return plugin_dir + "/" + script;
}

} // namespace

TEST_CASE("a config file that does not exist never yields a path that searches the filesystem root", "[compat][legacyconfig]")
{
	const CStringList config = legacyWebsiteMainConfig("/tmp/coreapi-legacyconfig-does-not-exist-4711");

	REQUIRE(asSearchedForScript(config.at("WebsiteMain.override_directory"), "exec.sh") != "/exec.sh");
	REQUIRE(asSearchedForScript(config.at("WebsiteMain.directory"), "exec.sh") != "/exec.sh");
}

TEST_CASE("a config file with both keys set is read verbatim", "[compat][legacyconfig]")
{
	TempConfigFile conf(
		"WebsiteMain.override_directory=/tmp/coreapi-legacyconfig-override\n"
		"WebsiteMain.directory=/tmp/coreapi-legacyconfig-main\n");
	REQUIRE(conf.ready);

	const CStringList config = legacyWebsiteMainConfig(conf.path.c_str());

	REQUIRE(config.at("WebsiteMain.override_directory") == "/tmp/coreapi-legacyconfig-override");
	REQUIRE(config.at("WebsiteMain.directory") == "/tmp/coreapi-legacyconfig-main");
}

TEST_CASE("a key present but written empty falls back the same way an absent key does", "[compat][legacyconfig]")
{
	// This is the case an old, minimal nhttpd.conf can actually produce: the
	// key is there, on purpose or not, with nothing after the "=". A reader
	// that only defaults for a missing key would still hand init() the ""
	// that turns into the filesystem root.
	TempConfigFile conf(
		"WebsiteMain.override_directory=\n"
		"WebsiteMain.directory=/tmp/coreapi-legacyconfig-main\n");
	REQUIRE(conf.ready);

	const CStringList config = legacyWebsiteMainConfig(conf.path.c_str());

	REQUIRE(config.at("WebsiteMain.override_directory") != "");
	REQUIRE(asSearchedForScript(config.at("WebsiteMain.override_directory"), "exec.sh") != "/exec.sh");
	REQUIRE(config.at("WebsiteMain.directory") == "/tmp/coreapi-legacyconfig-main");
}

TEST_CASE("a box without the old file looks for scripts where this image installs them", "[compat][legacyconfig]")
{
	// The removed server installed that file, and nothing installs it any more, so
	// every box that did not come here from an older image takes this fallback.
	// CControlAPI::init() appends "/scripts" to both values and data/httpd-scripts
	// installs into PRIVATE_HTTPDDIR/scripts, so a fallback naming anything else is
	// /control/exec finding none of the scripts that are on the box. These are also the
	// two values that file shipped with.
	const CStringList config = legacyWebsiteMainConfig("/tmp/coreapi-legacyconfig-no-old-file-4711");

	REQUIRE(config.at("WebsiteMain.directory") == PRIVATE_HTTPDDIR);
	REQUIRE(config.at("WebsiteMain.override_directory") == PUBLIC_HTTPDDIR);
}

TEST_CASE("an old file that names its own directories is still read for them", "[compat][legacyconfig]")
{
	// The other half of the case above: an owner who moved those directories
	// wrote it down in that file and nowhere else, so the file wins over the
	// fallback rather than the other way round.
	TempConfigFile conf(
		"WebsiteMain.override_directory=/tmp/coreapi-legacyconfig-moved-override\n"
		"WebsiteMain.directory=/tmp/coreapi-legacyconfig-moved-main\n");
	REQUIRE(conf.ready);

	const CStringList config = legacyWebsiteMainConfig(conf.path.c_str());

	REQUIRE(config.at("WebsiteMain.directory") != PRIVATE_HTTPDDIR);
	REQUIRE(config.at("WebsiteMain.directory") == "/tmp/coreapi-legacyconfig-moved-main");
	REQUIRE(config.at("WebsiteMain.override_directory") == "/tmp/coreapi-legacyconfig-moved-override");
}

TEST_CASE("neither key ever comes back as an empty string", "[compat][legacyconfig]")
{
	// The one invariant CControlAPI::init() depends on, checked directly
	// rather than through the two more specific cases above: whatever this
	// function is given, "" is never one of the two answers.
	const CStringList absent = legacyWebsiteMainConfig("/tmp/coreapi-legacyconfig-still-not-there-4711");
	REQUIRE_FALSE(absent.at("WebsiteMain.override_directory").empty());
	REQUIRE_FALSE(absent.at("WebsiteMain.directory").empty());

	TempConfigFile blank("WebsiteMain.override_directory=\nWebsiteMain.directory=\n");
	REQUIRE(blank.ready);
	const CStringList empty_keys = legacyWebsiteMainConfig(blank.path.c_str());
	REQUIRE_FALSE(empty_keys.at("WebsiteMain.override_directory").empty());
	REQUIRE_FALSE(empty_keys.at("WebsiteMain.directory").empty());
}

#endif // DISABLE_LEGACY_API
