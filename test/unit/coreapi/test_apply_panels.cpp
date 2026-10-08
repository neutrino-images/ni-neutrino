/*
 * test_apply_panels.cpp - tests for the apply groups of the weather, the panel displays and the plugins
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
#include "support/nothreads.h"
#include "support/phaseenv.h"

#include <neutrinoMessages.h>

#include "coreapi/base/apply.h"
#include "coreapi/base/deps.h"
#include "coreapi/box/apply_glcd.h"
#include "coreapi/box/apply_lcd4l.h"
#include "coreapi/box/apply_plugins.h"
#include "coreapi/box/apply_vfd.h"
#include "coreapi/box/applyworker.h"
#include "coreapi/box/apply_weather.h"
#include "coreapi/settings/settings.h"
#include "coreapi/settings/settingstable.h"

#include <system/settings.h>

#include <pthread.h>

#include <condition_variable>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

extern SNeutrinoSettings g_settings;

using namespace coreapi;

namespace
{

// Every case starts from an empty registry: it is process wide.
struct Fresh
{
	Fresh() { resetApplyRegistry(); }
	~Fresh() { resetApplyRegistry(); }
};

bool savedNothing() { return true; }

/* The web path as a case sees it: a settings source over the program's own
   settings, a sink for what the drain posts, and nothing else. */
struct WebWrites
{
	FakeCommandSink sink;
	InstalledSink sunk;
	FakeEventSink events;
	InstalledEventSink evented;
	ClearedSettingsSource cleared;

	WebWrites() : sunk(&sink), evented(&events) { installRealSettingsSource(&g_settings, savedNothing); }
	~WebWrites() { installRealSettingsSource(NULL, NULL); }

	// What the drain posts besides is its word that the settings landed, which asks nobody anything.
	bool postedOnlyTheLandingWord(size_t before) const
	{
		for (size_t i = before; i < sink.posted.size(); ++i)
			if (sink.posted[i].first != NeutrinoMessages::EVT_SETTINGS_WRITTEN)
				return false;
		return events.sent.empty();
	}
};

} // namespace

/* ------------------------------------------------------------------ plugins */

namespace
{

struct KeptPluginSettings
{
	std::string game, script;
	KeptPluginSettings() : game(g_settings.plugins_game), script(g_settings.plugins_script) {}
	~KeptPluginSettings()
	{
		g_settings.plugins_game = game;
		g_settings.plugins_script = script;
	}
};

} // namespace

TEST_CASE("the plugin folder and the five type lists are one group that runs once the list exists", "[apply][plugins]")
{
	Fresh fresh;
	registerApplyGroups();
	const char *const keys[] = { "plugin_hdd_dir", "plugins_disabled", "plugins_game", "plugins_lua",
				     "plugins_script", "plugins_tool" };
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		INFO(keys[i]);
		REQUIRE(groupOf(keys[i]) == &kPluginsApplyGroup);
	}
	REQUIRE(kPluginsApplyGroup.phase == ApplyPhase::Network);
}

TEST_CASE("startup reads the plugin list once", "[apply][plugins]")
{
	Fresh fresh;
	PhaseEnvironment env(ApplyPhase::Network);
	registerApplyGroups();
	runPhase(ApplyPhase::Network);
	REQUIRE(env.fake<FakePluginLoader>("pluginload").reloads == 1);
}

TEST_CASE("a change on the box reads the plugin list once, whichever key of the group it was", "[apply][plugins]")
{
	Fresh fresh;
	PhaseEnvironment env(ApplyPhase::Network);
	FakePluginLoader &loader = env.fake<FakePluginLoader>("pluginload");
	registerApplyGroups();
	runPhase(ApplyPhase::Network);
	loader.reloads = 0;

	REQUIRE(applyKey("plugins_game") == Status::Ok);
	REQUIRE(loader.reloads == 1);
	REQUIRE(applyKey("plugin_hdd_dir") == Status::Ok);
	REQUIRE(loader.reloads == 2);
	// A key that is read where it is used does not touch the list.
	REQUIRE(applyKey("plugin_dir_shown") == Status::Ok);
	REQUIRE(loader.reloads == 2);
}

TEST_CASE("a web batch of two plugin lists reads the plugin list once", "[apply][plugins]")
{
	Fresh fresh;
	KeptPluginSettings kept;
	PhaseEnvironment env(ApplyPhase::Network);
	FakePluginLoader &loader = env.fake<FakePluginLoader>("pluginload");
	WebWrites web;
	registerApplyGroups();
	runPhase(ApplyPhase::Network);
	loader.reloads = 0;

	REQUIRE(settings::set("plugins_game", "chess.cfg").ok());
	REQUIRE(settings::set("plugins_script", "backup.cfg").ok());
	const size_t posted = web.sink.posted.size();
	REQUIRE(loader.reloads == 0);
	applyPendingSettings();

	REQUIRE(loader.reloads == 1);
	REQUIRE(web.postedOnlyTheLandingWord(posted));
}

