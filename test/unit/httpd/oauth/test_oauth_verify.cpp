/*
 * test_oauth_verify.cpp - tests for the token check the mcp endpoint calls
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
#include "httpd/mcp/contract.h"
#include "httpd/oauth/scopes.h"
#include "httpd/oauth/store.h"
#include "httpd/oauth/verify.h"

#include <string>
#include <vector>

using namespace httpd;
using namespace httpd::oauth;

namespace
{

time_t g_now = 1700000000;
time_t fakeClock()
{
	return g_now;
}

std::string noDigest(const std::string &)
{
	return std::string();
}

const char kOld[] = "https://tv.example.org/mcp";

struct Fixture
{
	Store  s;
	Client c;
	Fixture()
	{
		g_now = 1700000000;
		s.setClock(&fakeClock);
		s.open(std::string());
		REQUIRE(s.registerClient("Claude", std::vector<std::string>(1, "https://claude.ai/api/mcp/auth_callback"), &c));
	}

	std::string access(unsigned scopes, const std::string &resource = kOld)
	{
		Issued out;
		REQUIRE(s.issue(c, "root", scopes, resource, &out, NULL));
		return out.access_token;
	}

	std::string staticToken(unsigned scopes, Client *st = NULL)
	{
		Client made;
		std::string token;
		REQUIRE(s.createStatic("HA", scopes, "root", &made, &token));
		if (st != NULL)
			*st = made;
		return token;
	}
};

void requireRefused(const coreapi::Result<mcp::Caller> &r)
{
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == coreapi::Status::Denied);
	REQUIRE(r.error().code == coreapi::ErrorCode::NotPermitted);
	REQUIRE(r.error().message == "the token is not valid here");
}

} // namespace

TEST_CASE("a static token on the lan names its caller and ignores the resource", "[oauth-verify]")
{
	Fixture f;
	Client st;
	const std::string token = f.staticToken(ScopeWrite, &st);
	const char *resources[] = { "", "https://tv.example.org/mcp", "http://192.168.1.5:8081/mcp" };
	for (size_t i = 0; i < 3; ++i)
	{
		INFO(resources[i]);
		const coreapi::Result<mcp::Caller> r = verifyAccessTokenIn(f.s, token, Origin::Lan, resources[i]);
		REQUIRE(r.ok());
		REQUIRE(r.value().client_id == st.key);
		REQUIRE(r.value().user == "root");
		REQUIRE(r.value().level == AuthLevel::Write);
		REQUIRE_FALSE(r.value().external);
	}
}

TEST_CASE("an access token on the lan is refused", "[oauth-verify]")
{
	Fixture f;
	const std::string token = f.access(ScopeSystem);
	requireRefused(verifyAccessTokenIn(f.s, token, Origin::Lan, kOld));
	requireRefused(verifyAccessTokenIn(f.s, token, Origin::Lan, ""));
}

TEST_CASE("a live access token through the tunnel names its caller", "[oauth-verify]")
{
	Fixture f;
	const std::string token = f.access(ScopeRead | ScopeWrite);
	g_now += 100;
	const coreapi::Result<mcp::Caller> r = verifyAccessTokenIn(f.s, token, Origin::Tunnel, kOld);
	REQUIRE(r.ok());
	REQUIRE(r.value().client_id == f.c.key);
	REQUIRE(r.value().user == "root");
	REQUIRE(r.value().level == AuthLevel::Write);
	REQUIRE(r.value().external);
	const std::vector<Client> list = f.s.listClients();
	REQUIRE(list.size() == 1u);
	REQUIRE(list[0].last_used == g_now);
}

TEST_CASE("a static token through the tunnel is refused", "[oauth-verify]")
{
	Fixture f;
	const std::string token = f.staticToken(ScopeSystem);
	requireRefused(verifyAccessTokenIn(f.s, token, Origin::Tunnel, kOld));
	requireRefused(verifyAccessTokenIn(f.s, token, Origin::Tunnel, ""));
}

TEST_CASE("a token issued for another resource is refused", "[oauth-verify]")
{
	Fixture f;
	const std::string token = f.access(ScopeSystem, "https://other.example/mcp");
	requireRefused(verifyAccessTokenIn(f.s, token, Origin::Tunnel, kOld));
}

TEST_CASE("a token for the old public url is refused after the url changes", "[oauth-verify]")
{
	Fixture f;
	const std::string token = f.access(ScopeRead);
	REQUIRE(verifyAccessTokenIn(f.s, token, Origin::Tunnel, kOld).ok());
	requireRefused(verifyAccessTokenIn(f.s, token, Origin::Tunnel, "https://tv.new.example/mcp"));
}

TEST_CASE("an empty resource accepts no access token", "[oauth-verify]")
{
	Fixture f;
	const std::string token = f.access(ScopeRead, "");
	requireRefused(verifyAccessTokenIn(f.s, token, Origin::Tunnel, ""));
}

TEST_CASE("a refused origin accepts nothing", "[oauth-verify]")
{
	Fixture f;
	requireRefused(verifyAccessTokenIn(f.s, f.access(ScopeRead), Origin::Refused, kOld));
	requireRefused(verifyAccessTokenIn(f.s, f.staticToken(ScopeRead), Origin::Refused, kOld));
	requireRefused(verifyAccessTokenIn(f.s, f.staticToken(ScopeRead), Origin::Refused, ""));
}

TEST_CASE("expired revoked and malformed tokens get one answer", "[oauth-verify]")
{
	Fixture f;
	Issued a;
	Issued b;
	REQUIRE(f.s.issue(f.c, "root", ScopeRead, kOld, &a, NULL));
	REQUIRE(f.s.issue(f.c, "root", ScopeRead | ScopeOffline, kOld, &b, NULL));
	f.s.revokeToken(b.refresh_token, f.c.client_id);
	g_now += 3600;
	const char *inputs[] = { "", "nia_", "nis_", "garbage" };
	std::vector<std::string> all(inputs, inputs + 4);
	all.push_back(a.access_token);
	all.push_back(b.access_token);
	all.push_back(b.refresh_token);
	for (size_t i = 0; i < all.size(); ++i)
	{
		INFO(i);
		requireRefused(verifyAccessTokenIn(f.s, all[i], Origin::Tunnel, kOld));
		requireRefused(verifyAccessTokenIn(f.s, all[i], Origin::Lan, ""));
	}
}

TEST_CASE("a token worth no level is refused", "[oauth-verify]")
{
	Fixture f;
	requireRefused(verifyAccessTokenIn(f.s, f.access(ScopeOffline), Origin::Tunnel, kOld));
}

TEST_CASE("a store that cannot compute a digest answers internal and not denied", "[oauth-verify]")
{
	Fixture f;
	const std::string token = f.access(ScopeRead);
	f.s.setDigestForTest(&noDigest);
	const coreapi::Result<mcp::Caller> r = verifyAccessTokenIn(f.s, token, Origin::Tunnel, kOld);
	f.s.setDigestForTest(NULL);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == coreapi::Status::Internal);
	REQUIRE(r.error().code == coreapi::ErrorCode::BoxUnreadable);
	REQUIRE(verifyAccessTokenIn(f.s, token, Origin::Tunnel, kOld).ok());
}

TEST_CASE("the installed check reads the server's store", "[oauth-verify]")
{
	store().open(std::string());
	Client st;
	std::string token;
	REQUIRE(store().createStatic("HA", ScopeRead, "root", &st, &token));
	REQUIRE(verifyAccessToken(token, Origin::Lan, "").ok());
	REQUIRE_FALSE(verifyAccessToken(token, Origin::Tunnel, kOld).ok());
	const mcp::VerifyToken installed = &verifyAccessToken;
	REQUIRE(installed(token, Origin::Lan, "").ok());
	store().open(std::string());
}
