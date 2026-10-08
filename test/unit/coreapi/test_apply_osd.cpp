/*
 * test_apply_osd.cpp - tests for the OSD apply groups
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
#include "coreapi/box/apply_osd.h"
#include "coreapi/settings/settings.h"

#include <system/settings.h>

#include <cstdio>
#include <string>
#include <utility>
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
struct KeptOsdSettings
{
	std::string font_file, font_mono;
	int scaling_x, scaling_y;
	SNeutrinoTheme theme;
	int preset, corner_x, corner_y;
	int sat_display, infobar_progressbar;
	int mode_clock, clock_size, clock_seconds, clock_background, volume_digits, volume_size;
	int radiotext;

	KeptOsdSettings()
		: font_file(settingsText(g_settings.font_file)), font_mono(settingsText(g_settings.font_file_monospace)),
		  scaling_x(g_settings.font_scaling_x), scaling_y(g_settings.font_scaling_y), theme(g_settings.theme),
		  preset(g_settings.screen_preset), corner_x(g_settings.screen_StartX_a_0), corner_y(g_settings.screen_StartY_a_0),
		  sat_display(g_settings.infobar_sat_display), infobar_progressbar(g_settings.infobar_progressbar),
		  mode_clock(g_settings.mode_clock), clock_size(g_settings.infoClockFontSize),
		  clock_seconds(g_settings.infoClockSeconds), clock_background(g_settings.infoClockBackground),
		  volume_digits(g_settings.volume_digits), volume_size(g_settings.volume_size),
		  radiotext(g_settings.radiotext_enable) {}

	~KeptOsdSettings()
	{
		setSettingsText(g_settings.font_file, font_file);
		setSettingsText(g_settings.font_file_monospace, font_mono);
		g_settings.font_scaling_x = scaling_x;
		g_settings.font_scaling_y = scaling_y;
		g_settings.theme = theme;
		g_settings.screen_preset = preset;
		g_settings.screen_StartX_a_0 = corner_x;
		g_settings.screen_StartY_a_0 = corner_y;
		g_settings.infobar_sat_display = sat_display;
		g_settings.infobar_progressbar = infobar_progressbar;
		g_settings.mode_clock = mode_clock;
		g_settings.infoClockFontSize = clock_size;
		g_settings.infoClockSeconds = clock_seconds;
		g_settings.infoClockBackground = clock_background;
		g_settings.volume_digits = volume_digits;
		g_settings.volume_size = volume_size;
		g_settings.radiotext_enable = radiotext;
	}
};

/* What an OSD case runs against: the seams the framebuffer phase has, its fake
   drawing objects among them, nothing sent yet, and the members it writes put
   back afterwards. */
struct OsdBox
{
	KeptOsdSettings kept;
	PhaseEnvironment env;
	FakeOsdOutput  &out;

	explicit OsdBox(ApplyPhase phase = ApplyPhase::Framebuffer)
		: env(phase), out(env.fake<FakeOsdOutput>("osdout")) { resetSentOsd(); }
	~OsdBox() { resetSentOsd(); }

	void forget() { out.forget(); }
};

bool osdSave() { return true; }

std::string number(int v)
{
	char text[16];
	snprintf(text, sizeof(text), "%d", v);
	return text;
}

} // namespace

TEST_CASE("the font settings are one group that runs after the framebuffer", "[apply][osd]")
{
	Fresh fresh;
	registerApplyGroups();

	const char *const keys[] = { "font_file", "font_file_monospace", "font_scaling_x", "font_scaling_y" };
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		INFO(keys[i]);
		REQUIRE(groupOf(keys[i]) == &kFontsApplyGroup);
	}
	REQUIRE(kFontsApplyGroup.phase == ApplyPhase::Framebuffer);
}

TEST_CASE("startup runs the font group once and rebuilds everything", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	registerApplyGroups();

	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	REQUIRE(box.out.fonts.size() == 1);
	REQUIRE(box.out.fonts[0] == FontSetup::All);
}

/* A change of one key rebuilds as much as that key needs and a key whose value is
   what was built asks for nothing: the rebuild replaces every font object a
   screen holds. */
