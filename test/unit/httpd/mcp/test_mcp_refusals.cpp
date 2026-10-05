/*
 * test_mcp_refusals.cpp - what a tool says can go wrong, and what to do then
 *
 * Copyright (C) 2026 NI-Team
 *
 * License: GPL-2.0-or-later, see COPYING.
 */

#include "support/catch.hpp"
#include "support/fakes.h"

#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/router.h"
#include "httpd/schema.h"
#include "httpd/mcp/composed.h"
#include "httpd/mcp/contract.h"
#include "httpd/mcp/problem.h"
#include "httpd/mcp/routetools.h"
#include "httpd/mcp/toolgen.h"
#include "httpd/mcp/toolguard.h"
#include "httpd/mcp/toolhint.h"
#include "httpd/mcp/toolschema.h"
#include "toolcaller.h"

#include "coreapi/archive.h"
#include "coreapi/base/errors.h"

#include <neutrinoMessages.h>
#include <timerdclient/timerdtypes.h>

#include "jsoncpp/json/json.h"

#include <cctype>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace httpd;

namespace
{

Response nothing(const Request &)
{
	return accepted();
}

const FieldDesc kEchoFields[] = {
	HTTPD_MEMBER("echo", FieldType::String, "what came back"),
};
const Schema kEcho = { "echo", HTTPD_FIELDS(kEchoFields) };

const Param kWakeParams[] = {
	HTTPD_BODY("wake", ParamType::Bool, "whether to wake the box"),
	HTTPD_BODY_IN("n", ParamType::Int, "a count", 0, 9),
};

const RouteRefusal kZapLike[] = {
	HTTPD_REFUSES(Conflict, BoxInStandby, "the box is in standby"),
	HTTPD_REFUSES(NotFound, NoSuchChannel, "no channel with that id"),
	HTTPD_REFUSES(Internal, ChangeRefused, "the box did not take it."),
};

const RouteRefusal kStandbyOnly[] = {
	HTTPD_REFUSES(Conflict, BoxInStandby, "the box is in standby"),
};

const Endpoint kRoutes[] = {
	{ Method::Post, "/api/v1/r/zap", AuthLevel::Write, "switches", NULL, HTTPD_PARAMS(kWakeParams), NULL, &nothing, false,
	  Answers202, HTTPD_REFUSALS(kZapLike) },
	{ Method::Post, "/api/v1/r/mode", AuthLevel::Write, "changes mode", NULL, NULL, 0, NULL, &nothing, false,
	  Answers202, HTTPD_REFUSALS(kStandbyOnly) },
	{ Method::Delete, "/api/v1/r/gone", AuthLevel::Write, "removes", NULL, NULL, 0, NULL, &nothing, false,
	  Answers204, HTTPD_NO_REFUSALS },
	{ Method::Put, "/api/v1/r/either", AuthLevel::Write, "sets", NULL, NULL, 0, NULL, &nothing, false,
	  Answers202 | Answers204, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/r/range", AuthLevel::Read, "a file", NULL, NULL, 0, &kEcho, &nothing, false,
	  Answers200 | Answers206, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/r/document", AuthLevel::Write, "answers a document", NULL, NULL, 0, NULL, &nothing, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/recordings/archive/{id}", AuthLevel::Read, "one archived thing", NULL, NULL, 0, NULL, &nothing, false,
	  Answers200, HTTPD_NO_REFUSALS },
};

const ToolFlag kTools[] = {
	HTTPD_TOOL_AS(Method::Post, "/api/v1/r/zap", "zap_like", NULL),
	HTTPD_TOOL_AS(Method::Post, "/api/v1/r/mode", "mode_like", NULL),
	HTTPD_TOOL_AS(Method::Delete, "/api/v1/r/gone", "gone_like", NULL),
};
const RouteTable kTable = { HTTPD_TABLE_WITH_TOOLS("r", kRoutes, kTools) };
const RouteTable *const kTables[] = { &kTable };
const RouteTable kNoComposed = { HTTPD_TABLE_N("composed", (const Endpoint *) NULL, 0) };

const ToolFlag kRangeTool[] = { HTTPD_TOOL_AS(Method::Get, "/api/v1/r/range", "range_like", NULL) };
const RouteTable kRangeTable = { HTTPD_TABLE_WITH_TOOLS("r", kRoutes, kRangeTool) };
const ToolFlag kDocumentTool[] = { HTTPD_TOOL_AS(Method::Post, "/api/v1/r/document", "document_like", NULL) };
const RouteTable kDocumentTable = { HTTPD_TABLE_WITH_TOOLS("r", kRoutes, kDocumentTool) };

std::string statusEnum(unsigned answers)
{
	std::string text;
	mcp::appendDoneSchema(text, answers);
	::Json::Value v;
	::Json::Reader r;
	REQUIRE(r.parse(text, v));
	std::string out;
	const ::Json::Value &e = v["properties"]["status"]["enum"];
	for (::Json::ArrayIndex i = 0; i < e.size(); ++i)
		out += (i > 0 ? "," : "") + e[i].asString();
	return out;
}

size_t occurrences(const std::string &text, const std::string &word)
{
	size_t n = 0;
	for (size_t at = text.find(word); at != std::string::npos; at = text.find(word, at + 1))
		++n;
	return n;
}

} // namespace

