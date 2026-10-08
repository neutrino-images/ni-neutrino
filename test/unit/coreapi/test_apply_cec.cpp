/*
 * test_apply_cec.cpp - the CEC apply group
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
#include "coreapi/box/apply_cec.h"
#include "coreapi/settings/settings.h"

#include <system/settings.h>

#include <hardware/video.h>

#include <cstdio>
#include <string>

extern SNeutrinoSettings g_settings;

using namespace coreapi;

namespace
{

// The members the cases write, put back so the rest of the suite finds them as they were.
struct KeptCecSettings
{
	int mode, view_on, standby, volume, current_volume;
	KeptCecSettings()
		: mode(g_settings.hdmi_cec_mode), view_on(g_settings.hdmi_cec_view_on),
		  standby(g_settings.hdmi_cec_standby), volume(g_settings.hdmi_cec_volume),
		  current_volume(g_settings.current_volume) {}
	~KeptCecSettings()
	{
		g_settings.hdmi_cec_mode = mode;
		g_settings.hdmi_cec_view_on = view_on;
		g_settings.hdmi_cec_standby = standby;
		g_settings.hdmi_cec_volume = volume;
		g_settings.current_volume = current_volume;
	}
};

/* What a CEC case runs against: the seams the channel daemon phase has, the fake
   link among them, nothing sent, no deferral, and the members put back. */
struct CecBox
{
	KeptCecSettings kept;
	PhaseEnvironment env;
	FakeCecLink &link;

	CecBox() : env(ApplyPhase::Zapit), link(env.fake<FakeCecLink>("cec"))
	{
		resetApplyRegistry();
		resetSentCec();
		registerApplyGroups();
	}
	~CecBox()
	{
		resetSentCec();
		resetApplyRegistry();
	}
};

bool cecSave() { return true; }

std::string cecNumber(int v)
{
	char text[16];
	snprintf(text, sizeof(text), "%d", v);
	return text;
}

} // namespace

TEST_CASE("the CEC group holds the four keys of both screens and runs after the channel daemon's client", "[apply][cec]")
{
	CecBox box;
	const char *const keys[] = { "hdmi_cec_mode", "hdmi_cec_view_on", "hdmi_cec_standby", "hdmi_cec_volume" };
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		INFO(keys[i]);
		REQUIRE(groupOf(keys[i]) == &kCecApplyGroup);
	}
	REQUIRE(kCecApplyGroup.phase == ApplyPhase::Zapit);
}

TEST_CASE("startup runs the CEC group once, the mode last", "[apply][cec]")
{
	CecBox box;
	g_settings.hdmi_cec_mode = VIDEO_HDMI_CEC_MODE_TUNER;
	g_settings.hdmi_cec_standby = 1;
	g_settings.hdmi_cec_view_on = 0;
	g_settings.hdmi_cec_volume = 2;
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);

	REQUIRE(box.link.count("standby") == 1);
	REQUIRE(box.link.count("view") == 1);
	REQUIRE(box.link.count("destination") == 1);
	REQUIRE(box.link.count("mode") == 1);
	REQUIRE(box.link.calls.back() == "mode");
	REQUIRE(box.link.values.back() == VIDEO_HDMI_CEC_MODE_TUNER);
	REQUIRE(box.link.values[0] == 1);
	REQUIRE(box.link.values[1] == 0);
	REQUIRE(box.link.values[2] == 2);
}

// A box whose driver has no destination is not asked for one.
TEST_CASE("a link without a destination is sent the rest", "[apply][cec]")
{
	CecBox box;
	box.link.takes_destination = false;
	g_settings.hdmi_cec_volume = 1;
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);
	REQUIRE(box.link.count("destination") == 0);
	REQUIRE(box.link.count("mode") == 1);
}