TEST_CASE("a key of the font group rebuilds what it needs and only when it differs", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.forget();

	g_settings.font_scaling_x = g_settings.font_scaling_x == 100 ? 110 : 100;
	REQUIRE(applyKey("font_scaling_x") == Status::Ok);
	REQUIRE(box.out.fonts.size() == 1);
	REQUIRE(box.out.fonts[0] == FontSetup::Scaling);

	// A sibling key with nothing new, and the key again, rebuild nothing.
	box.forget();
	REQUIRE(applyKey("font_scaling_y") == Status::Ok);
	REQUIRE(applyKey("font_scaling_x") == Status::Ok);
	REQUIRE(box.out.calls.empty());

	setSettingsText(g_settings.font_file_monospace, "/mono-other.ttf");
	REQUIRE(applyKey("font_file_monospace") == Status::Ok);
	REQUIRE(box.out.fonts.size() == 1);
	REQUIRE(box.out.fonts[0] == FontSetup::Monospace);

	box.forget();
	setSettingsText(g_settings.font_file, "/face-other.ttf");
	REQUIRE(applyKey("font_file") == Status::Ok);
	REQUIRE(box.out.fonts.size() == 1);
	REQUIRE(box.out.fonts[0] == FontSetup::All);
}

TEST_CASE("the widest rebuild wins when several font keys changed together", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.forget();

	setSettingsText(g_settings.font_file_monospace, "/mono-other.ttf");
	g_settings.font_scaling_y = g_settings.font_scaling_y == 100 ? 110 : 100;
	REQUIRE(applyKey("font_scaling_y") == Status::Ok);
	REQUIRE(box.out.fonts.size() == 1);
	REQUIRE(box.out.fonts[0] == FontSetup::Scaling);

	// The monospace face was covered by that rebuild.
	box.forget();
	REQUIRE(applyKey("font_file_monospace") == Status::Ok);
	REQUIRE(box.out.calls.empty());
}

TEST_CASE("a refused font rebuild is tried again by the next run", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.forget();

	box.out.fonts_answer = Status::Internal;
	g_settings.font_scaling_x = g_settings.font_scaling_x == 100 ? 110 : 100;
	REQUIRE(applyKey("font_scaling_x") == Status::Internal);
	box.out.fonts_answer = Status::Ok;
	box.forget();
	REQUIRE(applyKey("font_scaling_x") == Status::Ok);
	REQUIRE(box.out.count("fonts") == 1);
}

TEST_CASE("fonts something else rebuilt are rebuilt again once marked", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.forget();

	forgetSentOsd(OsdSent::Fonts);
	REQUIRE(applyKey("font_scaling_x") == Status::Ok);
	REQUIRE(box.out.fonts.size() == 1);
	REQUIRE(box.out.fonts[0] == FontSetup::All);

	// Marked once, rebuilt once.
	box.forget();
	REQUIRE(applyKey("font_scaling_x") == Status::Ok);
	REQUIRE(box.out.calls.empty());
}

/* The web path: both scalings in one batch rebuild the fonts once, not once per
   key, and nothing is asked of anybody. */
TEST_CASE("a web batch writing both font scalings runs the font group once", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, osdSave);

	g_settings.font_scaling_x = 105;
	g_settings.font_scaling_y = 105;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.forget();

	REQUIRE(settings::set("font_scaling_x", number(120)).ok());
	REQUIRE(settings::set("font_scaling_y", number(95)).ok());
	REQUIRE(box.out.calls.empty());
	const size_t posted = sink.posted.size();
	applyPendingSettings();

	REQUIRE(box.out.fonts.size() == 1);
	REQUIRE(box.out.fonts[0] == FontSetup::Scaling);
	REQUIRE(g_settings.font_scaling_x == 120);
	REQUIRE(g_settings.font_scaling_y == 95);
	REQUIRE(events.sent.empty());
	for (size_t i = posted; i < sink.posted.size(); ++i)
		CHECK(sink.posted[i].first == NeutrinoMessages::EVT_SETTINGS_WRITTEN);

	installRealSettingsSource(NULL, NULL);
}

