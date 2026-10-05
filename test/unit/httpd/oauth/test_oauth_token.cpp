/*
 * test_oauth_token.cpp - tests for the token and revocation endpoints
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
#include "httpd/oauth/authorize.h"
#include "httpd/oauth/oauthtest.h"
#include "httpd/oauth/scopes.h"
#include "httpd/oauth/store.h"
#include "httpd/oauth/token.h"
#include "httpd/oauth/uri.h"

#include <jsoncpp/json/json.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <time.h>
#include <unistd.h>

using namespace httpd;
using namespace httpd::oauth;

namespace
{

const char kBase[] = "https://tv.example.org";
const char kCallback[] = "https://claude.ai/api/mcp/auth_callback";
const char kVerifier[] = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
const char kChallenge[] = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM";
const char kForm[] = "application/x-www-form-urlencoded";

std::string header(const Response &r, const char *name)
{
	for (size_t i = 0; i < r.headers.size(); ++i)
	{
		if (r.headers[i].first == name)
			return r.headers[i].second;
	}
	return std::string();
}

std::string errorOf(const Response &r)
{
	return parsedJson(r.body)["error"].asString();
}

std::string form(const std::vector<std::pair<std::string, std::string> > &p)
{
	return withQuery("", p).substr(1);
}

typedef std::vector<std::pair<std::string, std::string> > Pairs;

void add(Pairs &p, const char *k, const std::string &v)
{
	p.push_back(std::make_pair(std::string(k), v));
}

std::string resource()
{
	return std::string(kBase) + "/mcp";
}

// The token checks out in the store and is bound to this resource.
bool liveFor(const std::string &token, TokenFacts *facts = NULL)
{
	TokenFacts t;
	const bool live = store().checkToken(token, &t) && t.resource == resource();
	if (facts != NULL)
		*facts = t;
	return live;
}

time_t g_now = 1700000000;

time_t fakeClock()
{
	return g_now;
}

struct StoreClock
{
	StoreClock()
	{
		g_now = 1700000000;
		store().setClock(&fakeClock);
	}
	~StoreClock()
	{
		store().setClock(NULL);
	}
};

struct Fixture
{
	Client client;
	explicit Fixture(const std::string &path = std::string())
	{
		store().open(path);
		forgetAuthorizationStateForTest();
		REQUIRE(store().registerClient("Claude", std::vector<std::string>(1, kCallback), &client));
	}

	std::string code(const char *scope = "read write offline_access",
	                 unsigned granted = ScopeRead | ScopeWrite | ScopeOffline,
	                 const std::string &client_id = std::string(),
	                 const std::string &redirect = kCallback)
	{
		Pairs p;
		add(p, "response_type", "code");
		add(p, "client_id", client_id.empty() ? client.client_id : client_id);
		add(p, "redirect_uri", redirect);
		add(p, "code_challenge", kChallenge);
		add(p, "code_challenge_method", "S256");
		add(p, "scope", scope);
		const Response a = answerAuthorize(form(p), kBase);
		const std::string loc = header(a, "Location");
		const std::string id = loc.substr(loc.find("request=") + 8);
		std::string redirect_out;
		REQUIRE(decideRequest(id, true, granted, "root", &redirect_out) == Decided::Redirect);
		Form back;
		REQUIRE(parseForm(redirect_out.substr(redirect_out.find('?') + 1), &back));
		return back["code"];
	}

	Pairs exchange(const std::string &c) const
	{
		Pairs p;
		add(p, "grant_type", "authorization_code");
		add(p, "code", c);
		add(p, "redirect_uri", kCallback);
		add(p, "code_verifier", kVerifier);
		add(p, "client_id", client.client_id);
		return p;
	}

	Pairs refreshing(const std::string &token) const
	{
		Pairs p;
		add(p, "grant_type", "refresh_token");
		add(p, "refresh_token", token);
		add(p, "client_id", client.client_id);
		return p;
	}
};

long elapsedMs(const struct timespec &a, const struct timespec &b)
{
	return (b.tv_sec - a.tv_sec) * 1000L + (b.tv_nsec - a.tv_nsec) / 1000000L;
}

} // namespace

TEST_CASE("a code is exchanged for tokens", "[oauth-token]")
{
	Fixture f;
	const Response r = answerToken(form(f.exchange(f.code())), kForm, kBase);
	REQUIRE(r.code == 200);
	REQUIRE(header(r, "Cache-Control") == "no-store");
	REQUIRE(header(r, "Pragma") == "no-cache");
	const ::Json::Value v = parsedJson(r.body);
	REQUIRE(v["token_type"].asString() == "Bearer");
	REQUIRE(v["expires_in"].asInt() == 3600);
	REQUIRE(v["scope"].asString() == "read write offline_access");
	REQUIRE(v["refresh_token"].asString().compare(0, 4, "nir_") == 0);
	TokenFacts t;
	REQUIRE(liveFor(v["access_token"].asString(), &t));
	REQUIRE(levelFor(t.scopes) == AuthLevel::Write);
	REQUIRE(t.user == "root");
}

TEST_CASE("a wrong verifier is invalid_grant and burns the code", "[oauth-token]")
{
	Fixture f;
	const std::string c = f.code();
	Pairs wrong = f.exchange(c);
	wrong[3].second = std::string(43, 'x');
	REQUIRE(errorOf(answerToken(form(wrong), kForm, kBase)) == "invalid_grant");
	REQUIRE(errorOf(answerToken(form(f.exchange(c)), kForm, kBase)) == "invalid_grant");
}

TEST_CASE("pkce cannot be skipped or downgraded at the token endpoint", "[oauth-token]")
{
	Fixture f;
	Pairs none = f.exchange(f.code());
	none.erase(none.begin() + 3);
	REQUIRE(errorOf(answerToken(form(none), kForm, kBase)) == "invalid_request");
	Pairs plain = f.exchange(f.code());
	plain[3].second = kChallenge;
	REQUIRE(errorOf(answerToken(form(plain), kForm, kBase)) == "invalid_grant");
}

TEST_CASE("a replayed code revokes what it issued", "[oauth-token]")
{
	Fixture f;
	const std::string c = f.code();
	const Response first = answerToken(form(f.exchange(c)), kForm, kBase);
	REQUIRE(first.code == 200);
	const ::Json::Value v = parsedJson(first.body);
	REQUIRE(liveFor(v["access_token"].asString()));

	const Response again = answerToken(form(f.exchange(c)), kForm, kBase);
	REQUIRE(again.code == 400);
	REQUIRE(errorOf(again) == "invalid_grant");
	REQUIRE_FALSE(liveFor(v["access_token"].asString()));
	REQUIRE(errorOf(answerToken(form(f.refreshing(v["refresh_token"].asString())), kForm, kBase)) == "invalid_grant");
}

TEST_CASE("a code is bound to its client and its redirect", "[oauth-token]")
{
	Fixture f;
	Client other;
	REQUIRE(store().registerClient("Other", std::vector<std::string>(1, kCallback), &other));
	Pairs p = f.exchange(f.code());
	p[4].second = other.client_id;
	REQUIRE(errorOf(answerToken(form(p), kForm, kBase)) == "invalid_grant");
	p = f.exchange(f.code());
	p[2].second = "https://claude.ai/other";
	REQUIRE(errorOf(answerToken(form(p), kForm, kBase)) == "invalid_grant");
}

TEST_CASE("a token for another resource is invalid_target", "[oauth-token]")
{
	Fixture f;
	Pairs p = f.exchange(f.code());
	add(p, "resource", "https://other.example/mcp");
	REQUIRE(errorOf(answerToken(form(p), kForm, kBase)) == "invalid_target");
}

TEST_CASE("the token endpoint takes forms and two grant types only", "[oauth-token]")
{
	Fixture f;
	const std::string body = form(f.exchange(f.code()));
	REQUIRE(errorOf(answerToken(body, "application/json", kBase)) == "invalid_request");
	REQUIRE(errorOf(answerToken(body + "&grant_type=refresh_token", kForm, kBase)) == "invalid_request");
	REQUIRE(errorOf(answerToken("code=x", kForm, kBase)) == "invalid_request");
	REQUIRE(errorOf(answerToken("grant_type=password&username=root&password=ni", kForm, kBase)) ==
	        "unsupported_grant_type");
	REQUIRE(errorOf(answerToken("grant_type=client_credentials", kForm, kBase)) == "unsupported_grant_type");

	const std::string over = form(f.exchange(f.code())) + "&pad=" + std::string(kMaxTokenRequestBytes, 'a');
	REQUIRE(over.size() > kMaxTokenRequestBytes);
	REQUIRE(errorOf(answerToken(over, kForm, kBase)) == "invalid_request");

	const std::string base = form(f.exchange(f.code()));
	const std::string under = base + "&pad=" + std::string(kMaxTokenRequestBytes - base.size() - 6, 'a');
	REQUIRE(under.size() <= kMaxTokenRequestBytes);
	REQUIRE(answerToken(under, kForm, kBase).code == 200);
}

TEST_CASE("refresh rotates and a reused refresh token is invalid_grant everywhere", "[oauth-token]")
{
	Fixture f;
	const ::Json::Value first = parsedJson(answerToken(form(f.exchange(f.code())), kForm, kBase).body);
	const Response r = answerToken(form(f.refreshing(first["refresh_token"].asString())), kForm, kBase);
	REQUIRE(r.code == 200);
	const ::Json::Value second = parsedJson(r.body);
	REQUIRE(second["refresh_token"].asString() != first["refresh_token"].asString());
	REQUIRE(liveFor(second["access_token"].asString()));

	REQUIRE(errorOf(answerToken(form(f.refreshing(first["refresh_token"].asString())), kForm, kBase)) == "invalid_grant");
	REQUIRE_FALSE(liveFor(second["access_token"].asString()));
	REQUIRE(errorOf(answerToken(form(f.refreshing(second["refresh_token"].asString())), kForm, kBase)) == "invalid_grant");
}

TEST_CASE("a refresh for a different base than the grant is invalid_grant", "[oauth-token]")
{
	Fixture f;
	const ::Json::Value first = parsedJson(answerToken(form(f.exchange(f.code())), kForm, kBase).body);
	REQUIRE(errorOf(answerToken(form(f.refreshing(first["refresh_token"].asString())), kForm,
	                            "https://other.example.org")) == "invalid_grant");
	REQUIRE(liveFor(first["access_token"].asString()));
}

TEST_CASE("a refresh may narrow the scope and never widen it", "[oauth-token]")
{
	Fixture f;
	const ::Json::Value first = parsedJson(answerToken(form(f.exchange(f.code())), kForm, kBase).body);
	Pairs wide = f.refreshing(first["refresh_token"].asString());
	add(wide, "scope", "system");
	REQUIRE(errorOf(answerToken(form(wide), kForm, kBase)) == "invalid_scope");
	Pairs narrow = f.refreshing(first["refresh_token"].asString());
	add(narrow, "scope", "read");
	const Response r = answerToken(form(narrow), kForm, kBase);
	REQUIRE(r.code == 200);
	REQUIRE(parsedJson(r.body)["scope"].asString() == "read");
}

TEST_CASE("a refresh token lives thirty days from its last use", "[oauth-token]")
{
	StoreClock clock;
	Fixture f;
	std::string refresh = parsedJson(answerToken(form(f.exchange(f.code())), kForm, kBase).body)["refresh_token"].asString();
	for (int i = 0; i < 3; ++i)
	{
		g_now += kRefreshLifetime - 60;
		const Response r = answerToken(form(f.refreshing(refresh)), kForm, kBase);
		REQUIRE(r.code == 200);
		refresh = parsedJson(r.body)["refresh_token"].asString();
	}
	g_now += kRefreshLifetime;
	REQUIRE(errorOf(answerToken(form(f.refreshing(refresh)), kForm, kBase)) == "invalid_grant");
}

TEST_CASE("a removed registered client is invalid_client", "[oauth-token]")
{
	Fixture f;
	const std::string c = f.code();
	REQUIRE(store().removeClient(f.client.key));
	const Response r = answerToken(form(f.exchange(c)), kForm, kBase);
	REQUIRE(r.code == 401);
	REQUIRE(errorOf(r) == "invalid_client");
}

TEST_CASE("a metadata client's code becomes a grant under its url", "[oauth-token]")
{
	Fixture f;
	const std::string id = "https://claude.ai/oauth/claude-code-client-metadata";
	FakeFetcher fetch;
	fetch.body = claudeCodeDocument(id);
	FetcherInstalled in(&fetch);
	const std::string c = f.code("read", ScopeRead, id, "http://localhost:4711/callback");
	Pairs p;
	add(p, "grant_type", "authorization_code");
	add(p, "code", c);
	add(p, "redirect_uri", "http://localhost:4711/callback");
	add(p, "code_verifier", kVerifier);
	add(p, "client_id", id);
	const Response r = answerToken(form(p), kForm, kBase);
	REQUIRE(r.code == 200);
	const std::vector<Client> list = store().listClients();
	bool found = false;
	for (size_t i = 0; i < list.size(); ++i)
		found = found || (list[i].client_id == id && list[i].kind == ClientKind::Metadata);
	REQUIRE(found);
}

TEST_CASE("a public metadata client's assertion is ignored on exchange and refresh", "[oauth-token]")
{
	Fixture f;
	const std::string ret = "https://chatgpt.com/connector_platform_oauth_redirect";
	FakeFetcher fetch;
	fetch.body = kChatGptDocument;
	FetcherInstalled in(&fetch);
	const std::string c = f.code("read offline_access", ScopeRead | ScopeOffline, kChatGptId, ret);
	const std::string assertion = "eyJhbGciOiJSUzI1NiJ9." + std::string(900, 'p') + "." + std::string(342, 's');
	Pairs p;
	add(p, "grant_type", "authorization_code");
	add(p, "code", c);
	add(p, "redirect_uri", ret);
	add(p, "code_verifier", kVerifier);
	add(p, "client_id", kChatGptId);
	add(p, "client_assertion_type", "urn:ietf:params:oauth:client-assertion-type:jwt-bearer");
	add(p, "client_assertion", assertion);
	const Response r = answerToken(form(p), kForm, kBase);
	REQUIRE(r.code == 200);
	const ::Json::Value v = parsedJson(r.body);
	REQUIRE(liveFor(v["access_token"].asString()));

	Pairs q;
	add(q, "grant_type", "refresh_token");
	add(q, "refresh_token", v["refresh_token"].asString());
	add(q, "client_id", kChatGptId);
	add(q, "client_assertion_type", "urn:ietf:params:oauth:client-assertion-type:jwt-bearer");
	add(q, "client_assertion", assertion);
	const Response again = answerToken(form(q), kForm, kBase);
	REQUIRE(again.code == 200);
	REQUIRE(liveFor(parsedJson(again.body)["access_token"].asString()));
}

TEST_CASE("an assertion does not stand in for the client_id", "[oauth-token]")
{
	Fixture f;
	const std::string ret = "https://chatgpt.com/connector_platform_oauth_redirect";
	FakeFetcher fetch;
	fetch.body = kChatGptDocument;
	FetcherInstalled in(&fetch);
	const std::string c = f.code("read", ScopeRead, kChatGptId, ret);
	Pairs p;
	add(p, "grant_type", "authorization_code");
	add(p, "code", c);
	add(p, "redirect_uri", ret);
	add(p, "code_verifier", kVerifier);
	add(p, "client_assertion_type", "urn:ietf:params:oauth:client-assertion-type:jwt-bearer");
	add(p, "client_assertion", "eyJhbGciOiJSUzI1NiJ9.e30.c2ln");
	const Response r = answerToken(form(p), kForm, kBase);
	REQUIRE(r.code == 400);
	REQUIRE(errorOf(r) == "invalid_request");
}

TEST_CASE("revocation answers 200 for anything and ends a grant", "[oauth-token]")
{
	Fixture f;
	const ::Json::Value v = parsedJson(answerToken(form(f.exchange(f.code())), kForm, kBase).body);
	Pairs p;
	add(p, "token", v["refresh_token"].asString());
	add(p, "token_type_hint", "refresh_token");
	add(p, "client_id", f.client.client_id);
	REQUIRE(answerRevoke(form(p), kForm).code == 200);
	REQUIRE_FALSE(liveFor(v["access_token"].asString()));
	REQUIRE(answerRevoke("token=nothing&client_id=x", kForm).code == 200);
	REQUIRE(answerRevoke(form(p), "application/json").code == 400);
	REQUIRE(answerRevoke("token=nothing", kForm).code == 400);
}

TEST_CASE("the token endpoint answers well within the clients' timeouts", "[oauth-token]")
{
	const std::string path = oauthTempPath("timing");
	{
		Fixture f(path);
		const std::string c = f.code();
		struct timespec a;
		struct timespec b;
		clock_gettime(CLOCK_MONOTONIC, &a);
		const Response r = answerToken(form(f.exchange(c)), kForm, kBase);
		clock_gettime(CLOCK_MONOTONIC, &b);
		REQUIRE(r.code == 200);
		REQUIRE(elapsedMs(a, b) < 1000);
		clock_gettime(CLOCK_MONOTONIC, &a);
		const Response rr = answerToken(form(f.refreshing(parsedJson(r.body)["refresh_token"].asString())), kForm, kBase);
		clock_gettime(CLOCK_MONOTONIC, &b);
		REQUIRE(rr.code == 200);
		REQUIRE(elapsedMs(a, b) < 1000);
	}
	store().open(std::string());
	::unlink(path.c_str());
}