TEST_CASE("a tool lists the route's own refusals with what to do about them", "[mcp-refusals]")
{
	const std::string d = mcp::describedTool(kRoutes[0], kTools[0]);
	INFO(d);
	REQUIRE(d.compare(0, 8, "switches") == 0);
	REQUIRE(d.find("\n\nRefusals:\n- box-in-standby: the box is in standby. Retry with wake true.\n") != std::string::npos);
	REQUIRE(d.find("- no-such-channel: no channel with that id. Look the channel up with list_channels") != std::string::npos);
	// No second full stop, and no hint where none helps.
	const std::string last = "\n- change-refused: the box did not take it.";
	REQUIRE(d.size() > last.size());
	REQUIRE(d.compare(d.size() - last.size(), last.size(), last) == 0);
}

TEST_CASE("standby on a tool without wake points at the tool that wakes the box", "[mcp-refusals]")
{
	const std::string d = mcp::describedTool(kRoutes[1], kTools[1]);
	INFO(d);
	REQUIRE(d.find("box-in-standby: the box is in standby. Wake the box with switch_channel and wake true, "
	               "or with set_standby (on false), which needs the system scope.") != std::string::npos);
	REQUIRE(d.find("Argument refusals") == std::string::npos);
}

TEST_CASE("a tool states only its route's own refusals", "[mcp-refusals]")
{
	const std::string d = mcp::describedTool(kRoutes[0], kTools[0]);
	INFO(d);
	REQUIRE(occurrences(d, "\n- ") == 3);
	REQUIRE(d.find("Argument refusals") == std::string::npos);
	REQUIRE(d.find("bad-bool") == std::string::npos);
	REQUIRE(d.find("out-of-range") == std::string::npos);
	REQUIRE(d.find("not-permitted") == std::string::npos);
	REQUIRE(d.find("body-too-large") == std::string::npos);
}

TEST_CASE("a tool with nothing declared and no arguments says only what it does", "[mcp-refusals]")
{
	REQUIRE(mcp::describedTool(kRoutes[2], kTools[2]) == "removes");
}

TEST_CASE("the answer of a body-less route names only the word it can be", "[mcp-refusals]")
{
	REQUIRE(statusEnum(Answers202) == "accepted");
	REQUIRE(statusEnum(Answers204) == "done");
	REQUIRE(statusEnum(Answers201) == "done");
	REQUIRE(statusEnum(Answers202 | Answers204) == "done,accepted");

	const mcp::ToolDef zap = mcp::toolFor(kRoutes[0], kTools[0]);
	REQUIRE(zap.output.find("\"accepted\"") != std::string::npos);
	REQUIRE(zap.output.find("\"done\"") == std::string::npos);
}

TEST_CASE("routes answering a range or an undescribed document are refused as tools", "[mcp-refusals]")
{
	std::string why;
	{
		const RouteTable *const list[] = { &kRangeTable };
		REQUIRE_FALSE(mcp::toolsAreSane(list, 1, kNoComposed, &why));
		REQUIRE(why.find("answers with part of a file") != std::string::npos);
	}
	{
		const RouteTable *const list[] = { &kDocumentTable };
		REQUIRE_FALSE(mcp::toolsAreSane(list, 1, kNoComposed, &why));
		REQUIRE(why.find("answers a document it does not describe") != std::string::npos);
	}
	REQUIRE(mcp::toolsAreSane(kTables, 1, kNoComposed, &why));
}