TEST_CASE("the theme colours are one group that runs after the framebuffer, next to the fonts", "[apply][osd]")
{
	Fresh fresh;
	registerApplyGroups();

	const char *const keys[] = { "theme.menu_Head", "theme.menu_Content_Selected_Text", "theme.infobar_casystem",
				     "theme.shadow", "theme.clock_Digit", "theme.progressbar_active" };
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		INFO(keys[i]);
		REQUIRE(groupOf(keys[i]) == &kPaletteApplyGroup);
	}
	REQUIRE(kPaletteApplyGroup.phase == ApplyPhase::Framebuffer);

	// The gradients are read where the screens draw and are no group's.
	REQUIRE(groupOf("menu_Head_gradient") == NULL);

	// After the fonts, as startup filled them in that order.
	const std::vector<const ApplyGroup *> all = applyGroups();
	size_t fonts = all.size(), palette = all.size();
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i] == &kFontsApplyGroup)
			fonts = i;
		if (all[i] == &kPaletteApplyGroup)
			palette = i;
	}
	REQUIRE(fonts < palette);
}

TEST_CASE("startup fills the palette once, after the fonts were built", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	registerApplyGroups();

	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	REQUIRE(box.out.count("fonts") == 1);
	REQUIRE(box.out.count("palette") == 1);

	// The groups of the phase run in the order they were registered.
	size_t fonts_at = box.out.calls.size(), palette_at = box.out.calls.size();
	for (size_t i = 0; i < box.out.calls.size(); ++i)
	{
		if (box.out.calls[i] == "fonts")
			fonts_at = i;
		if (box.out.calls[i] == "palette")
			palette_at = i;
	}
	REQUIRE(fonts_at < palette_at);
}

TEST_CASE("a change of one colour fills the palette once", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.forget();

	g_settings.theme.shadow_red = g_settings.theme.shadow_red == 10 ? 20 : 10;
	REQUIRE(applyKey("theme.shadow") == Status::Ok);
	REQUIRE(box.out.calls.size() == 1);
	REQUIRE(box.out.calls[0] == "palette");
}

TEST_CASE("the preset and every corner are one group that runs after the framebuffer", "[apply][osd]")
{
	Fresh fresh;
	registerApplyGroups();

	const char *const keys[] = { "screen_preset", "screen_StartX_a_0", "screen_EndY_a_0", "screen_StartX_a_1",
				     "screen_EndX_b_0", "screen_EndY_b_1" };
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		INFO(keys[i]);
		REQUIRE(groupOf(keys[i]) == &kScreenGeometryApplyGroup);
	}
	REQUIRE(kScreenGeometryApplyGroup.phase == ApplyPhase::Framebuffer);
}

TEST_CASE("startup takes the slot of the preset once", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	registerApplyGroups();

	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	REQUIRE(box.out.count("geometry") == 1);
}

TEST_CASE("a change of the preset takes the slot once", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.forget();

	g_settings.screen_preset = g_settings.screen_preset == 0 ? 1 : 0;
	REQUIRE(applyKey("screen_preset") == Status::Ok);
	REQUIRE(box.out.calls.size() == 1);
	REQUIRE(box.out.calls[0] == "geometry");
}

/* A corner written alone from the web is in effect at once: the rows are no longer ones that
   wait for a restart, which the layer would leave unapplied. */
TEST_CASE("a web write of one corner alone runs the geometry group", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, osdSave);

	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.forget();

	REQUIRE(settings::set("screen_StartX_a_0", number(g_settings.screen_StartX_a_0 == 12 ? 14 : 12)).ok());
	REQUIRE(box.out.calls.empty());
	applyPendingSettings();
	REQUIRE(box.out.count("geometry") == 1);

	installRealSettingsSource(NULL, NULL);
}

/* A corner written from the web is in effect at once: the rows are no longer ones that
   wait for a restart, and two corners and the preset in one batch take the slot once. */
