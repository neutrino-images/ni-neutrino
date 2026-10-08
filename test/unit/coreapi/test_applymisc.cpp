/*
 * test_applymisc.cpp - the miscellaneous, channel list and section daemon groups
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
#include "coreapi/base/deps.h"
#include "coreapi/box/apply_channels.h"
#include "coreapi/box/apply_misc.h"
#include "coreapi/box/apply_sectionsd.h"
#include "coreapi/network.h"
#include "coreapi/streaming.h"
#include "coreapi/settings/settings.h"

#include <system/settings.h>

#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <vector>

extern SNeutrinoSettings g_settings;

using namespace coreapi;

namespace
{

// Every case starts from nothing: the registry is process wide.
struct Fresh
{
	Fresh() { resetApplyRegistry(); }
	~Fresh() { resetApplyRegistry(); }
};

// The members the cases write, put back so the rest of the suite finds them as they were.
struct KeptMiscSettings
{
	int hd, webtv, webradio, empty_favorites, sdt, port, txt, scan, scan_mode, save_mode;
	int cache, extended, max_events, old_events, save, read, save_freq, read_freq;
	int tmdb, omdb, shoutcast, youtube;
	std::string omdb_key, shoutcast_key, youtube_key;
	int shutdown_real, shutdown_count;
	std::string key, dir, ntp_server, ntp_refresh;

	KeptMiscSettings()
		: hd(g_settings.make_hd_list), webtv(g_settings.make_webtv_list), webradio(g_settings.make_webradio_list),
		  empty_favorites(g_settings.show_empty_favorites), sdt(g_settings.enable_sdt),
		  port(g_settings.streaming_port), txt(g_settings.cacheTXT), scan(g_settings.epg_scan),
		  scan_mode(g_settings.epg_scan_mode), save_mode(g_settings.epg_save_mode),
		  cache(g_settings.epg_cache), extended(g_settings.epg_extendedcache),
		  max_events(g_settings.epg_max_events), old_events(g_settings.epg_old_events),
		  save(g_settings.epg_save), read(g_settings.epg_read), save_freq(g_settings.epg_save_frequently),
		  read_freq(g_settings.epg_read_frequently), tmdb(g_settings.tmdb_enabled), omdb(g_settings.omdb_enabled), shoutcast(g_settings.shoutcast_enabled),
		  youtube(g_settings.youtube_enabled), omdb_key(g_settings.omdb_api_key), shoutcast_key(g_settings.shoutcast_dev_id),
		  youtube_key(g_settings.youtube_api_key),
		  shutdown_real(g_settings.shutdown_real), shutdown_count(g_settings.shutdown_count),
		  key(g_settings.tmdb_api_key), dir(g_settings.epg_dir),
		  ntp_server(g_settings.network_ntpserver), ntp_refresh(g_settings.network_ntprefresh) {}
	~KeptMiscSettings()
	{
		g_settings.make_hd_list = hd;
		g_settings.make_webtv_list = webtv;
		g_settings.make_webradio_list = webradio;
		g_settings.show_empty_favorites = empty_favorites;
		g_settings.enable_sdt = sdt;
		g_settings.streaming_port = port;
		g_settings.cacheTXT = txt;
		g_settings.epg_scan = scan;
		g_settings.epg_scan_mode = scan_mode;
		g_settings.epg_save_mode = save_mode;
		g_settings.epg_cache = cache;
		g_settings.epg_extendedcache = extended;
		g_settings.epg_max_events = max_events;
		g_settings.epg_old_events = old_events;
		g_settings.epg_save = save;
		g_settings.epg_read = read;
		g_settings.epg_save_frequently = save_freq;
		g_settings.epg_read_frequently = read_freq;
		g_settings.tmdb_enabled = tmdb;
		g_settings.omdb_enabled = omdb;
		g_settings.shoutcast_enabled = shoutcast;
		g_settings.youtube_enabled = youtube;
		g_settings.omdb_api_key = omdb_key;
		g_settings.shoutcast_dev_id = shoutcast_key;
		g_settings.youtube_api_key = youtube_key;
		g_settings.shutdown_real = shutdown_real;
		g_settings.shutdown_count = shutdown_count;
		g_settings.tmdb_api_key = key;
		g_settings.epg_dir = dir;
		g_settings.network_ntpserver = ntp_server;
		g_settings.network_ntprefresh = ntp_refresh;
	}
};

/* What a case runs against: every seam the network phase has, the groups'
   fakes among them, nothing sent yet, and the members it writes put back
   afterwards. */
