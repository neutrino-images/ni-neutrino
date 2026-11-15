/*
 * script constants
 *
 * The lists both bindings carried: script_constants.h says what they are and
 * what of them stays with a binding.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "script_constants.h"

#include <config.h>
#include <cstddef>

#include <global.h>
#include <neutrino.h>
#include <system/settings.h>
#include <driver/framebuffer.h>
#include <driver/neutrinofonts.h>
#include <driver/rcinput.h>
#include <gui/color_custom.h>
#include <gui/movieplayer.h>
#include <gui/components/cc.h>
#include <gui/widget/menue.h>
#include <gui/widget/textbox.h>

#include "script_color.h"
#include "script_curl.h"
#include "script_misc.h"
#include "script_neutrino.h"

typedef ScriptConstants::Number Number;
typedef ScriptConstants::Text Text;

template <size_t N>
static void add(std::vector<ScriptConstants::Table> &out, const char *name, const Number (&list)[N], bool colors)
{
	ScriptConstants::Table table;
	table.name = name;
	table.colors = colors;
	out.push_back(table);
	out.back().entries.assign(list, list + N);
}

void ScriptConstants::numbers(std::vector<Table> &out)
{
	out.clear();

	/* remote control keys, exported as RC['ok'] */
	const Number keyname[] =
	{
		{ "0",			CRCInput::RC_0 },
		{ "1",			CRCInput::RC_1 },
		{ "2",			CRCInput::RC_2 },
		{ "3",			CRCInput::RC_3 },
		{ "4",			CRCInput::RC_4 },
		{ "5",			CRCInput::RC_5 },
		{ "6",			CRCInput::RC_6 },
		{ "7",			CRCInput::RC_7 },
		{ "8",			CRCInput::RC_8 },
		{ "9",			CRCInput::RC_9 },
		{ "backspace",		CRCInput::RC_backspace },
		{ "up",			CRCInput::RC_up },
		{ "left",		CRCInput::RC_left },
		{ "right",		CRCInput::RC_right },
		{ "down",		CRCInput::RC_down },
		{ "spkr",		CRCInput::RC_spkr },
		{ "minus",		CRCInput::RC_minus },
		{ "plus",		CRCInput::RC_plus },
		{ "standby",		CRCInput::RC_standby },
		{ "help",		CRCInput::RC_help },
		{ "home",		CRCInput::RC_home },
		{ "back",		CRCInput::RC_back },
		{ "setup",		CRCInput::RC_setup },
		{ "topleft",		CRCInput::RC_topleft },
		{ "topright",		CRCInput::RC_topright },
		{ "page_up",		CRCInput::RC_page_up },
		{ "page_down",		CRCInput::RC_page_down },
		{ "ok",			CRCInput::RC_ok },
		{ "red",		CRCInput::RC_red },
		{ "green",		CRCInput::RC_green },
		{ "yellow",		CRCInput::RC_yellow },
		{ "blue",		CRCInput::RC_blue },
		{ "top_left",		CRCInput::RC_top_left },
		{ "top_right",		CRCInput::RC_top_right },
		{ "bottom_left",	CRCInput::RC_bottom_left },
		{ "bottom_right",	CRCInput::RC_bottom_right },
		{ "audio",		CRCInput::RC_audio },
		{ "video",		CRCInput::RC_video },
		{ "tv",			CRCInput::RC_tv },
		{ "radio",		CRCInput::RC_radio },
		{ "text",		CRCInput::RC_text },
		{ "info",		CRCInput::RC_info },
		{ "epg",		CRCInput::RC_epg },
		{ "recall",		CRCInput::RC_recall },
		{ "favorites",		CRCInput::RC_favorites },
		{ "sat",		CRCInput::RC_sat },
		{ "sat2",		CRCInput::RC_sat2 },
		{ "record",		CRCInput::RC_record },
		{ "play",		CRCInput::RC_play },
		{ "playpause",		CRCInput::RC_playpause },
		{ "pause",		CRCInput::RC_pause },
		{ "forward",		CRCInput::RC_forward },
		{ "rewind",		CRCInput::RC_rewind },
		{ "stop",		CRCInput::RC_stop },
		{ "timeshift",		CRCInput::RC_timeshift },
		{ "mode",		CRCInput::RC_mode },
		{ "games",		CRCInput::RC_games },
		{ "next",		CRCInput::RC_next },
		{ "prev",		CRCInput::RC_prev },
		{ "www",		CRCInput::RC_www },
		{ "sub",		CRCInput::RC_sub },
		{ "pos",		CRCInput::RC_pos },
		{ "sleep",		CRCInput::RC_sleep },
		{ "nextsong",		CRCInput::RC_nextsong },
		{ "previoussong",	CRCInput::RC_previoussong },
		{ "bookmarks",		CRCInput::RC_bookmarks },
		{ "program",		CRCInput::RC_program },
		{ "power_on",		CRCInput::RC_power_on },
		{ "power_off",		CRCInput::RC_power_off },
		{ "standby_on",		CRCInput::RC_standby_on },
		{ "standby_off",	CRCInput::RC_standby_off },
		{ "mute_on",		CRCInput::RC_mute_on },
		{ "mute_off",		CRCInput::RC_mute_off },
		{ "analog_on",		CRCInput::RC_analog_on },
		{ "analog_off",		CRCInput::RC_analog_off },
		{ "timeout",		(long long)CRCInput::RC_timeout },
		/* to check if it is in our range */
		{ "MaxRC",		CRCInput::RC_MaxRC },
	};
	add(out, "RC", keyname, false);

	long long xRes = (long long)CFrameBuffer::getInstance()->getScreenWidth(true);
	long long yRes = (long long)CFrameBuffer::getInstance()->getScreenHeight(true);
	const Number screenopts[] =
	{
		{ "OFF_X", g_settings.screen_StartX },
		{ "OFF_Y", g_settings.screen_StartY },
		{ "END_X", g_settings.screen_EndX },
		{ "END_Y", g_settings.screen_EndY },
		{ "X_RES", xRes },
		{ "Y_RES", yRes },
	};
	add(out, "SCREEN", screenopts, false);

	/* the _TYPE_ in these names is redundant, exported as FONT['MENU'] */
	const Number fontlist[] =
	{
		{ "MENU",			SNeutrinoSettings::FONT_TYPE_MENU },
		{ "MENU_TITLE",			SNeutrinoSettings::FONT_TYPE_MENU_TITLE },
		{ "MENU_INFO",			SNeutrinoSettings::FONT_TYPE_MENU_INFO },
		{ "MENU_FOOT",			SNeutrinoSettings::FONT_TYPE_MENU_FOOT },
		{ "EPG_TITLE",			SNeutrinoSettings::FONT_TYPE_EPG_TITLE },
		{ "EPG_INFO1",			SNeutrinoSettings::FONT_TYPE_EPG_INFO1 },
		{ "EPG_INFO2",			SNeutrinoSettings::FONT_TYPE_EPG_INFO2 },
		{ "EPG_DATE",			SNeutrinoSettings::FONT_TYPE_EPG_DATE },
		{ "EPGPLUS_ITEM",		SNeutrinoSettings::FONT_TYPE_EPGPLUS_ITEM },
		{ "EVENTLIST_TITLE",		SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE },
		{ "EVENTLIST_ITEMLARGE",	SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE },
		{ "EVENTLIST_ITEMSMALL",	SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL },
		{ "EVENTLIST_DATETIME",		SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME },
		{ "EVENTLIST_EVENT",		SNeutrinoSettings::FONT_TYPE_EVENTLIST_EVENT },
		{ "CHANNELLIST",		SNeutrinoSettings::FONT_TYPE_CHANNELLIST },
		{ "CHANNELLIST_DESCR",		SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR },
		{ "CHANNELLIST_NUMBER",		SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER },
		{ "CHANNELLIST_EVENT",		SNeutrinoSettings::FONT_TYPE_CHANNELLIST_EVENT },
		{ "CHANNEL_NUM_ZAP",		SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP },
		{ "INFOBAR_NUMBER",		SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER },
		{ "INFOBAR_CHANNAME",		SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME },
		{ "INFOBAR_INFO",		SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO },
		{ "INFOBAR_SMALL",		SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL },
		{ "FILEBROWSER_ITEM",		SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM },
		{ "MENU_HINT",			SNeutrinoSettings::FONT_TYPE_MENU_HINT },
		{ "MOVIEBROWSER_HEAD",		SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_HEAD },
		{ "MOVIEBROWSER_LIST",		SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_LIST },
		{ "MOVIEBROWSER_INFO",		SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_INFO },
		{ "SUBTITLES",			SNeutrinoSettings::FONT_TYPE_SUBTITLES },
		{ "MESSAGE_TEXT",		SNeutrinoSettings::FONT_TYPE_MESSAGE_TEXT },
		{ "BUTTON_TEXT",		SNeutrinoSettings::FONT_TYPE_BUTTON_TEXT },
		{ "WINDOW_GENERAL",		SNeutrinoSettings::FONT_TYPE_WINDOW_GENERAL },
		{ "RADIOTEXT_TITLE",		SNeutrinoSettings::FONT_TYPE_WINDOW_RADIOTEXT_TITLE },
		{ "WINDOW_RADIOTEXT_DESC",	SNeutrinoSettings::FONT_TYPE_WINDOW_RADIOTEXT_DESC },
	};
	add(out, "FONT", fontlist, false);

	const Number corners[] =
	{
		{ "TOP_LEFT",		CORNER_TOP_LEFT },
		{ "TOP_RIGHT",		CORNER_TOP_RIGHT },
		{ "BOTTOM_LEFT",	CORNER_BOTTOM_LEFT },
		{ "BOTTOM_RIGHT",	CORNER_BOTTOM_RIGHT },
		/* those depend on g_settings.theme.rounded_corners */
		{ "RADIUS_LARGE",	RADIUS_LARGE },
		{ "RADIUS_MID",		RADIUS_MID },
		{ "RADIUS_SMALL",	RADIUS_SMALL },
		{ "RADIUS_MIN",		RADIUS_MIN },
		{ "RADIUS_NONE",	RADIUS_NONE },
	};
	add(out, "CORNER", corners, false);

	/* offsets, exported as OFFSET['SHADOW'] */
	const Number offsets[] =
	{
		{ "SHADOW",		OFFSET_SHADOW },
		{ "INTER",		OFFSET_INTER },
		{ "INNER_LARGE",	OFFSET_INNER_LARGE },
		{ "INNER_MID",		OFFSET_INNER_MID },
		{ "INNER_SMALL",	OFFSET_INNER_SMALL },
		{ "INNER_MIN",		OFFSET_INNER_MIN },
		{ "INNER_NONE",		OFFSET_INNER_NONE },
	};
	add(out, "OFFSET", offsets, false);

	const Number menureturn[] =
	{
		{ "NONE",		menu_return::RETURN_NONE },
		{ "REPAINT",		menu_return::RETURN_REPAINT },
		{ "EXIT",		menu_return::RETURN_EXIT },
		{ "EXIT_ALL",		menu_return::RETURN_EXIT_ALL },
		{ "EXIT_REPAINT",	menu_return::RETURN_EXIT_REPAINT },
	};
	add(out, "MENU_RETURN", menureturn, false);

	const Number playstate[] =
	{
		{ "NORMAL",		CMoviePlayerGui::PLUGIN_PLAYSTATE_NORMAL },
		{ "STOP",		CMoviePlayerGui::PLUGIN_PLAYSTATE_STOP },
		{ "NEXT",		CMoviePlayerGui::PLUGIN_PLAYSTATE_NEXT },
		{ "PREV",		CMoviePlayerGui::PLUGIN_PLAYSTATE_PREV },
		{ "LEAVE_ALL",		CMoviePlayerGui::PLUGIN_PLAYSTATE_LEAVE_ALL },
	};
	add(out, "PLAYSTATE", playstate, false);

	const Number ccomponents[] =
	{
		{ "SHADOW_OFF",		(long long)CC_SHADOW_OFF },
		{ "SHADOW_ON",		(long long)CC_SHADOW_ON },
		{ "SAVE_SCREEN_YES",	(long long)CC_SAVE_SCREEN_YES },
		{ "SAVE_SCREEN_NO",	(long long)CC_SAVE_SCREEN_NO },
	};
	add(out, "CC", ccomponents, false);

	const Number dynfont[] =
	{
		{ "STYLE_REGULAR",	(long long)CNeutrinoFonts::FONT_STYLE_REGULAR },
		{ "STYLE_BOLD",		(long long)CNeutrinoFonts::FONT_STYLE_BOLD },
		{ "STYLE_ITALIC",	(long long)CNeutrinoFonts::FONT_STYLE_ITALIC },
		{ "MAX",		(long long)CNeutrinoFonts::DYNFONTEXT_MAX },
		{ "MAXIMUM_FONTS",	(long long)ScriptNeutrino::DynFontNoRoom },
		{ "TO_WIDE",		(long long)ScriptNeutrino::DynFontTooWide },
		{ "TOO_WIDE",		(long long)ScriptNeutrino::DynFontTooWide },
		{ "TOO_HIGH",		(long long)ScriptNeutrino::DynFontTooHigh },
	};
	add(out, "DYNFONT", dynfont, false);

	const Number curl_status[] =
	{
		{ "OK",			ScriptCurl::OK },
		{ "ERR_HANDLE",		ScriptCurl::ERR_HANDLE },
		{ "ERR_NO_URL",		ScriptCurl::ERR_NO_URL },
		{ "ERR_CREATE_FILE",	ScriptCurl::ERR_CREATE_FILE },
		{ "ERR_CURL",		ScriptCurl::ERR_CURL },
	};
	add(out, "CURL", curl_status, false);

	const Number neutrino_mode[] =
	{
		{ "UNKNOWN",	(long long)NeutrinoModes::mode_unknown },
		{ "TV",		(long long)NeutrinoModes::mode_tv },
		{ "RADIO",	(long long)NeutrinoModes::mode_radio },
		{ "AVINPUT",	(long long)NeutrinoModes::mode_avinput },
		{ "STANDBY",	(long long)NeutrinoModes::mode_standby },
		{ "AUDIO",	(long long)NeutrinoModes::mode_audio },
		{ "PIC",	(long long)NeutrinoModes::mode_pic },
		{ "TS",		(long long)NeutrinoModes::mode_ts },
		{ "OFF",	(long long)NeutrinoModes::mode_off },
		{ "WEBTV",	(long long)NeutrinoModes::mode_webtv },
		{ "WEBRADIO",	(long long)NeutrinoModes::mode_webradio },
		{ "MASK",	(long long)NeutrinoModes::mode_mask },
		{ "NOREZAP",	(long long)NeutrinoModes::norezap },
	};
	add(out, "NMODE", neutrino_mode, false);

	const Number post_msg[] =
	{
		{ "STANDBY_ON",	(long long)ScriptMisc::POSTMSG_STANDBY_ON },
	};
	add(out, "POSTMSG", post_msg, false);

	/* possible text alignment modes usable for window header caption */
	const Number text_alignment[] =
	{
		{ "DEFAULT",	(long long)CTextBox::NO_AUTO_LINEBREAK },
		{ "CENTER",	(long long)CTextBox::CENTER },
		{ "RIGHT",	(long long)CTextBox::RIGHT },
	};
	add(out, "TEXT_ALIGNMENT", text_alignment, false);

	/* list of colors, exported as COL['INFOBAR_SHADOW'] */
	const Number colorlist[] =
	{
		{ "WHITE",			(long long)(COL_WHITE) },
		{ "BLACK",			(long long)(COL_BLACK) },
		{ "DARK_RED",			(long long)(COL_DARK_RED) },
		{ "RED",			(long long)(COL_RED) },
		{ "LIGHT_RED",			(long long)(COL_LIGHT_RED) },
		{ "DARK_GREEN",			(long long)(COL_DARK_GREEN) },
		{ "GREEN",			(long long)(COL_GREEN) },
		{ "LIGHT_GREEN",		(long long)(COL_LIGHT_GREEN) },
		{ "DARK_YELLOW",		(long long)(COL_DARK_YELLOW) },
		{ "YELLOW",			(long long)(COL_YELLOW) },
		{ "LIGHT_YELLOW",		(long long)(COL_LIGHT_YELLOW) },
		{ "DARK_BLUE",			(long long)(COL_DARK_BLUE) },
		{ "BLUE",			(long long)(COL_BLUE) },
		{ "LIGHT_BLUE",			(long long)(COL_LIGHT_BLUE) },
		{ "DARK_GRAY",			(long long)(COL_DARK_GRAY) },
		{ "GRAY",			(long long)(COL_GRAY) },
		{ "LIGHT_GRAY",			(long long)(COL_LIGHT_GRAY) },

		{ "COLORED_EVENTS_CHANNELLIST",	(long long)(MAGIC_COLOR | (COL_COLORED_EVENTS_CHANNELLIST)) },
		{ "COLORED_EVENTS_INFOBAR",	(long long)(MAGIC_COLOR | (COL_COLORED_EVENTS_INFOBAR)) },
		{ "SHADOW",			(long long)(MAGIC_COLOR | (COL_SHADOW)) },
		/* obsolete */
		{ "INFOBAR_SHADOW",		(long long)(MAGIC_COLOR | (COL_SHADOW)) },
		{ "INFOBAR",			(long long)(MAGIC_COLOR | (COL_INFOBAR)) },
		{ "MENUHEAD",			(long long)(MAGIC_COLOR | (COL_MENUHEAD)) },
		{ "MENUCONTENT",		(long long)(MAGIC_COLOR | (COL_MENUCONTENT)) },
		{ "MENUCONTENTDARK",		(long long)(MAGIC_COLOR | (COL_MENUCONTENTDARK)) },
		{ "MENUCONTENTSELECTED",	(long long)(MAGIC_COLOR | (COL_MENUCONTENTSELECTED)) },
		{ "MENUCONTENTINACTIVE",	(long long)(MAGIC_COLOR | (COL_MENUCONTENTINACTIVE)) },
		{ "MENUFOOT",			(long long)(MAGIC_COLOR | (COL_MENUFOOT)) },
		{ "FRAME",			(long long)(MAGIC_COLOR | (COL_FRAME)) },
		{ "SCROLLBAR",			(long long)(MAGIC_COLOR | (COL_SCROLLBAR)) },
		{ "SCROLLBAR_ACTIVE",		(long long)(MAGIC_COLOR | (COL_SCROLLBAR_ACTIVE)) },
		{ "SCROLLBAR_PASSIVE",		(long long)(MAGIC_COLOR | (COL_SCROLLBAR_PASSIVE)) },
		{ "PROGRESSBAR_ACTIVE",		(long long)(MAGIC_COLOR | (COL_PROGRESSBAR_ACTIVE_PLUS_0)) },
		{ "PROGRESSBAR_PASSIVE",	(long long)(MAGIC_COLOR | (COL_PROGRESSBAR_PASSIVE_PLUS_0)) },
		{ "BACKGROUND",			(long long)(MAGIC_COLOR | (COL_BACKGROUND)) },

		{ "COLORED_EVENTS_TEXT",	(long long)(COL_COLORED_EVENTS_TEXT) },
		{ "INFOBAR_TEXT",		(long long)(COL_INFOBAR_TEXT) },
		/* obsolete */
		{ "INFOBAR_SHADOW_TEXT",	(long long)(COL_MENUFOOT_TEXT) },
		{ "MENUHEAD_TEXT",		(long long)(COL_MENUHEAD_TEXT) },
		{ "MENUCONTENT_TEXT",		(long long)(COL_MENUCONTENT_TEXT) },
		{ "MENUCONTENT_TEXT_PLUS_1",	(long long)(COL_MENUCONTENT_TEXT_PLUS_1) },
		{ "MENUCONTENT_TEXT_PLUS_2",	(long long)(COL_MENUCONTENT_TEXT_PLUS_2) },
		{ "MENUCONTENT_TEXT_PLUS_3",	(long long)(COL_MENUCONTENT_TEXT_PLUS_3) },
		{ "MENUCONTENTDARK_TEXT",	(long long)(COL_MENUCONTENTDARK_TEXT) },
		{ "MENUCONTENTDARK_TEXT_PLUS_1",(long long)(COL_MENUCONTENTDARK_TEXT_PLUS_1) },
		{ "MENUCONTENTDARK_TEXT_PLUS_2",(long long)(COL_MENUCONTENTDARK_TEXT_PLUS_2) },
		{ "MENUCONTENTSELECTED_TEXT",	(long long)(COL_MENUCONTENTSELECTED_TEXT) },
		{ "MENUCONTENTSELECTED_TEXT_PLUS_1",	(long long)(COL_MENUCONTENTSELECTED_TEXT_PLUS_1) },
		{ "MENUCONTENTSELECTED_TEXT_PLUS_2",	(long long)(COL_MENUCONTENTSELECTED_TEXT_PLUS_2) },
		{ "MENUCONTENTINACTIVE_TEXT",	(long long)(COL_MENUCONTENTINACTIVE_TEXT) },
		{ "MENUFOOT_TEXT",		(long long)(COL_MENUFOOT_TEXT) },
		{ "SHADOW_PLUS_0",		(long long)(COL_SHADOW_PLUS_0) },
		{ "MENUHEAD_PLUS_0",		(long long)(COL_MENUHEAD_PLUS_0) },
		{ "MENUCONTENT_PLUS_0",		(long long)(COL_MENUCONTENT_PLUS_0) },
		{ "MENUCONTENT_PLUS_1",		(long long)(COL_MENUCONTENT_PLUS_1) },
		{ "MENUCONTENT_PLUS_2",		(long long)(COL_MENUCONTENT_PLUS_2) },
		{ "MENUCONTENT_PLUS_3",		(long long)(COL_MENUCONTENT_PLUS_3) },
		{ "MENUCONTENT_PLUS_4",		(long long)(COL_MENUCONTENT_PLUS_4) },
		{ "MENUCONTENT_PLUS_5",		(long long)(COL_MENUCONTENT_PLUS_5) },
		{ "MENUCONTENT_PLUS_6",		(long long)(COL_MENUCONTENT_PLUS_6) },
		{ "MENUCONTENT_PLUS_7",		(long long)(COL_MENUCONTENT_PLUS_7) },
		{ "MENUCONTENTDARK_PLUS_0",	(long long)(COL_MENUCONTENTDARK_PLUS_0) },
		{ "MENUCONTENTDARK_PLUS_2",	(long long)(COL_MENUCONTENTDARK_PLUS_2) },
		{ "MENUCONTENTSELECTED_PLUS_0",	(long long)(COL_MENUCONTENTSELECTED_PLUS_0) },
		{ "MENUCONTENTSELECTED_PLUS_2",	(long long)(COL_MENUCONTENTSELECTED_PLUS_2) },
		{ "MENUCONTENTINACTIVE_PLUS_0",	(long long)(COL_MENUCONTENTINACTIVE_PLUS_0) },
		{ "MENUFOOT_PLUS_0",		(long long)(COL_MENUFOOT_PLUS_0) },
		{ "FRAME_PLUS_0",		(long long)(COL_FRAME_PLUS_0) },
		{ "SCROLLBAR_PLUS_0",		(long long)(COL_SCROLLBAR_PLUS_0) },
		{ "SCROLLBAR_ACTIVE_PLUS_0",	(long long)(COL_SCROLLBAR_ACTIVE_PLUS_0) },
		{ "SCROLLBAR_PASSIVE_PLUS_0",	(long long)(COL_SCROLLBAR_PASSIVE_PLUS_0) },
		{ "PROGRESSBAR_ACTIVE_PLUS_0",	(long long)(COL_PROGRESSBAR_ACTIVE_PLUS_0) },
		{ "PROGRESSBAR_PASSIVE_PLUS_0",	(long long)(COL_PROGRESSBAR_PASSIVE_PLUS_0) },
	};
	add(out, "COL", colorlist, true);
}

