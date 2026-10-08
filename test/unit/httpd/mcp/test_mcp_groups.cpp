/*
 * test_mcp_groups.cpp - which group each tool is dealt to, and what a group offers
 *
 * Copyright (C) 2026 NI-Team
 *
 * License: GPL-2.0-or-later, see COPYING.
 */

#include "support/catch.hpp"

#include "httpd/mcp/allowlist.h"
#include "httpd/mcp/contract.h"
#include "httpd/mcp/endpoint.h"
#include "httpd/mcp/mcpfakes.h"
#include "httpd/mcp/toolgroups.h"
#include "httpd/mcp/wiring.h"

#include "toolcaller.h"

#include <string>
#include <vector>

using namespace httpd;

namespace
{

mcp::ToolDef def(const char *name, AuthLevel level)
{
	mcp::ToolDef d;
	d.name = name;
	d.level = level;
	return d;
}


} // namespace

TEST_CASE("the group table is the eight groups in their order with fixed bits", "[groups]")
{
	size_t n = 0;
	const mcp::ToolGroup *g = mcp::toolGroups(&n);
	REQUIRE(n == 8);
	const char *keys[] = { "programme", "timers", "recordings", "control", "bouquets", "status", "settings", "plugins" };
	unsigned all = 0;
	size_t tools = 0;
	for (size_t i = 0; i < n; ++i)
	{
		REQUIRE(std::string(g[i].key) == keys[i]);
		REQUIRE(g[i].bit == (1u << i));
		all |= g[i].bit;
		tools += g[i].tool_count;
	}
	REQUIRE(all == mcp::kAllGroups);
	REQUIRE(tools == kOfferedTools);
	REQUIRE(mcp::kDefaultGroups == 7u);
	REQUIRE(g[6].least == AuthLevel::System);
	REQUIRE(g[7].least == AuthLevel::System);
	REQUIRE(g[0].least == AuthLevel::Read);
}

TEST_CASE("a tool name finds its one group", "[groups]")
{
	REQUIRE(mcp::groupOfTool("whats_on") == mcp::GroupProgramme);
	REQUIRE(mcp::groupOfTool("now_playing") == mcp::GroupProgramme);
	REQUIRE(mcp::groupOfTool("change_timer") == mcp::GroupTimers);
	REQUIRE(mcp::groupOfTool("delete_recording") == mcp::GroupRecordings);
	REQUIRE(mcp::groupOfTool("screenshot") == mcp::GroupControl);
	REQUIRE(mcp::groupOfTool("list_bouquets") == mcp::GroupBouquets);
	REQUIRE(mcp::groupOfTool("read_settings") == mcp::GroupStatus);
	REQUIRE(mcp::groupOfTool("write_settings") == mcp::GroupSettings);
	REQUIRE(mcp::groupOfTool("start_plugin") == mcp::GroupPlugins);
	REQUIRE(mcp::groupOfTool("remote_key") == 0u);
	REQUIRE(mcp::groupByKey("bouquets")->bit == mcp::GroupBouquets);
	REQUIRE(mcp::groupByKey("Bouquets") == NULL);
}

TEST_CASE("group keys are read and written in table order", "[groups]")
{
	unsigned bits = 99;
	REQUIRE(mcp::readGroupKeys("timers programme", &bits));
	REQUIRE(bits == (mcp::GroupProgramme | mcp::GroupTimers));
	REQUIRE(mcp::readGroupKeys("", &bits));
	REQUIRE(bits == 0u);
	REQUIRE(mcp::readGroupKeys("programme programme", &bits));
	REQUIRE(bits == mcp::GroupProgramme);
	bits = 5;
	REQUIRE_FALSE(mcp::readGroupKeys("programme,timers", &bits));
	REQUIRE_FALSE(mcp::readGroupKeys("programme nonsense", &bits));
	REQUIRE(bits == 5u);
	const std::vector<std::string> k = mcp::groupKeys(mcp::GroupStatus | mcp::GroupProgramme);
	REQUIRE(k.size() == 2);
	REQUIRE(k[0] == "programme");
	REQUIRE(k[1] == "status");
}

TEST_CASE("the table is checked against a list of tools both ways", "[groups]")
{
	std::vector<mcp::ToolDef> tools;
	tools.push_back(def("whats_on", AuthLevel::Read));
	tools.push_back(def("not_in_any", AuthLevel::Read));
	REQUIRE(mcp::toolOutsideGroups(tools) == "not_in_any");
	tools.pop_back();
	REQUIRE(mcp::toolOutsideGroups(tools).empty());
	REQUIRE_FALSE(mcp::groupNameWithoutTool(tools).empty());
	tools.push_back(def("write_settings", AuthLevel::Write));
	REQUIRE(mcp::groupLeastIsRight(tools).find("settings") != std::string::npos);
}

TEST_CASE("every tool this box offers is in exactly one group", "[groups][box]")
{
	REQUIRE(mcp::boxToolsRefusal().empty());
	REQUIRE(mcp::toolOutsideGroups(mcp::boxTools().list()).empty());
}

