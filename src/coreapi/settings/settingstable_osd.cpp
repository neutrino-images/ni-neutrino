/*
 * settingstable_osd.cpp - on screen display settings, one row per field
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
#include "predicates.h"

#include "coreapi/base/deps.h"

namespace coreapi
{

namespace
{

/* The OSD section. The values of the position, alignment and mode choices have
   names in src/system/settings.h and the rows use those names, so what a row
   offers is what the code that reads it compares against. Many of the defaults
   are written by the program as a name rather than a number as well, so no
   scan can compare them and each was read out of the header that declares
   it. */

constexpr EnumValue kSubchanPos[] =
{
	option(0).label("settings.pos_top_right"),
	option(1).label("settings.pos_top_left"),
	option(2).label("settings.pos_bottom_left"),
	option(3).label("settings.pos_bottom_right"),
	option(4).label("infoviewer.subchan_infobar")
};

constexpr EnumValue kMenuPos[] =
{
	option(MENU_POS_CENTER).label("settings.pos_center"),
	option(MENU_POS_TOP_LEFT).label("settings.pos_top_left"),
	option(MENU_POS_TOP_RIGHT).label("settings.pos_top_right"),
	option(MENU_POS_BOTTOM_LEFT).label("settings.pos_bottom_left"),
	option(MENU_POS_BOTTOM_RIGHT).label("settings.pos_bottom_right")
};

constexpr EnumValue kChannelLogoPos[] =
{
	option(0).label("options.off"),
	option(CC_LOGO_RIGHT).label("settings.pos_right"),
	option(CC_LOGO_LEFT).label("settings.pos_left"),
	option(CC_LOGO_CENTER).label("settings.pos_center")
};

constexpr EnumValue kInfobarDisp[] =
{
	option(0).label("miscsettings.infobar_disp_0"),
	option(1).label("miscsettings.infobar_disp_1"),
	option(2).label("miscsettings.infobar_disp_2"),
	option(3).label("miscsettings.infobar_disp_3"),
	option(4).label("miscsettings.infobar_disp_4"),
	option(5).label("miscsettings.infobar_disp_5"),
	option(6).label("miscsettings.infobar_disp_6")
};

// Nought is on here and the last value is off, which is why this is a choice
// and not a flag.
constexpr EnumValue kCaSystem[] =
{
	option(0).label("options.on"),
	option(1).label("miscsettings.infobar_casystem_mode"),
	option(2).label("miscsettings.infobar_casystem_mini"),
	option(3).label("options.off")
};

constexpr EnumValue kEcmPos[] =
{
	option(0).label("options.off"),
	option(1).label("settings.pos_top_left"),
	option(2).label("settings.pos_top_center"),
	option(3).label("settings.pos_top_right")
};

constexpr EnumValue kHddStatfs[] =
{
	option(SNeutrinoSettings::HDD_STATFS_OFF).label("options.off"),
	option(SNeutrinoSettings::HDD_STATFS_ALWAYS).label("hdd_statfs_always"),
	option(SNeutrinoSettings::HDD_STATFS_RECORDING).label("hdd_statfs_recording")
};

// Nought is on here as well.
constexpr EnumValue kInfobarShowRes[] =
{
	option(0).label("options.on"),
	option(1).label("miscsettings.infobar_show_res_simple"),
	option(2).label("options.off")
};

constexpr EnumValue kProgressbarInfobarPos[] =
{
	option(SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_DEFAULT).label("miscsettings.progressbar_infobar_position_0"),
	option(SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BELOW_CH_NAME).label("miscsettings.progressbar_infobar_position_1"),
	option(SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BELOW_CH_NAME_SMALL).label("miscsettings.progressbar_infobar_position_2"),
	option(SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BETWEEN_EVENTS).label("miscsettings.progressbar_infobar_position_3")
};

constexpr EnumValue kChannellistAdditional[] =
{
	option(0).label("channellist.additional_off"),
	option(1).label("channellist.additional_on"),
	option(2).label("channellist.additional_on_minitv")
};

constexpr EnumValue kEpgtextAlignment[] =
{
	option(EPGTEXT_ALIGN_LEFT_MIDDLE).label("channellist.epgtext_align_left_middle"),
	option(EPGTEXT_ALIGN_LEFT_BOTTOM).label("channellist.epgtext_align_left_bottom"),
	option(EPGTEXT_ALIGN_RIGHT_MIDDLE).label("channellist.epgtext_align_right_middle"),
	option(EPGTEXT_ALIGN_RIGHT_BOTTOM).label("channellist.epgtext_align_right_bottom")
};

constexpr EnumValue kChannellistFoot[] =
{
	option(0).label("channellist.foot_freq"),
	option(1).label("channellist.foot_next"),
	option(2).label("channellist.foot_off")
};

