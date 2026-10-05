/*
 * test_mcp_composed_write.cpp - recording a programme, switching a channel, timers
 *
 * Copyright (C) 2026 NI-Team
 *
 * License: GPL-2.0-or-later, see COPYING.
 */

#include "support/catch.hpp"
#include "support/fakes.h"

#include "httpd/endpoint.h"
#include "httpd/mcp/composed.h"
#include "httpd/mcp/routetools.h"

#include "coreapi/archive.h"
#include "coreapi/base/errors.h"

#include <neutrinoMessages.h>
#include <timerdclient/timerdtypes.h>

#include "jsoncpp/json/json.h"

#include "toolcaller.h"

#include <cstring>
#include <string>
#include <vector>

using namespace httpd;

namespace
{

const coreapi::ChannelId ERSTE    = 0x00010001aaaa0001ULL;
const coreapi::ChannelId ERSTE_HD = 0x00010001aaaa0002ULL;
const time_t T0 = 1790000000;

coreapi::ChannelInfo channel(coreapi::ChannelId id, const char *name, int32_t number)
{
	coreapi::ChannelInfo c;
	c.id = id;
	c.epg_id = id;
	c.number = number;
	c.name = name;
	c.kind = coreapi::ServiceKind::Tv;
	return c;
}

coreapi::EventDetail detail(uint64_t id, const char *title, time_t start, unsigned duration)
{
	coreapi::EventDetail d;
	d.event_id = id;
	d.channel_id = ERSTE;
	d.title = title;
	d.start = start;
	d.duration = duration;
	return d;
}

// The box at one moment, its timer daemon and the tools reading the same clock.
struct Box
{
	InstalledDependencies deps;
	ToolClock clock;

	explicit Box(time_t now) : clock(now)
	{
		deps.channels.channels.push_back(channel(ERSTE, "Das Erste", 1));
		deps.channels.channels.push_back(channel(ERSTE_HD, "Das Erste HD", 0));
		deps.epg.details.push_back(detail(0x102, "Tatort", T0, 5400));
		deps.timers.clock = now;
	}
};

::Json::Value ok(const char *tool, const std::string &args, const char *path)
{
	return composedAnswer(AuthLevel::Write, tool, args, path);
}

coreapi::Error refused(const char *tool, const std::string &args, AuthLevel l = AuthLevel::Write)
{
	return composedRefusal(l, tool, args);
}

coreapi::TimerInfo timer(uint32_t id, coreapi::TimerType type, const char *title)
{
	coreapi::TimerInfo t;
	t.id = id;
	t.type = (int) type;
	t.channel_id = ERSTE;
	t.start = T0;
	t.stop = T0 + 3600;
	t.title = title;
	return t;
}

CTimerd::EventInfo takePayload(neutrino_msg_data_t data)
{
	unsigned char *payload = (unsigned char *) data;
	CTimerd::EventInfo info;
	std::memcpy(&info, payload, sizeof(info));
	delete[] payload;
	return info;
}

const char kTatort[] = "{\"id\":\"102\",\"start\":1790000000}";

} // namespace

TEST_CASE("a programme still to come is recorded from its start to its end", "[mcp-composed-write]")
{
	Box box(T0 - 3600);
	InstalledDependencies &deps = box.deps;
	const ::Json::Value v = ok("record_programme", kTatort, "/mcp/tools/record_programme");
	REQUIRE(v["already_scheduled"].asBool() == false);
	REQUIRE(v["started_now"].asBool() == false);
	REQUIRE(v["channel"].asString() == "Das Erste");
	REQUIRE(deps.timers.timers.size() == 1);
	const coreapi::TimerInfo &t = deps.timers.timers[0];
	REQUIRE(t.type == (int) coreapi::TimerType::Record);
	REQUIRE(t.channel_id == ERSTE);
	REQUIRE(t.start == T0);
	REQUIRE(t.stop == T0 + 5400);
	REQUIRE(t.epg_id == 0x102);
	REQUIRE(t.epg_start == T0);
	REQUIRE(t.recording_safety);
	REQUIRE(t.title == "Tatort");
	REQUIRE(v["timer_id"].asUInt() == t.id);
}