TEST_CASE("a key of the CEC group sends only what differs from what was sent", "[apply][cec]")
{
	CecBox box;
	g_settings.hdmi_cec_mode = VIDEO_HDMI_CEC_MODE_TUNER;
	g_settings.hdmi_cec_standby = 1;
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);
	box.link.forget();

	g_settings.hdmi_cec_standby = 0;
	REQUIRE(applyKey("hdmi_cec_standby") == Status::Ok);
	REQUIRE(box.link.calls.size() == 1);
	REQUIRE(box.link.calls[0] == "standby");
	REQUIRE(box.link.values[0] == 0);

	// Nothing changed, nothing sent: the link is not started over.
	box.link.forget();
	REQUIRE(applyKey("hdmi_cec_mode") == Status::Ok);
	REQUIRE(box.link.calls.empty());

	g_settings.hdmi_cec_mode = VIDEO_HDMI_CEC_MODE_OFF;
	REQUIRE(applyKey("hdmi_cec_mode") == Status::Ok);
	REQUIRE(box.link.calls.size() == 1);
	REQUIRE(box.link.calls[0] == "mode");
}

// A send the driver refused is not taken as made, so the next run tries again.
TEST_CASE("a refused CEC mode is sent again by the next run", "[apply][cec]")
{
	CecBox box;
	g_settings.hdmi_cec_mode = VIDEO_HDMI_CEC_MODE_OFF;
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);
	box.link.forget();

	box.link.mode_answer = Status::Internal;
	g_settings.hdmi_cec_mode = VIDEO_HDMI_CEC_MODE_TUNER;
	REQUIRE(applyKey("hdmi_cec_mode") == Status::Internal);
	box.link.mode_answer = Status::Ok;
	box.link.forget();
	REQUIRE(applyKey("hdmi_cec_mode") == Status::Ok);
	REQUIRE(box.link.count("mode") == 1);
}

/* The box woke for a recording: the television is left alone, a write meanwhile
   sends nothing either, and the run after the deferral sends everything. */
TEST_CASE("a deferred CEC group sends nothing until the deferral ends and then sends everything", "[apply][cec]")
{
	CecBox box;
	g_settings.hdmi_cec_mode = VIDEO_HDMI_CEC_MODE_TUNER;
	g_settings.hdmi_cec_standby = 1;
	g_settings.hdmi_cec_view_on = 1;
	deferCec(true);
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);
	REQUIRE(box.link.calls.empty());

	g_settings.hdmi_cec_standby = 0;
	REQUIRE(applyKey("hdmi_cec_standby") == Status::Ok);
	REQUIRE(box.link.calls.empty());

	deferCec(false);
	REQUIRE(applyKey("hdmi_cec_mode") == Status::Ok);
	REQUIRE(box.link.count("standby") == 1);
	REQUIRE(box.link.values[0] == 0);
	REQUIRE(box.link.count("view") == 1);
	REQUIRE(box.link.count("destination") == 1);
	REQUIRE(box.link.count("mode") == 1);
}

/* A write of the mode while the box sleeps would switch the television on and make the
   sleeping box its source, so it waits for the box to wake, and is sent once then. */
TEST_CASE("a CEC mode written while the box sleeps is sent once when it wakes", "[apply][cec]")
{
	CecBox box;
	g_settings.hdmi_cec_mode = VIDEO_HDMI_CEC_MODE_OFF;
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);
	box.link.forget();

	REQUIRE(cecStandby(true) == Status::Ok);
	g_settings.hdmi_cec_mode = VIDEO_HDMI_CEC_MODE_TUNER;
	REQUIRE(applyKey("hdmi_cec_mode") == Status::Ok);
	REQUIRE(box.link.calls.empty());

	REQUIRE(cecStandby(false) == Status::Ok);
	REQUIRE(box.link.count("mode") == 1);
	REQUIRE(box.link.values.back() == VIDEO_HDMI_CEC_MODE_TUNER);

	// A sleep in which nothing was written sends nothing on waking.
	box.link.forget();
	REQUIRE(cecStandby(true) == Status::Ok);
	REQUIRE(cecStandby(false) == Status::Ok);
	REQUIRE(box.link.calls.empty());
}

TEST_CASE("a CEC volume destination written while deferred leaves the box volume at full", "[apply][cec]")
{
	CecBox box;
	g_settings.hdmi_cec_mode = VIDEO_HDMI_CEC_MODE_TUNER;
	g_settings.hdmi_cec_volume = 0;
	g_settings.current_volume = 40;
	deferCec(true);
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);
	REQUIRE(g_settings.current_volume == 40);

	g_settings.hdmi_cec_volume = 2;
	REQUIRE(applyKey("hdmi_cec_volume") == Status::Ok);
	REQUIRE(g_settings.current_volume == 100);
	REQUIRE(box.link.calls.empty());
}

