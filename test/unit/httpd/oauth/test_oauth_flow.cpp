/*
 * test_oauth_flow.cpp - a client signing in end to end over a real connection
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
#include "support/httpclient.h"
#include "httpd/mcp/contract.h"
#include "httpd/oauth/authorize.h"
#include "httpd/oauth/oauthtest.h"
#include "httpd/oauth/registration.h"
#include "httpd/oauth/scopes.h"
#include "httpd/oauth/store.h"
#include "httpd/oauth/surface.h"
#include "httpd/oauth/twofactor.h"
#include "httpd/oauth/uri.h"

#include <jsoncpp/json/json.h>

#include <cstdio>
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

typedef std::vector<std::pair<std::string, std::string> > Headers;

const char kCallback[] = "https://claude.ai/api/mcp/auth_callback";
const char kVerifier[] = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
const char kChallenge[] = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM";

Headers with(const char *name, const std::string &value)
{
	return Headers(1, std::make_pair(std::string(name), value));
}

Form paramsOf(const std::string &url)
{
	Form f;
	REQUIRE(parseForm(url.substr(url.find('?') + 1), &f));
	return f;
}

std::string pathOf(const std::string &url)
{
	const size_t scheme = url.find("://");
	return url.substr(url.find('/', scheme + 3));
}

std::string slurp(const std::string &path)
{
	std::string out;
	FILE *f = std::fopen(path.c_str(), "r");
	if (f == NULL)
		return out;
	char buf[4096];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
		out.append(buf, n);
	std::fclose(f);
	return out;
}

std::string resource()
{
	return mcp::resourceOf(kPublicUrl);
}

struct Flow
{
	::Json::Value tokens;
	std::string   client_id;
	std::string   iss;
	long          token_ms;
};

// Register, authorize, consent, exchange, as a client through the tunnel does it.
Flow signIn(int port)
{
	const std::string base = kPublicUrl;
	Flow out;
	const testhttp::Reply reg = testhttp::request(port, "POST", "/oauth/register",
		with("Content-Type", "application/json"),
		std::string("{\"client_name\":\"Claude\",\"redirect_uris\":[\"") + kCallback + "\"],"
		"\"token_endpoint_auth_method\":\"none\"}");
	REQUIRE(reg.code == 201);
	out.client_id = parsedJson(reg.body)["client_id"].asString();

	const testhttp::Reply prm = testhttp::request(port, "GET", "/.well-known/oauth-protected-resource/mcp");
	REQUIRE(parsedJson(prm.body)["authorization_servers"][0].asString() == base);

	std::vector<std::pair<std::string, std::string> > q;
	q.push_back(std::make_pair(std::string("response_type"), std::string("code")));
	q.push_back(std::make_pair(std::string("client_id"), out.client_id));
	q.push_back(std::make_pair(std::string("redirect_uri"), std::string(kCallback)));
	q.push_back(std::make_pair(std::string("code_challenge"), std::string(kChallenge)));
	q.push_back(std::make_pair(std::string("code_challenge_method"), std::string("S256")));
	q.push_back(std::make_pair(std::string("state"), std::string("s1")));
	q.push_back(std::make_pair(std::string("scope"), std::string("read write offline_access")));
	q.push_back(std::make_pair(std::string("resource"), resource()));
	const testhttp::Reply authz = testhttp::request(port, "GET", withQuery("/oauth/authorize", q));
	REQUIRE(authz.code == 302);
	const std::string consent_url = authz.header("Location");
	REQUIRE(consent_url.compare(0, base.size(), base) == 0);

	const testhttp::Reply page = testhttp::request(port, "GET", pathOf(consent_url));
	REQUIRE(page.code == 200);
	const std::string set = page.header("Set-Cookie");
	const std::string token = set.substr(set.find('=') + 1, 32);
	const std::string id = paramsOf(consent_url)["request"];

	Headers h = with("Cookie", std::string(consentCookieName()) + "=" + token);
	h.push_back(std::make_pair(std::string("Content-Type"), std::string("application/x-www-form-urlencoded")));
	const testhttp::Reply decided = testhttp::request(port, "POST", "/oauth/consent", h,
		"request=" + id + "&csrf=" + token + "&user=root&password=sofa-2026&decision=approve"
		"&scope_read=on&scope_write=on&scope_offline_access=on" + "&totp=" + nextTotpCode());
	REQUIRE(decided.code == 302);
	Form back = paramsOf(decided.header("Location"));
	REQUIRE(back["state"] == "s1");
	out.iss = back["iss"];

	std::vector<std::pair<std::string, std::string> > t;
	t.push_back(std::make_pair(std::string("grant_type"), std::string("authorization_code")));
	t.push_back(std::make_pair(std::string("code"), back["code"]));
	t.push_back(std::make_pair(std::string("redirect_uri"), std::string(kCallback)));
	t.push_back(std::make_pair(std::string("code_verifier"), std::string(kVerifier)));
	t.push_back(std::make_pair(std::string("client_id"), out.client_id));
	t.push_back(std::make_pair(std::string("resource"), resource()));
	struct timespec a;
	struct timespec b;
	clock_gettime(CLOCK_MONOTONIC, &a);
	const testhttp::Reply tok = testhttp::request(port, "POST", "/oauth/token",
		with("Content-Type", "application/x-www-form-urlencoded"), withQuery("", t).substr(1));
	clock_gettime(CLOCK_MONOTONIC, &b);
	REQUIRE(tok.code == 200);
	out.token_ms = (b.tv_sec - a.tv_sec) * 1000L + (b.tv_nsec - a.tv_nsec) / 1000000L;
	out.tokens = parsedJson(tok.body);
	return out;
}

struct Clean
{
	std::string path;
	Clean() : path(oauthTempPath("flow"))
	{
		REQUIRE(store().open(path));
		forgetAuthorizationStateForTest();
		resetRegistrationLimitForTest();
	}
	~Clean()
	{
		store().open(std::string());
		::unlink(path.c_str());
	}
};

} // namespace

TEST_CASE("a tunnel client signs in refreshes and is revoked end to end", "[oauth-flow]")
{
	Clean clean;
	TunnelConfigured config;
	TotpInstalled totp;
	RunningServer srv;

	const Flow f = signIn(srv.port);
	REQUIRE(f.iss == kPublicUrl);
	REQUIRE(f.token_ms < 1000);
	const std::string access = f.tokens["access_token"].asString();
	REQUIRE(verifyAccessToken(access, Origin::Tunnel, resource()).ok());

	const std::string disk = slurp(clean.path);
	REQUIRE_FALSE(disk.empty());
	REQUIRE(disk.find(access) == std::string::npos);
	REQUIRE(disk.find(f.tokens["refresh_token"].asString()) == std::string::npos);

	const testhttp::Reply refreshed = testhttp::request(srv.port, "POST", "/oauth/token",
		with("Content-Type", "application/x-www-form-urlencoded"),
		"grant_type=refresh_token&refresh_token=" + f.tokens["refresh_token"].asString() +
		"&client_id=" + f.client_id);
	REQUIRE(refreshed.code == 200);
	const ::Json::Value next = parsedJson(refreshed.body);

	const testhttp::Reply revoked = testhttp::request(srv.port, "POST", "/oauth/revoke",
		with("Content-Type", "application/x-www-form-urlencoded"),
		"token=" + next["refresh_token"].asString() + "&client_id=" + f.client_id);
	REQUIRE(revoked.code == 200);
	REQUIRE_FALSE(verifyAccessToken(next["access_token"].asString(), Origin::Tunnel, resource()).ok());
}

TEST_CASE("an oauth token is good through the tunnel only", "[oauth-flow]")
{
	Clean clean;
	TunnelConfigured config;
	TotpInstalled totp;
	RunningServer srv;
	const std::string access = signIn(srv.port).tokens["access_token"].asString();
	const coreapi::Result<mcp::Caller> tunnel = verifyAccessToken(access, Origin::Tunnel, resource());
	REQUIRE(tunnel.ok());
	REQUIRE(tunnel.value().external);
	REQUIRE(tunnel.value().level == AuthLevel::Write);
	REQUIRE_FALSE(verifyAccessToken(access, Origin::Lan, resource()).ok());
	REQUIRE_FALSE(verifyAccessToken(access, Origin::Lan, "").ok());
	REQUIRE_FALSE(verifyAccessToken(access, Origin::Tunnel, "https://tv.other.example/mcp").ok());
	REQUIRE_FALSE(verifyAccessToken(access, Origin::Refused, resource()).ok());
}

TEST_CASE("a static token is good on the lan only", "[oauth-flow]")
{
	Clean clean;
	Client st;
	std::string token;
	REQUIRE(store().createStatic("Home Assistant", ScopeRead, "root", &st, &token));
	const coreapi::Result<mcp::Caller> lan = verifyAccessToken(token, Origin::Lan, "");
	REQUIRE(lan.ok());
	REQUIRE_FALSE(lan.value().external);
	REQUIRE(lan.value().client_id == st.key);
	REQUIRE_FALSE(verifyAccessToken(token, Origin::Tunnel, resource()).ok());
	REQUIRE_FALSE(verifyAccessToken(token, Origin::Refused, "").ok());
}

TEST_CASE("a lan client cannot reach the oauth surface", "[oauth-flow]")
{
	Clean clean;
	LanConfigured config;
	RunningServer srv;
	const testhttp::Reply reg = testhttp::request(srv.port, "POST", "/oauth/register",
		with("Content-Type", "application/json"),
		std::string("{\"redirect_uris\":[\"") + kCallback + "\"]}");
	REQUIRE(reg.code == 404);
	REQUIRE(testhttp::request(srv.port, "GET", "/.well-known/oauth-authorization-server").code == 404);
	REQUIRE(store().listClients().empty());
}

TEST_CASE("tokens survive a restart of the store", "[oauth-flow]")
{
	Clean clean;
	TunnelConfigured config;
	std::string access;
	{
		TotpInstalled totp;
		RunningServer srv;
		access = signIn(srv.port).tokens["access_token"].asString();
	}
	REQUIRE(store().open(clean.path));
	REQUIRE(verifyAccessToken(access, Origin::Tunnel, resource()).ok());
}

TEST_CASE("tokens granted before two-factor sign-in was turned off keep working and refresh", "[oauth-flow]")
{
	Clean clean;
	TunnelConfigured config;
	RunningServer srv;
	Flow f;
	{
		TotpInstalled totp;
		f = signIn(srv.port);
	}
	REQUIRE_FALSE(totpActive());
	REQUIRE(verifyAccessToken(f.tokens["access_token"].asString(), Origin::Tunnel, resource()).ok());
	const testhttp::Reply refreshed = testhttp::request(srv.port, "POST", "/oauth/token",
		with("Content-Type", "application/x-www-form-urlencoded"),
		"grant_type=refresh_token&refresh_token=" + f.tokens["refresh_token"].asString() +
		"&client_id=" + f.client_id);
	REQUIRE(refreshed.code == 200);
	REQUIRE(verifyAccessToken(parsedJson(refreshed.body)["access_token"].asString(), Origin::Tunnel, resource()).ok());
	REQUIRE(testhttp::request(srv.port, "GET", "/oauth/consent?request=x").code == 403);
}