struct MiscBox
{
	KeptMiscSettings kept;
	PhaseEnvironment env;
	FakeMiscOutput  &misc;
	FakeSectionsdOutput &sectionsd;
	FakeCommandSink &commands;

	MiscBox()
		: env(ApplyPhase::Network), misc(env.fake<FakeMiscOutput>("misc")),
		  sectionsd(env.fake<FakeSectionsdOutput>("sectionsd")),
		  commands(env.fake<FakeCommandSink>("command"))
	{
		resetSentMisc();
		resetChannelReload();
	}
	~MiscBox()
	{
		resetSentMisc();
		resetChannelReload();
	}

	// How many times the lists were asked to be built again.
	size_t rebuilds() const
	{
		size_t n = 0;
		for (size_t i = 0; i < commands.posted.size(); ++i)
			n += (commands.posted[i].first == NeutrinoMessages::EVT_SERVICESCHANGED) ? 1 : 0;
		return n;
	}

	void forget()
	{
		misc.forget();
		sectionsd.sent = 0;
		commands.posted.clear();
	}

	// Startup as the program runs it: the phases in order.
	void startup()
	{
		REQUIRE(runPhase(ApplyPhase::Sectionsd) == Status::Ok);
		REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
		forget();
	}
};

bool keepSettings() { return true; }

// The real settings over g_settings, taken off again whatever a check does.
struct RealSettings
{
	RealSettings() { installRealSettingsSource(&g_settings, keepSettings); }
	~RealSettings() { installRealSettingsSource(NULL, NULL); }
};

/* A write from outside with nothing running behind it: the settings over g_settings and
   the sinks a write posts to, the members it touches put back afterwards. */
struct WriteBox
{
	KeptMiscSettings kept;
	FakeCommandSink sink;
	InstalledSink sunk;
	FakeEventSink events;
	InstalledEventSink evented;
	ClearedSettingsSource cleared;
	RealSettings real;

	WriteBox() : sunk(&sink), evented(&events) {}
	// What a write queued is landed here, or the next case that lands writes would find it.
	~WriteBox() { applyPendingSettings(); }
};

std::string number(long v)
{
	char text[24];
	snprintf(text, sizeof(text), "%ld", v);
	return text;
}

} // namespace

TEST_CASE("the interfaces are the listed ones without the loopback and hidden entries, in order", "[apply][misc]")
{
	char dir[] = "/tmp/ifacesXXXXXX";
	REQUIRE(mkdtemp(dir) != NULL);
	const char *const made[] = { "wlan0", "lo", ".hidden", "eth0", "lo0" };
	std::vector<std::string> paths;
	for (size_t i = 0; i < sizeof(made) / sizeof(made[0]); ++i)
	{
		paths.push_back(std::string(dir) + "/" + made[i]);
		REQUIRE(mkdir(paths.back().c_str(), 0755) == 0);
	}

	const std::vector<std::string> names = network::interfacesIn(dir);
	for (size_t i = 0; i < paths.size(); ++i)
		rmdir(paths[i].c_str());
	rmdir(dir);

	REQUIRE(names.size() == 2);
	CHECK(names[0] == "eth0");
	CHECK(names[1] == "wlan0");
	CHECK(network::interfacesIn("/nonexistent/ifaces").empty());
}

/* The list the interface row offers, whatever this box has: a text row stores the
   name, so each entry must carry it as text or every write would be refused. */
TEST_CASE("an interface is offered by its name as text", "[misc][choices]")
{
	std::vector<SettingChoice> got;
	if (!network::interfaceChoices(got))
		return;
	for (size_t i = 0; i < got.size(); ++i)
	{
		CHECK_FALSE(got[i].text.empty());
		CHECK(got[i].text == got[i].label);
	}
}

