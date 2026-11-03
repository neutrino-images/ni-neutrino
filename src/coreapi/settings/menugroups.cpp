/*
 * menugroups.cpp - the headings the box's own menu files the settings under
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

namespace coreapi
{

/* Read off the screens under src/gui, each section's headings numbered in the order the
   menu that offers the section shows them. The cases hold the heading every row names, and
   this order, to those screens again on every run (test/unit/scan/groups.awk).

   One heading per submenu of that menu, named by the text the submenu is introduced with,
   or the titled separator where the menu files rows straight under one. A submenu inside a
   submenu is not a heading of its own: its rows stand under the one that leads to it, so a
   page has one level of them, the level a person walks first on the screen. Three sections
   are arranged the other way round on the screen and take their headings from there: the
   HDMI-CEC rows live in the video section while the screen offers them from the
   miscellaneous menu, the preferred languages are a submenu whose two separators tell its
   rows apart, and the key bindings are a submenu whose separators file the keys the
   submenus beside them do not. */

// video
const MenuGroup kVideoModes = { "video", "videomenu.enabled_modes", 1 };
const MenuGroup kVideoAutoModes = { "video", "videomenu.enabled_modes_auto", 2 };
const MenuGroup kVideoPip = { "video", "videomenu.pip", 3 };
const MenuGroup kVideoQuadPip = { "video", "quadpip", 4 };
const MenuGroup kVideoPsi = { "video", "videomenu.psi", 5 };
const MenuGroup kVideoCec = { "video", "videomenu.hdmi_cec", 6 };

// audio
const MenuGroup kAudioVolumeAdjustment = { "audio", "audiomenu.volume_adjustment", 1 };

// general
const MenuGroup kGeneralAudioLanguages = { "general", "audiomenu.pref_lang_head", 1 };
const MenuGroup kGeneralSubtitleLanguages = { "general", "audiomenu.pref_subs_head", 2 };

// channel
const MenuGroup kChannelStart = { "channel", "zapitsetup.head", 1 };
const MenuGroup kChannelWebTv = { "channel", "webtv.head", 2 };
const MenuGroup kChannelWebRadio = { "channel", "webradio.head", 3 };

// network
const MenuGroup kNetworkNtp = { "network", "networkmenu.ntptitle", 1 };
const MenuGroup kNetworkProxy = { "network", "flashupdate.proxyserver_sep", 2 };

// recording
const MenuGroup kRecordingTimeshift = { "recording", "recordingmenu.timeshift", 1 };
const MenuGroup kRecordingTimer = { "recording", "timersettings.separator", 2 };
const MenuGroup kRecordingAudioPids = { "recording", "recordingmenu.apids", 3 };
const MenuGroup kRecordingDataPids = { "recording", "recordingmenu.data_pids", 4 };
const MenuGroup kRecordingMovieBrowser = { "recording", "moviebrowser.head", 5 };

// player
const MenuGroup kPlayerAudio = { "player", "audioplayer_internetradio.name", 1 };
const MenuGroup kPlayerPictures = { "player", "pictureviewer.head", 2 };
const MenuGroup kPlayerMovies = { "player", "mainmenu.movieplayer", 3 };

// update
const MenuGroup kUpdateImage = { "update", "flashupdate.createimage_menu", 1 };

// hdd
const MenuGroup kHddManage = { "hdd", "hdd_manage", 1 };

// cam
const MenuGroup kCamDaemons = { "cam", "camd_control", 1 };

// keybindings
const MenuGroup kKeysModes = { "keybindings", "keybindingmenu.modechange", 1 };
const MenuGroup kKeysChannelList = { "keybindings", "keybindingmenu.channellist", 2 };
const MenuGroup kKeysQuickzap = { "keybindings", "keybindingmenu.quickzap", 3 };
const MenuGroup kKeysMoviePlayer = { "keybindings", "mainmenu.movieplayer", 4 };
const MenuGroup kKeysMovieBrowser = { "keybindings", "moviebrowser.head", 5 };
const MenuGroup kKeysVideo = { "keybindings", "keybindingmenu.video", 6 };
const MenuGroup kKeysNavigation = { "keybindings", "keybindingmenu.navigation", 7 };
const MenuGroup kKeysVolume = { "keybindings", "keybindingmenu.volume", 8 };
const MenuGroup kKeysMisc = { "keybindings", "keybindingmenu.misc", 9 };
const MenuGroup kKeysSpecial = { "keybindings", "keybindingmenu.special_active", 10 };

// display
const MenuGroup kDisplayLeds = { "display", "ledcontroler.menu", 1 };
const MenuGroup kDisplayBrightness = { "display", "lcdmenu.lcdcontroler", 2 };
const MenuGroup kDisplayBacklight = { "display", "ledcontroler.backlight", 3 };
const MenuGroup kDisplayGlcd = { "display", "glcd.head", 4 };
const MenuGroup kDisplayLcd4l = { "display", "lcd4l_support", 5 };

// osd
const MenuGroup kOsdColors = { "osd", "colormenu.menucolors", 1 };
const MenuGroup kOsdFonts = { "osd", "fontmenu.head", 2 };
const MenuGroup kOsdTiming = { "osd", "colormenu.timing", 3 };
const MenuGroup kOsdScreen = { "osd", "screen.menu", 4 };
const MenuGroup kOsdMenus = { "osd", "settings.menus", 5 };
const MenuGroup kOsdProgressbar = { "osd", "miscsettings.progressbar", 6 };
const MenuGroup kOsdLogos = { "osd", "miscsettings.channellogos", 7 };
const MenuGroup kOsdInfobar = { "osd", "miscsettings.infobar", 8 };
const MenuGroup kOsdChannelList = { "osd", "miscsettings.channellist", 9 };
const MenuGroup kOsdEventList = { "osd", "eventlist.name", 10 };
const MenuGroup kOsdVolume = { "osd", "miscsettings.volume", 11 };
const MenuGroup kOsdClock = { "osd", "miscsettings.infoclock", 12 };
const MenuGroup kOsdScreenshot = { "osd", "screenshot.menu", 13 };
const MenuGroup kOsdScreensaver = { "osd", "screensaver.menu", 14 };
const MenuGroup kOsdInfoIcons = { "osd", "infoicons_head", 15 };

// misc
const MenuGroup kMiscGeneral = { "misc", "miscsettings.general", 1 };
const MenuGroup kMiscEnergy = { "misc", "miscsettings.energy", 2 };
const MenuGroup kMiscEpg = { "misc", "miscsettings.epg_head", 3 };
const MenuGroup kMiscFileBrowser = { "misc", "filebrowser.head", 4 };
const MenuGroup kMiscChannelList = { "misc", "miscsettings.channellist", 5 };
const MenuGroup kMiscOnline = { "misc", "miscsettings.onlineservices", 6 };
const MenuGroup kMiscCpu = { "misc", "miscsettings.cpu", 7 };
const MenuGroup kMiscPlugins = { "misc", "plugins.control", 8 };
const MenuGroup kMiscStreaming = { "misc", "miscsettings.streaming", 9 };
const MenuGroup kMiscEventFinder = { "misc", "eventfinder.head", 10 };
const MenuGroup kMiscAdzap = { "misc", "adzap", 11 };
const MenuGroup kMiscDaemons = { "misc", "daemon_control", 12 };
const MenuGroup kMiscAccess = { "misc", "personalize.head", 13 };
const MenuGroup kMiscUserMenus = { "misc", "usermenus.head", 14 };
const MenuGroup kMiscPluginTypes = { "misc", "personalize.usermenu_plugin_types", 15 };

} // namespace coreapi
