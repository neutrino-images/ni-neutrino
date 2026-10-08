/*
 * settingstable_keys.cpp - key binding settings, one row per field
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
#include "boxdefaults.h"

// The remote control codes a keybinding stores, as the program names them, so
// that no default here is a number copied by hand.
#include <driver/rcinput.h>
#include <system/settings.h>

namespace coreapi
{

namespace
{

/* The keybinding section: the key, the type and the default off the line that
   loads the setting, the label and the hint off the menu table.

   Almost no default here is comparable against the program, which writes them
   as names of the remote control code. They are not copied as numbers either:
   the header that declares them is included above and each row names the same
   constant the loader names.

   Five defaults, and mbkey_cover which follows the first, are decided by the
   box model. The row states the arm every other box takes and default_fn the
   arm of the model the build is for, in the same arms the loader has: a reset
   or a read of an unset key would otherwise give another key than the box
   starts with. */

// What a keybinding may hold. Anything the box can send is a key, and the code
// that means no key at all is the floor. A long carries the second as the
// settings file does, which is what a signed read of it gives.
const long kKeyNone = (int32_t) CRCInput::RC_nokey;
const long kKeyMax = (long) CRCInput::RC_MaxRC;

long keyFavoritesDefault()
{
	return boxdefault::kFavoritesIsVideo ? (int32_t) CRCInput::RC_video : (int32_t) CRCInput::RC_favorites;
}

long keyTimeshiftDefault()
{
	if (boxdefault::kTimeshiftIsNone)
		return (int32_t) CRCInput::RC_nokey;
	if (boxdefault::kVuPlus)
		return (int32_t) CRCInput::RC_playpause;
	return (int32_t) CRCInput::RC_pause;
}

long keyTvRadioModeDefault()
{
	return boxdefault::kTvRadioKeyIsTv ? (int32_t) CRCInput::RC_tv : (int32_t) CRCInput::RC_nokey;
}

long mpKeyPauseDefault()
{
	if (boxdefault::kPlayPauseKey || boxdefault::kVuPlus)
		return (int32_t) CRCInput::RC_playpause;
	return (int32_t) CRCInput::RC_pause;
}

long mpKeyPlayDefault()
{
	return boxdefault::kPlayPauseKey ? (int32_t) CRCInput::RC_playpause : (int32_t) CRCInput::RC_play;
}

constexpr EnumValue kBouquetlistMode[] =
{
	option(SNeutrinoSettings::CHANNELLIST).label("keybindingmenu.channellist"),
	option(SNeutrinoSettings::FAVORITES).label("keybindingmenu.favorites")
};

constexpr EnumValue kRemoteHardware[] =
{
	option(0).label("keybindingmenu.remotecontrol_hardware_coolstream"),
	option(1).label("keybindingmenu.remotecontrol_hardware_dbox"),
	option(2).label("keybindingmenu.remotecontrol_hardware_philips")
};

// Nought is no blocking and no jump, which the screens say in words.
constexpr EnumValue kOffAtZero[] =
{
	option(0).label("options.off")
};

// The floor of the long key press is the value that means it is off.
constexpr EnumValue kOffAtLongPressFloor[] =
{
	option(LONGKEYPRESS_OFF).label("options.off")
};

constexpr EnumValue kLeftRightKeyTv[] =
{
	option(0).label("keybindingmenu.mode_left_right_key_tv_zap"),
	option(1).label("keybindingmenu.mode_left_right_key_tv_vzap"),
	option(2).label("keybindingmenu.mode_left_right_key_tv_volume"),
	option(3).label("keybindingmenu.mode_left_right_key_tv_infobar")
};

