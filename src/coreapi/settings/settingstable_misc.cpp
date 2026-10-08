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
constexpr EnumValue kShutdownReal[] =
{
	option(1).label("options.off"),
	option(0).label("options.on")
};

// Flags worded as a no and a yes rather than an off and an on.
constexpr EnumValue kNoYes[] =
{
	option(0).label("messagebox.no"),
	option(1).label("messagebox.yes")
};

constexpr EnumValue kSleeptimerMin[] =
{
	option(0).text("EPG"),
	option(30).text("30 min"),
	option(60).text("60 min"),
	option(90).text("90 min"),
	option(120).text("120 min"),
	option(150).text("150 min")
};

constexpr EnumValue kFilesystemUtf8[] =
{
	option(0).label("filesystem.is.utf8.option.iso8859.1"),
	option(1).label("filesystem.is.utf8.option.utf8")
};

constexpr EnumValue kNewZapMode[] =
{
	option(0).label("channellist.new_zap_mode_off"),
	option(1).label("channellist.new_zap_mode_allow"),
	option(2).label("channellist.new_zap_mode_active")
};

constexpr EnumValue kEnableSdt[] =
{
	option(0).label("channellist.enablesdt_off"),
	option(1).label("channellist.enablesdt_on"),
	option(2).label("channellist.enablesdt_on_extended")
};

// The two modes that scan live are offered only with more than one tuner switched on.
constexpr EnumValue kEpgScanMode[] =
{
	option(EPG_SCAN_MODE_OFF).label("options.off"),
	option(EPG_SCAN_MODE_STANDBY).label("miscsettings.epg_scan_standby"),
	option(EPG_SCAN_MODE_LIVE).label("miscsettings.epg_scan_live").availableIf(severalTunersEnabled),
	option(EPG_SCAN_MODE_ALWAYS).label("miscsettings.epg_scan_always").availableIf(severalTunersEnabled)
};

/* EPG_SCAN_OFF is not offered: the loader turns a stored nought into the
   first of the three below and the scan is turned off through the mode beside
   it. */
constexpr EnumValue kEpgScan[] =
{
	option(EPG_SCAN_CURRENT).label("miscsettings.epg_scan_bq"),
	option(EPG_SCAN_FAV).label("miscsettings.epg_scan_fav"),
	option(EPG_SCAN_SEL).label("miscsettings.epg_scan_sel")
};

// Megahertz; nought leaves the clock at the box's default.
constexpr EnumValue kCpuFreq[] =
{
	option(0).label("cpu.freq_default"),
	option(50).text("50 Mhz"),
	option(100).text("100 Mhz"),
	option(150).text("150 Mhz"),
	option(200).text("200 Mhz"),
	option(250).text("250 Mhz"),
	option(300).text("300 Mhz"),
	option(350).text("350 Mhz"),
	option(400).text("400 Mhz"),
	option(450).text("450 Mhz"),
	option(500).text("500 Mhz"),
	option(550).text("550 Mhz"),
	option(600).text("600 Mhz")
};

// Nought is the switch-off time that is not set: the box stays in soft standby.
constexpr EnumValue kShutdownCountOff[] =
{
	option(0).label("options.off")
};

// Nought is no ceiling on the events kept.
constexpr EnumValue kEpgMaxEventsUnlimited[] =
{
	option(0).label("options.unlimited")
};

// The code is asked for only while the menu is guarded, and changed only then.
constexpr Condition kPersonalizeGuarded[] =
{
	when("personalize_pinstatus").isNot(0)
};

// The two below are editable only while the box is left to switch off for real.
constexpr Condition kShutdownRealOff[] =
{
	when("shutdown_real").is(0)
};

// The scan has bouquets to walk only while it is switched on.
constexpr Condition kEpgScanOn[] =
{
	when("epg_scan_mode").isNot(EPG_SCAN_MODE_OFF)
};