TEST_CASE("the five plugin lists written as one answer read the list once and stay one partition", "[apply][plugins]")
{
	Fresh fresh;
	KeptPluginSettings kept;
	const std::string kept_tool = g_settings.plugins_tool;
	PhaseEnvironment env(ApplyPhase::Network);
	FakePluginLoader &loader = env.fake<FakePluginLoader>("pluginload");
	WebWrites web;
	registerApplyGroups();
	runPhase(ApplyPhase::Network);
	loader.reloads = 0;
	g_settings.plugins_game = "chess.cfg,tetris.cfg";
	g_settings.plugins_tool = "";

	// What the personalisation menu answers when chess is made a tool.
	std::vector<std::pair<std::string, std::string> > members;
	members.push_back(std::make_pair(std::string("plugins_disabled"), std::string()));
	members.push_back(std::make_pair(std::string("plugins_game"), std::string("tetris.cfg")));
	members.push_back(std::make_pair(std::string("plugins_tool"), std::string("chess.cfg")));
	members.push_back(std::make_pair(std::string("plugins_script"), std::string()));
	members.push_back(std::make_pair(std::string("plugins_lua"), std::string()));
	settings::Refusals failed;
	settings::writeBatch(members, failed, true);

	REQUIRE(failed.empty());
	REQUIRE(loader.reloads == 1);
	REQUIRE(g_settings.plugins_game == "tetris.cfg");
	REQUIRE(g_settings.plugins_tool == "chess.cfg");
	g_settings.plugins_tool = kept_tool;
}

TEST_CASE("a plugin list that cannot be read again is reported by the group", "[apply][plugins]")
{
	Fresh fresh;
	PhaseEnvironment env(ApplyPhase::Network);
	env.fake<FakePluginLoader>("pluginload").answer = Status::Internal;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Internal);
}

TEST_CASE("the hide flag of a plugin asks for the same reread without a setting", "[apply][plugins]")
{
	PhaseEnvironment env(ApplyPhase::Network);
	FakePluginLoader &loader = env.fake<FakePluginLoader>("pluginload");
	REQUIRE(reloadPlugins() == Status::Ok);
	REQUIRE(loader.reloads == 1);
}

/* ------------------------------------------------------------------ weather */

namespace
{

struct KeptWeatherSettings
{
	std::string key, version, city, location;
	KeptWeatherSettings()
		: key(g_settings.weather_api_key), version(g_settings.weather_api_version),
		  city(g_settings.weather_city), location(g_settings.weather_location) {}
	~KeptWeatherSettings()
	{
		g_settings.weather_api_key = key;
		g_settings.weather_api_version = version;
		g_settings.weather_city = city;
		g_settings.weather_location = location;
	}
};

} // namespace

TEST_CASE("the place and the access of the weather are one group that runs after the network", "[apply][weather]")
{
	Fresh fresh;
	registerApplyGroups();
	const char *const keys[] = { "weather_api_key", "weather_api_version", "weather_city", "weather_location" };
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		INFO(keys[i]);
		REQUIRE(groupOf(keys[i]) == &kWeatherApplyGroup);
	}
	REQUIRE(kWeatherApplyGroup.phase == ApplyPhase::Network);
	// The switch is read where the weather is drawn and the code only starts a lookup.
	REQUIRE(groupOf("weather_enabled") == NULL);
	REQUIRE(groupOf("weather_postalcode") == NULL);
}

TEST_CASE("startup only learns, and the application's own fetch gives the weather its place before its access", "[apply][weather]")
{
	Fresh fresh;
	KeptWeatherSettings kept;
	resetSentWeather();
	PhaseEnvironment env(ApplyPhase::Network);
	FakeWeatherService &w = env.fake<FakeWeatherService>("weather");
	g_settings.weather_location = "48.14,11.58";
	g_settings.weather_city = "Muenchen";
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	REQUIRE(w.calls.empty());

	REQUIRE(fetchWeather() == Status::Ok);
	REQUIRE(w.calls.size() == 2);
	REQUIRE(w.calls[0] == "place");
	REQUIRE(w.calls[1] == "api");
	REQUIRE(w.places[0].first == "48.14,11.58");
	REQUIRE(w.places[0].second == "Muenchen");
	// The access travels as the settings held it.
	REQUIRE(w.accesses.size() == 1);
	REQUIRE(w.accesses[0].first == g_settings.weather_api_key);
	REQUIRE(w.accesses[0].second == g_settings.weather_api_version);
}

TEST_CASE("a change on the box runs the weather group once and carries the settings as they are", "[apply][weather]")
{
	Fresh fresh;
	resetSentWeather();
	KeptWeatherSettings kept;
	PhaseEnvironment env(ApplyPhase::Network);
	FakeWeatherService &w = env.fake<FakeWeatherService>("weather");
	registerApplyGroups();
	runPhase(ApplyPhase::Network);
	w.forget();
	REQUIRE(w.calls.empty());

	g_settings.weather_location = "52.52,13.40";
	g_settings.weather_city = "Berlin";
	REQUIRE(applyKey("weather_location") == Status::Ok);
	REQUIRE(w.calls.size() == 2);
	REQUIRE(w.places.size() == 1);
	REQUIRE(w.places[0].first == "52.52,13.40");
	REQUIRE(w.places[0].second == "Berlin");

	w.forget();
	REQUIRE(applyKey("weather_api_version") == Status::Ok);
	REQUIRE(w.calls.size() == 2);

	// The switch is read where it is used.
	w.forget();
	REQUIRE(applyKey("weather_enabled") == Status::Ok);
	REQUIRE(w.calls.empty());
}

// A file loaded over the settings that changes only the key gives the weather the new access.
TEST_CASE("a loaded file that changes only the weather key runs the weather group", "[apply][weather]")
{
	Fresh fresh;
	resetSentWeather();
	KeptWeatherSettings kept;
	PhaseEnvironment env(ApplyPhase::Network);
	FakeWeatherService &w = env.fake<FakeWeatherService>("weather");
	WebWrites web;
	g_settings.weather_api_key = "0123456789abcdef0123456789abcdef";
	registerApplyGroups();
	runPhase(ApplyPhase::Network);
	w.forget();

	REQUIRE(settings::applyReplaced([]() { g_settings.weather_api_key = "fedcba9876543210fedcba9876543210"; }) == Status::Ok);
	REQUIRE(w.accesses.size() == 1);
	REQUIRE(w.accesses[0].first == "fedcba9876543210fedcba9876543210");
}

