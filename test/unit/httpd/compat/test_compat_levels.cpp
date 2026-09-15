/*
 * test_compat_levels.cpp - tests for the legacy permission levels
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

#include "httpd/compat/levels.h"
#include "httpd/compat/query.h"

#include "httpd/auth.h"
#include "httpd/credentials.h"
#include "httpd/webconfig.h"

#include <cstddef>
#include <string>

using httpd::AuthLevel;
using httpd::Credentials;
using httpd::granted;
using httpd::compat::legacyAllows;
using httpd::compat::levelFor;

namespace
{

Credentials fromPeer(const std::string &peer)
{
	Credentials c;
	c.peer = peer;
	return c;
}

/* Puts the login policy back where the next case finds it. Only the two
   Basic cases below ever change it, and each holds one of these for its own
   length, the same pattern test_auth.cpp's Policy uses for the same
   reason: what is in effect is one value for the whole program. */
struct Policy
{
	Policy() {}
	~Policy() { httpd::setConfigForTest(httpd::defaultWebConfig()); }

private:
	Policy(const Policy &);
	Policy &operator=(const Policy &);
};

// The account the two Basic cases below carry, set here rather than left at
// whatever the shipped default happens to be, so neither case is tied to a
// value this file does not own.
void setLegacyAccount(const std::string &user, const std::string &pass)
{
	httpd::WebConfig c = httpd::defaultWebConfig();
	c.username = user;
	c.password_hash = httpd::hashSecret(pass);
	httpd::setConfigForTest(c);
}

} // namespace

TEST_CASE("every endpoint in the table has a level", "[compat][levels]")
{
	// The names come from the endpoint table itself, so a new endpoint
	// without a level fails here rather than answering at whatever the
	// lookup returns for a name nobody wrote down.
	for (size_t i = 0; i < httpd::compat::endpointCount(); ++i)
	{
		const char *name = httpd::compat::endpointName(i);
		INFO(name);
		REQUIRE(httpd::compat::hasLevel(name));
	}
}

// The case above is read off the same table endpointCount() and
// endpointName() answer for, so a row going missing from it removes itself
// from what that case ever asks about rather than failing it. A bare count
// is what notices a row missing, without copying the table's own 74 names
// in here to drift against it.
TEST_CASE("the table carries all 74 legacy endpoints", "[compat][levels]")
{
	REQUIRE(httpd::compat::endpointCount() == 74);
}

