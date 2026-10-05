/*
 * test_mcp_jsonrpc.cpp - tests for reading and writing JSON-RPC messages
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

#include "httpd/mcp/jsonrpc.h"

#include <string>

using httpd::mcp::JsonValue;
using httpd::mcp::Message;
using httpd::mcp::MessageKind;
using httpd::mcp::ReadOutcome;

namespace
{

ReadOutcome readBody(const std::string &body, Message &m)
{
	return httpd::mcp::readMessage(body, 32, m);
}

std::string nested(size_t depth)
{
	std::string s = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"m\",\"params\":{\"a\":";
	s += std::string(depth, '[');
	s += std::string(depth, ']');
	s += "}}";
	return s;
}

} // namespace

TEST_CASE("a request is read with its id and method and params", "[mcp]")
{
	Message m;
	REQUIRE(readBody("{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"tools/list\",\"params\":{\"cursor\":\"x\"}}", m) ==
	        ReadOutcome::Ok);
	REQUIRE(m.kind == MessageKind::Request);
	REQUIRE(m.id.isInt());
	REQUIRE(m.id.asInt() == 7);
	REQUIRE(m.method == "tools/list");
	const JsonValue &params = m.params;
	REQUIRE(params["cursor"].asString() == "x");
}

TEST_CASE("a string id stays a string and absent params stay null", "[mcp]")
{
	Message m;
	REQUIRE(readBody("{\"jsonrpc\":\"2.0\",\"id\":\"a-1\",\"method\":\"m\"}", m) == ReadOutcome::Ok);
	REQUIRE(m.id.isString());
	REQUIRE(m.id.asString() == "a-1");
	REQUIRE(m.params.isNull());
}

TEST_CASE("no id is a notification and a result without a method is a response", "[mcp]")
{
	Message m;
	REQUIRE(readBody("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}", m) == ReadOutcome::Ok);
	REQUIRE(m.kind == MessageKind::Notification);
	REQUIRE(readBody("{\"jsonrpc\":\"2.0\",\"id\":9,\"result\":{}}", m) == ReadOutcome::Ok);
	REQUIRE(m.kind == MessageKind::Response);
	REQUIRE(readBody("{\"jsonrpc\":\"2.0\",\"id\":9,\"error\":{\"code\":1,\"message\":\"x\"}}", m) == ReadOutcome::Ok);
	REQUIRE(m.kind == MessageKind::Response);
}

TEST_CASE("what is not strict UTF-8 JSON is a parse error", "[mcp]")
{
	const char *const broken[] = {
		"",
		"{",
		"{\"jsonrpc\":\"2.0\",\"method\":\"m\"} x",
		"/*c*/{\"jsonrpc\":\"2.0\",\"method\":\"m\"}",
		"{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"method\":\"n\"}",
		"{'jsonrpc':'2.0','method':'m'}"
	};
	Message m;
	for (size_t i = 0; i < sizeof(broken) / sizeof(broken[0]); ++i)
	{
		INFO(broken[i]);
		REQUIRE(readBody(broken[i], m) == ReadOutcome::ParseError);
	}
	REQUIRE(readBody("{\"jsonrpc\":\"2.0\",\"method\":\"\xff\"}", m) == ReadOutcome::ParseError);
}

TEST_CASE("nesting stops at the limit", "[mcp]")
{
	Message m;
	REQUIRE(readBody(nested(20), m) == ReadOutcome::Ok);
	REQUIRE(readBody(nested(40), m) == ReadOutcome::ParseError);
}