TEST_CASE("a refusal at call time comes with the same hint", "[mcp-refusals]")
{
	mcp::RouteTools tools(kTables, 1, kNoComposed);
	REQUIRE(tools.hint("zap_like", coreapi::ErrorCode::BoxInStandby) == "Retry with wake true.");
	const std::string standby = tools.hint("mode_like", coreapi::ErrorCode::BoxInStandby);
	REQUIRE(standby.find("switch_channel") < standby.find("set_standby"));
	REQUIRE(standby.find("set_standby") != std::string::npos);
	REQUIRE(tools.hint("zap_like", coreapi::ErrorCode::ChangeRefused).empty());
	REQUIRE(tools.hint("no_such_tool", coreapi::ErrorCode::BoxInStandby).empty());
}

TEST_CASE("nothing playing tells a reading caller what it cannot do itself", "[mcp-refusals]")
{
	const char *hint = mcp::retryHint(coreapi::ErrorCode::NoRunningChannel, kRoutes[2]);
	REQUIRE(hint != NULL);
	REQUIRE(std::string(hint) == "No channel plays live: the box is in standby or plays a recording or a file, "
	                             "now_playing says which. A caller with write access can start a channel with "
	                             "switch_channel and wake true.");
}

TEST_CASE("a missing recording points an archive caller at list_archive and a running one at list_recordings",
         "[mcp-refusals]")
{
	const char *archived = mcp::retryHint(coreapi::ErrorCode::NoSuchRecording, kRoutes[6]);
	REQUIRE(archived != NULL);
	REQUIRE(std::string(archived) == "list_archive shows the finished recordings and their ids.");

	const char *running = mcp::retryHint(coreapi::ErrorCode::NoSuchRecording, kRoutes[0]);
	REQUIRE(running != NULL);
	REQUIRE(std::string(running) == "list_recordings shows what is recording.");
}

TEST_CASE("a denied section or credential is never pointed at Freigaben", "[mcp-refusals]")
{
	const char *denied = mcp::retryHint(coreapi::ErrorCode::SettingsSectionDenied, kRoutes[0]);
	REQUIRE(denied != NULL);
	const std::string d = denied;
	REQUIRE(d.find("Freigaben") == std::string::npos);
	REQUIRE(d == "No AI client may ever change this section or credential; tell the user to change it in "
	             "ni-web directly. Do not try another way.");

	const char *notAllowed = mcp::retryHint(coreapi::ErrorCode::SettingsSectionNotAllowed, kRoutes[0]);
	REQUIRE(notAllowed != NULL);
	REQUIRE(std::string(notAllowed).find("Freigaben") != std::string::npos);

	const char *pluginNotAllowed = mcp::retryHint(coreapi::ErrorCode::PluginNotAllowed, kRoutes[0]);
	REQUIRE(pluginNotAllowed != NULL);
	REQUIRE(std::string(pluginNotAllowed).find("Freigaben") != std::string::npos);
}

TEST_CASE("every tool a hint sends the model to is one the box offers", "[mcp-refusals]")
{
	setRoutesForTest(NULL);
	const std::vector<mcp::ToolDef> offered = mcp::boxTools().list();
	std::set<std::string> names;
	for (size_t i = 0; i < offered.size(); ++i)
		names.insert(offered[i].name);
	REQUIRE(names.size() == kOfferedTools);

	for (int c = 0; c <= (int) mcp::kLastErrorCode; ++c)
	{
		for (size_t r = 0; r < 2; ++r)
		{
			const char *hint = mcp::retryHint((coreapi::ErrorCode) c, kRoutes[r]);
			if (hint == NULL)
				continue;
			const std::string h = hint;
			for (size_t at = 0; at < h.size();)
			{
				size_t end = at;
				while (end < h.size() && (std::islower((unsigned char) h[end]) || h[end] == '_'))
					++end;
				const std::string word = h.substr(at, end - at);
				if (word.find('_') != std::string::npos)
				{
					INFO(coreapi::codeString((coreapi::ErrorCode) c) << ": " << word);
					REQUIRE(names.count(word) == 1);
				}
				at = (end > at) ? end : at + 1;
			}
		}
	}
	REQUIRE(mcp::boxTools().hint("switch_channel", coreapi::ErrorCode::BoxInStandby) == "Retry with wake true.");
	REQUIRE(mcp::boxTools().hint("reboot", coreapi::ErrorCode::BoxInStandby).empty());
	REQUIRE(mcp::boxTools().hint("switch_channel", coreapi::ErrorCode::PlaybackRunning)
		== "Retry with stop_playback true.");
	REQUIRE(mcp::boxTools().hint("reboot", coreapi::ErrorCode::PlaybackRunning).empty());
}

