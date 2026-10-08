/*
 * settingstable_elements.cpp - the settings the program keeps in arrays, one row to each element
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
#include "videomodes.h"

namespace coreapi
{

namespace
{

/* An array of the settings struct is not one setting but as many as it has
   elements, each stored under a key of its own, so each element is a row. The
   key is the one the settings file gives it, written out here rather than built
   from the index, so what a person searches for is what is in the file. */

/* Whether this box draws the video mode the settings file numbers i, and offers
   it to be enabled: the box has an HDMI output to enable it on, and its list of
   modes names it. Spelt as functions of the number because a row carries a test
   and not an argument. */
template <size_t I>
bool videoModeOffered()
{
	return hasHdmi() && videoModeDrawn(I);
}

/* The automatic switching is offered on the first family of box only, and for
   the modes it can switch between, which leaves out the last of the numbered
   ones. */
template <size_t I>
bool autoModeOffered()
{
#if COREAPI_VIDEOMODES == COREAPI_VIDEOMODES_CST_HD2
	return I < VIDEOMENU_VIDEOMODE_OPTION_COUNT - 1 && hasHdmi() && videoModeDrawn(I);
#else
	return false;
#endif
}

/* What the personalize screen lets a menu entry be: out of the menu, in it, or in
   it behind the PIN. */
constexpr EnumValue kPersonalizeMode[] =
{
	option(0).label("personalize.notvisible"),
	option(1).label("personalize.visible"),
	option(2).label("personalize.pin")
};

/* The entries that open a whole menu are offered only as protected or not, and
   nought and two are the two the screen stores. */
constexpr EnumValue kPersonalizeAccess[] =
{
	option(0).label("personalize.notprotected"),
	option(2).label("personalize.pinprotect")
};

// A button of the user menu is on or off.
constexpr EnumValue kPersonalizeActive[] =
{
	option(0).label("personalize.disabled"),
	option(1).label("personalize.enabled")
};

/* The key a feature is on, as the position of that key in the screen's own table
   of five, which is also what the code indexes by: no number outside these five
   is one it can be given. */
constexpr EnumValue kPersonalizeFeatKey[] =
{
	option(0).label("personalize.button_red"),
	option(1).label("personalize.button_green"),
	option(2).label("personalize.button_yellow"),
	option(3).label("personalize.button_blue"),
	option(4).label("personalize.button_auto")
};

// A timeout of nought is no timeout, which the screen words as off.
constexpr EnumValue kTimingOff[] =
{
	option(0).label("timing.off")
};

/* The infobar's own timeouts take minus one for the box's choice. Nought is no
   timeout here as well, but a named number may not sit above the floor, so only
   the floor is worded. */
constexpr EnumValue kHandlingInfobarAuto[] =
{
	option(-1).label("timing.off_auto")
};

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
// The clock the module is driven at, in the steps the screen words.
constexpr EnumValue kCiClock[] =
{
	option(6).label("ci.clock_normal"),
	option(7).label("ci.clock_high"),
#if BOXMODEL_VUPLUS_ALL
	option(12).label("ci.clock_extra_high"),
#endif
};
#endif

// What the front display's second line shows.
constexpr EnumValue kLcdStatusline[] =
{
	option(0).label("lcdmenu.statusline.playtime"),
	option(1).label("lcdmenu.statusline.volume"),
	option(2).label("options.off")
};

/* The three brightnesses of the front display are the one thing the applier test
   holds pending: the screen edits each through a copy of its own and writes it into
   the array when the item is focused, so applying one of these rows would apply what
   the copy last held and not what a write left. The test names them, with the
   stream that takes the screen over (test_settingsappliers.cpp). */