TEST_CASE("a web batch of the key and the version runs the weather group once", "[apply][weather]")
{
	Fresh fresh;
	resetSentWeather();
	KeptWeatherSettings kept;
	PhaseEnvironment env(ApplyPhase::Network);
	FakeWeatherService &w = env.fake<FakeWeatherService>("weather");
	WebWrites web;
	registerApplyGroups();
	runPhase(ApplyPhase::Network);
	w.forget();

	REQUIRE(settings::set("weather_api_key", "0123456789abcdef0123456789abcdef").ok());
	REQUIRE(settings::set("weather_api_version", "2.5").ok());
	const size_t posted = web.sink.posted.size();
	REQUIRE(w.calls.empty());
	applyPendingSettings();

	REQUIRE(w.calls.size() == 2);
	REQUIRE(web.postedOnlyTheLandingWord(posted));
}

TEST_CASE("a place the service refused does not keep the access from being refreshed", "[apply][weather]")
{
	Fresh fresh;
	resetSentWeather();
	PhaseEnvironment env(ApplyPhase::Network);
	FakeWeatherService &w = env.fake<FakeWeatherService>("weather");
	w.place_answer = Status::Internal;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	REQUIRE(applyKey("weather_city") == Status::Internal);
	REQUIRE(w.calls.size() == 2);
}

namespace
{

/* A fetch that takes as long as the case lets it, and records how many ran at once. */
struct SlowFetch
{
	std::mutex m;
	std::condition_variable cv;
	bool release;
	bool inside;
	int running, most, runs;
	std::vector<WeatherJob> jobs;
	SlowFetch() : release(false), inside(false), running(0), most(0), runs(0) {}
};

SlowFetch *g_slow = 0;

void slowRun(const WeatherJob &job)
{
	std::unique_lock<std::mutex> lock(g_slow->m);
	++g_slow->running;
	g_slow->most = g_slow->running > g_slow->most ? g_slow->running : g_slow->most;
	++g_slow->runs;
	g_slow->jobs.push_back(job);
	g_slow->inside = true;
	g_slow->cv.notify_all();
	g_slow->cv.wait(lock, []() { return g_slow->release; });
	--g_slow->running;
}

void letGo(SlowFetch &f)
{
	std::lock_guard<std::mutex> lock(f.m);
	f.release = true;
	f.cv.notify_all();
}

void waitInside(SlowFetch &f)
{
	std::unique_lock<std::mutex> lock(f.m);
	f.cv.wait(lock, [&f]() { return f.inside; });
}

} // namespace

/* A fetch is an HTTP request of up to a minute and the group runs on the program's
   loop, so the group has to hand it over and return while it is still going. */
TEST_CASE("the weather group returns while the fetch it started is still running", "[apply][weather]")
{
	Fresh fresh;
	resetSentWeather();
	KeptWeatherSettings kept;
	SlowFetch fetch;
	g_slow = &fetch;
	{
		PhaseEnvironment env(ApplyPhase::Network);
		QueuedWeatherService service(&slowRun);
		setWeatherService(&service);
		registerApplyGroups();
		REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);

		g_settings.weather_location = "52.52,13.40";
		g_settings.weather_city = "Berlin";
		REQUIRE(applyKey("weather_location") == Status::Ok);
		waitInside(fetch);
		// The fetch has not been let go, and the group is back.
		{
			std::lock_guard<std::mutex> lock(fetch.m);
			REQUIRE(fetch.running == 1);
			REQUIRE_FALSE(fetch.release);
		}
		letGo(fetch);
		service.wait();
		setWeatherService(0);
	}
	REQUIRE(fetch.runs >= 1);
	REQUIRE(fetch.jobs[0].place);
	REQUIRE(fetch.jobs[0].coords == "52.52,13.40");
	g_slow = 0;
}

/* The thread that fetches reads no setting that holds text, so the key a job fetches with
   is the one that was current when it was asked for, whatever is written before it runs. */
TEST_CASE("a weather job carries the key and the version that were current when it was posted", "[apply][weather]")
{
	Fresh fresh;
	resetSentWeather();
	KeptWeatherSettings kept;
	SlowFetch fetch;
	g_slow = &fetch;
	{
		PhaseEnvironment env(ApplyPhase::Network);
		QueuedWeatherService service(&slowRun);
		setWeatherService(&service);
		registerApplyGroups();
		REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);

		g_settings.weather_api_key = "key-at-post-time";
		g_settings.weather_api_version = "3.0";
		REQUIRE(fetchWeather() == Status::Ok);
		// Written after the post and before the worker is let go.
		g_settings.weather_api_key = "written-later";
		g_settings.weather_api_version = "9.9";
		waitInside(fetch);
		letGo(fetch);
		service.wait();
		setWeatherService(0);
	}
	size_t with_api = 0;
	for (size_t i = 0; i < fetch.jobs.size(); ++i)
	{
		if (!fetch.jobs[i].api)
			continue;
		++with_api;
		CHECK(fetch.jobs[i].api_key == "key-at-post-time");
		CHECK(fetch.jobs[i].api_version == "3.0");
	}
	REQUIRE(with_api == 1);
	g_slow = 0;
}