TEST_CASE("margins can be left off", "[mcp-composed-write]")
{
	Box box(T0 - 3600);
	(void) ok("record_programme", "{\"id\":\"102\",\"start\":1790000000,\"margins\":false}", "/mcp/tools/record_programme");
	REQUIRE_FALSE(box.deps.timers.timers[0].recording_safety);
}

TEST_CASE("a programme that is running is recorded from now", "[mcp-composed-write]")
{
	Box box(T0 + 600);
	InstalledDependencies &deps = box.deps;
	const ::Json::Value v = ok("record_programme", kTatort, "/mcp/tools/record_programme");
	REQUIRE(v["started_now"].asBool() == true);
	REQUIRE(v["start"].asInt64() == T0 + 600);
	REQUIRE(deps.timers.timers.size() == 1);
	REQUIRE(deps.timers.timers[0].start == T0 + 600);
	REQUIRE(deps.timers.timers[0].stop == T0 + 5400);
	REQUIRE(deps.timers.timers[0].epg_start == T0);
}

TEST_CASE("a programme that has ended is refused and nothing is made", "[mcp-composed-write]")
{
	Box box(T0 + 5400);
	const coreapi::Error e = refused("record_programme", kTatort);
	REQUIRE(e.code == coreapi::ErrorCode::TimerInThePast);
	REQUIRE(e.status == coreapi::Status::InvalidArgument);
	REQUIRE(box.deps.timers.timers.empty());
}

TEST_CASE("asking twice records once and says so", "[mcp-composed-write]")
{
	Box box(T0 - 3600);
	const ::Json::Value first = ok("record_programme", kTatort, "/mcp/tools/record_programme");
	const ::Json::Value second = ok("record_programme", kTatort, "/mcp/tools/record_programme");
	REQUIRE(box.deps.timers.timers.size() == 1);
	REQUIRE(second["already_scheduled"].asBool() == true);
	REQUIRE(second["timer_id"].asUInt() == first["timer_id"].asUInt());
}

TEST_CASE("record_programme refuses what it cannot do", "[mcp-composed-write]")
{
	Box box(T0 - 3600);
	REQUIRE(refused("record_programme", "{\"id\":\"999\",\"start\":1790000000}").code
		== coreapi::ErrorCode::NoSuchEvent);
	REQUIRE(refused("record_programme", kTatort, AuthLevel::Read).code == coreapi::ErrorCode::NotPermitted);
	REQUIRE(refused("record_programme", "{\"id\":258,\"start\":1790000000}").code == coreapi::ErrorCode::BadString);
	REQUIRE(box.deps.timers.timers.empty());
}

TEST_CASE("switch_channel switches by name", "[mcp-composed-write]")
{
	Box box(T0);
	InstalledDependencies &deps = box.deps;
	deps.channels.mode = NeutrinoModes::mode_tv;
	const ::Json::Value v = ok("switch_channel", "{\"channel\":\"das erste\"}", "/mcp/tools/switch_channel");
	REQUIRE(v["channel"].asString() == "Das Erste");
	REQUIRE(v["number"].asInt() == 1);
	REQUIRE(deps.commands.posted.size() == 1);
	REQUIRE(deps.commands.posted[0].first == NeutrinoMessages::ZAPTO);
	REQUIRE(takePayload(deps.commands.posted[0].second).channel_id == ERSTE);
}

TEST_CASE("switch_channel wakes the box only when asked", "[mcp-composed-write]")
{
	Box box(T0);
	InstalledDependencies &deps = box.deps;
	deps.channels.mode = NeutrinoModes::mode_standby;
	REQUIRE(refused("switch_channel", "{\"channel\":\"das erste\"}").code == coreapi::ErrorCode::BoxInStandby);
	REQUIRE(deps.commands.posted.empty());
	(void) ok("switch_channel", "{\"channel\":\"das erste\",\"wake\":true}", "/mcp/tools/switch_channel");
	REQUIRE(deps.commands.posted.size() == 1);
	(void) takePayload(deps.commands.posted[0].second);
}

