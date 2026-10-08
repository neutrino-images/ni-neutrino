/*
 * test_applykeys.cpp - the remote control group runs once per change and at startup
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
#include "support/applytest.h"

#include <neutrinoMessages.h>

#include "coreapi/base/apply.h"
#include "coreapi/base/deps.h"
#include "coreapi/box/apply_keys.h"
#include "coreapi/settings/settings.h"

#include <system/settings.h>

#include <cstdio>
#include <string>
#include <vector>

extern SNeutrinoSettings g_settings;

using namespace coreapi;

namespace
{

// The members the cases write, put back so the rest of the suite finds them as they were.
struct KeptSettings
{
	int block, generic, hardware;
	KeptSettings()
		: block(g_settings.repeat_blocker), generic(g_settings.repeat_genericblocker),
		  hardware(g_settings.remote_control_hardware) {}
	~KeptSettings()
	{
		g_settings.repeat_blocker = block;
		g_settings.repeat_genericblocker = generic;
		g_settings.remote_control_hardware = hardware;
	}
};

/* What a remote control case runs against: the seams the zapit phase has, the
   receiver taken as one that can be programmed, nothing sent yet, and the
   members it writes put back afterwards. */
struct RcBox
{
	KeptSettings kept;
	PhaseEnvironment env;
	FakeRcControl &rc;

	RcBox() : env(ApplyPhase::Zapit), rc(env.fake<FakeRcControl>("rc"))
	{
		env.fake<FakeSystemSource>("system").caps.rc_hw_select = 1;
		resetSentKeys();
	}
	~RcBox() { resetSentKeys(); }
};

} // namespace

TEST_CASE("the remote control group is registered by the one hook", "[apply][keys]")
{
	ApplyFresh fresh;
	registerApplyGroups();

	const char *const rc[] = { "repeat_blocker", "repeat_genericblocker", "remote_control_hardware" };
	for (size_t i = 0; i < sizeof(rc) / sizeof(rc[0]); ++i)
	{
		INFO(rc[i]);
		REQUIRE(groupOf(rc[i]) == &kRcApplyGroup);
	}
	// The input driver is built just before the zapit phase.
	REQUIRE(kRcApplyGroup.phase == ApplyPhase::Zapit);

	// Read where they are used.
	REQUIRE(groupOf("longkeypress_duration") == NULL);
}

TEST_CASE("startup runs the remote control group once and sends the repeat and the receiver", "[apply][keys]")
{
	ApplyFresh fresh;
	RcBox box;
	registerApplyGroups();

	g_settings.repeat_blocker = 450;
	g_settings.repeat_genericblocker = 100;
	g_settings.remote_control_hardware = 1;
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);

	REQUIRE(box.rc.repeats.size() == 1);
	REQUIRE(box.rc.repeats[0] == std::make_pair(450, 100));
	REQUIRE(box.rc.count("hardware") == 1);
}

TEST_CASE("a receiver that cannot be programmed is not told a remote control", "[apply][keys]")
{
	ApplyFresh fresh;
	RcBox box;
	registerApplyGroups();
	box.env.fake<FakeSystemSource>("system").caps.rc_hw_select = 0;

	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);
	REQUIRE(box.rc.count("repeat") == 1);
	REQUIRE(box.rc.count("hardware") == 0);
}

/* A change of one key sends that state and nothing else: the receiver is not
   programmed again for a change of the repeat, nor the repeat set again for a
   change of the remote control. */
TEST_CASE("a key of the remote control group sends only what differs from what was sent", "[apply][keys]")
{
	ApplyFresh fresh;
	RcBox box;
	registerApplyGroups();
	g_settings.remote_control_hardware = 0;
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);
	box.rc.forget();

	g_settings.repeat_blocker = 200;
	REQUIRE(applyKey("repeat_blocker") == Status::Ok);
	REQUIRE(box.rc.calls.size() == 1);
	REQUIRE(box.rc.calls[0] == "repeat");
	REQUIRE(box.rc.repeats[0].first == 200);
	box.rc.forget();

	g_settings.remote_control_hardware = 2;
	REQUIRE(applyKey("remote_control_hardware") == Status::Ok);
	REQUIRE(box.rc.calls.size() == 1);
	REQUIRE(box.rc.calls[0] == "hardware");
	box.rc.forget();

	// Nothing changed, nothing sent.
	REQUIRE(applyKey("repeat_genericblocker") == Status::Ok);
	REQUIRE(box.rc.calls.empty());
}

TEST_CASE("a refused repeat is sent again by the next run", "[apply][keys]")
{
	ApplyFresh fresh;
	RcBox box;
	registerApplyGroups();
	box.rc.repeat_answer = Status::Internal;
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Internal);
	REQUIRE(box.rc.count("repeat") == 1);

	box.rc.repeat_answer = Status::Ok;
	REQUIRE(applyKey("repeat_blocker") == Status::Ok);
	REQUIRE(box.rc.count("repeat") == 2);
}

/* The question about a new remote control puts back the one the receiver had
   right before the change, which the group knows and the screen does not. */
TEST_CASE("the group states the remote control the receiver had before the last change", "[apply][keys]")
{
	ApplyFresh fresh;
	RcBox box;
	registerApplyGroups();
	REQUIRE(remoteHardwareBeforeLastChange() == -1);

	g_settings.remote_control_hardware = 0;
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);
	REQUIRE(remoteHardwareBeforeLastChange() == 0);

	g_settings.remote_control_hardware = 2;
	REQUIRE(applyKey("remote_control_hardware") == Status::Ok);
	REQUIRE(remoteHardwareBeforeLastChange() == 0);

	// A no puts the old one back, and that is a change of its own.
	g_settings.remote_control_hardware = 0;
	REQUIRE(applyKey("remote_control_hardware") == Status::Ok);
	REQUIRE(remoteHardwareBeforeLastChange() == 2);
	REQUIRE(box.rc.count("hardware") == 3);
}

TEST_CASE("a web batch writing both repeat values and the remote control runs the group once", "[apply][keys]")
{
	ApplyFresh fresh;
	RcBox box;
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, applyNothingToSave);

	g_settings.remote_control_hardware = 0;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);
	box.rc.forget();

	REQUIRE(settings::set("repeat_blocker", "300").ok());
	REQUIRE(settings::set("repeat_genericblocker", "50").ok());
	REQUIRE(settings::set("remote_control_hardware", "1").ok());
	REQUIRE(box.rc.calls.empty());
	const size_t posted = sink.posted.size();
	applyPendingSettings();

	REQUIRE(box.rc.count("repeat") == 1);
	REQUIRE(box.rc.repeats[0] == std::make_pair(300, 50));
	REQUIRE(box.rc.count("hardware") == 1);
	REQUIRE(events.sent.empty());
	for (size_t i = posted; i < sink.posted.size(); ++i)
		CHECK(sink.posted[i].first == NeutrinoMessages::EVT_SETTINGS_WRITTEN);

	installRealSettingsSource(NULL, NULL);
}

TEST_CASE("the remote control group run before anything drives the input fails rather than doing nothing quietly", "[apply][keys]")
{
	ApplyFresh fresh;
	RcBox box;
	registerApplyGroups();
	setRcControl(0);
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::NotSupported);
}
