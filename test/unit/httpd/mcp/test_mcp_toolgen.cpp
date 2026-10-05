/*
 * test_mcp_toolgen.cpp - what a flagged route is described as
 *
 * Copyright (C) 2026 NI-Team
 *
 * License: GPL-2.0-or-later, see COPYING.
 */

#include "support/catch.hpp"

#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/schema.h"
#include "httpd/mcp/contract.h"
#include "httpd/mcp/toolgen.h"

#include "jsoncpp/json/json.h"

#include <string>

using namespace httpd;

namespace
{

Response nothing(const Request &)
{
	return noContent();
}

const FieldDesc kEchoFields[] = {
	HTTPD_MEMBER("echo", FieldType::String, "what came back"),
};
const Schema kEcho = { "echo", HTTPD_FIELDS(kEchoFields) };

const Param kOneId[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId, "the channel"),
};

const Endpoint kRoutes[] = {
	{ Method::Get, "/api/v1/channels/{id}", AuthLevel::Read, "one channel", NULL, HTTPD_PARAMS(kOneId), &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/system/standby", AuthLevel::System, "standby", NULL, NULL, 0, NULL, &nothing, false, Answers204, HTTPD_NO_REFUSALS },
	{ Method::Put, "/api/v1/osd/volume", AuthLevel::Write, "volume", NULL, NULL, 0, NULL, &nothing, false, Answers204, HTTPD_NO_REFUSALS },
	{ Method::Patch, "/api/v1/settings/{section}", AuthLevel::Write, "settings", NULL, NULL, 0, NULL, &nothing, false, Answers204, HTTPD_NO_REFUSALS },
	{ Method::Delete, "/api/v1/timers/{id}", AuthLevel::Write, "remove", NULL, NULL, 0, NULL, &nothing, false, Answers204, HTTPD_NO_REFUSALS },
	{ Method::Get, "/mcp/tools/whats_on", AuthLevel::Read, "composed", "What is on now.", NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
};

const ToolFlag kTools[] = {
	HTTPD_TOOL(Method::Get, "/api/v1/channels/{id}"),
	HTTPD_TOOL_AS(Method::Delete, "/api/v1/timers/{id}", "delete_timer", "Removes one timer."),
};

const RouteTable kTable = { HTTPD_TABLE_WITH_TOOLS("gen", kRoutes, kTools) };

} // namespace

TEST_CASE("a flag finds its route by method and path", "[mcp-toolgen]")
{
	REQUIRE(mcp::flaggedRoute(kTable, kTools[0]) == &kRoutes[0]);
	REQUIRE(mcp::flaggedRoute(kTable, kTools[1]) == &kRoutes[4]);
	const ToolFlag wrong_method = HTTPD_TOOL(Method::Put, "/api/v1/channels/{id}");
	REQUIRE(mcp::flaggedRoute(kTable, wrong_method) == NULL);
	const ToolFlag no_path = HTTPD_TOOL(Method::Get, NULL);
	REQUIRE(mcp::flaggedRoute(kTable, no_path) == NULL);
}

TEST_CASE("a name left out is the verb and the path below the API root", "[mcp-toolgen]")
{
	REQUIRE(mcp::derivedToolName(kRoutes[0]) == "get_channels_by_id");
	REQUIRE(mcp::derivedToolName(kRoutes[1]) == "do_system_standby");
	REQUIRE(mcp::derivedToolName(kRoutes[2]) == "set_osd_volume");
	REQUIRE(mcp::derivedToolName(kRoutes[3]) == "change_settings_by_section");
	REQUIRE(mcp::derivedToolName(kRoutes[4]) == "delete_timers_by_id");
	REQUIRE(mcp::derivedToolName(kRoutes[5]) == "get_mcp_tools_whats_on");
}

TEST_CASE("a name or description written on the flag wins over the route", "[mcp-toolgen]")
{
	REQUIRE(mcp::toolName(kRoutes[0], kTools[0]) == "get_channels_by_id");
	REQUIRE(mcp::toolName(kRoutes[4], kTools[1]) == "delete_timer");
	REQUIRE(mcp::toolDescription(kRoutes[0], kTools[0]) == "one channel");
	REQUIRE(mcp::toolDescription(kRoutes[4], kTools[1]) == "Removes one timer.");
	const ToolFlag bare = HTTPD_TOOL(Method::Get, "/mcp/tools/whats_on");
	REQUIRE(mcp::toolDescription(kRoutes[5], bare) == "What is on now.");
	REQUIRE(mcp::toolTitle("delete_timer") == "Delete timer");
	REQUIRE(mcp::toolTitle("whats_on") == "Whats on");
}

TEST_CASE("hints follow the method and the level", "[mcp-toolgen]")
{
	struct Row { Method m; AuthLevel l; bool ro; bool de; bool id; };
	static const Row rows[] = {
		{ Get,    AuthLevel::Read,   true,  false, true  },
		{ Get,    AuthLevel::System, true,  false, true  },
		{ Put,    AuthLevel::Write,  false, false, true  },
		{ Patch,  AuthLevel::Write,  false, false, false },
		{ Post,   AuthLevel::Write,  false, false, false },
		{ Delete, AuthLevel::Write,  false, true,  true  },
		{ Post,   AuthLevel::System, false, true,  false },
	};
	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
	{
		INFO("row " << i);
		bool ro = !rows[i].ro, de = !rows[i].de, id = !rows[i].id;
		mcp::hintsFor(rows[i].m, rows[i].l, ro, de, id);
		REQUIRE(ro == rows[i].ro);
		REQUIRE(de == rows[i].de);
		REQUIRE(id == rows[i].id);
	}
}

TEST_CASE("a tool carries its route's level and arguments and answer", "[mcp-toolgen]")
{
	const mcp::ToolDef read = mcp::toolFor(kRoutes[0], kTools[0]);
	REQUIRE(read.name == "get_channels_by_id");
	REQUIRE(read.title == "Get channels by id");
	REQUIRE(read.level == AuthLevel::Read);
	REQUIRE(read.read_only);

	::Json::Value in, out;
	::Json::Reader reader;
	REQUIRE(reader.parse(read.input, in));
	REQUIRE(in["properties"].isMember("id"));
	REQUIRE(reader.parse(read.output, out));
	REQUIRE(out["properties"].isMember("echo"));

	const mcp::ToolDef gone = mcp::toolFor(kRoutes[4], kTools[1]);
	REQUIRE(gone.level == AuthLevel::Write);
	REQUIRE(gone.destructive);
	::Json::Value done;
	REQUIRE(reader.parse(gone.output, done));
	REQUIRE(done["properties"].isMember("status"));
}