constexpr Condition kEpgSaveOn[] =
{
	when("epg_save").isNot(0)
};

constexpr Condition kEpgReadOn[] =
{
	when("epg_read").isNot(0)
};

// Where the guide is kept matters while it is either saved or read.
constexpr Condition kEpgSavedOrRead[] =
{
	when("epg_save").isNot(0),
	when("epg_read").isNot(0)
};

constexpr Condition kEpgDirUsed[] =
{
	anyOf(kEpgSavedOrRead)
};

// Where the box goes when an advert break is spotted.
constexpr EnumValue kAdzapZap[] =
{
	option(SNeutrinoSettings::ADZAP_ZAP_OFF).label("adzap.zap_off"),
	option(SNeutrinoSettings::ADZAP_ZAP_TO_LAST).label("adzap.zap_to_last_channel"),
	option(SNeutrinoSettings::ADZAP_ZAP_TO_START).label("adzap.zap_to_start_channel")
};

/* What each service key holds before anybody enters one, which is also the text
   the service switch is judged against: a key still holding it, or none, leaves
   the service without a key to work with. */
constexpr char kTmdbKeyPlaceholder[] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
constexpr char kOmdbKeyPlaceholder[] = "XXXXXXXX";
constexpr char kShoutcastKeyPlaceholder[] = "XXXXXXXXXXXXXXXX";
constexpr char kYoutubeKeyPlaceholder[] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

constexpr Condition kTmdbKeyEntered[] =
{
	when("tmdb_api_key").textValid(kTmdbKeyPlaceholder)
};

constexpr Condition kOmdbKeyEntered[] =
{
	when("omdb_api_key").textValid(kOmdbKeyPlaceholder)
};

constexpr Condition kShoutcastKeyEntered[] =
{
	when("shoutcast_dev_id").textValid(kShoutcastKeyPlaceholder)
};

constexpr Condition kYoutubeKeyEntered[] =
{
	when("youtube_api_key").textValid(kYoutubeKeyPlaceholder)
};