TEST_CASE("writes while a weather fetch runs wait for it and are merged, never run beside it", "[apply][weather]")
{
	SlowFetch fetch;
	g_slow = &fetch;
	{
		WeatherWorker worker(&slowRun);
		WeatherJob first;
		first.place = true;
		first.coords = "1,1";
		first.city = "A";
		worker.post(first);
		waitInside(fetch);

		WeatherJob second;
		second.place = true;
		second.coords = "2,2";
		second.city = "B";
		worker.post(second);
		WeatherJob third;
		third.place = true;
		third.coords = "3,3";
		third.city = "C";
		third.api = true;
		worker.post(third);

		letGo(fetch);
		worker.wait();
	}
	REQUIRE(fetch.runs == 2);
	REQUIRE(fetch.most == 1);
	REQUIRE(fetch.jobs[0].coords == "1,1");
	REQUIRE(fetch.jobs[1].coords == "3,3");
	REQUIRE(fetch.jobs[1].api);
	g_slow = 0;
}

namespace
{

std::vector<WeatherJob> g_ran;

void recordRun(const WeatherJob &job)
{
	g_ran.push_back(job);
}

} // namespace

/* The program is built without exceptions, so a thread that cannot be made must not
   be reported by throwing: that would end it. */
TEST_CASE("a weather fetch that gets no thread is skipped and the next one runs", "[apply][weather]")
{
	g_ran.clear();
	WeatherWorker worker(&recordRun);
	{
		NoNewThreads none;
		REQUIRE_FALSE(NoNewThreads::canMake());
		WeatherJob lost;
		lost.place = true;
		lost.coords = "1,1";
		worker.post(lost);
	}
	worker.wait();
	REQUIRE(g_ran.empty());

	WeatherJob next;
	next.api = true;
	next.api_key = "k";
	worker.post(next);
	worker.wait();
	REQUIRE(g_ran.size() == 1);
	REQUIRE_FALSE(g_ran[0].place);
	REQUIRE(g_ran[0].api_key == "k");
}

/* ------------------------------------------------------------------ LCD4Linux */

// The settings and their rows exist only in a build with LCD4Linux, and so do these cases.
#ifdef ENABLE_LCD4LINUX

namespace
{

/* The group hands the restart and the rewrite to the apply worker and returns; a
   case looks at what was sent once the worker is done. */
Status waited(Status s)
{
	applyWorker().wait();
	return s;
}

struct KeptLcd4lSettings
{
	int support, type, skin, brightness, screenshots;
	KeptLcd4lSettings()
		: support(g_settings.lcd4l_support), type(g_settings.lcd4l_display_type), skin(g_settings.lcd4l_skin),
		  brightness(g_settings.lcd4l_brightness), screenshots(g_settings.lcd4l_screenshots)
	{
		resetSentLcd4l();
	}
	~KeptLcd4lSettings()
	{
		g_settings.lcd4l_support = support;
		g_settings.lcd4l_display_type = type;
		g_settings.lcd4l_skin = skin;
		g_settings.lcd4l_brightness = brightness;
		g_settings.lcd4l_screenshots = screenshots;
		resetSentLcd4l();
	}
};

/* A box that has run its startup with the service off, the way the application
   leaves it before it starts the service itself. */
struct Lcd4lBox
{
	KeptLcd4lSettings kept;
	PhaseEnvironment env;
	FakeLcd4lControl &control;

	Lcd4lBox() : env(ApplyPhase::Network), control(env.fake<FakeLcd4lControl>("lcd4l"))
	{
		g_settings.lcd4l_support = 0;
		registerApplyGroups();
		waited(runPhase(ApplyPhase::Network));
		control.forget();
	}
};

} // namespace

TEST_CASE("the mode, the panel, the skin, the brightness and the screenshots are one group", "[apply][lcd4l]")
{
	Fresh fresh;
	registerApplyGroups();
	const char *const keys[] = { "lcd4l_support", "lcd4l_display_type", "lcd4l_skin", "lcd4l_brightness", "lcd4l_screenshots" };
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		INFO(keys[i]);
		REQUIRE(groupOf(keys[i]) == &kLcd4lApplyGroup);
	}
	REQUIRE(kLcd4lApplyGroup.phase == ApplyPhase::Network);
	// Read where they are used, on every pass of the service.
	REQUIRE(groupOf("lcd4l_skin_radio") == NULL);
	REQUIRE(groupOf("lcd4l_convert") == NULL);
	REQUIRE(groupOf("lcd4l_logodir") == NULL);
}

TEST_CASE("startup starts and stops nothing: the application starts the service later", "[apply][lcd4l]")
{
	Fresh fresh;
	KeptLcd4lSettings kept;
	PhaseEnvironment env(ApplyPhase::Network);
	FakeLcd4lControl &control = env.fake<FakeLcd4lControl>("lcd4l");
	registerApplyGroups();
	g_settings.lcd4l_support = 2;
	REQUIRE(waited(runPhase(ApplyPhase::Network)) == Status::Ok);
	REQUIRE(control.calls.empty());

	// The automatic mode is told not to wait for the daemon.
	resetApplyRegistry();
	resetSentLcd4l();
	control.forget();
	g_settings.lcd4l_support = 1;
	registerApplyGroups();
	REQUIRE(waited(runPhase(ApplyPhase::Network)) == Status::Ok);
	REQUIRE(control.calls.size() == 1);
	REQUIRE(control.calls[0] == "force");
}

TEST_CASE("a change of the mode restarts the service once and rewrites nothing", "[apply][lcd4l]")
{
	Fresh fresh;
	Lcd4lBox box;

	g_settings.lcd4l_support = 2;
	REQUIRE(waited(applyKey("lcd4l_support")) == Status::Ok);
	REQUIRE(box.control.count("restart") == 1);
	REQUIRE(box.control.modes[0] == 2);
	REQUIRE(box.control.count("reinit") == 0);

	// A sibling key finds the mode as it was sent.
	box.control.forget();
	REQUIRE(waited(applyKey("lcd4l_skin")) == Status::Ok);
	REQUIRE(box.control.calls.empty());

	g_settings.lcd4l_support = 0;
	REQUIRE(waited(applyKey("lcd4l_support")) == Status::Ok);
	REQUIRE(box.control.count("restart") == 1);
	REQUIRE(box.control.modes[0] == 0);
}