constexpr Descriptor kElements[] =
{
	boolRow("personalize_pinstatus")
		.section("misc")
		.label("personalize.pin_in_use")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_PINSTATUS)),
	boolRow("personalize_bluebutton")
		.section("misc")
		.label("usermenu.button_blue")
		.defaultValue(1)
		.values(kPersonalizeActive)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_BLUE_BUTTON)),
	boolRow("personalize_yellowbutton")
		.section("misc")
		.label("usermenu.button_yellow")
		.defaultValue(1)
		.values(kPersonalizeActive)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_YELLOW_BUTTON)),
	boolRow("personalize_greenbutton")
		.section("misc")
		.label("usermenu.button_green")
		.defaultValue(1)
		.values(kPersonalizeActive)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_GREEN_BUTTON)),
	boolRow("personalize_redbutton")
		.section("misc")
		.label("usermenu.button_red")
		.defaultValue(1)
		.values(kPersonalizeActive)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_RED_BUTTON)),
	enumRow("personalize_tv_mode")
		.section("misc")
		.defaultValue(0)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_TV_MODE)),
	enumRow("personalize_tv_radio_mode")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_TV_RADIO_MODE)),
	enumRow("personalize_radio_mode")
		.section("misc")
		.defaultValue(0)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_RADIO_MODE)),
	enumRow("personalize_timer")
		.section("misc")
		.defaultValue(0)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_TIMER)),
	enumRow("personalize_media")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_MEDIA)),
	enumRow("personalize_games")
		.section("misc")
		.defaultValue(0)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_GAMES)),
	enumRow("personalize_tools")
		.section("misc")
		.defaultValue(0)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_TOOLS)),
	enumRow("personalize_avinput")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_AVINPUT)),
#if ENABLE_PIP
	enumRow("personalize_avinput_pip")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_AVINPUT_PIP)),
