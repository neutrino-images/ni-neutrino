/*
 * settingstable_theme.cpp - the colours, gradients and display layout the themes carry, one row to each
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

#include "settingstable.h"
#include "settingsfield.h"

#include "coreapi/box/apply_glcd.h"

namespace coreapi
{

namespace
{

/* The theme of the screen and the theme of the front display. Both are structs
   of their own inside the settings, and each member of them is a setting under
   the key the theme files and the settings file give it, so each is a row.

   The bytes a colour is made of are not here: a colour is one setting and has one
   row, written with the colour rows, so a row for a byte would be a second way
   to write the same setting.

   The defaults are the program's own for a box with no theme file, which is what
   the load falls back to. */

/* The gradients, in the order the colour screen lists them. The numbers are the
   screen's own enumeration, which this layer cannot include: its header is the
   one place they are stated, and a changed one moves every theme file. */
constexpr EnumValue kGradient[] =
{
	option(0).label("options.off"),
	option(3).label("color.gradient_a2b"),
	option(4).label("color.gradient_b2a"),
	option(1).label("color.gradient_l2d"),
	option(2).label("color.gradient_d2l"),
	option(5).label("color.gradient_ldl"),
	option(6).label("color.gradient_dld")
};

// Horizontal and vertical, which the framebuffer numbers in that order.
constexpr EnumValue kGradientDirection[] =
{
	option(0).label("color.gradient_mode_direction_hor"),
	option(1).label("color.gradient_mode_direction_ver")
};

constexpr EnumValue kColoredEvents[] =
{
	option(0).label("miscsettings.colored_events_0"),
	option(1).label("miscsettings.colored_events_1"),
	option(2).label("miscsettings.colored_events_2")
};

/* The progress bar designs as the progress bar numbers them: nought is the
   matrix, and the two below it are the single colour and, lowest, no bar at all.
   The general design leaves out the last, which only the channel list offers. */
constexpr EnumValue kProgressDesign[] =
{
	option(-1).label("miscsettings.progressbar_design_4"),
	option(0).label("miscsettings.progressbar_design_0"),
	option(1).label("miscsettings.progressbar_design_1"),
	option(2).label("miscsettings.progressbar_design_2"),
	option(3).label("miscsettings.progressbar_design_3")
};

constexpr EnumValue kProgressDesignChannellist[] =
{
	option(-2).label("options.off"),
	option(-1).label("miscsettings.progressbar_design_4"),
	option(0).label("miscsettings.progressbar_design_0"),
	option(1).label("miscsettings.progressbar_design_1"),
	option(2).label("miscsettings.progressbar_design_2"),
	option(3).label("miscsettings.progressbar_design_3")
};

constexpr EnumValue kTimescaleInvert[] =
{
	option(0).label("miscsettings.progressbar_timescale_red_green"),
	option(1).label("miscsettings.progressbar_timescale_green_red")
};

constexpr EnumValue kRoundedCorners[] =
{
	option(0).label("extra.rounded_corners_off"),
	option(1).label("extra.rounded_corners_on")
};

