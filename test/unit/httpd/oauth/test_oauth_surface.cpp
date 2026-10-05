/*
 * test_oauth_surface.cpp - tests for the OAuth surface
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
#include "httpd/oauth/oauthtest.h"
#include "httpd/oauth/surface.h"

#include <string>
#include <utility>
#include <vector>

using namespace httpd;
using namespace httpd::oauth;

namespace
{

typedef std::vector<std::pair<std::string, std::string> > Headers;

const char kAsPath[] = "/.well-known/oauth-authorization-server";

Exchange metadataRequest(Origin o, const std::string &base)
{
	Exchange x;
	x.method = Get;
	x.path = kAsPath;
	x.origin = o;
	x.base = base;
	return x;
}

} // namespace

TEST_CASE("the surface serves nothing outside the tunnel", "[oauth-surface]")
{
	const Response lan = answer(metadataRequest(Origin::Lan, kPublicUrl));
	REQUIRE(lan.code == StatusNotFound);
	REQUIRE(parsedJson(lan.body)["error"].asString() == "not_found");
	REQUIRE(answer(metadataRequest(Origin::Refused, kPublicUrl)).code == StatusNotFound);
	REQUIRE(answer(metadataRequest(Origin::Tunnel, "")).code == StatusNotFound);
	REQUIRE(answer(Exchange()).code == StatusNotFound);

	const Response tunnel = answer(metadataRequest(Origin::Tunnel, kPublicUrl));
	REQUIRE(tunnel.code == StatusOk);
	REQUIRE(parsedJson(tunnel.body)["issuer"].asString() == kPublicUrl);
}

TEST_CASE("the server metadata is answered under the public url", "[oauth-surface]")
{
	TunnelConfigured config;
	RunningServer srv;
	const testhttp::Reply r = testhttp::request(srv.port, "GET", kAsPath);
	REQUIRE(r.code == 200);
	REQUIRE(r.header("Content-Type").find("application/json") == 0);
	REQUIRE(r.header("Cache-Control").find("no-store") != std::string::npos);
	REQUIRE(r.header("X-Content-Type-Options") == "nosniff");
	REQUIRE(parsedJson(r.body)["issuer"].asString() == kPublicUrl);
}

TEST_CASE("the resource metadata is answered at both discovery urls", "[oauth-surface]")
{
	TunnelConfigured config;
	RunningServer srv;
	const char *paths[] = { "/.well-known/oauth-protected-resource/mcp", "/.well-known/oauth-protected-resource" };
	for (size_t i = 0; i < 2; ++i)
	{
		INFO(paths[i]);
		const testhttp::Reply r = testhttp::request(srv.port, "GET", paths[i]);
		REQUIRE(r.code == 200);
		REQUIRE(parsedJson(r.body)["resource"].asString() == std::string(kPublicUrl) + "/mcp");
	}
}

TEST_CASE("a forged host through the tunnel does not change the issuer", "[oauth-surface]")
{
	TunnelConfigured config;
	RunningServer srv;
	Headers h;
	h.push_back(std::make_pair(std::string("Host"), std::string("evil.example")));
	h.push_back(std::make_pair(std::string("X-Forwarded-Host"), std::string("evil.example")));
	h.push_back(std::make_pair(std::string("Forwarded"), std::string("host=evil.example;proto=https")));
	const testhttp::Reply r = testhttp::request(srv.port, "GET", kAsPath, h);
	REQUIRE(r.code == 200);
	REQUIRE(parsedJson(r.body)["issuer"].asString() == kPublicUrl);
	REQUIRE(r.body.find("evil") == std::string::npos);
}

TEST_CASE("the oauth paths are hidden on the lan", "[oauth-surface]")
{
	{
		LanConfigured config;
		RunningServer srv;
		const testhttp::Reply meta = testhttp::request(srv.port, "GET", kAsPath);
		REQUIRE(meta.code == 404);
		REQUIRE(meta.body.find("issuer") == std::string::npos);
		REQUIRE(testhttp::request(srv.port, "POST", "/oauth/token").code == 404);
		REQUIRE(testhttp::request(srv.port, "GET", "/.well-known/oauth-protected-resource/mcp").code == 404);
	}
	{
		AccountConfigured config;
		RunningServer srv;
		REQUIRE(testhttp::request(srv.port, "GET", kAsPath).code == 404);
	}
}

TEST_CASE("a wrong method is 405 and an unknown path 404 with an oauth error", "[oauth-surface]")
{
	TunnelConfigured config;
	RunningServer srv;
	const testhttp::Reply post = testhttp::request(srv.port, "POST", kAsPath);
	REQUIRE(post.code == 405);
	REQUIRE(post.header("Allow") == "GET, HEAD");
	const testhttp::Reply nothing = testhttp::request(srv.port, "GET", "/oauth/nothing");
	REQUIRE(nothing.code == 404);
	REQUIRE(parsedJson(nothing.body)["error"].asString() == "not_found");
}

TEST_CASE("no answer carries a cross origin header", "[oauth-surface]")
{
	TunnelConfigured config;
	RunningServer srv;
	Headers h;
	h.push_back(std::make_pair(std::string("Origin"), std::string("https://inspector.example")));
	const char *const methods[] = { "GET", "GET", "POST", "GET", "DELETE" };
	const char *const paths[] = { kAsPath, "/.well-known/oauth-protected-resource/mcp", kAsPath,
	                              "/oauth/nothing", "/oauth/token" };
	for (size_t i = 0; i < 5; ++i)
	{
		INFO(paths[i]);
		const testhttp::Reply r = testhttp::request(srv.port, methods[i], paths[i], h);
		REQUIRE(r.transport_ok);
		REQUIRE_FALSE(crossOriginHeader(r.headers));
	}
}

TEST_CASE("only the posts that carry a form or a document keep their body", "[oauth-surface]")
{
	REQUIRE(handles("/oauth/token"));
	REQUIRE(handles(kAsPath));
	REQUIRE_FALSE(handles("/api/v1/ai/clients"));
	REQUIRE(keepsBody(httpd::Post, "/oauth/token"));
	REQUIRE(keepsBody(httpd::Post, "/oauth/register"));
	REQUIRE(keepsBody(httpd::Post, "/oauth/revoke"));
	REQUIRE(keepsBody(httpd::Post, "/oauth/consent"));
	REQUIRE_FALSE(keepsBody(httpd::Get, "/oauth/token"));
	REQUIRE_FALSE(keepsBody(httpd::Post, "/oauth/authorize"));
	REQUIRE_FALSE(keepsBody(httpd::Post, kAsPath));
}