TEST_CASE("a change of the panel, the skin, the brightness or the screenshots rewrites the service's files once", "[apply][lcd4l]")
{
	Fresh fresh;
	Lcd4lBox box;
	int *const members[] = { &g_settings.lcd4l_display_type, &g_settings.lcd4l_skin, &g_settings.lcd4l_brightness,
				 &g_settings.lcd4l_screenshots };
	const char *const keys[] = { "lcd4l_display_type", "lcd4l_skin", "lcd4l_brightness", "lcd4l_screenshots" };
	for (size_t i = 0; i < 4; ++i)
	{
		INFO(keys[i]);
		box.control.forget();
		*members[i] += 1;
		REQUIRE(waited(applyKey(keys[i])) == Status::Ok);
		REQUIRE(box.control.calls.size() == 1);
		REQUIRE(box.control.calls[0] == "reinit");
		// And not again for the key next to it.
		box.control.forget();
		REQUIRE(waited(applyKey("lcd4l_support")) == Status::Ok);
		REQUIRE(box.control.calls.empty());
	}
}

TEST_CASE("a service that was restarted writes its files itself and is not told to rewrite them", "[apply][lcd4l]")
{
	Fresh fresh;
	Lcd4lBox box;
	g_settings.lcd4l_support = 2;
	g_settings.lcd4l_skin += 1;
	REQUIRE(waited(applyKey("lcd4l_support")) == Status::Ok);
	REQUIRE(box.control.count("restart") == 1);
	REQUIRE(box.control.count("reinit") == 0);
	box.control.forget();
	REQUIRE(waited(applyKey("lcd4l_skin")) == Status::Ok);
	REQUIRE(box.control.calls.empty());
}

TEST_CASE("a restart the service refused is tried again by the next run", "[apply][lcd4l]")
{
	Fresh fresh;
	Lcd4lBox box;
	box.control.restart_answer = Status::Internal;
	g_settings.lcd4l_support = 2;
	// The worker meets the refusal after the run has answered.
	REQUIRE(waited(applyKey("lcd4l_support")) == Status::Ok);
	REQUIRE(box.control.count("restart") == 1);
	box.control.restart_answer = Status::Ok;
	box.control.forget();
	REQUIRE(waited(applyKey("lcd4l_skin")) == Status::Ok);
	REQUIRE(box.control.count("restart") == 1);
}

TEST_CASE("a web batch of the mode and the skin runs the LCD4Linux group once", "[apply][lcd4l]")
{
	Fresh fresh;
	// The box first: its phase environment installs a settings source of its own.
	Lcd4lBox box;
	WebWrites web;

	REQUIRE(settings::set("lcd4l_support", "2").ok());
	REQUIRE(settings::set("lcd4l_skin", "100").ok());
	const size_t posted = web.sink.posted.size();
	REQUIRE(box.control.calls.empty());
	applyPendingSettings();
	applyWorker().wait();

	REQUIRE(box.control.count("restart") == 1);
	REQUIRE(box.control.count("reinit") == 0);
	REQUIRE(web.postedOnlyTheLandingWord(posted));
}
#endif

/* ------------------------------------------------------------------ front panel */

namespace
{

struct KeptVfdSettings
{
	int brightness, standby, deep, statusline, scroll, led, backlight, volume, contrast, power, inverse;
	KeptVfdSettings()
		: brightness(g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS]),
		  standby(g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS]),
		  deep(g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS]),
		  statusline(g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME]),
		  scroll(g_settings.lcd_scroll), led(g_settings.led_tv_mode),
		  backlight(g_settings.backlight_tv), volume(g_settings.current_volume),
		  contrast(g_settings.lcd_setting[SNeutrinoSettings::LCD_CONTRAST]),
		  power(g_settings.lcd_setting[SNeutrinoSettings::LCD_POWER]),
		  inverse(g_settings.lcd_setting[SNeutrinoSettings::LCD_INVERSE])
	{
		resetSentVfd();
	}
	~KeptVfdSettings()
	{
		g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = brightness;
		g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] = standby;
		g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS] = deep;
		g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] = statusline;
		g_settings.lcd_scroll = scroll;
		g_settings.led_tv_mode = led;
		g_settings.backlight_tv = backlight;
		g_settings.current_volume = volume;
		g_settings.lcd_setting[SNeutrinoSettings::LCD_CONTRAST] = contrast;
		g_settings.lcd_setting[SNeutrinoSettings::LCD_POWER] = power;
		g_settings.lcd_setting[SNeutrinoSettings::LCD_INVERSE] = inverse;
		resetSentVfd();
	}
};

/* A box whose panel took the settings it was brought up with: the group has run
   its startup and nothing is sent since. */
struct VfdBox
{
	KeptVfdSettings kept;
	PhaseEnvironment env;
	FakeVfdPanel &panel;

	VfdBox() : env(ApplyPhase::Decoders), panel(env.fake<FakeVfdPanel>("vfd"))
	{
		env.fake<FakeSystemSource>("system").caps.display_can_set_brightness = true;
		g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = 15;
		g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] = 5;
		g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS] = 5;
		g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] = 1;
		g_settings.lcd_scroll = 1;
		g_settings.led_tv_mode = 2;
		g_settings.backlight_tv = 1;
		registerApplyGroups();
		runPhase(ApplyPhase::Decoders);
		panel.forget();
	}
};

} // namespace