TEST_CASE("the misc groups are registered by the one hook and each key has one", "[apply][misc]")
{
	Fresh fresh;
	registerApplyGroups();

	CHECK(groupOf("enable_sdt") == &kScanSdtApplyGroup);
	CHECK(groupOf("streaming_port") == &kStreamPortApplyGroup);
	CHECK(groupOf("cacheTXT") == &kTuxtxtApplyGroup);
	const char *const scan[] = { "epg_scan", "epg_scan_mode", "epg_save_mode" };
	for (size_t i = 0; i < sizeof(scan) / sizeof(scan[0]); ++i)
	{
		INFO(scan[i]);
		CHECK(groupOf(scan[i]) == &kEpgScanApplyGroup);
	}
	const char *const lists[] = { "make_hd_list", "make_webtv_list", "make_webradio_list", "show_empty_favorites" };
	for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); ++i)
	{
		INFO(lists[i]);
		CHECK(groupOf(lists[i]) == &kChannelReloadApplyGroup);
	}
	const char *const daemon[] = { "epg_cache_time", "epg_extendedcache_time", "epg_max_events", "epg_old_events",
				       "epg_save", "epg_read", "epg_save_frequently", "epg_read_frequently", "epg_dir",
				       "network_ntpenable", "network_ntpserver", "network_ntprefresh" };
	for (size_t i = 0; i < sizeof(daemon) / sizeof(daemon[0]); ++i)
	{
		INFO(daemon[i]);
		CHECK(groupOf(daemon[i]) == &kSectionsdConfigApplyGroup);
	}
	CHECK(kChannelReloadApplyGroup.phase == ApplyPhase::Sectionsd);
	CHECK(kSectionsdConfigApplyGroup.phase == ApplyPhase::Network);
	CHECK(kEpgScanApplyGroup.phase == ApplyPhase::Network);

	// Read where it is used, and nothing reads it at all.
	CHECK(groupOf("youtube_enabled") == NULL);
	// Read when the lists are built again, which is not caused by a change of it.
	CHECK(groupOf("make_new_list") == NULL);
}

TEST_CASE("startup runs each misc group once and starts nothing the lists will start", "[apply][misc]")
{
	Fresh fresh;
	MiscBox box;
	registerApplyGroups();
	g_settings.enable_sdt = 2;
	g_settings.streaming_port = 31339;
	g_settings.cacheTXT = 1;

	REQUIRE(runPhase(ApplyPhase::Sectionsd) == Status::Ok);
	// The lists are built from the settings later in startup.
	CHECK(box.rebuilds() == 0);
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);

	CHECK(box.misc.sdt == std::vector<int>(1, 2));
	CHECK(box.misc.ports == std::vector<int>(1, 31339));
	CHECK(box.misc.count("txt-on") == 1);
	CHECK(box.misc.count("txt-off") == 0);
	// The guide scan and its filter are set up with the lists.
	CHECK(box.misc.count("filter") == 0);
	CHECK(box.misc.count("scan-start") == 0);
	CHECK(box.misc.count("scan-clear") == 0);
	CHECK(box.sectionsd.sent == 1);
	CHECK(box.rebuilds() == 0);
}

TEST_CASE("startup with the teletext cache off sets nothing up and tears nothing down", "[apply][misc]")
{
	Fresh fresh;
	MiscBox box;
	registerApplyGroups();
	g_settings.cacheTXT = 0;
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	CHECK(box.misc.count("txt-on") == 0);
	CHECK(box.misc.count("txt-off") == 0);
}

TEST_CASE("a GUI change of a scan, port or teletext key calls its group once", "[apply][misc]")
{
	Fresh fresh;
	MiscBox box;
	registerApplyGroups();
	g_settings.cacheTXT = 0;
	box.startup();

	g_settings.enable_sdt = 0;
	REQUIRE(applyKey("enable_sdt") == Status::Ok);
	CHECK(box.misc.calls == std::vector<std::string>(1, "sdt"));
	CHECK(box.misc.sdt == std::vector<int>(1, 0));

	box.forget();
	g_settings.streaming_port = 4711;
	REQUIRE(applyKey("streaming_port") == Status::Ok);
	CHECK(box.misc.calls == std::vector<std::string>(1, "port"));
	CHECK(box.misc.ports == std::vector<int>(1, 4711));

	box.forget();
	g_settings.cacheTXT = 1;
	REQUIRE(applyKey("cacheTXT") == Status::Ok);
	CHECK(box.misc.calls == std::vector<std::string>(1, "txt-on"));
	box.forget();
	g_settings.cacheTXT = 0;
	REQUIRE(applyKey("cacheTXT") == Status::Ok);
	CHECK(box.misc.calls == std::vector<std::string>(1, "txt-off"));
}

