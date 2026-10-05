/*
 * test_oauth_api.cpp - tests for the ni-web routes that list, create and revoke AI clients
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

#include <config.h>

#include "support/answers.h"
#include "support/catch.hpp"
#include "support/fakes.h"
#include "support/httpclient.h"
#include "httpd/auth.h"
#include "httpd/oauth/api.h"
#include "httpd/oauth/oauthtest.h"
#include "httpd/oauth/scopes.h"
#include "httpd/oauth/store.h"
#include "httpd/router.h"
#include "httpd/server.h"
#include "httpd/webconfig.h"

#include <jsoncpp/json/json.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace httpd;
using namespace httpd::oauth;

namespace
{

struct Session
{
	std::string token;
	Session() : token(openSession("root"))
	{
		REQUIRE_FALSE(token.empty());
	}
	~Session()
	{
		closeSession(token);
	}
};

Response call(Method m, const std::string &path, const std::string &body, const Session &s)
{
	return dispatchIn(oauthTable, m, path, "", body, "127.0.0.1", AuthLevel::System,
	                  std::string(), s.token, std::string(), std::string(), Origin::Lan);
}

// A LAN server with the default settings, stopped and restored however the case ends.
struct LanServer
{
	InstalledDependencies wired;
	WebConfig             before;
	int                   port;

	LanServer() : before(config()), port(0)
	{
		setConfigForTest(defaultWebConfig());
		ServerConfig c = defaultConfig();
		c.port = 0;
		c.bind_address = "127.0.0.1";
		REQUIRE(start(c));
		port = boundPort();
	}
	~LanServer()
	{
		stop();
		setConfigForTest(before);
	}

	private:
		LanServer(const LanServer &);
		LanServer &operator=(const LanServer &);
};

} // namespace

TEST_CASE("the ai client table is one the server can answer from", "[oauth-api]")
{
	std::string why;
	const bool sane = tableIsSane(oauthTable, &why);
	INFO(why);
	REQUIRE(sane);
	REQUIRE(std::string(oauthTable.tag) == "ai");
	for (size_t i = 0; i < oauthTable.count; ++i)
		REQUIRE(oauthTable.endpoints[i].auth == AuthLevel::System);
}

TEST_CASE("the list shows clients holding access and static tokens and no secret", "[oauth-api]")
{
	store().open(std::string());
	Session s;
	Client c;
	REQUIRE(store().registerClient("Claude", std::vector<std::string>(1, "https://claude.ai/api/mcp/auth_callback"), &c));
	Issued out;
	REQUIRE(store().issue(c, "root", ScopeRead | ScopeOffline, "https://tv.example.org/mcp", &out, NULL));
	Client st;
	std::string token;
	REQUIRE(store().createStatic("HA", ScopeWrite, "root", &st, &token));

	const Response r = call(Get, "/api/v1/ai/clients", "", s);
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find(token) == std::string::npos);
	REQUIRE(r.body.find(out.access_token) == std::string::npos);
	REQUIRE(r.body.find(st.token_hash) == std::string::npos);
	const ::Json::Value v = parsedJson(r.body);
	REQUIRE(v["clients"].size() == 2u);
	for (unsigned i = 0; i < 2; ++i)
	{
		const ::Json::Value &e = v["clients"][i];
		REQUIRE(e["created"].isNumeric());
		REQUIRE(e["last_used"].isNumeric());
		if (e["kind"].asString() == "registered")
		{
			REQUIRE(e["id"].asString() == c.key);
			REQUIRE(e["name"].asString() == "Claude");
			REQUIRE(e["redirect_host"].asString() == "claude.ai");
			REQUIRE_FALSE(e["loopback_only"].asBool());
			REQUIRE(e["scopes"].size() == 2u);
		}
		else
		{
			REQUIRE(e["kind"].asString() == "static");
			REQUIRE(e["id"].asString() == st.key);
			REQUIRE(e["redirect_host"].asString() == "");
			REQUIRE(e["scopes"].size() == 2u);
			REQUIRE(e["last_used"].asInt64() == 0);
		}
	}
}

TEST_CASE("a static token is shown once and is live in the store", "[oauth-api]")
{
	store().open(std::string());
	Session s;
	const Response r = call(Post, "/api/v1/ai/clients", "{\"name\":\"Home Assistant\",\"scopes\":\"read write\"}", s);
	REQUIRE(r.code == 201);
	const ::Json::Value v = parsedJson(r.body);
	const std::string token = v["token"].asString();
	REQUIRE(token.compare(0, 4, "nis_") == 0);
	REQUIRE(v["kind"].asString() == "static");
	REQUIRE(v["name"].asString() == "Home Assistant");
	REQUIRE(v["scopes"].size() == 2u);
	REQUIRE(v["created"].isNumeric());
	TokenFacts t;
	REQUIRE(store().checkToken(token, &t));
	REQUIRE(t.is_static);
	REQUIRE(t.key == v["id"].asString());
	REQUIRE(levelFor(t.scopes) == AuthLevel::Write);
	REQUIRE(t.user == "root");
	REQUIRE(call(Get, "/api/v1/ai/clients", "", s).body.find(token) == std::string::npos);
}

TEST_CASE("a static token carries a level and nothing else", "[oauth-api]")
{
	store().open(std::string());
	Session s;
	const char *scopes[] = {
		"{\"name\":\"x\",\"scopes\":\"offline_access\"}",
		"{\"name\":\"x\",\"scopes\":\"read offline_access\"}",
		"{\"name\":\"x\",\"scopes\":\"admin\"}",
	};
	for (size_t i = 0; i < sizeof(scopes) / sizeof(scopes[0]); ++i)
	{
		INFO(scopes[i]);
		const Response r = call(Post, "/api/v1/ai/clients", scopes[i], s);
		REQUIRE(r.code == 400);
		REQUIRE(r.body.find("not-a-listed-value") != std::string::npos);
	}
	// An empty scopes value is absent as far as the router's own required check
	// is concerned, so this is refused earlier, as missing-parameter.
	const Response empty_scopes = call(Post, "/api/v1/ai/clients", "{\"name\":\"x\",\"scopes\":\"\"}", s);
	REQUIRE(empty_scopes.code == 400);
	REQUIRE(empty_scopes.body.find("missing-parameter") != std::string::npos);
	const char *names[] = {
		"{\"name\":\"a\\u0007b\",\"scopes\":\"read\"}",
		"{\"name\":\"\",\"scopes\":\"read\"}",
	};
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
	{
		INFO(names[i]);
		const Response r = call(Post, "/api/v1/ai/clients", names[i], s);
		REQUIRE(r.code == 400);
	}
	REQUIRE(call(Post, "/api/v1/ai/clients", "{\"name\":\"a\\u0007b\",\"scopes\":\"read\"}", s).body.find("bad-string") !=
	        std::string::npos);
	REQUIRE(store().listClients().empty());
}

TEST_CASE("deleting a client revokes what it holds", "[oauth-api]")
{
	store().open(std::string());
	Session s;
	Client st;
	std::string token;
	REQUIRE(store().createStatic("HA", ScopeRead, "root", &st, &token));
	REQUIRE(call(Delete, "/api/v1/ai/clients/" + st.key, "", s).code == 204);
	TokenFacts t;
	REQUIRE_FALSE(store().checkToken(token, &t));
	const Response again = call(Delete, "/api/v1/ai/clients/" + st.key, "", s);
	REQUIRE(again.code == 404);
	REQUIRE(again.body.find("no-such-name") != std::string::npos);
}

TEST_CASE("the static token ceiling answers 409", "[oauth-api]")
{
	store().open(std::string());
	Session s;
	for (size_t i = 0; i < kMaxStatic; ++i)
		REQUIRE(call(Post, "/api/v1/ai/clients", "{\"name\":\"x\",\"scopes\":\"read\"}", s).code == 201);
	const Response r = call(Post, "/api/v1/ai/clients", "{\"name\":\"x\",\"scopes\":\"read\"}", s);
	REQUIRE(r.code == 409);
	REQUIRE(r.body.find("no-room-for-a-result") != std::string::npos);
}

TEST_CASE("only the box owner with the second token reaches the routes", "[oauth-api]")
{
	store().open(std::string());
	LanServer srv;
	const testhttp::Reply anonymous = testhttp::request(srv.port, "GET", "/api/v1/ai/clients");
	REQUIRE(anonymous.code == 403);

	Session s;
	std::vector<std::pair<std::string, std::string> > h;
	h.push_back(std::make_pair(std::string("Cookie"), std::string(sessionCookieName()) + "=" + s.token));
	h.push_back(std::make_pair(std::string("Content-Type"), std::string("application/json")));
	const std::string body = "{\"name\":\"HA\",\"scopes\":\"read\"}";
	REQUIRE(testhttp::request(srv.port, "POST", "/api/v1/ai/clients", h, body).code == 403);
	h.push_back(std::make_pair(std::string(csrfHeaderName()), csrfFor(s.token)));
	REQUIRE(testhttp::request(srv.port, "POST", "/api/v1/ai/clients", h, body).code == 201);
}

TEST_CASE("the create body example is accepted", "[oauth-api]")
{
	store().open(std::string());
	REQUIRE(sendBodyExample("POST", "/api/v1/ai/clients", std::map<std::string, std::string>()) == 201);
}
