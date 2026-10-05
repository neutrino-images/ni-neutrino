/*
 * test_mcp_tools.cpp - tests for calling tools through the MCP endpoint
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

#include <chrono>
#include <string>

using httpd::mcp::JsonValue;
using mcpfake::errorCode;
using mcpfake::headerOf;
using mcpfake::parsed;
using mcpfake::roundTrip;

namespace
{

std::string callParams(const std::string &tool, const std::string &arguments)
{
	std::string p = "\"name\":\"" + tool + "\"";
	if (!arguments.empty())
		p += ",\"arguments\":" + arguments;
	return p;
}

httpd::Response callModern(const std::string &tool, const std::string &arguments,
                           const std::string &token = "tok-read")
{
	return roundTrip(mcpfake::modernHead("tools/call", tool, token),
	                 mcpfake::modernBody("1", "tools/call", callParams(tool, arguments)));
}

httpd::Response callTunnel(const std::string &tool, const std::string &arguments,
                           const std::string &token = "tok-read")
{
	return roundTrip(mcpfake::tunnelHead("tools/call", tool, token),
	                 mcpfake::modernBody("1", "tools/call", callParams(tool, arguments)));
}

httpd::Response callLegacy(const std::string &tool, const std::string &arguments,
                           const std::string &token = "tok-read")
{
	return roundTrip(mcpfake::legacyHead(token), mcpfake::legacyBody("1", "tools/call", callParams(tool, arguments)));
}

std::string textOf(const JsonValue &doc)
{
	return doc["result"]["content"][0]["text"].asString();
}

bool isError(const JsonValue &doc)
{
	return doc["result"]["isError"].isBool() && doc["result"]["isError"].asBool();
}

} // namespace

TEST_CASE("a call is answered with its value as text and as structure", "[mcp]")
{
	mcpfake::Wired wired;
	const httpd::Response r = callModern("echo", "{\"a\":1}");
	REQUIRE(r.code == 200);
	const JsonValue doc = parsed(r.body);
	REQUIRE(doc["result"]["isError"].isBool());
	REQUIRE_FALSE(isError(doc));
	REQUIRE(doc["result"]["resultType"].asString() == "complete");
	REQUIRE(doc["result"]["content"][0]["type"].asString() == "text");
	REQUIRE(textOf(doc) == "{\"a\":1}");
	REQUIRE(doc["result"]["structuredContent"]["a"].asInt() == 1);
}

TEST_CASE("arguments left out reach the tool as an empty object", "[mcp]")
{
	mcpfake::Wired wired;
	const JsonValue doc = parsed(callModern("echo", "").body);
	REQUIRE(textOf(doc) == "{}");
	REQUIRE(doc["result"]["structuredContent"].isObject());
	REQUIRE(doc["result"]["structuredContent"].size() == 0);
}

TEST_CASE("arguments that are not an object are refused before the tool runs", "[mcp]")
{
	mcpfake::Wired wired;
	const int before = mcpfake::tools().calls();
	const char *const not_objects[] = { "[1]", "\"x\"", "5", "null" };
	for (size_t i = 0; i < sizeof(not_objects) / sizeof(not_objects[0]); ++i)
	{
		const httpd::Response r = callModern("echo", not_objects[i]);
		INFO(not_objects[i]);
		REQUIRE(r.code == 200);
		REQUIRE(errorCode(r) == httpd::mcp::kInvalidParams);
	}
	REQUIRE(mcpfake::tools().calls() == before);
}

TEST_CASE("structure that is not an object is only sent where the revision takes it", "[mcp]")
{
	mcpfake::Wired wired;
	const JsonValue modern_doc = parsed(callModern("numbers", "").body);
	REQUIRE(modern_doc["result"]["structuredContent"].isArray());
	REQUIRE(modern_doc["result"]["structuredContent"].size() == 3);
	REQUIRE(textOf(modern_doc) == "[1,2,3]");

	const JsonValue legacy_doc = parsed(callLegacy("numbers", "").body);
	REQUIRE_FALSE(legacy_doc["result"].isMember("structuredContent"));
	REQUIRE(textOf(legacy_doc) == "[1,2,3]");
	REQUIRE_FALSE(legacy_doc["result"].isMember("resultType"));
	REQUIRE_FALSE(isError(legacy_doc));
}

TEST_CASE("text that is not UTF-8 reaches the client as valid JSON", "[mcp]")
{
	mcpfake::Wired wired;
	const httpd::Response r = callModern("latin1", "");
	REQUIRE(r.code == 200);
	const JsonValue doc = parsed(r.body);
	REQUIRE(doc.isObject());
	REQUIRE(isError(doc));
	REQUIRE(textOf(doc).find("box-unreadable") != std::string::npos);
	REQUIRE_FALSE(doc["result"].isMember("structuredContent"));
}

TEST_CASE("a raw control byte inside the tool's value is escaped and not spliced in broken", "[mcp]")
{
	mcpfake::Wired wired;
	const httpd::Response r = callModern("controlchars", "");
	REQUIRE(r.code == 200);
	// The suite's own reader accepts a raw LF, so this pins the escaping of it.
	REQUIRE(r.body.find('\n') == std::string::npos);
	const JsonValue doc = parsed(r.body);
	// A document the suite's own reader can parse at all is the point: the old
	// code spliced the tool's raw bytes in as they stood.
	REQUIRE(doc.isObject());
	REQUIRE_FALSE(isError(doc));
	REQUIRE(doc["result"]["structuredContent"]["msg"].asString() == std::string("a\nb\0c", 5));
}

TEST_CASE("a value nested past the depth limit is a failed call and not a crash", "[mcp]")
{
	mcpfake::Wired wired;
	const JsonValue doc = parsed(callModern("deep", "").body);
	REQUIRE(isError(doc));
	REQUIRE(textOf(doc).find("box-unreadable") != std::string::npos);
}

TEST_CASE("a refusal by the tool is a result the model can act on", "[mcp]")
{
	mcpfake::Wired wired;
	const httpd::Response r = callModern("standby", "", "tok-write");
	REQUIRE(r.code == 200);
	const JsonValue doc = parsed(r.body);
	REQUIRE(isError(doc));
	REQUIRE(textOf(doc).find("Error box-in-standby: the box is in standby") == 0);
	REQUIRE(textOf(doc).find("\nHint: Ask the user whether to switch the box on") != std::string::npos);
	REQUIRE_FALSE(doc["result"].isMember("structuredContent"));
}

TEST_CASE("a tool that throws is a failed result", "[mcp]")
{
	mcpfake::Wired wired;
	const JsonValue doc = parsed(callModern("throws", "").body);
	REQUIRE(isError(doc));
	REQUIRE(textOf(doc).find("box-unreadable") != std::string::npos);
	// Not the empty error a call that never filled one in would carry.
	REQUIRE(textOf(doc).find("the tool failed") != std::string::npos);
	REQUIRE(textOf(doc).find("Hint:") == std::string::npos);
}

TEST_CASE("tools that are unknown or malformed cannot be called", "[mcp]")
{
	mcpfake::Wired wired;
	const int before = mcpfake::tools().calls();
	const char *const names[] = { "nope", "bad name!", "bad_schema", "array_schema" };
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
	{
		const httpd::Response r = callModern(names[i], "");
		INFO(names[i]);
		REQUIRE(r.code == 200);
		REQUIRE(errorCode(r) == httpd::mcp::kInvalidParams);
	}
	const httpd::Response r = roundTrip(mcpfake::modernHead("tools/call", "x"),
	                                    mcpfake::modernBody("1", "tools/call", "\"name\":5"));
	REQUIRE(errorCode(r) == httpd::mcp::kInvalidParams);
	REQUIRE(mcpfake::tools().calls() == before);
}

TEST_CASE("a tool above the caller's level asks for the scope it needs", "[mcp]")
{
	mcpfake::Wired wired;
	const int before = mcpfake::tools().calls();
	httpd::Response r = callModern("standby", "", "tok-read");
	REQUIRE(r.code == 403);
	REQUIRE(headerOf(r, "WWW-Authenticate") == "Bearer error=\"insufficient_scope\", scope=\"write\"");
	REQUIRE(errorCode(r) == httpd::mcp::kInsufficientScope);
	REQUIRE(parsed(r.body)["id"].asInt() == 1);
	REQUIRE(mcpfake::tools().calls() == before);

	r = callTunnel("standby", "", "tok-read");
	REQUIRE(r.code == 403);
	REQUIRE(headerOf(r, "WWW-Authenticate") ==
	        "Bearer resource_metadata=\"https://tv.example.org/.well-known/oauth-protected-resource/mcp\", "
	        "error=\"insufficient_scope\", scope=\"write\"");

	r = callModern("reboot_like", "", "tok-write");
	REQUIRE(r.code == 403);
	REQUIRE(headerOf(r, "WWW-Authenticate").find("scope=\"system\"") != std::string::npos);

	r = callModern("reboot_like", "", "tok-system");
	REQUIRE(r.code == 200);
	REQUIRE(errorCode(r) == 0);
	const JsonValue system_doc = parsed(r.body);
	REQUIRE_FALSE(isError(system_doc));
	REQUIRE(system_doc["result"]["structuredContent"]["ok"].asBool() == true);
	REQUIRE(mcpfake::tools().calls() == before + 1);
}

TEST_CASE("the name header has to name the tool the body calls", "[mcp]")
{
	mcpfake::Wired wired;
	const std::string body = mcpfake::modernBody("1", "tools/call", callParams("echo", "{}"));
	httpd::Response r = roundTrip(mcpfake::modernHead("tools/call", "whoami"), body);
	REQUIRE(r.code == 400);
	REQUIRE(errorCode(r) == httpd::mcp::kHeaderMismatch);

	r = roundTrip(mcpfake::modernHead("tools/call"), body);
	REQUIRE(r.code == 400);
	REQUIRE(errorCode(r) == httpd::mcp::kHeaderMismatch);

	r = roundTrip(mcpfake::modernHead("tools/call", "=?base64?ZWNobw==?="), body);
	REQUIRE(r.code == 200);
	REQUIRE(errorCode(r) == 0);
	const JsonValue mirrored_doc = parsed(r.body);
	REQUIRE_FALSE(isError(mirrored_doc));
	REQUIRE(textOf(mirrored_doc) == "{}");

	r = roundTrip(mcpfake::modernHead("tools/call", "=?base64?ZWNobw=?="), body);
	REQUIRE(r.code == 400);

	httpd::mcp::Head twice = mcpfake::modernHead("tools/call", "echo");
	twice.mcp_name_count = 2;
	r = roundTrip(twice, body);
	REQUIRE(r.code == 400);

	r = callLegacy("echo", "{}");
	REQUIRE(r.code == 200);
	REQUIRE(errorCode(r) == 0);
	const JsonValue legacy_echo_doc = parsed(r.body);
	REQUIRE_FALSE(isError(legacy_echo_doc));
	REQUIRE(textOf(legacy_echo_doc) == "{}");
}

TEST_CASE("a call that runs too long is answered before it ends", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::mcp::Limits l = httpd::mcp::limits();
	l.call_timeout_ms = 100;
	httpd::mcp::setLimits(l);
	mcpfake::slowMs() = 600;
	const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	const JsonValue doc = parsed(callModern("slow", "").body);
	const std::chrono::milliseconds elapsed =
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
	REQUIRE(isError(doc));
	REQUIRE(textOf(doc).find("did not finish") != std::string::npos);
	// Answered at the 100 ms deadline, well inside the tool's own 600 ms.
	REQUIRE(elapsed.count() < 400);
	REQUIRE(mcpfake::waitForCalls());
}

TEST_CASE("while earlier calls still run a new one is turned away", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::mcp::Limits l = httpd::mcp::limits();
	l.call_timeout_ms = 50;
	l.max_running_calls = 1;
	httpd::mcp::setLimits(l);
	mcpfake::slowMs() = 500;
	REQUIRE(isError(parsed(callModern("slow", "").body)));
	const JsonValue doc = parsed(callModern("echo", "{}").body);
	REQUIRE(isError(doc));
	REQUIRE(textOf(doc).find("still working") != std::string::npos);
	REQUIRE(mcpfake::waitForCalls());
}

TEST_CASE("the tool sees the caller as admitted", "[mcp]")
{
	mcpfake::Wired wired;
	const JsonValue lan_doc = parsed(callModern("whoami", "{}").body);
	REQUIRE(lan_doc["result"]["structuredContent"]["client"].asString() == "client-main");
	REQUIRE(lan_doc["result"]["structuredContent"]["external"].asBool() == false);
	REQUIRE(lan_doc["result"]["structuredContent"]["level"].asInt() == (int) httpd::AuthLevel::Read);

	const JsonValue tunnel_doc = parsed(callTunnel("whoami", "{}").body);
	REQUIRE(tunnel_doc["result"]["structuredContent"]["external"].asBool() == true);
}