TEST_CASE("a web batch writing the preset and two corners runs the geometry group once", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, osdSave);

	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.forget();

	g_settings.screen_preset = 0;
	REQUIRE(settings::set("screen_preset", number(1)).ok());
	REQUIRE(settings::set("screen_StartX_a_0", number(12)).ok());
	REQUIRE(settings::set("screen_StartY_a_0", number(14)).ok());
	REQUIRE(box.out.calls.empty());
	applyPendingSettings();

	REQUIRE(box.out.calls.size() == 1);
	REQUIRE(box.out.calls[0] == "geometry");
	REQUIRE(g_settings.screen_StartX_a_0 == 12);
	REQUIRE(g_settings.screen_StartY_a_0 == 14);

	installRealSettingsSource(NULL, NULL);
}

// A menu that moves the preset and its corners in one call runs the geometry once, on all of them.
TEST_CASE("a batch on the loop writing the preset and two corners runs the geometry group once", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, osdSave);

	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.forget();

	g_settings.screen_preset = 0;
	std::vector<std::pair<std::string, std::string> > members;
	members.push_back(std::make_pair(std::string("screen_preset"), number(1)));
	members.push_back(std::make_pair(std::string("screen_StartX_a_0"), number(12)));
	members.push_back(std::make_pair(std::string("screen_StartY_a_0"), number(14)));
	settings::Refusals failed;
	settings::writeBatch(members, failed, true);

	REQUIRE(failed.empty());
	REQUIRE(box.out.count("geometry") == 1);
	REQUIRE(g_settings.screen_preset == 1);
	REQUIRE(g_settings.screen_StartX_a_0 == 12);
	REQUIRE(g_settings.screen_StartY_a_0 == 14);

	installRealSettingsSource(NULL, NULL);
}

TEST_CASE("the logo of the event is one group that runs after the framebuffer", "[apply][osd]")
{
	Fresh fresh;
	registerApplyGroups();

	REQUIRE(groupOf("channellist_show_eventlogo") == &kEventLogoApplyGroup);
	REQUIRE(kEventLogoApplyGroup.phase == ApplyPhase::Framebuffer);

	// Whether the list shows the logo of the channel is read where the list is drawn.
	REQUIRE(groupOf("channellist_show_channellogo") == NULL);
}

TEST_CASE("startup and a change of the event logo each reset the display skin once", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	registerApplyGroups();

	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	REQUIRE(box.out.count("lcd4lparse") == 1);

	box.forget();
	REQUIRE(applyKey("channellist_show_eventlogo") == Status::Ok);
	REQUIRE(box.out.calls.size() == 2);
	REQUIRE(box.out.calls[0] == "lcd4lparse");
	REQUIRE(box.out.calls[1] == "channellist");
}

TEST_CASE("a web write of the event logo resets the display skin once", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, osdSave);

	const int kept_logo = g_settings.channellist_show_channellogo, kept_event = g_settings.channellist_show_eventlogo;
	g_settings.channellist_show_channellogo = 1;
	g_settings.channellist_show_eventlogo = 0;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.forget();

	REQUIRE(settings::set("channellist_show_eventlogo", number(1)).ok());
	REQUIRE(box.out.calls.empty());
	applyPendingSettings();
	REQUIRE(box.out.calls.size() == 2);
	REQUIRE(box.out.calls[0] == "lcd4lparse");
	REQUIRE(box.out.calls[1] == "channellist");

	g_settings.channellist_show_channellogo = kept_logo;
	g_settings.channellist_show_eventlogo = kept_event;
	installRealSettingsSource(NULL, NULL);
}

TEST_CASE("what the infobar lays out its modules by is one group that runs when the infobar exists", "[apply][osd]")
{
	Fresh fresh;
	registerApplyGroups();

	const char *const keys[] = { "infobar_show_channellogo", "infobar_sat_display", "infobar_casystem_display",
				     "infobar_casystem_frame", "infobar_show_tuner", "infobar_progressbar" };
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		INFO(keys[i]);
		REQUIRE(groupOf(keys[i]) == &kInfoViewerApplyGroup);
	}
	REQUIRE(kInfoViewerApplyGroup.phase == ApplyPhase::Network);

	// What the infobar shows without laying anything out differently is read where it is drawn.
	REQUIRE(groupOf("infobar_show_res") == NULL);
	REQUIRE(groupOf("infobar_weather") == NULL);
}