TEST_CASE("JSON that is not one request is invalid and keeps the id it could read", "[mcp]")
{
	Message m;
	REQUIRE(readBody("[{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"m\"}]", m) == ReadOutcome::InvalidRequest);
	REQUIRE(m.id.isNull());

	REQUIRE(readBody("{\"jsonrpc\":\"1.0\",\"id\":3,\"method\":\"m\"}", m) == ReadOutcome::InvalidRequest);
	REQUIRE(m.id.isInt());
	REQUIRE(m.id.asInt() == 3);

	const char *const invalid[] = {
		"{\"jsonrpc\":\"2.0\",\"id\":null,\"method\":\"m\"}",
		"{\"jsonrpc\":\"2.0\",\"id\":1.5,\"method\":\"m\"}",
		"{\"jsonrpc\":\"2.0\",\"id\":{},\"method\":\"m\"}",
		"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":5}",
		"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"\"}",
		"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"m\",\"params\":[1]}",
		"{\"jsonrpc\":\"2.0\",\"id\":1}",
		"{\"id\":1,\"method\":\"m\"}"
	};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
	{
		INFO(invalid[i]);
		REQUIRE(readBody(invalid[i], m) == ReadOutcome::InvalidRequest);
	}
}

TEST_CASE("a result is written around the id it answers", "[mcp]")
{
	REQUIRE(httpd::mcp::resultResponse(JsonValue(7), "{}") == "{\"jsonrpc\":\"2.0\",\"id\":7,\"result\":{}}");
	REQUIRE(httpd::mcp::resultResponse(JsonValue("a\"b"), "{\"x\":1}") ==
	        "{\"jsonrpc\":\"2.0\",\"id\":\"a\\\"b\",\"result\":{\"x\":1}}");
}

TEST_CASE("an id the client wrote comes back exactly", "[mcp]")
{
	Message m;
	REQUIRE(readBody("{\"jsonrpc\":\"2.0\",\"id\":9007199254740993,\"method\":\"m\"}", m) == ReadOutcome::Ok);
	REQUIRE(httpd::mcp::resultResponse(m.id, "{}") == "{\"jsonrpc\":\"2.0\",\"id\":9007199254740993,\"result\":{}}");
	REQUIRE(readBody("{\"jsonrpc\":\"2.0\",\"id\":-9007199254740993,\"method\":\"m\"}", m) == ReadOutcome::Ok);
	REQUIRE(httpd::mcp::resultResponse(m.id, "{}") == "{\"jsonrpc\":\"2.0\",\"id\":-9007199254740993,\"result\":{}}");
}

TEST_CASE("an error leaves out an id it does not have", "[mcp]")
{
	REQUIRE(httpd::mcp::errorResponse(JsonValue(), httpd::mcp::kParseError, "Parse error") ==
	        "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32700,\"message\":\"Parse error\"}}");
	REQUIRE(httpd::mcp::errorResponse(JsonValue("x"), httpd::mcp::kUnsupportedProtocolVersion,
	                                  "Unsupported protocol version", "{\"a\":1}") ==
	        "{\"jsonrpc\":\"2.0\",\"id\":\"x\",\"error\":{\"code\":-32022,"
	        "\"message\":\"Unsupported protocol version\",\"data\":{\"a\":1}}}");
}

TEST_CASE("a value is written compactly and text that is not UTF-8 is mended", "[mcp]")
{
	JsonValue v(::Json::objectValue);
	v["a"] = 1;
	v["t"] = "\xc3\xa4";
	JsonValue list(::Json::arrayValue);
	list.append(true);
	list.append(JsonValue());
	list.append(1.5);
	v["l"] = list;
	std::string out;
	REQUIRE(httpd::mcp::toJson(v, out));
	REQUIRE(out == "{\"a\":1,\"l\":[true,null,1.5],\"t\":\"\xc3\xa4\"}");

	JsonValue bad(::Json::objectValue);
	bad["t"] = "\xe4rger";
	REQUIRE(httpd::mcp::toJson(bad, out));
	JsonValue back;
	REQUIRE(httpd::mcp::parseJson(out, 8, back));
	const JsonValue &cback = back;
	REQUIRE(cback["t"].asString() == "\xef\xbf\xbd" "rger");
}

TEST_CASE("a value nested past what the writer takes is not written", "[mcp]")
{
	JsonValue v(::Json::arrayValue);
	for (int i = 0; i < 40; ++i)
	{
		JsonValue outer(::Json::arrayValue);
		outer.append(v);
		v = outer;
	}
	std::string out;
	REQUIRE_FALSE(httpd::mcp::toJson(v, out));
}