TEST_CASE("every group names only tools the box offers and every tool is in one group", "[groups][box]")
{
	const std::vector<mcp::ToolDef> all = mcp::boxTools().list();
	REQUIRE(mcp::boxToolsRefusal().empty());
	REQUIRE(all.size() == kOfferedTools);
	const std::string outside = mcp::toolOutsideGroups(all);
	const std::string without = mcp::groupNameWithoutTool(all);
	const std::string least = mcp::groupLeastIsRight(all);
	CAPTURE(outside);
	CAPTURE(without);
	CAPTURE(least);
	REQUIRE(outside.empty());
	REQUIRE(without.empty());
	REQUIRE(least.empty());
}

namespace
{

// Two tools in two groups; the token decides which groups it carries.
class GroupedTools : public mcp::ToolSource
{
	public:
		std::vector<mcp::ToolDef> list()
		{
			std::vector<mcp::ToolDef> out;
			mcp::ToolDef a = mcpfake::tool("whats_on", AuthLevel::Read, true, false, true, "{\"type\":\"object\"}", "");
			a.group = mcp::GroupProgramme;
			mcp::ToolDef b = mcpfake::tool("set_standby", AuthLevel::System, false, false, false, "{\"type\":\"object\"}", "");
			b.group = mcp::GroupControl;
			out.push_back(a);
			out.push_back(b);
			return out;
		}
		coreapi::Result<mcp::JsonText> call(const mcp::Caller &, const std::string &, const mcp::JsonText &)
		{
			++calls;
			return coreapi::ok(std::string("{\"ok\":true}"));
		}
		std::string hint(const std::string &, coreapi::ErrorCode) const { return std::string(); }
		int calls = 0;
};

coreapi::Result<mcp::Caller> verifyGrouped(const std::string &bearer, Origin origin, const std::string &resource)
{
	const bool narrowed = bearer == "tok-programme";
	coreapi::Result<mcp::Caller> c = mcpfake::verify(narrowed ? "tok-read" : bearer, origin, resource);
	if (!c.ok())
		return c;
	mcp::Caller one = c.value();
	if (narrowed)
		one.groups = mcp::GroupProgramme;
	else if (bearer == "tok-system")
		one.groups = mcp::GroupControl;
	return coreapi::ok(one);
}

struct Grouped
{
	mcpfake::Wired wired;
	GroupedTools   tools;
	Grouped()
	{
		const mcp::Wiring w = { &tools, &verifyGrouped };
		mcp::install(w);
	}
};

std::string listNames(const std::string &token)
{
	const Response r = mcpfake::roundTrip(mcpfake::modernHead("tools/list", std::string(), token),
	                                      mcpfake::modernBody("1", "tools/list", ""));
	const mcp::JsonValue v = mcpfake::parsed(r.body);
	std::string names;
	for (unsigned i = 0; i < v["result"]["tools"].size(); ++i)
		names += v["result"]["tools"][i]["name"].asString() + " ";
	return names;
}

Response call(const std::string &tool, const std::string &token)
{
	return mcpfake::roundTrip(mcpfake::modernHead("tools/call", tool, token),
	                          mcpfake::modernBody("1", "tools/call", "\"name\":\"" + tool + "\""));
}

} // namespace

TEST_CASE("tools/list offers only the tools of the token's groups", "[groups][endpoint]")
{
	Grouped g;
	REQUIRE(listNames("tok-programme") == "whats_on ");
	REQUIRE(listNames("tok-system") == "set_standby ");
}

TEST_CASE("a tool outside the token's groups is refused as a tool error naming its group", "[groups][endpoint]")
{
	Grouped g;
	const Response r = call("set_standby", "tok-programme");
	REQUIRE(r.code == 200);
	const mcp::JsonValue v = mcpfake::parsed(r.body);
	REQUIRE(v["result"]["isError"].asBool());
	const std::string text = v["result"]["content"][0]["text"].asString();
	REQUIRE(text.find("group-not-enabled") != std::string::npos);
	REQUIRE(text.find("control") != std::string::npos);
	REQUIRE(text.find("KI tab") != std::string::npos);
	REQUIRE(g.tools.calls == 0);
}

TEST_CASE("a tool that does not exist is still an unknown tool", "[groups][endpoint]")
{
	Grouped g;
	REQUIRE(mcpfake::errorCode(call("reboot", "tok-programme")) == mcp::kInvalidParams);
}

TEST_CASE("a tool in an enabled group above the scope still asks to step up", "[groups][endpoint]")
{
	Grouped g;
	const Response r = call("set_standby", "tok-read");
	REQUIRE(r.code == 403);
}

TEST_CASE("the box stamps each tool with its group", "[groups][box]")
{
	const std::vector<mcp::ToolDef> all = mcp::boxTools().list();
	for (size_t i = 0; i < all.size(); ++i)
	{
		INFO(all[i].name);
		REQUIRE(all[i].group == mcp::groupOfTool(all[i].name));
		REQUIRE(all[i].group != 0u);
	}
}

TEST_CASE("a tool definition's size is what tools/list writes for it", "[groups][endpoint]")
{
	mcp::ToolDef d = mcpfake::tool("whats_on", AuthLevel::Read, true, false, true, "{\"type\":\"object\"}", "");
	REQUIRE(mcp::toolDefinitionBytes(d) > 100);
	d.description += std::string(400, 'x');
	REQUIRE(mcp::toolDefinitionBytes(d) > 500);
}
