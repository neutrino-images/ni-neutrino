/*
 * settingstable_misc.cpp - miscellaneous settings, one row per field
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

namespace coreapi
{

namespace
{

/* The misc section, as the program states it: the key, the type and the default
   off the line that loads the setting, the bounds, the choices and the labels
   off the menu that offers it. The menu has nine submenus and the descriptor
   carries no sub-section, so that structure is not in the table. Nothing here
   needs a restart: each of these is read again by whatever acts on it. */

/* The one inverted flag in the tree. Nought reads as on, so a Bool row would
   turn a frontend's true into the opposite and nothing would say so. */
const EnumValue kShutdownReal[] =
{
	{ 1, "options.off", NULL, NULL },
	{ 0, "options.on", NULL, NULL }
};

// Flags worded as a no and a yes rather than an off and an on.
const EnumValue kNoYes[] =
{
	{ 0, "messagebox.no", NULL, NULL },
	{ 1, "messagebox.yes", NULL, NULL }
};

const EnumValue kSleeptimerMin[] =
{
	{ 0, NULL, "EPG", NULL },
	{ 30, NULL, "30 min", NULL },
	{ 60, NULL, "60 min", NULL },
	{ 90, NULL, "90 min", NULL },
	{ 120, NULL, "120 min", NULL },
	{ 150, NULL, "150 min", NULL }
};

const EnumValue kFilesystemUtf8[] =
{
	{ 0, "filesystem.is.utf8.option.iso8859.1", NULL, NULL },
	{ 1, "filesystem.is.utf8.option.utf8", NULL, NULL }
};

const EnumValue kNewZapMode[] =
{
	{ 0, "channellist.new_zap_mode_off", NULL, NULL },
	{ 1, "channellist.new_zap_mode_allow", NULL, NULL },
	{ 2, "channellist.new_zap_mode_active", NULL, NULL }
};

const EnumValue kEnableSdt[] =
{
	{ 0, "channellist.enablesdt_off", NULL, NULL },
	{ 1, "channellist.enablesdt_on", NULL, NULL },
	{ 2, "channellist.enablesdt_on_extended", NULL, NULL }
};

// The two modes that scan live are offered only with more than one tuner switched on.
const EnumValue kEpgScanMode[] =
{
	{ EPG_SCAN_MODE_OFF, "options.off", NULL, NULL },
	{ EPG_SCAN_MODE_STANDBY, "miscsettings.epg_scan_standby", NULL, NULL },
	{ EPG_SCAN_MODE_LIVE, "miscsettings.epg_scan_live", NULL, severalTunersEnabled },
	{ EPG_SCAN_MODE_ALWAYS, "miscsettings.epg_scan_always", NULL, severalTunersEnabled }
};

/* EPG_SCAN_OFF is not offered: the loader turns a stored nought into the
   first of the three below and the scan is turned off through the mode beside
   it. */
const EnumValue kEpgScan[] =
{
	{ EPG_SCAN_CURRENT, "miscsettings.epg_scan_bq", NULL, NULL },
	{ EPG_SCAN_FAV, "miscsettings.epg_scan_fav", NULL, NULL },
	{ EPG_SCAN_SEL, "miscsettings.epg_scan_sel", NULL, NULL }
};

// Megahertz; nought leaves the clock at the box's default.
const EnumValue kCpuFreq[] =
{
	{ 0, "cpu.freq_default", NULL, NULL },
	{ 50, NULL, "50 Mhz", NULL },
	{ 100, NULL, "100 Mhz", NULL },
	{ 150, NULL, "150 Mhz", NULL },
	{ 200, NULL, "200 Mhz", NULL },
	{ 250, NULL, "250 Mhz", NULL },
	{ 300, NULL, "300 Mhz", NULL },
	{ 350, NULL, "350 Mhz", NULL },
	{ 400, NULL, "400 Mhz", NULL },
	{ 450, NULL, "450 Mhz", NULL },
	{ 500, NULL, "500 Mhz", NULL },
	{ 550, NULL, "550 Mhz", NULL },
	{ 600, NULL, "600 Mhz", NULL }
};

// The two below are editable only while the box is left to switch off for real.
const Condition kShutdownRealOff[] =
{
	{ "shutdown_real", CompareOp::Eq, 0, NULL, 0 }
};

const Condition kEpgSaveOn[] =
{
	{ "epg_save", CompareOp::Ne, 0, NULL, 0 }
};

const Condition kEpgReadOn[] =
{
	{ "epg_read", CompareOp::Ne, 0, NULL, 0 }
};

