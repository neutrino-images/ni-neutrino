/*
 * test_oauth_uri.cpp - tests for OAuth URL rules
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
#include "httpd/oauth/uri.h"

#include <string>
#include <utility>
#include <vector>

using namespace httpd::oauth;

TEST_CASE("a url is taken apart into its pieces", "[oauth-uri]")
{
	Url u;
	REQUIRE(parseUrl("HTTPS://Claude.AI:8443/api/mcp/auth_callback?x=1#f", &u));
	REQUIRE(u.scheme == "https");
	REQUIRE(u.host == "claude.ai");
	REQUIRE(u.port == "8443");
	REQUIRE(u.path == "/api/mcp/auth_callback");
	REQUIRE(u.has_query);
	REQUIRE(u.query == "x=1");
	REQUIRE(u.fragment);
	REQUIRE_FALSE(u.userinfo);

	REQUIRE(parseUrl("http://[::1]:5000/callback", &u));
	REQUIRE(u.host == "[::1]");
	REQUIRE(u.port == "5000");
	REQUIRE(parseUrl("https://a@b.example/x", &u));
	REQUIRE(u.userinfo);

	REQUIRE_FALSE(parseUrl("javascript:alert(1)", &u));
	REQUIRE_FALSE(parseUrl("https://", &u));
	REQUIRE_FALSE(parseUrl("https://host:/x", &u));
	REQUIRE_FALSE(parseUrl("https://host:99999/x", &u));
	REQUIRE_FALSE(parseUrl("https://ho st/x", &u));
	REQUIRE_FALSE(parseUrl("/relative", &u));
}

TEST_CASE("a redirect uri is https or http to the loopback and nothing else", "[oauth-uri]")
{
	REQUIRE(redirectUriAcceptable("https://claude.ai/api/mcp/auth_callback"));
	REQUIRE(redirectUriAcceptable("https://chatgpt.com/connector_platform_oauth_redirect"));
	REQUIRE(redirectUriAcceptable("http://localhost/callback"));
	REQUIRE(redirectUriAcceptable("http://127.0.0.1/callback"));
	REQUIRE(redirectUriAcceptable("http://[::1]:8080/cb"));

	REQUIRE_FALSE(redirectUriAcceptable("http://192.168.1.10/callback"));
	REQUIRE_FALSE(redirectUriAcceptable("http://localhost.evil.example/callback"));
	REQUIRE_FALSE(redirectUriAcceptable("cursor://anysphere.cursor-retrieval/oauth"));
	REQUIRE_FALSE(redirectUriAcceptable("https://claude.ai/cb#frag"));
	REQUIRE_FALSE(redirectUriAcceptable("https://user@claude.ai/cb"));
	REQUIRE_FALSE(redirectUriAcceptable("https://claude.ai/" + std::string(600, 'a')));
}

TEST_CASE("open redirect attempts do not match a registered uri", "[oauth-uri]")
{
	const std::string reg = "https://claude.ai/api/mcp/auth_callback";
	REQUIRE(redirectUriMatches(reg, reg));
	REQUIRE_FALSE(redirectUriMatches(reg, "https://claude.ai/api/mcp/auth_callback/"));
	REQUIRE_FALSE(redirectUriMatches(reg, "https://claude.ai/api/mcp/auth_callback?next=https://evil.example"));
	REQUIRE_FALSE(redirectUriMatches(reg, "https://claude.ai.evil.example/api/mcp/auth_callback"));
	REQUIRE_FALSE(redirectUriMatches(reg, "https://claude.ai:444/api/mcp/auth_callback"));
	REQUIRE_FALSE(redirectUriMatches(reg, "https://CLAUDE.ai/api/mcp/auth_callback"));
	REQUIRE_FALSE(redirectUriMatches(reg, "https://claude.ai/api/mcp/../evil"));
	REQUIRE_FALSE(redirectUriMatches(reg, "http://claude.ai/api/mcp/auth_callback"));
}

TEST_CASE("a loopback redirect matches on any port and nothing else", "[oauth-uri]")
{
	const std::string reg = "http://localhost/callback";
	REQUIRE(redirectUriMatches(reg, "http://localhost:53682/callback"));
	REQUIRE(redirectUriMatches(reg, "http://localhost/callback"));
	REQUIRE(redirectUriMatches("http://127.0.0.1:1/callback", "http://127.0.0.1:65000/callback"));
	REQUIRE_FALSE(redirectUriMatches(reg, "http://127.0.0.1:53682/callback"));
	REQUIRE_FALSE(redirectUriMatches(reg, "http://localhost:53682/callback2"));
	REQUIRE_FALSE(redirectUriMatches(reg, "http://localhost:53682/callback?x=1"));
	REQUIRE_FALSE(redirectUriMatches(reg, "http://localhost:53682/callback#x"));
	REQUIRE_FALSE(redirectUriMatches(reg, "https://localhost:53682/callback"));
	REQUIRE_FALSE(redirectUriMatches(reg, "http://evil@localhost:53682/callback"));
	REQUIRE_FALSE(redirectUriMatches(reg, "http://localhost.evil.example:80/callback"));
	// The port exemption is for loopback registrations only.
	REQUIRE_FALSE(redirectUriMatches("https://claude.ai/cb", "https://claude.ai:8443/cb"));
}

TEST_CASE("a client id metadata url follows the draft's rules", "[oauth-uri]")
{
	REQUIRE(cimdUrlAcceptable("https://claude.ai/oauth/claude-code-client-metadata"));
	REQUIRE(cimdUrlAcceptable("https://example.com:8443/client.json"));
	REQUIRE_FALSE(cimdUrlAcceptable("http://example.com/client.json"));
	REQUIRE_FALSE(cimdUrlAcceptable("https://example.com"));
	REQUIRE_FALSE(cimdUrlAcceptable("https://example.com/"));
	REQUIRE_FALSE(cimdUrlAcceptable("https://example.com/a/../client.json"));
	REQUIRE_FALSE(cimdUrlAcceptable("https://example.com/a/./client.json"));
	REQUIRE_FALSE(cimdUrlAcceptable("https://example.com/a/%2e%2e/client.json"));
	REQUIRE_FALSE(cimdUrlAcceptable("https://example.com/client.json?x=1"));
	REQUIRE_FALSE(cimdUrlAcceptable("https://example.com/client.json#f"));
	REQUIRE_FALSE(cimdUrlAcceptable("https://u:p@example.com/client.json"));
}

TEST_CASE("a form is read whole or not at all", "[oauth-uri]")
{
	Form f;
	REQUIRE(parseForm("grant_type=authorization_code&code=nic_1&state=a+b%26c", &f));
	REQUIRE(f["grant_type"] == "authorization_code");
	REQUIRE(f["state"] == "a b&c");
	REQUIRE(parseForm("", &f));
	REQUIRE(f.empty());
	REQUIRE(parseForm("a=&b", &f));
	REQUIRE(f.count("b") == 1u);
	REQUIRE(f["b"].empty());

	// RFC 6749 section 3.1: a parameter sent twice is an invalid request.
	REQUIRE_FALSE(parseForm("client_id=a&client_id=b", &f));
	REQUIRE_FALSE(parseForm("a=%zz", &f));
	REQUIRE_FALSE(parseForm("a=%4", &f));
	REQUIRE_FALSE(parseForm("=x", &f));
}

TEST_CASE("parameters are appended to a redirect encoded", "[oauth-uri]")
{
	std::vector<std::pair<std::string, std::string> > p;
	p.push_back(std::make_pair(std::string("code"), std::string("nic_1")));
	p.push_back(std::make_pair(std::string("iss"), std::string("https://tv.example.org")));
	REQUIRE(withQuery("https://claude.ai/cb", p) == "https://claude.ai/cb?code=nic_1&iss=https%3A%2F%2Ftv.example.org");
	REQUIRE(withQuery("http://localhost:5/cb?x=1", p) == "http://localhost:5/cb?x=1&code=nic_1&iss=https%3A%2F%2Ftv.example.org");
	REQUIRE(formEncode("a b&c~") == "a%20b%26c~");
}

TEST_CASE("formValue returns a reference to an empty string when absent", "[oauth-uri]")
{
	Form f;
	REQUIRE(parseForm("client_id=abc", &f));
	REQUIRE(formValue(f, "client_id") == "abc");
	REQUIRE(formValue(f, "missing").empty());
}