TEST_CASE("set_timer refusals carry hints the model can act on", "[mcp-refusals]")
{
	mcp::RouteTools tools(NULL, 0, mcp::composedTable());
	REQUIRE(tools.hint("set_timer", coreapi::ErrorCode::TimerInThePast) == "Pick a programme or a time that has not passed.");
	REQUIRE(tools.hint("set_timer", coreapi::ErrorCode::AmbiguousChannel)
		== "Retry with one of the names or ids the refusal lists.");
	REQUIRE(tools.hint("set_timer", coreapi::ErrorCode::TimerExists) == "list_timers shows the timer that is already there.");
	REQUIRE(tools.hint("set_timer", coreapi::ErrorCode::MissingParameter).empty());
}

TEST_CASE("remove_timer refusals carry hints the model can act on", "[mcp-refusals]")
{
	mcp::RouteTools tools(NULL, 0, mcp::composedTable());
	REQUIRE(tools.hint("remove_timer", coreapi::ErrorCode::NoSuchTimer) == "list_timers shows the timers there are.");
	REQUIRE(tools.hint("remove_timer", coreapi::ErrorCode::NotPermitted).empty());
	REQUIRE(tools.hint("set_timer", coreapi::ErrorCode::NotPermitted)
		== "This needs a scope the connection was not granted; ask the user to reconnect with it.");
}

TEST_CASE("the shipped tools carry their routes' refusals", "[mcp-refusals]")
{
	setRoutesForTest(NULL);
	const std::vector<mcp::ToolDef> all = mcp::boxTools().list();
	std::map<std::string, std::string> described;
	for (size_t i = 0; i < all.size(); ++i)
		described[all[i].name] = all[i].description;

	REQUIRE(described["switch_channel"].find("- box-in-standby: the box is in standby. Retry with wake true.")
		!= std::string::npos);
	REQUIRE(described["switch_channel"].find("- playback-running: something is playing in the movie player. "
		"Retry with stop_playback true.") != std::string::npos);
	REQUIRE(described["stop_recording"].find("- no-such-recording:") != std::string::npos);
	REQUIRE(described["record_programme"].find("- timer-in-the-past: the programme has already ended.")
		!= std::string::npos);
	REQUIRE(described["set_timer"].find("- timer-in-the-past: a timer that runs once cannot begin before now. Pick a")
		!= std::string::npos);
	REQUIRE(described["remove_timer"].find("- not-permitted: only a record, zap or reminder timer can be removed here; "
		"the user removes the others on the box or in ni-web.\n") != std::string::npos);
	for (size_t i = 0; i < all.size(); ++i)
	{
		INFO(all[i].name);
		REQUIRE(all[i].description.size() <= 4096);
		REQUIRE(all[i].description.find("Argument refusals") == std::string::npos);
	}
}

TEST_CASE("no shipped tool states an argument check", "[mcp-refusals]")
{
	static const char *const kChecks[] = {
		"missing-parameter", "no-such-parameter", "duplicate-parameter", "value-too-long",
		"value-has-zero-byte", "bad-string", "bad-int", "bad-bool", "bad-enum", "out-of-range",
	};
	setRoutesForTest(NULL);
	const std::vector<mcp::ToolDef> all = mcp::boxTools().list();
	for (size_t i = 0; i < all.size(); ++i)
	{
		for (size_t k = 0; k < sizeof(kChecks) / sizeof(kChecks[0]); ++k)
		{
			INFO(all[i].name << " " << kChecks[k]);
			REQUIRE(all[i].description.find(std::string("- ") + kChecks[k] + ":") == std::string::npos);
		}
	}
	// set_timer and find_programme declare such codes themselves.
	const RouteTable &c = mcp::composedTable();
	size_t own = 0;
	for (size_t e = 0; e < c.count; ++e)
		for (size_t r = 0; r < c.endpoints[e].refusal_count; ++r)
			for (size_t k = 0; k < sizeof(kChecks) / sizeof(kChecks[0]); ++k)
				own += std::strcmp(coreapi::codeString(c.endpoints[e].refusals[r].code), kChecks[k]) == 0;
	REQUIRE(own >= 3);
}

