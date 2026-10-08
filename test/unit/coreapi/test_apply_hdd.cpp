/*
 * test_apply_hdd.cpp - the disk power apply group
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
#include "support/fakes.h"
#include "support/phaseenv.h"

#include <neutrinoMessages.h>

#include "coreapi/base/apply.h"
#include "coreapi/base/eventbus.h"
#include "coreapi/box/applyworker.h"
#include "coreapi/base/deps.h"
#include "coreapi/box/apply_hdd.h"
#include "coreapi/osd.h"
#include "coreapi/settings/settings.h"
#include "httpd/auth.h"
#include "httpd/endpoints.h"
#include "httpd/router.h"

#include <system/settings.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern SNeutrinoSettings g_settings;

using namespace coreapi;

namespace
{

/* The group hands the slow part to the apply worker and returns; a case looks at
   what was sent once the worker is done. */
Status waited(Status s)
{
	applyWorker().wait();
	return s;
}

struct KeptHddSettings
{
	int sleep, noise;
	KeptHddSettings() : sleep(g_settings.hdd_sleep), noise(g_settings.hdd_noise) {}
	~KeptHddSettings()
	{
		g_settings.hdd_sleep = sleep;
		g_settings.hdd_noise = noise;
	}
};

struct HddBox
{
	KeptHddSettings kept;
	PhaseEnvironment env;
	FakeHdd &hdd;

	HddBox() : env(ApplyPhase::Network), hdd(env.fake<FakeHdd>("hdd"))
	{
		resetApplyRegistry();
		resetSentHdd();
		registerApplyGroups();
	}
	~HddBox()
	{
		resetSentHdd();
		resetApplyRegistry();
	}

	void started()
	{
		REQUIRE(waited(runPhase(ApplyPhase::Network)) == Status::Ok);
		hdd.forget();
	}
};

bool hddSave() { return true; }

} // namespace

TEST_CASE("the disk power group holds the two keys and runs after the mounts", "[apply][hdd]")
{
	HddBox box;
	REQUIRE(groupOf("hdd_sleep") == &kHdIdleApplyGroup);
	REQUIRE(groupOf("hdd_noise") == &kHdIdleApplyGroup);
	REQUIRE(kHdIdleApplyGroup.phase == ApplyPhase::Network);
}

TEST_CASE("startup starts the idle daemon with the timeout of the setting", "[apply][hdd]")
{
	HddBox box;
	box.hdd.idle_daemon = true;
	box.hdd.disks.push_back("sda");
	g_settings.hdd_sleep = 60;
	REQUIRE(waited(runPhase(ApplyPhase::Network)) == Status::Ok);
	REQUIRE(box.hdd.restarts.size() == 1);
	REQUIRE(box.hdd.restarts[0] == 300);
	// The daemon is the one way to the disk then.
	REQUIRE(box.hdd.set.empty());
}

TEST_CASE("the two codes above the minutes are half an hour and an hour", "[apply][hdd]")
{
	HddBox box;
	box.hdd.idle_daemon = true;
	g_settings.hdd_sleep = 241;
	REQUIRE(waited(runPhase(ApplyPhase::Network)) == Status::Ok);
	REQUIRE(box.hdd.restarts.back() == 1800);

	g_settings.hdd_sleep = 242;
	REQUIRE(waited(applyKey("hdd_sleep")) == Status::Ok);
	REQUIRE(box.hdd.restarts.back() == 3600);

	g_settings.hdd_sleep = 240;
	REQUIRE(waited(applyKey("hdd_sleep")) == Status::Ok);
	REQUIRE(box.hdd.restarts.back() == 1200);
}

// An old settings file may hold a timeout below the first step.
TEST_CASE("a timeout below the first step is taken as the first step", "[apply][hdd]")
{
	HddBox box;
	box.hdd.idle_daemon = true;
	g_settings.hdd_sleep = 12;
	REQUIRE(waited(runPhase(ApplyPhase::Network)) == Status::Ok);
	REQUIRE(box.hdd.restarts.back() == 300);
}