TEST_CASE("switch_channel ends a file playing only when asked", "[mcp-composed-write]")
{
	Box box(T0);
	InstalledDependencies &deps = box.deps;
	deps.channels.mode = NeutrinoModes::mode_ts;
	coreapi::archive::notePlaying("/media/hdd/movie/one.ts");
	const coreapi::ErrorCode busy = refused("switch_channel", "{\"channel\":\"das erste\"}").code;
	const size_t before = deps.commands.posted.size();
	(void) ok("switch_channel", "{\"channel\":\"das erste\",\"stop_playback\":true}", "/mcp/tools/switch_channel");
	coreapi::archive::notePlaying(std::string());

	REQUIRE(busy == coreapi::ErrorCode::PlaybackRunning);
	REQUIRE(before == 0);
	REQUIRE(deps.commands.posted.size() == 1);
	REQUIRE(takePayload(deps.commands.posted[0].second).channel_id == ERSTE);
}

TEST_CASE("set_timer makes a recording a zap and a reminder of the right kind", "[mcp-composed-write]")
{
	Box box(T0 - 3600);
	InstalledDependencies &deps = box.deps;
	const ::Json::Value rec = ok("set_timer",
		"{\"kind\":\"record\",\"channel\":\"das erste\",\"start\":1790000000,\"end\":1790003600}",
		"/mcp/tools/set_timer");
	REQUIRE(rec["kind"].asString() == "record");
	REQUIRE(rec["end"].asInt64() == T0 + 3600);
	const ::Json::Value zap = ok("set_timer",
		"{\"kind\":\"zap\",\"channel\":\"Das Erste\",\"start\":1790000000}", "/mcp/tools/set_timer");
	REQUIRE(zap["kind"].asString() == "zap");
	REQUIRE_FALSE(zap.isMember("end"));
	const ::Json::Value remind = ok("set_timer",
		"{\"kind\":\"reminder\",\"channel\":\"0x00010001aaaa0001\",\"start\":1790000000,\"message\":\"Tatort beginnt\"}",
		"/mcp/tools/set_timer");
	REQUIRE(remind["channel"].asString() == "Das Erste");

	REQUIRE(deps.timers.timers.size() == 3);
	const coreapi::TimerInfo &r = deps.timers.timers[0];
	REQUIRE(r.type == (int) coreapi::TimerType::Record);
	REQUIRE(r.channel_id == ERSTE);
	REQUIRE(r.start == T0);
	REQUIRE(r.stop == T0 + 3600);
	REQUIRE(r.recording_safety);
	REQUIRE(rec["timer_id"].asUInt() == r.id);
	const coreapi::TimerInfo &z = deps.timers.timers[1];
	REQUIRE(z.type == (int) coreapi::TimerType::Zapto);
	REQUIRE(z.channel_id == ERSTE);
	REQUIRE(z.start == T0);
	REQUIRE(z.stop == 0);
	const coreapi::TimerInfo &m = deps.timers.timers[2];
	REQUIRE(m.type == (int) coreapi::TimerType::Remind);
	REQUIRE(m.channel_id == ERSTE);
	REQUIRE(m.title == "Tatort beginnt");
	REQUIRE_FALSE(m.recording_safety);
}

TEST_CASE("set_timer leaves the margins off a recording when asked", "[mcp-composed-write]")
{
	Box box(T0 - 3600);
	(void) ok("set_timer",
		"{\"kind\":\"record\",\"channel\":\"das erste\",\"start\":1790000000,\"end\":1790003600,\"margins\":false}",
		"/mcp/tools/set_timer");
	REQUIRE(box.deps.timers.timers.size() == 1);
	REQUIRE_FALSE(box.deps.timers.timers[0].recording_safety);
}

TEST_CASE("set_timer refuses a start in the past and makes nothing", "[mcp-composed-write]")
{
	Box box(T0);
	const coreapi::Error e =
		refused("set_timer", "{\"kind\":\"zap\",\"channel\":\"das erste\",\"start\":1789996400}");
	REQUIRE(e.code == coreapi::ErrorCode::TimerInThePast);
	REQUIRE(e.status == coreapi::Status::InvalidArgument);
	REQUIRE(box.deps.timers.timers.empty());
}