namespace
{

/* The level every one of the 74 legacy endpoints is meant to answer for an empty query.
   An empty query is answerable for every Kind without knowing which handler a name
   reaches: Read for Read, Write for Write, System for System, and Read for Mixed too,
   since every Mixed handler's own empty-ParamList branch only ever reports.

   Kept here on purpose, a second table beside levels.cpp's own kTable, for the reason
   that file gives for keeping kTable beside yCgiCallList: a table that gates
   /control/shutdown is worth writing down twice. */
struct ExpectedLevel
{
	const char *name;
	AuthLevel   level;
};

const ExpectedLevel kExpectedLevel[] =
{
	{ "addbouquet",       AuthLevel::Write },
	{ "aspectratio",      AuthLevel::Read },
	{ "audio",            AuthLevel::Read },
	{ "avinputmode",      AuthLevel::Read },
	{ "boxinfo",          AuthLevel::Read },
	{ "build_live_url",   AuthLevel::Read },
	{ "build_playlist",   AuthLevel::Read },
	{ "changebouquet",    AuthLevel::Write },
	{ "channellist",      AuthLevel::Read },
	{ "config",           AuthLevel::Read },
	{ "crypt",            AuthLevel::Read },
	{ "currenttpchannels",AuthLevel::Read },
	{ "deletebouquet",    AuthLevel::Write },
	{ "epg",              AuthLevel::Read },
	{ "epgsearch",        AuthLevel::Read },
	{ "epgsearchxml",     AuthLevel::Read },
	{ "exec",             AuthLevel::System },
	{ "file",             AuthLevel::Read },
	{ "get_logo",         AuthLevel::Read },
	{ "getbouquet",       AuthLevel::Read },
	{ "getbouquets",      AuthLevel::Read },
	{ "getbouquetsxml",   AuthLevel::Read },
	{ "getchannel",       AuthLevel::Read },
	{ "getchannelid",     AuthLevel::Read },
	{ "getchannelinfo",   AuthLevel::Read },
	{ "getdate",          AuthLevel::Read },
	{ "getdir",           AuthLevel::Read },
	{ "getepgid",         AuthLevel::Read },
	{ "getmode",          AuthLevel::Read },
	{ "getmovies",        AuthLevel::Read },
	{ "getonidsid",       AuthLevel::Read },
	{ "getservicesxml",   AuthLevel::Read },
	{ "gettime",          AuthLevel::Read },
	{ "getubouquetsxml",  AuthLevel::Read },
	{ "glcdscreenshot",   AuthLevel::Write },
	{ "info",             AuthLevel::Read },
	{ "infoicons",        AuthLevel::Read },
	{ "lcd",              AuthLevel::Read },
	{ "logolist",         AuthLevel::Read },
	{ "message",          AuthLevel::Write },
	{ "movebouquet",      AuthLevel::Write },
	{ "rc",               AuthLevel::Read },
	{ "rcem",             AuthLevel::Write },
	{ "reboot",           AuthLevel::System },
	{ "reloadchannels",   AuthLevel::Write },
	{ "reloadplugins",    AuthLevel::Write },
	{ "reloadsetup",      AuthLevel::Write },
	{ "renamebouquet",    AuthLevel::Write },
	{ "restart",          AuthLevel::System },
	{ "savebouquet",      AuthLevel::Write },
	{ "screenshot",       AuthLevel::Write },
	{ "sendalltimers",    AuthLevel::Write },
	{ "setbouquet",       AuthLevel::Write },
	{ "setmode",          AuthLevel::Read },
	{ "shutdown",         AuthLevel::System },
	{ "signal",           AuthLevel::Read },
	{ "standby",          AuthLevel::Read },
	{ "startplugin",      AuthLevel::System },
	{ "statfs",           AuthLevel::Read },
	{ "system",           AuthLevel::System },
	{ "timer",            AuthLevel::Read },
	{ "updatebouquet",    AuthLevel::Write },
	{ "updateinfo",       AuthLevel::Read },
	{ "vcroutput",        AuthLevel::Read },
	{ "version",          AuthLevel::Read },
	{ "videoformat",      AuthLevel::Read },
	{ "videooutput",      AuthLevel::Read },
	{ "volume",           AuthLevel::Read },
	{ "xmltv.data",       AuthLevel::Read },
	{ "xmltv.m3u",        AuthLevel::Read },
	{ "xmltv.xml",        AuthLevel::Read },
	{ "xmltvlist",        AuthLevel::Write },
	{ "yweb",             AuthLevel::System },
	{ "zapto",            AuthLevel::Read }
};

const size_t kExpectedLevelCount = sizeof(kExpectedLevel) / sizeof(kExpectedLevel[0]);

} // namespace

TEST_CASE("every endpoint answers the level recorded for it here", "[compat][levels]")
{
	// Walked off the live table (endpointName/endpointCount), not off a
	// second copy of its names kept only in this file: a name that moved, was
	// renamed, or lost its row shows up here as whatever the live table
	// currently says, never as what this file last happened to remember for
	// the name it used to have.
	for (size_t i = 0; i < httpd::compat::endpointCount(); ++i)
	{
		const char *name = httpd::compat::endpointName(i);
		INFO(name);

		const ExpectedLevel *want = NULL;
		for (size_t j = 0; j < kExpectedLevelCount; ++j)
		{
			if (std::string(name) == kExpectedLevel[j].name)
			{
				want = &kExpectedLevel[j];
				break;
			}
		}

		// A live name with nothing recorded for it here is exactly the drift
		// this case exists to catch, so it fails rather than being skipped.
		REQUIRE(want != NULL);
		REQUIRE(levelFor(name, "") == want->level);
	}
}

