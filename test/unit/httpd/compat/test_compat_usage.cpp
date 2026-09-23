/*
 * test_compat_usage.cpp - tests for which legacy endpoints are still used
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

/* Whether this build carries the copied control API at all. Read here for
   the same reason test_openapi.cpp reads it for DISABLE_API_DOC: a case that
   exercises a layer a build option takes out has to not exist itself in that
   build, rather than run and skip itself, which is a case that can never go
   red. */
#include <config.h>

#ifndef DISABLE_LEGACY_API

#include "httpd/compat/levels.h"
#include "httpd/compat/mount.h"

#include "httpd/auth.h"
#include "httpd/http.h"

#include <cstddef>
#include <string>
#include <vector>

using httpd::AuthLevel;
using httpd::Credentials;
using httpd::Get;
using httpd::Head;
using httpd::Method;
using httpd::Response;

namespace
{

// The same split server.cpp's own callback makes: path and query off the raw target,
// both still percent-encoded, the way a caller off the wire would have sent them. A
// loopback peer and AuthLevel::Read, so every name this file asks for reaches exactly
// as far as answer() itself takes it, without either outcome depending on who is
// asking.
Response call(const std::string &target, Method m)
{
	std::string path = target;
	std::string query;
	const std::string::size_type q = target.find('?');
	if (q != std::string::npos)
	{
		path = target.substr(0, q);
		query = target.substr(q + 1);
	}

	Credentials cred;
	cred.peer = "127.0.0.1";
	return httpd::compat::answer(m, path, query, "", "", "", cred, AuthLevel::Read);
}

void get(const std::string &target)
{
	call(target, Get);
}

void head(const std::string &target)
{
	call(target, Head);
}

// How many times usage() reports name was asked for, known or not: both
// live in the one list answer() builds, so one reader serves a known name
// and an unknown one alike.
unsigned long usageFor(const std::string &name)
{
	const std::vector<httpd::compat::Usage> all = httpd::compat::usage();
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].name == name)
			return all[i].calls;
	}
	return 0;
}

unsigned long usageOfUnknown(const std::string &name)
{
	return usageFor(name);
}

// Everything usage() carries past the known table's own names, which is
// exactly the count of distinct names outside it this layer currently
// remembers.
size_t unknownEntryCount()
{
	return httpd::compat::usage().size() - httpd::compat::endpointCount();
}

} // namespace

TEST_CASE("a legacy call is counted", "[compat][usage]")
{
	// HEAD on purpose: Execute returns before the handler, so this runs
	// on a host with no Neutrino behind it and still counts the call.
	const unsigned long before = usageFor("getmode");
	head("/control/getmode");
	REQUIRE(usageFor("getmode") == before + 1);
}

TEST_CASE("a name the table does not know is counted too", "[compat][usage]")
{
	get("/control/gibtesnicht");
	REQUIRE(usageOfUnknown("gibtesnicht") >= 1);
}

// legacyRequest() lowercases the filename before this layer ever asks levels.h or
// countCall() about it, the same way controlapi.cpp's own Execute compares names. A
// caller spelling a known name in a different case is still that name: uncounted here,
// it would grow the unknown-name table with case variants of a name this layer already
// knows.
TEST_CASE("a known name in a different case is counted as itself, not as a new unknown one", "[compat][usage]")
{
	const unsigned long before = usageFor("getmode");
	const size_t unknownBefore = unknownEntryCount();

	get("/control/GETMODE");

	REQUIRE(usageFor("getmode") == before + 1);
	REQUIRE(unknownEntryCount() == unknownBefore);
}

// usage() answers for the demand a name has seen, not for what was let through. Every
// other case in this file calls through 127.0.0.1, which legacyAllows never refuses, so
// none of them can tell a counted-and-allowed call apart from a counted-and-refused
// one. A LAN peer with nothing granted on a System name still has to leave a true count
// behind, which is the premise the legacy-usage endpoint rests a shutdown decision on.
TEST_CASE("a name is counted even when the caller is refused", "[compat][usage]")
{
	Credentials cred;
	cred.peer = "192.168.1.9";
	const unsigned long before = usageFor("shutdown");

	const Response r = httpd::compat::answer(Get, "/control/shutdown", "", "", "", "",
	                                         cred, AuthLevel::Public);

	REQUIRE(r.code == httpd::StatusForbidden);
	REQUIRE(usageFor("shutdown") == before + 1);
}

TEST_CASE("unknown names cannot grow without bound", "[compat][usage]")
{
	const size_t kUnknownCeiling = httpd::compat::kMaxUnknownLegacyNames;

	for (int i = 0; i < 5000; ++i)
		get("/control/x" + std::to_string(i));
	REQUIRE(unknownEntryCount() <= kUnknownCeiling);
}

#endif // DISABLE_LEGACY_API
