/*
 * test_oauth_authorize.cpp - tests for the authorization endpoint and the consent decision
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
#include "httpd/oauth/surface.h"
#include "httpd/oauth/uri.h"

#include <string>
#include <vector>

using namespace httpd;
using namespace httpd::oauth;

namespace
{

const char kBase[] = "https://tv.example.org";
const char kCallback[] = "https://claude.ai/api/mcp/auth_callback";
const char kChallenge[] = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM";

time_t g_now = 1700000000;

time_t fakeClock()
{
	return g_now;
}

struct Fixture
{
	Client client;

	Fixture()
	{
		g_now = 1700000000;
		store().open(std::string());
		store().setClock(&fakeClock);
		forgetAuthorizationStateForTest();
		setAuthorizeClockForTest(&fakeClock);
		std::vector<std::string> r;
		r.push_back(kCallback);
		r.push_back("http://localhost/callback");
		REQUIRE(store().registerClient("Claude", r, &client));
	}

	~Fixture()
	{
		setAuthorizeClockForTest(NULL);
		store().setClock(NULL);
	}

	Form ask() const
	{
		Form f;
		f["response_type"] = "code";
		f["client_id"] = client.client_id;
		f["redirect_uri"] = kCallback;
		f["code_challenge"] = kChallenge;
		f["code_challenge_method"] = "S256";
		f["state"] = "xyz";
		f["scope"] = "read write";
		f["resource"] = std::string(kBase) + "/mcp";
		return f;
	}
};

std::string query(const Form &f)
{
	std::string out;
	for (Form::const_iterator it = f.begin(); it != f.end(); ++it)
	{
		if (!out.empty())
			out += '&';
		out += formEncode(it->first) + "=" + formEncode(it->second);
	}
	return out;
}

std::string location(const Response &r)
{
	for (size_t i = 0; i < r.headers.size(); ++i)
	{
		if (r.headers[i].first == "Location")
			return r.headers[i].second;
	}
	return std::string();
}

Form paramsOf(const std::string &url)
{
	Form f;
	const size_t q = url.find('?');
	REQUIRE(q != std::string::npos);
	REQUIRE(parseForm(url.substr(q + 1), &f));
	return f;
}

std::string pendingId(const Response &r)
{
	const std::string loc = location(r);
	const std::string head = std::string(kBase) + "/oauth/consent?request=";
	REQUIRE(loc.compare(0, head.size(), head) == 0);
	return loc.substr(head.size());
}

} // namespace

TEST_CASE("a valid request is held and the browser is sent to the consent page", "[oauth-authorize]")
{
	Fixture f;
	const Response r = answerAuthorize(query(f.ask()), kBase);
	REQUIRE(r.code == kStatusFound);
	const std::string id = pendingId(r);
	REQUIRE(id.find('.') != std::string::npos);
	PendingView v;
	REQUIRE(viewRequest(id, &v));
	REQUIRE(v.client_name == "Claude");
	REQUIRE(v.kind == ClientKind::Registered);
	REQUIRE(v.redirect_host == "claude.ai");
	REQUIRE_FALSE(v.loopback_only);
	REQUIRE(v.requested == (ScopeRead | ScopeWrite));
	REQUIRE(v.form_token.size() == 32u);
	REQUIRE(v.base == kBase);
}

TEST_CASE("an unknown client or an unregistered redirect never redirects", "[oauth-authorize]")
{
	Fixture f;
	const char *redirects[] = {
		"https://evil.example/cb",
		"https://claude.ai.evil.example/api/mcp/auth_callback",
		"https://claude.ai/api/mcp/auth_callback?next=https://evil.example",
		"http://localhost:5/elsewhere",
		"",
	};
	for (size_t i = 0; i < sizeof(redirects) / sizeof(redirects[0]); ++i)
	{
		INFO(redirects[i]);
		Form a = f.ask();
		a["redirect_uri"] = redirects[i];
		const Response r = answerAuthorize(query(a), kBase);
		REQUIRE(r.code == StatusBadRequest);
		REQUIRE(location(r).empty());
	}
	Form unknown = f.ask();
	unknown["client_id"] = "nid_00000000000000000000000000000000";
	REQUIRE(answerAuthorize(query(unknown), kBase).code == StatusBadRequest);
	REQUIRE(location(answerAuthorize(query(f.ask()) + "&client_id=x", kBase)).empty());
}

TEST_CASE("a client that vanished from the store is refused like an unknown one", "[oauth-authorize]")
{
	Fixture f;
	REQUIRE(store().removeClient(f.client.key));
	const Response r = answerAuthorize(query(f.ask()), kBase);
	REQUIRE(r.code == StatusBadRequest);
	REQUIRE(location(r).empty());
}

TEST_CASE("a request without s256 pkce is sent back with invalid_request", "[oauth-authorize]")
{
	Fixture f;
	for (int i = 0; i < 4; ++i)
	{
		INFO(i);
		Form a = f.ask();
		if (i == 0)
			a.erase("code_challenge");
		if (i == 1)
			a["code_challenge_method"] = "plain";
		if (i == 2)
			a.erase("code_challenge_method");
		if (i == 3)
			a["code_challenge"] = "short";
		const Response r = answerAuthorize(query(a), kBase);
		REQUIRE(r.code == kStatusFound);
		const std::string loc = location(r);
		REQUIRE(loc.compare(0, std::string(kCallback).size(), kCallback) == 0);
		Form p = paramsOf(loc);
		REQUIRE(p["error"] == "invalid_request");
		REQUIRE(p["state"] == "xyz");
		REQUIRE(p["iss"] == kBase);
		REQUIRE(p.count("code") == 0u);
	}
}

TEST_CASE("unsupported response types scopes and resources are sent back", "[oauth-authorize]")
{
	Fixture f;
	Form a = f.ask();
	a["response_type"] = "token";
	REQUIRE(paramsOf(location(answerAuthorize(query(a), kBase)))["error"] == "unsupported_response_type");
	a = f.ask();
	a["scope"] = "read admin";
	REQUIRE(paramsOf(location(answerAuthorize(query(a), kBase)))["error"] == "invalid_scope");
	a = f.ask();
	a["resource"] = "https://other.example/mcp";
	REQUIRE(paramsOf(location(answerAuthorize(query(a), kBase)))["error"] == "invalid_target");
	a = f.ask();
	a["state"] = std::string(kMaxStateBytes + 1, 's');
	REQUIRE(paramsOf(location(answerAuthorize(query(a), kBase)))["error"] == "invalid_request");
}

TEST_CASE("a loopback redirect is accepted on any port", "[oauth-authorize]")
{
	Fixture f;
	Form a = f.ask();
	a["redirect_uri"] = "http://localhost:53682/callback";
	const Response r = answerAuthorize(query(a), kBase);
	PendingView v;
	REQUIRE(viewRequest(pendingId(r), &v));
	REQUIRE(v.redirect_uri == "http://localhost:53682/callback");
}

TEST_CASE("a client known by its metadata document is asked about", "[oauth-authorize]")
{
	Fixture f;
	const std::string id = "https://claude.ai/oauth/claude-code-client-metadata";
	FakeFetcher fetch;
	fetch.body = claudeCodeDocument(id);
	FetcherInstalled in(&fetch);
	Form a = f.ask();
	a["client_id"] = id;
	a["redirect_uri"] = "http://127.0.0.1:40001/callback";
	PendingView v;
	REQUIRE(viewRequest(pendingId(answerAuthorize(query(a), kBase)), &v));
	REQUIRE(v.kind == ClientKind::Metadata);
	REQUIRE(v.client_name == "Claude Code");
	REQUIRE(v.loopback_only);
}

TEST_CASE("chatgpt and claude are asked about under their names and exact return addresses", "[oauth-authorize]")
{
	Fixture f;
	const char *ids[] = { kChatGptId, kClaudeId };
	const char *docs[] = { kChatGptDocument, kClaudeDocument };
	const char *names[] = { "ChatGPT", "Claude" };
	const char *hosts[] = { "chatgpt.com", "claude.ai" };
	const char *returns[] = { "https://chatgpt.com/connector_platform_oauth_redirect",
	                          "https://claude.ai/api/mcp/auth_callback" };
	for (size_t i = 0; i < 2; ++i)
	{
		INFO(ids[i]);
		FakeFetcher fetch;
		fetch.body = docs[i];
		FetcherInstalled in(&fetch);
		Form a = f.ask();
		a["client_id"] = ids[i];
		a["redirect_uri"] = returns[i];
		PendingView v;
		REQUIRE(viewRequest(pendingId(answerAuthorize(query(a), kBase)), &v));
		REQUIRE(v.kind == ClientKind::Metadata);
		REQUIRE(v.client_name == names[i]);
		REQUIRE(v.redirect_host == hosts[i]);
		REQUIRE(v.redirect_uri == returns[i]);
		REQUIRE_FALSE(v.loopback_only);

		const std::string r = returns[i];
		const std::string near[] = {
			r + "/", r + "?x=1", r + "#f", r.substr(0, r.size() - 1),
			"http" + r.substr(5), "https://evil.example" + r.substr(r.find('/', 8)),
			"https://" + std::string(hosts[i]) + ".evil.example" + r.substr(r.find('/', 8)),
		};
		for (size_t k = 0; k < sizeof(near) / sizeof(near[0]); ++k)
		{
			INFO(near[k]);
			Form b = f.ask();
			b["client_id"] = ids[i];
			b["redirect_uri"] = near[k];
			const Response refused = answerAuthorize(query(b), kBase);
			REQUIRE(refused.code == StatusBadRequest);
			REQUIRE(location(refused).empty());
		}
	}
}

TEST_CASE("approving issues a code and denying an access_denied error", "[oauth-authorize]")
{
	Fixture f;
	const std::string a = pendingId(answerAuthorize(query(f.ask()), kBase));
	std::string redirect;
	REQUIRE(decideRequest(a, true, ScopeRead | ScopeWrite, "root", &redirect) == Decided::Redirect);
	Form p = paramsOf(redirect);
	REQUIRE(redirect.compare(0, std::string(kCallback).size(), kCallback) == 0);
	REQUIRE(p["code"].compare(0, 4, "nic_") == 0);
	REQUIRE(p["state"] == "xyz");
	REQUIRE(p["iss"] == kBase);

	const std::string b = pendingId(answerAuthorize(query(f.ask()), kBase));
	REQUIRE(decideRequest(b, false, 0, "root", &redirect) == Decided::Redirect);
	p = paramsOf(redirect);
	REQUIRE(p["error"] == "access_denied");
	REQUIRE(p["iss"] == kBase);
	REQUIRE(p.count("code") == 0u);
}

TEST_CASE("a consumed request cannot be decided again", "[oauth-authorize]")
{
	Fixture f;
	const std::string id = pendingId(answerAuthorize(query(f.ask()), kBase));
	std::string redirect;
	REQUIRE(decideRequest(id, true, ScopeRead, "root", &redirect) == Decided::Redirect);
	REQUIRE(decideRequest(id, true, ScopeRead, "root", &redirect) == Decided::NoSuchRequest);
	REQUIRE(decideRequest(id, false, 0, "root", &redirect) == Decided::NoSuchRequest);
	PendingView v;
	REQUIRE_FALSE(viewRequest(id, &v));
}

TEST_CASE("a grant may raise the level and add staying signed in, but needs a level", "[oauth-authorize]")
{
	Fixture f;
	Form a = f.ask();
	a["scope"] = "read";
	const std::string id = pendingId(answerAuthorize(query(a), kBase));
	std::string redirect;
	REQUIRE(decideRequest(id, true, ScopeOffline, "root", &redirect) == Decided::BadScopes);
	REQUIRE(decideRequest(id, true, 0, "root", &redirect) == Decided::BadScopes);
	PendingView v;
	REQUIRE(viewRequest(id, &v));
	REQUIRE(decideRequest(id, true, ScopeSystem | ScopeOffline, "root", &redirect) == Decided::Redirect);
}

TEST_CASE("a code redeems once and a replay names its grant", "[oauth-authorize]")
{
	Fixture f;
	const std::string id = pendingId(answerAuthorize(query(f.ask()), kBase));
	std::string redirect;
	REQUIRE(decideRequest(id, true, ScopeRead | ScopeWrite, "root", &redirect) == Decided::Redirect);
	const std::string code = paramsOf(redirect)["code"];
	CodeGrant g;
	std::string replayed;
	REQUIRE(redeemCode(code, &g, &replayed) == Redeem::Fresh);
	REQUIRE(g.client_id == f.client.client_id);
	REQUIRE(g.redirect_uri == kCallback);
	REQUIRE(g.challenge == kChallenge);
	REQUIRE(g.user == "root");
	REQUIRE(g.scopes == (ScopeRead | ScopeWrite));
	REQUIRE(g.resource == std::string(kBase) + "/mcp");
	bindCodeToGrant(code, "g1");
	REQUIRE(redeemCode(code, &g, &replayed) == Redeem::Replayed);
	REQUIRE(replayed == "g1");
	REQUIRE(redeemCode("nic_" + std::string(64, '0'), &g, &replayed) == Redeem::Unknown);
}

TEST_CASE("a code expires after a minute", "[oauth-authorize]")
{
	Fixture f;
	const std::string id = pendingId(answerAuthorize(query(f.ask()), kBase));
	std::string redirect;
	REQUIRE(decideRequest(id, true, ScopeRead, "root", &redirect) == Decided::Redirect);
	g_now += kCodeLifetime;
	CodeGrant g;
	std::string replayed;
	REQUIRE(redeemCode(paramsOf(redirect)["code"], &g, &replayed) == Redeem::Unknown);
}

TEST_CASE("a pending request is gone after ten minutes", "[oauth-authorize]")
{
	Fixture f;
	const std::string id = pendingId(answerAuthorize(query(f.ask()), kBase));
	g_now += kPendingLifetime;
	PendingView v;
	REQUIRE_FALSE(viewRequest(id, &v));
	std::string redirect;
	REQUIRE(decideRequest(id, true, ScopeRead, "root", &redirect) == Decided::NoSuchRequest);
}

TEST_CASE("a code from before a restart is unknown", "[oauth-authorize]")
{
	Fixture f;
	const std::string id = pendingId(answerAuthorize(query(f.ask()), kBase));
	std::string redirect;
	REQUIRE(decideRequest(id, true, ScopeRead, "root", &redirect) == Decided::Redirect);
	forgetAuthorizationStateForTest();
	CodeGrant g;
	std::string replayed;
	REQUIRE(redeemCode(paramsOf(redirect)["code"], &g, &replayed) == Redeem::Unknown);
}

TEST_CASE("the state comes back exactly as it was sent", "[oauth-authorize]")
{
	Fixture f;
	Form a = f.ask();
	a["state"] = "a b&c=d/%";
	const std::string id = pendingId(answerAuthorize(query(a), kBase));
	std::string redirect;
	REQUIRE(decideRequest(id, false, 0, "root", &redirect) == Decided::Redirect);
	REQUIRE(paramsOf(redirect)["state"] == "a b&c=d/%");
}

TEST_CASE("a flood of authorize requests leaves an open one standing", "[oauth-authorize]")
{
	Fixture f;
	const std::string id = pendingId(answerAuthorize(query(f.ask()), kBase));
	for (int i = 0; i < 200; ++i)
		REQUIRE(answerAuthorize(query(f.ask()), kBase).code == kStatusFound);
	PendingView v;
	REQUIRE(viewRequest(id, &v));
	std::string redirect;
	REQUIRE(decideRequest(id, true, ScopeRead, "root", &redirect) == Decided::Redirect);
}

TEST_CASE("a request changed anywhere is refused", "[oauth-authorize]")
{
	Fixture f;
	const std::string id = pendingId(answerAuthorize(query(f.ask()), kBase));
	const size_t at[] = { 0, id.size() / 3, id.find('.') + 1, id.size() - 1 };
	for (size_t i = 0; i < sizeof(at) / sizeof(at[0]); ++i)
	{
		INFO(at[i]);
		std::string changed = id;
		changed[at[i]] = (changed[at[i]] == 'A') ? 'B' : 'A';
		PendingView v;
		CHECK_FALSE(viewRequest(changed, &v));
		std::string redirect;
		CHECK(decideRequest(changed, true, ScopeRead, "root", &redirect) == Decided::NoSuchRequest);
	}
	PendingView v;
	CHECK_FALSE(viewRequest(id + "x", &v));
	CHECK_FALSE(viewRequest(id.substr(0, id.find('.')), &v));
	CHECK_FALSE(viewRequest(std::string(), &v));
}

TEST_CASE("an open request outlives a restart of the store", "[oauth-authorize]")
{
	Fixture f;
	const std::string path = oauthTempPath("authorize");
	REQUIRE(store().open(path));
	std::vector<std::string> r(1, kCallback);
	Client c;
	REQUIRE(store().registerClient("Claude", r, &c));
	Form a = f.ask();
	a["client_id"] = c.client_id;
	const std::string id = pendingId(answerAuthorize(query(a), kBase));
	REQUIRE(store().open(path));
	PendingView v;
	CHECK(viewRequest(id, &v));
	CHECK(v.client_name == "Claude");
	store().open(std::string());
	::unlink(path.c_str());
	CHECK_FALSE(viewRequest(id, &v));
}

TEST_CASE("an open request does not outlive a restart of the server", "[oauth-authorize]")
{
	Fixture f;
	const std::string id = pendingId(answerAuthorize(query(f.ask()), kBase));
	forgetAuthorizationStateForTest();
	PendingView v;
	CHECK_FALSE(viewRequest(id, &v));
}
