/*
 * test_oauth_register.cpp - tests for dynamic client registration
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
#include "httpd/oauth/clientmeta.h"
#include "httpd/oauth/oauthtest.h"
#include "httpd/oauth/registration.h"
#include "httpd/oauth/store.h"

#include <jsoncpp/json/json.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace httpd;
using namespace httpd::oauth;

namespace
{

const char kClaude[] =
	"{\"client_name\":\"Claude\",\"redirect_uris\":[\"https://claude.ai/api/mcp/auth_callback\"],"
	"\"grant_types\":[\"authorization_code\",\"refresh_token\"],\"response_types\":[\"code\"],"
	"\"token_endpoint_auth_method\":\"none\",\"scope\":\"read write\"}";

struct Clean
{
	Clean()
	{
		store().open(std::string());
		resetRegistrationLimitForTest();
	}
};

std::string errorOf(const Response &r)
{
	return parsedJson(r.body)["error"].asString();
}

} // namespace

TEST_CASE("a client registers with json and is answered with its metadata", "[oauth-register]")
{
	Clean c;
	const Response r = answerRegister(kClaude, "application/json; charset=utf-8");
	REQUIRE(r.code == 201);
	const ::Json::Value v = parsedJson(r.body);
	const std::string id = v["client_id"].asString();
	REQUIRE(id.compare(0, 4, "nid_") == 0);
	REQUIRE(v["client_id_issued_at"].isNumeric());
	REQUIRE(v["token_endpoint_auth_method"].asString() == "none");
	REQUIRE(v["redirect_uris"][0].asString() == "https://claude.ai/api/mcp/auth_callback");
	REQUIRE(v["grant_types"].size() == 2u);
	REQUIRE_FALSE(v.isMember("client_secret"));
	Client back;
	REQUIRE(store().findClient(id, &back));
	REQUIRE(back.name == "Claude");
}

TEST_CASE("a client asking for a shared secret is registered as public", "[oauth-register]")
{
	Clean c;
	const Response r = answerRegister(
		"{\"redirect_uris\":[\"https://chatgpt.com/connector_platform_oauth_redirect\"],"
		"\"token_endpoint_auth_method\":\"client_secret_post\"}", "application/json");
	REQUIRE(r.code == 201);
	const ::Json::Value v = parsedJson(r.body);
	REQUIRE(v["token_endpoint_auth_method"].asString() == "none");
	REQUIRE(v["client_name"].asString() == "chatgpt.com");
}

TEST_CASE("a fallback name longer than the cap is still registered", "[oauth-register]")
{
	Clean c;
	const std::string host = std::string(101, 'a') + ".example";
	const Response r = answerRegister(
		"{\"redirect_uris\":[\"https://" + host + "/cb\"]}", "application/json");
	REQUIRE(r.code == 201);
	const ::Json::Value v = parsedJson(r.body);
	REQUIRE(v["client_name"].asString().size() <= kMaxClientNameBytes);
}

TEST_CASE("redirect uris that would open a redirect are refused at registration", "[oauth-register]")
{
	Clean c;
	const char *bodies[] = {
		"{\"redirect_uris\":[\"http://evil.example/cb\"]}",
		"{\"redirect_uris\":[\"cursor://x/cb\"]}",
		"{\"redirect_uris\":[\"https://claude.ai/cb#x\"]}",
		"{\"redirect_uris\":[]}",
		"{\"redirect_uris\":\"https://claude.ai/cb\"}",
		"{\"client_name\":\"x\"}",
		"{\"redirect_uris\":[\"https://a/1\",\"https://a/2\",\"https://a/3\",\"https://a/4\",\"https://a/5\","
		"\"https://a/6\",\"https://a/7\",\"https://a/8\",\"https://a/9\"]}",
	};
	for (size_t i = 0; i < sizeof(bodies) / sizeof(bodies[0]); ++i)
	{
		INFO(bodies[i]);
		const Response r = answerRegister(bodies[i], "application/json");
		REQUIRE(r.code == 400);
		REQUIRE(errorOf(r) == "invalid_redirect_uri");
	}
}

TEST_CASE("metadata the server does not offer is refused", "[oauth-register]")
{
	Clean c;
	const std::string ok_uris = "\"redirect_uris\":[\"https://claude.ai/cb\"]";
	const std::string bodies[] = {
		"{" + ok_uris + ",\"grant_types\":[\"client_credentials\"]}",
		"{" + ok_uris + ",\"grant_types\":[\"refresh_token\"]}",
		"{" + ok_uris + ",\"grant_types\":[\"authorization_code\",\"urn:ietf:params:oauth:grant-type:jwt-bearer\"]}",
		"{" + ok_uris + ",\"token_endpoint_auth_method\":\"private_key_jwt\",\"token_endpoint_auth_methods_supported\":[\"none\"]}",
		"{" + ok_uris + ",\"response_types\":[\"token\"]}",
		"{" + ok_uris + ",\"token_endpoint_auth_method\":\"private_key_jwt\"}",
		"{" + ok_uris + ",\"client_name\":\"" + std::string(101, 'a') + "\"}",
		"{" + ok_uris + ",\"client_name\":\"a\\u0007b\"}",
		"{" + ok_uris + ",\"client_name\":\"a\xc2" "b\"}",
		"{" + ok_uris + ",\"client_name\":7}",
	};
	for (size_t i = 0; i < sizeof(bodies) / sizeof(bodies[0]); ++i)
	{
		INFO(bodies[i]);
		const Response r = answerRegister(bodies[i], "application/json");
		REQUIRE(r.code == 400);
		REQUIRE(errorOf(r) == "invalid_client_metadata");
	}
}

TEST_CASE("bodies that are not one small json object are refused without a throw", "[oauth-register]")
{
	Clean c;
	std::string deep = "{\"a\":";
	for (int i = 0; i < 20000; ++i)
		deep += "[";
	const std::string bodies[] = {
		deep,
		"[1,2]",
		"not json",
		"{\"redirect_uris\":[\"https://claude.ai/cb\"],\"redirect_uris\":[\"https://evil.example/cb\"]}",
		"{\"redirect_uris\":[\"https://claude.ai/cb\"]} trailing",
	};
	for (size_t i = 0; i < sizeof(bodies) / sizeof(bodies[0]); ++i)
	{
		INFO(i);
		const Response r = answerRegister(bodies[i], "application/json");
		REQUIRE(r.code == 400);
		REQUIRE(errorOf(r) == "invalid_client_metadata");
	}
	REQUIRE(answerRegister(std::string(kMaxRegisterBytes + 1, ' '), "application/json").code == 400);
	REQUIRE(answerRegister(kClaude, "application/x-www-form-urlencoded").code == 400);
	REQUIRE(answerRegister(kClaude, "").code == 400);
}

TEST_CASE("depth is counted outside strings only", "[oauth-register]")
{
	REQUIRE(depthWithin("{\"a\":\"[[[[[[[[\"}", 1));
	REQUIRE(depthWithin("{\"a\":\"\\\"[[[[[\"}", 1));
	REQUIRE_FALSE(depthWithin("{\"a\":[[{}]]}", 3));
	REQUIRE(depthWithin("{\"a\":[[{}]]}", 4));
	REQUIRE_FALSE(depthWithin("]", 4));
}

TEST_CASE("registration flooding is held to ten a minute", "[oauth-register]")
{
	Clean c;
	for (unsigned i = 0; i < kRegistrationsPerMinute; ++i)
		REQUIRE(answerRegister(kClaude, "application/json").code == 201);
	const Response r = answerRegister(kClaude, "application/json");
	REQUIRE(r.code == 429);
	REQUIRE(errorOf(r) == "temporarily_unavailable");
	bool retry = false;
	for (size_t i = 0; i < r.headers.size(); ++i)
		retry = retry || (r.headers[i].first == "Retry-After" && r.headers[i].second == "60");
	REQUIRE(retry);
}

TEST_CASE("a registration over the wire keeps its body and answers 201", "[oauth-register]")
{
	Clean c;
	TunnelConfigured config;
	RunningServer srv;
	std::vector<std::pair<std::string, std::string> > h;
	h.push_back(std::make_pair(std::string("Content-Type"), std::string("application/json")));
	const testhttp::Reply r = testhttp::request(srv.port, "POST", "/oauth/register", h, kClaude);
	REQUIRE(r.code == 201);
	REQUIRE_FALSE(crossOriginHeader(r.headers));
	REQUIRE(parsedJson(r.body)["client_id"].asString().compare(0, 4, "nid_") == 0);
}
