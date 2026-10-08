/*
 * test_apply_record.cpp - the recording apply group
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
#include "coreapi/box/apply_record.h"
#include "coreapi/settings/settings.h"

#include <system/settings.h>

#include <string>

extern SNeutrinoSettings g_settings;

using namespace coreapi;

namespace
{

// The members the cases write, put back so the rest of the suite finds them as they were.
struct KeptRecordSettings
{
	std::string recording_dir, timeshift_dir;
	int stop, vtxt, pmt, subtitle;
	KeptRecordSettings()
		: recording_dir(g_settings.network_nfs_recordingdir), timeshift_dir(g_settings.timeshiftdir),
		  stop(g_settings.recording_stopsectionsd), vtxt(g_settings.recording_stream_vtxt_pid),
		  pmt(g_settings.recording_stream_pmt_pid), subtitle(g_settings.recording_stream_subtitle_pids) {}
	~KeptRecordSettings()
	{
		g_settings.network_nfs_recordingdir = recording_dir;
		g_settings.timeshiftdir = timeshift_dir;
		g_settings.recording_stopsectionsd = stop;
		g_settings.recording_stream_vtxt_pid = vtxt;
		g_settings.recording_stream_pmt_pid = pmt;
		g_settings.recording_stream_subtitle_pids = subtitle;
	}
};

struct RecordBox
{
	KeptRecordSettings kept;
	PhaseEnvironment env;
	FakeRecordConfig &out;

	RecordBox() : env(ApplyPhase::Network), out(env.fake<FakeRecordConfig>("recordconfig"))
	{
		resetApplyRegistry();
		resetSentRecord();
		registerApplyGroups();
	}
	~RecordBox()
	{
		resetSentRecord();
		resetApplyRegistry();
	}

	// The phase reached, as after startup, and nothing sent since.
	void started()
	{
		REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
		out.forget();
	}
};

bool recordSave() { return true; }

} // namespace

TEST_CASE("the recording group holds the directories and the data pid flags and runs after the mounts", "[apply][record]")
{
	RecordBox box;
	const char *const keys[] = { "network_nfs_recordingdir", "timeshiftdir", "recording_stopsectionsd",
				     "recordingmenu.stream_vtxt_pid", "recordingmenu.stream_pmt_pid",
				     "recordingmenu.stream_subtitle_pids" };
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		INFO(keys[i]);
		REQUIRE(groupOf(keys[i]) == &kRecordConfigApplyGroup);
	}
	REQUIRE(kRecordConfigApplyGroup.phase == ApplyPhase::Network);
}

TEST_CASE("startup tells the recorder everything once", "[apply][record]")
{
	RecordBox box;
	g_settings.network_nfs_recordingdir = "/media/sda1/movies";
	g_settings.timeshiftdir = "/media/sda1/ts";
	g_settings.recording_stopsectionsd = 1;
	g_settings.recording_stream_vtxt_pid = 0;
	g_settings.recording_stream_pmt_pid = 1;
	g_settings.recording_stream_subtitle_pids = 0;
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);

	REQUIRE(box.out.count("directory") == 1);
	REQUIRE(box.out.last("directory") == "/media/sda1/movies");
	REQUIRE(box.out.count("timeshift") == 1);
	REQUIRE(box.out.last("timeshift") == "/media/sda1/ts");
	REQUIRE(box.out.count("config") == 1);
	REQUIRE(box.out.last("config") == "1010");
	REQUIRE(box.out.count("usage") == 1);
	REQUIRE(box.out.last("usage") == "/media/sda1/movies");
	// A folder the user named is theirs to make.
	REQUIRE(box.out.count("make") == 0);
}

TEST_CASE("an empty timeshift folder or the recording folder itself puts timeshift in a folder below it", "[apply][record]")
{
	RecordBox box;
	box.started();
	g_settings.network_nfs_recordingdir = "/media/sda1/movies";

	g_settings.timeshiftdir = "";
	REQUIRE(applyKey("timeshiftdir") == Status::Ok);
	REQUIRE(box.out.last("timeshift") == "/media/sda1/movies/.timeshift");
	REQUIRE(box.out.last("make") == "/media/sda1/movies/.timeshift");

	box.out.forget();
	g_settings.timeshiftdir = "/media/sda1/other";
	REQUIRE(applyKey("timeshiftdir") == Status::Ok);
	REQUIRE(box.out.last("timeshift") == "/media/sda1/other");
	REQUIRE(box.out.count("make") == 0);

	box.out.forget();
	g_settings.timeshiftdir = "/media/sda1/movies";
	REQUIRE(applyKey("timeshiftdir") == Status::Ok);
	REQUIRE(box.out.last("timeshift") == "/media/sda1/movies/.timeshift");
	REQUIRE(box.out.count("make") == 1);

	REQUIRE(timeshiftDirectoryFor("/a", "") == "/a/.timeshift");
	REQUIRE(timeshiftDirectoryFor("/a", "/a") == "/a/.timeshift");
	REQUIRE(timeshiftDirectoryFor("/a", "/b") == "/b");
}

// The default timeshift folder follows the recording folder, so a change of the latter moves both.
TEST_CASE("a new recording folder reaches the recorder, the usage watcher and an empty timeshift folder", "[apply][record]")
{
	RecordBox box;
	box.started();
	g_settings.network_nfs_recordingdir = "/media/sdb1/movies";
	g_settings.timeshiftdir = "";
	REQUIRE(applyKey("network_nfs_recordingdir") == Status::Ok);
	REQUIRE(box.out.last("directory") == "/media/sdb1/movies");
	REQUIRE(box.out.last("usage") == "/media/sdb1/movies");
	REQUIRE(box.out.last("timeshift") == "/media/sdb1/movies/.timeshift");
	// The flags did not change, so the recorder is not given them again.
	REQUIRE(box.out.count("config") == 0);
}

TEST_CASE("a data pid flag reaches the recorder with the other three", "[apply][record]")
{
	RecordBox box;
	box.started();
	g_settings.recording_stopsectionsd = 0;
	g_settings.recording_stream_vtxt_pid = 1;
	g_settings.recording_stream_pmt_pid = 0;
	g_settings.recording_stream_subtitle_pids = 1;
	REQUIRE(applyKey("recordingmenu.stream_subtitle_pids") == Status::Ok);
	REQUIRE(box.out.count("config") == 1);
	REQUIRE(box.out.last("config") == "0101");
}

TEST_CASE("a web batch writing data pid flags runs the recording group once", "[apply][record]")
{
	RecordBox box;
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, recordSave);

	g_settings.recording_stream_vtxt_pid = 0;
	g_settings.recording_stream_subtitle_pids = 0;
	g_settings.recording_stream_pmt_pid = 0;
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	box.out.forget();

	REQUIRE(settings::set("recordingmenu.stream_vtxt_pid", "1").ok());
	REQUIRE(settings::set("recordingmenu.stream_subtitle_pids", "1").ok());
	REQUIRE(settings::set("recordingmenu.stream_pmt_pid", "1").ok());
	REQUIRE(box.out.calls.empty());
	const size_t posted = sink.posted.size();
	applyPendingSettings();

	REQUIRE(box.out.count("config") == 1);
	REQUIRE(box.out.last("config") == "0111");
	REQUIRE(events.sent.empty());
	for (size_t i = posted; i < sink.posted.size(); ++i)
		CHECK(sink.posted[i].first == NeutrinoMessages::EVT_SETTINGS_WRITTEN);

	installRealSettingsSource(NULL, NULL);
}

TEST_CASE("the recording group run before anything drives the recorder fails rather than doing nothing quietly", "[apply][record]")
{
	PhaseEnvironment env(ApplyPhase::Network);
	resetApplyRegistry();
	setRecordConfigOutput(NULL);
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Network) == Status::NotSupported);
	resetApplyRegistry();
}

// A second recording starting sets the recorder's stop-sectionsd flag by itself and a configuration sent meanwhile would clear it.
TEST_CASE("a change while a recording runs leaves the recorder's flags alone until it stops", "[apply][record]")
{
	RecordBox box;
	g_settings.recording_stream_vtxt_pid = 1;
	box.started();

	box.out.running = true;
	g_settings.recording_stream_vtxt_pid = 0;
	g_settings.timeshiftdir = "/media/sda1/ts2";
	REQUIRE(applyKey("recordingmenu.stream_vtxt_pid") == Status::Ok);
	REQUIRE(box.out.count("config") == 0);
	// What is not the recorder's running state still goes out.
	REQUIRE(box.out.last("timeshift") == "/media/sda1/ts2");

	// The application applies the group when the recording ends.
	box.out.running = false;
	box.out.forget();
	REQUIRE(applyKey("recording_stopsectionsd") == Status::Ok);
	REQUIRE(box.out.count("config") == 1);
	REQUIRE(box.out.last("config") == std::string("0") + "0" + (g_settings.recording_stream_pmt_pid ? "1" : "0") + (g_settings.recording_stream_subtitle_pids ? "1" : "0"));
}

TEST_CASE("a run with nothing changed sends nothing", "[apply][record]")
{
	RecordBox box;
	box.started();
	REQUIRE(applyKey("network_nfs_recordingdir") == Status::Ok);
	REQUIRE(applyKey("recordingmenu.stream_pmt_pid") == Status::Ok);
	REQUIRE(box.out.calls.empty());
}