#endif
	enumRow("personalize_scripts")
		.section("misc")
		.defaultValue(0)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_SCRIPTS)),
	enumRow("personalize_lua")
		.section("misc")
		.defaultValue(0)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_LUA)),
	enumRow("personalize_settings")
		.section("misc")
		.defaultValue(0)
		.values(kPersonalizeAccess)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_SETTINGS)),
	enumRow("personalize_service")
		.section("misc")
		.defaultValue(0)
		.values(kPersonalizeAccess)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_SERVICE)),
	enumRow("personalize_sleeptimer")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_SLEEPTIMER)),
	enumRow("personalize_standby")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_STANDBY)),
	enumRow("personalize_reboot")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_REBOOT)),
	enumRow("personalize_shutdown")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_SHUTDOWN)),
	enumRow("personalize_poweroff_menu")
		.section("misc")
		.defaultValue(0)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_POWEROFF_MENU)),
	enumRow("personalize_blank_screen")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_BLANK_SCREEN)),
	enumRow("personalize_infomenu_main")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_INFOMENU)),
	enumRow("personalize_cisettings_main")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MAIN_CISETTINGS)),
	enumRow("personalize_settingsmager")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSET_SETTINGS_MANAGER)),
	enumRow("personalize_video")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSET_VIDEO)),
	enumRow("personalize_audio")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSET_AUDIO)),
	enumRow("personalize_parentallock")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSET_PARENTALLOCK)),
	enumRow("personalize_network")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSET_NETWORK)),
	enumRow("personalize_recording")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSET_RECORDING)),
	enumRow("personalize_osdlang")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSET_OSDLANG)),
	enumRow("personalize_osd")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSET_OSD)),
	enumRow("personalize_vfd")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSET_VFD)),
	enumRow("personalize_drives")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSET_DRIVES)),
	enumRow("personalize_cisettings_settings")
		.section("misc")
		.defaultValue(0)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSET_CISETTINGS)),
	enumRow("personalize_keybindings")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSET_KEYBINDING)),
	enumRow("personalize_mediaplayer")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSET_MEDIAPLAYER)),
	enumRow("personalize_misc")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSET_MISC)),
	enumRow("personalize_tuner")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSER_TUNER)),
	enumRow("personalize_scants")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSER_SCANTS)),
	enumRow("personalize_reload_channels")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSER_RELOAD_CHANNELS)),
	enumRow("personalize_bouquet_edit")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSER_BOUQUET_EDIT)),
	enumRow("personalize_reset_channels")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSER_RESET_CHANNELS)),
	enumRow("personalize_daemon_control")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSER_DAEMON_CONTROL)),
	enumRow("personalize_camd_control")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSER_CAMD_CONTROL)),
	enumRow("personalize_restart")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSER_RESTART)),
	enumRow("personalize_restart_tuner")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSER_RESTART_TUNER)),
	enumRow("personalize_reload_plugins")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSER_RELOAD_PLUGINS)),
	enumRow("personalize_infomenu_service")
		.section("misc")
		.defaultValue(0)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSER_SERVICE_INFOMENU)),
	enumRow("personalize_softupdate")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MSER_SOFTUPDATE)),
	enumRow("personalize_media_menu")
		.section("misc")
		.defaultValue(0)
		.values(kPersonalizeAccess)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MEDIA_MENU)),
	enumRow("personalize_media_audio")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MEDIA_AUDIO)),
	enumRow("personalize_media_intetplay")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MEDIA_INETPLAY)),
	enumRow("personalize_media_movieplayer")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MEDIA_MPLAYER)),
	enumRow("personalize_media_pviewer")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MEDIA_PVIEWER)),
	enumRow("personalize_media_upnp")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MEDIA_UPNP)),
	enumRow("personalize_mplayer_mbrowser")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MPLAYER_MBROWSER)),
	enumRow("personalize_mplayer_fileplay_video")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MPLAYER_FILEPLAY_VIDEO)),
	enumRow("personalize_mplayer_fileplay_audio")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeMode)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_MPLAYER_FILEPLAY_AUDIO)),
	enumRow("personalize_feat_key_fav")
		.section("misc")
		.defaultValue(1)
		.values(kPersonalizeFeatKey)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_FEAT_KEY_FAVORIT)),
	enumRow("personalize_feat_key_timerlist")
		.section("misc")
		.defaultValue(2)
		.values(kPersonalizeFeatKey)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_FEAT_KEY_TIMERLIST)),
	enumRow("personalize_feat_key_vtxt")
		.section("misc")
		.defaultValue(3)
		.values(kPersonalizeFeatKey)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_FEAT_KEY_VTXT)),
	enumRow("personalize_feat_key_rclock")
		.section("misc")
		.defaultValue(4)
		.values(kPersonalizeFeatKey)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_FEAT_KEY_RC_LOCK)),
	boolRow("personalize_usermenu_show_cancel")
		.section("misc")
		.label("personalize.usermenu_show_cancel")
		.defaultValue(1)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_UMENU_SHOW_CANCEL)),
	boolRow("personalize_usermenu_plugin_type_games")
		.section("misc")
		.label("mainmenu.games")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_UMENU_PLUGIN_TYPE_GAMES)),
	boolRow("personalize_usermenu_plugin_type_tools")
		.section("misc")
		.label("mainmenu.tools")
		.defaultValue(1)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_UMENU_PLUGIN_TYPE_TOOLS)),
	boolRow("personalize_usermenu_plugin_type_scripts")
		.section("misc")
		.label("mainmenu.scripts")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_UMENU_PLUGIN_TYPE_SCRIPTS)),
	boolRow("personalize_usermenu_plugin_type_lua")
		.section("misc")
		.label("mainmenu.lua")
		.defaultValue(1)
		.field(COREAPI_ELEMENT_FIELD(personalize, SNeutrinoSettings::P_UMENU_PLUGIN_TYPE_LUA)),
	intRow("timing.menu")
		.section("osd")
		.label("timing.menu")
		.hint("menu.hint_osd_timing")
		.range(0, 240)
		.defaultValue(180)
		.unit("unit.short.second")
		.values(kTimingOff)
		.field(COREAPI_ELEMENT_FIELD(timing, SNeutrinoSettings::TIMING_MENU)),
	intRow("timing.chanlist")
		.section("osd")
		.label("timing.chanlist")
		.hint("menu.hint_osd_timing")
		.range(0, 240)
		.defaultValue(180)
		.unit("unit.short.second")
		.values(kTimingOff)
		.field(COREAPI_ELEMENT_FIELD(timing, SNeutrinoSettings::TIMING_CHANLIST)),
	intRow("timing.epg")
		.section("osd")
		.label("timing.epg")
		.hint("menu.hint_osd_timing")
		.range(0, 240)
		.defaultValue(180)
		.unit("unit.short.second")
		.values(kTimingOff)
		.field(COREAPI_ELEMENT_FIELD(timing, SNeutrinoSettings::TIMING_EPG)),
	intRow("timing.volumebar")
		.section("osd")
		.label("timing.volumebar")
		.hint("menu.hint_osd_timing")
		.range(0, 240)
		.defaultValue(3)
		.unit("unit.short.second")
		.values(kTimingOff)
		.field(COREAPI_ELEMENT_FIELD(timing, SNeutrinoSettings::TIMING_VOLUMEBAR)),
	intRow("timing.filebrowser")
		.section("osd")
		.label("timing.filebrowser")
		.hint("menu.hint_osd_timing")
		.range(0, 240)
		.defaultValue(180)
		.unit("unit.short.second")
		.values(kTimingOff)
		.field(COREAPI_ELEMENT_FIELD(timing, SNeutrinoSettings::TIMING_FILEBROWSER)),
	intRow("timing.numericzap")
		.section("osd")
		.label("timing.numericzap")
		.hint("menu.hint_osd_timing")
		.range(0, 240)
		.defaultValue(3)
		.unit("unit.short.second")
		.values(kTimingOff)
		.field(COREAPI_ELEMENT_FIELD(timing, SNeutrinoSettings::TIMING_NUMERICZAP)),
	intRow("timing.popup_messages")
		.section("osd")
		.label("timing.popup_messages")
		.hint("menu.hint_osd_timing")
		.range(0, 240)
		.defaultValue(6)
		.unit("unit.short.second")
		.values(kTimingOff)
		.field(COREAPI_ELEMENT_FIELD(timing, SNeutrinoSettings::TIMING_POPUP_MESSAGES)),
	intRow("timing.static_messages")
		.section("osd")
		.label("timing.static_messages")
		.hint("menu.hint_timeouts_static_messages")
		.range(0, 240)
		.defaultValue(180)
		.unit("unit.short.second")
		.values(kTimingOff)
		.field(COREAPI_ELEMENT_FIELD(timing, SNeutrinoSettings::TIMING_STATIC_MESSAGES)),
	intRow("timing.infobar_tv")
		.section("osd")
		.label("timing.infobar_tv")
		.hint("menu.hint_osd_behavior_infobar")
		.range(-1, 240)
		.defaultValue(6)
		.unit("unit.short.second")
		.values(kHandlingInfobarAuto)
		.field(COREAPI_ELEMENT_FIELD(handling_infobar, SNeutrinoSettings::HANDLING_INFOBAR)),
	intRow("timing.infobar_radio")
		.section("osd")
		.label("timing.infobar_radio")
		.hint("menu.hint_osd_behavior_infobar")
		.range(-1, 240)
		.defaultValue(-1)
		.unit("unit.short.second")
		.values(kHandlingInfobarAuto)
		.field(COREAPI_ELEMENT_FIELD(handling_infobar, SNeutrinoSettings::HANDLING_INFOBAR_RADIO)),
	intRow("timing.infobar_media_audio")
		.section("osd")
		.label("timing.infobar_media_audio")
		.hint("menu.hint_osd_behavior_infobar")
		.range(-1, 240)
		.defaultValue(-1)
		.unit("unit.short.second")
		.values(kHandlingInfobarAuto)
		.field(COREAPI_ELEMENT_FIELD(handling_infobar, SNeutrinoSettings::HANDLING_INFOBAR_MEDIA_AUDIO)),
	intRow("timing.infobar_media_video")
		.section("osd")
		.label("timing.infobar_media_video")
		.hint("menu.hint_osd_behavior_infobar")
		.range(-1, 240)
		.defaultValue(6)
		.unit("unit.short.second")
		.values(kHandlingInfobarAuto)
		.field(COREAPI_ELEMENT_FIELD(handling_infobar, SNeutrinoSettings::HANDLING_INFOBAR_MEDIA_VIDEO)),
	boolRow("enabled_video_mode_0")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<0>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 0)),
	boolRow("enabled_video_mode_1")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<1>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 1)),
	boolRow("enabled_video_mode_2")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<2>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 2)),
	boolRow("enabled_video_mode_3")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<3>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 3)),
	boolRow("enabled_video_mode_4")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<4>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 4)),
	boolRow("enabled_video_mode_5")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<5>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 5)),
	boolRow("enabled_video_mode_6")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<6>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 6)),
	boolRow("enabled_video_mode_7")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<7>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 7)),
	boolRow("enabled_video_mode_8")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<8>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 8)),
	boolRow("enabled_video_mode_9")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<9>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 9)),
	boolRow("enabled_video_mode_10")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<10>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 10)),
	boolRow("enabled_video_mode_11")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<11>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 11)),
	boolRow("enabled_video_mode_12")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<12>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 12)),
	boolRow("enabled_video_mode_13")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<13>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 13)),
	boolRow("enabled_video_mode_14")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<14>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 14)),
	boolRow("enabled_video_mode_15")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<15>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 15)),
	boolRow("enabled_video_mode_16")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<16>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 16)),
	boolRow("enabled_video_mode_17")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<17>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 17)),
	boolRow("enabled_video_mode_18")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<18>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 18)),
	boolRow("enabled_video_mode_19")
		.section("video")
		.defaultValue(0)
		.availableIf(&videoModeOffered<19>)
		.field(COREAPI_ELEMENT_FIELD(enabled_video_modes, 19)),
	boolRow("enabled_auto_mode_0")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<0>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 0)),
	boolRow("enabled_auto_mode_1")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<1>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 1)),
	boolRow("enabled_auto_mode_2")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<2>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 2)),
	boolRow("enabled_auto_mode_3")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<3>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 3)),
	boolRow("enabled_auto_mode_4")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<4>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 4)),
	boolRow("enabled_auto_mode_5")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<5>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 5)),
	boolRow("enabled_auto_mode_6")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<6>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 6)),
	boolRow("enabled_auto_mode_7")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<7>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 7)),
	boolRow("enabled_auto_mode_8")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<8>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 8)),
	boolRow("enabled_auto_mode_9")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<9>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 9)),
	boolRow("enabled_auto_mode_10")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<10>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 10)),
	boolRow("enabled_auto_mode_11")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<11>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 11)),
	boolRow("enabled_auto_mode_12")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<12>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 12)),
	boolRow("enabled_auto_mode_13")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<13>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 13)),
	boolRow("enabled_auto_mode_14")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<14>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 14)),
	boolRow("enabled_auto_mode_15")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<15>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 15)),
	boolRow("enabled_auto_mode_16")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<16>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 16)),
	boolRow("enabled_auto_mode_17")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<17>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 17)),
	boolRow("enabled_auto_mode_18")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<18>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 18)),
	boolRow("enabled_auto_mode_19")
		.section("video")
		.defaultValue(1)
		.availableIf(&autoModeOffered<19>)
		.field(COREAPI_ELEMENT_FIELD(enabled_auto_modes, 19)),
	boolRow("ci_ignore_messages_0")
		.section("cam")
		.label("ci.ignore_msg")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(ci_ignore_messages, 0)),
	boolRow("ci_ignore_messages_1")
		.section("cam")
		.label("ci.ignore_msg")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(ci_ignore_messages, 1)),
	boolRow("ci_ignore_messages_2")
		.section("cam")
		.label("ci.ignore_msg")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(ci_ignore_messages, 2)),
	boolRow("ci_ignore_messages_3")
		.section("cam")
		.label("ci.ignore_msg")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(ci_ignore_messages, 3)),
	boolRow("ci_save_pincode_0")
		.section("cam")
		.label("ci.save_pincode")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(ci_save_pincode, 0)),
	boolRow("ci_save_pincode_1")
		.section("cam")
		.label("ci.save_pincode")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(ci_save_pincode, 1)),
	boolRow("ci_save_pincode_2")
		.section("cam")
		.label("ci.save_pincode")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(ci_save_pincode, 2)),
	boolRow("ci_save_pincode_3")
		.section("cam")
		.label("ci.save_pincode")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(ci_save_pincode, 3)),
	/* The pin a module is unlocked with is a credential, so it is under the section that
	   already holds credentials: a section with one in it is one no AI client may write,
	   and putting this under the module section would add that one to them. */
	textRow("ci_pincode_0")
		.section("misc")
		.defaultValue("")
		.secret()
		.field(COREAPI_ELEMENT_TEXT_FIELD(ci_pincode, 0)),
	textRow("ci_pincode_1")
		.section("misc")
		.defaultValue("")
		.secret()
		.field(COREAPI_ELEMENT_TEXT_FIELD(ci_pincode, 1)),
	textRow("ci_pincode_2")
		.section("misc")
		.defaultValue("")
		.secret()
		.field(COREAPI_ELEMENT_TEXT_FIELD(ci_pincode, 2)),
	textRow("ci_pincode_3")
		.section("misc")
		.defaultValue("")
		.secret()
		.field(COREAPI_ELEMENT_TEXT_FIELD(ci_pincode, 3)),
	boolRow("ci_op_0")
		.section("cam")
		.label("ci.op")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(ci_op, 0)),
	boolRow("ci_op_1")
		.section("cam")
		.label("ci.op")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(ci_op, 1)),
	boolRow("ci_op_2")
		.section("cam")
		.label("ci.op")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(ci_op, 2)),
	boolRow("ci_op_3")
		.section("cam")
		.label("ci.op")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(ci_op, 3)),
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	enumRow("ci_clock_0")
		.section("cam")
		.label("ci.clock")
		.defaultValue(6)
		.values(kCiClock)
		.field(COREAPI_ELEMENT_FIELD(ci_clock, 0)),