TEST_CASE("the mixed endpoints read their parameters", "[compat][levels]")
{
	using httpd::AuthLevel;
	REQUIRE(levelFor("standby", "")            == AuthLevel::Read);
	REQUIRE(levelFor("standby", "on")          == AuthLevel::Write);
	REQUIRE(levelFor("setmode", "status")      == AuthLevel::Read);
	REQUIRE(levelFor("setmode", "tv")          == AuthLevel::Write);
	REQUIRE(levelFor("timer", "")              == AuthLevel::Read);
	REQUIRE(levelFor("timer", "action=new")    == AuthLevel::Write);

	// VolumeCGI (controlapi.cpp) reads only ParamList["1"], the bare token a
	// query with no "=" leaves there. "vol=50" would leave "50" under "2",
	// not "1" (query.cpp's positional counter runs over the whole list, named
	// pairs included), and VolumeCGI would answer error rather than set
	// anything - so the write case below has to be the bare form.
	REQUIRE(levelFor("volume", "status")       == AuthLevel::Read);
	REQUIRE(levelFor("volume", "50")           == AuthLevel::Write);

	REQUIRE(levelFor("rc", "status")           == AuthLevel::Read);
	REQUIRE(levelFor("rc", "lock")             == AuthLevel::Write);

	REQUIRE(levelFor("videoformat", "status")  == AuthLevel::Read);
	REQUIRE(levelFor("videoformat", "pal")     == AuthLevel::Write);

	// ZaptoCGI's read-only branches are keyed off ParamList["1"] the same
	// bare-token way; anything else, a plain channel name included, falls
	// through to ZapTo() and switches.
	REQUIRE(levelFor("zapto", "getpids")       == AuthLevel::Read);
	REQUIRE(levelFor("zapto", "ARD")           == AuthLevel::Write);

	REQUIRE(levelFor("config", "config=neutrino")               == AuthLevel::Read);
	REQUIRE(levelFor("config", "config=neutrino&action=submit") == AuthLevel::Write);

	REQUIRE(levelFor("file", "action=list&path=/media")   == AuthLevel::Read);
	REQUIRE(levelFor("file", "action=delete&path=/media") == AuthLevel::Write);

	REQUIRE(levelFor("infoicons", "status")    == AuthLevel::Read);
	REQUIRE(levelFor("infoicons", "static")    == AuthLevel::Write);
}

TEST_CASE("a command endpoint is never less than system", "[compat][levels]")
{
	using httpd::AuthLevel;
	REQUIRE(levelFor("exec", "")        == AuthLevel::System);
	REQUIRE(levelFor("system", "")      == AuthLevel::System);
	REQUIRE(levelFor("startplugin", "") == AuthLevel::System);
	REQUIRE(levelFor("yweb", "")        == AuthLevel::System);
}

/* The floor under the whole table: what a name nobody wrote down is worth. levelFor
   answers System for it, which refuses rather than guesses. Turned the other way it
   fails open: every name yCgiCallList gains and kTable has not been told about would
   answer out of the LAN with no credential at all.

   check-legacy-levels.sh holds the two tables to each other, so it catches the names
   drifting apart. It cannot catch what happens to a name neither of them carries,
   because there is no such name for it to compare. That is this case.

   Held at both levels it is read at: levelFor, which is what the table says, and
   legacyAllows, which is what the gate does with it. The second is not implied by the
   first, and it is the one a caller meets. */
TEST_CASE("a name the table does not carry is refused, not read", "[compat][levels]")
{
	using httpd::AuthLevel;

	REQUIRE_FALSE(httpd::compat::hasLevel("gibtesnicht"));
	REQUIRE(levelFor("gibtesnicht", "") == AuthLevel::System);

	// Parameters cannot talk it down either: mixedLevel is reached off the
	// row, and there is no row.
	REQUIRE(levelFor("gibtesnicht", "format=json") == AuthLevel::System);

	// The name a bare /control/ leaves behind, which is the one an empty
	// path splits into and the one nothing has to misspell to produce.
	REQUIRE_FALSE(httpd::compat::hasLevel(""));
	REQUIRE(levelFor("", "") == AuthLevel::System);

	// A name the table does carry, spelled the way the table does not. The
	// lookup compares whole strings, so this is a name nobody wrote down.
	REQUIRE_FALSE(httpd::compat::hasLevel("Standby"));
	REQUIRE(levelFor("Standby", "") == AuthLevel::System);

	// And what the gate makes of all three for a caller off the LAN, which
	// is what the box actually answers.
	const Credentials lan = fromPeer("192.168.1.9");
	REQUIRE(granted(lan) == AuthLevel::Read);
	REQUIRE_FALSE(legacyAllows("gibtesnicht", "", granted(lan), lan));
	REQUIRE_FALSE(legacyAllows("", "", granted(lan), lan));
	REQUIRE_FALSE(legacyAllows("Standby", "", granted(lan), lan));
}

