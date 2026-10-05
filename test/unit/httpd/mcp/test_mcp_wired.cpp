/*
 * test_mcp_wired.cpp - the endpoint wired as neutrino wires it, over a socket
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
#include "support/fakes.h"
#include "support/httpclient.h"

#include "httpd/mcp/contract.h"
#include "httpd/mcp/jsonrpc.h"
#include "httpd/mcp/limits.h"
#include "httpd/mcp/mcpfakes.h"
#include "httpd/mcp/ratelimit.h"
#include "httpd/mcp/wiring.h"
#include "httpd/netmatch.h"
#include "httpd/oauth/scopes.h"
#include "httpd/oauth/store.h"
#include "httpd/router.h"
#include "httpd/server.h"
#include "httpd/webconfig.h"

#include <string>
#include <utility>
#include <vector>

#include <stdlib.h>
#include <unistd.h>

namespace
{

typedef std::vector<std::pair<std::string, std::string> > Headers;

const char kPublic[] = "https://tv.example.org";
const char kResource[] = "https://tv.example.org/mcp";

httpd::WebConfig wiredConfig(bool tunnel)
{
	httpd::WebConfig c = httpd::defaultWebConfig();
	c.ai_enabled = true;
	c.ai_named = true;
	c.ai_allow_lan = true;
	c.ai_public_url = kPublic;
	if (tunnel)
	{
		const char *const proxies[] = { "127.0.0.1/32", "::1/128" };
		for (size_t i = 0; i < sizeof(proxies) / sizeof(proxies[0]); ++i)
		{
			httpd::NetPrefix p;
			REQUIRE(httpd::parsePrefix(proxies[i], &p));
			c.ai_trusted_proxies.push_back(p);
		}
	}
	return c;
}

// The real store in a directory of its own, the wiring of neutrino.cpp and a server.
struct WiredBox
{
	InstalledDependencies deps;
	std::string           dir;
	std::string           file;

	explicit WiredBox(bool tunnel)
	{
		char tmpl[] = "/tmp/ni-mcp-wired-XXXXXX";
		REQUIRE(::mkdtemp(tmpl) != NULL);
		dir = tmpl;
		file = dir + "/ni-web-oauth";
		REQUIRE(httpd::oauth::store().open(file));

		httpd::mcp::setLimits(httpd::mcp::defaultLimits());
		httpd::mcp::forgetRatesForTest();
		httpd::mcp::setRateClockForTest(0);
		httpd::setRoutesForTest(NULL);

		httpd::mcp::Wiring w;
		w.tools = &httpd::mcp::boxTools();
		w.verify = &httpd::oauth::verifyAccessToken;
		httpd::mcp::install(w);

		httpd::setConfigForTest(wiredConfig(tunnel));
		httpd::ServerConfig s = httpd::defaultConfig();
		s.port = 0;
		s.bind_address = "127.0.0.1";
		REQUIRE(httpd::start(s));
	}

	~WiredBox()
	{
		httpd::stop();
		httpd::mcp::uninstall();
		httpd::mcp::forgetRatesForTest();
		httpd::setConfigForTest(httpd::defaultWebConfig());
		httpd::oauth::store().open(std::string());
		::unlink(file.c_str());
		::rmdir(dir.c_str());
	}

	std::string staticToken(unsigned scopes)
	{
		httpd::oauth::Client made;
		std::string token;
		REQUIRE(httpd::oauth::store().createStatic("HA", scopes, "root", &made, &token));
		return token;
	}

	// Full groups by default: these cases test scope wiring, not groups.
	std::string accessToken(unsigned scopes, unsigned groups = httpd::mcp::kAllGroups)
	{
		httpd::oauth::Client c;
		REQUIRE(httpd::oauth::store().registerClient(
			"Claude", std::vector<std::string>(1, "https://claude.ai/api/mcp/auth_callback"), &c));
		httpd::oauth::Issued out;
		REQUIRE(httpd::oauth::store().issue(c, "root", scopes, kResource, &out, NULL, groups));
		return out.access_token;
	}

private:
	WiredBox(const WiredBox &);
	WiredBox &operator=(const WiredBox &);
};

Headers lanHeaders(const std::string &method, const std::string &token)
{
	Headers h;
	h.push_back(std::make_pair(std::string("Host"), std::string("box.test")));
	h.push_back(std::make_pair(std::string("Content-Type"), std::string("application/json")));
	h.push_back(std::make_pair(std::string("Accept"), std::string("application/json, text/event-stream")));
	h.push_back(std::make_pair(std::string("MCP-Protocol-Version"), std::string("2026-07-28")));
	h.push_back(std::make_pair(std::string("Mcp-Method"), method));
	h.push_back(std::make_pair(std::string("Authorization"), "Bearer " + token));
	return h;
}

Headers tunnelHeaders(const std::string &method, const std::string &token)
{
	Headers h = lanHeaders(method, token);
	h[0].second = "tv.example.org";
	h.push_back(std::make_pair(std::string("X-Forwarded-For"), std::string("203.0.113.7")));
	h.push_back(std::make_pair(std::string("X-Forwarded-Proto"), std::string("https")));
	h.push_back(std::make_pair(std::string("X-Forwarded-Host"), std::string("tv.example.org")));
	return h;
}

testhttp::Reply listTools(const Headers &h)
{
	return testhttp::request(httpd::boundPort(), "POST", "/mcp", h, mcpfake::modernBody("1", "tools/list"));
}

bool lists(const testhttp::Reply &r, const char *name)
{
	const httpd::mcp::JsonValue tools = mcpfake::parsed(r.body)["result"]["tools"];
	if (!tools.isArray())
		return false;
	for (unsigned i = 0; i < tools.size(); ++i)
	{
		if (tools[i]["name"].asString() == name)
			return true;
	}
	return false;
}

} // namespace

TEST_CASE("a wired box lists its tools to a static token on the lan", "[mcp][mcp-wired]")
{
	WiredBox box(false);
	const testhttp::Reply r = listTools(lanHeaders("tools/list", box.staticToken(httpd::oauth::ScopeRead)));
	INFO(r.body);
	REQUIRE(r.code == 200);
	REQUIRE(mcpfake::errorCode(r.body) == 0);
	REQUIRE(mcpfake::parsed(r.body)["result"]["tools"].size() > 0u);
	REQUIRE(lists(r, "list_channels"));
}

TEST_CASE("a wired box lists its tools to an access token through the tunnel", "[mcp][mcp-wired]")
{
	WiredBox box(true);
	const std::string token = box.accessToken(httpd::oauth::ScopeRead | httpd::oauth::ScopeWrite);
	const testhttp::Reply r = listTools(tunnelHeaders("tools/list", token));
	INFO(r.body);
	REQUIRE(r.code == 200);
	REQUIRE(mcpfake::errorCode(r.body) == 0);
	REQUIRE(mcpfake::parsed(r.body)["result"]["tools"].size() > 0u);
	REQUIRE(lists(r, "switch_channel"));
}

TEST_CASE("a wired box refuses a static token through the tunnel", "[mcp][mcp-wired]")
{
	WiredBox box(true);
	const testhttp::Reply r = listTools(tunnelHeaders("tools/list", box.staticToken(httpd::oauth::ScopeSystem)));
	INFO(r.body);
	REQUIRE(r.code == 401);
}

TEST_CASE("a wired box refuses a write tool to a read token", "[mcp][mcp-wired]")
{
	WiredBox box(true);
	Headers h = tunnelHeaders("tools/call", box.accessToken(httpd::oauth::ScopeRead));
	h.push_back(std::make_pair(std::string("Mcp-Name"), std::string("switch_channel")));
	const testhttp::Reply r = testhttp::request(httpd::boundPort(), "POST", "/mcp", h,
	                                            mcpfake::modernBody("1", "tools/call",
	                                                                "\"name\":\"switch_channel\",\"arguments\":{}"));
	INFO(r.body);
	REQUIRE(r.code == 403);
	REQUIRE(mcpfake::errorCode(r.body) == httpd::mcp::kInsufficientScope);
	REQUIRE(r.header("WWW-Authenticate").find("error=\"insufficient_scope\"") != std::string::npos);
	REQUIRE(r.header("WWW-Authenticate").find("scope=\"write\"") != std::string::npos);
}
