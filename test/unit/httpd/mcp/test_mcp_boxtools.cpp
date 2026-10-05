/*
 * test_mcp_boxtools.cpp - the tools this box offers
 *
 * Copyright (C) 2026 NI-Team
 *
 * License: GPL-2.0-or-later, see COPYING.
 */

#include "support/catch.hpp"
#include "support/fakes.h"

#include "httpd/endpoint.h"
#include "httpd/router.h"
#include "httpd/mcp/allowlist.h"
#include "httpd/mcp/composed.h"
#include "httpd/mcp/contract.h"
#include "httpd/mcp/toolguard.h"
#include "httpd/mcp/toolgroups.h"

#include "coreapi/archive.h"
#include "coreapi/base/errors.h"

#include "jsoncpp/json/json.h"

#include "toolcaller.h"
#include <neutrinoMessages.h>

#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

using namespace httpd;

namespace
{

mcp::Caller at(AuthLevel l)
{
	mcp::Caller c;
	c.level = l;
	c.external = false;
	return c;
}

std::set<std::string> names(const std::vector<mcp::ToolDef> &tools, AuthLevel level)
{
	std::set<std::string> out;
	for (size_t i = 0; i < tools.size(); ++i)
		if (tools[i].level == level)
			out.insert(tools[i].name);
	return out;
}

mcp::ToolSource &shipped()
{
	setRoutesForTest(NULL);
	return mcp::boxTools();
}

/* Channels and a guide over them, built for the grid tool alone: each channel
   carries the same number of events, each event titled forty characters long,
   so a case can ask what one page of the grid weighs without caring what any
   of it says. */
struct GridFixture
{
	FakeChannelSource channel_src;
	FakeEpgSource epg_src;
	InstalledChannelSource installed_channels;
	InstalledEpgSource installed_epg;
	std::vector<uint64_t> ids;

	GridFixture(size_t channels, size_t events_per_channel)
		: installed_channels(&channel_src), installed_epg(&epg_src)
	{
		const uint64_t first = 0x2b66000000010000ULL;
		const std::string title(40, 'x');
		for (size_t c = 0; c < channels; ++c)
		{
			const uint64_t id = first + c;
			ids.push_back(id);

			coreapi::ChannelInfo ch;
			ch.id = id;
			ch.epg_id = id;
			ch.name = "tv";
			ch.kind = coreapi::ServiceKind::Tv;
			channel_src.channels.push_back(ch);

			for (size_t e = 0; e < events_per_channel; ++e)
			{
				coreapi::EventInfo ev;
				ev.event_id = (id << 16) | e;
				ev.channel_id = id;
				ev.title = title;
				ev.start = (time_t)(2000000000 + e * 900);
				ev.duration = 900;
				epg_src.events.push_back(ev);
			}
		}
	}

	std::string channelList() const
	{
		std::string out;
		char buf[24];
		for (size_t i = 0; i < ids.size(); ++i)
		{
			if (i)
				out += ",";
			std::snprintf(buf, sizeof(buf), "%llx", (unsigned long long) ids[i]);
			out += buf;
		}
		return out;
	}
};

// A record directory with bare recordings, removed whole afterwards.
struct RecordDir
{
	std::string dir;
	std::vector<std::string> made;

	RecordDir()
	{
		char tmpl[] = "/tmp/mcp_archive_XXXXXX";
		if (mkdtemp(tmpl) != NULL)
			dir = tmpl;
	}
	~RecordDir()
	{
		for (size_t i = 0; i < made.size(); ++i)
			unlink(made[i].c_str());
		rmdir(dir.c_str());
	}
	void recording(const std::string &stem, const std::string &title, const std::string &more = std::string())
	{
		const std::string base = dir + "/" + stem;
		FILE *f = std::fopen((base + ".ts").c_str(), "wb");
		if (f != NULL)
			std::fclose(f);
		made.push_back(base + ".ts");
		f = std::fopen((base + ".xml").c_str(), "wb");
		if (f != NULL)
		{
			std::fprintf(f, "<epgtitle>%s</epgtitle>\n%s", title.c_str(), more.c_str());
			std::fclose(f);
		}
		made.push_back(base + ".xml");
	}
};

} // namespace

