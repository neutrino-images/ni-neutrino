/*
 * test_oauth_store.cpp - tests for the OAuth store in memory
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
#include "httpd/oauth/store.h"

#include <string>
#include <vector>

using namespace httpd::oauth;

namespace
{

time_t g_now = 1700000000;

time_t fakeClock()
{
	return g_now;
}

const char kResource[] = "https://tv.example.org/mcp";

std::vector<std::string> claudeRedirects()
{
	std::vector<std::string> v;
	v.push_back("https://claude.ai/api/mcp/auth_callback");
	return v;
}

struct Fresh
{
	Store s;
	Fresh()
	{
		g_now = 1700000000;
		s.setClock(&fakeClock);
		s.open(std::string());
	}
};

Client registered(Store &s)
{
	Client c;
	REQUIRE(s.registerClient("Claude", claudeRedirects(), &c));
	return c;
}

} // namespace

TEST_CASE("a registered client is found under its client id", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	REQUIRE(c.client_id == "nid_" + c.key);
	REQUIRE(c.kind == ClientKind::Registered);
	Client back;
	REQUIRE(f.s.findClient(c.client_id, &back));
	REQUIRE(back.name == "Claude");
	REQUIRE(back.redirect_uris == claudeRedirects());
	REQUIRE_FALSE(f.s.findClient("nid_nothing", &back));
}

TEST_CASE("an access token lives one hour and carries its grant", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	Issued out;
	std::string grant;
	REQUIRE(f.s.issue(c, "root", ScopeRead | ScopeWrite, kResource, &out, &grant));
	REQUIRE(out.refresh_token.empty());
	REQUIRE(out.expires_in == 3600);

	TokenFacts t;
	REQUIRE(f.s.checkToken(out.access_token, &t));
	REQUIRE(t.client_id == c.client_id);
	REQUIRE(t.key == c.key);
	REQUIRE(t.user == "root");
	REQUIRE(t.resource == kResource);
	REQUIRE(t.scopes == (ScopeRead | ScopeWrite));
	REQUIRE_FALSE(t.is_static);

	g_now += 3599;
	REQUIRE(f.s.checkToken(out.access_token, &t));
	g_now += 1;
	REQUIRE_FALSE(f.s.checkToken(out.access_token, &t));
}

TEST_CASE("offline access is what brings a refresh token", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	Issued out;
	REQUIRE(f.s.issue(c, "root", ScopeRead | ScopeOffline, kResource, &out, NULL));
	REQUIRE(out.refresh_token.compare(0, 4, "nir_") == 0);
}

TEST_CASE("a refresh rotates and the old refresh token is spent", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	Issued first;
	REQUIRE(f.s.issue(c, "root", ScopeRead | ScopeOffline, kResource, &first, NULL));
	Issued second;
	REQUIRE(f.s.refresh(first.refresh_token, c.client_id, kResource, 0, &second) == RefreshOutcome::Issued);
	REQUIRE(second.refresh_token != first.refresh_token);
	REQUIRE(second.access_token != first.access_token);
	TokenFacts t;
	REQUIRE(f.s.checkToken(second.access_token, &t));
}

TEST_CASE("a reused refresh token kills what its successor issued", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	Issued first;
	REQUIRE(f.s.issue(c, "root", ScopeRead | ScopeOffline, kResource, &first, NULL));
	Issued second;
	REQUIRE(f.s.refresh(first.refresh_token, c.client_id, kResource, 0, &second) == RefreshOutcome::Issued);

	Issued third;
	REQUIRE(f.s.refresh(first.refresh_token, c.client_id, kResource, 0, &third) == RefreshOutcome::Reused);
	TokenFacts t;
	REQUIRE_FALSE(f.s.checkToken(second.access_token, &t));
	REQUIRE_FALSE(f.s.checkToken(first.access_token, &t));
	REQUIRE(f.s.refresh(second.refresh_token, c.client_id, kResource, 0, &third) == RefreshOutcome::Invalid);
	REQUIRE(f.s.grantCountForTest() == 0u);
}

TEST_CASE("a reused refresh token is caught even under a foreign resource", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	Issued first;
	REQUIRE(f.s.issue(c, "root", ScopeRead | ScopeOffline, kResource, &first, NULL));
	Issued second;
	REQUIRE(f.s.refresh(first.refresh_token, c.client_id, kResource, 0, &second) == RefreshOutcome::Issued);

	Issued third;
	REQUIRE(f.s.refresh(first.refresh_token, c.client_id, "https://other.example.org/mcp", 0, &third) ==
	        RefreshOutcome::Reused);
	REQUIRE(f.s.grantCountForTest() == 0u);
	TokenFacts t;
	REQUIRE_FALSE(f.s.checkToken(second.access_token, &t));
}

TEST_CASE("a refresh token is no use to another client", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	const Client other = registered(f.s);
	Issued first;
	REQUIRE(f.s.issue(c, "root", ScopeRead | ScopeOffline, kResource, &first, NULL));
	Issued out;
	REQUIRE(f.s.refresh(first.refresh_token, other.client_id, kResource, 0, &out) == RefreshOutcome::Invalid);
	REQUIRE(f.s.refresh(first.refresh_token, c.client_id, kResource, 0, &out) == RefreshOutcome::Issued);
}

TEST_CASE("a refresh token is no use for a different resource", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	Issued first;
	REQUIRE(f.s.issue(c, "root", ScopeRead | ScopeOffline, kResource, &first, NULL));
	Issued out;
	REQUIRE(f.s.refresh(first.refresh_token, c.client_id, "https://other.example.org/mcp", 0, &out) ==
	        RefreshOutcome::Invalid);
	REQUIRE(f.s.refresh(first.refresh_token, c.client_id, kResource, 0, &out) == RefreshOutcome::Issued);
}

TEST_CASE("a refresh may narrow the scopes and never widen them", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	Issued first;
	REQUIRE(f.s.issue(c, "root", ScopeRead | ScopeWrite | ScopeOffline, kResource, &first, NULL));
	Issued out;
	REQUIRE(f.s.refresh(first.refresh_token, c.client_id, kResource, ScopeSystem, &out) == RefreshOutcome::BadScope);
	REQUIRE(f.s.refresh(first.refresh_token, c.client_id, kResource, ScopeRead, &out) == RefreshOutcome::Issued);
	TokenFacts t;
	REQUIRE(f.s.checkToken(out.access_token, &t));
	REQUIRE(t.scopes == ScopeRead);
}

TEST_CASE("an expired refresh token is refused", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	Issued first;
	REQUIRE(f.s.issue(c, "root", ScopeRead | ScopeOffline, kResource, &first, NULL));
	g_now += kRefreshLifetime;
	Issued out;
	REQUIRE(f.s.refresh(first.refresh_token, c.client_id, kResource, 0, &out) == RefreshOutcome::Invalid);
}

TEST_CASE("a refresh token lives thirty days from the refresh that issued it", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	Issued cur;
	REQUIRE(f.s.issue(c, "root", ScopeRead | ScopeOffline, kResource, &cur, NULL));
	for (int i = 0; i < 3; ++i)
	{
		g_now += kRefreshLifetime - 60;
		Issued next;
		REQUIRE(f.s.refresh(cur.refresh_token, c.client_id, kResource, 0, &next) == RefreshOutcome::Issued);
		cur = next;
	}
	g_now += kRefreshLifetime;
	Issued out;
	REQUIRE(f.s.refresh(cur.refresh_token, c.client_id, kResource, 0, &out) == RefreshOutcome::Invalid);
}

namespace
{

std::string noDigest(const std::string &)
{
	return std::string();
}

} // namespace

TEST_CASE("a token whose digest cannot be computed is a failure and not a refusal", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	Issued out;
	REQUIRE(f.s.issue(c, "root", ScopeRead, kResource, &out, NULL));
	TokenFacts t;
	bool failed = true;
	REQUIRE(f.s.checkToken(out.access_token, &t, &failed));
	REQUIRE_FALSE(failed);
	REQUIRE_FALSE(f.s.checkToken("nia_" + std::string(64, '0'), &t, &failed));
	REQUIRE_FALSE(failed);
	f.s.setDigestForTest(&noDigest);
	REQUIRE_FALSE(f.s.checkToken(out.access_token, &t, &failed));
	REQUIRE(failed);
	f.s.setDigestForTest(NULL);
	REQUIRE(f.s.checkToken(out.access_token, &t, &failed));
	REQUIRE_FALSE(failed);
}

TEST_CASE("revoking a refresh token ends the grant and an access token only itself", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	Issued a;
	Issued b;
	REQUIRE(f.s.issue(c, "root", ScopeRead | ScopeOffline, kResource, &a, NULL));
	REQUIRE(f.s.issue(c, "root", ScopeRead | ScopeOffline, kResource, &b, NULL));
	TokenFacts t;

	f.s.revokeToken(a.access_token, "nid_someone_else");
	REQUIRE(f.s.checkToken(a.access_token, &t));

	f.s.revokeToken(a.access_token, c.client_id);
	REQUIRE_FALSE(f.s.checkToken(a.access_token, &t));
	REQUIRE(f.s.grantCountForTest() == 2u);

	f.s.revokeToken(b.refresh_token, c.client_id);
	REQUIRE_FALSE(f.s.checkToken(b.access_token, &t));
	REQUIRE(f.s.grantCountForTest() == 1u);
}

TEST_CASE("removing a client revokes its tokens and nobody else's", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	const Client other = registered(f.s);
	Issued mine;
	Issued theirs;
	REQUIRE(f.s.issue(c, "root", ScopeRead, kResource, &mine, NULL));
	REQUIRE(f.s.issue(other, "root", ScopeRead, kResource, &theirs, NULL));
	REQUIRE(f.s.removeClient(c.key));
	TokenFacts t;
	REQUIRE_FALSE(f.s.checkToken(mine.access_token, &t));
	REQUIRE(f.s.checkToken(theirs.access_token, &t));
	Client back;
	REQUIRE_FALSE(f.s.findClient(c.client_id, &back));
	REQUIRE_FALSE(f.s.removeClient(c.key));
}

TEST_CASE("a static token works without a resource and is revocable", "[oauth-store]")
{
	Fresh f;
	Client c;
	std::string token;
	REQUIRE(f.s.createStatic("Home Assistant", ScopeWrite, "root", &c, &token));
	REQUIRE(token.compare(0, 4, "nis_") == 0);
	REQUIRE(c.client_id == "static:" + c.key);
	REQUIRE(c.scopes == (ScopeRead | ScopeWrite));
	TokenFacts t;
	REQUIRE(f.s.checkToken(token, &t));
	REQUIRE(t.is_static);
	REQUIRE(t.key == c.key);
	REQUIRE(t.resource.empty());
	REQUIRE(t.scopes == (ScopeRead | ScopeWrite));
	g_now += 400L * 24 * 3600;
	REQUIRE(f.s.checkToken(token, &t));
	REQUIRE(f.s.removeClient(c.key));
	REQUIRE_FALSE(f.s.checkToken(token, &t));
}

TEST_CASE("static tokens stop at their ceiling", "[oauth-store]")
{
	Fresh f;
	Client c;
	std::string token;
	for (size_t i = 0; i < kMaxStatic; ++i)
		REQUIRE(f.s.createStatic("x", ScopeRead, "root", &c, &token));
	REQUIRE_FALSE(f.s.createStatic("x", ScopeRead, "root", &c, &token));
}

TEST_CASE("registration flooding evicts unused registrations and then stops", "[oauth-store]")
{
	Fresh f;
	std::vector<Client> all;
	for (size_t i = 0; i < kMaxRegistered; ++i)
	{
		all.push_back(registered(f.s));
		// Distinct registration times, so the oldest is one client and not a tie.
		g_now += 1;
	}

	Client extra;
	REQUIRE(f.s.registerClient("late", claudeRedirects(), &extra));
	Client back;
	REQUIRE_FALSE(f.s.findClient(all[0].client_id, &back));

	Issued out;
	for (size_t i = 1; i < all.size(); ++i)
		REQUIRE(f.s.issue(all[i], "root", ScopeRead, kResource, &out, NULL));
	REQUIRE(f.s.issue(extra, "root", ScopeRead, kResource, &out, NULL));
	REQUIRE_FALSE(f.s.registerClient("one too many", claudeRedirects(), &back));
}

TEST_CASE("an unused registration is dropped after a day", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	g_now += kUnusedClientLife + 61;
	Client back;
	REQUIRE_FALSE(f.s.findClient(c.client_id, &back));
}

TEST_CASE("a client is kept by its last use and not only by when it registered", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	g_now += 20L * 3600L;
	Issued out;
	REQUIRE(f.s.issue(c, "root", ScopeRead, kResource, &out, NULL));
	TokenFacts t;
	REQUIRE(f.s.checkToken(out.access_token, &t));
	g_now += kAccessLifetime + 1; // the access token dies; without offline_access the grant dies with it
	Client back;
	REQUIRE(f.s.findClient(c.client_id, &back));
	g_now += kUnusedClientLife - kAccessLifetime - 62;
	REQUIRE(f.s.findClient(c.client_id, &back)); // just under a day since the last use
	g_now += 122;
	REQUIRE_FALSE(f.s.findClient(c.client_id, &back)); // just over a day since the last use
}

TEST_CASE("token records per grant stay bounded under refresh spam", "[oauth-store]")
{
	Fresh f;
	const Client c = registered(f.s);
	Issued cur;
	REQUIRE(f.s.issue(c, "root", ScopeRead | ScopeOffline, kResource, &cur, NULL));
	for (int i = 0; i < 50; ++i)
	{
		Issued next;
		REQUIRE(f.s.refresh(cur.refresh_token, c.client_id, kResource, 0, &next) == RefreshOutcome::Issued);
		cur = next;
	}
	// access + rotated refresh + the live refresh
	REQUIRE(f.s.tokenCountForTest() <= kMaxAccessPerGrant + kMaxRotatedPerGrant + 1);
}

TEST_CASE("the list shows clients that hold access and every static token", "[oauth-store]")
{
	Fresh f;
	const Client unused = registered(f.s);
	const Client used = registered(f.s);
	Issued out;
	REQUIRE(f.s.issue(used, "root", ScopeRead | ScopeOffline, kResource, &out, NULL));
	Client meta;
	meta.client_id = "https://claude.ai/oauth/claude-code-client-metadata";
	meta.kind = ClientKind::Metadata;
	meta.name = "Claude Code";
	meta.redirect_uris.push_back("http://localhost/callback");
	REQUIRE(f.s.issue(meta, "root", ScopeSystem, kResource, &out, NULL));
	Client st;
	std::string token;
	REQUIRE(f.s.createStatic("HA", ScopeRead, "root", &st, &token));

	const std::vector<Client> list = f.s.listClients();
	REQUIRE(list.size() == 3u);
	bool saw_used = false;
	bool saw_meta = false;
	bool saw_static = false;
	for (size_t i = 0; i < list.size(); ++i)
	{
		REQUIRE(list[i].client_id != unused.client_id);
		REQUIRE(list[i].token_hash.empty());
		if (list[i].client_id == used.client_id)
		{
			saw_used = true;
			REQUIRE(list[i].scopes == (ScopeRead | ScopeOffline));
		}
		if (list[i].kind == ClientKind::Metadata)
		{
			saw_meta = true;
			REQUIRE(list[i].name == "Claude Code");
		}
		saw_static = saw_static || list[i].kind == ClientKind::Static;
	}
	REQUIRE(saw_used);
	REQUIRE(saw_meta);
	REQUIRE(saw_static);
}

TEST_CASE("a token of the wrong shape is never looked up", "[oauth-store]")
{
	Fresh f;
	TokenFacts t;
	REQUIRE_FALSE(f.s.checkToken("", &t));
	REQUIRE_FALSE(f.s.checkToken("Bearer nia_00", &t));
	REQUIRE_FALSE(f.s.checkToken("nir_" + std::string(64, '0'), &t));
}