TEST_CASE("the sibling key does not start the idle daemon over", "[apply][hdd]")
{
	HddBox box;
	box.hdd.idle_daemon = true;
	g_settings.hdd_sleep = 120;
	box.started();

	g_settings.hdd_noise = 128;
	REQUIRE(waited(applyKey("hdd_noise")) == Status::Ok);
	REQUIRE(box.hdd.restarts.empty());

	g_settings.hdd_sleep = 240;
	REQUIRE(waited(applyKey("hdd_sleep")) == Status::Ok);
	REQUIRE(box.hdd.restarts.size() == 1);
	REQUIRE(box.hdd.restarts[0] == 1200);
}

TEST_CASE("a refused start of the idle daemon is tried again by the next run", "[apply][hdd]")
{
	HddBox box;
	box.hdd.idle_daemon = true;
	g_settings.hdd_sleep = 60;
	box.started();

	box.hdd.idle_answer = Status::Internal;
	g_settings.hdd_sleep = 120;
	// The worker meets the refusal after the run has answered.
	REQUIRE(waited(applyKey("hdd_sleep")) == Status::Ok);
	REQUIRE(box.hdd.restarts.size() == 1);
	box.hdd.idle_answer = Status::Ok;
	REQUIRE(waited(applyKey("hdd_noise")) == Status::Ok);
	REQUIRE(box.hdd.restarts.size() == 2);
	REQUIRE(waited(applyKey("hdd_noise")) == Status::Ok);
	REQUIRE(box.hdd.restarts.size() == 2);
}

TEST_CASE("without the idle daemon or with no timeout every disk is told with hdparm", "[apply][hdd]")
{
	HddBox box;
	box.hdd.hdparm = true;
	box.hdd.disks.push_back("sda");
	box.hdd.disks.push_back("sdb");
	g_settings.hdd_sleep = 120;
	g_settings.hdd_noise = 190;
	REQUIRE(waited(runPhase(ApplyPhase::Network)) == Status::Ok);
	REQUIRE(box.hdd.set.size() == 2);
	REQUIRE(box.hdd.set[0] == "sda:190:120:1");
	REQUIRE(box.hdd.set[1] == "sdb:190:120:1");

	// An idle daemon with no timeout wanted leaves the disks to hdparm.
	box.hdd.forget();
	box.hdd.idle_daemon = true;
	g_settings.hdd_sleep = 0;
	REQUIRE(waited(applyKey("hdd_sleep")) == Status::Ok);
	REQUIRE(box.hdd.restarts.empty());
	REQUIRE(box.hdd.set.size() == 2);
	REQUIRE(box.hdd.set[0] == "sda:190:0:1");
}

TEST_CASE("the busybox tool is told the timeout only", "[apply][hdd]")
{
	HddBox box;
	box.hdd.hdparm = true;
	box.hdd.full_hdparm = false;
	box.hdd.disks.push_back("sda");
	g_settings.hdd_sleep = 60;
	g_settings.hdd_noise = 254;
	box.started();

	// A noise level the tool cannot take is not a change.
	g_settings.hdd_noise = 128;
	REQUIRE(waited(applyKey("hdd_noise")) == Status::Ok);
	REQUIRE(box.hdd.set.empty());

	g_settings.hdd_sleep = 120;
	REQUIRE(waited(applyKey("hdd_sleep")) == Status::Ok);
	REQUIRE(box.hdd.set.size() == 1);
	REQUIRE(box.hdd.set[0] == "sda:128:120:0");
}

TEST_CASE("hdparm is told again when a disk has come since", "[apply][hdd]")
{
	HddBox box;
	box.hdd.hdparm = true;
	box.hdd.disks.push_back("sda");
	g_settings.hdd_sleep = 60;
	box.started();

	REQUIRE(waited(applyKey("hdd_noise")) == Status::Ok);
	REQUIRE(box.hdd.set.empty());

	box.hdd.disks.push_back("sdb");
	REQUIRE(waited(applyKey("hdd_noise")) == Status::Ok);
	REQUIRE(box.hdd.set.size() == 2);
}

TEST_CASE("a box with neither tool sends nothing and answers ok", "[apply][hdd]")
{
	HddBox box;
	box.hdd.disks.push_back("sda");
	REQUIRE(waited(runPhase(ApplyPhase::Network)) == Status::Ok);
	REQUIRE(box.hdd.restarts.empty());
	REQUIRE(box.hdd.set.empty());
}

