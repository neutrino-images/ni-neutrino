/*
 * settingstable_player.cpp - player settings, one row per field
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
#include <system/settings.h>

namespace coreapi
{

namespace
{

/* What the three players offer: the audio player and its internet radio, the
   movie player, and the picture viewer. Some of the flags are worded as a no and
   a yes rather than an off and an on; those rows carry the two words and the
   values are the two a flag carries either way. */

constexpr EnumValue kNoYes[] =
{
	option(0).label("messagebox.no"),
	option(1).label("messagebox.yes")
};

constexpr EnumValue kDisplayOrder[] =
{
	option(ARTIST_TITLE).label("audioplayer.artist_title"),
	option(TITLE_ARTIST).label("audioplayer.title_artist")
};

constexpr EnumValue kPicviewerScaling[] =
{
	option(PICVIEWER_SCALING_SIMPLE).label("pictureviewer.resize.simple"),
	option(PICVIEWER_SCALING_COLOR).label("pictureviewer.resize.color_average"),
	option(PICVIEWER_SCALING_NONE).label("pictureviewer.resize.none")
};

long displayPlaytimeDefault()
{
	return hasNumericPanel() ? 1 : 0;
}

constexpr Descriptor kSettings[] =
{
	enumRow("audioplayer_display")
		.section("player")
		.label("audioplayer.display_order")
		.hint("menu.hint_audioplayer_order")
		.defaultValue(0)
		.values(kDisplayOrder)
		.field(COREAPI_NUMBER_FIELD(audioplayer_display)),
	boolRow("audioplayer_follow")
		.section("player")
		.label("audioplayer.follow")
		.hint("menu.hint_audioplayer_follow")
		.defaultValue(0)
		.values(kNoYes)
		.field(COREAPI_NUMBER_FIELD(audioplayer_follow)),
	boolRow("audioplayer_select_title_by_name")
		.section("player")
		.label("audioplayer.select_title_by_name")
		.hint("menu.hint_audioplayer_title")
		.defaultValue(0)
		.values(kNoYes)
		.field(COREAPI_NUMBER_FIELD(audioplayer_select_title_by_name)),
	boolRow("audioplayer_repeat_on")
		.section("player")
		.label("audioplayer.repeat_on")
		.hint("menu.hint_audioplayer_repeat")
		.defaultValue(0)
		.values(kNoYes)
		.field(COREAPI_NUMBER_FIELD(audioplayer_repeat_on)),
	boolRow("audioplayer_show_playlist")
		.section("player")
		.label("audioplayer.show_playlist")
		.hint("menu.hint_audioplayer_playlist")
		.defaultValue(1)
		.values(kNoYes)
		.field(COREAPI_NUMBER_FIELD(audioplayer_show_playlist)),
	boolRow("audioplayer_cover_as_screensaver")
		.section("player")
		.label("audioplayer.cover_as_screensaver")
		.hint("menu.hint_audioplayer_cover_as_screensaver")
		.defaultValue(1)
		.values(kNoYes)
		.field(COREAPI_NUMBER_FIELD(audioplayer_cover_as_screensaver)),
	boolRow("audioplayer_highprio")
		.section("player")
		.label("audioplayer.highprio")
		.hint("menu.hint_audioplayer_highprio")
		.defaultValue(0)
		.values(kNoYes)
		.field(COREAPI_NUMBER_FIELD(audioplayer_highprio)),
	/* No menu offers this one in this build, and the value is read while a
	   track plays all the same. The label and the words come off the item the
	   build leaves out, which is the only statement of them there is. */
	boolRow("spectrum")
		.section("player")
		.label("audioplayer.spectrum")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(spectrum)),
	textRow("network_nfs_audioplayerdir")
		.section("player")
		.label("audioplayer.defdir")
		.hint("menu.hint_audioplayer_defdir")
		.defaultValue(TARGET_ROOT "/media/sda1/music")
		.text(kRuleDirectoryExists)
		.field(COREAPI_TEXT_FIELD(network_nfs_audioplayerdir)),
	boolRow("inetradio_autostart")
		.section("player")
		.label("inetradio.autostart")
		.hint("menu.hint_inetradio_autostart")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(inetradio_autostart)),
	boolRow("audioplayer_enable_sc_metadata")
		.section("player")
		.label("audioplayer.enable_sc_metadata")
		.hint("menu.hint_audioplayer_sc_metadata")
		.defaultValue(1)
		.values(kNoYes)
		.field(COREAPI_NUMBER_FIELD(audioplayer_enable_sc_metadata)),
	textRow("network_nfs_streamripperdir")
		.section("player")
		.label("audioplayer.streamripper_dir")
		.hint("menu.hint_audioplayer_streamripper_dir")
		.defaultValue(TARGET_ROOT "/media/sda1/music/streamripper")
		.text(kRuleDirectoryExists)
		.field(COREAPI_TEXT_FIELD(network_nfs_streamripperdir)),
	/* The default is the box's own panel: the loader asks the hardware whether
	   the display is a numeric one. Nought is what every other box gets. A panel
	   too narrow for the time has no use for it. */
	boolRow("movieplayer_display_playtime")
		.section("player")
		.label("movieplayer.display_playtime")
		.hint("menu.hint_movieplayer_display_playtime")
		.defaultValue(0)
		.defaultFrom(displayPlaytimeDefault)
		.field(COREAPI_NUMBER_FIELD_ON(movieplayer_display_playtime, displayFitsPlaytime, NULL)),
	boolRow("movieplayer_timeosd_while_searching")
		.section("player")
		.label("movieplayer.timeosd_while_searching")
		.hint("menu.hint_movieplayer_timeosd_while_searching")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(movieplayer_timeosd_while_searching)),
	// How many times the end of a file has to be seen before the player stops.
	intRow("movieplayer_eof_cnt")
		.section("player")
		.label("movieplayer.eof_cnt")
		.range(1, 10)
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(movieplayer_eof_cnt)),
	enumRow("picviewer_scaling")
		.section("player")
		.label("pictureviewer.scaling")
		.hint("menu.hint_pictureviewer_scaling")
		.defaultValue(1)
		.values(kPicviewerScaling)
		.field(COREAPI_NUMBER_FIELD(picviewer_scaling)),
	// Seconds a slide is shown.
	intRow("picviewer_slide_time")
		.section("player")
		.label("pictureviewer.slide_time")
		.hint("menu.hint_pictureviewer_slide_time")
		.range(0, 999)
		.defaultValue(10)
		.unit("unit.short.second")
		.field(COREAPI_NUMBER_FIELD(picviewer_slide_time)),
	textRow("network_nfs_picturedir")
		.section("player")
		.label("pictureviewer.defdir")
		.hint("menu.hint_pictureviewer_defdir")
		.defaultValue(TARGET_ROOT "/media/sda1/pictures")
		.text(kRuleDirectoryExists)
		.field(COREAPI_TEXT_FIELD(network_nfs_picturedir)),

	/* The character set the box reads a subtitle file in. The list on offer is
	   filled at run time, so this is text and not a choice. */
	textRow("subs_charset")
		.section("player")
		.label("subtitles.charset")
		.defaultValue("CP1252")
		.text(kRuleNameFromList)
		.field(COREAPI_TEXT_FIELD(subs_charset)),
	/* What the player repeats: nothing, the track or everything. The player
	   writes it itself when the repeat key is pressed. A number and not a
	   choice: the three modes have no locales of their own. */
	intRow("movieplayer_repeat_on")
		.section("player")
		.range(0, 2)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(movieplayer_repeat_on)),
#if HAVE_CST_HARDWARE
	/* Behind the arm the settings struct puts the field behind. Whether the
	   player treats a track of no stated kind as AC3, which the player turns
	   over itself rather than offering as an item, so the program states no
	   name for it. */
	boolRow("movieplayer_select_ac3_atype0")
		.section("player")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(movieplayer_select_ac3_atype0)),
#endif
};

} // anonymous namespace

const Descriptor *settingsTablePlayer(size_t &count)
{
	count = sizeof(kSettings) / sizeof(kSettings[0]);
	return kSettings;
}

} // namespace coreapi