constexpr EnumValue kVolumePos[] =
{
	option(VOLUMEBAR_POS_TOP_RIGHT).label("settings.pos_top_right"),
	option(VOLUMEBAR_POS_TOP_LEFT).label("settings.pos_top_left"),
	option(VOLUMEBAR_POS_BOTTOM_LEFT).label("settings.pos_bottom_left"),
	option(VOLUMEBAR_POS_BOTTOM_RIGHT).label("settings.pos_bottom_right"),
	option(VOLUMEBAR_POS_TOP_CENTER).label("settings.pos_top_center"),
	option(VOLUMEBAR_POS_BOTTOM_CENTER).label("settings.pos_bottom_center"),
	option(VOLUMEBAR_POS_HIGHER_CENTER).label("settings.pos_higher_center")
};

constexpr EnumValue kScreenPreset[] =
{
	option(PRESET_SCREEN_A).label("osd.preset_screen_a"),
	option(PRESET_SCREEN_B).label("osd.preset_screen_b")
};

// The formats are named by what they are and not by a locale.
constexpr EnumValue kScreenshotFormat[] =
{
	option(FORMAT_PNG).text("PNG"),
	option(FORMAT_JPG).text("JPEG"),
	option(FORMAT_BMP).text("BMP")
};

constexpr EnumValue kScreenshotMode[] =
{
	option(0).label("screenshot.tv"),
	option(1).label("screenshot.osd")
};

// The floors of the screensaver delay and timeout, shown in words.
constexpr EnumValue kScreensaverDelayOff[] =
{
	option(0).label("screensaver.off")
};

constexpr EnumValue kScreensaverTimeoutOff[] =
{
	option(0).label("options.off")
};

constexpr EnumValue kScreensaverMode[] =
{
	option(SCR_MODE_IMAGE).label("screensaver.mode_image"),
	option(SCR_MODE_CLOCK).label("screensaver.mode_clock"),
	option(SCR_MODE_CLOCK_COLOR).label("screensaver.mode_clock_color")
};

// The value of one setting that makes another editable.
constexpr Condition kChannelLogoOn[] =
{
	when("channellist_show_channellogo").isNot(0)
};

// Two and three are the mini bar and off, and neither has a frame to draw.
constexpr Condition kCaSystemDrawn[] =
{
	when("infobar_casystem_display").below(2)
};

constexpr Condition kSysfsHddOn[] =
{
	when("infobar_show_sysfs_hdd").isNot(0)
};

constexpr Condition kInfoboxOn[] =
{
	when("channellist_show_infobox").isNot(0)
};

constexpr Condition kScreensaverOn[] =
{
	when("screensaver_delay").isNot(0)
};

// The image mode is the only one that reads a directory of its own.
constexpr Condition kScreensaverImage[] =
{
	when("screensaver_delay").isNot(0),
	when("screensaver_mode").is(0)
};

constexpr EnumValue kInfoiconsSkin[] =
{
	option(INFOICONS_STATIC).label("infoicons_static"),
	option(INFOICONS_INFOVIEWER).label("infoicons_infoviewer"),
	option(INFOICONS_POPUP).label("infoicons_popup")
};

/* A choice and not a flag: the two values are start and stop rather than on and
   off, and a flag row would carry the values while nothing held the words. */
constexpr EnumValue kInfoiconsMode[] =
{
	option(0).label("options.start"),
	option(1).label("options.stop")
};

// The skin is offered only while the icons are off.
constexpr Condition kIconsOff[] =
{
	when("mode_icons").is(0)
};

// And the icons only where the skin is not the one the infobar draws.
constexpr Condition kSkinNotInfoviewer[] =
{
	when("mode_icons_skin").isNot(INFOICONS_INFOVIEWER)
};

/* The size the box draws its own screen at, asked of and told to the object
   that keeps it. Not the member of the settings named after it: that one is
   filled at load and is not what the save writes, so a value written into it
   would be gone at the next save. */
bool askOsdResolution(long &out)
{
	int mode = 0;
	if (osdResolutionSource().read(mode) != Status::Ok)
		return false;
	out = mode;
	return true;
}

bool tellOsdResolution(long value)
{
	return osdResolutionSource().write((int) value) == Status::Ok;
}

constexpr EnumValue kOsdResolution[] =
{
	option(OSDMODE_720).text("1280x720").availableIf(drawsOsd720),
	option(OSDMODE_1080).text("1920x1080").availableIf(drawsOsd1080)
};

