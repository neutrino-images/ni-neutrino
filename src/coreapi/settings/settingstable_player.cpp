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

const EnumValue kNoYes[] =
{
	{ 0, "messagebox.no", NULL, NULL },
	{ 1, "messagebox.yes", NULL, NULL }
};

const EnumValue kDisplayOrder[] =
{
	{ ARTIST_TITLE, "audioplayer.artist_title", NULL, NULL },
	{ TITLE_ARTIST, "audioplayer.title_artist", NULL, NULL }
};

const EnumValue kPicviewerScaling[] =
{
	{ PICVIEWER_SCALING_SIMPLE, "pictureviewer.resize.simple", NULL, NULL },
	{ PICVIEWER_SCALING_COLOR, "pictureviewer.resize.color_average", NULL, NULL },
	{ PICVIEWER_SCALING_NONE, "pictureviewer.resize.none", NULL, NULL }
};

const Descriptor kSettings[] =
{
	{
		"audioplayer_display", ValueType::Enum, "player",
		"audioplayer.display_order", "menu.hint_audioplayer_order",
		0, 0, COREAPI_ENUM(kDisplayOrder), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audioplayer_display)
	},
	{
		"audioplayer_follow", ValueType::Bool, "player",
		"audioplayer.follow", "menu.hint_audioplayer_follow",
		0, 1, COREAPI_ENUM(kNoYes), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audioplayer_follow)
	},
	{
		"audioplayer_select_title_by_name", ValueType::Bool, "player",
		"audioplayer.select_title_by_name", "menu.hint_audioplayer_title",
		0, 1, COREAPI_ENUM(kNoYes), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audioplayer_select_title_by_name)
	},
	{
		"audioplayer_repeat_on", ValueType::Bool, "player",
		"audioplayer.repeat_on", "menu.hint_audioplayer_repeat",
		0, 1, COREAPI_ENUM(kNoYes), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audioplayer_repeat_on)
	},
	{
		"audioplayer_show_playlist", ValueType::Bool, "player",
		"audioplayer.show_playlist", "menu.hint_audioplayer_playlist",
		0, 1, COREAPI_ENUM(kNoYes), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audioplayer_show_playlist)
	},
	{
		"audioplayer_cover_as_screensaver", ValueType::Bool, "player",
		"audioplayer.cover_as_screensaver", "menu.hint_audioplayer_cover_as_screensaver",
		0, 1, COREAPI_ENUM(kNoYes), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audioplayer_cover_as_screensaver)
	},
	{
		"audioplayer_highprio", ValueType::Bool, "player",
		"audioplayer.highprio", "menu.hint_audioplayer_highprio",
		0, 1, COREAPI_ENUM(kNoYes), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audioplayer_highprio)
	},
	/* No menu offers this one in this build, and the value is read while a
	   track plays all the same. The label and the words come off the item the
	   build leaves out, which is the only statement of them there is. */
	{
		"spectrum", ValueType::Bool, "player",
		"audioplayer.spectrum", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(spectrum)
	},
	{
		"network_nfs_audioplayerdir", ValueType::String, "player",
		"audioplayer.defdir", "menu.hint_audioplayer_defdir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/music", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(network_nfs_audioplayerdir)
	},
	{
		"inetradio_autostart", ValueType::Bool, "player",
		"inetradio.autostart", "menu.hint_inetradio_autostart",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(inetradio_autostart)
	},
	{
		"audioplayer_enable_sc_metadata", ValueType::Bool, "player",
		"audioplayer.enable_sc_metadata", "menu.hint_audioplayer_sc_metadata",
		0, 1, COREAPI_ENUM(kNoYes), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audioplayer_enable_sc_metadata)
	},
	{
		"network_nfs_streamripperdir", ValueType::String, "player",
		"audioplayer.streamripper_dir", "menu.hint_audioplayer_streamripper_dir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/music/streamripper", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(network_nfs_streamripperdir)
	},
	/* The default is the box's own panel and no constant states it: the loader
	   asks the hardware whether the display is a numeric one. Nought is what
	   every other box gets. A panel too narrow for the time has no use for it. */
	{
		"movieplayer_display_playtime", ValueType::Bool, "player",
		"movieplayer.display_playtime", "menu.hint_movieplayer_display_playtime",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(movieplayer_display_playtime, displayFitsPlaytime, NULL)
	},
	{
		"movieplayer_timeosd_while_searching", ValueType::Bool, "player",
		"movieplayer.timeosd_while_searching", "menu.hint_movieplayer_timeosd_while_searching",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(movieplayer_timeosd_while_searching)
	},
	// How many times the end of a file has to be seen before the player stops.
	{
		"movieplayer_eof_cnt", ValueType::Int, "player",
		"movieplayer.eof_cnt", NULL,
		1, 10, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(movieplayer_eof_cnt)
	},
	{
		"picviewer_scaling", ValueType::Enum, "player",
		"pictureviewer.scaling", "menu.hint_pictureviewer_scaling",
		0, 0, COREAPI_ENUM(kPicviewerScaling), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(picviewer_scaling)
	},
	// Seconds a slide is shown.
	{
		"picviewer_slide_time", ValueType::Int, "player",
		"pictureviewer.slide_time", "menu.hint_pictureviewer_slide_time",
		0, 999, NULL, 0, 10, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(picviewer_slide_time)
	},
	{
		"network_nfs_picturedir", ValueType::String, "player",
		"pictureviewer.defdir", "menu.hint_pictureviewer_defdir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/pictures", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(network_nfs_picturedir)
	},

	/* The character set the box reads a subtitle file in. The list on offer is
	   filled at run time, so this is text and not a choice. */
	{
		"subs_charset", ValueType::String, "player",
		"subtitles.charset", NULL,
		0, 0, NULL, 0, 0, "CP1252", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(subs_charset)
	},
	/* What the player repeats: nothing, the track or everything. The player
	   writes it itself when the repeat key is pressed. A number and not a
	   choice: the three modes have no locales of their own. */
	{
		"movieplayer_repeat_on", ValueType::Int, "player",
		NULL, NULL,
		0, 2, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(movieplayer_repeat_on)
	},
#if HAVE_CST_HARDWARE
	/* Behind the arm the settings struct puts the field behind. Whether the
	   player treats a track of no stated kind as AC3, which the player turns
	   over itself rather than offering as an item, so the program states no
	   name for it. */
	{
		"movieplayer_select_ac3_atype0", ValueType::Bool, "player",
		NULL, NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(movieplayer_select_ac3_atype0)
	},
#endif
};

} // anonymous namespace

const Descriptor *settingsTablePlayer(size_t &count)
{
	count = sizeof(kSettings) / sizeof(kSettings[0]);
	return kSettings;
}

} // namespace coreapi
