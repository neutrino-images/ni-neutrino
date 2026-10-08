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

const EnumValue kSubchanPos[] =
{
	{ 0, "settings.pos_top_right", NULL, NULL },
	{ 1, "settings.pos_top_left", NULL, NULL },
	{ 2, "settings.pos_bottom_left", NULL, NULL },
	{ 3, "settings.pos_bottom_right", NULL, NULL },
	{ 4, "infoviewer.subchan_infobar", NULL, NULL }
};

const EnumValue kMenuPos[] =
{
	{ MENU_POS_CENTER, "settings.pos_center", NULL, NULL },
	{ MENU_POS_TOP_LEFT, "settings.pos_top_left", NULL, NULL },
	{ MENU_POS_TOP_RIGHT, "settings.pos_top_right", NULL, NULL },
	{ MENU_POS_BOTTOM_LEFT, "settings.pos_bottom_left", NULL, NULL },
	{ MENU_POS_BOTTOM_RIGHT, "settings.pos_bottom_right", NULL, NULL }
};

const EnumValue kChannelLogoPos[] =
{
	{ 0, "options.off", NULL, NULL },
	{ CC_LOGO_RIGHT, "settings.pos_right", NULL, NULL },
	{ CC_LOGO_LEFT, "settings.pos_left", NULL, NULL },
	{ CC_LOGO_CENTER, "settings.pos_center", NULL, NULL }
};

const EnumValue kInfobarDisp[] =
{
	{ 0, "miscsettings.infobar_disp_0", NULL, NULL },
	{ 1, "miscsettings.infobar_disp_1", NULL, NULL },
	{ 2, "miscsettings.infobar_disp_2", NULL, NULL },
	{ 3, "miscsettings.infobar_disp_3", NULL, NULL },
	{ 4, "miscsettings.infobar_disp_4", NULL, NULL },
	{ 5, "miscsettings.infobar_disp_5", NULL, NULL },
	{ 6, "miscsettings.infobar_disp_6", NULL, NULL }
};

// Nought is on here and the last value is off, which is why this is a choice
// and not a flag.
const EnumValue kCaSystem[] =
{
	{ 0, "options.on", NULL, NULL },
	{ 1, "miscsettings.infobar_casystem_mode", NULL, NULL },
	{ 2, "miscsettings.infobar_casystem_mini", NULL, NULL },
	{ 3, "options.off", NULL, NULL }
};

const EnumValue kEcmPos[] =
{
	{ 0, "options.off", NULL, NULL },
	{ 1, "settings.pos_top_left", NULL, NULL },
	{ 2, "settings.pos_top_center", NULL, NULL },
	{ 3, "settings.pos_top_right", NULL, NULL }
};

const EnumValue kHddStatfs[] =
{
	{ SNeutrinoSettings::HDD_STATFS_OFF, "options.off", NULL, NULL },
	{ SNeutrinoSettings::HDD_STATFS_ALWAYS, "hdd_statfs_always", NULL, NULL },
	{ SNeutrinoSettings::HDD_STATFS_RECORDING, "hdd_statfs_recording", NULL, NULL }
};

// Nought is on here as well.
const EnumValue kInfobarShowRes[] =
{
	{ 0, "options.on", NULL, NULL },
	{ 1, "miscsettings.infobar_show_res_simple", NULL, NULL },
	{ 2, "options.off", NULL, NULL }
};

const EnumValue kProgressbarInfobarPos[] =
{
	{ SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_DEFAULT, "miscsettings.progressbar_infobar_position_0", NULL, NULL },
	{ SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BELOW_CH_NAME, "miscsettings.progressbar_infobar_position_1", NULL, NULL },
	{ SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BELOW_CH_NAME_SMALL, "miscsettings.progressbar_infobar_position_2", NULL, NULL },
	{ SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BETWEEN_EVENTS, "miscsettings.progressbar_infobar_position_3", NULL, NULL }
};

const EnumValue kChannellistAdditional[] =
{
	{ 0, "channellist.additional_off", NULL, NULL },
	{ 1, "channellist.additional_on", NULL, NULL },
	{ 2, "channellist.additional_on_minitv", NULL, NULL }
};