constexpr Descriptor kTheme[] =
{
	enumRow("menu_Head_gradient")
		.section("osd")
		.label("color.gradient_head")
		.menuLabel("color.gradient")
		.hint("menu.hint_color_gradient")
		.defaultValue(0)
		.values(kGradient)
		.field(COREAPI_THEME_FIELD(menu_Head_gradient)),
	enumRow("menu_Head_gradient_direction")
		.section("osd")
		.label("color.gradient_direction_head")
		.menuLabel("color.gradient_mode_direction")
		.hint("menu.hint_color_gradient_direction")
		.defaultValue(1)
		.values(kGradientDirection)
		.field(COREAPI_THEME_FIELD(menu_Head_gradient_direction)),
	enumRow("menu_SubHead_gradient")
		.section("osd")
		.label("color.gradient_subhead")
		.menuLabel("color.gradient")
		.hint("menu.hint_color_gradient")
		.defaultValue(0)
		.values(kGradient)
		.field(COREAPI_THEME_FIELD(menu_SubHead_gradient)),
	enumRow("menu_SubHead_gradient_direction")
		.section("osd")
		.label("color.gradient_direction_subhead")
		.menuLabel("color.gradient_mode_direction")
		.hint("menu.hint_color_gradient_direction")
		.defaultValue(1)
		.values(kGradientDirection)
		.field(COREAPI_THEME_FIELD(menu_SubHead_gradient_direction)),
	boolRow("menu_Separator_gradient_enable")
		.section("osd")
		.label("color.gradient_separator_enable")
		.hint("menu.hint_color_gradient_separator_enable")
		.defaultValue(0)
		.field(COREAPI_THEME_FIELD(menu_Separator_gradient_enable)),
	enumRow("menu_Foot_gradient")
		.section("osd")
		.defaultValue(0)
		.values(kGradient)
		.field(COREAPI_THEME_FIELD(menu_Foot_gradient)),
	enumRow("menu_Foot_gradient_direction")
		.section("osd")
		.defaultValue(1)
		.values(kGradientDirection)
		.field(COREAPI_THEME_FIELD(menu_Foot_gradient_direction)),
	enumRow("menu_Hint_gradient")
		.section("osd")
		.label("color.gradient_hint")
		.menuLabel("color.gradient")
		.hint("menu.hint_color_gradient")
		.defaultValue(0)
		.values(kGradient)
		.field(COREAPI_THEME_FIELD(menu_Hint_gradient)),
	enumRow("menu_Hint_gradient_direction")
		.section("osd")
		.label("color.gradient_direction_hint")
		.menuLabel("color.gradient_mode_direction")
		.hint("menu.hint_color_gradient_direction")
		.defaultValue(1)
		.values(kGradientDirection)
		.field(COREAPI_THEME_FIELD(menu_Hint_gradient_direction)),
	enumRow("infobar_gradient_top")
		.section("osd")
		.label("miscsettings.infobar_gradient_top")
		.hint("menu.hint_color_gradient")
		.defaultValue(0)
		.values(kGradient)
		.field(COREAPI_THEME_FIELD(infobar_gradient_top)),
	enumRow("infobar_gradient_top_direction")
		.section("osd")
		.label("color.gradient_direction_infobar_top")
		.menuLabel("color.gradient_mode_direction")
		.hint("menu.hint_color_gradient_direction")
		.defaultValue(1)
		.values(kGradientDirection)
		.field(COREAPI_THEME_FIELD(infobar_gradient_top_direction)),
	enumRow("infobar_gradient_body")
		.section("osd")
		.label("miscsettings.infobar_gradient_body")
		.hint("menu.hint_color_gradient")
		.defaultValue(0)
		.values(kGradient)
		.field(COREAPI_THEME_FIELD(infobar_gradient_body)),
	enumRow("infobar_gradient_body_direction")
		.section("osd")
		.label("color.gradient_direction_infobar_body")
		.menuLabel("color.gradient_mode_direction")
		.hint("menu.hint_color_gradient_direction")
		.defaultValue(1)
		.values(kGradientDirection)
		.field(COREAPI_THEME_FIELD(infobar_gradient_body_direction)),
	enumRow("infobar_gradient_bottom")
		.section("osd")
		.label("miscsettings.infobar_gradient_bottom")
		.hint("menu.hint_color_gradient")
		.defaultValue(0)
		.values(kGradient)
		.field(COREAPI_THEME_FIELD(infobar_gradient_bottom)),
	enumRow("infobar_gradient_bottom_direction")
		.section("osd")
		.label("color.gradient_direction_infobar_bottom")
		.menuLabel("color.gradient_mode_direction")
		.hint("menu.hint_color_gradient_direction")
		.defaultValue(1)
		.values(kGradientDirection)
		.field(COREAPI_THEME_FIELD(infobar_gradient_bottom_direction)),
	enumRow("colored_events_channellist")
		.section("osd")
		.label("miscsettings.colored_events_channellist")
		.hint("menu.hint_colored_events")
		.defaultValue(1)
		.values(kColoredEvents)
		.field(COREAPI_THEME_FIELD(colored_events_channellist)),
	enumRow("colored_events_infobar")
		.section("osd")
		.label("miscsettings.colored_events_infobar")
		.hint("menu.hint_colored_events")
		.defaultValue(1)
		.values(kColoredEvents)
		.field(COREAPI_THEME_FIELD(colored_events_infobar)),
	enumRow("progressbar_design")
		.section("osd")
		.label("miscsettings.progressbar_design_long")
		.hint("menu.hint_progressbar_color")
		.defaultValue(-1)
		.values(kProgressDesign)
		.field(COREAPI_THEME_FIELD(progressbar_design)),
	enumRow("progressbar_design_channellist")
		.section("osd")
		.label("channellist.extended")
		.hint("menu.hint_channellist_extended")
		.defaultValue(-1)
		.values(kProgressDesignChannellist)
		.field(COREAPI_THEME_FIELD(progressbar_design_channellist)),
	boolRow("progressbar_gradient")
		.section("osd")
		.label("miscsettings.progressbar_gradient")
		.hint("menu.hint_progressbar_gradient")
		.defaultValue(1)
		.field(COREAPI_THEME_FIELD(progressbar_gradient)),
	intRow("progressbar_timescale_red")
		.section("osd")
		.label("miscsettings.progressbar_timescale_red")
		.hint("menu.hint_progressbar_timescale_red")
		.range(0, 100)
		.defaultValue(0)
		.unit("unit.short.percent")
		.field(COREAPI_THEME_FIELD(progressbar_timescale_red)),
	intRow("progressbar_timescale_green")
		.section("osd")
		.label("miscsettings.progressbar_timescale_green")
		.hint("menu.hint_progressbar_timescale_green")
		.range(0, 100)
		.defaultValue(100)
		.unit("unit.short.percent")
		.field(COREAPI_THEME_FIELD(progressbar_timescale_green)),
	intRow("progressbar_timescale_yellow")
		.section("osd")
		.label("miscsettings.progressbar_timescale_yellow")
		.hint("menu.hint_progressbar_timescale_yellow")
		.range(0, 100)
		.defaultValue(70)
		.unit("unit.short.percent")
		.field(COREAPI_THEME_FIELD(progressbar_timescale_yellow)),
	boolRow("progressbar_timescale_invert")
		.section("osd")
		.label("miscsettings.progressbar_timescale_invert")
		.hint("menu.hint_progressbar_timescale_invert")
		.defaultValue(0)
		.values(kTimescaleInvert)
		.field(COREAPI_THEME_FIELD(progressbar_timescale_invert)),
	boolRow("rounded_corners")
		.section("osd")
		.label("extra.rounded_corners")
		.hint("menu.hint_rounded_corners")
		.defaultValue(0)
		.values(kRoundedCorners)
		.field(COREAPI_THEME_FIELD(rounded_corners)),
	boolRow("message_frame_enable")
		.section("osd")
		.label("message.frame_enable")
		.hint("message.frame_enable_hint")
		.defaultValue(0)
		.field(COREAPI_THEME_FIELD(message_frame_enable)),
};