TEST_CASE("the private range reads the legacy surface and does not write", "[compat][levels]")
{
	using httpd::AuthLevel;
	const Credentials lan = fromPeer("192.168.1.9");
	REQUIRE(legacyAllows("getmode", "",   granted(lan), lan));
	REQUIRE_FALSE(legacyAllows("standby", "on", granted(lan), lan));
}

TEST_CASE("basic carries a caller from the private range", "[compat][levels]")
{
	// timerlist.cpp:815 builds exactly this form for a remote box
	Policy policy;
	setLegacyAccount("ni", "ni");

	Credentials lan = fromPeer("192.168.1.9");
	lan.basic = "ni:ni";
	REQUIRE(legacyAllows("standby", "on", granted(lan), lan));

	// standby?on alone leaves every System name unproven: a build that
	// answered Basic no higher than Write would pass the line above and
	// still lock a correctly authenticated remote box out of exec, system,
	// startplugin, yweb, shutdown, reboot and restart.
	REQUIRE(legacyAllows("exec", "", granted(lan), lan));
}

TEST_CASE("basic is not accepted outside the legacy surface", "[compat][levels]")
{
	Policy policy;
	setLegacyAccount("ni", "ni");

	Credentials lan = fromPeer("192.168.1.9");
	lan.basic = "ni:ni";
	REQUIRE(granted(lan) == AuthLevel::Read);
}

// The old server let loopback through unconditionally, authenticate=true or not
// (mod_auth.cpp:19), and 22 files in the official plugin repo are built against exactly
// that: all 19 hardcoded addresses among them are loopback and none carries a
// credential. This is the case that promise rests on.
TEST_CASE("a loopback caller with no credential still reaches every level, which is what the plugin scripts rely on", "[compat][levels]")
{
	const Credentials loop = fromPeer("127.0.0.1");
	REQUIRE(legacyAllows("standby", "on", granted(loop), loop));
	REQUIRE(legacyAllows("exec", "",      granted(loop), loop));
}


//=============================================================================
// The browser session, which is the one credential the old server never had and the one
// this surface does not honour. httpd::granted answers System for a live one, which is
// right under /api/: everything there that changes something is a POST, and a POST on a
// cookie alone is refused for want of the second token. Every legacy route that changes
// something is a GET, no safe method is ever asked for that token, and the cookie is
// written Path=/ SameSite=Lax. Measured on the box before this was written: GET
// /control/setmode?radio with a live session cookie and no second token answered 200
// and the box really switched.
//=============================================================================

TEST_CASE("a live browser session does not carry a caller past the legacy levels", "[compat][levels]")
{
	Policy policy;
	const std::string token = httpd::openSession("root");
	REQUIRE(!token.empty());

	Credentials c = fromPeer("192.168.1.9");
	c.cookie_token = token;

	// The premise, stated rather than assumed: without this line the case
	// below would pass just as well on a box where a session grants nothing
	// anywhere, and would then be holding nothing.
	REQUIRE(granted(c) == AuthLevel::System);
	REQUIRE(httpd::sessionIsLive(token));

	// What the address alone earns is still earned: this caller is in the
	// private range, and taking the session off leaves them a reader.
	REQUIRE(legacyAllows("getmode", "", granted(c), c));

	// And nothing above it.
	REQUIRE_FALSE(legacyAllows("setmode", "radio", granted(c), c));
	REQUIRE_FALSE(legacyAllows("standby", "on",    granted(c), c));
	REQUIRE_FALSE(legacyAllows("shutdown", "",     granted(c), c));
	REQUIRE_FALSE(legacyAllows("exec", "",         granted(c), c));
	REQUIRE_FALSE(legacyAllows("xmltvlist", "",    granted(c), c));

	REQUIRE(httpd::closeSession(token));
}

TEST_CASE("taking the session off can only take away, never give", "[compat][levels]")
{
	/* Written as a floor and not as an assignment (levels.cpp): whatever else
	   raised or lowered this caller, a request that also carries a cookie must
	   not come out of the gate holding more than it would have held without
	   one. An assignment would read the address again here and hand back the
	   Read this caller was not given. */
	Policy policy;
	const std::string token = httpd::openSession("root");
	REQUIRE(!token.empty());

	Credentials c = fromPeer("192.168.1.9");
	c.cookie_token = token;

	REQUIRE_FALSE(legacyAllows("getmode", "", AuthLevel::Public, c));

	REQUIRE(httpd::closeSession(token));
}