TEST_CASE("the brightnesses, the scrolling, the LEDs, the backlight and the second line are one group", "[apply][vfd]")
{
	Fresh fresh;
	registerApplyGroups();
	const char *const keys[] = { "lcd_brightness", "lcd_standbybrightness", "lcd_deepbrightness", "lcd_scroll",
				     "lcd_show_volume", "led_tv_mode", "backlight_tv", "lcd_contrast", "lcd_power",
				     "lcd_inverse" };
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		INFO(keys[i]);
		REQUIRE(groupOf(keys[i]) == &kVfdApplyGroup);
	}
	REQUIRE(kVfdApplyGroup.phase == ApplyPhase::Decoders);
	// The driver reads the dim brightness when it dims, and the other modes' LEDs when it changes mode.
	REQUIRE(groupOf("lcd_dim_brightness") == NULL);
	REQUIRE(groupOf("led_standby_mode") == NULL);
}

TEST_CASE("startup sends the scrolling and the backlight, which it always sent, and only learns the rest", "[apply][vfd]")
{
	Fresh fresh;
	KeptVfdSettings kept;
	PhaseEnvironment env(ApplyPhase::Decoders);
	FakeVfdPanel &panel = env.fake<FakeVfdPanel>("vfd");
	g_settings.lcd_scroll = 3;
	g_settings.backlight_tv = 1;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);

	REQUIRE(panel.count("scroll") == 1);
	REQUIRE(panel.scrolls[0] == 3);
#ifndef ENABLE_LCD
	REQUIRE(panel.count("backlight") == 1);
	REQUIRE(panel.backlights[0] == 1);
#endif
	REQUIRE(panel.count("brightness") == 0);
	REQUIRE(panel.count("leds") == 0);
	REQUIRE(panel.count("statusline") == 0);
}

TEST_CASE("a changed brightness sends that brightness once and the others are left alone", "[apply][vfd]")
{
	Fresh fresh;
	VfdBox box;
	const int kinds[] = { VfdPanel::Normal, VfdPanel::Standby, VfdPanel::DeepStandby };
	const char *const keys[] = { "lcd_brightness", "lcd_standbybrightness", "lcd_deepbrightness" };
	const int members[] = { SNeutrinoSettings::LCD_BRIGHTNESS, SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS,
				SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS };
	for (size_t i = 0; i < 3; ++i)
	{
		INFO(keys[i]);
		box.panel.forget();
		g_settings.lcd_setting[members[i]] = 9;
		REQUIRE(applyKey(keys[i]) == Status::Ok);
		REQUIRE(box.panel.calls.size() == 1);
		REQUIRE(box.panel.brightness[0].first == kinds[i]);
		REQUIRE(box.panel.brightness[0].second == 9);

		// The key next to it finds it as it was sent.
		box.panel.forget();
		REQUIRE(applyKey(keys[(i + 1) % 3]) == Status::Ok);
		REQUIRE(box.panel.calls.empty());
	}
}

TEST_CASE("a changed scroll mode, LED mode, backlight or statusline reaches the panel once", "[apply][vfd]")
{
	Fresh fresh;
	VfdBox box;

	g_settings.lcd_scroll = 4;
	REQUIRE(applyKey("lcd_scroll") == Status::Ok);
	REQUIRE(box.panel.calls.size() == 1);
	REQUIRE(box.panel.scrolls[0] == 4);

	box.panel.forget();
	g_settings.led_tv_mode = 3;
	REQUIRE(applyKey("led_tv_mode") == Status::Ok);
	REQUIRE(box.panel.calls.size() == 1);
	REQUIRE(box.panel.calls[0] == "leds");

#ifndef ENABLE_LCD
	box.panel.forget();
	g_settings.backlight_tv = 0;
	REQUIRE(applyKey("backlight_tv") == Status::Ok);
	REQUIRE(box.panel.calls.size() == 1);
	REQUIRE(box.panel.backlights[0] == 0);
#endif

	box.panel.forget();
	g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] = 2;
	g_settings.current_volume = 40;
	REQUIRE(applyKey("lcd_show_volume") == Status::Ok);
	REQUIRE(box.panel.calls.size() == 1);
	REQUIRE(box.panel.statuslines[0].first == 2);
	REQUIRE(box.panel.statuslines[0].second == 40);

	// Nothing changed since, nothing is sent.
	box.panel.forget();
	REQUIRE(applyKey("lcd_scroll") == Status::Ok);
	REQUIRE(box.panel.calls.empty());
}

TEST_CASE("a changed contrast, power or inverse reaches the panel once and an unrelated key sends nothing", "[apply][vfd]")
{
	Fresh fresh;
	VfdBox box;
	const char *const keys[] = { "lcd_contrast", "lcd_power", "lcd_inverse" };
	const int members[] = { SNeutrinoSettings::LCD_CONTRAST, SNeutrinoSettings::LCD_POWER,
				SNeutrinoSettings::LCD_INVERSE };
	for (size_t i = 0; i < 3; ++i)
	{
		INFO(keys[i]);
		box.panel.forget();
		g_settings.lcd_setting[members[i]] = g_settings.lcd_setting[members[i]] ? 0 : 1;
		REQUIRE(applyKey(keys[i]) == Status::Ok);
		REQUIRE(box.panel.calls.size() == 1);
		REQUIRE(box.panel.calls[0] == "parameters");

		box.panel.forget();
		REQUIRE(applyKey("lcd_scroll") == Status::Ok);
		REQUIRE(box.panel.calls.empty());
	}
}