#else
	intRow("ci_clock_0")
		.section("cam")
		.label("ci.clock")
		.range(6, 12)
		.defaultValue(9)
		.field(COREAPI_ELEMENT_FIELD(ci_clock, 0)),
#endif
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	enumRow("ci_clock_1")
		.section("cam")
		.label("ci.clock")
		.defaultValue(6)
		.values(kCiClock)
		.field(COREAPI_ELEMENT_FIELD(ci_clock, 1)),
#else
	intRow("ci_clock_1")
		.section("cam")
		.label("ci.clock")
		.range(6, 12)
		.defaultValue(9)
		.field(COREAPI_ELEMENT_FIELD(ci_clock, 1)),
#endif
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	enumRow("ci_clock_2")
		.section("cam")
		.label("ci.clock")
		.defaultValue(6)
		.values(kCiClock)
		.field(COREAPI_ELEMENT_FIELD(ci_clock, 2)),
#else
	intRow("ci_clock_2")
		.section("cam")
		.label("ci.clock")
		.range(6, 12)
		.defaultValue(9)
		.field(COREAPI_ELEMENT_FIELD(ci_clock, 2)),
#endif
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	enumRow("ci_clock_3")
		.section("cam")
		.label("ci.clock")
		.defaultValue(6)
		.values(kCiClock)
		.field(COREAPI_ELEMENT_FIELD(ci_clock, 3)),
