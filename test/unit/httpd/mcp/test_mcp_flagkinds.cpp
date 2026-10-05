/*
 * test_mcp_flagkinds.cpp - flag defaults and one whole-body argument
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

#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/mcp/composed.h"
#include "httpd/mcp/routetools.h"
#include "httpd/mcp/toolguard.h"
#include "httpd/mcp/jsonrpc.h"

#include "coreapi/base/errors.h"

#include "toolcaller.h"

#include <string>

using namespace httpd;

namespace
{

std::string &seenQuery() { static std::string s; return s; }
std::string &seenBody() { static std::string s; return s; }

Response probe(const Request &r)
{
	seenQuery() = r.has("limit") ? std::to_string(r.asUInt("limit")) : std::string("none");
	Response out = okJson();
	out.body = "{\"ok\":true}";
	return out;
}

Response fill(const Request &r)
{
	seenBody() = r.body();
	return noContent();
}

const FieldDesc kOkFields[] = {
	HTTPD_MEMBER("ok", FieldType::Bool, "whether it went"),
};
const Schema kOkSchema = { "ok", HTTPD_FIELDS(kOkFields) };

const Param kPagedParams[] = {
	HTTPD_QUERY_IN("limit", ParamType::UInt, "how many at most", 1, 50),
};

const Param kFillParams[] = {
	HTTPD_SEGMENT_TEXT("name", "which list", 32),
	HTTPD_BODY_IS_LIST_OF("channels", ParamType::ChannelId, "channel ids, hexadecimal", 0, 10),
};

const Param kMapParams[] = {
	HTTPD_SEGMENT_TEXT("section", "which section", 32),
	HTTPD_BODY_IS_MAP_OF("settings", ParamType::String, "keys and values", 1, 10),
};

const Param kWordParams[] = {
	HTTPD_SEGMENT_TEXT("name", "which list", 32),
	HTTPD_BODY_IS_LIST_OF("words", ParamType::String, "words, free text", 1, 5),
};

const Endpoint kProbe[] = {
	{ Method::Get, "/api/v1/paged", AuthLevel::Read, "a page", NULL,
	  HTTPD_PARAMS(kPagedParams), &kOkSchema, &probe, false, Answers200, HTTPD_NO_REFUSALS },
	{ Method::Put, "/api/v1/lists/{name}/channels", AuthLevel::Write, "fills a list", NULL,
	  HTTPD_PARAMS(kFillParams), NULL, &fill, false, Answers204, HTTPD_NO_REFUSALS },
	{ Method::Patch, "/api/v1/conf/{section}", AuthLevel::Write, "writes a section", NULL,
	  HTTPD_PARAMS(kMapParams), NULL, &fill, false, Answers204, HTTPD_NO_REFUSALS },
	{ Method::Put, "/api/v1/lists/{name}/words", AuthLevel::Write, "fills a word list", NULL,
	  HTTPD_PARAMS(kWordParams), NULL, &fill, false, Answers204, HTTPD_NO_REFUSALS },
};

const ToolFlag kProbeTools[] = {
	HTTPD_TOOL_DEFAULTS(Method::Get, "/api/v1/paged", "paged", "A page of things.", "limit=5"),
	HTTPD_TOOL_AS(Method::Put, "/api/v1/lists/{name}/channels", "fill_list", "Fills one list."),
	HTTPD_TOOL_AS(Method::Patch, "/api/v1/conf/{section}", "write_conf", "Writes one section."),
	HTTPD_TOOL_AS(Method::Put, "/api/v1/lists/{name}/words", "fill_words", "Fills one word list."),
};

const RouteTable kProbeTable = { HTTPD_TABLE_WITH_TOOLS("probe", kProbe, kProbeTools) };
const RouteTable *const kTables[] = { &kProbeTable };

mcp::RouteTools &tools()
{
	static mcp::RouteTools t(kTables, 1, mcp::composedTable());
	return t;
}

mcp::JsonValue mcpParsed(const std::string &text)
{
	mcp::JsonValue v;
	mcp::parseJson(text, 64, v);
	return v;
}

} // namespace

TEST_CASE("a flag adds its default only when the arguments leave it out", "[flagkinds]")
{
	REQUIRE(tools().refusal().empty());
	REQUIRE(tools().call(callerAt(AuthLevel::Read), "paged", "{}").ok());
	REQUIRE(seenQuery() == "5");
	REQUIRE(tools().call(callerAt(AuthLevel::Read), "paged", "{\"limit\":20}").ok());
	REQUIRE(seenQuery() == "20");
}

TEST_CASE("a whole-body list argument becomes the body", "[flagkinds]")
{
	coreapi::Result<mcp::JsonText> r = tools().call(callerAt(AuthLevel::Write), "fill_list",
		"{\"name\":\"Krimi\",\"channels\":[\"283d0001\",\"283d0002\"]}");
	REQUIRE(r.ok());
	REQUIRE(seenBody() == "[\"283d0001\",\"283d0002\"]");
	REQUIRE(tools().call(callerAt(AuthLevel::Write), "fill_list", "{\"name\":\"Krimi\",\"channels\":[]}").ok());
	REQUIRE(seenBody() == "[]");
}

TEST_CASE("a whole-body argument of the wrong kind is refused before the route", "[flagkinds]")
{
	seenBody() = "untouched";
	const char *wrong[] = {
		"{\"name\":\"Krimi\",\"channels\":\"283d0001\"}",
		"{\"name\":\"Krimi\",\"channels\":7}",
		"{\"name\":\"Krimi\",\"channels\":{\"a\":1}}",
	};
	for (size_t i = 0; i < 3; ++i)
	{
		coreapi::Result<mcp::JsonText> r = tools().call(callerAt(AuthLevel::Write), "fill_list", wrong[i]);
		REQUIRE_FALSE(r.ok());
		REQUIRE(r.error().code == coreapi::ErrorCode::BadString);
		REQUIRE(r.error().message.find("channels") != std::string::npos);
	}
	coreapi::Result<mcp::JsonText> nums = tools().call(callerAt(AuthLevel::Write), "fill_list",
		"{\"name\":\"Krimi\",\"channels\":[1,2]}");
	REQUIRE_FALSE(nums.ok());
	REQUIRE(seenBody() == "untouched");
}

TEST_CASE("a whole-body argument left out or sent as null is refused as missing", "[flagkinds]")
{
	seenBody() = "untouched";
	coreapi::Result<mcp::JsonText> gone = tools().call(callerAt(AuthLevel::Write), "fill_list",
		"{\"name\":\"Krimi\"}");
	REQUIRE_FALSE(gone.ok());
	REQUIRE(gone.error().code == coreapi::ErrorCode::MissingParameter);
	REQUIRE(gone.error().message.find("channels") != std::string::npos);
	REQUIRE(seenBody() == "untouched");

	coreapi::Result<mcp::JsonText> null_body = tools().call(callerAt(AuthLevel::Write), "fill_list",
		"{\"name\":\"Krimi\",\"channels\":null}");
	REQUIRE_FALSE(null_body.ok());
	REQUIRE(null_body.error().code == coreapi::ErrorCode::MissingParameter);
	REQUIRE(null_body.error().message.find("channels") != std::string::npos);
}

TEST_CASE("a whole-body argument is required in the schema whatever its minimum", "[flagkinds]")
{
	const std::vector<mcp::ToolDef> all = tools().list();
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].name != "fill_list")
			continue;
		const mcp::JsonValue in = mcpParsed(all[i].input);
		bool found = false;
		for (mcp::JsonValue::const_iterator it = in["required"].begin(); it != in["required"].end(); ++it)
			found = found || (*it).asString() == "channels";
		REQUIRE(found);
	}
}

TEST_CASE("a whole-body map argument becomes the body", "[flagkinds]")
{
	REQUIRE(tools().call(callerAt(AuthLevel::Write), "write_conf",
		"{\"section\":\"audio\",\"settings\":{\"auto_subs\":\"1\"}}").ok());
	REQUIRE(seenBody() == "{\"auto_subs\":\"1\"}");
	REQUIRE_FALSE(tools().call(callerAt(AuthLevel::Write), "write_conf",
		"{\"section\":\"audio\",\"settings\":[\"x\"]}").ok());
}

TEST_CASE("a whole-body map accepts a number or a boolean the route reads as text", "[flagkinds]")
{
	REQUIRE(tools().call(callerAt(AuthLevel::Write), "write_conf",
		"{\"section\":\"audio\",\"settings\":{\"volume\":50,\"muted\":true}}").ok());
	REQUIRE(seenBody() == "{\"muted\":true,\"volume\":50}");
}

TEST_CASE("a whole-body argument is described as an array or an object", "[flagkinds]")
{
	const std::vector<mcp::ToolDef> all = tools().list();
	for (size_t i = 0; i < all.size(); ++i)
	{
		const mcp::JsonValue in = mcpParsed(all[i].input);
		if (all[i].name == "fill_list")
		{
			REQUIRE(in["properties"]["channels"]["type"].asString() == "array");
			REQUIRE(in["properties"]["channels"]["maxItems"].asInt() == 10);
		}
		if (all[i].name == "write_conf")
			REQUIRE(in["properties"]["settings"]["type"].asString() == "object");
	}
}

TEST_CASE("a whole-body list of strings keeps its item type free of the list's own bounds", "[flagkinds]")
{
	const std::vector<mcp::ToolDef> all = tools().list();
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].name != "fill_words")
			continue;
		const mcp::JsonValue in = mcpParsed(all[i].input);
		const mcp::JsonValue &words = in["properties"]["words"];
		REQUIRE(words["type"].asString() == "array");
		REQUIRE(words["minItems"].asInt() == 1);
		REQUIRE(words["maxItems"].asInt() == 5);
		REQUIRE(words["items"]["type"].asString() == "string");
		REQUIRE_FALSE(words["items"].isMember("maxLength"));
		REQUIRE_FALSE(words["items"].isMember("x-max-bytes"));
	}
}

TEST_CASE("the guard holds a flag to one whole body and to defaults it can name", "[flagkinds]")
{
	const Param two[] = {
		HTTPD_BODY_IS_LIST_OF("a", ParamType::String, "a", 0, 2),
		HTTPD_BODY_IS_MAP_OF("b", ParamType::String, "b", 0, 2),
	};
	const Endpoint ep[] = {
		{ Method::Put, "/api/v1/two", AuthLevel::Write, "two bodies", NULL,
		  HTTPD_PARAMS(two), NULL, &fill, false, Answers204, HTTPD_NO_REFUSALS },
	};
	const ToolFlag f[] = { HTTPD_TOOL_AS(Method::Put, "/api/v1/two", "two", "Two.") };
	const RouteTable t = { HTTPD_TABLE_WITH_TOOLS("two", ep, f) };
	const RouteTable *const ts[] = { &t };
	std::string why;
	REQUIRE_FALSE(mcp::toolsAreSane(ts, 1, mcp::composedTable(), &why));
	REQUIRE(why.find("more than one whole body") != std::string::npos);

	const ToolFlag bad[] = { HTTPD_TOOL_DEFAULTS(Method::Get, "/api/v1/paged", "paged", "Page.", "size=5") };
	const RouteTable tb = { HTTPD_TABLE_WITH_TOOLS("probe", kProbe, bad) };
	const RouteTable *const tbs[] = { &tb };
	REQUIRE_FALSE(mcp::toolsAreSane(tbs, 1, mcp::composedTable(), &why));
	REQUIRE(why.find("size") != std::string::npos);
}