TEST_CASE("startup and a change of an infobar key each reset the infobar once", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box(ApplyPhase::Network);
	registerApplyGroups();

	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	REQUIRE(box.out.count("infoviewer") == 1);

	box.forget();
	REQUIRE(applyKey("infobar_progressbar") == Status::Ok);
	REQUIRE(box.out.calls.size() == 1);
	REQUIRE(box.out.calls[0] == "infoviewer");
}

TEST_CASE("a web batch writing two infobar keys resets the infobar once", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box(ApplyPhase::Network);
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, osdSave);

	g_settings.infobar_sat_display = 1;
	g_settings.infobar_progressbar = 0;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	box.forget();

	REQUIRE(settings::set("infobar_sat_display", number(0)).ok());
	REQUIRE(settings::set("infobar_progressbar", number(1)).ok());
	REQUIRE(box.out.calls.empty());
	applyPendingSettings();

	REQUIRE(box.out.calls.size() == 1);
	REQUIRE(box.out.calls[0] == "infoviewer");
	REQUIRE(g_settings.infobar_sat_display == 0);
	REQUIRE(g_settings.infobar_progressbar == 1);

	installRealSettingsSource(NULL, NULL);
}

TEST_CASE("the clock and the volume bar are two groups that run when the infobar exists", "[apply][osd]")
{
	Fresh fresh;
	registerApplyGroups();

	const char *const clock[] = { "mode_clock", "infoClockFontSize", "infoClockSeconds", "infoClockBackground" };
	for (size_t i = 0; i < sizeof(clock) / sizeof(clock[0]); ++i)
	{
		INFO(clock[i]);
		REQUIRE(groupOf(clock[i]) == &kInfoClockApplyGroup);
	}
	REQUIRE(groupOf("volume_digits") == &kVolumeBarApplyGroup);
	REQUIRE(groupOf("volume_size") == &kVolumeBarApplyGroup);
	REQUIRE(kInfoClockApplyGroup.phase == ApplyPhase::Network);
	REQUIRE(kVolumeBarApplyGroup.phase == ApplyPhase::Network);

	// Where the volume bar sits and whether the mute icon shows at nought is read where they are drawn.
	REQUIRE(groupOf("volume_pos") == NULL);
	REQUIRE(groupOf("show_mute_icon") == NULL);
}

TEST_CASE("startup draws the clock and lays the volume bar out once each", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box(ApplyPhase::Network);
	registerApplyGroups();

	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	REQUIRE(box.out.count("clock") == 1);
	// The clock's size and the volume bar's own settings each lay the bar out.
	REQUIRE(box.out.count("volume") == 2);
	REQUIRE(box.out.count("mute") == 1);
}

/* A new size moves the volume bar and the mute icon, which sit around the clock; the other
   three settings only redraw it. */
TEST_CASE("a change of one clock key redraws the clock and only a new size moves the volume bar", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box(ApplyPhase::Network);
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	box.forget();

	g_settings.infoClockSeconds = g_settings.infoClockSeconds ? 0 : 1;
	REQUIRE(applyKey("infoClockSeconds") == Status::Ok);
	REQUIRE(box.out.calls.size() == 1);
	REQUIRE(box.out.calls[0] == "clock");

	// Nothing changed since, nothing drawn.
	box.forget();
	REQUIRE(applyKey("infoClockSeconds") == Status::Ok);
	REQUIRE(applyKey("mode_clock") == Status::Ok);
	REQUIRE(box.out.calls.empty());

	g_settings.infoClockFontSize = g_settings.infoClockFontSize == 40 ? 50 : 40;
	REQUIRE(applyKey("infoClockFontSize") == Status::Ok);
	REQUIRE(box.out.calls.size() == 3);
	REQUIRE(box.out.calls[0] == "volume");
	REQUIRE(box.out.calls[1] == "mute");
	REQUIRE(box.out.calls[2] == "clock");
}