const EnumValue kEpgtextAlignment[] =
{
	{ EPGTEXT_ALIGN_LEFT_MIDDLE, "channellist.epgtext_align_left_middle", NULL, NULL },
	{ EPGTEXT_ALIGN_LEFT_BOTTOM, "channellist.epgtext_align_left_bottom", NULL, NULL },
	{ EPGTEXT_ALIGN_RIGHT_MIDDLE, "channellist.epgtext_align_right_middle", NULL, NULL },
	{ EPGTEXT_ALIGN_RIGHT_BOTTOM, "channellist.epgtext_align_right_bottom", NULL, NULL }
};

const EnumValue kChannellistFoot[] =
{
	{ 0, "channellist.foot_freq", NULL, NULL },
	{ 1, "channellist.foot_next", NULL, NULL },
	{ 2, "channellist.foot_off", NULL, NULL }
};

const EnumValue kVolumePos[] =
{
	{ VOLUMEBAR_POS_TOP_RIGHT, "settings.pos_top_right", NULL, NULL },
	{ VOLUMEBAR_POS_TOP_LEFT, "settings.pos_top_left", NULL, NULL },
	{ VOLUMEBAR_POS_BOTTOM_LEFT, "settings.pos_bottom_left", NULL, NULL },
	{ VOLUMEBAR_POS_BOTTOM_RIGHT, "settings.pos_bottom_right", NULL, NULL },
	{ VOLUMEBAR_POS_TOP_CENTER, "settings.pos_top_center", NULL, NULL },
	{ VOLUMEBAR_POS_BOTTOM_CENTER, "settings.pos_bottom_center", NULL, NULL },
	{ VOLUMEBAR_POS_HIGHER_CENTER, "settings.pos_higher_center", NULL, NULL }
};

const EnumValue kScreenPreset[] =
{
	{ PRESET_SCREEN_A, "osd.preset_screen_a", NULL, NULL },
	{ PRESET_SCREEN_B, "osd.preset_screen_b", NULL, NULL }
};

// The formats are named by what they are and not by a locale.
const EnumValue kScreenshotFormat[] =
{
	{ FORMAT_PNG, NULL, "PNG", NULL },
	{ FORMAT_JPG, NULL, "JPEG", NULL },
	{ FORMAT_BMP, NULL, "BMP", NULL }
};

const EnumValue kScreenshotMode[] =
{
	{ 0, "screenshot.tv", NULL, NULL },
	{ 1, "screenshot.osd", NULL, NULL }
};

// The floors of the screensaver delay and timeout, shown in words.
const EnumValue kScreensaverDelayOff[] =
{
	{ 0, "screensaver.off", NULL, NULL }
};

const EnumValue kScreensaverTimeoutOff[] =
{
	{ 0, "options.off", NULL, NULL }
};

const EnumValue kScreensaverMode[] =
{
	{ SCR_MODE_IMAGE, "screensaver.mode_image", NULL, NULL },
	{ SCR_MODE_CLOCK, "screensaver.mode_clock", NULL, NULL },
	{ SCR_MODE_CLOCK_COLOR, "screensaver.mode_clock_color", NULL, NULL }
};

// The value of one setting that makes another editable.
const Condition kChannelLogoOn[] =
{
	{ "channellist_show_channellogo", CompareOp::Ne, 0, NULL, 0 }
};

// Two and three are the mini bar and off, and neither has a frame to draw.
const Condition kCaSystemDrawn[] =
{
	{ "infobar_casystem_display", CompareOp::Lt, 2, NULL, 0 }
};

const Condition kSysfsHddOn[] =
{
	{ "infobar_show_sysfs_hdd", CompareOp::Ne, 0, NULL, 0 }
};

const Condition kInfoboxOn[] =
{
	{ "channellist_show_infobox", CompareOp::Ne, 0, NULL, 0 }
};

const Condition kScreensaverOn[] =
{
	{ "screensaver_delay", CompareOp::Ne, 0, NULL, 0 }
};

// The image mode is the only one that reads a directory of its own.
const Condition kScreensaverImage[] =
{
	{ "screensaver_delay", CompareOp::Ne, 0, NULL, 0 },
	{ "screensaver_mode", CompareOp::Eq, 0, NULL, 0 }
};

const EnumValue kInfoiconsSkin[] =
{
	{ INFOICONS_STATIC, "infoicons_static", NULL, NULL },
	{ INFOICONS_INFOVIEWER, "infoicons_infoviewer", NULL, NULL },
	{ INFOICONS_POPUP, "infoicons_popup", NULL, NULL }
};

