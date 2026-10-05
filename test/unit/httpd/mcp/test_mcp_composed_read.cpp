/*
 * test_mcp_composed_read.cpp - what is on, and where a programme is
 *
 * Copyright (C) 2026 NI-Team
 *
 * License: GPL-2.0-or-later, see COPYING.
 */

#include "support/catch.hpp"
#include "support/fakes.h"
#include "support/shape.h"

#include "httpd/endpoint.h"
#include "httpd/router.h"
#include "httpd/mcp/channelname.h"
#include "httpd/mcp/composed.h"
#include "httpd/mcp/routetools.h"
#include "httpd/mcp/toolguard.h"

#include "coreapi/base/errors.h"

#include <neutrinoMessages.h>

#include "jsoncpp/json/json.h"

#include "toolcaller.h"

#include <string>

using namespace httpd;

namespace
{

const coreapi::ChannelId ERSTE    = 0x00010001aaaa0001ULL;
const coreapi::ChannelId ERSTE_HD = 0x00010001aaaa0002ULL;
const coreapi::ChannelId ZDF      = 0x00010001bbbb0001ULL;
const coreapi::ChannelId ZDF_HD   = 0x00010001bbbb0002ULL;
const coreapi::ChannelId BBC      = 0x00010001cccc0001ULL;
const coreapi::ChannelId DLF      = 0x00010001dddd0001ULL;

const time_t T0 = 1790000000;

coreapi::ChannelInfo channel(coreapi::ChannelId id, const char *name, int32_t number,
                             coreapi::ServiceKind kind = coreapi::ServiceKind::Tv)
{
	coreapi::ChannelInfo c;
	c.id = id;
	c.epg_id = id;
	c.number = number;
	c.name = name;
	c.kind = kind;
	return c;
}

coreapi::EventInfo event(uint64_t id, coreapi::ChannelId ch, const char *title, time_t start, unsigned duration)
{
	coreapi::EventInfo e;
	e.event_id = id;
	e.channel_id = ch;
	e.title = title;
	e.description = std::string("about ") + title;
	e.start = start;
	e.duration = duration;
	return e;
}

void fill(InstalledDependencies &deps)
{
	deps.channels.channels.push_back(channel(ERSTE, "Das Erste", 1));
	deps.channels.channels.push_back(channel(ERSTE_HD, "Das Erste HD", 0));
	deps.channels.channels.push_back(channel(ZDF_HD, "ZDF HD", 2));
	deps.channels.channels.push_back(channel(ZDF, "ZDF", 12));
	deps.channels.channels.push_back(channel(BBC, "BBC", 30));
	deps.channels.channels.push_back(channel(DLF, "Deutschlandfunk", 1, coreapi::ServiceKind::Radio));

	deps.epg.events.push_back(event(0x101, ERSTE, "Tagesschau", T0 - 900, 900));
	deps.epg.events.push_back(event(0x102, ERSTE, "Tatort", T0, 5400));
	deps.epg.events.push_back(event(0x103, ERSTE, "Tagesthemen", T0 + 5400, 1800));
	deps.epg.events.push_back(event(0x201, ZDF, "heute", T0 + 600, 900));
	deps.epg.events.push_back(event(0x301, BBC, "Tatort Classics", T0 + 86400, 3600));
	deps.epg.events.push_back(event(0x302, BBC, "Tatort Classics", T0 - 7200, 3600));
}

// Checked before reading, so a wrong answer fails the case and not the run.
coreapi::ChannelId resolvedId(const char *text)
{
	INFO(text);
	const coreapi::Result<coreapi::ChannelInfo> got = mcp::resolveChannel(text);
	REQUIRE(got.ok());
	return got.value().id;
}

coreapi::ErrorCode resolveRefusal(const char *text)
{
	INFO(text);
	const coreapi::Result<coreapi::ChannelInfo> got = mcp::resolveChannel(text);
	REQUIRE_FALSE(got.ok());
	return got.error().code;
}

} // namespace

TEST_CASE("the composed tools pass the guard and are not reachable over HTTP", "[mcp-composed]")
{
	std::string why;
	const bool passed = mcp::toolsAreSane(NULL, 0, mcp::composedTable(), &why);
	INFO(why);
	REQUIRE(passed);
	REQUIRE(mcp::composedTable().count <= mcp::kMaxComposedTools);
	setRoutesForTest(NULL);
	REQUIRE_FALSE(routeLevelFor(Get, "/mcp/tools/whats_on", NULL));
}