TEST_CASE("a web batch writing both disk keys runs the group once", "[apply][hdd]")
{
	HddBox box;
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, hddSave);
	box.hdd.hdparm = true;
	box.hdd.disks.push_back("sda");
	g_settings.hdd_sleep = 60;
	g_settings.hdd_noise = 254;
	box.started();

	REQUIRE(settings::set("hdd_sleep", "120").ok());
	REQUIRE(settings::set("hdd_noise", "128").ok());
	REQUIRE(box.hdd.set.empty());
	const size_t posted = sink.posted.size();
	applyPendingSettings();
	applyWorker().wait();

	REQUIRE(box.hdd.set.size() == 1);
	REQUIRE(box.hdd.set[0] == "sda:128:120:1");
	REQUIRE(events.sent.empty());
	for (size_t i = posted; i < sink.posted.size(); ++i)
		CHECK(sink.posted[i].first == NeutrinoMessages::EVT_SETTINGS_WRITTEN);

	installRealSettingsSource(NULL, NULL);
}

namespace
{

struct ApplyFailures : public Subscriber
{
	std::mutex m;
	std::vector<Event> seen;
	ApplyFailures() { EventBus::instance().subscribe(this); }
	void onEvent(const Event &e)
	{
		if (e.type != EventType::SettingApplyFailed)
			return;
		std::lock_guard<std::mutex> lock(m);
		seen.push_back(e);
	}
};

} // namespace

/* A write over the network is answered before the daemon is told, so a refusal is said on the
   bus afterwards, with the key and who wrote it, and the menu's own write says it is the box's. */
TEST_CASE("a refused daemon start is published with the key and who wrote it", "[apply][hdd]")
{
	HddBox box;
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, hddSave);
	box.hdd.idle_daemon = true;
	g_settings.hdd_sleep = 60;
	box.started();
	box.hdd.idle_answer = Status::Internal;
	ApplyFailures watch;
	// This thread is the loop, so a write from another one is a write over the network.
	bindApplyLoop();

	// Written on a thread that is not the loop, the way a web or MCP request writes.
	bool written = false;
	std::thread web([&written]() { written = settings::set("hdd_sleep", "120").ok(); });
	web.join();
	REQUIRE(written);
	applyPendingSettings();
	applyWorker().wait();
	REQUIRE(watch.seen.size() == 1);
	CHECK(watch.seen[0].text == "hdd_sleep");
	CHECK(watch.seen[0].value == 500);
	CHECK(watch.seen[0].initiator == "remote");

	// The loop's own write, the way a menu writes.
	REQUIRE(settings::set("hdd_sleep", "240", NULL, true).ok());
	applyWorker().wait();
	REQUIRE(watch.seen.size() == 2);
	CHECK(watch.seen[1].initiator == "box");

	installRealSettingsSource(NULL, NULL);
}

/* Finer than over the network: the writer a route names is carried from the write through
   the store and the drain into the job, and two writers of one drain are both named. */
TEST_CASE("a refused daemon start names the writer the write was made for", "[apply][hdd][writer]")
{
	HddBox box;
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, hddSave);
	box.hdd.idle_daemon = true;
	g_settings.hdd_sleep = 60;
	box.started();
	box.hdd.idle_answer = Status::Internal;
	ApplyFailures watch;
	bindApplyLoop();

	bool written = false;
	std::thread web([&written]() { written = settings::set("hdd_sleep", "120", NULL, false, "web:7").ok(); });
	web.join();
	REQUIRE(written);
	applyPendingSettings();
	applyWorker().wait();
	REQUIRE(watch.seen.size() == 1);
	CHECK(watch.seen[0].initiator == "web:7");

	// An in-process caller names its writer around the call, as the MCP side does.
	std::thread mcp([&written]()
	{
		const WriterScope who("mcp:g1");
		std::vector<std::pair<std::string, std::string> > one(1, std::make_pair(std::string("hdd_sleep"), std::string("240")));
		settings::Refusals failed;
		settings::writeBatch(one, failed);
		written = failed.empty();
	});
	mcp.join();
	REQUIRE(written);
	std::thread other([&written]() { written = settings::set("hdd_sleep", "241", NULL, false, "web:8").ok(); });
	other.join();
	REQUIRE(written);
	applyPendingSettings();
	applyWorker().wait();
	REQUIRE(watch.seen.size() == 2);
	CHECK(watch.seen[1].initiator == "mcp:g1 web:8");

	// A writer of another group in the same drain is not told of this group's failure.
	std::thread own([&written]() { written = settings::set("hdd_sleep", "242", NULL, false, "web:10").ok(); });
	own.join();
	REQUIRE(written);
	const std::string digits = g_settings.volume_digits ? "0" : "1";
	std::thread stranger([&written, &digits]() { written = settings::set("volume_digits", digits, NULL, false, "web:11").ok(); });
	stranger.join();
	REQUIRE(written);
	applyPendingSettings();
	applyWorker().wait();
	REQUIRE(watch.seen.size() == 3);
	CHECK(watch.seen[2].initiator == "web:10");

	installRealSettingsSource(NULL, NULL);
}