TEST_CASE("a clock another writer switched is drawn again once marked, and a refused redraw is retried", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box(ApplyPhase::Network);
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	box.forget();

	// The hotkey flips the flag past the group.
	g_settings.mode_clock = g_settings.mode_clock ? 0 : 1;
	forgetSentOsd(OsdSent::InfoClock);
	REQUIRE(applyKey("mode_clock") == Status::Ok);
	REQUIRE(box.out.count("clock") == 1);
	box.forget();
	REQUIRE(applyKey("mode_clock") == Status::Ok);
	REQUIRE(box.out.calls.empty());

	box.out.clock_answer = Status::Internal;
	g_settings.infoClockBackground = g_settings.infoClockBackground ? 0 : 1;
	REQUIRE(applyKey("infoClockBackground") == Status::Internal);
	box.out.clock_answer = Status::Ok;
	box.forget();
	REQUIRE(applyKey("infoClockBackground") == Status::Ok);
	REQUIRE(box.out.count("clock") == 1);
}

TEST_CASE("a change of the volume bar's digits lays it out once", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box(ApplyPhase::Network);
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	box.forget();

	g_settings.volume_digits = g_settings.volume_digits ? 0 : 1;
	REQUIRE(applyKey("volume_digits") == Status::Ok);
	REQUIRE(box.out.calls.size() == 1);
	REQUIRE(box.out.calls[0] == "volume");
}

TEST_CASE("a web batch writing clock and volume keys draws the clock once and lays the bar out once", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box(ApplyPhase::Network);
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, osdSave);

	g_settings.infoClockSeconds = 0;
	g_settings.infoClockBackground = 0;
	g_settings.volume_digits = 0;
	g_settings.volume_size = 26;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	box.forget();

	REQUIRE(settings::set("infoClockSeconds", number(1)).ok());
	REQUIRE(settings::set("infoClockBackground", number(1)).ok());
	REQUIRE(settings::set("volume_digits", number(1)).ok());
	REQUIRE(settings::set("volume_size", number(30)).ok());
	REQUIRE(box.out.calls.empty());
	applyPendingSettings();

	REQUIRE(box.out.count("clock") == 1);
	REQUIRE(box.out.count("volume") == 1);
	REQUIRE(box.out.count("mute") == 0);

	installRealSettingsSource(NULL, NULL);
}

TEST_CASE("the radio text and the size of the screen are groups of their own", "[apply][osd]")
{
	Fresh fresh;
	registerApplyGroups();

	REQUIRE(groupOf("radiotext_enable") == &kRadioTextApplyGroup);
	REQUIRE(kRadioTextApplyGroup.phase == ApplyPhase::Network);
	REQUIRE(groupOf("osd_resolution") == &kOsdResolutionApplyGroup);
	REQUIRE(kOsdResolutionApplyGroup.phase == ApplyPhase::Framebuffer);
}

TEST_CASE("startup and a change of the radio text each run the decoder group once", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box(ApplyPhase::Network);
	registerApplyGroups();

	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	REQUIRE(box.out.count("radiotext") == 1);

	box.forget();
	g_settings.radiotext_enable = g_settings.radiotext_enable ? 0 : 1;
	REQUIRE(applyKey("radiotext_enable") == Status::Ok);
	REQUIRE(box.out.calls.size() == 1);
	REQUIRE(box.out.calls[0] == "radiotext");
}

TEST_CASE("a web write of the radio text runs the decoder group once", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box(ApplyPhase::Network);
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, osdSave);

	g_settings.radiotext_enable = 1;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	box.forget();

	REQUIRE(settings::set("radiotext_enable", number(0)).ok());
	REQUIRE(box.out.calls.empty());
	applyPendingSettings();
	REQUIRE(box.out.calls.size() == 1);
	REQUIRE(box.out.calls[0] == "radiotext");

	installRealSettingsSource(NULL, NULL);
}

TEST_CASE("startup and a change of the size of the screen each empty the icon cache once", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	registerApplyGroups();

	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	REQUIRE(box.out.count("iconcache") == 1);

	box.forget();
	REQUIRE(applyKey("osd_resolution") == Status::Ok);
	REQUIRE(box.out.calls.size() == 1);
	REQUIRE(box.out.calls[0] == "iconcache");
}