TEST_CASE("a channel is found by the name people say", "[mcp-composed]")
{
	InstalledDependencies deps;
	fill(deps);
	REQUIRE(resolvedId("  das erste ") == ERSTE);
	// An exact name beats a longer one that contains it.
	REQUIRE(resolvedId("zdf") == ZDF);
	REQUIRE(resolvedId("deutschland") == DLF);
	// Valid hex and still a name.
	REQUIRE(resolvedId("BBC") == BBC);
	REQUIRE(resolvedId("0x00010001bbbb0002") == ZDF_HD);

	const coreapi::Result<coreapi::ChannelInfo> two = mcp::resolveChannel("erste");
	REQUIRE_FALSE(two.ok());
	REQUIRE(two.error().code == coreapi::ErrorCode::AmbiguousChannel);
	REQUIRE(two.error().message.find("Das Erste (10001aaaa0001)") != std::string::npos);
	REQUIRE(two.error().message.find("Das Erste HD") != std::string::npos);

	REQUIRE(resolveRefusal("arte") == coreapi::ErrorCode::NoSuchChannel);
	REQUIRE(resolveRefusal("   ") == coreapi::ErrorCode::MissingParameter);
}

TEST_CASE("two channels of one name go to the one the list numbers first", "[mcp-composed]")
{
	InstalledDependencies deps;
	deps.channels.channels.push_back(channel(ZDF_HD, "ZDF", 0));
	deps.channels.channels.push_back(channel(ZDF, "ZDF", 7));
	deps.channels.channels.push_back(channel(BBC, "ZDF", 3));
	REQUIRE(resolvedId("zdf") == BBC);
}

TEST_CASE("whats_on names now and next and an event ending at the moment is over", "[mcp-composed]")
{
	InstalledDependencies deps;
	fill(deps);
	const ::Json::Value v = composedAnswer(AuthLevel::Read, "whats_on",
		"{\"channel\":\"Das Erste\",\"at\":1790000000}", "/mcp/tools/whats_on");
	REQUIRE(v["at"].asInt64() == T0);
	REQUIRE(v["truncated"].asBool() == false);
	REQUIRE(v["items"].size() == 1);
	const ::Json::Value &row = v["items"][0];
	REQUIRE(row["channel"].asString() == "Das Erste");
	REQUIRE(row["now"]["title"].asString() == "Tatort");
	REQUIRE(row["now"]["end"].asInt64() == T0 + 5400);
	REQUIRE(row["now"]["id"].asString() == "102");
	REQUIRE(row["next"]["title"].asString() == "Tagesthemen");
}

TEST_CASE("whats_on calls the latest-starting of two covering events now", "[mcp-composed]")
{
	InstalledDependencies deps;
	deps.channels.channels.push_back(channel(ERSTE, "Das Erste", 1));
	deps.epg.events.push_back(event(0x401, ERSTE, "Sommerfest", T0 - 3600, 7200));
	deps.epg.events.push_back(event(0x402, ERSTE, "Brennpunkt", T0 - 600, 1200));
	const ::Json::Value v = composedAnswer(AuthLevel::Read, "whats_on",
		"{\"channel\":\"Das Erste\",\"at\":1790000000}", "/mcp/tools/whats_on");
	REQUIRE(v["items"][0]["now"]["title"].asString() == "Brennpunkt");
}

TEST_CASE("whats_on leaves out what is not there", "[mcp-composed]")
{
	InstalledDependencies deps;
	fill(deps);
	const ::Json::Value v = composedAnswer(AuthLevel::Read, "whats_on",
		"{\"channel\":\"zdf\",\"at\":1790000000}", "/mcp/tools/whats_on");
	REQUIRE_FALSE(v["items"][0].isMember("now"));
	REQUIRE(v["items"][0]["next"]["title"].asString() == "heute");

	// Tagesthemen ends at exactly this moment and nothing follows it.
	const ::Json::Value late = composedAnswer(AuthLevel::Read, "whats_on",
		"{\"channel\":\"das erste\",\"at\":1790007200}", "/mcp/tools/whats_on");
	REQUIRE_FALSE(late["items"][0].isMember("now"));
	REQUIRE_FALSE(late["items"][0].isMember("next"));
}

TEST_CASE("whats_on with nothing named is the channel playing at the clock's now", "[mcp-composed]")
{
	InstalledDependencies deps;
	fill(deps);
	const ToolClock clock(T0 + 60);
	deps.channels.current = channel(ERSTE, "Das Erste", 1);
	deps.channels.current_status = coreapi::Status::Ok;
	const ::Json::Value v = composedAnswer(AuthLevel::Read, "whats_on", "{}", "/mcp/tools/whats_on");
	REQUIRE(v["at"].asInt64() == T0 + 60);
	REQUIRE(v["items"][0]["now"]["title"].asString() == "Tatort");

	deps.channels.mode = NeutrinoModes::mode_standby;
	REQUIRE(composedRefusal(AuthLevel::Read, "whats_on", "{}").code == coreapi::ErrorCode::NoRunningChannel);
}