#else
	intRow("ci_clock_3")
		.section("cam")
		.label("ci.clock")
		.range(6, 12)
		.defaultValue(9)
		.field(COREAPI_ELEMENT_FIELD(ci_clock, 3)),
#endif
#if BOXMODEL_VUPLUS_ALL
	intRow("ci_rpr_0")
		.section("cam")
		.label("ci.rpr")
		.range(0, 9)
		.defaultValue(9)
		.field(COREAPI_ELEMENT_FIELD(ci_rpr, 0)),
#endif
#if BOXMODEL_VUPLUS_ALL
	intRow("ci_rpr_1")
		.section("cam")
		.label("ci.rpr")
		.range(0, 9)
		.defaultValue(9)
		.field(COREAPI_ELEMENT_FIELD(ci_rpr, 1)),
#endif
#if BOXMODEL_VUPLUS_ALL
	intRow("ci_rpr_2")
		.section("cam")
		.label("ci.rpr")
		.range(0, 9)
		.defaultValue(9)
		.field(COREAPI_ELEMENT_FIELD(ci_rpr, 2)),
#endif
#if BOXMODEL_VUPLUS_ALL
	intRow("ci_rpr_3")
		.section("cam")
		.label("ci.rpr")
		.range(0, 9)
		.defaultValue(9)
		.field(COREAPI_ELEMENT_FIELD(ci_rpr, 3)),