#ifdef ENABLE_GRAPHLCD

// The alignments the display screens offer for a line of text.
constexpr EnumValue kGlcdAlign[] =
{
	option(0).label("glcd.align_none"),
	option(1).label("glcd.align_left"),
	option(2).label("glcd.align_center"),
	option(3).label("glcd.align_right")
};

// What the standby clock is drawn as, in the order the screen lists them.
constexpr EnumValue kGlcdStandbyClock[] =
{
	option(0).label("options.off"),
	option(1).label("glcd.standby_clock_simple"),
	option(2).label("glcd.standby_clock_led"),
	option(3).label("glcd.standby_clock_lcd"),
	option(4).label("glcd.standby_clock_digital"),
	option(5).label("glcd.standby_clock_analog")
};

/* Where on the panel a line is drawn is bounded by the panel, which the program
   asks the driver for when the screen is built. The constants below are the largest
   panel the display drivers are written for and what the bound is while no panel
   answers; maxNow() names the question to ask, so a row that is read for the
   connected panel stops at its width and height. */
constexpr long kPanelWidth = kGlcdPanelWidthMax;
constexpr long kPanelHeight = kGlcdPanelHeightMax;

// The parts of the display layout that can be switched off, and what that leaves untouched.
constexpr Condition kGlcdLogoOn[] =
{
	when("glcd_logo").isNot(0)
};