// The answer watch sees only shipped tables, so the composed ones are held here.
namespace
{

struct Case
{
	const char *tool;
	const char *args;
	void (*prepare)(InstalledDependencies &);
};

const coreapi::ChannelId ERSTE    = 0x00010001aaaa0001ULL;
const coreapi::ChannelId ERSTE_HD = 0x00010001aaaa0002ULL;
const time_t T0 = 1790000000;

coreapi::ChannelInfo channelInfo(coreapi::ChannelId id, const char *name)
{
	coreapi::ChannelInfo c;
	c.id = id;
	c.epg_id = id;
	c.name = name;
	c.kind = coreapi::ServiceKind::Tv;
	return c;
}

void base(InstalledDependencies &d)
{
	d.channels.channels.push_back(channelInfo(ERSTE, "Das Erste"));
	d.channels.channels.push_back(channelInfo(ERSTE_HD, "Das Erste HD"));
	d.channels.mode = NeutrinoModes::mode_tv;
	coreapi::EventDetail e;
	e.event_id = 0x102;
	e.channel_id = ERSTE;
	e.title = "Tatort";
	e.start = T0;
	e.duration = 5400;
	d.epg.details.push_back(e);
	d.timers.clock = T0 - 3600;
	mcp::setToolClockForTest(T0 - 3600);
}

void standby(InstalledDependencies &d)
{
	base(d);
	d.channels.mode = NeutrinoModes::mode_standby;
}

void ended(InstalledDependencies &d)
{
	base(d);
	d.timers.clock = T0 + 5400;
	mcp::setToolClockForTest(T0 + 5400);
}

void taken(InstalledDependencies &d)
{
	base(d);
	d.timers.add_status = coreapi::Status::Conflict;
}

void nowhere(InstalledDependencies &d)
{
	base(d);
	d.epg.details[0].channel_id = 0;
}

void tunerHeld(InstalledDependencies &d)
{
	base(d);
	d.channels.zap_possible = false;
}

// Taken away again after every call below.
void filePlaying(InstalledDependencies &d)
{
	base(d);
	coreapi::archive::notePlaying("/media/hdd/movie/one.ts");
}

void timersHeld(InstalledDependencies &d)
{
	base(d);
	coreapi::TimerInfo off;
	off.id = 3;
	off.type = (int) coreapi::TimerType::Shutdown;
	d.timers.timers.push_back(off);
	coreapi::TimerInfo rec;
	rec.id = 4;
	rec.type = (int) coreapi::TimerType::Record;
	rec.channel_id = ERSTE;
	d.timers.timers.push_back(rec);
}

void deaf(InstalledDependencies &d)
{
	timersHeld(d);
	d.timers.ignore_removals = true;
}

void timerRunning(InstalledDependencies &d)
{
	timersHeld(d);
	for (size_t i = 0; i < d.timers.timers.size(); ++i)
		if (d.timers.timers[i].id == 4)
			d.timers.timers[i].state = (int) CTimerd::TIMERSTATE_ISRUNNING;
}

void zapHeld(InstalledDependencies &d)
{
	base(d);
	coreapi::TimerInfo z;
	z.id = 6;
	z.type = (int) coreapi::TimerType::Zapto;
	z.channel_id = ERSTE;
	z.start = T0;
	d.timers.timers.push_back(z);
}

void screenNotCaptured(InstalledDependencies &d)
{
	base(d);
	d.screen.screen_status = coreapi::Status::Internal;
}

void screenTooLarge(InstalledDependencies &d)
{
	base(d);
	// Starts with the JPEG marker, so it passes that check, but breaks off before a
	// header libjpeg can read; fitJpeg fails and the route cannot tell that apart
	// from a picture it decoded but could not shrink enough.
	d.screen.content = std::string("\xFF\xD8", 2) + std::string(500, 'x');
}

const Case kCases[] = {
	{ "whats_on", "{\"channel\":\"x\",\"bouquet\":\"y\"}", &base },
	{ "whats_on", "{\"channel\":\"erste\"}", &base },
	{ "whats_on", "{\"channel\":\"arte\"}", &base },
	{ "whats_on", "{\"bouquet\":\"Sport\"}", &base },
	{ "whats_on", "{}", &standby },
	{ "find_programme", "{\"query\":\"T\"}", &base },
	{ "find_programme", "{\"query\":\"Tatort\",\"channel\":\"erste\"}", &base },
	{ "find_programme", "{\"query\":\"Tatort\",\"channel\":\"arte\"}", &base },
	{ "find_programme", "{\"query\":\"Tatort\",\"from\":100,\"to\":50}", &base },
	{ "record_programme", "{\"id\":\"999\",\"start\":1790000000}", &base },
	{ "record_programme", "{\"id\":\"102\",\"start\":1790000000}", &ended },
	{ "record_programme", "{\"id\":\"102\",\"start\":1790000000}", &taken },
	{ "record_programme", "{\"id\":\"102\",\"start\":1790000000}", &nowhere },
	{ "switch_channel", "{\"channel\":\"erste\"}", &base },
	{ "switch_channel", "{\"channel\":\"arte\"}", &base },
	{ "switch_channel", "{\"channel\":\"das erste\"}", &standby },
	{ "switch_channel", "{\"channel\":\"das erste\"}", &tunerHeld },
	{ "switch_channel", "{\"channel\":\"das erste\"}", &filePlaying },
	{ "set_timer", "{\"kind\":\"record\",\"channel\":\"das erste\",\"start\":1790000000}", &base },
	{ "set_timer", "{\"kind\":\"reminder\",\"channel\":\"das erste\",\"start\":1790000000}", &base },
	{ "set_timer", "{\"kind\":\"zap\",\"channel\":\"das erste\",\"start\":1790000000,\"end\":1790003600}", &base },
	{ "set_timer", "{\"kind\":\"zap\",\"channel\":\"erste\",\"start\":1790000000}", &base },
	{ "set_timer", "{\"kind\":\"zap\",\"channel\":\"arte\",\"start\":1790000000}", &base },
	{ "set_timer", "{\"kind\":\"zap\",\"channel\":\"das erste\",\"start\":1789990000}", &base },
	{ "set_timer", "{\"kind\":\"record\",\"channel\":\"das erste\",\"start\":1790000000,\"end\":1790000000}", &base },
	{ "set_timer", "{\"kind\":\"zap\",\"channel\":\"das erste\",\"start\":1790000000}", &taken },
	{ "remove_timer", "{\"id\":99}", &timersHeld },
	{ "remove_timer", "{\"id\":3}", &timersHeld },
	{ "remove_timer", "{\"id\":4}", &deaf },
	{ "change_timer", "{\"id\":99}", &base },
	{ "change_timer", "{\"id\":3}", &timersHeld },
	{ "change_timer", "{\"id\":4}", &timersHeld },
	{ "change_timer", "{\"id\":6,\"end\":1790003600}", &zapHeld },
	{ "change_timer", "{\"id\":4,\"channel\":\"Das Erste HD\"}", &timerRunning },
	{ "change_timer", "{\"id\":4,\"channel\":\"erste\"}", &timersHeld },
	{ "change_timer", "{\"id\":4,\"channel\":\"arte\"}", &timersHeld },
	{ "change_timer", "{\"id\":6,\"start\":1}", &zapHeld },
	{ "change_timer", "{\"id\":4,\"start\":500,\"end\":400}", &timersHeld },
	{ "screenshot", "{}", &screenNotCaptured },
	{ "screenshot", "{}", &screenTooLarge },
};

} // namespace