void ScriptConstants::directories(std::vector<Text> &out)
{
	static const Text list[] =
	{
		{ "CONFIGDIR",		CONFIGDIR },
		{ "ZAPITDIR",		ZAPITDIR },
		{ "DATADIR",		DATADIR },
		{ "DATADIR_VAR",	DATADIR_VAR },
		{ "CONTROLDIR",		CONTROLDIR },
		{ "CONTROLDIR_VAR",	CONTROLDIR_VAR },
		{ "FONTDIR",		FONTDIR },
		{ "FONTDIR_VAR",	FONTDIR_VAR },
		{ "LIBDIR",		LIBDIR },
		{ "GAMESDIR",		GAMESDIR },
		{ "ICONSDIR",		ICONSDIR },
		{ "ICONSDIR_VAR",	ICONSDIR_VAR },
		{ "LOCALEDIR",		LOCALEDIR },
		{ "LOCALEDIR_VAR",	LOCALEDIR_VAR },
		{ "PLUGINDIR",		PLUGINDIR },
		{ "PLUGINDIR_MNT",	PLUGINDIR_MNT },
		{ "PLUGINDIR_VAR",	PLUGINDIR_VAR },
		{ "LUAPLUGINDIR",	LUAPLUGINDIR },
		{ "LUAPLUGINDIR_VAR",	LUAPLUGINDIR_VAR },
		{ "THEMESDIR",		THEMESDIR },
		{ "THEMESDIR_VAR",	THEMESDIR_VAR },
		{ "WEBRADIODIR",	WEBRADIODIR },
		{ "WEBRADIODIR_VAR",	WEBRADIODIR_VAR },
		{ "WEBTVDIR",		WEBTVDIR },
		{ "WEBTVDIR_VAR",	WEBTVDIR_VAR },
		{ "LOGODIR",		LOGODIR },
		{ "LOGODIR_VAR",	LOGODIR_VAR },
		{ "PRIVATE_HTTPDDIR",	PRIVATE_HTTPDDIR },
		{ "PUBLIC_HTTPDDIR",	PUBLIC_HTTPDDIR },
		{ "HOSTED_HTTPDDIR",	HOSTED_HTTPDDIR },
		{ "FLAGDIR",		FLAGDIR },
	};

	out.assign(list, list + sizeof(list) / sizeof(list[0]));
}