namespace
{

const std::string *batchValue(const settings::BatchOverlay &b, const char *key)
{
	for (size_t i = 0; i < b.values.size(); ++i)
		if (b.values[i].first == key)
			return &b.values[i].second;
	return NULL;
}

settings::BatchOverlay batchOfOne(const char *key, const char *value)
{
	settings::BatchOverlay b;
	b.values.push_back(std::make_pair(std::string(key), std::string(value)));
	return b;
}

} // namespace

/* The picture fix of the SCART output moves the drawn area and the fonts with it. The coupling
   is what the screen's flag item and a write from outside both go through. */
TEST_CASE("switching the SCART picture fix on takes the second preset, its corners and unscaled fonts", "[couple][osd]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	// The settle asks whether the box has a row before it judges a member of it.
	FakeSystemSource box;
	InstalledSystemSource system(&box);
	settings::Refusals refused;

	settings::BatchOverlay on = batchOfOne("flag_scart_osd_fix", "1");
	settings::settleBatch(on, refused);
	REQUIRE(refused.empty());
	REQUIRE(batchValue(on, "screen_preset") != NULL);
	CHECK(*batchValue(on, "screen_preset") == "1");
	CHECK(*batchValue(on, "screen_StartX_b_0") == "30");
	CHECK(*batchValue(on, "screen_StartY_b_0") == "45");
	CHECK(*batchValue(on, "screen_EndX_b_0") == "690");
	CHECK(*batchValue(on, "screen_EndY_b_0") == "535");
	CHECK(*batchValue(on, "font_scaling_x") == "100");
	CHECK(*batchValue(on, "font_scaling_y") == "100");
}

TEST_CASE("switching the SCART picture fix off puts the corners and fonts back and leaves the preset", "[couple][osd]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	// The flag is the default, so the settle asks whether the box has the row before it drops it.
	FakeSystemSource box;
	InstalledSystemSource system(&box);
	settings::Refusals refused;

	settings::BatchOverlay off = batchOfOne("flag_scart_osd_fix", "0");
	settings::settleBatch(off, refused);
	REQUIRE(refused.empty());
	CHECK(batchValue(off, "screen_preset") == NULL);
	CHECK(*batchValue(off, "screen_StartX_b_0") == "22");
	CHECK(*batchValue(off, "screen_StartY_b_0") == "12");
	CHECK(*batchValue(off, "screen_EndX_b_0") == "1236");
	CHECK(*batchValue(off, "screen_EndY_b_0") == "695");
	CHECK(*batchValue(off, "font_scaling_x") == "105");
	CHECK(*batchValue(off, "font_scaling_y") == "105");

	// The values it puts back are the rows' own defaults, so a reset and a switch agree.
	const char *const keys[] = { "screen_StartX_b_0", "screen_StartY_b_0", "screen_EndX_b_0", "screen_EndY_b_0",
				     "font_scaling_x", "font_scaling_y" };
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		INFO(keys[i]);
		Result<Descriptor> d = settings::describe(keys[i]);
		REQUIRE(d.ok());
		CHECK(std::to_string(defaultInt(d.value())) == *batchValue(off, keys[i]));
	}
}

TEST_CASE("a value that contradicts what the SCART fix implies is refused with the flag", "[couple][osd]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	// The settle asks whether the box has a row before it judges a member of it.
	FakeSystemSource box;
	InstalledSystemSource system(&box);
	settings::Refusals refused;

	settings::BatchOverlay both;
	both.values.push_back(std::make_pair(std::string("flag_scart_osd_fix"), std::string("1")));
	both.values.push_back(std::make_pair(std::string("font_scaling_x"), std::string("105")));
	settings::settleBatch(both, refused);
	CHECK(!refused.empty());
	CHECK(batchValue(both, "font_scaling_x") == NULL);
	CHECK(batchValue(both, "flag_scart_osd_fix") == NULL);
}

