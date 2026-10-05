/*
 * test_mcp_headers.cpp - tests for the MCP endpoint's header rules
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

#include "httpd/credentials.h"
#include "httpd/endpoint.h"
#include "httpd/mcp/headers.h"

#include <string>
#include <vector>

TEST_CASE("a mirrored value is plain ASCII or the base64 sentinel", "[mcp]")
{
	std::string out;
	REQUIRE(httpd::mcp::decodeMirrored("us-west1", out));
	REQUIRE(out == "us-west1");
	REQUIRE(httpd::mcp::decodeMirrored("=?base64?SGVsbG8sIOS4lueVjA==?=", out));
	REQUIRE(out == "Hello, \xe4\xb8\x96\xe7\x95\x8c");
	REQUIRE(httpd::mcp::decodeMirrored("=?base64?IHBhZGRlZCA=?=", out));
	REQUIRE(out == " padded ");
	REQUIRE(httpd::mcp::decodeMirrored("=?base64?bGluZTEKbGluZTI=?=", out));
	REQUIRE(out == "line1\nline2");
	REQUIRE(httpd::mcp::decodeMirrored("=?base64?PT9iYXNlNjQ/bGl0ZXJhbD89?=", out));
	REQUIRE(out == "=?base64?literal?=");
}

TEST_CASE("a mirrored value that is neither is refused", "[mcp]")
{
	std::string out;
	REQUIRE_FALSE(httpd::mcp::decodeMirrored("=?base64?@@@@?=", out));
	REQUIRE_FALSE(httpd::mcp::decodeMirrored("=?base64?QR==?=", out));
	REQUIRE_FALSE(httpd::mcp::decodeMirrored("=?base64?/w==?=", out));
	REQUIRE_FALSE(httpd::mcp::decodeMirrored(std::string("a\x01" "b"), out));
	REQUIRE_FALSE(httpd::mcp::decodeMirrored("\xc3\xa4", out));
}

TEST_CASE("the strict base64 reader takes one spelling per byte string", "[mcp]")
{
	std::vector<unsigned char> out;
	REQUIRE(httpd::decodeBase64Strict("QQ==", out));
	REQUIRE(out.size() == 1);
	REQUIRE(out[0] == 'A');
	REQUIRE_FALSE(httpd::decodeBase64Strict("QQ=", out));
	REQUIRE_FALSE(httpd::decodeBase64Strict("QR==", out));
	REQUIRE_FALSE(httpd::decodeBase64Strict("", out));
}

TEST_CASE("the body type is JSON whatever its parameters and case", "[mcp]")
{
	REQUIRE(httpd::mcp::isJsonMediaType("application/json"));
	REQUIRE(httpd::mcp::isJsonMediaType("application/json; charset=utf-8"));
	REQUIRE(httpd::mcp::isJsonMediaType("Application/JSON;charset=UTF-8"));
	REQUIRE(httpd::mcp::isJsonMediaType(" application/json "));
	REQUIRE_FALSE(httpd::mcp::isJsonMediaType(""));
	REQUIRE_FALSE(httpd::mcp::isJsonMediaType("text/plain"));
	REQUIRE_FALSE(httpd::mcp::isJsonMediaType("application/jsonx"));
	REQUIRE_FALSE(httpd::mcp::isJsonMediaType("application/problem+json"));
}

TEST_CASE("an origin compares as lower case without the default port", "[mcp]")
{
	REQUIRE(httpd::mcp::originOf("HTTPS://TV.Example.org:443/mcp") == "https://tv.example.org");
	REQUIRE(httpd::mcp::originOf("https://tv.example.org") == "https://tv.example.org");
	REQUIRE(httpd::mcp::originOf("http://box.test:80") == "http://box.test");
	REQUIRE(httpd::mcp::originOf("http://box.test:8080/x?y") == "http://box.test:8080");
	REQUIRE(httpd::mcp::originOf("http://[::1]:80/mcp") == "http://[::1]");
	REQUIRE(httpd::mcp::originOf("ftp://box.test") == "");
	REQUIRE(httpd::mcp::originOf("null") == "");
	REQUIRE(httpd::mcp::originOf("http://user@box.test/") == "");
	REQUIRE(httpd::mcp::originOf("http://") == "");
}

TEST_CASE("a challenge names the metadata and only the parameters given", "[mcp]")
{
	const std::string url = "https://tv.example.org/.well-known/oauth-protected-resource/mcp";
	REQUIRE(httpd::mcp::bearerChallenge(url, NULL, "read") ==
	        "Bearer resource_metadata=\"" + url + "\", scope=\"read\"");
	REQUIRE(httpd::mcp::bearerChallenge(url, "invalid_token", "read") ==
	        "Bearer resource_metadata=\"" + url + "\", error=\"invalid_token\", scope=\"read\"");
	REQUIRE(httpd::mcp::bearerChallenge(url, "insufficient_scope", "write") ==
	        "Bearer resource_metadata=\"" + url + "\", error=\"insufficient_scope\", scope=\"write\"");
	REQUIRE(httpd::mcp::bearerChallenge("a\"b\\c\r\n", NULL, "") == "Bearer resource_metadata=\"a\\\"b\\\\c\"");
}

TEST_CASE("a challenge without metadata names only what it has", "[mcp]")
{
	REQUIRE(httpd::mcp::bearerChallenge("", NULL, "") == "Bearer");
	REQUIRE(httpd::mcp::bearerChallenge("", "invalid_token", "") == "Bearer error=\"invalid_token\"");
	REQUIRE(httpd::mcp::bearerChallenge("", "insufficient_scope", "system") ==
	        "Bearer error=\"insufficient_scope\", scope=\"system\"");
}

TEST_CASE("each level has its scope", "[mcp]")
{
	REQUIRE(std::string(httpd::mcp::scopeFor(httpd::AuthLevel::Public)) == "read");
	REQUIRE(std::string(httpd::mcp::scopeFor(httpd::AuthLevel::Read)) == "read");
	REQUIRE(std::string(httpd::mcp::scopeFor(httpd::AuthLevel::Write)) == "write");
	REQUIRE(std::string(httpd::mcp::scopeFor(httpd::AuthLevel::System)) == "system");
}
