/*
 * test_mcp_guard.cpp - what may never become a tool, and what a tool must say
 *
 * Copyright (C) 2026 NI-Team
 *
 * License: GPL-2.0-or-later, see COPYING.
 */

#include "support/catch.hpp"

#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/router.h"
#include "httpd/schema.h"
#include "httpd/mcp/toolguard.h"

#include <cstring>
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
const Schema kHollow = { "hollow", kEchoFields, 0 };

const Param kOn[] = {
	HTTPD_BODY_REQUIRED("on", ParamType::Bool, "whether"),
};
const Param kKeyName[] = {
	HTTPD_BODY_REQUIRED_TEXT("name", "the key", 32),
};
const Param kSilent[] = {
	HTTPD_QUERY("q", ParamType::String, ""),
};
const Param kSameTwice[] = {
	HTTPD_SEGMENT("id", ParamType::UInt, "in the path"),
	HTTPD_BODY("id", ParamType::UInt, "in the body"),
};
const Param kBytes[] = {
	HTTPD_BODY_IS_BYTES("blob", "the bytes", 0, 4),
};

const RouteTable kNoComposed = { HTTPD_TABLE_N("composed", (const Endpoint *) NULL, 0) };

bool sane(const RouteTable &t, std::string &why)
{
	const RouteTable *const list[] = { &t };
	return mcp::toolsAreSane(list, 1, kNoComposed, &why);
}

bool composedSane(const RouteTable &c, std::string &why)
{
	return mcp::toolsAreSane(NULL, 0, c, &why);
}

#define REFUSED_WITH(table, words) \
	do { \
		std::string why; \
		const bool passed = sane((table), why); \
		INFO(why); \
		REQUIRE_FALSE(passed); \
		REQUIRE(why.find(words) != std::string::npos); \
	} while (0)