TEST_CASE("set_timer needs an end for a recording and a message for a reminder", "[mcp-composed-write]")
{
	Box box(T0 - 3600);
	REQUIRE(refused("set_timer", "{\"kind\":\"record\",\"channel\":\"das erste\",\"start\":1790000000}").code
		== coreapi::ErrorCode::MissingParameter);
	REQUIRE(refused("set_timer", "{\"kind\":\"reminder\",\"channel\":\"das erste\",\"start\":1790000000}").code
		== coreapi::ErrorCode::MissingParameter);
	REQUIRE(refused("set_timer",
		"{\"kind\":\"reminder\",\"channel\":\"das erste\",\"start\":1790000000,\"message\":\"\"}").code
		== coreapi::ErrorCode::MissingParameter);
	REQUIRE(refused("set_timer",
		"{\"kind\":\"zap\",\"channel\":\"das erste\",\"start\":1790000000,\"end\":1790003600}").code
		== coreapi::ErrorCode::NoSuchParameter);
	REQUIRE(refused("set_timer",
		"{\"kind\":\"record\",\"channel\":\"das erste\",\"start\":1790000000,\"end\":1790003600,\"message\":\"x\"}").code
		== coreapi::ErrorCode::NoSuchParameter);
	REQUIRE(refused("set_timer",
		"{\"kind\":\"zap\",\"channel\":\"das erste\",\"start\":1790000000,\"margins\":true}").code
		== coreapi::ErrorCode::NoSuchParameter);
	REQUIRE(refused("set_timer",
		"{\"kind\":\"reminder\",\"channel\":\"das erste\",\"start\":1790000000,\"message\":\"x\",\"margins\":false}").code
		== coreapi::ErrorCode::NoSuchParameter);
	REQUIRE(box.deps.timers.timers.empty());
}

TEST_CASE("set_timer offers no kind but record zap and reminder", "[mcp-composed-write]")
{
	Box box(T0 - 3600);
	static const char *const kOther[] = {
		"shutdown", "standby", "exec-plugin", "sleep", "immediate-record", "remind", "Record",
	};
	for (size_t i = 0; i < sizeof(kOther) / sizeof(kOther[0]); ++i)
	{
		const std::string args = std::string("{\"kind\":\"") + kOther[i] +
			"\",\"channel\":\"das erste\",\"start\":1790000000}";
		INFO(args);
		REQUIRE(refused("set_timer", args).code == coreapi::ErrorCode::BadEnum);
	}
	REQUIRE(box.deps.timers.timers.empty());

	mcp::RouteTools tools(NULL, 0, mcp::composedTable());
	const std::vector<mcp::ToolDef> all = tools.list();
	bool found = false;
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].name != "set_timer")
			continue;
		found = true;
		REQUIRE(all[i].level == AuthLevel::Write);
		::Json::Value in;
		::Json::Reader reader;
		REQUIRE(reader.parse(all[i].input, in));
		const ::Json::Value &kinds = in["properties"]["kind"]["enum"];
		REQUIRE(kinds.size() == 3);
		REQUIRE(kinds[0].asString() == "record");
		REQUIRE(kinds[1].asString() == "zap");
		REQUIRE(kinds[2].asString() == "reminder");
		const std::string words = in["properties"]["kind"]["description"].asString();
		REQUIRE(words.find("\n- `zap`: switches the box to the channel at start\n") != std::string::npos);
	}
	REQUIRE(found);
}

TEST_CASE("a composed body example is a request its tool accepts", "[mcp-composed-write]")
{
	Box box(T0 - 3600);
	InstalledDependencies &deps = box.deps;
	coreapi::TimerInfo held = timer(4, coreapi::TimerType::Record, "Tatort");
	held.stop = 2100000000;
	deps.timers.timers.push_back(held);

	const RouteTable &t = mcp::composedTable();
	size_t examples = 0;
	for (size_t i = 0; i < t.count; ++i)
	{
		if (t.endpoints[i].body_example == NULL)
			continue;
		const std::string path = t.endpoints[i].path;
		INFO(path);
		std::string args = t.endpoints[i].body_example;
		// A path segment is the example's own id, not one of its body fields.
		if (path.find("{id}") != std::string::npos)
			args.insert(1, "\"id\":4,");
		(void) ok(t.tools[i].name, args, path.c_str());
		++examples;
	}
	REQUIRE(examples == 2);
	REQUIRE(deps.timers.timers.size() == 2);
}