TEST_CASE("the shipped flags pass every rule", "[mcp-boxtools]")
{
	setRoutesForTest(NULL);
	size_t count = 0;
	const RouteTable *const *tables = allRoutes(&count);
	std::string why;
	const bool passed = mcp::toolsAreSane(tables, count, mcp::composedTable(), &why);
	INFO(why);
	REQUIRE(passed);
	(void) shipped();
	REQUIRE(mcp::boxToolsRefusal().empty());
}

TEST_CASE("the box offers the proposed set at the proposed levels", "[mcp-boxtools]")
{
	static const char *const kRead[] = {
		"current_channel", "list_channels", "list_bouquets", "channel_schedule", "programme_details",
		"list_timers", "list_recordings", "box_info", "standby_state", "get_volume",
		"signal_quality", "storage_space", "whats_on", "find_programme", "screenshot",
		"epg_grid", "channel_logo", "list_tuners", "list_plugins", "settings_schema", "read_settings",
		"list_archive", "recording_details", "now_playing",
	};
	static const char *const kWrite[] = {
		"stop_recording", "set_volume", "set_mute", "show_message",
		"record_programme", "switch_channel", "set_timer", "remove_timer", "change_timer",
		"timeshift_start", "timeshift_stop", "set_mode", "create_bouquet", "delete_bouquet",
		"rename_bouquet", "move_bouquet", "hide_bouquet", "lock_bouquet", "set_bouquet_channels",
		"play_recording", "delete_recording",
	};
	const std::vector<mcp::ToolDef> all = shipped().list();
	const std::set<std::string> read = names(all, AuthLevel::Read);
	const std::set<std::string> write = names(all, AuthLevel::Write);
	const std::set<std::string> system = names(all, AuthLevel::System);

	REQUIRE(read.size() == sizeof(kRead) / sizeof(kRead[0]));
	for (size_t i = 0; i < sizeof(kRead) / sizeof(kRead[0]); ++i)
		REQUIRE(read.count(kRead[i]) == 1);
	REQUIRE(write.size() == sizeof(kWrite) / sizeof(kWrite[0]));
	for (size_t i = 0; i < sizeof(kWrite) / sizeof(kWrite[0]); ++i)
		REQUIRE(write.count(kWrite[i]) == 1);
	REQUIRE(system.size() == 1);
	REQUIRE(system.count("set_standby") == 1);
	REQUIRE(names(all, AuthLevel::Public).empty());
}

TEST_CASE("every offered tool says what it does and how it is called", "[mcp-boxtools]")
{
	const std::vector<mcp::ToolDef> all = shipped().list();
	::Json::Reader reader;
	for (size_t i = 0; i < all.size(); ++i)
	{
		INFO(all[i].name);
		REQUIRE_FALSE(all[i].description.empty());
		REQUIRE_FALSE(all[i].title.empty());
		::Json::Value in, out;
		REQUIRE(reader.parse(all[i].input, in));
		REQUIRE(in["type"].asString() == "object");
		if (!all[i].image)
		{
			REQUIRE(reader.parse(all[i].output, out));
			REQUIRE(out["type"].asString() == "object");
		}
		REQUIRE(all[i].level != AuthLevel::Public);
		if (all[i].read_only)
			REQUIRE_FALSE(all[i].destructive);
	}
	REQUIRE(all.size() == kOfferedTools);
}

TEST_CASE("a flagged route answers through the box's tools", "[mcp-boxtools]")
{
	InstalledDependencies deps;
	coreapi::TimerInfo t;
	t.id = 7;
	t.type = (int) coreapi::TimerType::Record;
	t.title = "Tatort";
	deps.timers.timers.push_back(t);
	deps.recordings.add(7, 0x00010001aaaa0001ULL, "/media/hdd/movie/tatort.ts");

	mcp::ToolSource &tools = shipped();
	const coreapi::Result<std::string> listed = tools.call(at(AuthLevel::Read), "list_timers", "{}");
	REQUIRE(listed.ok());
	REQUIRE(listed.value().find("\"Tatort\"") != std::string::npos);

	const coreapi::Result<std::string> stopped = tools.call(at(AuthLevel::Write), "stop_recording", "{\"id\":7}");
	INFO((stopped.ok() ? stopped.value() : stopped.error().message));
	REQUIRE(stopped.ok());
	REQUIRE(stopped.value() == "{\"status\":\"accepted\"}");
	REQUIRE(deps.recordings.stopped.size() == 1);
	REQUIRE(deps.recordings.stopped[0] == 7);

	const coreapi::Result<std::string> absent = tools.call(at(AuthLevel::Write), "stop_recording", "{\"id\":8}");
	REQUIRE_FALSE(absent.ok());
	REQUIRE(absent.error().code == coreapi::ErrorCode::NoSuchRecording);
	REQUIRE(deps.recordings.stopped.size() == 1);

	const coreapi::Result<std::string> closed = tools.call(at(AuthLevel::Write), "delete_timer", "{\"id\":7}");
	REQUIRE_FALSE(closed.ok());
	REQUIRE(closed.error().code == coreapi::ErrorCode::NoSuchTool);
}