constexpr Descriptor kOsd[] =
{
	boolRow("radiotext_enable")
		.section("osd")
		.label("miscsettings.radiotext")
		.hint("menu.hint_infobar_radiotext")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(radiotext_enable)),
	boolRow("scrambled_message")
		.section("osd")
		.label("extra.scrambled_message")
		.hint("menu.hint_scrambled_message")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(scrambled_message)),
	boolRow("widget_fade")
		.section("osd")
		.label("colormenu.fade")
		.hint("menu.hint_fade")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(widget_fade)),
	/* The two below are one item, edited by a key loop rather than by a
	   chooser, so the range is the one that loop enforces and the label is the
	   one item's. The program falls back to whatever window_size holds, and the
	   value below is what that key falls back to in turn, which is the only
	   part of it a constant can carry. */
	intRow("window_width")
		.section("osd")
		.label("window_size")
		.hint("menu.hint_window_size")
		.range(50, 100)
		.defaultValue(100)
		.field(COREAPI_NUMBER_FIELD(window_width)),
	intRow("window_height")
		.section("osd")
		.label("window_size")
		.hint("menu.hint_window_size")
		.range(50, 100)
		.defaultValue(100)
		.field(COREAPI_NUMBER_FIELD(window_height)),
	enumRow("infobar_subchan_disp_pos")
		.section("osd")
		.label("infoviewer.subchan_disp_pos")
		.hint("menu.hint_subchannel_pos")
		.defaultValue(4)
		.values(kSubchanPos)
		.field(COREAPI_NUMBER_FIELD(infobar_subchan_disp_pos)),

	// fonts
	textRow("font_file")
		.section("osd")
		.label("colormenu.font")
		.hint("menu.hint_font_gui")
		.defaultValue(FONTDIR "/neutrino.ttf")
		.text(kRuleFontFile)
		.field(COREAPI_TEXT_FIELD(font_file)),
	textRow("font_file_monospace")
		.section("osd")
		.label("colormenu.font_ttx")
		.hint("menu.hint_font_ttx")
		.defaultValue(FONTDIR "/tuxtxt.ttf")
		.text(kRuleFontFile)
		.field(COREAPI_TEXT_FIELD(font_file_monospace)),
	// Per cent of the size each font is configured at.
	intRow("font_scaling_x")
		.section("osd")
		.label("fontmenu.scaling_x")
		.hint("fontmenu.scaling_x_hint2")
		.range(50, 200)
		.defaultValue(105)
		.unit("unit.short.percent")
		.field(COREAPI_NUMBER_FIELD(font_scaling_x)),
	intRow("font_scaling_y")
		.section("osd")
		.label("fontmenu.scaling_y")
		.hint("fontmenu.scaling_y_hint2")
		.range(50, 200)
		.defaultValue(105)
		.unit("unit.short.percent")
		.field(COREAPI_NUMBER_FIELD(font_scaling_y)),

	// menus
	enumRow("menu_pos")
		.section("osd")
		.label("settings.menu_pos")
		.hint("menu.hint_menu_pos")
		.defaultValue(0)
		.values(kMenuPos)
		.field(COREAPI_NUMBER_FIELD(menu_pos)),
	boolRow("show_menu_hints")
		.section("osd")
		.label("settings.menu_hints")
		.hint("menu.hint_menu_hints")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(show_menu_hints)),
	boolRow("show_menu_hints_line")
		.section("osd")
		.label("settings.menu_hints_line")
		.hint("menu.hint_menu_hints_line")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(show_menu_hints_line)),

	// channel logos
	textRow("logo_hdd_dir")
		.section("osd")
		.label("miscsettings.infobar_logo_hdd_dir")
		.hint("menu.hint_infobar_logo_dir")
		.defaultValue(TARGET_ROOT "/media/sda1/logos")
		.text(kRuleDirectory)
		.field(COREAPI_TEXT_FIELD(logo_hdd_dir)),
	enumRow("channellist_show_channellogo")
		.section("osd")
		.label("channellist.show_channellogo")
		.hint("menu.hint_channellist_show_channellogo")
		.defaultValue(1)
		.values(kChannelLogoPos)
		.field(COREAPI_NUMBER_FIELD(channellist_show_channellogo)),
	boolRow("channellist_show_eventlogo")
		.section("osd")
		.label("channellist.show_eventlogo")
		.hint("menu.hint_channellist_show_eventlogo")
		.defaultValue(1)
		.changeableWhen(kChannelLogoOn)
		.field(COREAPI_NUMBER_FIELD(channellist_show_eventlogo)),

	// infobar
	boolRow("infobar_show")
		.section("osd")
		.label("miscsettings.infobar_show")
		.hint("menu.hint_infobar_on_epg")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(infobar_show)),
	boolRow("infobar_buttons_usertitle")
		.section("osd")
		.label("miscsettings.infobar_buttons_usertitle")
		.hint("menu.hint_infobar_buttons_usertitle")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(infobar_buttons_usertitle)),
	boolRow("infobar_analogclock")
		.section("osd")
		.label("miscsettings.infobar_analogclock")
		.hint("menu.hint_infobar_analogclock")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(infobar_analogclock)),
	// Meaningful only where the weather is switched on, which is a key another
	// section declares and a condition cannot name yet.
	boolRow("infobar_weather")
		.section("osd")
		.label("miscsettings.infobar_weather")
		.hint("menu.hint_infobar_weather")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(infobar_weather)),
	enumRow("infobar_show_channellogo")
		.section("osd")
		.label("miscsettings.infobar_disp")
		.hint("menu.hint_infobar_logo")
		.defaultValue(5)
		.values(kInfobarDisp)
		.field(COREAPI_NUMBER_FIELD(infobar_show_channellogo)),
	boolRow("infobar_sat_display")
		.section("osd")
		.label("miscsettings.infobar_sat_display")
		.hint("menu.hint_infobar_sat")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(infobar_sat_display)),
	enumRow("infobar_casystem_display")
		.section("osd")
		.label("miscsettings.infobar_casystem_display")
		.hint("menu.hint_infobar_casys")
		.defaultValue(0)
		.values(kCaSystem)
		.field(COREAPI_NUMBER_FIELD(infobar_casystem_display)),
	// Both halves of this one are switched off in the program: the menu item
	// that would set it and the drawing that would read it. Written and stored,
	// it reaches nothing until one of those comes back, and the row stays so
	// that it works again when they do.
	boolRow("infobar_casystem_dotmatrix")
		.section("osd")
		.label("miscsettings.infobar_casystem_dotmatrix")
		.hint("menu.hint_infobar_casys_dotmatrix")
		.defaultValue(0)
		.changeableWhen(kCaSystemDrawn)
		.field(COREAPI_NUMBER_FIELD(infobar_casystem_dotmatrix)),
	boolRow("infobar_casystem_frame")
		.section("osd")
		.label("miscsettings.infobar_casystem_frame")
		.hint("menu.hint_infobar_casys_frame")
		.defaultValue(0)
		.changeableWhen(kCaSystemDrawn)
		.field(COREAPI_NUMBER_FIELD(infobar_casystem_frame)),
	enumRow("show_ecm_pos")
		.section("osd")
		.label("ecminfo_show")
		.hint("menu.hint_infobar_ecminfo")
		.defaultValue(0)
		.values(kEcmPos)
		.field(COREAPI_NUMBER_FIELD(show_ecm_pos)),
	boolRow("infobar_show_sysfs_hdd")
		.section("osd")
		.label("miscsettings.infobar_show_sysfs_hdd")
		.hint("menu.hint_infobar_filesys")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(infobar_show_sysfs_hdd)),
	enumRow("hdd_statfs_mode")
		.section("osd")
		.label("hdd_statfs")
		.hint("menu.hint_hdd_statfs")
		.defaultValue(2)
		.values(kHddStatfs)
		.changeableWhen(kSysfsHddOn)
		.field(COREAPI_NUMBER_FIELD(hdd_statfs_mode)),
	/* Applies only where the box has a second tuner, counted at run time and
	   whether or not it is switched on: the screen offers the setting on that
	   count, and a tuner that is switched on later finds it already chosen. */
	boolRow("infobar_show_tuner")
		.section("osd")
		.label("miscsettings.infobar_show_tuner")
		.hint("menu.hint_infobar_tuner")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD_ON(infobar_show_tuner, severalTunersFitted, NULL)),
	enumRow("infobar_show_res")
		.section("osd")
		.label("miscsettings.infobar_show_res")
		.hint("menu.hint_infobar_res")
		.defaultValue(0)
		.values(kInfobarShowRes)
		.field(COREAPI_NUMBER_FIELD(infobar_show_res)),
	boolRow("infobar_show_dd_available")
		.section("osd")
		.label("miscsettings.infobar_show_dd_available")
		.hint("menu.hint_infobar_dd")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(infobar_show_dd_available)),
	enumRow("infobar_progressbar")
		.section("osd")
		.label("miscsettings.progressbar_infobar_position")
		.hint("menu.hint_progressbar_infobar_position")
		.defaultValue(2)
		.values(kProgressbarInfobarPos)
		.field(COREAPI_NUMBER_FIELD(infobar_progressbar)),

	// channel list
	enumRow("channellist_additional")
		.section("osd")
		.label("channellist.additional")
		.hint("menu.hint_channellist_additional")
		.defaultValue(1)
		.values(kChannellistAdditional)
		.field(COREAPI_NUMBER_FIELD(channellist_additional)),
	enumRow("channellist_epgtext_alignment")
		.section("osd")
		.label("miscsettings.channellist_epgtext_alignment")
		.hint("menu.hint_channellist_epg_align")
		.defaultValue(0)
		.values(kEpgtextAlignment)
		.field(COREAPI_NUMBER_FIELD(channellist_epgtext_alignment)),
	boolRow("channellist_show_res_icon")
		.section("osd")
		.label("channellist.show_res_icon")
		.hint("menu.hint_channellist_show_res_icon")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(channellist_show_res_icon)),
	boolRow("channellist_show_infobox")
		.section("osd")
		.label("channellist.show_infobox")
		.hint("menu.hint_channellist_show_infobox")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(channellist_show_infobox)),
	enumRow("channellist_foot")
		.section("osd")
		.label("channellist.foot")
		.hint("menu.hint_channellist_foot")
		.defaultValue(1)
		.values(kChannellistFoot)
		.changeableWhen(kInfoboxOn)
		.field(COREAPI_NUMBER_FIELD(channellist_foot)),
	boolRow("channellist_show_numbers")
		.section("osd")
		.label("channellist.show_channelnumber")
		.hint("menu.hint_channellist_show_channelnumber")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(channellist_show_numbers)),

	// event list
	boolRow("eventlist_additional")
		.section("osd")
		.label("eventlist.additional")
		.hint("menu.hint_eventlist_additional")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(eventlist_additional)),
	boolRow("eventlist_epgplus")
		.section("osd")
		.label("eventlist.epgplus")
		.hint("menu.hint_eventlist_epgplus")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(eventlist_epgplus)),

	// volume
	enumRow("volume_pos")
		.section("osd")
		.label("extra.volume_pos")
		.hint("menu.hint_volume_pos")
		.defaultValue(5)
		.values(kVolumePos)
		.field(COREAPI_NUMBER_FIELD(volume_pos)),
	/* A height in pixels. The floor the box enforces is the height of the
	   volume icon it loaded, so the floor below is the widest one that holds
	   every value that could be offered rather than the box's own. */
	intRow("volume_size")
		.section("osd")
		.label("extra.volume_size")
		.hint("menu.hint_volume_size")
		.range(0, 50)
		.defaultValue(26)
		.field(COREAPI_NUMBER_FIELD(volume_size)),
	boolRow("volume_digits")
		.section("osd")
		.label("extra.volume_digits")
		.hint("menu.hint_volume_digits")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(volume_digits)),
	boolRow("show_mute_icon")
		.section("osd")
		.label("extra.show_mute_icon")
		.hint("menu.hint_show_mute_icon")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(show_mute_icon)),

	// info clock
	boolRow("mode_clock")
		.section("osd")
		.label("miscsettings.infoclock")
		.hint("menu.hint_clock_mode")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(mode_clock)),
	// A height in pixels again. The program refuses nought and below and puts
	// the default back, which is the same number as the floor.
	intRow("infoClockFontSize")
		.section("osd")
		.label("clock_size_height")
		.hint("menu.hint_clock_size")
		.range(30, 120)
		.defaultValue(30)
		.field(COREAPI_NUMBER_FIELD(infoClockFontSize)),
	boolRow("infoClockSeconds")
		.section("osd")
		.label("clock_seconds")
		.hint("menu.hint_clock_seconds")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(infoClockSeconds)),
	boolRow("infoClockBackground")
		.section("osd")
		.label("clock_background")
		.hint("menu.hint_clock_background")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(infoClockBackground)),

	// screen
	enumRow("screen_preset")
		.section("osd")
		.label("colormenu.osd_preset")
		.hint("menu.hint_osd_preset")
		.defaultValue(0)
		.values(kScreenPreset)
		.field(COREAPI_NUMBER_FIELD(screen_preset)),

	// screenshot
	textRow("screenshot_dir")
		.section("osd")
		.label("screenshot.defdir")
		.hint("menu.hint_screenshot_dir")
		.defaultValue(TARGET_ROOT "/media/sda1/movies")
		.text(kRuleDirectoryDurable)
		.field(COREAPI_TEXT_FIELD(screenshot_dir)),
	intRow("screenshot_count")
		.section("osd")
		.label("screenshot.count")
		.hint("menu.hint_screenshot_count")
		.range(1, 5)
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(screenshot_count)),
	enumRow("screenshot_format")
		.section("osd")
		.label("screenshot.format")
		.hint("menu.hint_screenshot_format")
		.defaultValue(1)
		.values(kScreenshotFormat)
		.field(COREAPI_NUMBER_FIELD(screenshot_format)),
	enumRow("screenshot_mode")
		.section("osd")
		.label("screenshot.res")
		.hint("menu.hint_screenshot_res")
		.defaultValue(0)
		.values(kScreenshotMode)
		.field(COREAPI_NUMBER_FIELD(screenshot_mode)),
	boolRow("screenshot_video")
		.section("osd")
		.label("screenshot.video")
		.hint("menu.hint_screenshot_video")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(screenshot_video)),
	boolRow("screenshot_scale")
		.section("osd")
		.label("screenshot.scale")
		.hint("menu.hint_screenshot_scale")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(screenshot_scale)),
	boolRow("screenshot_cover")
		.section("osd")
		.label("screenshot.cover")
		.hint("menu.hint_screenshot_cover")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(screenshot_cover)),

	// screensaver
	// Minutes, and nought is off.
	intRow("screensaver_delay")
		.section("osd")
		.label("screensaver.delay")
		.hint("menu.hint_screensaver_delay")
		.range(0, 999)
		.defaultValue(1)
		.values(kScreensaverDelayOff)
		.unit("unit.short.minute")
		.field(COREAPI_NUMBER_FIELD(screensaver_delay)),
	enumRow("screensaver_mode")
		.section("osd")
		.label("screensaver.mode")
		.hint("menu.hint_screensaver_mode")
		.defaultValue(1)
		.values(kScreensaverMode)
		.changeableWhen(kScreensaverOn)
		.field(COREAPI_NUMBER_FIELD(screensaver_mode)),
	// Seconds, and nought is off again.
	intRow("screensaver_timeout")
		.section("osd")
		.label("screensaver.timeout")
		.hint("menu.hint_screensaver_timeout")
		.range(0, 60)
		.defaultValue(10)
		.values(kScreensaverTimeoutOff)
		.changeableWhen(kScreensaverOn)
		.unit("unit.short.second")
		.field(COREAPI_NUMBER_FIELD(screensaver_timeout)),
	textRow("screensaver_dir")
		.section("osd")
		.label("screensaver.dir")
		.hint("menu.hint_screensaver_dir")
		.defaultValue(ICONSDIR "/screensaver")
		.changeableWhen(kScreensaverImage)
		.text(kRuleDirectory)
		.field(COREAPI_TEXT_FIELD(screensaver_dir)),
	boolRow("screensaver_random")
		.section("osd")
		.label("screensaver.random")
		.hint("menu.hint_screensaver_random")
		.defaultValue(0)
		.changeableWhen(kScreensaverImage)
		.field(COREAPI_NUMBER_FIELD(screensaver_random)),
	// The item for this one is switched off in the program as well, and the
	// screensaver reads the setting either way.
	boolRow("screensaver_mode_text")
		.section("osd")
		.label("screensaver.enable_text_info")
		.hint("menu.hint_screensaver_enable_text_info")
		.defaultValue(0)
		.changeableWhen(kScreensaverOn)
		.field(COREAPI_NUMBER_FIELD(screensaver_mode_text)),
	// infoicons
	enumRow("mode_icons_skin")
		.section("osd")
		.label("infoicons_skin")
		.hint("menu.hint_infoicons_skin")
		.defaultValue(0)
		.values(kInfoiconsSkin)
		.changeableWhen(kIconsOff)
		.field(COREAPI_NUMBER_FIELD(mode_icons_skin)),
	enumRow("mode_icons")
		.section("osd")
		.label("infoicons_modeicon")
		.hint("menu.hint_infoicons_modeicon")
		.defaultValue(0)
		.values(kInfoiconsMode)
		.changeableWhen(kSkinNotInfoviewer)
		.field(COREAPI_NUMBER_FIELD(mode_icons)),
	boolRow("mode_icons_background")
		.section("osd")
		.label("infoicons_background")
		.hint("menu.hint_infoicons_background")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(mode_icons_background)),
	/* Which of the two sizes the box draws its own screen at, as the mode the
	   program stores and not as a place in the list the framebuffer builds;
	   every driver builds its list in the order of the modes. Each is offered
	   where the box draws at that size.

	   The default is the one the pass that loads the settings falls back to.
	   The pass that sets the framebuffer up reads the same key with a different
	   fallback, which is a disagreement in the program and not in this row:
	   whoever changes one belongs at the other. */
	enumRow("osd_resolution")
		.section("osd")
		.label("colormenu.osd_resolution")
		.hint("menu.hint_osd_resolution")