TEST_CASE("the teletext cache is set up and torn down once per change", "[apply][misc]")
{
	Fresh fresh;
	MiscBox box;
	registerApplyGroups();
	g_settings.cacheTXT = 1;
	box.startup();

	// The same value again: a hotkey or another writer of it.
	REQUIRE(applyKey("cacheTXT") == Status::Ok);
	CHECK(box.misc.calls.empty());

	// A refused send is not taken as made.
	g_settings.cacheTXT = 0;
	box.misc.answer = Status::Internal;
	CHECK(applyKey("cacheTXT") == Status::Internal);
	box.misc.answer = Status::Ok;
	box.forget();
	REQUIRE(applyKey("cacheTXT") == Status::Ok);
	CHECK(box.misc.calls == std::vector<std::string>(1, "txt-off"));
}

TEST_CASE("the guide scan follows its mode and bouquets and the filter follows the save mode", "[apply][misc]")
{
	Fresh fresh;
	MiscBox box;
	registerApplyGroups();
	g_settings.epg_scan_mode = EPG_SCAN_MODE_OFF;
	g_settings.epg_scan = EPG_SCAN_FAV;
	g_settings.epg_save_mode = 0;
	box.startup();

	// A mode that is on starts a pass.
	g_settings.epg_scan_mode = EPG_SCAN_MODE_STANDBY;
	REQUIRE(applyKey("epg_scan_mode") == Status::Ok);
	CHECK(box.misc.calls == std::vector<std::string>(1, "scan-start"));

	// Other bouquets start it again.
	box.forget();
	g_settings.epg_scan = EPG_SCAN_SEL;
	REQUIRE(applyKey("epg_scan") == Status::Ok);
	CHECK(box.misc.calls == std::vector<std::string>(1, "scan-start"));

	// Nothing changed, nothing started.
	box.forget();
	REQUIRE(applyKey("epg_scan") == Status::Ok);
	CHECK(box.misc.calls.empty());

	// The filter is the save mode's and the pass is left alone.
	g_settings.epg_save_mode = 1;
	REQUIRE(applyKey("epg_save_mode") == Status::Ok);
	CHECK(box.misc.calls == std::vector<std::string>(1, "filter"));

	box.forget();
	g_settings.epg_scan_mode = EPG_SCAN_MODE_OFF;
	REQUIRE(applyKey("epg_scan_mode") == Status::Ok);
	CHECK(box.misc.calls == std::vector<std::string>(1, "scan-clear"));
}

TEST_CASE("a start of the guide scan that was refused is made again by the next run", "[apply][misc]")
{
	Fresh fresh;
	MiscBox box;
	registerApplyGroups();
	g_settings.epg_scan_mode = EPG_SCAN_MODE_OFF;
	box.startup();

	g_settings.epg_scan_mode = EPG_SCAN_MODE_STANDBY;
	box.misc.answer = Status::Internal;
	CHECK(applyKey("epg_scan_mode") == Status::Internal);
	box.misc.answer = Status::Ok;
	box.forget();
	REQUIRE(applyKey("epg_scan_mode") == Status::Ok);
	CHECK(box.misc.calls == std::vector<std::string>(1, "scan-start"));
}

TEST_CASE("a GUI change of a channel list key asks for the lists once and only when they differ", "[apply][misc]")
{
	Fresh fresh;
	MiscBox box;
	registerApplyGroups();
	g_settings.make_hd_list = 0;
	g_settings.show_empty_favorites = 0;
	box.startup();

	g_settings.make_hd_list = 1;
	REQUIRE(applyKey("make_hd_list") == Status::Ok);
	CHECK(box.rebuilds() == 1);

	// The same shape again, from a sibling key or a second writer.
	REQUIRE(applyKey("make_webtv_list") == Status::Ok);
	CHECK(box.rebuilds() == 1);

	g_settings.show_empty_favorites = 1;
	REQUIRE(applyKey("show_empty_favorites") == Status::Ok);
	CHECK(box.rebuilds() == 2);
}