TEST_CASE("whats_on over a bouquet keeps its order and says when it was cut", "[mcp-composed]")
{
	InstalledDependencies deps;
	fill(deps);
	coreapi::BouquetInfo b;
	b.id = 4;
	b.name = "Favoriten";
	deps.channels.bouquets.push_back(b);
	deps.channels.bouquet_members[4].push_back(channel(ZDF, "ZDF", 12));
	deps.channels.bouquet_members[4].push_back(channel(ERSTE, "Das Erste", 1));
	const ::Json::Value v = composedAnswer(AuthLevel::Read, "whats_on",
		"{\"bouquet\":\"favoriten\",\"at\":1790000000}", "/mcp/tools/whats_on");
	REQUIRE(v["items"].size() == 2);
	REQUIRE(v["items"][0]["channel"].asString() == "ZDF");
	REQUIRE(v["truncated"].asBool() == false);

	coreapi::BouquetInfo big;
	big.id = 5;
	big.name = "Alle";
	deps.channels.bouquets.push_back(big);
	for (int i = 0; i < 51; ++i)
		deps.channels.bouquet_members[5].push_back(channel(0x00020000000000ULL + i, "x", i + 1));
	const ::Json::Value cut = composedAnswer(AuthLevel::Read, "whats_on",
		"{\"bouquet\":\"Alle\",\"at\":1790000000}", "/mcp/tools/whats_on");
	REQUIRE(cut["items"].size() == 50);
	REQUIRE(cut["truncated"].asBool() == true);

	REQUIRE(composedRefusal(AuthLevel::Read, "whats_on", "{\"channel\":\"zdf\",\"bouquet\":\"Alle\"}").code
		== coreapi::ErrorCode::ConflictingParameters);
	REQUIRE(composedRefusal(AuthLevel::Read, "whats_on",
		"{\"bouquet\":\"Sport\"}").code == coreapi::ErrorCode::NoSuchBouquet);
}

TEST_CASE("find_programme finds across channels by start with names and on air", "[mcp-composed]")
{
	InstalledDependencies deps;
	fill(deps);
	const ToolClock clock(T0 + 60);
	const ::Json::Value v = composedAnswer(AuthLevel::Read, "find_programme",
		"{\"query\":\"Tatort\",\"from\":1789990000}", "/mcp/tools/find_programme");
	REQUIRE(v["items"].size() == 3);
	REQUIRE(v["items"][0]["start"].asInt64() == T0 - 7200);
	REQUIRE(v["items"][1]["title"].asString() == "Tatort");
	REQUIRE(v["items"][1]["channel"].asString() == "Das Erste");
	REQUIRE(v["items"][1]["on_air"].asBool() == true);
	REQUIRE(v["items"][2]["channel"].asString() == "BBC");
	REQUIRE(v["items"][2]["on_air"].asBool() == false);
	REQUIRE(v["truncated"].asBool() == false);
}

TEST_CASE("find_programme on a channel showing another's schedule finds its programmes", "[mcp-composed]")
{
	InstalledDependencies deps;
	const ToolClock clock(T0 - 10000);
	coreapi::ChannelInfo hd = channel(ZDF_HD, "ZDF HD", 2);
	hd.epg_id = ZDF;
	deps.channels.channels.push_back(hd);
	deps.channels.channels.push_back(channel(ZDF, "ZDF", 12));
	deps.epg.events.push_back(event(0x201, ZDF, "heute", T0 + 600, 900));
	const ::Json::Value v = composedAnswer(AuthLevel::Read, "find_programme",
		"{\"query\":\"heute\",\"channel\":\"zdf hd\"}", "/mcp/tools/find_programme");
	REQUIRE(v["items"].size() == 1);
	REQUIRE(v["items"][0]["title"].asString() == "heute");
}

TEST_CASE("find_programme narrows to a channel and cuts at the limit", "[mcp-composed]")
{
	InstalledDependencies deps;
	fill(deps);
	const ToolClock clock(T0 - 10000);
	const ::Json::Value one = composedAnswer(AuthLevel::Read, "find_programme",
		"{\"query\":\"Tatort\",\"channel\":\"bbc\"}", "/mcp/tools/find_programme");
	REQUIRE(one["items"].size() == 2);
	REQUIRE(one["items"][0]["channel_id"].asString() == "10001cccc0001");

	const ::Json::Value cut = composedAnswer(AuthLevel::Read, "find_programme",
		"{\"query\":\"Tatort\",\"limit\":1}", "/mcp/tools/find_programme");
	REQUIRE(cut["items"].size() == 1);
	// The guide holds the later showing first.
	REQUIRE(cut["items"][0]["start"].asInt64() == T0 - 7200);
	REQUIRE(cut["truncated"].asBool() == true);

	REQUIRE(composedRefusal(AuthLevel::Read, "find_programme",
		"{\"query\":\"T\"}").code == coreapi::ErrorCode::QueryTooShort);
	REQUIRE(composedRefusal(AuthLevel::Read, "find_programme",
		"{\"query\":\"" + std::string(256, 'a') + "\"}").code
		== coreapi::ErrorCode::ValueTooLong);
	REQUIRE(composedRefusal(AuthLevel::Read, "find_programme",
		"{\"query\":\"Tatort\",\"limit\":51}").code == coreapi::ErrorCode::OutOfRange);
}