/* A choice and not a flag: the two values are start and stop rather than on and
   off, and a flag row would carry the values while nothing held the words. */
const EnumValue kInfoiconsMode[] =
{
	{ 0, "options.start", NULL, NULL },
	{ 1, "options.stop", NULL, NULL }
};

// The skin is offered only while the icons are off.
const Condition kIconsOff[] =
{
	{ "mode_icons", CompareOp::Eq, 0, NULL, 0 }
};

// And the icons only where the skin is not the one the infobar draws.
const Condition kSkinNotInfoviewer[] =
{
	{ "mode_icons_skin", CompareOp::Ne, INFOICONS_INFOVIEWER, NULL, 0 }
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

const EnumValue kOsdResolution[] =
{
	{ OSDMODE_720, NULL, "1280x720", drawsOsd720 },
	{ OSDMODE_1080, NULL, "1920x1080", drawsOsd1080 }
};

const Descriptor kOsd[] =
{
	{
		"radiotext_enable", ValueType::Bool, "osd",
		"miscsettings.radiotext", "menu.hint_infobar_radiotext",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(radiotext_enable)
	},
	{
		"scrambled_message", ValueType::Bool, "osd",
		"extra.scrambled_message", "menu.hint_scrambled_message",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(scrambled_message)
	},
	{
		"widget_fade", ValueType::Bool, "osd",
		"colormenu.fade", "menu.hint_fade",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(widget_fade)
	},
	/* The two below are one item, edited by a key loop rather than by a
	   chooser, so the range is the one that loop enforces and the label is the
	   one item's. The program falls back to whatever window_size holds, and the
	   value below is what that key falls back to in turn, which is the only
	   part of it a constant can carry. */
	{
		"window_width", ValueType::Int, "osd",
		"window_size", "menu.hint_window_size",
		50, 100, NULL, 0, 100, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(window_width)
	},
	{
		"window_height", ValueType::Int, "osd",
		"window_size", "menu.hint_window_size",
		50, 100, NULL, 0, 100, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(window_height)
	},
	{
		"infobar_subchan_disp_pos", ValueType::Enum, "osd",
		"infoviewer.subchan_disp_pos", "menu.hint_subchannel_pos",
		0, 0, COREAPI_ENUM(kSubchanPos), 4, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_subchan_disp_pos)
	},

	// fonts
	{
		"font_file", ValueType::String, "osd",
		"colormenu.font", "menu.hint_font_gui",
		0, 0, NULL, 0, 0, FONTDIR "/neutrino.ttf", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(font_file)
	},
	{
		"font_file_monospace", ValueType::String, "osd",
		"colormenu.font_ttx", "menu.hint_font_ttx",
		0, 0, NULL, 0, 0, FONTDIR "/tuxtxt.ttf", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(font_file_monospace)
	},
	// Per cent of the size each font is configured at.
	{
		"font_scaling_x", ValueType::Int, "osd",
		"fontmenu.scaling_x", "fontmenu.scaling_x_hint2",
		50, 200, NULL, 0, 105, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(font_scaling_x)
	},
	{
		"font_scaling_y", ValueType::Int, "osd",
		"fontmenu.scaling_y", "fontmenu.scaling_y_hint2",
		50, 200, NULL, 0, 105, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(font_scaling_y)
	},

	// menus
	{
		"menu_pos", ValueType::Enum, "osd",
		"settings.menu_pos", "menu.hint_menu_pos",
		0, 0, COREAPI_ENUM(kMenuPos), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(menu_pos)
	},
	{
		"show_menu_hints", ValueType::Bool, "osd",
		"settings.menu_hints", "menu.hint_menu_hints",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(show_menu_hints)
	},
	{
		"show_menu_hints_line", ValueType::Bool, "osd",
		"settings.menu_hints_line", "menu.hint_menu_hints_line",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(show_menu_hints_line)
	},

	// channel logos
	{
		"logo_hdd_dir", ValueType::String, "osd",
		"miscsettings.infobar_logo_hdd_dir", "menu.hint_infobar_logo_dir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/logos", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(logo_hdd_dir)
	},
	{
		"channellist_show_channellogo", ValueType::Enum, "osd",
		"channellist.show_channellogo", "menu.hint_channellist_show_channellogo",
		0, 0, COREAPI_ENUM(kChannelLogoPos), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_show_channellogo)
	},
	{
		"channellist_show_eventlogo", ValueType::Bool, "osd",
		"channellist.show_eventlogo", "menu.hint_channellist_show_eventlogo",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_CONDITIONS(kChannelLogoOn),
		COREAPI_NUMBER_FIELD(channellist_show_eventlogo)
	},

	// infobar
	{
		"infobar_show", ValueType::Bool, "osd",
		"miscsettings.infobar_show", "menu.hint_infobar_on_epg",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_show)
	},
	{
		"infobar_buttons_usertitle", ValueType::Bool, "osd",
		"miscsettings.infobar_buttons_usertitle", "menu.hint_infobar_buttons_usertitle",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_buttons_usertitle)
	},
	{
		"infobar_analogclock", ValueType::Bool, "osd",
		"miscsettings.infobar_analogclock", "menu.hint_infobar_analogclock",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_analogclock)
	},
	// Meaningful only where the weather is switched on, which is a key another
	// section declares and a condition cannot name yet.
	{
		"infobar_weather", ValueType::Bool, "osd",
		"miscsettings.infobar_weather", "menu.hint_infobar_weather",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_weather)
	},
	{
		"infobar_show_channellogo", ValueType::Enum, "osd",
		"miscsettings.infobar_disp", "menu.hint_infobar_logo",
		0, 0, COREAPI_ENUM(kInfobarDisp), 5, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_show_channellogo)
	},
	{
		"infobar_sat_display", ValueType::Bool, "osd",
		"miscsettings.infobar_sat_display", "menu.hint_infobar_sat",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_sat_display)
	},
	{
		"infobar_casystem_display", ValueType::Enum, "osd",
		"miscsettings.infobar_casystem_display", "menu.hint_infobar_casys",
		0, 0, COREAPI_ENUM(kCaSystem), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_casystem_display)
	},
	// Both halves of this one are switched off in the program: the menu item
	// that would set it and the drawing that would read it. Written and stored,
	// it reaches nothing until one of those comes back, and the row stays so
	// that it works again when they do.
	{
		"infobar_casystem_dotmatrix", ValueType::Bool, "osd",
		"miscsettings.infobar_casystem_dotmatrix", "menu.hint_infobar_casys_dotmatrix",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kCaSystemDrawn),
		COREAPI_NUMBER_FIELD(infobar_casystem_dotmatrix)
	},
	{
		"infobar_casystem_frame", ValueType::Bool, "osd",
		"miscsettings.infobar_casystem_frame", "menu.hint_infobar_casys_frame",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kCaSystemDrawn),
		COREAPI_NUMBER_FIELD(infobar_casystem_frame)
	},
	{
		"show_ecm_pos", ValueType::Enum, "osd",
		"ecminfo_show", "menu.hint_infobar_ecminfo",
		0, 0, COREAPI_ENUM(kEcmPos), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(show_ecm_pos)
	},
	{
		"infobar_show_sysfs_hdd", ValueType::Bool, "osd",
		"miscsettings.infobar_show_sysfs_hdd", "menu.hint_infobar_filesys",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_show_sysfs_hdd)
	},
	{
		"hdd_statfs_mode", ValueType::Enum, "osd",
		"hdd_statfs", "menu.hint_hdd_statfs",
		0, 0, COREAPI_ENUM(kHddStatfs), 2, NULL, false, false, COREAPI_CONDITIONS(kSysfsHddOn),
		COREAPI_NUMBER_FIELD(hdd_statfs_mode)
	},
	// Applies only where the box has a second tuner, which is counted at run
	// time and not a setting to condition on.
	{
		"infobar_show_tuner", ValueType::Bool, "osd",
		"miscsettings.infobar_show_tuner", "menu.hint_infobar_tuner",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_show_tuner)
	},
	{
		"infobar_show_res", ValueType::Enum, "osd",
		"miscsettings.infobar_show_res", "menu.hint_infobar_res",
		0, 0, COREAPI_ENUM(kInfobarShowRes), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_show_res)
	},
	{
		"infobar_show_dd_available", ValueType::Bool, "osd",
		"miscsettings.infobar_show_dd_available", "menu.hint_infobar_dd",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_show_dd_available)
	},
	{
		"infobar_progressbar", ValueType::Enum, "osd",
		"miscsettings.progressbar_infobar_position", "menu.hint_progressbar_infobar_position",
		0, 0, COREAPI_ENUM(kProgressbarInfobarPos), 2, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_progressbar)
	},

	// channel list
	{
		"channellist_additional", ValueType::Enum, "osd",
		"channellist.additional", "menu.hint_channellist_additional",
		0, 0, COREAPI_ENUM(kChannellistAdditional), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_additional)
	},
	{
		"channellist_epgtext_alignment", ValueType::Enum, "osd",
		"miscsettings.channellist_epgtext_alignment", "menu.hint_channellist_epg_align",
		0, 0, COREAPI_ENUM(kEpgtextAlignment), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_epgtext_alignment)
	},
	{
		"channellist_show_res_icon", ValueType::Bool, "osd",
		"channellist.show_res_icon", "menu.hint_channellist_show_res_icon",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_show_res_icon)
	},
	{
		"channellist_show_infobox", ValueType::Bool, "osd",
		"channellist.show_infobox", "menu.hint_channellist_show_infobox",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_show_infobox)
	},
	{
		"channellist_foot", ValueType::Enum, "osd",
		"channellist.foot", "menu.hint_channellist_foot",
		0, 0, COREAPI_ENUM(kChannellistFoot), 1, NULL, false, false, COREAPI_CONDITIONS(kInfoboxOn),
		COREAPI_NUMBER_FIELD(channellist_foot)
	},
	{
		"channellist_show_numbers", ValueType::Bool, "osd",
		"channellist.show_channelnumber", "menu.hint_channellist_show_channelnumber",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_show_numbers)
	},

	// event list
	{
		"eventlist_additional", ValueType::Bool, "osd",
		"eventlist.additional", "menu.hint_eventlist_additional",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(eventlist_additional)
	},
	{
		"eventlist_epgplus", ValueType::Bool, "osd",
		"eventlist.epgplus", "menu.hint_eventlist_epgplus",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(eventlist_epgplus)
	},

	// volume
	{
		"volume_pos", ValueType::Enum, "osd",
		"extra.volume_pos", "menu.hint_volume_pos",
		0, 0, COREAPI_ENUM(kVolumePos), 5, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(volume_pos)
	},
	/* A height in pixels. The floor the box enforces is the height of the
	   volume icon it loaded, so the floor below is the widest one that holds
	   every value that could be offered rather than the box's own. */
	{
		"volume_size", ValueType::Int, "osd",
		"extra.volume_size", "menu.hint_volume_size",
		0, 50, NULL, 0, 26, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(volume_size)
	},
	{
		"volume_digits", ValueType::Bool, "osd",
		"extra.volume_digits", "menu.hint_volume_digits",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(volume_digits)
	},
	{
		"show_mute_icon", ValueType::Bool, "osd",
		"extra.show_mute_icon", "menu.hint_show_mute_icon",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(show_mute_icon)
	},

	// info clock
	{
		"mode_clock", ValueType::Bool, "osd",
		"miscsettings.infoclock", "menu.hint_clock_mode",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mode_clock)
	},
	// A height in pixels again. The program refuses nought and below and puts
	// the default back, which is the same number as the floor.
	{
		"infoClockFontSize", ValueType::Int, "osd",
		"clock_size_height", "menu.hint_clock_size",
		30, 120, NULL, 0, 30, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infoClockFontSize)
	},
	{
		"infoClockSeconds", ValueType::Bool, "osd",
		"clock_seconds", "menu.hint_clock_seconds",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infoClockSeconds)
	},
	{
		"infoClockBackground", ValueType::Bool, "osd",
		"clock_background", "menu.hint_clock_background",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infoClockBackground)
	},

	// screen
	{
		"screen_preset", ValueType::Enum, "osd",
		"colormenu.osd_preset", "menu.hint_osd_preset",
		0, 0, COREAPI_ENUM(kScreenPreset), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_preset)
	},

	// screenshot
	{
		"screenshot_dir", ValueType::String, "osd",
		"screenshot.defdir", "menu.hint_screenshot_dir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/movies", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(screenshot_dir)
	},
	{
		"screenshot_count", ValueType::Int, "osd",
		"screenshot.count", "menu.hint_screenshot_count",
		1, 5, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screenshot_count)
	},
	{
		"screenshot_format", ValueType::Enum, "osd",
		"screenshot.format", "menu.hint_screenshot_format",
		0, 0, COREAPI_ENUM(kScreenshotFormat), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screenshot_format)
	},
	{
		"screenshot_mode", ValueType::Enum, "osd",
		"screenshot.res", "menu.hint_screenshot_res",
		0, 0, COREAPI_ENUM(kScreenshotMode), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screenshot_mode)
	},
	{
		"screenshot_video", ValueType::Bool, "osd",
		"screenshot.video", "menu.hint_screenshot_video",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screenshot_video)
	},
	{
		"screenshot_scale", ValueType::Bool, "osd",
		"screenshot.scale", "menu.hint_screenshot_scale",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screenshot_scale)
	},
	{
		"screenshot_cover", ValueType::Bool, "osd",
		"screenshot.cover", "menu.hint_screenshot_cover",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screenshot_cover)
	},

	// screensaver
	// Minutes, and nought is off.
	{
		"screensaver_delay", ValueType::Int, "osd",
		"screensaver.delay", "menu.hint_screensaver_delay",
		0, 999, COREAPI_VALUES(kScreensaverDelayOff), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screensaver_delay)
	},
	{
		"screensaver_mode", ValueType::Enum, "osd",
		"screensaver.mode", "menu.hint_screensaver_mode",
		0, 0, COREAPI_ENUM(kScreensaverMode), 1, NULL, false, false, COREAPI_CONDITIONS(kScreensaverOn),
		COREAPI_NUMBER_FIELD(screensaver_mode)
	},
	// Seconds, and nought is off again.
	{
		"screensaver_timeout", ValueType::Int, "osd",
		"screensaver.timeout", "menu.hint_screensaver_timeout",
		0, 60, COREAPI_VALUES(kScreensaverTimeoutOff), 10, NULL, false, false, COREAPI_CONDITIONS(kScreensaverOn),
		COREAPI_NUMBER_FIELD(screensaver_timeout)
	},
	{
		"screensaver_dir", ValueType::String, "osd",
		"screensaver.dir", "menu.hint_screensaver_dir",
		0, 0, NULL, 0, 0, ICONSDIR "/screensaver", false, false, COREAPI_CONDITIONS(kScreensaverImage),
		COREAPI_TEXT_FIELD(screensaver_dir)
	},
	{
		"screensaver_random", ValueType::Bool, "osd",
		"screensaver.random", "menu.hint_screensaver_random",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kScreensaverImage),
		COREAPI_NUMBER_FIELD(screensaver_random)
	},
	// The item for this one is switched off in the program as well, and the
	// screensaver reads the setting either way.
	{
		"screensaver_mode_text", ValueType::Bool, "osd",
		"screensaver.enable_text_info", "menu.hint_screensaver_enable_text_info",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kScreensaverOn),
		COREAPI_NUMBER_FIELD(screensaver_mode_text)
	},
	// infoicons
	{
		"mode_icons_skin", ValueType::Enum, "osd",
		"infoicons_skin", "menu.hint_infoicons_skin",
		0, 0, COREAPI_ENUM(kInfoiconsSkin), 0, NULL, false, false,
		COREAPI_CONDITIONS(kIconsOff),
		COREAPI_NUMBER_FIELD(mode_icons_skin)
	},
	{
		"mode_icons", ValueType::Enum, "osd",
		"infoicons_modeicon", "menu.hint_infoicons_modeicon",
		0, 0, COREAPI_ENUM(kInfoiconsMode), 0, NULL, false, false,
		COREAPI_CONDITIONS(kSkinNotInfoviewer),
		COREAPI_NUMBER_FIELD(mode_icons)
	},
	{
		"mode_icons_background", ValueType::Bool, "osd",
		"infoicons_background", "menu.hint_infoicons_background",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mode_icons_background)
	},
	/* Which of the two sizes the box draws its own screen at, as the mode the
	   program stores and not as a place in the list the framebuffer builds;
	   every driver builds its list in the order of the modes. Each is offered
	   where the box draws at that size.

	   The default is the one the pass that loads the settings falls back to.
	   The pass that sets the framebuffer up reads the same key with a different
	   fallback, which is a disagreement in the program and not in this row:
	   whoever changes one belongs at the other. */
	{
		"osd_resolution", ValueType::Enum, "osd",
		"colormenu.osd_resolution", "menu.hint_osd_resolution",
		0, 0, COREAPI_VALUES(kOsdResolution),
#if HAVE_ARM_HARDWARE || (HAVE_CST_HARDWARE && defined(BOXMODEL_CST_HD2))
		OSDMODE_1080,
#else
		OSDMODE_720,
#endif
		NULL, false, false, COREAPI_ALWAYS,
		COREAPI_SERVICE_FIELD(osd_resolution, askOsdResolution, tellOsdResolution)
	},
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
	{
		"screen_StartX_a_0", ValueType::Int, "osd",
		"screensetup.upperleft", NULL,
		0, 200, NULL, 0, 0, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_StartX_a_0)
	},
	{
		"screen_StartY_a_0", ValueType::Int, "osd",
		"screensetup.upperleft", NULL,
		0, 200, NULL, 0, 0, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_StartY_a_0)
	},
	{
		"screen_EndX_a_0", ValueType::Int, "osd",
		"screensetup.lowerright", NULL,
		400, 1279, NULL, 0, 1279, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_EndX_a_0)
	},
	{
		"screen_EndY_a_0", ValueType::Int, "osd",
		"screensetup.lowerright", NULL,
		400, 719, NULL, 0, 719, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_EndY_a_0)
	},
	{
		"screen_StartX_a_1", ValueType::Int, "osd",
		"screensetup.upperleft", NULL,
		0, 200, NULL, 0, 0, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_StartX_a_1)
	},
	{
		"screen_StartY_a_1", ValueType::Int, "osd",
		"screensetup.upperleft", NULL,
		0, 200, NULL, 0, 0, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_StartY_a_1)
	},
	{
		"screen_EndX_a_1", ValueType::Int, "osd",
		"screensetup.lowerright", NULL,
		400, 1919, NULL, 0, 1919, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_EndX_a_1)
	},
	{
		"screen_EndY_a_1", ValueType::Int, "osd",
		"screensetup.lowerright", NULL,
		400, 1079, NULL, 0, 1079, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_EndY_a_1)
	},
	{
		"screen_StartX_b_0", ValueType::Int, "osd",
		"screensetup.upperleft", NULL,
		0, 200, NULL, 0, 22, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_StartX_b_0)
	},
	{
		"screen_StartY_b_0", ValueType::Int, "osd",
		"screensetup.upperleft", NULL,
		0, 200, NULL, 0, 12, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_StartY_b_0)
	},
	{
		"screen_EndX_b_0", ValueType::Int, "osd",
		"screensetup.lowerright", NULL,
		400, 1279, NULL, 0, 1236, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_EndX_b_0)
	},
	{
		"screen_EndY_b_0", ValueType::Int, "osd",
		"screensetup.lowerright", NULL,
		400, 719, NULL, 0, 695, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_EndY_b_0)
	},
	{
		"screen_StartX_b_1", ValueType::Int, "osd",
		"screensetup.upperleft", NULL,
		0, 200, NULL, 0, 33, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_StartX_b_1)
	},
	{
		"screen_StartY_b_1", ValueType::Int, "osd",
		"screensetup.upperleft", NULL,
		0, 200, NULL, 0, 18, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_StartY_b_1)
	},
	{
		"screen_EndX_b_1", ValueType::Int, "osd",
		"screensetup.lowerright", NULL,
		400, 1919, NULL, 0, 1854, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_EndX_b_1)
	},
	{
		"screen_EndY_b_1", ValueType::Int, "osd",
		"screensetup.lowerright", NULL,
		400, 1079, NULL, 0, 1043, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_EndY_b_1)
	},

	/* Whether the infobar shows the module line at all. No item sets it: the
	   menu derives it from the position beside it, so writing the position
	   alone leaves this one as it was and the line does not follow. */
	{
		"show_ecm", ValueType::Bool, "osd",
		NULL, NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(show_ecm)
	},
	// Whether the infobar carries the channel description.
	{
		"infobar_show_channeldesc", ValueType::Bool, "osd",
		NULL, NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_show_channeldesc)
	},
	/* The name of the colour theme, picked from a file list rather than typed,
	   and read once at start. Its default is a migration name where the box has
	   no settings file yet and empty otherwise, which is what every box that
	   has saved once gets and what the row states. */
	{
		"theme_name", ValueType::String, "osd",
		NULL, NULL,
		0, 0, NULL, 0, 0, "", true, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(theme_name)
	},
};

} // anonymous namespace

const Descriptor *settingsTableOsd(size_t &count)
{
	count = sizeof(kOsd) / sizeof(kOsd[0]);
	return kOsd;
}

} // namespace coreapi