#endif
	intRow("lcd_brightness")
		.section("display")
		.label("lcdcontroler.brightness")
		.hint("menu.hint_vfd_brightness")
#ifdef ENABLE_LCD
		.range(0, 255)
#else
		.range(0, 15)
#endif
		.defaultValue(15)
		.availableIf(canSetPanelBrightness)
		.field(COREAPI_ELEMENT_FIELD(lcd_setting, SNeutrinoSettings::LCD_BRIGHTNESS)),
	intRow("lcd_standbybrightness")
		.section("display")
		.label("lcdcontroler.brightnessstandby")
		.hint("menu.hint_vfd_brightnessstandby")
#ifdef ENABLE_LCD
		.range(0, 255)
#else
		.range(0, 15)
#endif
		.defaultValue(5)
		.availableIf(canSetPanelBrightness)
		.field(COREAPI_ELEMENT_FIELD(lcd_setting, SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS)),
	intRow("lcd_contrast")
		.section("display")
		.range(0, 255)
		.defaultValue(15)
		.field(COREAPI_ELEMENT_FIELD(lcd_setting, SNeutrinoSettings::LCD_CONTRAST)),
	boolRow("lcd_power")
		.section("display")
		.defaultValue(1)
		.field(COREAPI_ELEMENT_FIELD(lcd_setting, SNeutrinoSettings::LCD_POWER)),
	boolRow("lcd_inverse")
		.section("display")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(lcd_setting, SNeutrinoSettings::LCD_INVERSE)),
	enumRow("lcd_show_volume")
		.section("display")
		.label("lcdmenu.statusline")
		.hint("menu.hint_vfd_statusline")
		.defaultValue(1)
		.values(kLcdStatusline)
		.field(COREAPI_ELEMENT_FIELD(lcd_setting, SNeutrinoSettings::LCD_SHOW_VOLUME)),
	boolRow("lcd_autodimm")
		.section("display")
		.defaultValue(0)
		.field(COREAPI_ELEMENT_FIELD(lcd_setting, SNeutrinoSettings::LCD_AUTODIMM)),
	intRow("lcd_deepbrightness")
		.section("display")
		.label("lcdcontroler.brightnessdeepstandby")
		.hint("menu.hint_vfd_brightnessdeepstandby")