constexpr Descriptor kSettings[] =
{
	keyRow("key_tvradio_mode")
		.section("keybindings")
		.label("keybindingmenu.tvradiomode")
		.hint("menu.hint_key_tvradiomode")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.defaultFrom(keyTvRadioModeDefault)
		.field(COREAPI_NUMBER_FIELD(key_tvradio_mode)),
	keyRow("key_power_off")
		.section("keybindings")
		.label("keybindingmenu.poweroff")
		.hint("menu.hint_key_poweroff")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_standby)
		.field(COREAPI_NUMBER_FIELD(key_power_off)),
	keyRow("key_standby_off_add")
		.section("keybindings")
		.label("keybindingmenu.standbyoff_add")
		.hint("menu.hint_key_standbyoff_add")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_ok)
		.field(COREAPI_NUMBER_FIELD(key_standby_off_add)),
	keyRow("key_favorites")
		.section("keybindings")
		.label("keybindingmenu.favorites")
		.hint("menu.hint_key_favorites")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_favorites)
		.defaultFrom(keyFavoritesDefault)
		.field(COREAPI_NUMBER_FIELD(key_favorites)),
	keyRow("key_channelList_pageup")
		.section("keybindings")
		.label("keybindingmenu.pageup")
		.hint("menu.hint_key_pageup")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_page_up)
		.field(COREAPI_NUMBER_FIELD(key_pageup)),
	keyRow("key_channelList_pagedown")
		.section("keybindings")
		.label("keybindingmenu.pagedown")
		.hint("menu.hint_key_pagedown")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_page_down)
		.field(COREAPI_NUMBER_FIELD(key_pagedown)),
	keyRow("key_volumeup")
		.section("keybindings")
		.label("keybindingmenu.volumeup")
		.hint("menu.hint_key_volumeup")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_plus)
		.field(COREAPI_NUMBER_FIELD(key_volumeup)),
	keyRow("key_volumedown")
		.section("keybindings")
		.label("keybindingmenu.volumedown")
		.hint("menu.hint_key_volumedown")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_minus)
		.field(COREAPI_NUMBER_FIELD(key_volumedown)),
	keyRow("key_list_start")
		.section("keybindings")
		.label("extra.key_list_start")
		.hint("menu.hint_key_list_start")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_list_start)),
	keyRow("key_list_end")
		.section("keybindings")
		.label("extra.key_list_end")
		.hint("menu.hint_key_list_end")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_list_end)),
	keyRow("key_channelList_cancel")
		.section("keybindings")
		.label("keybindingmenu.cancel")
		.hint("menu.hint_key_cancel")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_home)
		.field(COREAPI_NUMBER_FIELD(key_channelList_cancel)),
	keyRow("key_channelList_sort")
		.section("keybindings")
		.label("keybindingmenu.sort")
		.hint("menu.hint_key_sort")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_blue)
		.field(COREAPI_NUMBER_FIELD(key_channelList_sort)),
	keyRow("key_channelList_addrecord")
		.section("keybindings")
		.label("keybindingmenu.addrecord")
		.hint("menu.hint_key_addrecord")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_red)
		.field(COREAPI_NUMBER_FIELD(key_channelList_addrecord)),
	keyRow("key_channelList_addremind")
		.section("keybindings")
		.label("keybindingmenu.addremind")
		.hint("menu.hint_key_addremind")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_yellow)
		.field(COREAPI_NUMBER_FIELD(key_channelList_addremind)),
	keyRow("key_bouquet_up")
		.section("keybindings")
		.label("keybindingmenu.bouquetup")
		.hint("menu.hint_key_bouquetup")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_right)
		.field(COREAPI_NUMBER_FIELD(key_bouquet_up)),
	keyRow("key_bouquet_down")
		.section("keybindings")
		.label("keybindingmenu.bouquetdown")
		.hint("menu.hint_key_bouquetdown")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_left)
		.field(COREAPI_NUMBER_FIELD(key_bouquet_down)),
	keyRow("key_current_transponder")
		.section("keybindings")
		.label("extra.key_current_transponder")
		.hint("menu.hint_key_transponder")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_current_transponder)),
	keyRow("key_quickzap_up")
		.section("keybindings")
		.label("keybindingmenu.channelup")
		.hint("menu.hint_key_channelup")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_up)
		.field(COREAPI_NUMBER_FIELD(key_quickzap_up)),
	keyRow("key_quickzap_down")
		.section("keybindings")
		.label("keybindingmenu.channeldown")
		.hint("menu.hint_key_channeldown")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_down)
		.field(COREAPI_NUMBER_FIELD(key_quickzap_down)),
	keyRow("key_subchannel_up")
		.section("keybindings")
		.label("keybindingmenu.subchannelup")
		.hint("menu.hint_key_subchannelup")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_right)
		.field(COREAPI_NUMBER_FIELD(key_subchannel_up)),
	keyRow("key_subchannel_down")
		.section("keybindings")
		.label("keybindingmenu.subchanneldown")
		.hint("menu.hint_key_subchanneldown")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_left)
		.field(COREAPI_NUMBER_FIELD(key_subchannel_down)),
	keyRow("key_zaphistory")
		.section("keybindings")
		.label("keybindingmenu.zaphistory")
		.hint("menu.hint_key_history")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_home)
		.field(COREAPI_NUMBER_FIELD(key_zaphistory)),
	keyRow("key_lastchannel")
		.section("keybindings")
		.label("keybindingmenu.lastchannel")
		.hint("menu.hint_key_lastchannel")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_0)
		.field(COREAPI_NUMBER_FIELD(key_lastchannel)),
	keyRow("mpkey.play")
		.section("keybindings")
		.label("mpkey.play")
		.hint("menu.hint_key_mpplay")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_play)
		.defaultFrom(mpKeyPlayDefault)
		.field(COREAPI_NUMBER_FIELD(mpkey_play)),
	keyRow("mpkey.pause")
		.section("keybindings")
		.label("mpkey.pause")
		.hint("menu.hint_key_mppause")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_pause)
		.defaultFrom(mpKeyPauseDefault)
		.field(COREAPI_NUMBER_FIELD(mpkey_pause)),
	keyRow("mpkey.stop")
		.section("keybindings")
		.label("mpkey.stop")
		.hint("menu.hint_key_mpstop")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_stop)
		.field(COREAPI_NUMBER_FIELD(mpkey_stop)),
	keyRow("mpkey.forward")
		.section("keybindings")
		.label("mpkey.forward")
		.hint("menu.hint_key_mpforward")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_forward)
		.field(COREAPI_NUMBER_FIELD(mpkey_forward)),
	keyRow("mpkey.rewind")
		.section("keybindings")
		.label("mpkey.rewind")
		.hint("menu.hint_key_mprewind")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_rewind)
		.field(COREAPI_NUMBER_FIELD(mpkey_rewind)),
	keyRow("mpkey.audio")
		.section("keybindings")
		.label("mpkey.audio")
		.hint("menu.hint_key_mpaudio")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_green)
		.field(COREAPI_NUMBER_FIELD(mpkey_audio)),
	keyRow("mpkey.subtitle")
		.section("keybindings")
		.label("mpkey.subtitle")
		.hint("menu.hint_key_mpsubtitle")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_sub)
		.field(COREAPI_NUMBER_FIELD(mpkey_subtitle)),
	keyRow("mpkey.time")
		.section("keybindings")
		.label("mpkey.time")
		.hint("menu.hint_key_mptime")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_timeshift)
		.field(COREAPI_NUMBER_FIELD(mpkey_time)),
	keyRow("mpkey.bookmark")
		.section("keybindings")
		.label("mpkey.bookmark")
		.hint("menu.hint_key_mpbookmark")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_yellow)
		.field(COREAPI_NUMBER_FIELD(mpkey_bookmark)),
	keyRow("mpkey.goto")
		.section("keybindings")
		.label("mpkey.goto")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(mpkey_goto)),
	keyRow("mpkey.next_repeat_mode")
		.section("keybindings")
		.label("mpkey.next_repeat_mode")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(mpkey_next_repeat_mode)),
	keyRow("mpkey.plugin")
		.section("keybindings")
		.label("mpkey.plugin")
		.hint("menu.hint_key_mpplugin")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(mpkey_plugin)),
	keyRow("key_timeshift")
		.section("keybindings")
		.label("extra.key_timeshift")
		.hint("menu.hint_key_timeshift")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_pause)
		.defaultFrom(keyTimeshiftDefault)
		.field(COREAPI_NUMBER_FIELD(key_timeshift)),
	keyRow("key_unlock")
		.section("keybindings")
		.label("extra.key_unlock")
		.hint("menu.hint_key_unlock")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_setup)
		.field(COREAPI_NUMBER_FIELD(key_unlock)),
	keyRow("key_help")
		.section("keybindings")
		.label("extra.key_help")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_help)
		.field(COREAPI_NUMBER_FIELD(key_help)),
	keyRow("key_next43mode")
		.section("keybindings")
		.label("extra.key_next43mode")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_next43mode)),
	keyRow("key_switchformat")
		.section("keybindings")
		.label("extra.key_switchformat")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_switchformat)),
	keyRow("key_screenshot")
		.section("keybindings")
		.label("extra.key_screenshot")
		.hint("menu.hint_key_screenshot")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_games)
		.field(COREAPI_NUMBER_FIELD(key_screenshot)),
	keyRow("key_sleep")
		.section("keybindings")
		.label("extra.key_sleep")
		.hint("menu.hint_key_sleep")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_sleep)
		.field(COREAPI_NUMBER_FIELD(key_sleep)),