/* The icons flip with the setting; a screen writing it reaches the icons only through the group. */
TEST_CASE("a change of the mode icons setting starts or stops the icons", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box(ApplyPhase::Network);
	registerApplyGroups();
	REQUIRE(groupOf("mode_icons") == &kInfoIconsApplyGroup);
	REQUIRE(kInfoIconsApplyGroup.phase == ApplyPhase::Network);
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	box.forget();

	REQUIRE(applyKey("mode_icons") == Status::Ok);
	REQUIRE(box.out.count("infoicons") == 1);
	REQUIRE(box.out.calls.size() == 1);

	// The skin and the background are read where the icons are drawn.
	box.forget();
	REQUIRE(groupOf("mode_icons_skin") == NULL);
	REQUIRE(groupOf("mode_icons_background") == NULL);
}

/* The list's header and mini TV were made again when the screen menu closed, so a write from
   anywhere else left them as they were. */
TEST_CASE("a change of a channel list setting makes the list's modules again, once for a batch", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box(ApplyPhase::Network);
	registerApplyGroups();
	const char *const keys[] = { "channellist_additional", "channellist_epgtext_alignment", "channellist_show_res_icon",
				     "progressbar_design_channellist", "channellist_show_infobox", "channellist_foot",
				     "channellist_show_numbers" };
	std::vector<std::string> all;
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		INFO(keys[i]);
		REQUIRE(groupOf(keys[i]) == &kChannelListApplyGroup);
		all.push_back(keys[i]);
	}
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	box.forget();

	REQUIRE(applyBatch(all) == Status::Ok);
	REQUIRE(box.out.count("channellist") == 1);
	REQUIRE(box.out.calls.size() == 1);
}

/* A settings file loaded over the running ones replaces members without a write, so
   what changed is found by comparing readings, and each group holding a changed key
   is asked once. */
TEST_CASE("a loaded file puts in force the groups whose keys changed and no other", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, osdSave);
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.forget();

	const settings::Snapshot before = settings::snapshot();

	// Nothing differs: nobody is asked.
	REQUIRE(settings::applyChangedSince(before) == Status::Ok);
	REQUIRE(box.out.calls.empty());

	g_settings.font_scaling_x = g_settings.font_scaling_x == 100 ? 110 : 100;
	g_settings.theme.shadow_red = g_settings.theme.shadow_red == 10 ? 20 : 10;
	g_settings.theme.shadow_green = g_settings.theme.shadow_green == 10 ? 20 : 10;
	REQUIRE(settings::applyChangedSince(before) == Status::Ok);

	REQUIRE(box.out.fonts.size() == 1);
	REQUIRE(box.out.fonts[0] == FontSetup::Scaling);
	REQUIRE(box.out.count("palette") == 1);
	REQUIRE(box.out.calls.size() == 2);

	installRealSettingsSource(NULL, NULL);
}

/* A reset of the settings replaces them as a loaded file does. The font group rebuilds for the
   defaults and knows it did, so a change back to the scaling of before is a change again. */
TEST_CASE("a reset to the defaults leaves the font group knowing what it built", "[apply][osd]")
{
	Fresh fresh;
	OsdBox box;
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, osdSave);
	g_settings.font_scaling_x = 110;
	g_settings.font_scaling_y = 110;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Framebuffer) == Status::Ok);
	box.forget();

	REQUIRE(settings::applyReplaced([]() { g_settings.font_scaling_x = 100; g_settings.font_scaling_y = 100; }) == Status::Ok);
	REQUIRE(box.out.fonts.size() == 1);

	box.forget();
	g_settings.font_scaling_x = 110;
	g_settings.font_scaling_y = 110;
	REQUIRE(applyKey("font_scaling_x") == Status::Ok);
	REQUIRE(box.out.fonts.size() == 1);

	installRealSettingsSource(NULL, NULL);
}

TEST_CASE("the keys a loaded file changed are named by their reading", "[apply][osd]")
{
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, osdSave);
	KeptOsdSettings kept;

	const settings::Snapshot before = settings::snapshot();
	REQUIRE(before.count("font_scaling_x") == 1);
	g_settings.font_scaling_y = g_settings.font_scaling_y == 100 ? 110 : 100;

	const std::vector<std::string> changed = settings::changedSince(before);
	REQUIRE(changed.size() == 1);
	REQUIRE(changed[0] == "font_scaling_y");

	installRealSettingsSource(NULL, NULL);
}
