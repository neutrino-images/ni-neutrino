/*
 * test_applyci.cpp - tests for the ci apply group
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
#include "coreapi/box/apply_ci.h"
#include "coreapi/settings/settings.h"

#include <system/settings.h>

#include <cstdio>
#include <string>

extern SNeutrinoSettings g_settings;

using namespace coreapi;

namespace
{

// The members the cases write, put back so the rest of the suite finds them as they were.
struct KeptCiSettings
{
	int tuner, clock[4], save_pin[4];
	std::string pin[4];
	KeptCiSettings() : tuner(g_settings.ci_tuner)
	{
		for (int i = 0; i < 4; ++i)
		{
			clock[i] = g_settings.ci_clock[i];
			save_pin[i] = g_settings.ci_save_pincode[i];
			pin[i] = g_settings.ci_pincode[i];
		}
	}
	~KeptCiSettings()
	{
		g_settings.ci_tuner = tuner;
		for (int i = 0; i < 4; ++i)
		{
			g_settings.ci_clock[i] = clock[i];
			g_settings.ci_save_pincode[i] = save_pin[i];
			g_settings.ci_pincode[i] = pin[i];
		}
	}
};

/* What a ci case runs against: the seams the first phase has, the module seam
   among them, a box with the slots the case names, nothing sent yet. */
struct CiBox
{
	KeptCiSettings   kept;
	PhaseEnvironment env;
	FakeCiControl   &ci;
	FakeSystemSource &system;

	explicit CiBox(unsigned slots)
		: env(ApplyPhase::Framebuffer), ci(env.fake<FakeCiControl>("ci")),
		  system(env.fake<FakeSystemSource>("system"))
	{
		resetApplyRegistry();
		system.ci_slots = slots;
		resetSentCi();
		g_settings.ci_tuner = -1;
		for (int i = 0; i < 4; ++i)
			g_settings.ci_clock[i] = 6;
	}
	~CiBox()
	{
		resetSentCi();
		resetApplyRegistry();
	}
};

bool ciSave() { return true; }

std::string number(int v)
{
	char text[16];
	snprintf(text, sizeof(text), "%d", v);
	return text;
}

} // namespace

TEST_CASE("the ci group answers for the module slots' settings and for no other cam setting", "[apply][ci]")
{
	CiBox box(2);
	registerApplyGroups();
	const char *const grouped[] = { "ci_tuner", "ci_check_live", "ci_delay", "ci_clock_0", "ci_clock_3",
					"ci_rpr_0", "ci_rpr_3", "ci_op_0", "ci_op_3" };
	for (size_t i = 0; i < sizeof(grouped) / sizeof(grouped[0]); ++i)
	{
		INFO(grouped[i]);
		REQUIRE(groupOf(grouped[i]) == &kCiApplyGroup);
	}
	// Read where they are used. The pin a slot keeps follows its switch by a coupling of the write.
	REQUIRE(groupOf("ci_save_pincode_0") == NULL);
	REQUIRE(groupOf("ci_mode") == NULL);
	REQUIRE(groupOf("ci_rec_zapto") == NULL);
	REQUIRE(groupOf("ci_standby_reset") == NULL);
	REQUIRE(kCiApplyGroup.phase == ApplyPhase::Framebuffer);
}

TEST_CASE("startup runs the ci group once, tells the tuner first and the clock of each slot the box has", "[apply][ci]")
{
	CiBox box(2);
	registerApplyGroups();
	g_settings.ci_tuner = 3;
	g_settings.ci_clock[0] = 7;
	g_settings.ci_clock[1] = 8;
	g_settings.ci_clock[2] = 9;

	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);

	REQUIRE(box.ci.calls.size() >= 3);
	REQUIRE(box.ci.calls[0] == "tuner");
	REQUIRE(box.ci.args[0].first == 3);
	REQUIRE(box.ci.count("clock") == 2);
	REQUIRE(box.ci.args[1] == std::make_pair(0, 7));
	REQUIRE(box.ci.args[2] == std::make_pair(1, 8));
}

TEST_CASE("a box that cannot say how many slots it has gets no clock", "[apply][ci]")
{
	CiBox box(0);
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	REQUIRE(box.ci.count("clock") == 0);
	REQUIRE(box.ci.count("tuner") == 1);
}

TEST_CASE("a change of one clock on the box sends that slot's clock and nothing else", "[apply][ci]")
{
	CiBox box(2);
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.ci.forget();

	g_settings.ci_clock[1] = 10;
	REQUIRE(applyKey("ci_clock_1") == Status::Ok);

	REQUIRE(box.ci.calls.size() == 1);
	REQUIRE(box.ci.calls[0] == "clock");
	REQUIRE(box.ci.args[0] == std::make_pair(1, 10));
}

TEST_CASE("a key whose value the box already has sends nothing", "[apply][ci]")
{
	CiBox box(2);
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.ci.forget();

	REQUIRE(applyKey("ci_tuner") == Status::Ok);
	REQUIRE(applyKey("ci_clock_0") == Status::Ok);
	REQUIRE(box.ci.calls.empty());
}

TEST_CASE("a clock the driver refused is sent again by the next run", "[apply][ci]")
{
	CiBox box(1);
	registerApplyGroups();
	box.ci.clock_answer = Status::Internal;
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Internal);
	box.ci.forget();

	box.ci.clock_answer = Status::Ok;
	REQUIRE(applyKey("ci_tuner") == Status::Ok);
	REQUIRE(box.ci.count("clock") == 1);
	REQUIRE(box.ci.count("tuner") == 0);
}

TEST_CASE("a web batch of two clocks and the tuner runs the ci group once and sends each once", "[apply][ci]")
{
	CiBox box(2);
	// The tuner written must be one the box offers, since the row takes only those.
	FakeTunerSource tuner;
	for (int i = 0; i < 3; ++i)
	{
		FrontendInfo fe;
		fe.number = i;
		fe.name = "tuner";
		tuner.tuners.push_back(fe);
	}
	InstalledTunerSource installed_tuner(&tuner);
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, ciSave);
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.ci.forget();

	REQUIRE(settings::set("ci_clock_0", number(11)).ok());
	REQUIRE(settings::set("ci_clock_1", number(12)).ok());
	REQUIRE(settings::set("ci_tuner", number(2)).ok());
	REQUIRE(box.ci.calls.empty());
	applyPendingSettings();

	REQUIRE(box.ci.calls.size() == 3);
	REQUIRE(box.ci.count("tuner") == 1);
	REQUIRE(box.ci.count("clock") == 2);
	REQUIRE(events.sent.empty());

	installRealSettingsSource(NULL, NULL);
}

TEST_CASE("a clock of a slot the box lacks is refused by the declaration", "[apply][ci]")
{
	CiBox box(1);
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, ciSave);

	REQUIRE(settings::set("ci_clock_0", number(7)).ok());
	REQUIRE_FALSE(settings::set("ci_clock_1", number(7)).ok());

	installRealSettingsSource(NULL, NULL);
}