TEST_CASE("a rebuild of the lists that was refused is asked for again by the next run", "[apply][misc]")
{
	Fresh fresh;
	MiscBox box;
	registerApplyGroups();
	box.startup();

	g_settings.make_webradio_list = !g_settings.make_webradio_list;
	box.commands.answer = Status::Busy;
	CHECK(applyKey("make_webradio_list") == Status::Busy);
	box.commands.answer = Status::Ok;
	box.forget();
	REQUIRE(applyKey("make_webradio_list") == Status::Ok);
	CHECK(box.rebuilds() == 1);
}

TEST_CASE("a GUI change of a guide cache or daemon key sends the daemon its configuration once", "[apply][misc]")
{
	Fresh fresh;
	MiscBox box;
	registerApplyGroups();
	box.startup();

	g_settings.epg_cache = 3;
	REQUIRE(applyKey("epg_cache_time") == Status::Ok);
	CHECK(box.sectionsd.sent == 1);

	g_settings.epg_dir = "/tmp";
	REQUIRE(applyKey("epg_dir") == Status::Ok);
	CHECK(box.sectionsd.sent == 2);

	// What the daemon is configured by is read where the message is made, so the
	// switch that gates a frequency is a key of the group too.
	g_settings.epg_save = 1;
	REQUIRE(applyKey("epg_save") == Status::Ok);
	CHECK(box.sectionsd.sent == 3);
}

TEST_CASE("a failed send to the daemon is what the group answers", "[apply][misc]")
{
	Fresh fresh;
	MiscBox box;
	registerApplyGroups();
	box.startup();
	box.sectionsd.answer = Status::Internal;
	CHECK(applyKey("epg_max_events") == Status::Internal);
}

/* The web path: nobody stands at the television, so the run asks nothing, and a
   batch of several keys of one group runs it once. */
TEST_CASE("a web batch of several keys of one group runs the group once", "[apply][misc]")
{
	Fresh fresh;
	MiscBox box;
	RealSettings real;
	ClearedSettingsSource cleared;
	registerApplyGroups();
	g_settings.epg_read = 1;
	g_settings.epg_cache = 7;
	g_settings.epg_max_events = 30000;
	g_settings.epg_read_frequently = 1;
	box.startup();

	REQUIRE(settings::set("epg_cache_time", number(3)).ok());
	REQUIRE(settings::set("epg_max_events", number(1000)).ok());
	REQUIRE(settings::set("epg_read_frequently", number(0)).ok());
	CHECK(box.sectionsd.sent == 0);
	applyPendingSettings();

	CHECK(box.sectionsd.sent == 1);
	CHECK(g_settings.epg_cache == 3);
	CHECK(g_settings.epg_max_events == 1000);
}

TEST_CASE("a time server change from the screen or the web sends the daemon its configuration once", "[apply][misc]")
{
	Fresh fresh;
	MiscBox box;
	RealSettings real;
	ClearedSettingsSource cleared;
	registerApplyGroups();
	box.startup();

	g_settings.network_ntpserver = "ntp.example.org";
	REQUIRE(applyKey("network_ntpserver") == Status::Ok);
	CHECK(box.sectionsd.sent == 1);

	box.forget();
	REQUIRE(settings::set("network_ntpserver", "pool.ntp.org").ok());
	REQUIRE(settings::set("network_ntprefresh", "45").ok());
	applyPendingSettings();
	CHECK(box.sectionsd.sent == 1);
	CHECK(g_settings.network_ntprefresh == "45");
}

TEST_CASE("a web batch writing channel list keys asks for the lists once", "[apply][misc]")
{
	Fresh fresh;
	MiscBox box;
	RealSettings real;
	ClearedSettingsSource cleared;
	registerApplyGroups();
	g_settings.make_hd_list = 0;
	g_settings.make_webtv_list = 1;
	g_settings.show_empty_favorites = 0;
	box.startup();

	REQUIRE(settings::set("make_hd_list", "1").ok());
	REQUIRE(settings::set("make_webtv_list", "0").ok());
	REQUIRE(settings::set("show_empty_favorites", "1").ok());
	CHECK(box.rebuilds() == 0);
	applyPendingSettings();

	CHECK(box.rebuilds() == 1);
}