TEST_CASE("set_timer refuses a channel name that fits several", "[mcp-composed-write]")
{
	Box box(T0 - 3600);
	const coreapi::Error e =
		refused("set_timer", "{\"kind\":\"zap\",\"channel\":\"erste\",\"start\":1790000000}");
	REQUIRE(e.code == coreapi::ErrorCode::AmbiguousChannel);
	REQUIRE(e.message.find("Das Erste HD") != std::string::npos);
	REQUIRE(box.deps.timers.timers.empty());
}

TEST_CASE("remove_timer removes a recording a zap and a reminder", "[mcp-composed-write]")
{
	Box box(T0 - 3600);
	InstalledDependencies &deps = box.deps;
	deps.timers.timers.push_back(timer(4, coreapi::TimerType::Record, "Tatort"));
	deps.timers.timers.push_back(timer(5, coreapi::TimerType::ImmediateRecord, "Tagesschau"));
	deps.timers.timers.push_back(timer(6, coreapi::TimerType::Zapto, "Sportschau"));
	deps.timers.timers.push_back(timer(7, coreapi::TimerType::Remind, "Tatort beginnt"));

	const ::Json::Value rec = ok("remove_timer", "{\"id\":4}", "/mcp/tools/remove_timer/{id}");
	REQUIRE(rec["timer_id"].asUInt() == 4);
	REQUIRE(rec["kind"].asString() == "record");
	REQUIRE(rec["title"].asString() == "Tatort");
	REQUIRE(rec["start"].asInt64() == T0);
	REQUIRE(ok("remove_timer", "{\"id\":5}", "/mcp/tools/remove_timer/{id}")["kind"].asString() == "record");
	REQUIRE(ok("remove_timer", "{\"id\":6}", "/mcp/tools/remove_timer/{id}")["kind"].asString() == "zap");
	REQUIRE(ok("remove_timer", "{\"id\":7}", "/mcp/tools/remove_timer/{id}")["kind"].asString() == "reminder");
	REQUIRE(deps.timers.timers.empty());
	REQUIRE(deps.timers.removals == 4);
}

TEST_CASE("remove_timer refuses every other kind and leaves it", "[mcp-composed-write]")
{
	Box box(T0 - 3600);
	InstalledDependencies &deps = box.deps;
	deps.timers.timers.push_back(timer(3, coreapi::TimerType::Shutdown, ""));
	deps.timers.timers.push_back(timer(8, coreapi::TimerType::Standby, ""));
	deps.timers.timers.push_back(timer(9, coreapi::TimerType::ExecPlugin, "backup"));
	deps.timers.timers.push_back(timer(10, coreapi::TimerType::Sleeptimer, ""));
	coreapi::TimerInfo foreign = timer(11, coreapi::TimerType::Record, "");
	foreign.type = 42;
	deps.timers.timers.push_back(foreign);

	static const char *const kArgs[] = {
		"{\"id\":3}", "{\"id\":8}", "{\"id\":9}", "{\"id\":10}", "{\"id\":11}",
	};
	for (size_t i = 0; i < sizeof(kArgs) / sizeof(kArgs[0]); ++i)
	{
		INFO(kArgs[i]);
		const coreapi::Error e = refused("remove_timer", kArgs[i]);
		REQUIRE(e.code == coreapi::ErrorCode::NotPermitted);
		REQUIRE(e.status == coreapi::Status::Denied);
	}
	REQUIRE(deps.timers.timers.size() == 5);
	REQUIRE(deps.timers.removals == 0);
}