/* Handing the volume keys to somebody else puts the box's own volume at full, so
   the other end is the one that attenuates. */
TEST_CASE("changing the CEC volume destination with the link on sets the box volume to full", "[apply][cec]")
{
	CecBox box;
	g_settings.hdmi_cec_mode = VIDEO_HDMI_CEC_MODE_TUNER;
	g_settings.hdmi_cec_volume = 0;
	g_settings.current_volume = 40;
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);
	// The first run is the box starting with the volume it was saved at.
	REQUIRE(g_settings.current_volume == 40);

	g_settings.hdmi_cec_volume = 2;
	REQUIRE(applyKey("hdmi_cec_volume") == Status::Ok);
	REQUIRE(g_settings.current_volume == 100);
	REQUIRE(box.link.values.back() == 2);
}

/* With the link off the destination is not sent and the box volume stays; the
   link coming on sends it, before the mode. */
TEST_CASE("changing the CEC volume destination with the link off sends it once the link is on", "[apply][cec]")
{
	CecBox box;
	g_settings.hdmi_cec_mode = VIDEO_HDMI_CEC_MODE_OFF;
	g_settings.hdmi_cec_volume = 0;
	g_settings.current_volume = 40;
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);
	REQUIRE(box.link.count("destination") == 1);
	box.link.forget();

	g_settings.hdmi_cec_volume = 1;
	REQUIRE(applyKey("hdmi_cec_volume") == Status::Ok);
	REQUIRE(g_settings.current_volume == 40);
	REQUIRE(box.link.calls.empty());

	g_settings.hdmi_cec_mode = VIDEO_HDMI_CEC_MODE_TUNER;
	REQUIRE(applyKey("hdmi_cec_mode") == Status::Ok);
	REQUIRE(box.link.calls.size() == 2);
	REQUIRE(box.link.calls[0] == "destination");
	REQUIRE(box.link.values[0] == 1);
	REQUIRE(box.link.calls[1] == "mode");
}

TEST_CASE("a web batch writing CEC keys runs the group once and asks nothing", "[apply][cec]")
{
	CecBox box;
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, cecSave);
	// The rows are the box's to offer, and a box with no link offers none of them.
	box.env.fake<FakeSystemSource>("system").caps.can_cec = 1;

	g_settings.hdmi_cec_mode = VIDEO_HDMI_CEC_MODE_OFF;
	g_settings.hdmi_cec_standby = 0;
	g_settings.hdmi_cec_view_on = 0;
	g_settings.current_volume = 40;
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);
	box.link.forget();

	REQUIRE(settings::set("hdmi_cec_mode", cecNumber(VIDEO_HDMI_CEC_MODE_TUNER)).ok());
	REQUIRE(settings::set("hdmi_cec_standby", cecNumber(1)).ok());
	REQUIRE(settings::set("hdmi_cec_view_on", cecNumber(1)).ok());
	REQUIRE(box.link.calls.empty());
	const size_t posted = sink.posted.size();
	applyPendingSettings();

	REQUIRE(box.link.count("mode") == 1);
	REQUIRE(box.link.count("standby") == 1);
	REQUIRE(box.link.count("view") == 1);
	REQUIRE(box.link.calls.size() == 3);
	REQUIRE(events.sent.empty());
	REQUIRE(g_settings.current_volume == 40);
	for (size_t i = posted; i < sink.posted.size(); ++i)
		CHECK(sink.posted[i].first == NeutrinoMessages::EVT_SETTINGS_WRITTEN);

	installRealSettingsSource(NULL, NULL);
}

TEST_CASE("a group run before anything drives the link fails rather than doing nothing quietly", "[apply][cec]")
{
	PhaseEnvironment env(ApplyPhase::Zapit);
	resetApplyRegistry();
	resetSentCec();
	setCecLink(NULL);
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::NotSupported);
	REQUIRE(applyKey("hdmi_cec_mode") == Status::NotSupported);
	resetApplyRegistry();
	resetSentCec();
}
