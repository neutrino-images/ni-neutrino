/*
 * test_applyupdate.cpp - tests for the update check apply group
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

#include "coreapi/base/apply.h"
#include "coreapi/box/apply_update.h"

#include <system/settings.h>

extern SNeutrinoSettings g_settings;

using namespace coreapi;

namespace
{

struct UpdateBox
{
	int              flash;
#if ENABLE_PKG_MANAGEMENT
	int              packages;
#endif
	PhaseEnvironment env;
	FakeUpdateCheck &check;

	UpdateBox()
		: flash(g_settings.softupdate_autocheck),
#if ENABLE_PKG_MANAGEMENT
		  packages(g_settings.softupdate_autocheck_packages),
#endif
		  env(ApplyPhase::Network), check(env.fake<FakeUpdateCheck>("updatecheck"))
	{
		resetApplyRegistry();
		resetSentUpdateChecks();
		registerApplyGroups();
	}
	~UpdateBox()
	{
		g_settings.softupdate_autocheck = flash;
#if ENABLE_PKG_MANAGEMENT
		g_settings.softupdate_autocheck_packages = packages;
#endif
		resetSentUpdateChecks();
		resetApplyRegistry();
	}
};

} // namespace

TEST_CASE("the update group answers for both automatic checks", "[apply][update]")
{
	UpdateBox box;
	REQUIRE(groupOf("softupdate_autocheck") == &kUpdateApplyGroup);
	REQUIRE(groupOf("softupdate_autocheck_packages") == &kUpdateApplyGroup);
}

TEST_CASE("the phase run and a write before the program has finished starting start nothing", "[apply][update]")
{
	UpdateBox box;
	g_settings.softupdate_autocheck = 1;
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	REQUIRE(applyKey("softupdate_autocheck") == Status::Ok);
	REQUIRE(box.check.calls.empty());
}

TEST_CASE("the end of startup starts the checks the settings ask for, once", "[apply][update]")
{
	UpdateBox box;
	g_settings.softupdate_autocheck = 1;
#if ENABLE_PKG_MANAGEMENT
	g_settings.softupdate_autocheck_packages = 0;
#endif
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);

	startUpdateChecks();
	REQUIRE(box.check.calls.size() >= 1);
	REQUIRE(box.check.calls[0] == "flash:on");

	// Asking again changes nothing.
	const size_t sent = box.check.calls.size();
	REQUIRE(applyKey("softupdate_autocheck") == Status::Ok);
	REQUIRE(box.check.calls.size() == sent);
}

TEST_CASE("a change of one check after startup touches that check only", "[apply][update]")
{
	UpdateBox box;
	g_settings.softupdate_autocheck = 0;
#if ENABLE_PKG_MANAGEMENT
	g_settings.softupdate_autocheck_packages = 0;
#endif
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	startUpdateChecks();
	box.check.calls.clear();

	g_settings.softupdate_autocheck = 1;
	REQUIRE(applyKey("softupdate_autocheck") == Status::Ok);
	REQUIRE(box.check.calls.size() == 1);
	REQUIRE(box.check.calls[0] == "flash:on");

	// A second run with the same setting leaves the running check alone.
	REQUIRE(applyKey("softupdate_autocheck_packages") == Status::Ok);
	REQUIRE(box.check.calls.size() == 1);

	g_settings.softupdate_autocheck = 0;
	REQUIRE(applyKey("softupdate_autocheck") == Status::Ok);
	REQUIRE(box.check.calls.size() == 2);
	REQUIRE(box.check.calls[1] == "flash:off");
}

TEST_CASE("a check that is off at startup is not touched, and switching it on later fixes its hours", "[apply][update]")
{
	UpdateBox box;
	g_settings.softupdate_autocheck = 0;
#if ENABLE_PKG_MANAGEMENT
	g_settings.softupdate_autocheck_packages = 0;
#endif
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	startUpdateChecks();
	REQUIRE(box.check.calls.empty());

#if ENABLE_PKG_MANAGEMENT
	g_settings.softupdate_autocheck_packages = 12;
	REQUIRE(applyKey("softupdate_autocheck_packages") == Status::Ok);
	REQUIRE(box.check.calls.size() == 1);
	REQUIRE(box.check.calls[0] == "packages:on");
	REQUIRE(box.check.hours_at_on == 12);
#endif
}