TEST_CASE("remove_timer names an id the box does not hold", "[mcp-composed-write]")
{
	Box box(T0 - 3600);
	InstalledDependencies &deps = box.deps;
	deps.timers.timers.push_back(timer(4, coreapi::TimerType::Record, "Tatort"));
	const coreapi::Error e = refused("remove_timer", "{\"id\":99}");
	REQUIRE(e.code == coreapi::ErrorCode::NoSuchTimer);
	REQUIRE(e.status == coreapi::Status::NotFound);
	REQUIRE(refused("remove_timer", "{\"id\":4}", AuthLevel::Read).code == coreapi::ErrorCode::NotPermitted);
	REQUIRE(deps.timers.timers.size() == 1);
	REQUIRE(deps.timers.removals == 0);
}

TEST_CASE("change_timer moves a recording and widens it", "[mcp-composed-write][change]")
{
	Box box(T0 - 7200);
	InstalledDependencies &deps = box.deps;
	deps.timers.timers.push_back(timer(4, coreapi::TimerType::Record, "Tatort"));
	const ::Json::Value v = ok("change_timer",
		"{\"id\":4,\"start\":" + std::to_string(T0 + 600) + ",\"end\":" + std::to_string(T0 + 4200) +
		",\"pad_before\":5,\"pad_after\":10}", "/mcp/tools/change_timer/{id}");
	REQUIRE(v["replaced"].asBool() == false);
	REQUIRE(v["kind"].asString() == "record");
	REQUIRE(v["repeat"].asString() == "once");
	REQUIRE(deps.timers.timers.size() == 1);
	const coreapi::TimerInfo &t = deps.timers.timers[0];
	REQUIRE(t.start == T0 + 600 - 300);
	REQUIRE(t.stop == T0 + 4200 + 600);
	REQUIRE(v["start"].asInt64() == t.start);
	REQUIRE(v["end"].asInt64() == t.stop);
}

TEST_CASE("change_timer sets a repeat by its name", "[mcp-composed-write][change]")
{
	Box box(T0 - 7200);
	InstalledDependencies &deps = box.deps;
	deps.timers.timers.push_back(timer(6, coreapi::TimerType::Zapto, "Sportschau"));
	REQUIRE(ok("change_timer", "{\"id\":6,\"repeat\":\"weekdays\"}", "/mcp/tools/change_timer/{id}")
		["repeat"].asString() == "weekdays");
	REQUIRE(deps.timers.timers[0].repeat == 256 + 512 + 1024 + 2048 + 4096 + 8192);
	(void) refused("change_timer", "{\"id\":6,\"repeat\":\"hourly\"}");
	REQUIRE(deps.timers.timers[0].repeat == 256 + 512 + 1024 + 2048 + 4096 + 8192);
}

TEST_CASE("change_timer moves a timer to another channel by making it anew", "[mcp-composed-write][change]")
{
	Box box(T0 - 7200);
	InstalledDependencies &deps = box.deps;
	coreapi::TimerInfo t = timer(4, coreapi::TimerType::Record, "Tatort");
	t.epg_id = 102;
	t.epg_start = T0;
	deps.timers.timers.push_back(t);
	const ::Json::Value v = ok("change_timer", "{\"id\":4,\"channel\":\"Das Erste HD\"}",
		"/mcp/tools/change_timer/{id}");
	REQUIRE(v["replaced"].asBool());
	REQUIRE(v["timer_id"].asUInt() != 4);
	REQUIRE(deps.timers.timers.size() == 1);
	REQUIRE(deps.timers.timers[0].id == v["timer_id"].asUInt());
	REQUIRE(deps.timers.timers[0].channel_id == ERSTE_HD);
	REQUIRE(deps.timers.timers[0].epg_id == 0);
	REQUIRE(deps.timers.timers[0].epg_start == 0);
	REQUIRE(deps.timers.timers[0].title.empty());
}

TEST_CASE("change_timer refuses a timer kind it must not touch and leaves it", "[mcp-composed-write][change]")
{
	Box box(T0 - 7200);
	InstalledDependencies &deps = box.deps;
	deps.timers.timers.push_back(timer(3, coreapi::TimerType::Shutdown, ""));
	const coreapi::Error e = refused("change_timer", "{\"id\":3,\"start\":" + std::to_string(T0 + 600) + "}");
	REQUIRE(e.code == coreapi::ErrorCode::NotPermitted);
	REQUIRE(e.status == coreapi::Status::Denied);
	REQUIRE(deps.timers.timers[0].start == T0);
}