#if HAVE_ARM_HARDWARE || (HAVE_CST_HARDWARE && defined(BOXMODEL_CST_HD2))
		.defaultValue(OSDMODE_1080)
#else
		.defaultValue(OSDMODE_720)
#endif
		.values(kOsdResolution)
		.field(COREAPI_SERVICE_FIELD(osd_resolution, askOsdResolution, tellOsdResolution)),
	/* The corners of the drawn area, four of them for each pairing of an OSD
	   resolution and a preset. All sixteen share the two words that are drawn
	   beside the corners and the key is what tells them apart: a names the full
	   pixel preset and b the other, while 0 names the 720 line OSD and 1 the
	   1080 line one.

	   Each needs a restart. The box copies the quartet its resolution and
	   preset name into the four values it draws with, and that runs at start
	   and where the resolution or the preset changes, not where one of these
	   does.

	   The upper left corner is held to 200 in both directions and the lower
	   right to at least 400; the ceiling of the lower right is the OSD's own
	   size, which the suffix names. */
	intRow("screen_StartX_a_0")
		.section("osd")
		.label("screensetup.upperleft")
		.range(0, 200)
		.defaultValue(0)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_StartX_a_0)),
	intRow("screen_StartY_a_0")
		.section("osd")
		.label("screensetup.upperleft")
		.range(0, 200)
		.defaultValue(0)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_StartY_a_0)),
	intRow("screen_EndX_a_0")
		.section("osd")
		.label("screensetup.lowerright")
		.range(400, 1279)
		.defaultValue(1279)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_EndX_a_0)),
	intRow("screen_EndY_a_0")
		.section("osd")
		.label("screensetup.lowerright")
		.range(400, 719)
		.defaultValue(719)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_EndY_a_0)),
	intRow("screen_StartX_a_1")
		.section("osd")
		.label("screensetup.upperleft")
		.range(0, 200)
		.defaultValue(0)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_StartX_a_1)),
	intRow("screen_StartY_a_1")
		.section("osd")
		.label("screensetup.upperleft")
		.range(0, 200)
		.defaultValue(0)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_StartY_a_1)),
	intRow("screen_EndX_a_1")
		.section("osd")
		.label("screensetup.lowerright")
		.range(400, 1919)
		.defaultValue(1919)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_EndX_a_1)),
	intRow("screen_EndY_a_1")
		.section("osd")
		.label("screensetup.lowerright")
		.range(400, 1079)
		.defaultValue(1079)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_EndY_a_1)),
	intRow("screen_StartX_b_0")
		.section("osd")
		.label("screensetup.upperleft")
		.range(0, 200)
		.defaultValue(22)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_StartX_b_0)),
	intRow("screen_StartY_b_0")
		.section("osd")
		.label("screensetup.upperleft")
		.range(0, 200)
		.defaultValue(12)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_StartY_b_0)),
	intRow("screen_EndX_b_0")
		.section("osd")
		.label("screensetup.lowerright")
		.range(400, 1279)
		.defaultValue(1236)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_EndX_b_0)),
	intRow("screen_EndY_b_0")
		.section("osd")
		.label("screensetup.lowerright")
		.range(400, 719)
		.defaultValue(695)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_EndY_b_0)),
	intRow("screen_StartX_b_1")
		.section("osd")
		.label("screensetup.upperleft")
		.range(0, 200)
		.defaultValue(33)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_StartX_b_1)),
	intRow("screen_StartY_b_1")
		.section("osd")
		.label("screensetup.upperleft")
		.range(0, 200)
		.defaultValue(18)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_StartY_b_1)),
	intRow("screen_EndX_b_1")
		.section("osd")
		.label("screensetup.lowerright")
		.range(400, 1919)
		.defaultValue(1854)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_EndX_b_1)),
	intRow("screen_EndY_b_1")
		.section("osd")
		.label("screensetup.lowerright")
		.range(400, 1079)
		.defaultValue(1043)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_EndY_b_1)),

	/* Whether the infobar shows the module line at all. No item sets it: the
	   menu derives it from the position beside it, so writing the position
	   alone leaves this one as it was and the line does not follow. */
	boolRow("show_ecm")
		.section("osd")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(show_ecm)),
	// Whether the infobar carries the channel description.
	boolRow("infobar_show_channeldesc")
		.section("osd")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(infobar_show_channeldesc)),
	/* The name of the colour theme, picked from a file list rather than typed,
	   and read once at start. Its default is a migration name where the box has
	   no settings file yet and empty otherwise, which is what every box that
	   has saved once gets and what the row states. */
	textRow("theme_name")
		.section("osd")
		.defaultValue("")
		.needsRestart()
		.text(kRuleNameFromList)
		.field(COREAPI_TEXT_FIELD(theme_name)),
	/* The colours of the theme, one row each. Every channel is a step from 0 to 100 in
	   the struct, which is what the colour screens move it by, and the row's text is
	   that step scaled to a byte. The three text rows leave the alpha the struct
	   carries beside them alone: the screens never offered it. The defaults are the
	   ones a theme file falls back to for a channel it does not name. */
	colorRow("theme.menu_Head")
		.section("osd")
		.label("colormenu.background")
		.hint("menu.hint_head_back")
		.withAlpha()
		.defaultValue("#0000001a")
		.field(COREAPI_COLOR_FIELD(theme, menu_Head, true)),
	colorRow("theme.menu_Head_Text")
		.section("osd")
		.label("colormenu.textcolor")
		.hint("menu.hint_head_textcolor")
		.defaultValue("#fc6e12")
		.field(COREAPI_COLOR_FIELD(theme, menu_Head_Text, false)),
	colorRow("theme.menu_Content")
		.section("osd")
		.label("colormenu.background")
		.hint("menu.hint_content_back")
		.withAlpha()
		.defaultValue("#2121211a")
		.field(COREAPI_COLOR_FIELD(theme, menu_Content, true)),
	colorRow("theme.menu_Content_Text")
		.section("osd")
		.label("colormenu.textcolor")
		.hint("menu.hint_content_textcolor")
		.defaultValue("#fafafa")
		.field(COREAPI_COLOR_FIELD(theme, menu_Content_Text, false)),
	colorRow("theme.menu_Content_Selected")
		.section("osd")
		.label("colormenu.background")
		.hint("menu.hint_selected_back")
		.withAlpha()
		.defaultValue("#fc6e121a")
		.field(COREAPI_COLOR_FIELD(theme, menu_Content_Selected, true)),
	colorRow("theme.menu_Content_Selected_Text")
		.section("osd")
		.label("colormenu.textcolor")
		.hint("menu.hint_selected_text")
		.defaultValue("#000000")
		.field(COREAPI_COLOR_FIELD(theme, menu_Content_Selected_Text, false)),
	colorRow("theme.menu_Content_inactive")
		.section("osd")
		.label("colormenu.background")
		.hint("menu.hint_inactive_back")
		.withAlpha()
		.defaultValue("#2121211a")
		.field(COREAPI_COLOR_FIELD(theme, menu_Content_inactive, true)),
	colorRow("theme.menu_Content_inactive_Text")
		.section("osd")
		.label("colormenu.textcolor")
		.hint("menu.hint_inactive_textcolor")
		.defaultValue("#9e9e9e")
		.field(COREAPI_COLOR_FIELD(theme, menu_Content_inactive_Text, false)),
	colorRow("theme.menu_Foot")
		.section("osd")
		.label("colormenu.background")
		.hint("menu.hint_foot_back")
		.withAlpha()
		.defaultValue("#0000001a")
		.field(COREAPI_COLOR_FIELD(theme, menu_Foot, true)),
	colorRow("theme.menu_Foot_Text")
		.section("osd")
		.label("colormenu.textcolor")
		.hint("menu.hint_foot_textcolor")
		.defaultValue("#fafafa")
		.field(COREAPI_COLOR_FIELD(theme, menu_Foot_Text, false)),
	colorRow("theme.infobar")
		.section("osd")
		.label("colormenu.background")
		.hint("menu.hint_infobar_back")
		.withAlpha()
		.defaultValue("#2121211a")
		.field(COREAPI_COLOR_FIELD(theme, infobar, true)),
	colorRow("theme.infobar_Text")
		.section("osd")
		.label("colormenu.textcolor")
		.hint("menu.hint_infobar_textcolor")
		.defaultValue("#fafafa")
		.field(COREAPI_COLOR_FIELD(theme, infobar_Text, false)),
	colorRow("theme.infobar_casystem")
		.section("osd")
		.label("miscsettings.infobar_casystem_display")
		.hint("menu.hint_infobar_casys_color")
		.withAlpha()
		.defaultValue("#2121211a")
		.field(COREAPI_COLOR_FIELD(theme, infobar_casystem, true)),
	colorRow("theme.channellist_Description_Text")
		.section("osd")
		.label("colormenu.channellist_description_text")
		.hint("menu.hint_color_channellist_description_text")
		.defaultValue("#fafafa")
		.field(COREAPI_COLOR_FIELD(theme, channellist_Description_Text, false)),
	colorRow("theme.colored_events")
		.section("osd")
		.label("colormenu.textcolor")
		.hint("menu.hint_colored_events_textcolor")
		.defaultValue("#fc6e12")
		.field(COREAPI_COLOR_FIELD(theme, colored_events, false)),
	colorRow("theme.progressbar_passive")
		.section("osd")
		.label("colormenu.progressbar_passive")
		.hint("menu.hint_progressbar_passive")
		.defaultValue("#424242")
		.field(COREAPI_COLOR_FIELD(theme, progressbar_passive, false)),
	colorRow("theme.progressbar_active")
		.section("osd")
		.label("colormenu.progressbar_active")
		.hint("menu.hint_progressbar_active")
		.defaultValue("#9e9e9e")
		.field(COREAPI_COLOR_FIELD(theme, progressbar_active, false)),
	colorRow("theme.shadow")
		.section("osd")
		.label("colormenu.shadow_color")
		.hint("menu.hint_colors_shadow")
		.withAlpha()
		.defaultValue("#00000040")
		.field(COREAPI_COLOR_FIELD(theme, shadow, true)),
	colorRow("theme.clock_Digit")
		.section("osd")
		.label("colormenu.clock_textcolor")
		.hint("menu.hint_clock_textcolor")
		.defaultValue("#9e9e9e")
		.field(COREAPI_COLOR_FIELD(theme, clock_Digit, false)),
};

} // anonymous namespace

const Descriptor *settingsTableOsd(size_t &count)
{
	count = sizeof(kOsd) / sizeof(kOsd[0]);
	return kOsd;
}

} // namespace coreapi
