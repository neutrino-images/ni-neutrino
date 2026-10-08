/*
 * test_compat_standby.cpp - what /control/standby answers and sends
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

#include "support/catch.hpp"

// compat/ is not compiled with --disable-legacy-api (src/httpd/Makefile.am).
#include <config.h>

#ifndef DISABLE_LEGACY_API

#include "support/fakes.h"
#include "support/fakececlink.h"

#include "httpd/compat/standby.h"

#include <neutrinoMessages.h>

#include <string>

using httpd::compat::CyhookHandler;

namespace
{

/* The handler as the legacy dispatcher hands it over: the bare token at "1" and
   the pairs by name. */
std::string ask(const std::string &token, const std::string &cec)
{
	CyhookHandler hh;
	if (!token.empty())
		hh.ParamList["1"] = token;
	if (!cec.empty())
		hh.ParamList["cec"] = cec;
	httpd::compat::answerStandby(hh);
	return hh.yresult;
}

/* The box in standby or not, the event sink recording, and the CEC settings on,
   so a hold-back would have something to hold. */
struct StandbyBox
{
	FakeChannelSource channels;
	InstalledChannelSource in_channels;
	FakeEventSink events;
	InstalledEventSink in_events;
	CecSettingsAndLink cec;

	explicit StandbyBox(bool asleep)
		: in_channels(&channels), in_events(&events), cec(1, 1)
	{
		channels.mode = asleep ? NeutrinoModes::mode_standby : NeutrinoModes::mode_tv;
	}
};

} // namespace

TEST_CASE("control/standby with cec=off sends the change with the television left alone", "[compat][standby]")
{
	const std::string leave(1, (char) NeutrinoStandby::leave_tv);
	{
		StandbyBox box(false);
		REQUIRE(ask("on", "off") == "ok");
		REQUIRE(box.events.sent.size() == 1);
		REQUIRE(box.events.sent[0].id == (unsigned) NeutrinoMessages::STANDBY_ON);
		REQUIRE(box.events.sent[0].body == leave);
		REQUIRE(box.cec.link.calls.empty());
	}
	{
		StandbyBox box(true);
		REQUIRE(ask("off", "off") == "ok");
		REQUIRE(box.events.sent.size() == 1);
		REQUIRE(box.events.sent[0].id == (unsigned) NeutrinoMessages::STANDBY_OFF);
		REQUIRE(box.events.sent[0].body == leave);
		REQUIRE(box.cec.link.calls.empty());
	}
}

TEST_CASE("control/standby without cec=off sends the plain change", "[compat][standby]")
{
	{
		StandbyBox box(false);
		REQUIRE(ask("on", "") == "ok");
		REQUIRE(box.events.sent.size() == 1);
		REQUIRE(box.events.sent[0].id == (unsigned) NeutrinoMessages::STANDBY_ON);
		REQUIRE(box.events.sent[0].body.empty());
	}
	{
		// Any other value is not off.
		StandbyBox box(true);
		REQUIRE(ask("off", "on") == "ok");
		REQUIRE(box.events.sent.size() == 1);
		REQUIRE(box.events.sent[0].id == (unsigned) NeutrinoMessages::STANDBY_OFF);
		REQUIRE(box.events.sent[0].body.empty());
	}
}

TEST_CASE("control/standby to the state the box is in sends nothing and answers ok", "[compat][standby]")
{
	{
		StandbyBox box(true);
		REQUIRE(ask("on", "off") == "ok");
		REQUIRE(box.events.sent.empty());
		REQUIRE(box.cec.link.calls.empty());
	}
	{
		StandbyBox box(false);
		REQUIRE(ask("off", "off") == "ok");
		REQUIRE(box.events.sent.empty());
	}
}

TEST_CASE("control/standby answers the state without a parameter and an error for an unknown one", "[compat][standby]")
{
	{
		StandbyBox box(true);
		REQUIRE(ask("", "") == "on\r\n");
	}
	{
		StandbyBox box(false);
		REQUIRE(ask("", "") == "off\r\n");
		REQUIRE(ask("toggle", "") == "error");
		REQUIRE(box.events.sent.empty());
	}
}

TEST_CASE("control/standby whose change did not go out answers error", "[compat][standby]")
{
	StandbyBox box(false);
	box.events.unsupported = NeutrinoMessages::STANDBY_ON;
	REQUIRE(ask("on", "off") == "error");
}

#endif // DISABLE_LEGACY_API