TEST_CASE("a web write of the scan, port and teletext keys reaches the drivers once each", "[apply][misc]")
{
	Fresh fresh;
	MiscBox box;
	RealSettings real;
	ClearedSettingsSource cleared;
	registerApplyGroups();
	g_settings.cacheTXT = 0;
	g_settings.enable_sdt = 1;
	g_settings.epg_scan_mode = EPG_SCAN_MODE_OFF;
	g_settings.epg_save_mode = 0;
	box.startup();

	REQUIRE(settings::set("enable_sdt", "0").ok());
	REQUIRE(settings::set("streaming_port", "4711").ok());
	REQUIRE(settings::set("cacheTXT", "1").ok());
	REQUIRE(settings::set("epg_scan_mode", number(EPG_SCAN_MODE_STANDBY)).ok());
	REQUIRE(settings::set("epg_save_mode", "1").ok());
	CHECK(box.misc.calls.empty());
	applyPendingSettings();

	CHECK(box.misc.count("sdt") == 1);
	CHECK(box.misc.count("port") == 1);
	CHECK(box.misc.count("txt-on") == 1);
	CHECK(box.misc.count("scan-start") == 1);
	CHECK(box.misc.count("filter") == 1);
	CHECK(box.misc.calls.size() == 5);
}

TEST_CASE("the port is a port: nought and anything past sixteen bits are refused", "[apply][misc]")
{
	WriteBox box;
	CHECK_FALSE(settings::set("streaming_port", "0").ok());
	CHECK_FALSE(settings::set("streaming_port", "65536").ok());
	CHECK(settings::set("streaming_port", "1").ok());
	CHECK(settings::set("streaming_port", "65535").ok());
}

TEST_CASE("a service switch is changeable only while its key holds something else than the placeholder", "[apply][misc]")
{
	WriteBox box;

	g_settings.tmdb_api_key = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
	CHECK_FALSE(settings::set("tmdb_enabled", "1").ok());

	g_settings.tmdb_api_key = "";
	CHECK_FALSE(settings::set("tmdb_enabled", "1").ok());

	g_settings.tmdb_api_key = "0123456789abcdef0123456789abcdef";
	CHECK(settings::set("tmdb_enabled", "1").ok());
}

TEST_CASE("the energy and scan rows are changeable only while what they depend on allows it", "[apply][misc]")
{
	WriteBox box;

	// Switching off for real is the option worded on, stored as nought.
	g_settings.shutdown_real = 1;
	CHECK_FALSE(settings::set("shutdown_count", "5").ok());
	g_settings.shutdown_real = 0;
	CHECK(settings::set("shutdown_count", "5").ok());

	g_settings.epg_scan_mode = EPG_SCAN_MODE_OFF;
	CHECK_FALSE(settings::set("epg_scan", number(EPG_SCAN_CURRENT)).ok());
	g_settings.epg_scan_mode = EPG_SCAN_MODE_STANDBY;
	CHECK(settings::set("epg_scan", number(EPG_SCAN_CURRENT)).ok());
}

TEST_CASE("every service switch is changeable only while its own key is entered", "[apply][misc]")
{
	WriteBox box;
	struct Service { const char *flag; const char *key; std::string *text; size_t length; };
	const Service services[] =
	{
		{ "tmdb_enabled", "tmdb_api_key", &g_settings.tmdb_api_key, 32 },
		{ "omdb_enabled", "omdb_api_key", &g_settings.omdb_api_key, 8 },
		{ "shoutcast_enabled", "shoutcast_dev_id", &g_settings.shoutcast_dev_id, 16 },
		{ "youtube_enabled", "youtube_api_key", &g_settings.youtube_api_key, 39 }
	};
	for (size_t i = 0; i < sizeof(services) / sizeof(services[0]); ++i)
	{
		INFO(services[i].flag);
		const std::string kept = *services[i].text;
		*services[i].text = std::string(services[i].length, 'X');
		CHECK_FALSE(settings::set(services[i].flag, "1").ok());
		*services[i].text = "";
		CHECK_FALSE(settings::set(services[i].flag, "1").ok());
		*services[i].text = std::string(services[i].length, 'a');
		CHECK(settings::set(services[i].flag, "1").ok());
		applyPendingSettings();
		*services[i].text = kept;
	}
}