TEST_CASE("with no seam installed the disk power group finds no tool and does nothing", "[apply][hdd]")
{
	PhaseEnvironment env(ApplyPhase::Network);
	resetApplyRegistry();
	resetSentHdd();
	setHddControl(NULL);
	registerApplyGroups();
	// No tool is found, so there is nothing to ask for and nothing has failed.
	REQUIRE(waited(runPhase(ApplyPhase::Network)) == Status::Ok);
	resetSentHdd();
	resetApplyRegistry();
}

/* hdparm waits for a disk that sleeps to spin up, and a web write runs the group on
   the program's loop. */
TEST_CASE("a run hands hdparm to the worker and returns while a disk spins up", "[apply][hdd]")
{
	HddBox box;
	box.hdd.hdparm = true;
	box.hdd.disks.push_back("sda");
	g_settings.hdd_sleep = 60;
	g_settings.hdd_noise = 128;
	box.started();

	std::mutex m;
	std::condition_variable cv;
	bool inside = false, open = false, gave_up = false;
	box.hdd.before = [&]() {
		std::unique_lock<std::mutex> lock(m);
		inside = true;
		cv.notify_all();
		if (!cv.wait_for(lock, std::chrono::seconds(2), [&]() { return open; }))
			gave_up = true;
	};

	g_settings.hdd_sleep = 120;
	REQUIRE(applyKey("hdd_sleep") == Status::Ok);
	{
		std::unique_lock<std::mutex> lock(m);
		cv.wait_for(lock, std::chrono::seconds(5), [&]() { return inside; });
		open = true;
		cv.notify_all();
	}
	applyWorker().wait();
	REQUIRE_FALSE(gave_up);
	REQUIRE(box.hdd.set.size() == 1);
	REQUIRE(box.hdd.set[0] == "sda:128:120:1");
}

// The route takes the writer from the session the request carries, never from the token's text.
TEST_CASE("a write over a browser session is reported to that session's writer", "[apply][hdd][writer]")
{
	HddBox box;
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, hddSave);
	box.hdd.idle_daemon = true;
	g_settings.hdd_sleep = 60;
	box.started();
	box.hdd.idle_answer = Status::Internal;
	ApplyFailures watch;
	bindApplyLoop();

	const std::string token = httpd::openSession("root");
	REQUIRE_FALSE(token.empty());
	const std::string writer = httpd::sessionWriter(token);
	REQUIRE(writer.compare(0, 4, "web:") == 0);
	CHECK(writer.find(token) == std::string::npos);

	int code = 0;
	std::thread web([&code, &token]()
	{
		code = httpd::dispatchIn(httpd::settingsTable, httpd::Patch, "/api/v1/settings/hdd", "",
		                         "{\"hdd_sleep\":\"120\"}", "127.0.0.1", httpd::AuthLevel::System,
		                         std::string(), token).code;
	});
	web.join();
	REQUIRE(code == 200);
	applyPendingSettings();
	applyWorker().wait();
	REQUIRE(watch.seen.size() == 1);
	CHECK(watch.seen[0].initiator == writer);

	httpd::closeSession(token);
	installRealSettingsSource(NULL, NULL);
}