TEST_CASE("the archive is offered as three tools and deleting is destructive", "[mcp-boxtools]")
{
	const std::vector<mcp::ToolDef> all = mcp::boxTools().list();
	const mcp::ToolDef *list = NULL;
	const mcp::ToolDef *play = NULL;
	const mcp::ToolDef *del = NULL;
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].name == "list_archive") list = &all[i];
		if (all[i].name == "play_recording") play = &all[i];
		if (all[i].name == "delete_recording") del = &all[i];
	}
	REQUIRE(list != NULL);
	REQUIRE(play != NULL);
	REQUIRE(del != NULL);
	REQUIRE(list->read_only);
	REQUIRE(list->level == AuthLevel::Read);
	REQUIRE(play->level == AuthLevel::Write);
	REQUIRE(del->destructive);
	REQUIRE(del->description.find("Ask the user") != std::string::npos);
}

TEST_CASE("list_archive takes a sort and an order", "[mcp-boxtools]")
{
	const std::vector<mcp::ToolDef> all = mcp::boxTools().list();
	std::string input;
	for (size_t i = 0; i < all.size(); ++i)
		if (all[i].name == "list_archive")
			input = all[i].input;
	REQUIRE(input.find("\"sort\"") != std::string::npos);
	REQUIRE(input.find("\"duration\"") != std::string::npos);
	REQUIRE(input.find("\"order\"") != std::string::npos);
	REQUIRE(input.find("\"desc\"") != std::string::npos);

	RecordDir disk;
	REQUIRE(!disk.dir.empty());
	disk.recording("r0", "beta");
	disk.recording("r1", "Alpha");
	disk.recording("r2", "gamma");
	FakeSettingsSource settings;
	settings.strings["network_nfs_recordingdir"] = disk.dir;
	InstalledSettingsSource in_settings(&settings);
	const coreapi::Result<std::string> got =
		shipped().call(at(AuthLevel::Read), "list_archive", "{\"sort\":\"title\",\"order\":\"desc\"}");
	INFO((got.ok() ? got.value() : got.error().message));
	REQUIRE(got.ok());
	const size_t gamma = got.value().find("\"gamma\"");
	const size_t beta = got.value().find("\"beta\"");
	const size_t alpha = got.value().find("\"Alpha\"");
	REQUIRE(gamma != std::string::npos);
	REQUIRE(gamma < beta);
	REQUIRE(beta < alpha);
}