#if ENABLE_PIP
	keyRow("key_pip_close")
		.section("keybindings")
		.label("extra.key_pip_close")
		.hint("menu.hint_key_pip_close")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_prev)
		.field(COREAPI_NUMBER_FIELD(key_pip_close)),
	keyRow("key_pip_close_avinput")
		.section("keybindings")
		.label("extra.key_pip_close_avinput")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_pip_close_avinput)),
	keyRow("key_pip_rotate_cw")
		.section("keybindings")
		.label("extra.key_pip_rotate_cw")
		.hint("menu.hint_key_pip_rotate_cw")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_pip_rotate_cw)),
	keyRow("key_pip_rotate_ccw")
		.section("keybindings")
		.label("extra.key_pip_rotate_ccw")
		.hint("menu.hint_key_pip_rotate_ccw")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_pip_rotate_ccw)),
	keyRow("key_pip_setup")
		.section("keybindings")
		.label("extra.key_pip_setup")
		.hint("menu.hint_key_pip_setup")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_pip_setup)),
	keyRow("key_pip_swap")
		.section("keybindings")
		.label("extra.key_pip_swap")
		.hint("menu.hint_key_pip_close")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_next)
		.field(COREAPI_NUMBER_FIELD(key_pip_swap)),
#endif
	// The remote control's own format key, so only where it has one.
	boolRow("key_format_mode_active")
		.section("keybindings")
		.label("extra.key_format_mode")
		.hint("menu.hint_key_format_mode_active")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD_ON(key_format_mode_active, hasFormatButton, NULL)),
	boolRow("key_pic_mode_active")
		.section("keybindings")
		.label("extra.key_pic_mode")
		.hint("menu.hint_key_pic_mode_active")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(key_pic_mode_active)),
	boolRow("key_pic_size_active")
		.section("keybindings")
		.label("extra.key_pic_size")
		.hint("menu.hint_key_pic_size_active")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(key_pic_size_active)),
	keyRow("key_record")
		.section("keybindings")
		.label("extra.key_record")
		.hint("menu.hint_key_record")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_record)
		.field(COREAPI_NUMBER_FIELD(key_record)),
	keyRow("mbkey.copy_onefile")
		.section("keybindings")
		.label("mbkey.copy_onefile")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(mbkey_copy_onefile)),
	keyRow("mbkey.copy_several")
		.section("keybindings")
		.label("mbkey.copy_several")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(mbkey_copy_several)),
	keyRow("mbkey.cut")
		.section("keybindings")
		.label("mbkey.cut")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(mbkey_cut)),
	keyRow("mbkey.truncate")
		.section("keybindings")
		.label("mbkey.truncate")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(mbkey_truncate)),
	keyRow("mbkey.toggle_view_cw")
		.section("keybindings")
		.label("mbkey.toggle_view_cw")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_right)
		.field(COREAPI_NUMBER_FIELD(mbkey_toggle_view_cw)),
	keyRow("mbkey.toggle_view_ccw")
		.section("keybindings")
		.label("mbkey.toggle_view_ccw")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_left)
		.field(COREAPI_NUMBER_FIELD(mbkey_toggle_view_ccw)),
	keyRow("mbkey.cover")
		.section("keybindings")
		.label("mbkey.cover")
		.hint("menu.hint_mbkey_cover")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_favorites)
		.defaultFrom(keyFavoritesDefault)
		.field(COREAPI_NUMBER_FIELD(mbkey_cover)),
	enumRow("bouquetlist_mode")
		.section("keybindings")
		.label("keybindingmenu.bouquetlist_mode")
		.defaultValue(0)
		.values(kBouquetlistMode)
		.field(COREAPI_NUMBER_FIELD(bouquetlist_mode)),
	// The receiver is programmed for the remote control when the input driver is
	// built and again whenever this changes.
	enumRow("remote_control_hardware")
		.section("keybindings")
		.label("keybindingmenu.remotecontrol_hardware")
		.hint("menu.hint_key_hardware")
		.defaultValue(0)
		.values(kRemoteHardware)
		.field(COREAPI_NUMBER_FIELD_ON(remote_control_hardware, canSelectRemote, NULL)),
	// Milliseconds, and zero is off.
	intRow("repeat_genericblocker")
		.section("keybindings")
		.label("keybindingmenu.repeatblockgeneric")
		.hint("menu.hint_key_repeatblockgeneric")
		.range(0, 999)
		.defaultValue(100)
		.unit("unit.short.millisecond")
		.values(kOffAtZero)
		.field(COREAPI_NUMBER_FIELD(repeat_genericblocker)),
	boolRow("sms_channel")
		.section("keybindings")
		.label("extra.sms_channel")
		.hint("menu.hint_sms_channel")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(sms_channel)),
	boolRow("sms_movie")
		.section("keybindings")
		.label("extra.sms_movie")
		.hint("menu.hint_sms_movie")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(sms_movie)),
	enumRow("mode_left_right_key_tv")
		.section("keybindings")
		.label("keybindingmenu.mode_left_right_key_tv")
		.hint("menu.hint_key_right")
		.defaultValue(1)
		.values(kLeftRightKeyTv)
		.field(COREAPI_NUMBER_FIELD(mode_left_right_key_tv)),
	// Minutes, and zero is off.
	intRow("movieplayer_bisection_jump")
		.section("keybindings")
		.label("movieplayer.bisection_jump")
		.hint("menu.hint_movieplayer_bisection_jump")
		.range(0, 10)
		.defaultValue(5)
		.unit("unit.short.minute")
		.values(kOffAtZero)
		.field(COREAPI_NUMBER_FIELD(movieplayer_bisection_jump)),
	/* The three below are loaded and saved beside the keybindings, which is why
	   they are here and not with the other options: the program loads them in
	   the same pass. */
	boolRow("menu_left_exit")
		.section("keybindings")
		.label("extra.menu_left_exit")
		.hint("menu.hint_key_left_exit")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(menu_left_exit)),
	// Milliseconds, and zero is off.
	intRow("repeat_blocker")
		.section("keybindings")
		.label("keybindingmenu.repeatblock")
		.hint("menu.hint_key_repeatblock")
		.range(0, 999)
		.defaultValue(450)
		.unit("unit.short.millisecond")
		.values(kOffAtZero)
		.field(COREAPI_NUMBER_FIELD(repeat_blocker)),
	/* Milliseconds again, and here the floor is what off means rather than
	   zero: anything above it is a duration and the value itself is the only
	   one that is not. */
	intRow("longkeypress_duration")
		.section("keybindings")
		.label("keybindingmenu.longkeypress_duration")
		.hint("menu.hint_longkeypress_duration")
		.range(LONGKEYPRESS_OFF, 9999)
		.defaultValue(LONGKEYPRESS_OFF)
		.unit("unit.short.millisecond")
		.values(kOffAtLongPressFloor)
		.field(COREAPI_NUMBER_FIELD(longkeypress_duration)),
};

} // anonymous namespace

const Descriptor *settingsTableKeys(size_t &count)
{
	count = sizeof(kSettings) / sizeof(kSettings[0]);
	return kSettings;
}

} // namespace coreapi