TEST_CASE("a session grants nothing to a caller the address grants nothing to", "[compat][levels]")
{
	// Not the private range and not the loopback: what is left after the
	// session comes off is nothing at all, which is where the same request
	// without a cookie would have been.
	Policy policy;
	const std::string token = httpd::openSession("root");
	REQUIRE(!token.empty());

	Credentials c = fromPeer("203.0.113.9");
	c.cookie_token = token;

	REQUIRE(granted(c) == AuthLevel::System);
	REQUIRE_FALSE(legacyAllows("getmode", "", granted(c), c));
	REQUIRE_FALSE(legacyAllows("shutdown", "", granted(c), c));

	REQUIRE(httpd::closeSession(token));
}

TEST_CASE("the two credentials this surface does honour are untouched by the one it does not", "[compat][levels]")
{
	/* The two things that must not break with the session taken off. Loopback
	   with nothing at all is what 22 files in the plugin repo are built on;
	   HTTP Basic is what a second box carries when it syncs timers over this
	   surface (gui/timerlist.cpp:815). Each is asked here WITH a live session
	   cookie beside it, because that is the combination the change touches. */
	Policy policy;
	setLegacyAccount("ni", "ni");
	const std::string token = httpd::openSession("root");
	REQUIRE(!token.empty());

	Credentials loop = fromPeer("127.0.0.1");
	loop.cookie_token = token;
	REQUIRE(legacyAllows("standby", "on", granted(loop), loop));
	REQUIRE(legacyAllows("exec", "",      granted(loop), loop));

	Credentials lan = fromPeer("192.168.1.9");
	lan.cookie_token = token;
	lan.basic = "ni:ni";
	REQUIRE(legacyAllows("standby", "on", granted(lan), lan));
	REQUIRE(legacyAllows("exec", "",      granted(lan), lan));

	// And a wrong password is still a wrong password, cookie or no cookie.
	Credentials wrong = fromPeer("192.168.1.9");
	wrong.cookie_token = token;
	wrong.basic = "ni:falsch";
	REQUIRE_FALSE(legacyAllows("exec", "", granted(wrong), wrong));

	REQUIRE(httpd::closeSession(token));
}

TEST_CASE("a cookie naming no session leaves a caller exactly where it found them", "[compat][levels]")
{
	// A browser holding last week's cookie is a browser holding nothing, and
	// must not be answered any differently from one holding no cookie at all.
	Policy policy;
	Credentials c = fromPeer("192.168.1.9");
	c.cookie_token = "0000000000000000000000000000000000000000000000000000000000000000";

	REQUIRE(granted(c) == AuthLevel::Read);
	REQUIRE(legacyAllows("getmode", "", granted(c), c));
	REQUIRE_FALSE(legacyAllows("shutdown", "", granted(c), c));
}

//=============================================================================
// The levels a POST decides by. Since a body reaches ParamList (compat/
// mount.h), the parameters the level is read off are the query and the body
// together, and the list form below is what the mount point hands this file.
//=============================================================================

TEST_CASE("a level reads the list a handler will read, not the query it came from", "[compat][levels]")
{
	using httpd::compat::CStringList;
	using httpd::compat::parseParams;

	// The same words, once as a query and once as the list a POST body left
	// behind. TimerCGI cannot tell them apart and neither may this.
	CStringList fromBody;
	parseParams("action=remove&id=1", fromBody);
	REQUIRE(levelFor("timer", fromBody) == AuthLevel::Write);
	REQUIRE(levelFor("timer", std::string("action=remove&id=1")) == AuthLevel::Write);

	// An empty list is the empty query's reading, and the reading a request
	// with no "?" at all leaves behind: every Mixed handler's own
	// empty-ParamList branch only reports.
	CStringList none;
	REQUIRE(levelFor("timer", none) == AuthLevel::Read);
	REQUIRE(levelFor("timer", std::string("")) == AuthLevel::Read);

	// And the gate reads the same list.
	const Credentials lan = fromPeer("192.168.1.9");
	REQUIRE_FALSE(legacyAllows("timer", fromBody, granted(lan), lan));
	REQUIRE(legacyAllows("timer", none, granted(lan), lan));
}

#endif // DISABLE_LEGACY_API