TEST_CASE("recording_details reads one finished recording by its id", "[mcp-boxtools]")
{
	const std::vector<mcp::ToolDef> all = mcp::boxTools().list();
	const mcp::ToolDef *details = NULL;
	for (size_t i = 0; i < all.size(); ++i)
		if (all[i].name == "recording_details")
			details = &all[i];
	REQUIRE(details != NULL);
	REQUIRE(details->level == AuthLevel::Read);
	REQUIRE(details->read_only);
	REQUIRE(details->input.find("\"id\"") != std::string::npos);
	REQUIRE(details->output.find("\"long_description\"") != std::string::npos);

	RecordDir disk;
	REQUIRE(!disk.dir.empty());
	disk.recording("r0", "Tatort", "<info1>Krimi aus Wiesbaden</info1>\n");
	FakeSettingsSource settings;
	settings.strings["network_nfs_recordingdir"] = disk.dir;
	InstalledSettingsSource in_settings(&settings);
	const coreapi::Result<std::string> listed = shipped().call(at(AuthLevel::Read), "list_archive", "{}");
	REQUIRE(listed.ok());
	::Json::Value page;
	::Json::Reader reader;
	REQUIRE(reader.parse(listed.value(), page));
	const std::string id = page["items"][0]["id"].asString();
	REQUIRE(id.size() == 16);

	const coreapi::Result<std::string> got =
		shipped().call(at(AuthLevel::Read), "recording_details", "{\"id\":\"" + id + "\"}");
	INFO((got.ok() ? got.value() : got.error().message));
	REQUIRE(got.ok());
	REQUIRE(got.value().find("\"Krimi aus Wiesbaden\"") != std::string::npos);
	REQUIRE(got.value().find("\"cover\":false") != std::string::npos);

	const coreapi::Result<std::string> absent =
		shipped().call(at(AuthLevel::Read), "recording_details", "{\"id\":\"0000000000000000\"}");
	REQUIRE_FALSE(absent.ok());
	REQUIRE(absent.error().code == coreapi::ErrorCode::NoSuchRecording);
}

TEST_CASE("play_recording wakes a sleeping box only when asked", "[mcp-boxtools]")
{
	const std::vector<mcp::ToolDef> all = shipped().list();
	const mcp::ToolDef *play = NULL;
	for (size_t i = 0; i < all.size(); ++i)
		if (all[i].name == "play_recording")
			play = &all[i];
	REQUIRE(play != NULL);
	REQUIRE(play->level == AuthLevel::Write);
	REQUIRE(play->input.find("\"wake\"") != std::string::npos);
	REQUIRE(play->description.find("box-in-standby unless wake is true") != std::string::npos);
	REQUIRE(play->description.find("- box-in-standby: the box is in standby. Retry with wake true.")
		!= std::string::npos);
	REQUIRE(mcp::boxTools().hint("play_recording", coreapi::ErrorCode::BoxInStandby) == "Retry with wake true.");

	RecordDir disk;
	REQUIRE(!disk.dir.empty());
	disk.recording("r0", "Tatort");
	FakeSettingsSource settings;
	settings.strings["network_nfs_recordingdir"] = disk.dir;
	InstalledSettingsSource in_settings(&settings);
	FakeChannelSource channels;
	channels.mode = NeutrinoModes::mode_standby;
	InstalledChannelSource in_channels(&channels);
	FakeCommandSink sink;
	InstalledSink in_sink(&sink);

	const coreapi::Result<std::string> listed = shipped().call(at(AuthLevel::Read), "list_archive", "{}");
	REQUIRE(listed.ok());
	::Json::Value page;
	::Json::Reader reader;
	REQUIRE(reader.parse(listed.value(), page));
	const std::string id = page["items"][0]["id"].asString();

	const coreapi::Result<std::string> asleep =
		shipped().call(at(AuthLevel::Write), "play_recording", "{\"id\":\"" + id + "\"}");
	REQUIRE_FALSE(asleep.ok());
	REQUIRE(asleep.error().code == coreapi::ErrorCode::BoxInStandby);
	REQUIRE(sink.posted.empty());

	const coreapi::Result<std::string> woken =
		shipped().call(at(AuthLevel::Write), "play_recording", "{\"id\":\"" + id + "\",\"wake\":true}");
	INFO((woken.ok() ? woken.value() : woken.error().message));
	REQUIRE(woken.ok());
	REQUIRE(sink.posted.size() == 1);
	REQUIRE(sink.posted[0].first == NeutrinoMessages::EVT_PLAY_RECORDING);
	for (size_t i = 0; i < sink.posted.size(); ++i)
		delete[] (unsigned char *) sink.posted[i].second;
}