// Where the box goes when an advert break is spotted.
const EnumValue kAdzapZap[] =
{
	{ SNeutrinoSettings::ADZAP_ZAP_OFF, "adzap.zap_off", NULL, NULL },
	{ SNeutrinoSettings::ADZAP_ZAP_TO_LAST, "adzap.zap_to_last_channel", NULL, NULL },
	{ SNeutrinoSettings::ADZAP_ZAP_TO_START, "adzap.zap_to_start_channel", NULL, NULL }
};

const Descriptor kMisc[] =
{
	// general
	{
		"power_standby", ValueType::Bool, "misc",
		"extra.start_tostandby", "menu.hint_start_tostandby",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(power_standby)
	},
	{
		"cacheTXT", ValueType::Bool, "misc",
		"extra.cache_txt", "menu.hint_cache_txt",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(cacheTXT)
	},
	/* Nought would stop the fan, and the load lifts anything below one to one,
	   so no value names it off. */
	{
		"fan_speed", ValueType::Int, "misc",
		"fan_speed", "menu.hint_fan_speed",
		1, 14, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(fan_speed, hasFan, NULL)
	},

	/* The energy menu is also opened from the power menu on boxes that cannot
	   switch themselves off, and it has always shown these three there, so only
	   the setting that needs deep standby carries the test of the box. */
	// energy and shutdown
	{
		"shutdown_real", ValueType::Enum, "misc",
		"miscsettings.shutdown_real", "menu.hint_shutdown_real",
		0, 0, COREAPI_VALUES(kShutdownReal), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(shutdown_real)
	},
	{
		"shutdown_real_rcdelay", ValueType::Bool, "misc",
		"miscsettings.shutdown_real_rcdelay", "menu.hint_shutdown_rcdelay",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kShutdownRealOff),
		COREAPI_NUMBER_FIELD(shutdown_real_rcdelay)
	},
	{
		"shutdown_block_while_recording", ValueType::Bool, "misc",
		"miscsettings.shutdown_block_recording", "menu.hint_shutdown_block_recording",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(shutdown_block_while_recording, canShutdown, NULL)
	},
	// Minutes; 0 takes the length off the programme being shown.
	{
		"sleeptimer_min", ValueType::Enum, "misc",
		"miscsettings.sleeptimer_min", "menu.hint_sleeptimer_min",
		0, 0, COREAPI_VALUES(kSleeptimerMin), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(sleeptimer_min)
	},

	// epg
	{
		"epg_save", ValueType::Bool, "misc",
		"miscsettings.epg_save", "menu.hint_epg_save",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_save)
	},
	{
		"epg_save_standby", ValueType::Bool, "misc",
		"miscsettings.epg_save_standby", "menu.hint_epg_save_standby",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_CONDITIONS(kEpgSaveOn),
		COREAPI_NUMBER_FIELD(epg_save_standby)
	},
	{
		"epg_save_frequently", ValueType::Bool, "misc",
		"miscsettings.epg_save_frequently", "menu.hint_epg_save_frequently",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kEpgSaveOn),
		COREAPI_NUMBER_FIELD(epg_save_frequently)
	},
	{
		"epg_save_mode", ValueType::Bool, "misc",
		"miscsettings.epg_save_mode", "menu.hint_epg_save_mode",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_save_mode)
	},
	{
		"epg_read", ValueType::Bool, "misc",
		"miscsettings.epg_read", "menu.hint_epg_read",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_read)
	},
	{
		"epg_read_frequently", ValueType::Bool, "misc",
		"miscsettings.epg_read_frequently", "menu.hint_epg_read_frequently",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_CONDITIONS(kEpgReadOn),
		COREAPI_NUMBER_FIELD(epg_read_frequently)
	},
	/* Offered while either of the two above is on, a disjunction across two
	   keys that a conjunction of comparisons cannot say, so it carries no
	   condition and is always shown. */
	{
		"epg_dir", ValueType::String, "misc",
		"miscsettings.epg_dir", "menu.hint_epg_dir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/epg", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(epg_dir)
	},
	{
		"epg_scan", ValueType::Enum, "misc",
		"miscsettings.epg_scan_bouquets", "menu.hint_epg_scan",
		0, 0, COREAPI_VALUES(kEpgScan), EPG_SCAN_FAV, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_scan)
	},
	/* The value is a pair of bits, live and standby. The two modes that scan
	   live need a second tuner. */
	{
		"epg_scan_mode", ValueType::Enum, "misc",
		"miscsettings.epg_scan", "menu.hint_epg_scan_mode",
		0, 0, COREAPI_ENUM(kEpgScanMode), EPG_SCAN_MODE_STANDBY, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_scan_mode)
	},

	// file browser
	{
		"filesystem_is_utf8", ValueType::Enum, "misc",
		"filesystem.is.utf8", "menu.hint_filesystem_is_utf8",
		0, 0, COREAPI_VALUES(kFilesystemUtf8), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(filesystem_is_utf8)
	},
	{
		"filebrowser_showrights", ValueType::Bool, "misc",
		"filebrowser.showrights", "menu.hint_filebrowser_showrights",
		0, 1, COREAPI_VALUES(kNoYes), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(filebrowser_showrights)
	},
	{
		"filebrowser_denydirectoryleave", ValueType::Bool, "misc",
		"filebrowser.denydirectoryleave", "menu.hint_filebrowser_denydirectoryleave",
		0, 1, COREAPI_VALUES(kNoYes), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(filebrowser_denydirectoryleave)
	},

	// channel list
	{
		"make_hd_list", ValueType::Bool, "misc",
		"channellist.make_hdlist", "menu.hint_make_hdlist",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(make_hd_list)
	},
	{
		"make_webtv_list", ValueType::Bool, "misc",
		"channellist.make_webtvlist", "menu.hint_make_webtvlist",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(make_webtv_list)
	},
	{
		"make_webradio_list", ValueType::Bool, "misc",
		"channellist.make_webradiolist", "menu.hint_make_webradiolist",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(make_webradio_list)
	},
	{
		"make_new_list", ValueType::Bool, "misc",
		"channellist.make_newlist", "menu.hint_make_newlist",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(make_new_list)
	},
	{
		"make_removed_list", ValueType::Bool, "misc",
		"channellist.make_removedlist", "menu.hint_make_removedlist",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(make_removed_list)
	},
	{
		"keep_channel_numbers", ValueType::Bool, "misc",
		"channellist.keep_numbers", "menu.hint_keep_numbers",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(keep_channel_numbers)
	},
	{
		"zap_cycle", ValueType::Bool, "misc",
		"extra.zap_cycle", "menu.hint_zap_cycle",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(zap_cycle)
	},
	{
		"channellist_new_zap_mode", ValueType::Enum, "misc",
		"channellist.new_zap_mode", "menu.hint_new_zap_mode",
		0, 0, COREAPI_VALUES(kNewZapMode), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_new_zap_mode)
	},
	{
		"channellist_numeric_adjust", ValueType::Bool, "misc",
		"channellist.numeric_adjust", "menu.hint_numeric_adjust",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_numeric_adjust)
	},
	{
		"show_empty_favorites", ValueType::Bool, "misc",
		"channellist.show_empty_favs", "menu.hint_channellist_show_empty_favs",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(show_empty_favorites)
	},
	{
		"enable_sdt", ValueType::Enum, "misc",
		"miscsettings.channellist_enablesdt", "menu.hint_channellist_enablesdt",
		0, 0, COREAPI_VALUES(kEnableSdt), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(enable_sdt)
	},

	/* Online services. Each of the four flags is editable only where the key
	   beside it passes a check that reads the key itself, which is a function
	   call and not a comparison against another setting, so none of them
	   carries a condition. Each key defaults to the placeholder below only
	   where the build carries none of its own; one configured with a key falls
	   back to that key instead, which is not a constant this can carry.

	   The four keys are credentials and are declared secret. The flags beside
	   them are not: which service a box uses is not a secret, and marking them
	   would hide the state of a switch a frontend has to draw. */
	{
		"tmdb_enabled", ValueType::Bool, "misc",
		"tmdb.enabled", "menu.hint_tmdb_enabled",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(tmdb_enabled)
	},
	{
		"tmdb_api_key", ValueType::String, "misc",
		"tmdb.api_key", "menu.hint_tmdb_api_key",
		0, 0, NULL, 0, 0, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(tmdb_api_key)
	},
	{
		"omdb_enabled", ValueType::Bool, "misc",
		"omdb.enabled", "menu.hint_omdb_enabled",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(omdb_enabled)
	},
	{
		"omdb_api_key", ValueType::String, "misc",
		"omdb.api_key", "menu.hint_omdb_api_key",
		0, 0, NULL, 0, 0, "XXXXXXXX", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(omdb_api_key)
	},
	{
		"shoutcast_enabled", ValueType::Bool, "misc",
		"shoutcast.enabled", "menu.hint_shoutcast_enabled",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(shoutcast_enabled)
	},
	{
		"shoutcast_dev_id", ValueType::String, "misc",
		"shoutcast.dev_id", "menu.hint_shoutcast_dev_id",
		0, 0, NULL, 0, 0, "XXXXXXXXXXXXXXXX", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(shoutcast_dev_id)
	},
	{
		"youtube_enabled", ValueType::Bool, "misc",
		"youtube.enabled", "menu.hint_youtube_enabled",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(youtube_enabled)
	},
	{
		"youtube_api_key", ValueType::String, "misc",
		"youtube.api_key", "menu.hint_youtube_api_key",
		0, 0, NULL, 0, 0, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(youtube_api_key)
	},

	// plugins
	{
		"plugin_hdd_dir", ValueType::String, "misc",
		"plugins.hdd_dir", "menu.hint_plugins_hdd_dir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/plugins", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(plugin_hdd_dir)
	},
	// The name of a plugin, and the default stands for none.
	{
		"movieplayer_plugin", ValueType::String, "misc",
		"mpkey.plugin", "menu.hint_movieplayer_plugin",
		0, 0, NULL, 0, 0, "---", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(movieplayer_plugin)
	},

	// streaming
	// Bound: five digits.
	{
		"streaming_port", ValueType::Int, "misc",
		"streaming.port", NULL,
		0, 99999, NULL, 0, 31339, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(streaming_port)
	},
	{
		"streaming_ecmmode", ValueType::Bool, "misc",
		"streaming.ecmmode", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(streaming_ecmmode)
	},
	{
		"streaming_decryptmode", ValueType::Bool, "misc",
		"streaming.decryptmode", NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(streaming_decryptmode)
	},

	/* Where the box cannot change its clock the loader forces nought for the
	   first and fifty for the second. */
	{
		"cpufreq", ValueType::Enum, "misc",
		"cpu.freq_normal", NULL,
		0, 0, COREAPI_VALUES(kCpuFreq), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(cpufreq, canCpufreq, NULL)
	},
	{
		"standby_cpufreq", ValueType::Enum, "misc",
		"cpu.freq_standby", NULL,
		0, 0, COREAPI_VALUES(kCpuFreq), 100, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(standby_cpufreq, canCpufreq, NULL)
	},

	/* What follows has no item in the settings menus, or an item offered
	   outside them. Where the program states no name for a setting the row says
	   so rather than borrowing the name of a neighbouring item, which reads as
	   right on the page and is wrong in the menu. */
	/* The small channel list, which the lists ask for while they draw. */
	{
		"minimode", ValueType::Bool, "misc",
		NULL, NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(minimode)
	},
	/* Three digits, the only bound stated anywhere: the value is kept as text
	   in the menu and turned into a number on the way out. */
	{
		"shutdown_count", ValueType::Int, "misc",
		"miscsettings.shutdown_count", "menu.hint_shutdown_count",
		0, 999, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(shutdown_count)
	},
	/* The standing sleep timer in minutes, held as three digits of text in the
	   sleep timer box. That box is reached on every box, also where it cannot
	   switch itself off, so the row carries no test of the box. */
	{
		"shutdown_min", ValueType::Int, "misc",
		"sleeptimerbox.title2", NULL,
		0, 999, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(shutdown_min)
	},
	/* Which of the three the power menu did last, standby, off or restart. The
	   menu writes it and reads it back to mark the entry. Its default is the
	   switch beside it, and the literal below is what that one falls back to. */
	{
		"power_off_selected", ValueType::Int, "misc",
		NULL, NULL,
		0, 2, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(power_off_selected)
	},
	/* Whether the box follows a recorder onto its own input. Read once a
	   message arrives from one, and offered by no menu at all; the web
	   interface offers it instead. */
	{
		"vcr_AutoSwitch", ValueType::Bool, "misc",
		NULL, NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(vcr_AutoSwitch)
	},
	/* Whether the up and down keys walk the audio tracks while the infobar
	   stands. No menu offers it. */
	{
		"audiochannel_up_down_enable", ValueType::Bool, "misc",
		NULL, NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audiochannel_up_down_enable)
	},
	/* The four below are the section daemon's cache sizes. Each is kept as a
	   fixed number of digits of text, which is the only bound stated anywhere
	   and is what the bounds here are. They reach
	   the daemon through the same call as the time settings. */
	{
		"epg_cache_time", ValueType::Int, "misc",
		"miscsettings.epg_cache", "menu.hint_epg_cache",
		0, 99, NULL, 0, 7, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_cache)
	},
	{
		"epg_extendedcache_time", ValueType::Int, "misc",
		"miscsettings.epg_extendedcache", "menu.hint_epg_extendedcache",
		0, 999, NULL, 0, 168, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_extendedcache)
	},
	{
		"epg_max_events", ValueType::Int, "misc",
		"miscsettings.epg_max_events", "menu.hint_epg_max_events",
		0, 999999, NULL, 0, 30000, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_max_events)
	},
	{
		"epg_old_events", ValueType::Int, "misc",
		"miscsettings.epg_old_events", "menu.hint_epg_old_events",
		0, 999, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_old_events)
	},
	/* How many hours pass before the standby scan runs again. Nothing states a
	   bound for it, so the row states the widest the field holds and a floor of
	   nought. A bound no menu governs and no check can compare. */
	{
		"epg_scan_rescan", ValueType::Int, "misc",
		NULL, NULL,
		0, 2147483647, NULL, 0, 24, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_scan_rescan)
	},
	{
		"epg_search_history_max", ValueType::Int, "misc",
		"eventfinder.max_history", NULL,
		0, 50, NULL, 0, 10, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_search_history_max)
	},
	/* How many searches the box actually kept, which the search list writes
	   itself and holds to the count above. Writing it does not add a search;
	   the ceiling is the one the list enforces. */
	{
		"epg_search_history_size", ValueType::Int, "misc",
		NULL, NULL,
		0, 50, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_search_history_size)
	},
	/* Which keyboard the on screen one comes up as, written by the keyboard
	   itself when it is changed and read the next time it opens. Empty means
	   the box picks by language. */
	{
		"keyboard_layout", ValueType::String, "misc",
		NULL, NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(keyboard_layout)
	},
	/* How the file browser sorts, which it writes itself. The loader holds it
	   to the number of sorts there are and turns anything else into nought, and
	   that count is the ceiling here. */
	{
		"filebrowser_sortmethod", ValueType::Int, "misc",
		NULL, NULL,
		0, 4, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(filebrowser_sortmethod)
	},
	/* Whether the event view opens in the larger window. Read as a flag and
	   offered by no menu. */
	{
		"bigFonts", ValueType::Bool, "misc",
		NULL, NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(bigFonts)
	},
	/* The code that guards the personalisation menu. A credential, so a read
	   answers nothing and an empty write is refused; the declared default is
	   the program's own fallback and not a box's value. Nothing here asks for
	   the old code before taking a new one. */
	{
		"personalize_pincode", ValueType::String, "misc",
		"personalize.pincode", "personalize.pinhint",
		0, 0, NULL, 0, 0, "0000", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(personalize_pincode)
	},
	/* The five below are lists of plugin file names joined with commas, which
	   the personalisation menu rebuilds whole whenever it is left. A name this
	   layer cannot check is what a frontend would be writing. */
	{
		"plugins_disabled", ValueType::String, "misc",
		NULL, NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(plugins_disabled)
	},
	{
		"plugins_game", ValueType::String, "misc",
		NULL, NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(plugins_game)
	},
	{
		"plugins_lua", ValueType::String, "misc",
		NULL, NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(plugins_lua)
	},
	{
		"plugins_script", ValueType::String, "misc",
		NULL, NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(plugins_script)
	},
	{
		"plugins_tool", ValueType::String, "misc",
		NULL, NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(plugins_tool)
	},
	/* Where the box goes back to after an advert break, in seconds. The menu
	   offers nine fixed minutes and then a chooser over ten to a hundred and
	   twenty, so the floor is the smallest of the nine and the ceiling the
	   chooser's own, both in seconds. That chooser binds a local in minutes
	   rather than the field, so its label names minutes and this row is
	   seconds; the row therefore states no label. */
	{
		"adzap_zapBackPeriod", ValueType::Int, "misc",
		NULL, NULL,
		60, 7200, NULL, 0, 180, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(adzap_zapBackPeriod)
	},
	{
		"adzap_zapOnActivation", ValueType::Enum, "misc",
		"adzap.zap", "menu.hint_adzap_zap",
		0, 0, COREAPI_ENUM(kAdzapZap), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(adzap_zapOnActivation)
	},
	{
		"adzap_writeData", ValueType::Bool, "misc",
		"adzap.writedata", "menu.hint_adzap_writedata",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(adzap_writeData)
	},
	/* Where the settings backup is written. Two file browsers write it, and
	   neither names it: the one locale that carries the words is a question
	   with the directory in it and not the name of a field. */
	{
		"backup_dir", ValueType::String, "misc",
		NULL, NULL,
		0, 0, NULL, 0, 0, TARGET_ROOT "/media", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(backup_dir)
	},
};

} // anonymous namespace

const Descriptor *settingsTableMisc(size_t &count)
{
	count = sizeof(kMisc) / sizeof(kMisc[0]);
	return kMisc;
}

} // namespace coreapi