const Endpoint kGoodRoutes[] = {
	{ Method::Get, "/api/v1/g/read", AuthLevel::Read, "reads", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/system/standby", AuthLevel::System, "standby", NULL, HTTPD_PARAMS(kOn), NULL, &nothing, false, Answers204, HTTPD_NO_REFUSALS },
};
const ToolFlag kGoodTools[] = {
	HTTPD_TOOL_AS(Method::Get, "/api/v1/g/read", "read_it", NULL),
	HTTPD_TOOL_AS(Method::Post, "/api/v1/system/standby", "set_standby", NULL),
};
const RouteTable kGood = { HTTPD_TABLE_WITH_TOOLS("g", kGoodRoutes, kGoodTools) };

const ToolFlag kAbsentTool[] = { HTTPD_TOOL_AS(Method::Get, "/api/v1/g/absent", "absent", NULL) };
const RouteTable kAbsent = { HTTPD_TABLE_WITH_TOOLS("g", kGoodRoutes, kAbsentTool) };

const ToolFlag kTwiceTools[] = {
	HTTPD_TOOL_AS(Method::Get, "/api/v1/g/read", "same", NULL),
	HTTPD_TOOL_AS(Method::Post, "/api/v1/system/standby", "same", NULL),
};
const RouteTable kTwice = { HTTPD_TABLE_WITH_TOOLS("g", kGoodRoutes, kTwiceTools) };

// Two different names flagging the one route: caught by the route check, not the name check.
const ToolFlag kFlaggedTwiceTools[] = {
	HTTPD_TOOL_AS(Method::Get, "/api/v1/g/read", "read_it", NULL),
	HTTPD_TOOL_AS(Method::Get, "/api/v1/g/read", "read_it_again", NULL),
};
const RouteTable kFlaggedTwice = { HTTPD_TABLE_WITH_TOOLS("g", kGoodRoutes, kFlaggedTwiceTools) };

const ToolFlag kBadNameTool[] = { HTTPD_TOOL_AS(Method::Get, "/api/v1/g/read", "Read-It", NULL) };
const RouteTable kBadName = { HTTPD_TABLE_WITH_TOOLS("g", kGoodRoutes, kBadNameTool) };

const Endpoint kMuteRoutes[] = {
	{ Method::Get, "/api/v1/g/read", AuthLevel::Read, "", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
};
const ToolFlag kMuteTool[] = { HTTPD_TOOL_AS(Method::Get, "/api/v1/g/read", "read_it", NULL) };
const RouteTable kMute = { HTTPD_TABLE_WITH_TOOLS("g", kMuteRoutes, kMuteTool) };

// A function and not a literal, so the 1025 bytes are built once and never typed out.
const char *longDescription()
{
	static const std::string s(1025, 'x');
	return s.c_str();
}
const Endpoint kLongDescRoutes[] = {
	{ Method::Get, "/api/v1/g/read", AuthLevel::Read, "reads", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
};
const ToolFlag kLongDescTool[] = { HTTPD_TOOL_AS(Method::Get, "/api/v1/g/read", "read_it", longDescription()) };
const RouteTable kLongDesc = { HTTPD_TABLE_WITH_TOOLS("g", kLongDescRoutes, kLongDescTool) };

const Endpoint kHeadRoutes[] = {
	{ Method::Head, "/api/v1/g/read", AuthLevel::Read, "heads", NULL, NULL, 0, NULL, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
};
const ToolFlag kHeadTool[] = { HTTPD_TOOL_AS(Method::Head, "/api/v1/g/read", "head_it", "a head") };
const RouteTable kHeadMethod = { HTTPD_TABLE_WITH_TOOLS("g", kHeadRoutes, kHeadTool) };

const Endpoint kPublicRoutes[] = {
	{ Method::Get, "/api/v1/g/open", AuthLevel::Public, "open", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
};
const ToolFlag kPublicTool[] = { HTTPD_TOOL(Method::Get, "/api/v1/g/open") };
const RouteTable kPublic = { HTTPD_TABLE_WITH_TOOLS("g", kPublicRoutes, kPublicTool) };

const Endpoint kRebootRoutes[] = {
	{ Method::Post, "/api/v1/system/reboot", AuthLevel::System, "reboot", NULL, NULL, 0, NULL, &nothing, false, Answers204, HTTPD_NO_REFUSALS },
};
const ToolFlag kRebootTool[] = { HTTPD_TOOL(Method::Post, "/api/v1/system/reboot") };
const RouteTable kReboot = { HTTPD_TABLE_WITH_TOOLS("g", kRebootRoutes, kRebootTool) };

const Endpoint kKeyRoutes[] = {
	{ Method::Post, "/api/v1/osd/remote/key", AuthLevel::Write, "a key", NULL, HTTPD_PARAMS(kKeyName), NULL, &nothing, false, Answers204, HTTPD_NO_REFUSALS },
};
const ToolFlag kKeyTool[] = { HTTPD_TOOL(Method::Post, "/api/v1/osd/remote/key") };
const RouteTable kKey = { HTTPD_TABLE_WITH_TOOLS("g", kKeyRoutes, kKeyTool) };

const Param kTimerId[] = {
	HTTPD_SEGMENT("id", ParamType::UInt, "the timer"),
};
const Endpoint kTimerWriteRoutes[] = {
	{ Method::Delete, "/api/v1/timers/{id}", AuthLevel::Write, "removes", NULL, HTTPD_PARAMS(kTimerId), NULL, &nothing, false, Answers204, HTTPD_NO_REFUSALS },
};
const ToolFlag kTimerWriteTool[] = { HTTPD_TOOL(Method::Delete, "/api/v1/timers/{id}") };
const RouteTable kTimerWrite = { HTTPD_TABLE_WITH_TOOLS("g", kTimerWriteRoutes, kTimerWriteTool) };

const Endpoint kAiRoutes[] = {
	{ Method::Get, "/api/v1/ai/settings", AuthLevel::System, "settings", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
};
const ToolFlag kAiTool[] = { HTTPD_TOOL(Method::Get, "/api/v1/ai/settings") };
const RouteTable kAi = { HTTPD_TABLE_WITH_TOOLS("g", kAiRoutes, kAiTool) };

const Endpoint kWipeRoutes[] = {
	{ Method::Post, "/api/v1/g/wipe", AuthLevel::System, "wipes", NULL, NULL, 0, NULL, &nothing, false, Answers204, HTTPD_NO_REFUSALS },
};
const ToolFlag kWipeTool[] = { HTTPD_TOOL(Method::Post, "/api/v1/g/wipe") };
const RouteTable kWipe = { HTTPD_TABLE_WITH_TOOLS("g", kWipeRoutes, kWipeTool) };

const Param kSectionSegment[] = {
	HTTPD_SEGMENT_TEXT("section", "the section", 32),
};
const Endpoint kGatedRoutes[] = {
	{ Method::Patch, "/api/v1/settings/{section}", AuthLevel::System, "writes", NULL, HTTPD_PARAMS(kSectionSegment), NULL, &nothing, false, Answers204, HTTPD_NO_REFUSALS },
};
const ToolFlag kGatedTool[] = { HTTPD_TOOL_AS(Method::Patch, "/api/v1/settings/{section}", "write_it", NULL) };
const RouteTable kGatedTable = { HTTPD_TABLE_WITH_TOOLS("g", kGatedRoutes, kGatedTool) };

const Endpoint kTokenRoutes[] = {
	{ Method::Get, "/api/v1/g/media", AuthLevel::Read, "media", NULL, NULL, 0, &kEcho, &nothing, true, Answers200, HTTPD_NO_REFUSALS },
};
const ToolFlag kTokenTool[] = { HTTPD_TOOL(Method::Get, "/api/v1/g/media") };
const RouteTable kToken = { HTTPD_TABLE_WITH_TOOLS("g", kTokenRoutes, kTokenTool) };

const Endpoint kBytesRoutes[] = {
	{ Method::Put, "/api/v1/g/fill", AuthLevel::Write, "fills", NULL, HTTPD_PARAMS(kBytes), NULL, &nothing, false, Answers204, HTTPD_NO_REFUSALS },
};
const ToolFlag kBytesTool[] = { HTTPD_TOOL(Method::Put, "/api/v1/g/fill") };
const RouteTable kBytesBody = { HTTPD_TABLE_WITH_TOOLS("g", kBytesRoutes, kBytesTool) };

const Endpoint kSilentRoutes[] = {
	{ Method::Get, "/api/v1/g/q", AuthLevel::Read, "asks", NULL, HTTPD_PARAMS(kSilent), &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
};
const ToolFlag kSilentTool[] = { HTTPD_TOOL(Method::Get, "/api/v1/g/q") };
const RouteTable kSilentArg = { HTTPD_TABLE_WITH_TOOLS("g", kSilentRoutes, kSilentTool) };

const Endpoint kSameRoutes[] = {
	{ Method::Put, "/api/v1/g/{id}", AuthLevel::Write, "twice", NULL, HTTPD_PARAMS(kSameTwice), NULL, &nothing, false, Answers204, HTTPD_NO_REFUSALS },
};
const ToolFlag kSameTool[] = { HTTPD_TOOL(Method::Put, "/api/v1/g/{id}") };
const RouteTable kSame = { HTTPD_TABLE_WITH_TOOLS("g", kSameRoutes, kSameTool) };

const Endpoint kBlindRoutes[] = {
	{ Method::Get, "/api/v1/g/blind", AuthLevel::Read, "reads", NULL, NULL, 0, NULL, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/g/hollow", AuthLevel::Read, "reads", NULL, NULL, 0, &kHollow, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
};
const ToolFlag kBlindTool[] = { HTTPD_TOOL(Method::Get, "/api/v1/g/blind") };
const ToolFlag kHollowTool[] = { HTTPD_TOOL(Method::Get, "/api/v1/g/hollow") };
const RouteTable kBlind = { HTTPD_TABLE_WITH_TOOLS("g", kBlindRoutes, kBlindTool) };
const RouteTable kHollowAnswer = { HTTPD_TABLE_WITH_TOOLS("g", kBlindRoutes, kHollowTool) };

const Endpoint kEmptyRoutes[] = {
	{ Method::Post, "/api/v1/g/accept", AuthLevel::Write, "accepts", NULL, NULL, 0, &kEcho, &nothing, false, Answers200 | Answers202, HTTPD_NO_REFUSALS },
	{ Method::Delete, "/api/v1/g/drop", AuthLevel::Write, "drops", NULL, NULL, 0, &kEcho, &nothing, false, Answers204, HTTPD_NO_REFUSALS },
};
const ToolFlag kAcceptTool[] = { HTTPD_TOOL_AS(Method::Post, "/api/v1/g/accept", "accept_it", NULL) };
const ToolFlag kDropTool[] = { HTTPD_TOOL_AS(Method::Delete, "/api/v1/g/drop", "drop_it", NULL) };
const RouteTable kEmptyAccepted = { HTTPD_TABLE_WITH_TOOLS("g", kEmptyRoutes, kAcceptTool) };
const RouteTable kEmptyDone = { HTTPD_TABLE_WITH_TOOLS("g", kEmptyRoutes, kDropTool) };

const RouteTable kPhantom ={ "g", kGoodRoutes, 2, NULL, 1 };

const Endpoint kNine[] = {
	{ Method::Get, "/mcp/tools/a", AuthLevel::Read, "a", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/mcp/tools/b", AuthLevel::Read, "b", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/mcp/tools/c", AuthLevel::Read, "c", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/mcp/tools/d", AuthLevel::Read, "d", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/mcp/tools/e", AuthLevel::Read, "e", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/mcp/tools/f", AuthLevel::Read, "f", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/mcp/tools/g", AuthLevel::Read, "g", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/mcp/tools/h", AuthLevel::Read, "h", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/mcp/tools/i", AuthLevel::Read, "i", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
};
const ToolFlag kNineTools[] = {
	HTTPD_TOOL_AS(Method::Get, "/mcp/tools/a", "a", NULL),
	HTTPD_TOOL_AS(Method::Get, "/mcp/tools/b", "b", NULL),
	HTTPD_TOOL_AS(Method::Get, "/mcp/tools/c", "c", NULL),
	HTTPD_TOOL_AS(Method::Get, "/mcp/tools/d", "d", NULL),
	HTTPD_TOOL_AS(Method::Get, "/mcp/tools/e", "e", NULL),
	HTTPD_TOOL_AS(Method::Get, "/mcp/tools/f", "f", NULL),
	HTTPD_TOOL_AS(Method::Get, "/mcp/tools/g", "g", NULL),
	HTTPD_TOOL_AS(Method::Get, "/mcp/tools/h", "h", NULL),
	HTTPD_TOOL_AS(Method::Get, "/mcp/tools/i", "i", NULL),
};
const RouteTable kNineComposed = { HTTPD_TABLE_WITH_TOOLS("composed", kNine, kNineTools) };
const RouteTable kEightComposed = { "composed", kNine, 8, kNineTools, 8 };
const RouteTable kUnoffered = { "composed", kNine, 2, kNineTools, 1 };

const Endpoint kOutside[] = {
	{ Method::Get, "/api/v1/g/composed", AuthLevel::Read, "out", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
};
const ToolFlag kOutsideTool[] = { HTTPD_TOOL_AS(Method::Get, "/api/v1/g/composed", "outside", NULL) };
const RouteTable kOutsideComposed = { HTTPD_TABLE_WITH_TOOLS("composed", kOutside, kOutsideTool) };

const Endpoint kMuteComposed[] = {
	{ Method::Put, "/mcp/tools/act", AuthLevel::Write, "acts", NULL, HTTPD_PARAMS(kOn), NULL, &nothing, false, Answers204, HTTPD_NO_REFUSALS },
};
const ToolFlag kMuteComposedTool[] = { HTTPD_TOOL_AS(Method::Put, "/mcp/tools/act", "act", NULL) };
const RouteTable kAnswerless = { HTTPD_TABLE_WITH_TOOLS("composed", kMuteComposed, kMuteComposedTool) };

// One route, named once but carried twice: the composed table itself is not sane.
const Endpoint kDupeComposedRoutes[] = {
	{ Method::Get, "/mcp/tools/a", AuthLevel::Read, "a", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/mcp/tools/a", AuthLevel::Read, "a again", NULL, NULL, 0, &kEcho, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
};
const ToolFlag kDupeComposedTools[] = {
	HTTPD_TOOL_AS(Method::Get, "/mcp/tools/a", "a", NULL),
	HTTPD_TOOL_AS(Method::Get, "/mcp/tools/a", "a2", NULL),
};
const RouteTable kDupeComposed = { HTTPD_TABLE_WITH_TOOLS("composed", kDupeComposedRoutes, kDupeComposedTools) };

// The never-exposed set, pinned row for row.
struct NamedNever
{
	Method      method;
	const char *path;
};
const NamedNever kPinnedNever[] = {
	{ Method::Post,   "/api/v1/system/reboot" },
	{ Method::Post,   "/api/v1/system/shutdown" },
	{ Method::Post,   "/api/v1/system/restart" },
	{ Method::Post,   "/api/v1/daemons/{name}/start" },
	{ Method::Post,   "/api/v1/daemons/{name}/stop" },
	{ Method::Post,   "/api/v1/daemons/{name}/restart" },
	{ Method::Post,   "/api/v1/system/reload-setup" },
	{ Method::Post,   "/api/v1/tuner/reset" },
	{ Method::Post,   "/api/v1/settings/secret/clear" },
	{ Method::Put,    "/api/v1/system/webserver" },
	{ Method::Put,    "/api/v1/storage/netfs/{table}/{slot}" },
	{ Method::Delete, "/api/v1/storage/netfs/{table}/{slot}" },
	{ Method::Post,   "/api/v1/scripts/{name}" },
	{ Method::Post,   "/api/v1/osd/remote/key" },
	{ Method::Post,   "/api/v1/login" },
	{ Method::Post,   "/api/v1/logout" },
	{ Method::Post,   "/api/v1/token/media" },
};

} // namespace

TEST_CASE("a flag set that keeps every rule passes with standby included", "[mcp-guard]")
{
	std::string why;
	const bool passed = sane(kGood, why);
	INFO(why);
	REQUIRE(passed);
	REQUIRE(why.empty());
}

TEST_CASE("each rule refuses the flag set that breaks it", "[mcp-guard]")
{
	REFUSED_WITH(kAbsent, "does not carry");
	REFUSED_WITH(kPhantom, "counts tools it does not carry");
	REFUSED_WITH(kTwice, "named twice");
	REFUSED_WITH(kFlaggedTwice, "flagged twice");
	REFUSED_WITH(kBadName, "not a tool name");
	REFUSED_WITH(kMute, "says nothing");
	REFUSED_WITH(kLongDesc, "too long");
	REFUSED_WITH(kHeadMethod, "method");
	REFUSED_WITH(kPublic, "without a credential");
	REFUSED_WITH(kReboot, "never offered");
	REFUSED_WITH(kKey, "never offered");
	REFUSED_WITH(kTimerWrite, "never offered");
	REFUSED_WITH(kAi, "never offered");
	REFUSED_WITH(kWipe, "system level");
	REFUSED_WITH(kToken, "credential out of its address");
	REFUSED_WITH(kBytesBody, "body no argument can name");
	REFUSED_WITH(kSilentArg, "says nothing about itself");
	REFUSED_WITH(kSame, "two arguments of one name");
	REFUSED_WITH(kBlind, "declares no answer");
	REFUSED_WITH(kHollowAnswer, "answer that is wrong");
	REFUSED_WITH(kEmptyAccepted, "no document where it describes one");
	REFUSED_WITH(kEmptyDone, "no document where it describes one");
}

TEST_CASE("composed tools stay at eight or fewer all offered and under their prefix", "[mcp-guard]")
{
	std::string why;
	REQUIRE(mcp::kMaxComposedTools == 8);
	REQUIRE(composedSane(kEightComposed, why));
	REQUIRE_FALSE(composedSane(kNineComposed, why));
	REQUIRE(why.find("more composed tools than the ceiling of 8") != std::string::npos);
	REQUIRE_FALSE(composedSane(kUnoffered, why));
	REQUIRE(why.find("not offered") != std::string::npos);
	REQUIRE_FALSE(composedSane(kOutsideComposed, why));
	REQUIRE(why.find("/mcp/tools/") != std::string::npos);
	REQUIRE_FALSE(composedSane(kAnswerless, why));
	REQUIRE(why.find("declares no answer") != std::string::npos);
	REQUIRE_FALSE(composedSane(kDupeComposed, why));
	REQUIRE(why.find("composed table") != std::string::npos);
}

TEST_CASE("the never exposed set names routes the server ships", "[mcp-guard]")
{
	setRoutesForTest(NULL);
	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	size_t count = 0;
	const mcp::NeverExposed *never = mcp::neverExposedRoutes(&count);

	// The set is exactly the pinned rows, not merely a floor.
	REQUIRE(count == sizeof(kPinnedNever) / sizeof(kPinnedNever[0]));
	for (size_t p = 0; p < sizeof(kPinnedNever) / sizeof(kPinnedNever[0]); ++p)
	{
		INFO(methodName(kPinnedNever[p].method) << " " << kPinnedNever[p].path);
		bool found = false;
		for (size_t n = 0; n < count && !found; ++n)
			found = never[n].method == kPinnedNever[p].method &&
			        std::strcmp(never[n].path, kPinnedNever[p].path) == 0;
		REQUIRE(found);
		REQUIRE(mcp::neverExposed(kPinnedNever[p].method, kPinnedNever[p].path));
	}

	for (size_t n = 0; n < count; ++n)
	{
		INFO(methodName(never[n].method) << " " << never[n].path);
		bool shipped = false;
		for (size_t i = 0; i < tables && !shipped; ++i)
			for (size_t j = 0; j < t[i]->count && !shipped; ++j)
				shipped = t[i]->endpoints[j].method == never[n].method &&
				          std::strcmp(t[i]->endpoints[j].path, never[n].path) == 0;
		REQUIRE(shipped);
		REQUIRE(mcp::neverExposed(never[n].method, never[n].path));
	}
	REQUIRE_FALSE(mcp::neverExposed(Post, "/api/v1/system/standby"));
	REQUIRE_FALSE(mcp::neverExposed(Get, "/api/v1/system/reboot"));
}

TEST_CASE("every write under the timers and everything under the ai area is never offered", "[mcp-guard]")
{
	REQUIRE(mcp::neverExposed(Post, "/api/v1/timers"));
	REQUIRE(mcp::neverExposed(Patch, "/api/v1/timers/{id}"));
	REQUIRE(mcp::neverExposed(Delete, "/api/v1/timers/{id}"));
	REQUIRE(mcp::neverExposed(Put, "/api/v1/timers/{id}/anything"));
	REQUIRE_FALSE(mcp::neverExposed(Get, "/api/v1/timers"));
	REQUIRE_FALSE(mcp::neverExposed(Post, "/api/v1/timersx"));

	REQUIRE(mcp::neverExposed(Get, "/api/v1/ai"));
	REQUIRE(mcp::neverExposed(Get, "/api/v1/ai/settings"));
	REQUIRE(mcp::neverExposed(Put, "/api/v1/ai/settings"));
	REQUIRE(mcp::neverExposed(Get, "/api/v1/ai/clients"));
	REQUIRE(mcp::neverExposed(Delete, "/api/v1/ai/clients/{id}"));
	REQUIRE_FALSE(mcp::neverExposed(Get, "/api/v1/aim"));

	setRoutesForTest(NULL);
	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	size_t writes = 0;
	for (size_t i = 0; i < tables; ++i)
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			const Endpoint &ep = t[i]->endpoints[j];
			if (std::strncmp(ep.path, "/api/v1/timers", 14) != 0 || ep.method == Get)
				continue;
			INFO(methodName(ep.method) << " " << ep.path);
			REQUIRE(mcp::neverExposed(ep.method, ep.path));
			++writes;
		}
	REQUIRE(writes >= 3);
}

TEST_CASE("a gated route passes the system rule and no other system write does", "[mcp-guard][gated]")
{
	std::string why;
	const bool passed = sane(kGatedTable, why);
	INFO(why);
	REQUIRE(passed);
	REFUSED_WITH(kWipe, "system level");
}

TEST_CASE("plugin start and settings write are gated and nothing else is", "[mcp-guard][gated]")
{
	size_t n = 0;
	const mcp::NeverExposed *g = mcp::gatedRoutes(&n);
	REQUIRE(n == 2);
	REQUIRE(g[0].method == Patch);
	REQUIRE(std::string(g[0].path) == "/api/v1/settings/{section}");
	REQUIRE(g[1].method == Post);
	REQUIRE(std::string(g[1].path) == "/api/v1/plugins/{name}/start");
	REQUIRE(mcp::gated(Patch, "/api/v1/settings/{section}"));
	REQUIRE_FALSE(mcp::neverExposed(Patch, "/api/v1/settings/{section}"));
	REQUIRE(mcp::neverExposed(Post, "/api/v1/osd/remote/key"));
	REQUIRE_FALSE(mcp::gated(Post, "/api/v1/osd/remote/key"));
}