TEST_CASE("play_recording ends a file playing only when asked", "[mcp-boxtools]")
{
	const std::vector<mcp::ToolDef> all = shipped().list();
	const mcp::ToolDef *play = NULL;
	for (size_t i = 0; i < all.size(); ++i)
		if (all[i].name == "play_recording")
			play = &all[i];
	REQUIRE(play != NULL);
	REQUIRE(play->input.find("\"stop_playback\"") != std::string::npos);
	REQUIRE(play->description.find("- playback-running: something is playing in the movie player. "
		"Retry with stop_playback true.") != std::string::npos);
	REQUIRE(mcp::boxTools().hint("play_recording", coreapi::ErrorCode::PlaybackRunning)
		== "Retry with stop_playback true.");

	RecordDir disk;
	REQUIRE(!disk.dir.empty());
	disk.recording("r0", "Tatort");
	FakeSettingsSource settings;
	settings.strings["network_nfs_recordingdir"] = disk.dir;
	InstalledSettingsSource in_settings(&settings);
	FakeChannelSource channels;
	channels.mode = NeutrinoModes::mode_ts;
	InstalledChannelSource in_channels(&channels);
	FakeCommandSink sink;
	InstalledSink in_sink(&sink);

	const coreapi::Result<std::string> listed = shipped().call(at(AuthLevel::Read), "list_archive", "{}");
	REQUIRE(listed.ok());
	::Json::Value page;
	::Json::Reader reader;
	REQUIRE(reader.parse(listed.value(), page));
	const std::string id = page["items"][0]["id"].asString();

	coreapi::archive::notePlaying(disk.dir + "/other.ts");
	const coreapi::Result<std::string> busy =
		shipped().call(at(AuthLevel::Write), "play_recording", "{\"id\":\"" + id + "\"}");
	const size_t before = sink.posted.size();
	const coreapi::Result<std::string> ended = shipped().call(at(AuthLevel::Write), "play_recording",
		"{\"id\":\"" + id + "\",\"stop_playback\":true}");
	coreapi::archive::notePlaying(std::string());

	REQUIRE_FALSE(busy.ok());
	REQUIRE(busy.error().code == coreapi::ErrorCode::PlaybackRunning);
	REQUIRE(before == 0);
	INFO((ended.ok() ? ended.value() : ended.error().message));
	REQUIRE(ended.ok());
	REQUIRE(sink.posted.size() == 1);
	REQUIRE(sink.posted[0].first == NeutrinoMessages::EVT_PLAY_RECORDING);
	for (size_t i = 0; i < sink.posted.size(); ++i)
		delete[] (unsigned char *) sink.posted[i].second;
}

TEST_CASE("nothing in the never exposed set is offered", "[mcp-boxtools]")
{
	setRoutesForTest(NULL);
	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	for (size_t i = 0; i < tables; ++i)
		for (size_t k = 0; k < t[i]->tool_count; ++k)
		{
			INFO(t[i]->tools[k].path);
			REQUIRE_FALSE(mcp::neverExposed(t[i]->tools[k].method, t[i]->tools[k].path));
		}
}

TEST_CASE("the box offers the grid, logo, timeshift, mode, bouquet, tuner, plugin, settings, timer and screenshot tools at their levels", "[boxtools]")
{
	const std::vector<mcp::ToolDef> all = mcp::boxTools().list();
	REQUIRE(mcp::boxToolsRefusal().empty());
	REQUIRE(all.size() == kOfferedTools);
	struct Want { const char *name; AuthLevel level; bool read_only; bool image; };
	const Want want[] = {
		{ "epg_grid", AuthLevel::Read, true, false },
		{ "channel_logo", AuthLevel::Read, true, true },
		{ "timeshift_start", AuthLevel::Write, false, false },
		{ "timeshift_stop", AuthLevel::Write, false, false },
		{ "set_mode", AuthLevel::Write, false, false },
		{ "create_bouquet", AuthLevel::Write, false, false },
		{ "delete_bouquet", AuthLevel::Write, false, false },
		{ "rename_bouquet", AuthLevel::Write, false, false },
		{ "move_bouquet", AuthLevel::Write, false, false },
		{ "hide_bouquet", AuthLevel::Write, false, false },
		{ "lock_bouquet", AuthLevel::Write, false, false },
		{ "set_bouquet_channels", AuthLevel::Write, false, false },
		{ "list_tuners", AuthLevel::Read, true, false },
		{ "list_plugins", AuthLevel::Read, true, false },
		{ "settings_schema", AuthLevel::Read, true, false },
		{ "read_settings", AuthLevel::Read, true, false },
		{ "change_timer", AuthLevel::Write, false, false },
		{ "screenshot", AuthLevel::Read, true, true },
	};
	for (size_t w = 0; w < sizeof(want) / sizeof(want[0]); ++w)
	{
		const mcp::ToolDef *d = NULL;
		for (size_t i = 0; i < all.size(); ++i)
			d = all[i].name == want[w].name ? &all[i] : d;
		INFO(want[w].name);
		REQUIRE(d != NULL);
		REQUIRE(d->level == want[w].level);
		REQUIRE(d->read_only == want[w].read_only);
		REQUIRE(d->image == want[w].image);
	}
}