namespace
{

const char *const kTraceKeys[] = { "weather_api_key", "mode_icons", "mode_icons_skin" };

// A group whose job always fails, so the event it publishes says who the write was made for.
Status runFailingJob()
{
	const std::string who = applyInitiator();
	applyWorker().post("writer.trace", [who]() { publishApplyFailed("trace", Status::Internal, who); }, []() {});
	return Status::Ok;
}

const ApplyGroup kTraceGroup = { "writer.trace", ApplyPhase::Network, COREAPI_KEYS(kTraceKeys), &runFailingJob };

struct TraceBox
{
	FakeCommandSink sink;
	InstalledSink sunk;
	FakeEventSink events;
	InstalledEventSink evented;
	ClearedSettingsSource cleared;
	PhaseEnvironment env;

	TraceBox() : sunk(&sink), evented(&events), env(ApplyPhase::Network)
	{
		resetApplyRegistry();
		REQUIRE(registerApplyGroup(&kTraceGroup) == Status::Ok);
		installRealSettingsSource(&g_settings, hddSave);
		REQUIRE(waited(runPhase(ApplyPhase::Network)) == Status::Ok);
	}
	~TraceBox()
	{
		installRealSettingsSource(NULL, NULL);
		resetApplyRegistry();
	}
};

} // namespace

/* Emptying a credential and putting the icons in a state are writes like any other, so a
   job that fails after them names the writer, whether the call named it or the route took
   the session's. */
TEST_CASE("a refused apply after a credential is cleared names the writer", "[apply][writer]")
{
	TraceBox box;
	const std::string kept = g_settings.weather_api_key;
	ApplyFailures watch;
	bindApplyLoop();

	g_settings.weather_api_key = "0123456789abcdef0123456789abcdef";
	bool written = false;
	std::thread named([&written]() { written = settings::clearSecret("weather_api_key", "web:9").ok(); });
	named.join();
	REQUIRE(written);
	applyPendingSettings();
	applyWorker().wait();
	REQUIRE(watch.seen.size() == 1);
	CHECK(watch.seen[0].initiator == "web:9");

	g_settings.weather_api_key = "0123456789abcdef0123456789abcdef";
	const std::string token = httpd::openSession("root");
	REQUIRE_FALSE(token.empty());
	int code = 0;
	std::thread web([&code, &token]()
	{
		code = httpd::dispatchIn(httpd::settingsTable, httpd::Post, "/api/v1/settings/secret/clear", "",
		                         "{\"key\":\"weather_api_key\"}", "127.0.0.1", httpd::AuthLevel::System,
		                         std::string(), token).code;
	});
	web.join();
	REQUIRE(code == 200);
	applyPendingSettings();
	applyWorker().wait();
	REQUIRE(watch.seen.size() == 2);
	CHECK(watch.seen[1].initiator == httpd::sessionWriter(token));

	httpd::closeSession(token);
	g_settings.weather_api_key = kept;
}

TEST_CASE("a refused apply after the info icons are set names the writer", "[apply][writer]")
{
	TraceBox box;
	const int mode = g_settings.mode_icons;
	const int skin = g_settings.mode_icons_skin;
	ApplyFailures watch;
	bindApplyLoop();

	g_settings.mode_icons = 0;
	g_settings.mode_icons_skin = 0;
	bool written = false;
	std::thread named([&written]() { written = osd::setInfoIcons(osd::InfoIcons::Popup, "web:4").ok(); });
	named.join();
	REQUIRE(written);
	applyPendingSettings();
	applyWorker().wait();
	REQUIRE(watch.seen.size() == 1);
	CHECK(watch.seen[0].initiator == "web:4");

	const std::string token = httpd::openSession("root");
	REQUIRE_FALSE(token.empty());
	int code = 0;
	std::thread web([&code, &token]()
	{
		code = httpd::dispatchIn(httpd::osdTable, httpd::Put, "/api/v1/osd/infoicons", "",
		                         "{\"state\":\"infoviewer\"}", "127.0.0.1", httpd::AuthLevel::Write,
		                         std::string(), token).code;
	});
	web.join();
	REQUIRE(code == 204);
	applyPendingSettings();
	applyWorker().wait();
	REQUIRE(watch.seen.size() == 2);
	CHECK(watch.seen[1].initiator == httpd::sessionWriter(token));

	httpd::closeSession(token);
	g_settings.mode_icons = mode;
	g_settings.mode_icons_skin = skin;
}