constexpr Condition kGlcdDurationOn[] =
{
	when("glcd_duration").isNot(0)
};

constexpr Condition kGlcdStartOn[] =
{
	when("glcd_start").isNot(0)
};

constexpr Condition kGlcdEndOn[] =
{
	when("glcd_end").isNot(0)
};

constexpr Condition kGlcdProgressbarOn[] =
{
	when("glcd_progressbar").isNot(0)
};

constexpr Condition kGlcdTimeOn[] =
{
	when("glcd_time").isNot(0)
};

constexpr Condition kGlcdWeatherOn[] =
{
	when("glcd_weather").isNot(0)
};

constexpr Condition kGlcdStandbyWeatherOn[] =
{
	when("glcd_standby_weather").isNot(0)
};

/* The standby clock's position is meaningful only for the clock that has one,
   and only while the layout is positioned by these numbers at all. */
constexpr Condition kGlcdClockDigital[] =
{
	when("glcd_position_settings").isNot(0),
	when("glcd_standby_clock").is(4)
};

constexpr Condition kGlcdClockSimple[] =
{
	when("glcd_position_settings").isNot(0),
	when("glcd_standby_clock").is(1)
};

constexpr Descriptor kGlcdTheme[] =
{
	textRow("glcd_background_image")
		.section("display")
		.defaultValue("")
		.field(COREAPI_GLCD_THEME_TEXT_FIELD(glcd_background_image)),
	textRow("glcd_font")
		.section("display")
		.defaultValue("")
		.field(COREAPI_GLCD_THEME_TEXT_FIELD(glcd_font)),
	intRow("glcd_channel_percent")
		.section("display")
		.label("glcd.channel_size")
		.range(0, 100)
		.defaultValue(25)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_channel_percent)),
	enumRow("glcd_channel_align")
		.section("display")
		.label("glcd.channel_align")
		.defaultValue(2)
		.values(kGlcdAlign)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_channel_align)),
	intRow("glcd_channel_x_position")
		.section("display")
		.label("glcd.channel_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_channel_x_position)),
	intRow("glcd_channel_y_position")
		.section("display")
		.label("glcd.channel_y_position")
		.range(0, kPanelHeight)
		.maxNow(glcdPanelHeight)
		.defaultValue(60)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_channel_y_position)),
	boolRow("glcd_logo")
		.section("display")
		.label("glcd.logo_show")
		.defaultValue(1)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_logo)),
	intRow("glcd_logo_percent")
		.section("display")
		.label("glcd.logo_size")
		.range(0, 100)
		.defaultValue(25)
		.changeableWhen(kGlcdLogoOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_logo_percent)),
	intRow("glcd_logo_width_percent")
		.section("display")
		.label("glcd.logo_width")
		.range(0, 100)
		.defaultValue(99)
		.changeableWhen(kGlcdLogoOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_logo_width_percent)),
	intRow("glcd_logo_x_position")
		.section("display")
		.label("glcd.logo_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.changeableWhen(kGlcdLogoOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_logo_x_position)),
	intRow("glcd_logo_y_position")
		.section("display")
		.label("glcd.logo_y_position")
		.range(0, kPanelHeight)
		.maxNow(glcdPanelHeight)
		.defaultValue(60)
		.changeableWhen(kGlcdLogoOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_logo_y_position)),
	intRow("glcd_epg_percent")
		.section("display")
		.label("glcd.epg_size")
		.range(0, 100)
		.defaultValue(15)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_epg_percent)),
	enumRow("glcd_epg_align")
		.section("display")
		.label("glcd.epg_align")
		.defaultValue(2)
		.values(kGlcdAlign)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_epg_align)),
	intRow("glcd_epg_x_position")
		.section("display")
		.label("glcd.epg_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_epg_x_position)),
	intRow("glcd_epg_y_position")
		.section("display")
		.label("glcd.epg_y_position")
		.range(0, kPanelHeight)
		.maxNow(glcdPanelHeight)
		.defaultValue(150)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_epg_y_position)),
	boolRow("glcd_start")
		.section("display")
		.label("glcd.start_show")
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_start)),
	intRow("glcd_start_percent")
		.section("display")
		.label("glcd.start_size")
		.range(0, 100)
		.defaultValue(0)
		.changeableWhen(kGlcdStartOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_start_percent)),
	enumRow("glcd_start_align")
		.section("display")
		.label("glcd.start_align")
		.defaultValue(0)
		.values(kGlcdAlign)
		.changeableWhen(kGlcdStartOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_start_align)),
	intRow("glcd_start_x_position")
		.section("display")
		.label("glcd.start_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.changeableWhen(kGlcdStartOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_start_x_position)),
	intRow("glcd_start_y_position")
		.section("display")
		.label("glcd.start_y_position")
		.range(0, kPanelHeight)
		.maxNow(glcdPanelHeight)
		.defaultValue(0)
		.changeableWhen(kGlcdStartOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_start_y_position)),
	boolRow("glcd_end")
		.section("display")
		.label("glcd.end_show")
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_end)),
	intRow("glcd_end_percent")
		.section("display")
		.label("glcd.end_size")
		.range(0, 100)
		.defaultValue(0)
		.changeableWhen(kGlcdEndOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_end_percent)),
	enumRow("glcd_end_align")
		.section("display")
		.label("glcd.end_align")
		.defaultValue(0)
		.values(kGlcdAlign)
		.changeableWhen(kGlcdEndOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_end_align)),
	intRow("glcd_end_x_position")
		.section("display")
		.label("glcd.end_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.changeableWhen(kGlcdEndOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_end_x_position)),
	intRow("glcd_end_y_position")
		.section("display")
		.label("glcd.end_y_position")
		.range(0, kPanelHeight)
		.maxNow(glcdPanelHeight)
		.defaultValue(0)
		.changeableWhen(kGlcdEndOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_end_y_position)),
	boolRow("glcd_duration")
		.section("display")
		.label("glcd.duration_show")
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_duration)),
	intRow("glcd_duration_percent")
		.section("display")
		.label("glcd.duration_size")
		.range(0, 100)
		.defaultValue(0)
		.changeableWhen(kGlcdDurationOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_duration_percent)),
	enumRow("glcd_duration_align")
		.section("display")
		.label("glcd.duration_align")
		.defaultValue(0)
		.values(kGlcdAlign)
		.changeableWhen(kGlcdDurationOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_duration_align)),
	intRow("glcd_duration_x_position")
		.section("display")
		.label("glcd.duration_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.changeableWhen(kGlcdDurationOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_duration_x_position)),
	intRow("glcd_duration_y_position")
		.section("display")
		.label("glcd.duration_y_position")
		.range(0, kPanelHeight)
		.maxNow(glcdPanelHeight)
		.defaultValue(0)
		.changeableWhen(kGlcdDurationOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_duration_y_position)),
	boolRow("glcd_progressbar")
		.section("display")
		.label("glcd.progressbar_show")
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_progressbar)),
	intRow("glcd_progressbar_percent")
		.section("display")
		.label("glcd.progressbar_size")
		.range(0, 100)
		.defaultValue(0)
		.changeableWhen(kGlcdProgressbarOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_progressbar_percent)),
	intRow("glcd_progressbar_width")
		.section("display")
		.label("glcd.progressbar_width")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.changeableWhen(kGlcdProgressbarOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_progressbar_width)),
	intRow("glcd_progressbar_x_position")
		.section("display")
		.label("glcd.progressbar_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.changeableWhen(kGlcdProgressbarOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_progressbar_x_position)),
	intRow("glcd_progressbar_y_position")
		.section("display")
		.label("glcd.progressbar_y_position")
		.range(0, kPanelHeight)
		.maxNow(glcdPanelHeight)
		.defaultValue(0)
		.changeableWhen(kGlcdProgressbarOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_progressbar_y_position)),
	boolRow("glcd_time")
		.section("display")
		.label("glcd.time_show")
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_time)),
	intRow("glcd_time_percent")
		.section("display")
		.label("glcd.time_size")
		.range(0, 100)
		.defaultValue(0)
		.changeableWhen(kGlcdTimeOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_time_percent)),
	enumRow("glcd_time_align")
		.section("display")
		.label("glcd.time_align")
		.defaultValue(0)
		.values(kGlcdAlign)
		.changeableWhen(kGlcdTimeOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_time_align)),
	intRow("glcd_time_x_position")
		.section("display")
		.label("glcd.time_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.changeableWhen(kGlcdTimeOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_time_x_position)),
	intRow("glcd_time_y_position")
		.section("display")
		.label("glcd.time_y_position")
		.range(0, kPanelHeight)
		.maxNow(glcdPanelHeight)
		.defaultValue(0)
		.changeableWhen(kGlcdTimeOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_time_y_position)),
	intRow("glcd_icons_percent")
		.section("display")
		.label("glcd.icon_y_percent")
		.range(0, 100)
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_icons_percent)),
	intRow("glcd_icons_y_position")
		.section("display")
		.label("glcd.icon_y_position")
		.range(0, kPanelHeight)
		.maxNow(glcdPanelHeight)
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_icons_y_position)),
	intRow("glcd_icon_ecm_x_position")
		.section("display")
		.label("glcd.icon_ecm_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_icon_ecm_x_position)),
	intRow("glcd_icon_cam_x_position")
		.section("display")
		.label("glcd.icon_cam_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_icon_cam_x_position)),
	intRow("glcd_icon_txt_x_position")
		.section("display")
		.label("glcd.icon_txt_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_icon_txt_x_position)),
	intRow("glcd_icon_dd_x_position")
		.section("display")
		.label("glcd.icon_dd_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_icon_dd_x_position)),
	intRow("glcd_icon_mute_x_position")
		.section("display")
		.label("glcd.icon_mute_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_icon_mute_x_position)),
	intRow("glcd_icon_timer_x_position")
		.section("display")
		.label("glcd.icon_timer_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_icon_timer_x_position)),
	intRow("glcd_icon_rec_x_position")
		.section("display")
		.label("glcd.icon_rec_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_icon_rec_x_position)),
	intRow("glcd_icon_ts_x_position")
		.section("display")
		.label("glcd.icon_ts_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_icon_ts_x_position)),
	enumRow("glcd_standby_clock")
		.section("display")
		.label("glcd.standby_clock")
		.defaultValue(1)
		.values(kGlcdStandbyClock)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_standby_clock)),
	intRow("glcd_standby_clock_digital_y_position")
		.section("display")
		.label("glcd.clock_digital_y_position")
		.range(0, 500)
		.defaultValue(0)
		.changeableWhen(kGlcdClockDigital)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_standby_clock_digital_y_position)),
	intRow("glcd_standby_clock_simple_size")
		.section("display")
		.label("glcd.clock_simple_size")
		.range(0, 100)
		.defaultValue(0)
		.changeableWhen(kGlcdClockSimple)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_standby_clock_simple_size)),
	intRow("glcd_standby_clock_simple_y_position")
		.section("display")
		.label("glcd.clock_simple_y_position")
		.range(0, 500)
		.defaultValue(0)
		.changeableWhen(kGlcdClockSimple)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_standby_clock_simple_y_position)),
	boolRow("glcd_weather")
		.section("display")
		.label("glcd.weather_show")
		.defaultValue(0)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_weather)),
	intRow("glcd_weather_percent")
		.section("display")
		.label("glcd.weather_percent")
		.range(0, 100)
		.defaultValue(15)
		.changeableWhen(kGlcdWeatherOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_weather_percent)),
	intRow("glcd_weather_curr_temp_x_position")
		.section("display")
		.label("glcd.weather_curr_temp_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.changeableWhen(kGlcdWeatherOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_weather_curr_temp_x_position)),
	intRow("glcd_weather_curr_icon_x_position")
		.section("display")
		.label("glcd.weather_curr_icon_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.changeableWhen(kGlcdWeatherOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_weather_curr_icon_x_position)),
	intRow("glcd_weather_next_temp_x_position")
		.section("display")
		.label("glcd.weather_next_temp_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.changeableWhen(kGlcdWeatherOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_weather_next_temp_x_position)),
	intRow("glcd_weather_next_icon_x_position")
		.section("display")
		.label("glcd.weather_next_icon_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.changeableWhen(kGlcdWeatherOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_weather_next_icon_x_position)),
	intRow("glcd_weather_y_position")
		.section("display")
		.label("glcd.weather_y_position")
		.range(0, kPanelHeight)
		.maxNow(glcdPanelHeight)
		.defaultValue(0)
		.changeableWhen(kGlcdWeatherOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_weather_y_position)),
	boolRow("glcd_standby_weather")
		.section("display")
		.label("glcd.standby_weather")
		.defaultValue(1)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_standby_weather)),
	intRow("glcd_standby_weather_percent")
		.section("display")
		.label("glcd.standby_weather_percent")
		.range(0, 100)
		.defaultValue(40)
		.changeableWhen(kGlcdStandbyWeatherOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_standby_weather_percent)),
	intRow("glcd_standby_weather_curr_temp_x_position")
		.section("display")
		.label("glcd.standby_weather_curr_temp_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.changeableWhen(kGlcdStandbyWeatherOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_standby_weather_curr_temp_x_position)),
	intRow("glcd_standby_weather_curr_icon_x_position")
		.section("display")
		.label("glcd.standby_weather_curr_icon_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.changeableWhen(kGlcdStandbyWeatherOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_standby_weather_curr_icon_x_position)),
	intRow("glcd_standby_weather_next_temp_x_position")
		.section("display")
		.label("glcd.standby_weather_next_temp_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.changeableWhen(kGlcdStandbyWeatherOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_standby_weather_next_temp_x_position)),
	intRow("glcd_standby_weather_next_icon_x_position")
		.section("display")
		.label("glcd.standby_weather_next_icon_x_position")
		.range(0, kPanelWidth)
		.maxNow(glcdPanelWidth)
		.defaultValue(0)
		.changeableWhen(kGlcdStandbyWeatherOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_standby_weather_next_icon_x_position)),
	intRow("glcd_standby_weather_y_position")
		.section("display")
		.label("glcd.standby_weather_y_position")
		.range(0, kPanelHeight)
		.maxNow(glcdPanelHeight)
		.defaultValue(0)
		.changeableWhen(kGlcdStandbyWeatherOn)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_standby_weather_y_position)),
	boolRow("glcd_position_settings")
		.section("display")
		.defaultValue(1)
		.field(COREAPI_GLCD_THEME_FIELD(glcd_position_settings)),
};
#endif

} // anonymous namespace

const Descriptor *settingsTableTheme(size_t &count)
{
#ifdef ENABLE_GRAPHLCD
	/* The two tables are one answer. Joined once on the first call, for the
	   reason the whole table is: two threads arriving together must not both
	   build it. */
	static const std::vector<Descriptor> joined = []() -> std::vector<Descriptor>
	{
		std::vector<Descriptor> v(kTheme, kTheme + sizeof(kTheme) / sizeof(kTheme[0]));
		v.insert(v.end(), kGlcdTheme, kGlcdTheme + sizeof(kGlcdTheme) / sizeof(kGlcdTheme[0]));
		return v;
	}();
	count = joined.size();
	return &joined[0];
#else
	count = sizeof(kTheme) / sizeof(kTheme[0]);
	return kTheme;
#endif
}

} // namespace coreapi