TEST_CASE("deleting a bouquet is marked destructive", "[boxtools]")
{
	const std::vector<mcp::ToolDef> all = mcp::boxTools().list();
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].name == "delete_bouquet")
			REQUIRE(all[i].destructive);
	}
}

TEST_CASE("the first epg_grid page fits in eight kilobytes", "[boxtools]")
{
	GridFixture g(20, 12);
	coreapi::Result<mcp::JsonText> r = mcp::boxTools().call(callerAt(AuthLevel::Read), "epg_grid",
		"{\"channels\":\"" + g.channelList() + "\",\"from\":2000000000,\"to\":2000010800}");
	REQUIRE(r.ok());
	REQUIRE(r.value().size() <= 8192);
}

TEST_CASE("epg_grid through the box keeps an explicit limit", "[boxtools]")
{
	setRoutesForTest(NULL);
	GridFixture g(20, 12);
	coreapi::Result<mcp::JsonText> r = mcp::boxTools().call(callerAt(AuthLevel::Read), "epg_grid",
		"{\"channels\":\"" + g.channelList() + "\",\"from\":2000000000,\"to\":2000010800,\"limit\":1}");
	REQUIRE(r.ok());
	::Json::Reader reader;
	::Json::Value body;
	REQUIRE(reader.parse(r.value(), body));
	REQUIRE(body["items"].size() == 1);
}

TEST_CASE("set_bouquet_channels through the box refuses a channels body of the wrong kind",
	  "[boxtools]")
{
	setRoutesForTest(NULL);
	InstalledDependencies deps;
	mcp::ToolSource &tools = mcp::boxTools();
	const char *wrong[] = {
		"{\"bouquet\":\"favourites\",\"channels\":\"283d0001\"}",
		"{\"bouquet\":\"favourites\",\"channels\":7}",
		"{\"bouquet\":\"favourites\",\"channels\":{\"a\":1}}",
		"{\"bouquet\":\"favourites\",\"channels\":[1,2]}",
	};
	for (size_t i = 0; i < sizeof(wrong) / sizeof(wrong[0]); ++i)
	{
		INFO(wrong[i]);
		const coreapi::Result<std::string> r = tools.call(callerAt(AuthLevel::Write), "set_bouquet_channels",
			wrong[i]);
		REQUIRE_FALSE(r.ok());
		REQUIRE(r.error().code == coreapi::ErrorCode::BadString);
		REQUIRE(r.error().message.find("channels") != std::string::npos);
	}
	REQUIRE(deps.channels.saves == 0);
}

TEST_CASE("with both allowlists filled the box offers both gated tools", "[boxtools][gate]")
{
	OpenAllowlists open;
	REQUIRE(mcp::boxTools().list().size() == kOfferedToolsGated);
}

TEST_CASE("now_playing reads what the television shows and only reads", "[mcp-boxtools]")
{
	InstalledDependencies deps;
	deps.channels.mode = NeutrinoModes::mode_tv;
	deps.channels.current.id = 0x2b66;
	deps.channels.current.name = "Das Erste HD";
	deps.channels.current_status = coreapi::Status::Ok;
	const std::vector<mcp::ToolDef> all = shipped().list();
	const mcp::ToolDef *tool = NULL;
	for (size_t i = 0; i < all.size(); ++i)
		if (all[i].name == "now_playing")
			tool = &all[i];
	REQUIRE(tool != NULL);
	REQUIRE(tool->read_only);
	REQUIRE(tool->level == AuthLevel::Read);
	REQUIRE(tool->group == mcp::GroupProgramme);
	const coreapi::Result<std::string> got = shipped().call(at(AuthLevel::Read), "now_playing", "{}");
	REQUIRE(got.ok());
	REQUIRE(got.value().find("\"source\":\"channel\"") != std::string::npos);
	REQUIRE(got.value().find("Das Erste HD") != std::string::npos);
}