constexpr Descriptor kMisc[] =
{
	// general
	boolRow("power_standby")
		.section("misc")
		.label("extra.start_tostandby")
		.hint("menu.hint_start_tostandby")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(power_standby)),
	boolRow("cacheTXT")
		.section("misc")
		.label("extra.cache_txt")
		.hint("menu.hint_cache_txt")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(cacheTXT)),
	/* Nought would stop the fan, and the load lifts anything below one to one,
	   so no value names it off. */
	intRow("fan_speed")
		.section("misc")
		.label("fan_speed")
		.hint("menu.hint_fan_speed")
		.range(1, 14)
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD_ON(fan_speed, hasFan, NULL)),

	/* The energy menu is also opened from the power menu on boxes that cannot
	   switch themselves off, and it has always shown these three there, so only
	   the setting that needs deep standby carries the test of the box. */
	// energy and shutdown
	enumRow("shutdown_real")
		.section("misc")
		.label("miscsettings.shutdown_real")
		.hint("menu.hint_shutdown_real")
		.defaultValue(0)
		.values(kShutdownReal)
		.field(COREAPI_NUMBER_FIELD(shutdown_real)),
	boolRow("shutdown_real_rcdelay")
		.section("misc")
		.label("miscsettings.shutdown_real_rcdelay")
		.hint("menu.hint_shutdown_rcdelay")
		.defaultValue(0)
		.changeableWhen(kShutdownRealOff)
		.field(COREAPI_NUMBER_FIELD(shutdown_real_rcdelay)),
	boolRow("shutdown_block_while_recording")
		.section("misc")
		.label("miscsettings.shutdown_block_recording")
		.hint("menu.hint_shutdown_block_recording")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD_ON(shutdown_block_while_recording, canShutdown, NULL)),
	// Minutes; 0 takes the length off the programme being shown.
	enumRow("sleeptimer_min")
		.section("misc")
		.label("miscsettings.sleeptimer_min")
		.hint("menu.hint_sleeptimer_min")
		.defaultValue(0)
		.values(kSleeptimerMin)
		.field(COREAPI_NUMBER_FIELD(sleeptimer_min)),

	// epg
	boolRow("epg_save")
		.section("misc")
		.label("miscsettings.epg_save")
		.hint("menu.hint_epg_save")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(epg_save)),
	boolRow("epg_save_standby")
		.section("misc")
		.label("miscsettings.epg_save_standby")
		.hint("menu.hint_epg_save_standby")
		.defaultValue(1)
		.changeableWhen(kEpgSaveOn)
		.field(COREAPI_NUMBER_FIELD(epg_save_standby)),
	boolRow("epg_save_frequently")
		.section("misc")
		.label("miscsettings.epg_save_frequently")
		.hint("menu.hint_epg_save_frequently")
		.defaultValue(0)
		.changeableWhen(kEpgSaveOn)
		.field(COREAPI_NUMBER_FIELD(epg_save_frequently)),
	boolRow("epg_save_mode")
		.section("misc")
		.label("miscsettings.epg_save_mode")
		.hint("menu.hint_epg_save_mode")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(epg_save_mode)),
	boolRow("epg_read")
		.section("misc")
		.label("miscsettings.epg_read")
		.hint("menu.hint_epg_read")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(epg_read)),
	boolRow("epg_read_frequently")
		.section("misc")
		.label("miscsettings.epg_read_frequently")
		.hint("menu.hint_epg_read_frequently")
		.defaultValue(1)
		.changeableWhen(kEpgReadOn)
		.field(COREAPI_NUMBER_FIELD(epg_read_frequently)),
	textRow("epg_dir")
		.section("misc")
		.label("miscsettings.epg_dir")
		.hint("menu.hint_epg_dir")
		.defaultValue(TARGET_ROOT "/media/sda1/epg")
		.changeableWhen(kEpgDirUsed)
		.text(kRuleDirectoryDurable)
		.field(COREAPI_TEXT_FIELD(epg_dir)),
	enumRow("epg_scan")
		.section("misc")
		.label("miscsettings.epg_scan_bouquets")
		.hint("menu.hint_epg_scan")
		.defaultValue(EPG_SCAN_FAV)
		.values(kEpgScan)
		.changeableWhen(kEpgScanOn)
		.field(COREAPI_NUMBER_FIELD(epg_scan)),
	/* The value is a pair of bits, live and standby. The two modes that scan
	   live need a second tuner. */
	enumRow("epg_scan_mode")
		.section("misc")
		.label("miscsettings.epg_scan")
		.hint("menu.hint_epg_scan_mode")
		.defaultValue(EPG_SCAN_MODE_STANDBY)
		.values(kEpgScanMode)
		.field(COREAPI_NUMBER_FIELD(epg_scan_mode)),

	// file browser
	enumRow("filesystem_is_utf8")
		.section("misc")
		.label("filesystem.is.utf8")
		.hint("menu.hint_filesystem_is_utf8")
		.defaultValue(1)
		.values(kFilesystemUtf8)
		.field(COREAPI_NUMBER_FIELD(filesystem_is_utf8)),
	boolRow("filebrowser_showrights")
		.section("misc")
		.label("filebrowser.showrights")
		.hint("menu.hint_filebrowser_showrights")
		.defaultValue(1)
		.values(kNoYes)
		.field(COREAPI_NUMBER_FIELD(filebrowser_showrights)),
	boolRow("filebrowser_denydirectoryleave")
		.section("misc")
		.label("filebrowser.denydirectoryleave")
		.hint("menu.hint_filebrowser_denydirectoryleave")
		.defaultValue(0)
		.values(kNoYes)
		.field(COREAPI_NUMBER_FIELD(filebrowser_denydirectoryleave)),

	// channel list
	boolRow("make_hd_list")
		.section("misc")
		.label("channellist.make_hdlist")
		.hint("menu.hint_make_hdlist")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(make_hd_list)),
	boolRow("make_webtv_list")
		.section("misc")
		.label("channellist.make_webtvlist")
		.hint("menu.hint_make_webtvlist")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(make_webtv_list)),
	boolRow("make_webradio_list")
		.section("misc")
		.label("channellist.make_webradiolist")
		.hint("menu.hint_make_webradiolist")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(make_webradio_list)),
	boolRow("make_new_list")
		.section("misc")
		.label("channellist.make_newlist")
		.hint("menu.hint_make_newlist")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(make_new_list)),
	boolRow("make_removed_list")
		.section("misc")
		.label("channellist.make_removedlist")
		.hint("menu.hint_make_removedlist")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(make_removed_list)),
	boolRow("keep_channel_numbers")
		.section("misc")
		.label("channellist.keep_numbers")
		.hint("menu.hint_keep_numbers")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(keep_channel_numbers)),
	boolRow("zap_cycle")
		.section("misc")
		.label("extra.zap_cycle")
		.hint("menu.hint_zap_cycle")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(zap_cycle)),
	enumRow("channellist_new_zap_mode")
		.section("misc")
		.label("channellist.new_zap_mode")
		.hint("menu.hint_new_zap_mode")
		.defaultValue(0)
		.values(kNewZapMode)
		.field(COREAPI_NUMBER_FIELD(channellist_new_zap_mode)),
	boolRow("channellist_numeric_adjust")
		.section("misc")
		.label("channellist.numeric_adjust")
		.hint("menu.hint_numeric_adjust")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(channellist_numeric_adjust)),
	boolRow("show_empty_favorites")
		.section("misc")
		.label("channellist.show_empty_favs")
		.hint("menu.hint_channellist_show_empty_favs")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(show_empty_favorites)),
	enumRow("enable_sdt")
		.section("misc")
		.label("miscsettings.channellist_enablesdt")
		.hint("menu.hint_channellist_enablesdt")
		.defaultValue(1)
		.values(kEnableSdt)
		.field(COREAPI_NUMBER_FIELD(enable_sdt)),

	/* Online services. Each of the four flags is editable only while the key
	   beside it holds something other than its placeholder; the loader turns a
	   flag off at start for a key that does not. Each key defaults to the
	   placeholder only where the build carries none of its own; one configured
	   with a key falls back to that key instead, which is not a constant this
	   can carry.

	   The four keys are credentials and are declared secret. The flags beside
	   them are not: which service a box uses is not a secret, and marking them
	   would hide the state of a switch a frontend has to draw. */
	boolRow("tmdb_enabled")
		.section("misc")
		.label("tmdb.enabled")
		.hint("menu.hint_tmdb_enabled")
		.defaultValue(1)
		.changeableWhen(kTmdbKeyEntered)
		.field(COREAPI_NUMBER_FIELD(tmdb_enabled)),
	textRow("tmdb_api_key")
		.section("misc")
		.label("tmdb.api_key")
		.hint("menu.hint_tmdb_api_key")
		.defaultValue(kTmdbKeyPlaceholder)
		.secret()
		.text(kRuleKey32)
		.field(COREAPI_TEXT_FIELD(tmdb_api_key)),
	boolRow("omdb_enabled")
		.section("misc")
		.label("omdb.enabled")
		.hint("menu.hint_omdb_enabled")
		.defaultValue(1)
		.changeableWhen(kOmdbKeyEntered)
		.field(COREAPI_NUMBER_FIELD(omdb_enabled)),
	textRow("omdb_api_key")
		.section("misc")
		.label("omdb.api_key")
		.hint("menu.hint_omdb_api_key")
		.defaultValue(kOmdbKeyPlaceholder)
		.secret()
		.text(kRuleKey8)
		.field(COREAPI_TEXT_FIELD(omdb_api_key)),
	boolRow("shoutcast_enabled")
		.section("misc")
		.label("shoutcast.enabled")
		.hint("menu.hint_shoutcast_enabled")
		.defaultValue(1)
		.changeableWhen(kShoutcastKeyEntered)
		.field(COREAPI_NUMBER_FIELD(shoutcast_enabled)),
	textRow("shoutcast_dev_id")
		.section("misc")
		.label("shoutcast.dev_id")
		.hint("menu.hint_shoutcast_dev_id")
		.defaultValue(kShoutcastKeyPlaceholder)
		.secret()
		.text(kRuleKey16)
		.field(COREAPI_TEXT_FIELD(shoutcast_dev_id)),
	/* Nothing in the program reads this back after the load: the switch is for the YouTube
	   plugin, which takes it out of the settings file. */
	boolRow("youtube_enabled")
		.section("misc")
		.label("youtube.enabled")
		.hint("menu.hint_youtube_enabled")
		.defaultValue(1)
		.readOutside()
		.changeableWhen(kYoutubeKeyEntered)
		.field(COREAPI_NUMBER_FIELD(youtube_enabled)),
	textRow("youtube_api_key")
		.section("misc")
		.label("youtube.api_key")
		.hint("menu.hint_youtube_api_key")
		.defaultValue(kYoutubeKeyPlaceholder)
		.secret()
		.text(kRuleKey39)
		.field(COREAPI_TEXT_FIELD(youtube_api_key)),

	// plugins
	textRow("plugin_hdd_dir")
		.section("misc")
		.label("plugins.hdd_dir")
		.hint("menu.hint_plugins_hdd_dir")
		.defaultValue(TARGET_ROOT "/media/sda1/plugins")
		.text(kRuleDirectory)
		.field(COREAPI_TEXT_FIELD(plugin_hdd_dir)),
	// The name of a plugin, and the default stands for none.
	textRow("movieplayer_plugin")
		.section("misc")
		.label("mpkey.plugin")
		.hint("menu.hint_movieplayer_plugin")
		.defaultValue("---")
		.text(kRuleNameFromList)
		.field(COREAPI_TEXT_FIELD(movieplayer_plugin)),

	// streaming
	// A port number; nought and anything above the sixteen bits name no port.
	intRow("streaming_port")
		.section("misc")
		.label("streaming.port")
		.range(1, 65535)
		.defaultValue(31339)
		.field(COREAPI_NUMBER_FIELD(streaming_port)),
	boolRow("streaming_ecmmode")
		.section("misc")
		.label("streaming.ecmmode")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(streaming_ecmmode)),
	boolRow("streaming_decryptmode")
		.section("misc")
		.label("streaming.decryptmode")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(streaming_decryptmode)),

	/* Where the box cannot change its clock the loader forces nought for the
	   first and fifty for the second. */
	enumRow("cpufreq")
		.section("misc")
		.label("cpu.freq_normal")
		.defaultValue(0)
		.values(kCpuFreq)
		.field(COREAPI_NUMBER_FIELD_ON(cpufreq, canCpufreq, NULL)),
	enumRow("standby_cpufreq")
		.section("misc")
		.label("cpu.freq_standby")
		.defaultValue(100)
		.values(kCpuFreq)
		.field(COREAPI_NUMBER_FIELD_ON(standby_cpufreq, canCpufreq, NULL)),

	/* What follows has no item in the settings menus, or an item offered
	   outside them. Where the program states no name for a setting the row says
	   so rather than borrowing the name of a neighbouring item, which reads as
	   right on the page and is wrong in the menu. */
	/* The small channel list, which the lists ask for while they draw. */
	boolRow("minimode")
		.section("misc")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(minimode)),
	/* Three digits, the only bound stated anywhere: the value is kept as text
	   in the menu and turned into a number on the way out. */
	intRow("shutdown_count")
		.section("misc")
		.label("miscsettings.shutdown_count")
		.hint("menu.hint_shutdown_count")
		.range(0, 999)
		.defaultValue(0)
		.unit("unit.short.minute")
		.values(kShutdownCountOff)
		.changeableWhen(kShutdownRealOff)
		.field(COREAPI_NUMBER_FIELD(shutdown_count)),
	/* The standing sleep timer in minutes, held as three digits of text in the
	   sleep timer box. That box is reached on every box, also where it cannot
	   switch itself off, so the row carries no test of the box. */
	intRow("shutdown_min")
		.section("misc")
		.label("sleeptimerbox.title2")
		.range(0, 999)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(shutdown_min)),
	/* Which of the three the power menu did last, standby, off or restart. The
	   menu writes it and reads it back to mark the entry. Its default is the
	   switch beside it, and the literal below is what that one falls back to. */
	intRow("power_off_selected")
		.section("misc")
		.range(0, 2)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(power_off_selected)),
	/* Whether the box follows a recorder onto its own input. Read once a
	   message arrives from one, and offered by no menu at all; the web
	   interface offers it instead. */
	boolRow("vcr_AutoSwitch")
		.section("misc")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(vcr_AutoSwitch)),
	/* Whether the up and down keys walk the audio tracks while the infobar
	   stands. No menu offers it. */
	boolRow("audiochannel_up_down_enable")
		.section("misc")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(audiochannel_up_down_enable)),
	/* The four below are the section daemon's cache sizes. Each is kept as a
	   fixed number of digits of text, which is the only bound stated anywhere
	   and is what the bounds here are. They reach
	   the daemon through the same call as the time settings. */
	intRow("epg_cache_time")
		.section("misc")
		.label("miscsettings.epg_cache")
		.hint("menu.hint_epg_cache")
		.range(0, 99)
		.defaultValue(7)
		.field(COREAPI_NUMBER_FIELD(epg_cache)),
	intRow("epg_extendedcache_time")
		.section("misc")
		.label("miscsettings.epg_extendedcache")
		.hint("menu.hint_epg_extendedcache")
		.range(0, 999)
		.defaultValue(168)
		.unit("unit.short.hour")
		.field(COREAPI_NUMBER_FIELD(epg_extendedcache)),
	intRow("epg_max_events")
		.section("misc")
		.label("miscsettings.epg_max_events")
		.hint("menu.hint_epg_max_events")
		.range(0, 999999)
		.defaultValue(30000)
		.values(kEpgMaxEventsUnlimited)
		.field(COREAPI_NUMBER_FIELD(epg_max_events)),
	intRow("epg_old_events")
		.section("misc")
		.label("miscsettings.epg_old_events")
		.hint("menu.hint_epg_old_events")
		.range(0, 999)
		.defaultValue(1)
		.unit("unit.short.hour")
		.field(COREAPI_NUMBER_FIELD(epg_old_events)),
	/* How many hours pass before the standby scan runs again. Nothing states a
	   bound for it, so the row states the widest the field holds and a floor of
	   nought. A bound no menu governs and no check can compare. */
	intRow("epg_scan_rescan")
		.section("misc")
		.range(0, 2147483647)
		.defaultValue(24)
		.field(COREAPI_NUMBER_FIELD(epg_scan_rescan)),
	intRow("epg_search_history_max")
		.section("misc")
		.label("eventfinder.max_history")
		.range(0, 50)
		.defaultValue(10)
		.field(COREAPI_NUMBER_FIELD(epg_search_history_max)),
	/* How many searches the box actually kept, which the search list writes
	   itself and holds to the count above. Writing it does not add a search;
	   the ceiling is the one the list enforces. */
	intRow("epg_search_history_size")
		.section("misc")
		.range(0, 50)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(epg_search_history_size)),
	/* Which keyboard the on screen one comes up as, written by the keyboard
	   itself when it is changed and read the next time it opens. Empty means
	   the box picks by language. */
	textRow("keyboard_layout")
		.section("misc")
		.defaultValue("")
		.field(COREAPI_TEXT_FIELD(keyboard_layout)),
	/* How the file browser sorts, which it writes itself. The loader holds it
	   to the number of sorts there are and turns anything else into nought, and
	   that count is the ceiling here. */
	intRow("filebrowser_sortmethod")
		.section("misc")
		.range(0, 4)
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(filebrowser_sortmethod)),
	/* Whether the event view opens in the larger window. Read as a flag and
	   offered by no menu. */
	boolRow("bigFonts")
		.section("misc")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(bigFonts)),
	/* The code that guards the personalisation menu. A credential, so a read
	   answers nothing and an empty write is refused; the declared default is
	   the program's own fallback and not a box's value. Nothing here asks for
	   the old code before taking a new one. */
	textRow("personalize_pincode")
		.section("misc")
		.label("personalize.pincode")
		.hint("personalize.pinhint")
		.defaultValue("0000")
		.secret()
		.text(kRulePin)
		.changeableWhen(kPersonalizeGuarded)
		.field(COREAPI_TEXT_FIELD(personalize_pincode)),
	/* The five below are lists of plugin file names joined with commas, which
	   the personalisation menu rebuilds whole whenever it is left. A name this
	   layer cannot check is what a frontend would be writing. */
	textRow("plugins_disabled")
		.section("misc")
		.defaultValue("")
		.text(kRulePathList)
		.field(COREAPI_TEXT_FIELD(plugins_disabled)),
	textRow("plugins_game")
		.section("misc")
		.defaultValue("")
		.text(kRulePathList)
		.field(COREAPI_TEXT_FIELD(plugins_game)),
	textRow("plugins_lua")
		.section("misc")
		.defaultValue("")
		.text(kRulePathList)
		.field(COREAPI_TEXT_FIELD(plugins_lua)),
	textRow("plugins_script")
		.section("misc")
		.defaultValue("")
		.text(kRulePathList)
		.field(COREAPI_TEXT_FIELD(plugins_script)),
	textRow("plugins_tool")
		.section("misc")
		.defaultValue("")
		.text(kRulePathList)
		.field(COREAPI_TEXT_FIELD(plugins_tool)),
	/* Where the box goes back to after an advert break, in seconds. The menu
	   offers nine fixed minutes and then a chooser over ten to a hundred and
	   twenty, so the floor is the smallest of the nine and the ceiling the
	   chooser's own, both in seconds. That chooser binds a local in minutes
	   rather than the field, so its label names minutes and this row is
	   seconds; the row therefore states no label. */
	intRow("adzap_zapBackPeriod")
		.section("misc")
		.range(60, 7200)
		.defaultValue(180)
		.field(COREAPI_NUMBER_FIELD(adzap_zapBackPeriod)),
	enumRow("adzap_zapOnActivation")
		.section("misc")
		.label("adzap.zap")
		.hint("menu.hint_adzap_zap")
		.defaultValue(0)
		.values(kAdzapZap)
		.field(COREAPI_NUMBER_FIELD(adzap_zapOnActivation)),
	boolRow("adzap_writeData")
		.section("misc")
		.label("adzap.writedata")
		.hint("menu.hint_adzap_writedata")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(adzap_writeData)),
	/* Where the settings backup is written. Two file browsers write it, and
	   neither names it: the one locale that carries the words is a question
	   with the directory in it and not the name of a field. */
	textRow("backup_dir")
		.section("misc")
		.defaultValue(TARGET_ROOT "/media")
		.text(kRuleDirectory)
		.field(COREAPI_TEXT_FIELD(backup_dir)),
};

} // anonymous namespace

const Descriptor *settingsTableMisc(size_t &count)
{
	count = sizeof(kMisc) / sizeof(kMisc[0]);
	return kMisc;
}

} // namespace coreapi
