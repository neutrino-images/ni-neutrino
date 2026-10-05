/*
 * test_mcp_admit.cpp - tests for what the MCP endpoint decides off the head
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

#include "httpd/http.h"
#include "httpd/mcp/endpoint.h"
#include "httpd/mcp/jsonrpc.h"
#include "httpd/mcp/limits.h"
#include "httpd/mcp/mcpfakes.h"
#include "httpd/mcp/ratelimit.h"
#include "httpd/mcp/wiring.h"

#include <string>

using mcpfake::errorCode;
using mcpfake::headerOf;
using mcpfake::modernHead;
using mcpfake::tunnelHead;

namespace
{

const char kTunnelMetadata[] = "https://tv.example.org/.well-known/oauth-protected-resource/mcp";

std::string tunnelChallenge(const char *error)
{
	std::string c = std::string("Bearer resource_metadata=\"") + kTunnelMetadata + "\"";
	if (error != NULL)
		c += std::string(", error=\"") + error + "\"";
	return c + ", scope=\"read\"";
}

} // namespace

TEST_CASE("the endpoint is no path at all until it is wired", "[mcp]")
{
	httpd::mcp::uninstall();
	REQUIRE_FALSE(httpd::mcp::handles("/mcp"));
	const httpd::mcp::Admission a = httpd::mcp::admit(modernHead("tools/list"));
	REQUIRE_FALSE(a.admitted);
	REQUIRE(a.refusal.code == 404);

	mcpfake::Wired wired;
	REQUIRE(httpd::mcp::handles("/mcp"));
	REQUIRE_FALSE(httpd::mcp::handles("/mcp/"));
	REQUIRE_FALSE(httpd::mcp::handles("/MCP"));
	REQUIRE_FALSE(httpd::mcp::handles("/api/v1/mcp"));
}

TEST_CASE("a wiring with a member missing is no wiring", "[mcp]")
{
	mcpfake::Wired wired;
	const httpd::mcp::Wiring partial = { &mcpfake::tools(), NULL };
	httpd::mcp::install(partial);
	REQUIRE_FALSE(httpd::mcp::installed(NULL));
	REQUIRE_FALSE(httpd::mcp::handles("/mcp"));
	REQUIRE(httpd::mcp::admit(modernHead("tools/list")).refusal.code == 404);
}

TEST_CASE("a refused origin meets the answer for a path nobody has", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::mcp::Head h = modernHead("tools/list");
	h.origin = httpd::Origin::Refused;
	const httpd::mcp::Admission a = httpd::mcp::admit(h);
	REQUIRE_FALSE(a.admitted);
	REQUIRE(a.refusal.code == 404);
	REQUIRE(a.refusal.content_type == "application/problem+json");
	REQUIRE(a.refusal.body.find("/errors/no-such-route") != std::string::npos);
	REQUIRE(headerOf(a.refusal, "WWW-Authenticate").empty());
}

TEST_CASE("the verifier hears the origin and the resource the transport decided", "[mcp]")
{
	mcpfake::Wired wired;
	REQUIRE(httpd::mcp::admit(modernHead("tools/list")).admitted);
	REQUIRE(mcpfake::seenOrigin() == httpd::Origin::Lan);
	REQUIRE(mcpfake::seenResource().empty());

	REQUIRE(httpd::mcp::admit(tunnelHead("tools/list")).admitted);
	REQUIRE(mcpfake::seenOrigin() == httpd::Origin::Tunnel);
	REQUIRE(mcpfake::seenResource() == "https://tv.example.org/mcp");
}

TEST_CASE("without a configured address the endpoint says so instead of guessing one", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::mcp::Head h = modernHead("tools/list");
	h.base.clear();
	httpd::mcp::Admission a = httpd::mcp::admit(h);
	REQUIRE_FALSE(a.admitted);
	REQUIRE(a.refusal.code == 503);
	REQUIRE(a.refusal.body.find("/errors/webserver-not-configured") != std::string::npos);

	h = tunnelHead("tools/list");
	h.base.clear();
	a = httpd::mcp::admit(h);
	REQUIRE(a.refusal.code == 503);
}

TEST_CASE("only POST is taken", "[mcp]")
{
	mcpfake::Wired wired;
	const httpd::Method others[] = { httpd::Get, httpd::Delete, httpd::Put, httpd::Head, httpd::Options };
	for (size_t i = 0; i < sizeof(others) / sizeof(others[0]); ++i)
	{
		httpd::mcp::Head h = modernHead("tools/list");
		h.method = others[i];
		const httpd::mcp::Admission a = httpd::mcp::admit(h);
		INFO(httpd::methodName(others[i]));
		REQUIRE(a.refusal.code == 405);
		REQUIRE(headerOf(a.refusal, "Allow") == "POST");
		REQUIRE(headerOf(a.refusal, "Access-Control-Allow-Origin").empty());
		REQUIRE(errorCode(a.refusal) == httpd::mcp::kInvalidRequest);
	}
}

TEST_CASE("an Origin is taken only when it is the box's own", "[mcp]")
{
	mcpfake::Wired wired;

	httpd::mcp::Head h = modernHead("tools/list");
	REQUIRE(httpd::mcp::admit(h).admitted);
	h.origin_header = "http://BOX.test:80";
	h.origin_header_count = 1;
	REQUIRE(httpd::mcp::admit(h).admitted);
	const char *const foreign_on_lan[] = {
		"https://box.test", "http://evil.example", "null", "http://box.test:8081", "https://tv.example.org"
	};
	for (size_t i = 0; i < sizeof(foreign_on_lan) / sizeof(foreign_on_lan[0]); ++i)
	{
		h.origin_header = foreign_on_lan[i];
		INFO(foreign_on_lan[i]);
		const httpd::mcp::Admission a = httpd::mcp::admit(h);
		REQUIRE(a.refusal.code == 403);
		REQUIRE(errorCode(a.refusal) == httpd::mcp::kInvalidRequest);
	}

	h = tunnelHead("tools/list");
	h.origin_header = "https://TV.example.org:443";
	h.origin_header_count = 1;
	REQUIRE(httpd::mcp::admit(h).admitted);
	const char *const foreign_on_tunnel[] = {
		"https://evil.example", "http://tv.example.org", "null", "https://tv.example.org:8443", "http://box.test"
	};
	for (size_t i = 0; i < sizeof(foreign_on_tunnel) / sizeof(foreign_on_tunnel[0]); ++i)
	{
		h.origin_header = foreign_on_tunnel[i];
		INFO(foreign_on_tunnel[i]);
		REQUIRE(httpd::mcp::admit(h).refusal.code == 403);
	}

	h.origin_header = "https://tv.example.org";
	h.origin_header_count = 2;
	REQUIRE(httpd::mcp::admit(h).refusal.code == 403);
}

TEST_CASE("a base that is no URL lets no Origin through", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::mcp::Head h = modernHead("tools/list");
	h.base = "box.test";
	h.origin_header = "null";
	h.origin_header_count = 1;
	REQUIRE(httpd::mcp::admit(h).refusal.code == 403);
}

TEST_CASE("a request without a bearer token is challenged as its origin asks", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::mcp::Head h = modernHead("tools/list");
	h.authorization.clear();
	h.authorization_count = 0;
	httpd::mcp::Admission a = httpd::mcp::admit(h);
	REQUIRE(a.refusal.code == 401);
	REQUIRE(headerOf(a.refusal, "WWW-Authenticate") == "Bearer");

	h.authorization = "Basic cm9vdDpuaQ==";
	h.authorization_count = 1;
	a = httpd::mcp::admit(h);
	REQUIRE(a.refusal.code == 401);
	REQUIRE(headerOf(a.refusal, "WWW-Authenticate") == "Bearer");

	h = tunnelHead("tools/list");
	h.authorization.clear();
	h.authorization_count = 0;
	a = httpd::mcp::admit(h);
	REQUIRE(a.refusal.code == 401);
	REQUIRE(headerOf(a.refusal, "WWW-Authenticate") == tunnelChallenge(NULL));
	REQUIRE(headerOf(a.refusal, "WWW-Authenticate").find("offline_access") == std::string::npos);
}

TEST_CASE("a token that does not verify is refused as invalid", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::mcp::Admission a = httpd::mcp::admit(modernHead("tools/list", "", "tok-nobody"));
	REQUIRE(a.refusal.code == 401);
	REQUIRE(headerOf(a.refusal, "WWW-Authenticate") == "Bearer error=\"invalid_token\"");

	a = httpd::mcp::admit(tunnelHead("tools/list", "", "tok-nobody"));
	REQUIRE(a.refusal.code == 401);
	REQUIRE(headerOf(a.refusal, "WWW-Authenticate") == tunnelChallenge("invalid_token"));

	mcpfake::state().token_resource = "https://elsewhere.example/mcp";
	a = httpd::mcp::admit(tunnelHead("tools/list"));
	REQUIRE(a.refusal.code == 401);
}

TEST_CASE("the scheme of the token is read in any case", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::mcp::Head h = modernHead("tools/list");
	h.authorization = "bearer tok-read";
	REQUIRE(httpd::mcp::admit(h).admitted);
}

TEST_CASE("two Authorization headers are a malformed request", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::mcp::Head h = modernHead("tools/list");
	h.authorization_count = 2;
	httpd::mcp::Admission a = httpd::mcp::admit(h);
	REQUIRE(a.refusal.code == 400);
	REQUIRE(headerOf(a.refusal, "WWW-Authenticate") == "Bearer error=\"invalid_request\"");

	h = tunnelHead("tools/list");
	h.authorization_count = 2;
	a = httpd::mcp::admit(h);
	REQUIRE(a.refusal.code == 400);
	REQUIRE(headerOf(a.refusal, "WWW-Authenticate") == tunnelChallenge("invalid_request"));
}

TEST_CASE("a verifier that cannot read its store is the box's fault and not the token's", "[mcp]")
{
	mcpfake::Wired wired;
	const httpd::mcp::Admission a = httpd::mcp::admit(modernHead("tools/list", "", "tok-broken"));
	REQUIRE(a.refusal.code == 500);
	REQUIRE(headerOf(a.refusal, "WWW-Authenticate").empty());
}

TEST_CASE("the caller is who the token says from where the transport says", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::mcp::Admission a = httpd::mcp::admit(modernHead("tools/list", "", "tok-write"));
	REQUIRE(a.admitted);
	REQUIRE(a.caller.client_id == "client-main");
	REQUIRE(a.caller.user == "root");
	REQUIRE(a.caller.level == httpd::AuthLevel::Write);
	REQUIRE_FALSE(a.caller.external);
	REQUIRE(a.resource.empty());
	REQUIRE(a.metadata_url.empty());

	a = httpd::mcp::admit(tunnelHead("tools/list", "", "tok-write"));
	REQUIRE(a.admitted);
	REQUIRE(a.caller.external);
	REQUIRE(a.resource == "https://tv.example.org/mcp");
	REQUIRE(a.metadata_url == kTunnelMetadata);
}

TEST_CASE("the body has to be JSON whatever its parameters and case", "[mcp]")
{
	mcpfake::Wired wired;
	const char *const json[] = {
		"application/json", "application/json; charset=utf-8", "Application/JSON;charset=UTF-8"
	};
	for (size_t i = 0; i < sizeof(json) / sizeof(json[0]); ++i)
	{
		httpd::mcp::Head h = modernHead("tools/list");
		h.content_type = json[i];
		INFO(json[i]);
		REQUIRE(httpd::mcp::admit(h).admitted);
	}
	const char *const other[] = { "", "text/plain", "application/jsonx", "multipart/form-data" };
	for (size_t i = 0; i < sizeof(other) / sizeof(other[0]); ++i)
	{
		httpd::mcp::Head h = modernHead("tools/list");
		h.content_type = other[i];
		INFO(other[i]);
		REQUIRE(httpd::mcp::admit(h).refusal.code == 415);
	}
}

TEST_CASE("an earlier refusal is given even when a later one would also apply", "[mcp]")
{
	mcpfake::Wired wired;

	httpd::mcp::Head h = modernHead("tools/list");
	h.method = httpd::Get;
	h.origin_header = "https://evil.example";
	h.origin_header_count = 1;
	REQUIRE(httpd::mcp::admit(h).refusal.code == 403);

	h = modernHead("tools/list");
	h.method = httpd::Get;
	h.authorization.clear();
	h.authorization_count = 0;
	REQUIRE(httpd::mcp::admit(h).refusal.code == 405);

	h = modernHead("tools/list");
	h.authorization.clear();
	h.authorization_count = 0;
	h.content_type = "text/plain";
	REQUIRE(httpd::mcp::admit(h).refusal.code == 401);

	httpd::mcp::Limits l = httpd::mcp::limits();
	l.rate_burst = 1;
	l.rate_per_minute = 60;
	httpd::mcp::setLimits(l);
	httpd::mcp::setRateClockForTest(5000);
	REQUIRE(httpd::mcp::admit(modernHead("tools/list")).admitted);

	h = modernHead("tools/list");
	h.content_type = "text/plain";
	REQUIRE(httpd::mcp::admit(h).refusal.code == 429);
}

TEST_CASE("each client has its own allowance of requests", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::mcp::Limits l = httpd::mcp::limits();
	l.rate_burst = 2;
	l.rate_per_minute = 60;
	httpd::mcp::setLimits(l);
	httpd::mcp::setRateClockForTest(5000);

	REQUIRE(httpd::mcp::admit(modernHead("tools/list")).admitted);
	REQUIRE(httpd::mcp::admit(modernHead("tools/list")).admitted);
	const httpd::mcp::Admission a = httpd::mcp::admit(modernHead("tools/list"));
	REQUIRE(a.refusal.code == 429);
	REQUIRE(headerOf(a.refusal, "Retry-After") == "1");
	REQUIRE(errorCode(a.refusal) == httpd::mcp::kRateLimited);
	REQUIRE(mcpfake::parsed(a.refusal.body)["error"]["data"]["retryAfterSeconds"].asInt() == 1);

	REQUIRE(httpd::mcp::admit(modernHead("tools/list", "", "tok-other")).admitted);
}

TEST_CASE("the endpoint holds a body to its own ceiling", "[mcp]")
{
	mcpfake::Wired wired;
	REQUIRE(httpd::mcp::maxBodyBytes() == 65536u);
	httpd::mcp::Limits l = httpd::mcp::limits();
	l.max_body_bytes = 100;
	httpd::mcp::setLimits(l);
	REQUIRE(httpd::mcp::maxBodyBytes() == 100u);
}
