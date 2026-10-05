/*
 * test_oauth_scopes.cpp - tests for the OAuth scopes
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
#include "httpd/oauth/scopes.h"

#include <string>
#include <vector>

using namespace httpd;
using namespace httpd::oauth;

TEST_CASE("scope names read into bits and unknown names are refused", "[oauth-scopes]")
{
	unsigned bits = 99;
	REQUIRE(parseScopes("read write", &bits));
	REQUIRE(bits == (ScopeRead | ScopeWrite));
	REQUIRE(parseScopes("", &bits));
	REQUIRE(bits == 0u);
	REQUIRE(parseScopes("read  offline_access", &bits));
	REQUIRE(bits == (ScopeRead | ScopeOffline));
	REQUIRE(parseScopes("system read system", &bits));
	REQUIRE(bits == (ScopeRead | ScopeSystem));

	bits = 77;
	REQUIRE_FALSE(parseScopes("read admin", &bits));
	REQUIRE(bits == 77u);
	REQUIRE_FALSE(parseScopes("READ", &bits));
	REQUIRE_FALSE(parseScopes("rea", &bits));
	REQUIRE_FALSE(parseScopes("readx", &bits));
}

TEST_CASE("a level scope brings the levels below it", "[oauth-scopes]")
{
	REQUIRE(withImplied(ScopeSystem) == ScopeLevels);
	REQUIRE(withImplied(ScopeWrite) == (ScopeRead | ScopeWrite));
	REQUIRE(withImplied(ScopeRead) == ScopeRead);
	REQUIRE(withImplied(ScopeOffline) == ScopeOffline);
	REQUIRE(withImplied(ScopeWrite | ScopeOffline) == (ScopeRead | ScopeWrite | ScopeOffline));
}

TEST_CASE("the level a token is worth is its highest level scope", "[oauth-scopes]")
{
	REQUIRE(levelFor(0u) == AuthLevel::Public);
	REQUIRE(levelFor(ScopeOffline) == AuthLevel::Public);
	REQUIRE(levelFor(ScopeRead) == AuthLevel::Read);
	REQUIRE(levelFor(ScopeRead | ScopeWrite) == AuthLevel::Write);
	REQUIRE(levelFor(ScopeWrite) == AuthLevel::Write);
	REQUIRE(levelFor(ScopeLevels | ScopeOffline) == AuthLevel::System);
}

TEST_CASE("scopes are written in one order", "[oauth-scopes]")
{
	REQUIRE(scopeString(ScopeOffline | ScopeSystem | ScopeRead) == "read system offline_access");
	REQUIRE(scopeString(0u) == "");
	const std::vector<std::string> all = scopeNames(ScopeLevels | ScopeOffline);
	REQUIRE(all.size() == 4u);
	REQUIRE(all[3] == "offline_access");
}

TEST_CASE("offline access is offered by the server and not by the resource", "[oauth-scopes]")
{
	const std::vector<std::string> server = serverScopes();
	const std::vector<std::string> resource = resourceScopes();
	REQUIRE(server.size() == 4u);
	REQUIRE(resource.size() == 3u);
	for (size_t i = 0; i < resource.size(); ++i)
		REQUIRE(resource[i] != "offline_access");
	REQUIRE(server[3] == "offline_access");
}