TEST_CASE("the units and named values of the number rows are the rows' own", "[apply][misc]")
{
	const char *const minutes[] = { "shutdown_count" };
	const char *const hours[] = { "epg_extendedcache_time", "epg_old_events" };
	for (size_t i = 0; i < 1; ++i)
	{
		Result<Descriptor> d = settings::describe(minutes[i]);
		REQUIRE(d.ok());
		CHECK(std::string(d.value().unit_key) == "unit.short.minute");
		REQUIRE(d.value().value_count == 1);
		CHECK(d.value().values[0].value == 0);
	}
	for (size_t i = 0; i < 2; ++i)
	{
		Result<Descriptor> d = settings::describe(hours[i]);
		REQUIRE(d.ok());
		CHECK(std::string(d.value().unit_key) == "unit.short.hour");
	}
	Result<Descriptor> max = settings::describe("epg_max_events");
	REQUIRE(max.ok());
	REQUIRE(max.value().value_count == 1);
	CHECK(max.value().values[0].value == 0);
}

TEST_CASE("a stored port outside 1 to 65535 falls back to the default", "[apply][misc]")
{
	CHECK(streaming::portOrDefault(0) == 31339);
	CHECK(streaming::portOrDefault(-5) == 31339);
	CHECK(streaming::portOrDefault(65536) == 31339);
	CHECK(streaming::portOrDefault(99999) == 31339);
	CHECK(streaming::portOrDefault(1) == 1);
	CHECK(streaming::portOrDefault(65535) == 65535);
	CHECK(streaming::portOrDefault(4711) == 4711);
}

/* A reload of the settings file changes members past the groups; applyReplaced,
   which the reload goes through, has to catch each of them up once. */
TEST_CASE("a settings reload catches every misc group up once", "[apply][misc]")
{
	Fresh fresh;
	MiscBox box;
	RealSettings real;
	ClearedSettingsSource cleared;
	registerApplyGroups();
	g_settings.cacheTXT = 0;
	g_settings.epg_scan_mode = EPG_SCAN_MODE_OFF;
	box.startup();

	REQUIRE(settings::applyReplaced([]() {
		g_settings.make_hd_list = !g_settings.make_hd_list;
		g_settings.epg_cache = g_settings.epg_cache + 1;
		g_settings.enable_sdt = g_settings.enable_sdt ? 0 : 1;
		g_settings.streaming_port = 4711;
		g_settings.cacheTXT = 1;
		g_settings.epg_scan_mode = EPG_SCAN_MODE_STANDBY;
	}) == Status::Ok);

	CHECK(box.rebuilds() == 1);
	CHECK(box.sectionsd.sent == 1);
	CHECK(box.misc.count("sdt") == 1);
	CHECK(box.misc.count("port") == 1);
	CHECK(box.misc.count("txt-on") == 1);
	CHECK(box.misc.count("scan-start") == 1);
}

/* The standby values are the box's own until it wakes. A write meanwhile is kept and
   sent once, at the wake, and the standby value is not overridden before. */
TEST_CASE("the processor clock and the fan keep the standby value and are sent at the wake", "[apply][misc]")
{
	Fresh fresh;
	MiscBox box;
	registerApplyGroups();
	const int cpu = g_settings.cpufreq;
	const int fan = g_settings.fan_speed;
	g_settings.cpufreq = 0;
	g_settings.fan_speed = 3;
	box.startup();

	// A repeat of what is in force sends nothing.
	REQUIRE(applyKey("cpufreq") == Status::Ok);
	REQUIRE(applyKey("fan_speed") == Status::Ok);
	CHECK(box.misc.calls.empty());

	holdCpuFreq(true);
	holdFanSpeed(true);
	g_settings.cpufreq = 400;
	g_settings.fan_speed = 7;
	REQUIRE(applyKey("cpufreq") == Status::Ok);
	REQUIRE(applyKey("fan_speed") == Status::Ok);
	CHECK(box.misc.calls.empty());

	holdCpuFreq(false);
	holdFanSpeed(false);
	REQUIRE(applyKey("cpufreq") == Status::Ok);
	REQUIRE(applyKey("fan_speed") == Status::Ok);
	REQUIRE(box.misc.cpu.size() == 1);
	CHECK(box.misc.cpu[0] == 400);
	REQUIRE(box.misc.fan.size() == 1);
	CHECK(box.misc.fan[0] == 7);

	// The standby value was on the box, so the same setting is sent again at the wake.
	box.misc.forget();
	holdCpuFreq(true);
	holdCpuFreq(false);
	REQUIRE(applyKey("cpufreq") == Status::Ok);
	CHECK(box.misc.cpu.size() == 1);

	g_settings.cpufreq = cpu;
	g_settings.fan_speed = fan;
}