#ifdef ENABLE_LCD
		.range(0, 255)
#else
		.range(0, 15)
#endif
		.defaultValue(5)
		.availableIf(canSetPanelBrightness)
		.field(COREAPI_ELEMENT_FIELD(lcd_setting, SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS)),
	textRow("pref_lang_0")
		.section("general")
		.label("audiomenu.pref_lang")
		.hint("menu.hint_pref_lang")
		.defaultValue("German")
		.field(COREAPI_ELEMENT_TEXT_FIELD(pref_lang, 0)),
	textRow("pref_lang_1")
		.section("general")
		.label("audiomenu.pref_lang")
		.hint("menu.hint_pref_lang")
		.defaultValue("English")
		.field(COREAPI_ELEMENT_TEXT_FIELD(pref_lang, 1)),
	textRow("pref_lang_2")
		.section("general")
		.label("audiomenu.pref_lang")
		.hint("menu.hint_pref_lang")
		.defaultValue("French")
		.field(COREAPI_ELEMENT_TEXT_FIELD(pref_lang, 2)),
	textRow("pref_subs_0")
		.section("general")
		.label("audiomenu.pref_subs")
		.hint("menu.hint_pref_subs")
		.defaultValue("German")
		.field(COREAPI_ELEMENT_TEXT_FIELD(pref_subs, 0)),
	textRow("pref_subs_1")
		.section("general")
		.label("audiomenu.pref_subs")
		.hint("menu.hint_pref_subs")
		.defaultValue("English")
		.field(COREAPI_ELEMENT_TEXT_FIELD(pref_subs, 1)),
	textRow("pref_subs_2")
		.section("general")
		.label("audiomenu.pref_subs")
		.hint("menu.hint_pref_subs")
		.defaultValue("French")
		.field(COREAPI_ELEMENT_TEXT_FIELD(pref_subs, 2)),
	textRow("mode_icons_flag0")
		.section("osd")
		.label("infoicons_flag_name0")
		.hint("menu.hint_infoicons_flag_name0")
		.defaultValue("/tmp/tuxmail.new")
		.field(COREAPI_ELEMENT_TEXT_FIELD(mode_icons_flag, 0)),
	textRow("mode_icons_flag1")
		.section("osd")
		.label("infoicons_flag_name1")
		.hint("menu.hint_infoicons_flag_name1")
		.defaultValue("/var/etc/.call")
		.field(COREAPI_ELEMENT_TEXT_FIELD(mode_icons_flag, 1)),
	textRow("mode_icons_flag2")
		.section("osd")
		.label("infoicons_flag_name2")
		.hint("menu.hint_infoicons_flag_name2")
		.defaultValue("/var/etc/.srv")
		.field(COREAPI_ELEMENT_TEXT_FIELD(mode_icons_flag, 2)),
	textRow("mode_icons_flag3")
		.section("osd")
		.label("infoicons_flag_name3")
		.hint("menu.hint_infoicons_flag_name3")
		.defaultValue("/var/etc/.card")
		.field(COREAPI_ELEMENT_TEXT_FIELD(mode_icons_flag, 3)),
	textRow("mode_icons_flag4")
		.section("osd")
		.label("infoicons_flag_name4")
		.hint("menu.hint_infoicons_flag_name4")
		.defaultValue("/var/etc/.update")
		.field(COREAPI_ELEMENT_TEXT_FIELD(mode_icons_flag, 4)),
	textRow("mode_icons_flag5")
		.section("osd")
		.label("infoicons_flag_name5")
		.hint("menu.hint_infoicons_flag_name5")
		.defaultValue("")
		.field(COREAPI_ELEMENT_TEXT_FIELD(mode_icons_flag, 5)),
	textRow("mode_icons_flag6")
		.section("osd")
		.label("infoicons_flag_name6")
		.hint("menu.hint_infoicons_flag_name6")
		.defaultValue("")
		.field(COREAPI_ELEMENT_TEXT_FIELD(mode_icons_flag, 6)),
	textRow("mode_icons_flag7")
		.section("osd")
		.label("infoicons_flag_name7")
		.hint("menu.hint_infoicons_flag_name7")
		.defaultValue("")
		.field(COREAPI_ELEMENT_TEXT_FIELD(mode_icons_flag, 7)),