TEST_CASE("every refusal a composed tool declares is one it gives and the other way round", "[mcp-refusals]")
{
	std::map<std::string, std::set<std::string> > seen;
	mcp::Caller writer;
	writer.level = AuthLevel::Write;

	std::string long_query = "{\"query\":\"";
	long_query.append(256, 'x');
	long_query += "\"}";

	for (size_t i = 0; i <= sizeof(kCases) / sizeof(kCases[0]); ++i)
	{
		const bool extra = (i == sizeof(kCases) / sizeof(kCases[0]));
		const char *tool = extra ? "find_programme" : kCases[i].tool;
		InstalledDependencies deps;
		if (extra)
			base(deps);
		else
			kCases[i].prepare(deps);
		mcp::RouteTools tools(NULL, 0, mcp::composedTable());
		const coreapi::Result<std::string> got = tools.call(writer, tool, extra ? long_query : kCases[i].args);
		mcp::setToolClockForTest(0);
		coreapi::archive::notePlaying(std::string());
		INFO(tool << " " << (extra ? long_query : kCases[i].args));
		REQUIRE_FALSE(got.ok());
		seen[tool].insert(coreapi::codeString(got.error().code));
	}

	const RouteTable &c = mcp::composedTable();
	for (size_t e = 0; e < c.count; ++e)
	{
		const std::string name = mcp::toolName(c.endpoints[e], c.tools[e]);
		std::set<std::string> declared;
		for (size_t r = 0; r < c.endpoints[e].refusal_count; ++r)
			declared.insert(coreapi::codeString(c.endpoints[e].refusals[r].code));
		INFO(name);
		REQUIRE(declared == seen[name]);
	}
}