TEST_CASE("a brightness the driver refused is sent again by the next run", "[apply][vfd]")
{
	Fresh fresh;
	VfdBox box;
	box.panel.brightness_answer = Status::Internal;
	g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = 8;
	REQUIRE(applyKey("lcd_brightness") == Status::Internal);
	box.panel.brightness_answer = Status::Ok;
	box.panel.forget();
	REQUIRE(applyKey("lcd_scroll") == Status::Ok);
	REQUIRE(box.panel.count("brightness") == 1);
}

TEST_CASE("a web batch of two brightnesses runs the front panel group once", "[apply][vfd]")
{
	Fresh fresh;
	VfdBox box;
	WebWrites web;

	REQUIRE(settings::set("lcd_brightness", "7").ok());
	REQUIRE(settings::set("lcd_standbybrightness", "3").ok());
	const size_t posted = web.sink.posted.size();
	REQUIRE(box.panel.calls.empty());
	applyPendingSettings();

	REQUIRE(box.panel.count("brightness") == 2);
	REQUIRE(box.panel.brightness[0].second == 7);
	REQUIRE(box.panel.brightness[1].second == 3);
	REQUIRE(web.postedOnlyTheLandingWord(posted));
}

#ifndef ENABLE_LCD
// The box in standby has the backlight of its own; the group must not put the TV one over it.
TEST_CASE("the backlight is not sent while the box is in standby and is sent once it wakes", "[apply][vfd]")
{
	Fresh fresh;
	VfdBox box;

	holdVfdBacklight(true);
	g_settings.backlight_tv = 0;
	REQUIRE(applyKey("backlight_tv") == Status::Ok);
	REQUIRE(box.panel.count("backlight") == 0);

	// Other keys of the group still reach the panel meanwhile.
	g_settings.lcd_scroll = 5;
	REQUIRE(applyKey("lcd_scroll") == Status::Ok);
	REQUIRE(box.panel.count("scroll") == 1);
	REQUIRE(box.panel.count("backlight") == 0);

	holdVfdBacklight(false);
	REQUIRE(applyKey("backlight_tv") == Status::Ok);
	REQUIRE(box.panel.count("backlight") == 1);
	REQUIRE(box.panel.backlights[0] == 0);
	resetSentVfd();
}
#endif

/* The dim brightness is read when the driver dims. Writing it from the web must not
   reach the normal brightness, which the old notifier overwrote with it. */
TEST_CASE("a web write of the dim brightness leaves the normal brightness and the panel alone", "[apply][vfd]")
{
	Fresh fresh;
	int dim = g_settings.lcd_setting_dim_brightness;
	VfdBox box;
	WebWrites web;
	const int normal = g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS];

	REQUIRE(settings::set("lcd_dim_brightness", "3").ok());
	const size_t posted = web.sink.posted.size();
	applyPendingSettings();

	REQUIRE(g_settings.lcd_setting_dim_brightness == 3);
	REQUIRE(g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] == normal);
	REQUIRE(box.panel.calls.empty());
	REQUIRE(web.postedOnlyTheLandingWord(posted));
	g_settings.lcd_setting_dim_brightness = dim;
}

/* ------------------------------------------------------------------ graphic display */

// The settings of the display exist only in a build with it, so the cases hand the group readings.

TEST_CASE("the display's switch, driver, font, brightnesses and theme are one group", "[apply][glcd]")
{
	Fresh fresh;
	registerApplyGroups();
	const char *const keys[] = { "glcd_enable", "glcd_mirror_osd", "glcd_selected_config", "glcd_font",
				     "glcd_brightness", "glcd_scroll_speed", "glcd_channel_percent",
				     "glcd_standby_weather_y_position", "glcd_theme.glcd_progressbar_color" };
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		INFO(keys[i]);
		REQUIRE(groupOf(keys[i]) == &kGlcdApplyGroup);
	}
	REQUIRE(kGlcdApplyGroup.phase == ApplyPhase::Decoders);
	// Read when the service is made, which a restart does.
	REQUIRE(groupOf("glcd_theme_name") == NULL);
}

namespace
{

struct GlcdBox
{
	PhaseEnvironment env;
	FakeGlcdPanel &panel;

	GlcdBox() : env(ApplyPhase::Decoders), panel(env.fake<FakeGlcdPanel>("glcd"))
	{
		resetSentGlcd();
		GlcdSnapshot start;
		start.enable = 1;
		start.mirror = 0;
		start.config = 0;
		start.font = "a.ttf";
		applyGlcdSnapshot(start);
	}
	~GlcdBox() { resetSentGlcd(); }

	GlcdSnapshot now() const
	{
		GlcdSnapshot s;
		s.enable = 1;
		s.mirror = 0;
		s.config = 0;
		s.font = "a.ttf";
		return s;
	}
};

} // namespace

TEST_CASE("the first run of the display group only learns what the service was made with", "[apply][glcd]")
{
	GlcdBox box;
	REQUIRE(box.panel.calls.empty());
}

TEST_CASE("a run with nothing changed redraws the layout and sets the brightness, nothing else", "[apply][glcd]")
{
	GlcdBox box;
	REQUIRE(applyGlcdSnapshot(box.now()) == Status::Ok);
	REQUIRE(box.panel.calls.size() == 2);
	REQUIRE(box.panel.calls[0] == "brightness");
	REQUIRE(box.panel.calls[1] == "update");
}