#if ENABLE_QUADPIP
	textRow("quadpip_channel_window_0")
		.section("video")
		.defaultValue("-")
		.field(COREAPI_ELEMENT_TEXT_FIELD(quadpip_channel_window, 0)),
	textRow("quadpip_channel_id_window_0")
		.section("video")
		.defaultValue("0")
		.field(COREAPI_ELEMENT_CHANNEL_ID_FIELD(quadpip_channel_id_window, 0)),
	textRow("quadpip_channel_window_1")
		.section("video")
		.defaultValue("-")
		.field(COREAPI_ELEMENT_TEXT_FIELD(quadpip_channel_window, 1)),
	textRow("quadpip_channel_id_window_1")
		.section("video")
		.defaultValue("0")
		.field(COREAPI_ELEMENT_CHANNEL_ID_FIELD(quadpip_channel_id_window, 1)),
	textRow("quadpip_channel_window_2")
		.section("video")
		.defaultValue("-")
		.field(COREAPI_ELEMENT_TEXT_FIELD(quadpip_channel_window, 2)),
	textRow("quadpip_channel_id_window_2")
		.section("video")
		.defaultValue("0")
		.field(COREAPI_ELEMENT_CHANNEL_ID_FIELD(quadpip_channel_id_window, 2)),
	textRow("quadpip_channel_window_3")
		.section("video")
		.defaultValue("-")
		.field(COREAPI_ELEMENT_TEXT_FIELD(quadpip_channel_window, 3)),
	textRow("quadpip_channel_id_window_3")
		.section("video")
		.defaultValue("0")
		.field(COREAPI_ELEMENT_CHANNEL_ID_FIELD(quadpip_channel_id_window, 3)),
#endif
};

} // anonymous namespace

const Descriptor *settingsTableElements(size_t &count)
{
	count = sizeof(kElements) / sizeof(kElements[0]);
	return kElements;
}

} // namespace coreapi
