/*
 * test_mcp_protocol.cpp - tests for the MCP endpoint's revisions
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

#include "support/catch.hpp"

#include "httpd/http.h"
#include "httpd/mcp/endpoint.h"
#include "httpd/mcp/jsonrpc.h"
#include "httpd/mcp/mcpfakes.h"

#include <string>
#include <vector>

using httpd::mcp::JsonValue;
using mcpfake::bareHead;
using mcpfake::errorCode;
using mcpfake::headerOf;
using mcpfake::legacyBody;
using mcpfake::legacyHead;
using mcpfake::modernBody;
using mcpfake::modernHead;
using mcpfake::parsed;
using mcpfake::roundTrip;

namespace
{

std::vector<std::string> namesIn(const JsonValue &doc)
{
	std::vector<std::string> out;
	const JsonValue &list = doc["result"]["tools"];
	for (JsonValue::ArrayIndex i = 0; i < list.size(); ++i)
		out.push_back(list[i]["name"].asString());
	return out;
}

const JsonValue &toolNamed(const JsonValue &doc, const char *name)
{
	const JsonValue &list = doc["result"]["tools"];
	for (JsonValue::ArrayIndex i = 0; i < list.size(); ++i)
	{
		if (list[i]["name"].asString() == name)
			return list[i];
	}
	return JsonValue::nullSingleton();
}

std::string initializeBody(const std::string &version)
{
	return "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"" + version +
	       "\",\"capabilities\":{},\"clientInfo\":{\"name\":\"c\",\"version\":\"1\"}}}";
}

} // namespace

TEST_CASE("server/discover names the revisions and the capability and the server", "[mcp]")
{
	mcpfake::Wired wired;
	const httpd::Response r = roundTrip(modernHead("server/discover"), modernBody("\"d-1\"", "server/discover"));
	REQUIRE(r.code == 200);
	REQUIRE(r.content_type == "application/json");
	REQUIRE(headerOf(r, "Cache-Control") == "no-store");
	const JsonValue doc = parsed(r.body);
	REQUIRE(doc["id"].asString() == "d-1");
	const JsonValue &result = doc["result"];
	REQUIRE(result["resultType"].asString() == "complete");
	REQUIRE(result["supportedVersions"].size() == 3);
	REQUIRE(result["supportedVersions"][0].asString() == "2026-07-28");
	REQUIRE(result["supportedVersions"][1].asString() == "2025-11-25");
	REQUIRE(result["supportedVersions"][2].asString() == "2025-06-18");
	REQUIRE(result["capabilities"]["tools"].isObject());
	REQUIRE(result["instructions"].isString());
	REQUIRE(result["instructions"].asString().find("missing-parameter") != std::string::npos);
	REQUIRE(result["instructions"].asString().find("bad-string") != std::string::npos);
	REQUIRE(result["ttlMs"].asUInt() == 300000u);
	REQUIRE(result["cacheScope"].asString() == "public");
	const JsonValue &info = result["_meta"]["io.modelcontextprotocol/serverInfo"];
	REQUIRE(info["name"].asString() == PACKAGE_NAME);
	REQUIRE(info["version"].asString() == PACKAGE_VERSION);
}

TEST_CASE("a request names its revision in exactly one header", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::mcp::Head h = modernHead("server/discover");
	h.protocol_version_count = 0;
	httpd::Response r = roundTrip(h, modernBody("5", "server/discover"));
	REQUIRE(r.code == 400);
	REQUIRE(errorCode(r) == httpd::mcp::kHeaderMismatch);
	REQUIRE(parsed(r.body)["id"].asInt() == 5);

	h.protocol_version_count = 2;
	r = roundTrip(h, modernBody("5", "server/discover"));
	REQUIRE(r.code == 400);
	REQUIRE(errorCode(r) == httpd::mcp::kHeaderMismatch);
}

TEST_CASE("a revision the box does not speak is answered with the ones it does", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::mcp::Head h = modernHead("server/discover");
	h.protocol_version = "1900-01-01";
	const httpd::Response r = roundTrip(h, modernBody("4", "server/discover"));
	REQUIRE(r.code == 400);
	const JsonValue doc = parsed(r.body);
	REQUIRE(doc["id"].asInt() == 4);
	REQUIRE(doc["error"]["code"].asInt() == httpd::mcp::kUnsupportedProtocolVersion);
	REQUIRE(doc["error"]["data"]["supported"].size() == 3);
	REQUIRE(doc["error"]["data"]["supported"][0].asString() == "2026-07-28");
	REQUIRE(doc["error"]["data"]["requested"].asString() == "1900-01-01");
}

TEST_CASE("revisions older than 2025-06-18 are refused wherever they are named", "[mcp]")
{
	mcpfake::Wired wired;
	const char *const older[] = { "2025-03-26", "2024-11-05" };
	for (size_t i = 0; i < sizeof(older) / sizeof(older[0]); ++i)
	{
		INFO(older[i]);
		httpd::mcp::Head h = legacyHead();
		h.protocol_version = older[i];
		httpd::Response r = roundTrip(h, legacyBody("1", "tools/list"));
		REQUIRE(r.code == 400);
		REQUIRE(errorCode(r) == httpd::mcp::kUnsupportedProtocolVersion);

		r = roundTrip(h, initializeBody("2025-11-25"));
		REQUIRE(r.code == 400);
		REQUIRE(errorCode(r) == httpd::mcp::kUnsupportedProtocolVersion);

		r = roundTrip(modernHead("server/discover"),
		              std::string("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"server/discover\",\"params\":{\"_meta\":{"
		                          "\"io.modelcontextprotocol/protocolVersion\":\"") + older[i] + "\","
		              "\"io.modelcontextprotocol/clientCapabilities\":{}}}}");
		REQUIRE(r.code == 400);
		REQUIRE(errorCode(r) == httpd::mcp::kUnsupportedProtocolVersion);
		REQUIRE(parsed(r.body)["error"]["data"]["requested"].asString() == older[i]);
	}
}

TEST_CASE("the method header has to be there once and match the body", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::mcp::Head h = modernHead("server/discover");
	h.mcp_method_count = 0;
	httpd::Response r = roundTrip(h, modernBody("1", "server/discover"));
	REQUIRE(r.code == 400);
	REQUIRE(errorCode(r) == httpd::mcp::kHeaderMismatch);

	r = roundTrip(modernHead("tools/list"), modernBody("1", "server/discover"));
	REQUIRE(r.code == 400);
	REQUIRE(errorCode(r) == httpd::mcp::kHeaderMismatch);

	h = modernHead("server/discover");
	h.mcp_method_count = 2;
	r = roundTrip(h, modernBody("1", "server/discover"));
	REQUIRE(r.code == 400);
	REQUIRE(errorCode(r) == httpd::mcp::kHeaderMismatch);
}

TEST_CASE("a modern request carries its version and capabilities in _meta", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::Response r = roundTrip(modernHead("server/discover"),
	                              "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"server/discover\"}");
	REQUIRE(r.code == 400);
	REQUIRE(errorCode(r) == httpd::mcp::kInvalidParams);

	r = roundTrip(modernHead("server/discover"),
	              "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"server/discover\",\"params\":{\"_meta\":{"
	              "\"io.modelcontextprotocol/protocolVersion\":\"2025-11-25\","
	              "\"io.modelcontextprotocol/clientCapabilities\":{}}}}");
	REQUIRE(r.code == 400);
	REQUIRE(errorCode(r) == httpd::mcp::kHeaderMismatch);

	r = roundTrip(modernHead("server/discover"),
	              "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"server/discover\",\"params\":{\"_meta\":{"
	              "\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\"}}}");
	REQUIRE(r.code == 400);
	REQUIRE(errorCode(r) == httpd::mcp::kInvalidParams);
}

TEST_CASE("methods the modern revision does not have are not found", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::Response r = roundTrip(modernHead("ping"), modernBody("1", "ping"));
	REQUIRE(r.code == 404);
	REQUIRE(errorCode(r) == httpd::mcp::kMethodNotFound);
	r = roundTrip(modernHead("resources/list"), modernBody("1", "resources/list"));
	REQUIRE(r.code == 404);
	REQUIRE(errorCode(r) == httpd::mcp::kMethodNotFound);
}

TEST_CASE("initialize is answered for the older revisions and opens no session", "[mcp]")
{
	mcpfake::Wired wired;
	const httpd::Response r1 = roundTrip(bareHead(), initializeBody("2025-11-25"));
	REQUIRE(r1.code == 200);
	REQUIRE(headerOf(r1, "Mcp-Session-Id").empty());
	const JsonValue doc1 = parsed(r1.body);
	REQUIRE(doc1["result"]["protocolVersion"].asString() == "2025-11-25");
	REQUIRE(doc1["result"]["capabilities"]["tools"].isObject());
	REQUIRE(doc1["result"]["serverInfo"]["name"].asString() == PACKAGE_NAME);
	REQUIRE(doc1["result"]["serverInfo"]["version"].asString() == PACKAGE_VERSION);
	REQUIRE(doc1["result"]["instructions"].isString());
	REQUIRE(doc1["result"]["instructions"].asString().find("out-of-range") != std::string::npos);
	REQUIRE_FALSE(doc1["result"].isMember("resultType"));

	const JsonValue doc2 = parsed(roundTrip(bareHead(), initializeBody("2025-06-18")).body);
	REQUIRE(doc2["result"]["protocolVersion"].asString() == "2025-06-18");
	const JsonValue doc3 = parsed(roundTrip(bareHead(), initializeBody("2024-11-05")).body);
	REQUIRE(doc3["result"]["protocolVersion"].asString() == "2025-11-25");
	const JsonValue doc4 = parsed(roundTrip(bareHead(), initializeBody("2026-07-28")).body);
	REQUIRE(doc4["result"]["protocolVersion"].asString() == "2025-11-25");
	const JsonValue doc5 = parsed(roundTrip(legacyHead(), initializeBody("2025-06-18")).body);
	REQUIRE(doc5["result"]["protocolVersion"].asString() == "2025-06-18");

	const httpd::Response r2 = roundTrip(bareHead(), "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
	REQUIRE(r2.code == 200);
	REQUIRE(errorCode(r2) == httpd::mcp::kInvalidParams);
}

TEST_CASE("the older revisions have ping and no discovery", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::Response r = roundTrip(legacyHead(), legacyBody("\"p\"", "ping"));
	REQUIRE(r.code == 200);
	REQUIRE(r.body == "{\"jsonrpc\":\"2.0\",\"id\":\"p\",\"result\":{}}");
	r = roundTrip(legacyHead(), legacyBody("2", "server/discover"));
	REQUIRE(r.code == 200);
	REQUIRE(errorCode(r) == httpd::mcp::kMethodNotFound);
}

TEST_CASE("notifications and responses are taken without an answer", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::Response r = roundTrip(legacyHead(), "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}");
	REQUIRE(r.code == 202);
	REQUIRE(r.body.empty());
	// Sent right after initialize, before the client has a version to name.
	r = roundTrip(bareHead(), "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}");
	REQUIRE(r.code == 202);
	r = roundTrip(legacyHead(), "{\"jsonrpc\":\"2.0\",\"id\":9,\"result\":{}}");
	REQUIRE(r.code == 202);

	// A bad version header is answered even for a message with no reply of its own.
	httpd::mcp::Head unsupported = legacyHead();
	unsupported.protocol_version = "1900-01-01";
	r = roundTrip(unsupported, "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}");
	REQUIRE(r.code == 400);
	REQUIRE(errorCode(r) == httpd::mcp::kUnsupportedProtocolVersion);

	httpd::mcp::Head doubled = legacyHead();
	doubled.protocol_version_count = 2;
	r = roundTrip(doubled, "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}");
	REQUIRE(r.code == 400);
	REQUIRE(errorCode(r) == httpd::mcp::kHeaderMismatch);
}

TEST_CASE("a body that is not one JSON-RPC request is refused with the matching code", "[mcp]")
{
	mcpfake::Wired wired;
	const char *const unparsable[] = { "", "{", "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"\xff\"}" };
	for (size_t i = 0; i < sizeof(unparsable) / sizeof(unparsable[0]); ++i)
	{
		const httpd::Response r = roundTrip(modernHead("server/discover"), unparsable[i]);
		INFO(i);
		REQUIRE(r.code == 400);
		REQUIRE(errorCode(r) == httpd::mcp::kParseError);
		REQUIRE_FALSE(parsed(r.body).isMember("id"));
	}

	httpd::Response r = roundTrip(modernHead("server/discover"), "[" + modernBody("1", "server/discover") + "]");
	REQUIRE(r.code == 400);
	REQUIRE(errorCode(r) == httpd::mcp::kInvalidRequest);

	r = roundTrip(modernHead("server/discover"), "{\"jsonrpc\":\"1.0\",\"id\":3,\"method\":\"server/discover\"}");
	REQUIRE(r.code == 400);
	REQUIRE(errorCode(r) == httpd::mcp::kInvalidRequest);
	REQUIRE(parsed(r.body)["id"].asInt() == 3);
}

TEST_CASE("ids come back exactly as sent", "[mcp]")
{
	mcpfake::Wired wired;
	const httpd::Response r1 = roundTrip(modernHead("server/discover"),
	                                     modernBody("9007199254740993", "server/discover"));
	REQUIRE(errorCode(r1) == 0);
	const JsonValue doc1 = parsed(r1.body);
	REQUIRE(doc1["id"].asUInt64() == 9007199254740993ULL);
	REQUIRE(doc1["result"]["resultType"].asString() == "complete");

	const httpd::Response r2 = roundTrip(modernHead("server/discover"),
	                                     modernBody("\"\xc3\xa4-1\"", "server/discover"));
	REQUIRE(errorCode(r2) == 0);
	const JsonValue doc2 = parsed(r2.body);
	REQUIRE(doc2["id"].asString() == "\xc3\xa4-1");
	REQUIRE(doc2["result"]["resultType"].asString() == "complete");
}

TEST_CASE("tools/list offers the sound tools the caller's level reaches by name", "[mcp]")
{
	mcpfake::Wired wired;
	const JsonValue doc = parsed(roundTrip(modernHead("tools/list"), modernBody("1", "tools/list")).body);
	const std::vector<std::string> read = {
		"controlchars", "deep", "echo", "latin1", "numbers", "slow", "throws", "whoami"
	};
	REQUIRE(namesIn(doc) == read);
	REQUIRE(doc["result"]["resultType"].asString() == "complete");
	REQUIRE(doc["result"]["ttlMs"].asUInt() == 300000u);
	REQUIRE(doc["result"]["cacheScope"].asString() == "private");
	REQUIRE(doc["result"]["_meta"]["io.modelcontextprotocol/serverInfo"]["name"].asString() == PACKAGE_NAME);

	const JsonValue &whoami = toolNamed(doc, "whoami");
	REQUIRE(whoami["title"].asString() == "Title of whoami");
	REQUIRE(whoami["description"].asString() == "Description of whoami");
	REQUIRE(whoami["inputSchema"]["type"].asString() == "object");
	REQUIRE(whoami["inputSchema"]["additionalProperties"].asBool() == false);
	REQUIRE(whoami["outputSchema"]["type"].asString() == "object");
	REQUIRE(whoami["annotations"]["readOnlyHint"].asBool() == true);
	REQUIRE(whoami["annotations"]["destructiveHint"].asBool() == false);
	REQUIRE(whoami["annotations"]["idempotentHint"].asBool() == true);
	REQUIRE(whoami["annotations"]["openWorldHint"].isBool());
	REQUIRE(whoami["annotations"]["openWorldHint"].asBool() == false);
	REQUIRE_FALSE(toolNamed(doc, "echo").isMember("outputSchema"));
	REQUIRE(toolNamed(doc, "echo")["annotations"]["readOnlyHint"].asBool() == true);

	const JsonValue docSystem = parsed(roundTrip(modernHead("tools/list", "", "tok-system"),
	                                            modernBody("1", "tools/list")).body);
	const std::vector<std::string> all = {
		"controlchars", "deep", "echo", "latin1", "numbers", "reboot_like", "slow", "standby", "throws", "whoami"
	};
	REQUIRE(namesIn(docSystem) == all);
	REQUIRE(toolNamed(docSystem, "reboot_like")["annotations"]["destructiveHint"].asBool() == true);
}

TEST_CASE("the older revisions get the same list without the modern members", "[mcp]")
{
	mcpfake::Wired wired;
	const JsonValue doc = parsed(roundTrip(legacyHead(), legacyBody("1", "tools/list")).body);
	REQUIRE(namesIn(doc).size() == 8);
	REQUIRE_FALSE(doc["result"].isMember("resultType"));
	REQUIRE_FALSE(doc["result"].isMember("ttlMs"));
	REQUIRE_FALSE(doc["result"].isMember("cacheScope"));
	REQUIRE_FALSE(doc["result"].isMember("_meta"));
}

TEST_CASE("a cursor the box never handed out is refused", "[mcp]")
{
	mcpfake::Wired wired;
	const httpd::Response r = roundTrip(modernHead("tools/list"), modernBody("1", "tools/list", "\"cursor\":\"x\""));
	REQUIRE(r.code == 200);
	REQUIRE(errorCode(r) == httpd::mcp::kInvalidParams);
}