TEST_CASE("a changed switch, mirroring, driver or font is sent once and not again by the next run", "[apply][glcd]")
{
	GlcdBox box;

	GlcdSnapshot s = box.now();
	s.enable = 0;
	REQUIRE(applyGlcdSnapshot(s) == Status::Ok);
	REQUIRE(box.panel.count("enable") == 1);
	REQUIRE(box.panel.switched[0] == 0);
	box.panel.forget();
	REQUIRE(applyGlcdSnapshot(s) == Status::Ok);
	REQUIRE(box.panel.count("enable") == 0);

	box.panel.forget();
	s.mirror = 1;
	REQUIRE(applyGlcdSnapshot(s) == Status::Ok);
	REQUIRE(box.panel.count("mirror") == 1);
	REQUIRE(box.panel.switched[0] == 1);

	box.panel.forget();
	s.config = 2;
	REQUIRE(applyGlcdSnapshot(s) == Status::Ok);
	REQUIRE(box.panel.count("respawn") == 1);

	box.panel.forget();
	s.font = "b.ttf";
	REQUIRE(applyGlcdSnapshot(s) == Status::Ok);
	REQUIRE(box.panel.count("font") == 1);
	REQUIRE(box.panel.count("enable") + box.panel.count("mirror") + box.panel.count("respawn") == 0);
}

TEST_CASE("a driver change the service refused is tried again by the next run", "[apply][glcd]")
{
	GlcdBox box;
	box.panel.respawn_answer = Status::Internal;
	GlcdSnapshot s = box.now();
	s.config = 1;
	REQUIRE(applyGlcdSnapshot(s) == Status::Internal);
	box.panel.respawn_answer = Status::Ok;
	box.panel.forget();
	REQUIRE(applyGlcdSnapshot(s) == Status::Ok);
	REQUIRE(box.panel.count("respawn") == 1);
}

TEST_CASE("a key of the display group runs the group once through the registry", "[apply][glcd]")
{
	Fresh fresh;
	PhaseEnvironment env(ApplyPhase::Decoders);
	FakeGlcdPanel &panel = env.fake<FakeGlcdPanel>("glcd");
	resetSentGlcd();
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);
	REQUIRE(panel.calls.empty());
	REQUIRE(applyKey("glcd_logodir") == Status::Ok);
	REQUIRE(panel.count("update") == 1);
	std::vector<std::string> keys;
	keys.push_back("glcd_brightness");
	keys.push_back("glcd_time_percent");
	panel.forget();
	REQUIRE(applyBatch(keys) == Status::Ok);
	REQUIRE(panel.count("update") == 1);
	resetSentGlcd();
}

TEST_CASE("a position is bounded by the connected panel, and by the largest panel while none answers", "[apply][glcd]")
{
	PhaseEnvironment env(ApplyPhase::Decoders);
	FakeGlcdPanel &panel = env.fake<FakeGlcdPanel>("glcd");
	panel.width = 128;
	panel.height = 64;
	REQUIRE(glcdPanelWidth() == 128);
	REQUIRE(glcdPanelHeight() == 64);

	panel.size_answer = Status::NotSupported;
	REQUIRE(glcdPanelWidth() == kGlcdPanelWidthMax);
	REQUIRE(glcdPanelHeight() == kGlcdPanelHeightMax);

	// A panel larger than the drivers are written for, or no size at all, is not believed.
	panel.size_answer = Status::Ok;
	panel.width = 5000;
	panel.height = 0;
	REQUIRE(glcdPanelWidth() == kGlcdPanelWidthMax);
	REQUIRE(glcdPanelHeight() == kGlcdPanelHeightMax);
}

namespace
{

/* What the real seam answers: only what the loop noted, never the service. */
struct RecordedPanel : public FakeGlcdPanel
{
	coreapi::Status panelSize(int &w, int &h) { return recordedGlcdPanelSize(w, h); }
};

long g_seen_width = 0, g_seen_height = 0;

void *askFromAnotherThread(void *)
{
	g_seen_width = glcdPanelWidth();
	g_seen_height = glcdPanelHeight();
	return 0;
}

} // namespace

/* A written position is checked against the panel on the web server's thread, which must not
   reach the display service: it reads what the loop recorded, and the largest panel until then. */
TEST_CASE("the panel size a write is checked against is what the loop recorded, whichever thread asks", "[apply][glcd]")
{
	resetSentGlcd();
	RecordedPanel panel;
	setGlcdPanel(&panel);

	REQUIRE(glcdPanelWidth() == kGlcdPanelWidthMax);
	REQUIRE(glcdPanelHeight() == kGlcdPanelHeightMax);

	noteGlcdPanelSize(128, 64);
	pthread_t other;
	REQUIRE(pthread_create(&other, 0, askFromAnotherThread, 0) == 0);
	REQUIRE(pthread_join(other, 0) == 0);
	REQUIRE(g_seen_width == 128);
	REQUIRE(g_seen_height == 64);
	// The service was not asked anything on that thread.
	REQUIRE(panel.sizes == 0);

	resetSentGlcd();
	REQUIRE(glcdPanelWidth() == kGlcdPanelWidthMax);
	setGlcdPanel(0);
}

TEST_CASE("every run of the display group has the loop record the panel's size, the first too", "[apply][glcd]")
{
	GlcdBox box;
	REQUIRE(box.panel.sizes == 1);
	REQUIRE(box.panel.calls.empty());
	REQUIRE(applyGlcdSnapshot(box.now()) == Status::Ok);
	REQUIRE(box.panel.sizes == 2);
}

#ifdef ENABLE_GRAPHLCD
// Every row the theme bounds by the panel asks for it.
TEST_CASE("the position rows of the display theme state the connected panel as their bound", "[apply][glcd]")
{
	size_t wide = 0, high = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.max_now == glcdPanelWidth)
			++wide;
		if (d.max_now == glcdPanelHeight)
			++high;
		if (d.max_now != NULL)
		{
			INFO(d.key);
			CHECK((d.max == kGlcdPanelWidthMax || d.max == kGlcdPanelHeightMax));
		}
	}
	CHECK(wide == 25);
	CHECK(high == 11);
}
#endif