TEST_CASE("change_timer keeps the channel of a recording that is running", "[mcp-composed-write][change]")
{
	Box box(T0 + 100);
	InstalledDependencies &deps = box.deps;
	coreapi::TimerInfo t = timer(4, coreapi::TimerType::Record, "Tatort");
	t.state = (int) CTimerd::TIMERSTATE_ISRUNNING;
	deps.timers.timers.push_back(t);
	const coreapi::Error e = refused("change_timer", "{\"id\":4,\"channel\":\"Das Erste HD\"}");
	REQUIRE(e.code == coreapi::ErrorCode::RecordingRunning);
	REQUIRE(e.status == coreapi::Status::Conflict);
	REQUIRE(deps.timers.timers.size() == 1);
	REQUIRE(deps.timers.timers[0].channel_id == ERSTE);
}

TEST_CASE("change_timer moves the channel of a repeating recording past its latest start",
          "[mcp-composed-write][change]")
{
	// Not running (state is still the default, scheduled) even though start lies behind
	// now: a daily repeat is not held to the once-timer past check either, so this is the
	// channel-move branch's own running check, not create()'s.
	Box box(T0 + 100);
	InstalledDependencies &deps = box.deps;
	coreapi::TimerInfo t = timer(4, coreapi::TimerType::Record, "Tatort");
	t.repeat = (int) CTimerd::TIMERREPEAT_DAILY;
	deps.timers.timers.push_back(t);
	const ::Json::Value v = ok("change_timer", "{\"id\":4,\"channel\":\"Das Erste HD\"}",
		"/mcp/tools/change_timer/{id}");
	REQUIRE(v["replaced"].asBool());
	REQUIRE(deps.timers.timers.size() == 1);
	REQUIRE(deps.timers.timers[0].channel_id == ERSTE_HD);
}

TEST_CASE("change_timer refuses a parameter a zap has no use for", "[mcp-composed-write][change]")
{
	Box box(T0 - 7200);
	InstalledDependencies &deps = box.deps;
	deps.timers.timers.push_back(timer(6, coreapi::TimerType::Zapto, "Sportschau"));
	REQUIRE(refused("change_timer", "{\"id\":6,\"end\":" + std::to_string(T0 + 900) + "}").code ==
		coreapi::ErrorCode::NoSuchParameter);
	REQUIRE(deps.timers.timers.size() == 1);
}

TEST_CASE("change_timer needs at least one thing named to change", "[mcp-composed-write][change]")
{
	Box box(T0 - 7200);
	InstalledDependencies &deps = box.deps;
	deps.timers.timers.push_back(timer(6, coreapi::TimerType::Zapto, "Sportschau"));
	REQUIRE(refused("change_timer", "{\"id\":6}").code == coreapi::ErrorCode::MissingParameter);
	REQUIRE(deps.timers.timers.size() == 1);
}

TEST_CASE("change_timer names an id the box does not hold", "[mcp-composed-write][change]")
{
	Box box(T0 - 7200);
	InstalledDependencies &deps = box.deps;
	deps.timers.timers.push_back(timer(6, coreapi::TimerType::Zapto, "Sportschau"));
	REQUIRE(refused("change_timer", "{\"id\":999999}").code == coreapi::ErrorCode::NoSuchTimer);
	REQUIRE(deps.timers.timers.size() == 1);
}

TEST_CASE("a channel move whose removal fails takes the new timer back", "[mcp-composed-write][change]")
{
	Box box(T0 - 7200);
	InstalledDependencies &deps = box.deps;
	deps.timers.timers.push_back(timer(4, coreapi::TimerType::Record, "Tatort"));
	const size_t before = deps.timers.timers.size();
	deps.timers.fail_next_remove = true;
	(void) refused("change_timer", "{\"id\":4,\"channel\":\"Das Erste HD\"}");
	REQUIRE(deps.timers.timers.size() == before);
	REQUIRE(deps.timers.timers[0].id == 4);
	REQUIRE(deps.timers.timers[0].channel_id == ERSTE);
}
