/*
 * test_apply_webchannels.cpp - the web channel apply groups
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
#include "coreapi/box/apply_webchannels.h"
#include "coreapi/settings/settings.h"

#include <system/settings.h>

extern SNeutrinoSettings g_settings;

using namespace coreapi;

namespace
{

struct KeptWebSettings
{
	int tv_auto, radio_auto, resolution;
	KeptWebSettings()
		: tv_auto(g_settings.webtv_xml_auto), radio_auto(g_settings.webradio_xml_auto),
		  resolution(g_settings.livestreamResolution) {}
	~KeptWebSettings()
	{
		g_settings.webtv_xml_auto = tv_auto;
		g_settings.webradio_xml_auto = radio_auto;
		g_settings.livestreamResolution = resolution;
	}
};

struct WebBox
{
	KeptWebSettings kept;
	PhaseEnvironment env;
	FakeWebChannels &web;

	WebBox() : env(ApplyPhase::Sectionsd), web(env.fake<FakeWebChannels>("webchannels"))
	{
		resetApplyRegistry();
		resetWebChannels();
		registerApplyGroups();
	}
	~WebBox()
	{
		resetWebChannels();
		resetApplyRegistry();
	}

	// Startup done: the shape noted, nothing asked.
	void started()
	{
		REQUIRE(runPhase(ApplyPhase::Sectionsd) == Status::Ok);
		REQUIRE(web.reloads == 0);
		REQUIRE(web.restarts == 0);
	}
};

bool webSave() { return true; }

} // namespace

TEST_CASE("the web channel groups hold their keys and run once the lists are made", "[apply][webchannels]")
{
	WebBox box;
	REQUIRE(groupOf("webtv_xml_auto") == &kWebChannelsApplyGroup);
	REQUIRE(groupOf("webradio_xml_auto") == &kWebChannelsApplyGroup);
	REQUIRE(groupOf("livestreamResolution") == &kLivestreamApplyGroup);
	REQUIRE(kWebChannelsApplyGroup.phase == ApplyPhase::Sectionsd);
	REQUIRE(kLivestreamApplyGroup.phase == ApplyPhase::Sectionsd);
}

TEST_CASE("startup reads no list again and starts no stream over", "[apply][webchannels]")
{
	WebBox box;
	g_settings.webtv_xml_auto = 1;
	g_settings.webradio_xml_auto = 0;
	g_settings.livestreamResolution = 1280;
	box.started();
}

TEST_CASE("a change of either switch reads the lists again once", "[apply][webchannels]")
{
	WebBox box;
	g_settings.webtv_xml_auto = 1;
	g_settings.webradio_xml_auto = 1;
	box.started();

	g_settings.webtv_xml_auto = 0;
	REQUIRE(applyKey("webtv_xml_auto") == Status::Ok);
	REQUIRE(box.web.reloads == 1);

	// Nothing differs from what the lists were read with.
	REQUIRE(applyKey("webradio_xml_auto") == Status::Ok);
	REQUIRE(box.web.reloads == 1);

	g_settings.webradio_xml_auto = 0;
	REQUIRE(applyKey("webradio_xml_auto") == Status::Ok);
	REQUIRE(box.web.reloads == 2);
	REQUIRE(box.web.restarts == 0);
}

TEST_CASE("a refused reload of the lists is asked for again by the next run", "[apply][webchannels]")
{
	WebBox box;
	g_settings.webtv_xml_auto = 1;
	box.started();

	box.web.reload_answer = Status::Internal;
	g_settings.webtv_xml_auto = 0;
	REQUIRE(applyKey("webtv_xml_auto") == Status::Internal);
	box.web.reload_answer = Status::Ok;
	REQUIRE(applyKey("webtv_xml_auto") == Status::Ok);
	REQUIRE(box.web.reloads == 2);
	REQUIRE(applyKey("webtv_xml_auto") == Status::Ok);
	REQUIRE(box.web.reloads == 2);
}

TEST_CASE("a change of the picture size starts the stream over once", "[apply][webchannels]")
{
	WebBox box;
	g_settings.livestreamResolution = 1920;
	box.started();

	g_settings.livestreamResolution = 1280;
	REQUIRE(applyKey("livestreamResolution") == Status::Ok);
	REQUIRE(box.web.restarts == 1);

	// The same size again must not bounce the stream.
	REQUIRE(applyKey("livestreamResolution") == Status::Ok);
	REQUIRE(box.web.restarts == 1);
	REQUIRE(box.web.reloads == 0);
}

TEST_CASE("a refused restart of the stream is asked for again by the next run", "[apply][webchannels]")
{
	WebBox box;
	g_settings.livestreamResolution = 1920;
	box.started();

	box.web.restart_answer = Status::Internal;
	g_settings.livestreamResolution = 854;
	REQUIRE(applyKey("livestreamResolution") == Status::Internal);
	box.web.restart_answer = Status::Ok;
	REQUIRE(applyKey("livestreamResolution") == Status::Ok);
	REQUIRE(box.web.restarts == 2);
}

TEST_CASE("a web batch writing both switches and the size runs each group once", "[apply][webchannels]")
{
	WebBox box;
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, webSave);
	g_settings.webtv_xml_auto = 1;
	g_settings.webradio_xml_auto = 1;
	g_settings.livestreamResolution = 1920;
	box.started();

	REQUIRE(settings::set("webtv_xml_auto", "0").ok());
	REQUIRE(settings::set("webradio_xml_auto", "0").ok());
	REQUIRE(settings::set("livestreamResolution", "1280").ok());
	REQUIRE(box.web.reloads == 0);
	REQUIRE(box.web.restarts == 0);
	const size_t posted = sink.posted.size();
	applyPendingSettings();

	REQUIRE(box.web.reloads == 1);
	REQUIRE(box.web.restarts == 1);
	REQUIRE(events.sent.empty());
	for (size_t i = posted; i < sink.posted.size(); ++i)
		CHECK(sink.posted[i].first == NeutrinoMessages::EVT_SETTINGS_WRITTEN);

	installRealSettingsSource(NULL, NULL);
}

// An old settings file may hold a size the list does not offer: it loads and plays, and only a new pick of one is refused.
TEST_CASE("a stored size outside the list stays as loaded and another one outside it is refused", "[apply][webchannels]")
{
	WebBox box;
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, webSave);
	g_settings.livestreamResolution = 1000;
	box.started();
	REQUIRE(g_settings.livestreamResolution == 1000);

	REQUIRE_FALSE(settings::set("livestreamResolution", "1001").ok());
	REQUIRE(g_settings.livestreamResolution == 1000);
	REQUIRE(box.web.restarts == 0);
	// The stored value written back is no new pick.
	CHECK(settings::set("livestreamResolution", "1000").ok());

	installRealSettingsSource(NULL, NULL);
}
