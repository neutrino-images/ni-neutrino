/*
	$port: osd_setup.cpp,v 1.6 2010/09/30 20:13:59 tuxbox-cvs Exp $

	osd_setup implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2010, 2018 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

	Copyright (C) 2010, 2012-2013 Stefan Seyfried

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include "osd_setup.h"
#include "osd_helpers.h"
#include "themes.h"
#include "screensetup.h"
#include "screensaver.h"
#include "osdlang_setup.h"
#include "filebrowser.h"
#include "osd_progressbar_setup.h"

#include <coreapi/box/apply_osd.h>
#include <coreapi/osd.h>
#include <coreapi/settings/menuspec.h>
#include <coreapi/settings/predicates.h>
#include <coreapi/settings/settings.h>

#include <gui/audiomute.h>
#include <gui/color_custom.h>
#include <gui/infoclock.h>
#include <gui/infoicons.h>
#include <gui/timeosd.h>
#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/colorchooser.h>
#include <gui/widget/settingitem.h>
#include <gui/widget/stringinput.h>
#include <gui/radiotext_window.h>

#include <driver/screen_max.h>
#include <driver/neutrinofonts.h>
#include <driver/screenshot.h>
#include <driver/volume.h>
#include <driver/radiotext.h>

#include <zapit/femanager.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <system/setting_helpers.h>
#include "cs_api.h"

#include <hardware/video.h>
#include <string.h>

#ifdef ENABLE_LCD4LINUX
#include "driver/lcd4l.h"
#endif

extern CRemoteControl * g_RemoteControl;

extern const char * locale_real_names[];

static bool simulate_fe_enabled()
{
	const char *simulate_fe = getenv("SIMULATE_FE");

	return simulate_fe && simulate_fe[0] != '\0' && strcmp(simulate_fe, "0") != 0;
}
extern std::string font_file_monospace;
extern CTimeOSD *FileTimeOSD;

coreapi::Status coreapi::applicationResetLcd4lParse()
{
#ifdef ENABLE_LCD4LINUX
	CLCD4l::getInstance()->ResetParseID();
#endif
	return coreapi::Status::Ok;
}

/* The clock is made again from the settings, and the file time that is drawn in the same
   place with it. */
coreapi::Status coreapi::applicationClearInfoClock()
{
	CInfoClock::getInstance()->ClearDisplay();
	if (FileTimeOSD != NULL)
		FileTimeOSD->Init();
	return coreapi::Status::Ok;
}

/* The box in standby has the icons stopped and starts them with the wake, so a change made
   meanwhile is only the setting. */
coreapi::Status coreapi::applicationResetInfoIcons()
{
	if (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_standby)
		return coreapi::Status::Ok;
	if (g_settings.mode_icons)
		CInfoIcons::getInstance()->StartIcons();
	else
		CInfoIcons::getInstance()->StopIcons();
	return coreapi::Status::Ok;
}

/* The header, the separator and the mini TV are made again by the list's next drawing. */
coreapi::Status coreapi::applicationResetChannelList()
{
	if (CNeutrinoApp::getInstance()->channelList)
		CNeutrinoApp::getInstance()->channelList->ResetModules();
	return coreapi::Status::Ok;
}

coreapi::Status coreapi::applicationRefreshVolumeBar()
{
	CVolumeHelper::getInstance()->refresh();
	return coreapi::Status::Ok;
}

coreapi::Status coreapi::applicationRefreshMuteIcon()
{
	if (CNeutrinoApp::getInstance()->isMuted())
		CAudioMute::getInstance()->enableMuteIcon(true);
	return coreapi::Status::Ok;
}

coreapi::Status coreapi::applicationResetInfoViewer()
{
	if (g_InfoViewer == NULL)
		return coreapi::Status::Ok;
	g_InfoViewer->changePB();
	g_InfoViewer->ResetModules();
	return coreapi::Status::Ok;
}

/* The decoder is made with the radio programme, so with the setting on there is something
   to start only while one is on, and the one that runs is given the audio pid on the
   screen. With the setting off it is stopped and dropped wherever the box is. */
coreapi::Status coreapi::applicationResetRadioText()
{
	if (simulate_fe_enabled())
	{
		dprintf(DEBUG_NORMAL, "\033[33m[COsdSetup][%s - %d] SIMULATE_FE is set, no radiotext function availavble \033[0m\n", __func__, __LINE__);
		return coreapi::Status::Ok;
	}

	if (g_settings.radiotext_enable)
	{
		if (CNeutrinoApp::getInstance()->getMode() != NeutrinoModes::mode_radio)
			return coreapi::Status::Ok;

		if (g_Radiotext == NULL)
			g_Radiotext = new CRadioText;

		if (g_RadiotextWin)
		{
			delete g_RadiotextWin;
			g_RadiotextWin = NULL;
		}
		unsigned int pid = 0;
		if(!g_RemoteControl->current_PIDs.APIDs.empty())
			pid = g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].pid;

		g_Radiotext->setPid(pid);
		printf("\033[32m[COsdSetup] %s - %d: %d\033[0m\n", __func__, __LINE__, pid);
	}
	else
	{
		if (g_Radiotext)
			g_Radiotext->radiotext_stop();
		delete g_Radiotext;
		g_Radiotext = NULL;
	}
	return coreapi::Status::Ok;
}

coreapi::Status coreapi::applicationClearIconCache()
{
	CFrameBuffer::getInstance()->clearIconCache();
	return coreapi::Status::Ok;
}

/* The corners of the area the preset names, and the infobar's bars made again for them.
   The infobar builds itself from the settings when it comes, so there is nothing to do
   for it before it is there. */
coreapi::Status coreapi::applicationSetScreenGeometry()
{
	CNeutrinoApp::getInstance()->setScreenSettings();
	if (g_InfoViewer != NULL)
		g_InfoViewer->changePB();
	return coreapi::Status::Ok;
}

namespace
{
// Only the program's loop sets or reads it.
bool fonts_waiting = false;
}

void setupWaitingFonts()
{
	if (!fonts_waiting || CNeutrinoApp::getInstance()->ownPainterOpen())
		return;
	fonts_waiting = false;
	const coreapi::Status s = coreapi::applyKey("font_file");
	if (s != coreapi::Status::Ok && s != coreapi::Status::Busy)
		dprintf(DEBUG_NORMAL, "[osd setup] the fonts were not set up\n");
}

/* The shell reads the monospace face from the program's own copy, which the settings
   load fills, so a change of the face has to reach it as well. */
coreapi::Status coreapi::applicationSetupFonts(coreapi::FontSetup what)
{
	/* A write drained in the nested loop of a screen whose own thread paints would
	   delete the font under it. Not done means not recorded as sent, so the run that
	   setupWaitingFonts asks for later rebuilds. */
	if (CNeutrinoApp::getInstance()->ownPainterOpen())
	{
		fonts_waiting = true;
		return coreapi::Status::Busy;
	}

	font_file_monospace = settingsText(g_settings.font_file_monospace);

	int mode = CNeutrinoFonts::FONTSETUP_ALL;
	switch (what)
	{
		case coreapi::FontSetup::Scaling:
			mode = CNeutrinoFonts::FONTSETUP_NEUTRINO_FONT | CNeutrinoFonts::FONTSETUP_NEUTRINO_FONT_INST | CNeutrinoFonts::FONTSETUP_DYN_FONT;
			break;
		case coreapi::FontSetup::Monospace:
			mode = CNeutrinoFonts::FONTSETUP_NEUTRINO_FONT | CNeutrinoFonts::FONTSETUP_NEUTRINO_FONT_INST;
			break;
		case coreapi::FontSetup::All:
			break;
	}

	/* A web write comes here with no menu open, and the info clock paints from its
	   timer thread with a font the rebuild deletes. It stops around the rebuild, as for
	   a change of the OSD resolution, and its start takes the new font. A menu has
	   stopped it already, and drops its own header clock before the rebuild. At startup
	   there is no clock yet, and building one here would ask for fonts that do not exist. */
	CInfoClock *clock = CInfoClock::existing();
	const bool ticking = clock != NULL && !clock->isBlocked();
	if (ticking)
		clock->StopInfoClock();
	CNeutrinoApp::getInstance()->SetupFonts(mode);
	if (ticking)
		clock->StartInfoClock();
	return coreapi::Status::Ok;
}

COsdSetup::COsdSetup(int wizard_mode)
{
	frameBuffer = CFrameBuffer::getInstance();
	fontsizenotifier = new CFontSizeNotifier;
	osd_menu = NULL;
	submenu_menus = NULL;
	mfWindowSize = NULL;
	win_demo = NULL;
	osd_menu_colors = NULL;
	is_wizard = wizard_mode;

	width = 50;
}

COsdSetup::~COsdSetup()
{
	delete fontsizenotifier;
	delete win_demo;
	if (osd_menu_colors)
		delete osd_menu_colors;
}

//font settings
const SNeutrinoSettings::FONT_TYPES channellist_font_sizes[] =
{
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST,
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR,
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER,
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_EVENT,
	SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP
};
size_t channellist_font_items = sizeof(channellist_font_sizes)/sizeof(channellist_font_sizes[0]);

const SNeutrinoSettings::FONT_TYPES eventlist_font_sizes[] =
{
	//SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_EVENT
};
size_t eventlist_font_items = sizeof(eventlist_font_sizes)/sizeof(eventlist_font_sizes[0]);

const SNeutrinoSettings::FONT_TYPES infobar_font_sizes[] =
{
	SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER,
	SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME,
	SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO,
	SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL,
	SNeutrinoSettings::FONT_TYPE_INFOBAR_ECMINFO
};
size_t infobar_font_items = sizeof(infobar_font_sizes)/sizeof(infobar_font_sizes[0]);

const SNeutrinoSettings::FONT_TYPES epg_font_sizes[] =
{
	//SNeutrinoSettings::FONT_TYPE_EPG_TITLE,
	SNeutrinoSettings::FONT_TYPE_EPG_INFO1,
	SNeutrinoSettings::FONT_TYPE_EPG_INFO2,
	SNeutrinoSettings::FONT_TYPE_EPG_DATE,
	SNeutrinoSettings::FONT_TYPE_EPGPLUS_ITEM
};
size_t epg_font_items = sizeof(epg_font_sizes)/sizeof(epg_font_sizes[0]);

const SNeutrinoSettings::FONT_TYPES menu_font_sizes[] =
{
	SNeutrinoSettings::FONT_TYPE_MENU_TITLE,
	SNeutrinoSettings::FONT_TYPE_MENU,
	SNeutrinoSettings::FONT_TYPE_MENU_INFO,
	SNeutrinoSettings::FONT_TYPE_MENU_FOOT,
	SNeutrinoSettings::FONT_TYPE_MENU_HINT
};
size_t menu_font_items = sizeof(menu_font_sizes)/sizeof(menu_font_sizes[0]);

const SNeutrinoSettings::FONT_TYPES moviebrowser_font_sizes[] =
{
	SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_HEAD,
	SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_LIST,
	SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_INFO
};
size_t moviebrowser_font_items = sizeof(moviebrowser_font_sizes)/sizeof(moviebrowser_font_sizes[0]);

const SNeutrinoSettings::FONT_TYPES other_font_sizes[] =
{
	SNeutrinoSettings::FONT_TYPE_WINDOW_GENERAL,
	SNeutrinoSettings::FONT_TYPE_SUBTITLES,
	SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM,
	SNeutrinoSettings::FONT_TYPE_BUTTON_TEXT,
	SNeutrinoSettings::FONT_TYPE_WINDOW_RADIOTEXT_TITLE,
	SNeutrinoSettings::FONT_TYPE_WINDOW_RADIOTEXT_DESC
};
size_t other_font_items = sizeof(other_font_sizes)/sizeof(other_font_sizes[0]);

const SNeutrinoSettings::FONT_TYPES msgtext_font_sizes[] =
{
	SNeutrinoSettings::FONT_TYPE_MESSAGE_TEXT
};
size_t msgtext_font_items = sizeof(msgtext_font_sizes)/sizeof(msgtext_font_sizes[0]);


font_sizes_groups font_sizes_groups[] =
{
	{LOCALE_FONTMENU_MENU       , menu_font_items       , menu_font_sizes       , "fontsize.dmen", LOCALE_MENU_HINT_MENU_FONTS },
	{LOCALE_FONTMENU_CHANNELLIST, channellist_font_items, channellist_font_sizes, "fontsize.dcha", LOCALE_MENU_HINT_CHANNELLIST_FONTS },
	{LOCALE_FONTMENU_EVENTLIST  , eventlist_font_items  , eventlist_font_sizes  , "fontsize.deve", LOCALE_MENU_HINT_EVENTLIST_FONTS },
	{LOCALE_FONTMENU_EPG        , epg_font_items        , epg_font_sizes        , "fontsize.depg", LOCALE_MENU_HINT_EPG_FONTS },
	{LOCALE_FONTMENU_INFOBAR    , infobar_font_items    , infobar_font_sizes    , "fontsize.dinf", LOCALE_MENU_HINT_INFOBAR_FONTS },
	{LOCALE_FONTMENU_MOVIEBROWSER,moviebrowser_font_items,moviebrowser_font_sizes,"fontsize.dmbr", LOCALE_MENU_HINT_MOVIEBROWSER_FONTS },
	{LOCALE_FONTMENU_MESSAGES   , msgtext_font_items    , msgtext_font_sizes    , "fontsize.dmsg",  LOCALE_MENU_HINT_MESSAGE_FONTS },
	{LOCALE_FONTMENU_OTHER      , other_font_items      , other_font_sizes      , "fontsize.doth", LOCALE_MENU_HINT_OTHER_FONTS }
};
#define FONT_GROUP_COUNT (sizeof(font_sizes_groups)/sizeof(font_sizes_groups[0]))

font_sizes_struct neutrino_font[SNeutrinoSettings::FONT_TYPE_COUNT] =
{
	{LOCALE_FONTSIZE_MENU               ,  20, CNeutrinoFonts::FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_MENU_TITLE         ,  30, CNeutrinoFonts::FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_MENU_INFO          ,  16, CNeutrinoFonts::FONT_STYLE_REGULAR, 0},
	{LOCALE_FONTSIZE_MENU_FOOT          ,  14, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_EPG_TITLE          ,  25, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_EPG_INFO1          ,  17, CNeutrinoFonts::FONT_STYLE_ITALIC , 2},
	{LOCALE_FONTSIZE_EPG_INFO2          ,  17, CNeutrinoFonts::FONT_STYLE_REGULAR, 2},
	{LOCALE_FONTSIZE_EPG_DATE           ,  15, CNeutrinoFonts::FONT_STYLE_REGULAR, 2},
	{LOCALE_FONTSIZE_EPGPLUS_ITEM       ,  17, CNeutrinoFonts::FONT_STYLE_REGULAR, 2},
	{LOCALE_FONTSIZE_EVENTLIST_TITLE    ,  30, CNeutrinoFonts::FONT_STYLE_REGULAR, 0},
	{LOCALE_FONTSIZE_EVENTLIST_ITEMLARGE,  20, CNeutrinoFonts::FONT_STYLE_BOLD   , 1},
	{LOCALE_FONTSIZE_EVENTLIST_ITEMSMALL,  14, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_EVENTLIST_DATETIME ,  16, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_EVENTLIST_EVENT    ,  17, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_CHANNELLIST        ,  20, CNeutrinoFonts::FONT_STYLE_BOLD   , 1},
	{LOCALE_FONTSIZE_CHANNELLIST_DESCR  ,  20, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_CHANNELLIST_NUMBER ,  14, CNeutrinoFonts::FONT_STYLE_BOLD   , 2},
	{LOCALE_FONTSIZE_CHANNELLIST_EVENT  ,  17, CNeutrinoFonts::FONT_STYLE_REGULAR, 2},
	{LOCALE_FONTSIZE_CHANNEL_NUM_ZAP    ,  40, CNeutrinoFonts::FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_INFOBAR_NUMBER     ,  50, CNeutrinoFonts::FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_INFOBAR_CHANNAME   ,  30, CNeutrinoFonts::FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_INFOBAR_INFO       ,  20, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_INFOBAR_SMALL      ,  14, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_INFOBAR_ECMINFO    ,  15, CNeutrinoFonts::FONT_STYLE_REGULAR, 0},
	{LOCALE_FONTSIZE_FILEBROWSER_ITEM   ,  17, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_MENU_HINT          ,  16, CNeutrinoFonts::FONT_STYLE_REGULAR, 0},
	{LOCALE_FONTSIZE_MOVIEBROWSER_HEAD  ,  14, CNeutrinoFonts::FONT_STYLE_REGULAR, 2},
	{LOCALE_FONTSIZE_MOVIEBROWSER_LIST  ,  20, CNeutrinoFonts::FONT_STYLE_REGULAR, 0},
	{LOCALE_FONTSIZE_MOVIEBROWSER_INFO  ,  16, CNeutrinoFonts::FONT_STYLE_REGULAR, 0},
	{LOCALE_FONTSIZE_SUBTITLES          ,  25, CNeutrinoFonts::FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_MESSAGE_TEXT       ,  20, CNeutrinoFonts::FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_BUTTON_TEXT        ,  14, CNeutrinoFonts::FONT_STYLE_REGULAR, 0},
	{LOCALE_FONTSIZE_GENERAL_WINDOW_TEXT,  20, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_WINDOW_RADIOTEXT_DESC0, 22, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_WINDOW_RADIOTEXT_DESC1 , 17, CNeutrinoFonts::FONT_STYLE_REGULAR, 1}
};

// The rows of the timeout screen, in the order it lists them.
static const char *const kTimingKeys[] =
{
	"timing.menu", "timing.chanlist", "timing.epg", "timing.volumebar", "timing.filebrowser",
	"timing.numericzap", "timing.popup_messages", "timing.static_messages"
};

static const char *const kInfobarTimingKeys[] =
{
	"timing.infobar_tv", "timing.infobar_radio", "timing.infobar_media_audio", "timing.infobar_media_video"
};

int COsdSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init osd setup\n");

	printf("COsdSetup::exec:: action  %s\n", actionKey.c_str());
	if(parent != NULL)
		parent->hide();

	int res = menu_return::RETURN_REPAINT;
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	if (actionKey=="window_size")
	{
		int old_window_width = g_settings.window_width;
		int old_window_height = g_settings.window_height;

		paintWindowSize(old_window_width, old_window_height);

		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

		bool loop=true;
		while (loop)
		{
			g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true);

			if (msg <= CRCInput::RC_MaxRC)
				timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

			if (msg == CRCInput::RC_ok)
			{
				memset(window_size_value, 0, sizeof(window_size_value));
				snprintf(window_size_value, sizeof(window_size_value), "%d / %d", g_settings.window_width, g_settings.window_height);
				mfWindowSize->setOption(window_size_value);
				CNeutrinoApp::getInstance()->channelList->ResetModules();
				break;
			}
			else if (CNeutrinoApp::getInstance()->backKey(msg) || (msg == CRCInput::RC_timeout))
			{
				g_settings.window_width = old_window_width;
				g_settings.window_height = old_window_height;
				loop = false;
			}
			else if ((msg == CRCInput::RC_page_up) || (msg == CRCInput::RC_page_down) ||
				(msg == CRCInput::RC_left) || (msg == CRCInput::RC_right) ||
				(msg == CRCInput::RC_up) || (msg == CRCInput::RC_down))
			{

				int dir = 1;
				if ((msg == CRCInput::RC_page_down) || (msg == CRCInput::RC_left) || (msg == CRCInput::RC_down))
					dir = -1;

				int mask = 3;
				if ((msg == CRCInput::RC_left) || (msg == CRCInput::RC_right))
					mask = 1;
				else if ((msg == CRCInput::RC_up) || (msg == CRCInput::RC_down))
					mask = 2;
				if (mask & 1)
					g_settings.window_width += dir;
				if (mask & 2)
					g_settings.window_height += dir;

				paintWindowSize(g_settings.window_width, g_settings.window_height);

			}
			else if ((msg == CRCInput::RC_left) || (msg == CRCInput::RC_right))
			{
			}
			else if (msg > CRCInput::RC_MaxRC)
			{
				if (CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all)
				{
					loop = false;
					res = menu_return::RETURN_EXIT_ALL;
				}
			}
		}
		win_demo->kill();

		return res;
	}
	else if (actionKey=="osd.def")
	{
		// The defaults are the rows' own, written like any other change.
		std::vector<std::string> keys(kTimingKeys, kTimingKeys + sizeof(kTimingKeys) / sizeof(kTimingKeys[0]));
		keys.insert(keys.end(), kInfobarTimingKeys, kInfobarTimingKeys + sizeof(kInfobarTimingKeys) / sizeof(kInfobarTimingKeys[0]));
		coreapi::settings::Refusals refused;
		coreapi::settings::resetDefaults(keys, refused, true);
		return res;
	}
	else if(strncmp(actionKey.c_str(), "fontsize.d", 10) == 0)
	{
		for (unsigned int i = 0; i < FONT_GROUP_COUNT; i++)
		{
			if (actionKey == font_sizes_groups[i].actionkey)
			{
				for (unsigned int j = 0; j < font_sizes_groups[i].count; j++)
				{
					SNeutrinoSettings::FONT_TYPES k = font_sizes_groups[i].content[j];
					CNeutrinoApp::getInstance()->getConfigFile()->setInt32(locale_real_names[neutrino_font[k].name], neutrino_font[k].defaultsize);
				}
				break;
			}
		}
		fontsizenotifier->changeNotify(NONEXISTANT_LOCALE, NULL);
		return res;
	}

	res = showOsdSetup();

	//return menu_return::RETURN_REPAINT;
	return res;
}

// show osd setup
int COsdSetup::showOsdSetup()
{
	int shortcut = 1;

	// osd main menu
	osd_menu = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP);
	osd_menu->setWizardMode(is_wizard);

	// intro with subhead and back button
	osd_menu->addIntroItems(LOCALE_MAINSETTINGS_OSD);

	// item menu colors
	if (osd_menu_colors == NULL)
	{
		osd_menu_colors = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_COLORS, width, MN_WIDGET_ID_OSDSETUP_MENUCOLORS);
		showOsdMenueColorSetup(osd_menu_colors);
	}
	CMenuForwarder * mf = new CMenuForwarder(LOCALE_COLORMENU_MENUCOLORS, true, NULL, osd_menu_colors, NULL, CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_COLORS);
	osd_menu->addItem(mf);

	// fonts
	CMenuWidget osd_menu_fonts(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_FONT);
	showOsdFontSizeSetup(&osd_menu_fonts);
	mf = new CMenuForwarder(LOCALE_FONTMENU_HEAD, true, NULL, &osd_menu_fonts, NULL, CRCInput::RC_green);
	mf->setHint("", LOCALE_MENU_HINT_FONTS);
	osd_menu->addItem(mf);

	// timeouts
	CMenuWidget osd_menu_timing(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_TIMEOUT);
	showOsdTimeoutSetup(&osd_menu_timing);
	mf = new CMenuForwarder(LOCALE_COLORMENU_TIMING, true, NULL, &osd_menu_timing, NULL, CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_TIMEOUTS);
	osd_menu->addItem(mf);

	// screen
	CMenuWidget osd_menu_screen(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_SCREEN);
	showOsdScreenSetup(&osd_menu_screen);
	mf = new CMenuForwarder(LOCALE_SCREEN_MENU, true, NULL, &osd_menu_screen, NULL, CRCInput::RC_blue);
	mf->setHint("", LOCALE_MENU_HINT_SCREEN);
	osd_menu->addItem(mf);

	// menus
	CMenuWidget osd_menu_menus(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_MENUS);
	showOsdMenusSetup(&osd_menu_menus);
	mf = new CMenuForwarder(LOCALE_SETTINGS_MENUS, true, NULL, &osd_menu_menus, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_MENUS);
	osd_menu->addItem(mf);

	// progressbar
	mf = new CMenuDForwarder(LOCALE_MISCSETTINGS_PROGRESSBAR, true, NULL, new CProgressbarSetup(), NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_PROGRESSBAR);
	osd_menu->addItem(mf);

	// channellogos
	CMenuWidget osd_menu_channellogos(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_CHANNELLOGOS);
	showOsdChannellogosSetup(&osd_menu_channellogos);
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_CHANNELLOGOS, true, NULL, &osd_menu_channellogos, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_CHANNELLOGOS_SETUP);
	osd_menu->addItem(mf);

	// infobar
	CMenuWidget osd_menu_infobar(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_INFOBAR);
	showOsdInfobarSetup(&osd_menu_infobar);
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_INFOBAR, true, NULL, &osd_menu_infobar, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_INFOBAR_SETUP);
	osd_menu->addItem(mf);

	// channellist
	CMenuWidget osd_menu_chanlist(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_CHANNELLIST);
	showOsdChanlistSetup(&osd_menu_chanlist);
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_CHANNELLIST, true, NULL, &osd_menu_chanlist, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_CHANNELLIST_SETUP);
	osd_menu->addItem(mf);

	// eventlist
	CMenuWidget osd_menu_eventlist(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_EVENTLIST);
	showOsdEventlistSetup(&osd_menu_eventlist);
	mf = new CMenuForwarder(LOCALE_EVENTLIST_NAME, true, NULL, &osd_menu_eventlist, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_EVENTLIST_SETUP);
	osd_menu->addItem(mf);

	// volume
	CMenuWidget osd_menu_volume(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_VOLUME);
	showOsdVolumeSetup(&osd_menu_volume);
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_VOLUME, true, NULL, &osd_menu_volume, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_VOLUME);
	osd_menu->addItem(mf);

	// info clock
	CMenuWidget osd_menu_infoclock(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_INFOCLOCK);
	showOsdInfoclockSetup(&osd_menu_infoclock);
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_INFOCLOCK, true, NULL, &osd_menu_infoclock, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_INFOCLOCK_SETUP);
	osd_menu->addItem(mf);

#ifdef SCREENSHOT
	// screenshot
	CMenuWidget osd_menu_screenshot(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_SCREENSHOT);
	showOsdScreenShotSetup(&osd_menu_screenshot);
	mf = new CMenuForwarder(LOCALE_SCREENSHOT_MENU, true, NULL, &osd_menu_screenshot, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_SCREENSHOT_SETUP);
	osd_menu->addItem(mf);
#endif

	// screensaver
	CMenuWidget osd_menu_screensaver(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_SCREENSAVER);
	showOsdScreensaverSetup(&osd_menu_screensaver);
	mf = new CMenuForwarder(LOCALE_SCREENSAVER_MENU, true, NULL, &osd_menu_screensaver, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_SCREENSAVER_SETUP);
	osd_menu->addItem(mf);

	osd_menu->addItem(GenericMenuSeparatorLine);

	// radiotext
	addSetting(osd_menu, "radiotext_enable");

	// scrambled
	addSetting(osd_menu, "scrambled_message");

#ifdef ENABLE_CHANGE_OSD_RESOLUTION
	// osd resolution, for the video standards that take the larger size
	addSetting(osd_menu, "osd_resolution",
		   []() { return coreapi::drawsOsd720() && coreapi::drawsOsd1080() &&
				 coreapi::osd::videoSystemNeeds1080(COsdHelpers::getInstance()->getVideoSystem()); },
		   this);
#endif

	// the picture fix of the SCART output, which only the first preset can switch
	CMenuItem *scart = addSetting(osd_menu, "flag_scart_osd_fix", []() { return !g_settings.screen_preset; }, this);
	if (scart != NULL)
		scart->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_SCART_OSD_FIX);

	// fade windows
	addSetting(osd_menu, "widget_fade");

	// window size
	memset(window_size_value, 0, sizeof(window_size_value));
	snprintf(window_size_value, sizeof(window_size_value), "%d / %d", g_settings.window_width, g_settings.window_height);
	mfWindowSize = new CMenuForwarder(LOCALE_WINDOW_SIZE, true, window_size_value, this, "window_size", CRCInput::convertDigitToKey(shortcut++));
	mfWindowSize->setHint("", LOCALE_MENU_HINT_WINDOW_SIZE);
	osd_menu->addItem(mfWindowSize);

	// subchannel menu position
	addSetting(osd_menu, "infobar_subchan_disp_pos");

#ifdef ENABLE_CHANGE_OSD_RESOLUTION
	/* A size switch clears the screen and redraws at the new size, so the menu
	   is hidden first, at the geometry it was drawn with. A call that leaves
	   the size alone does not hide it. */
	sigc::connection hide_before_switch = COsdHelpers::getInstance()->OnBeforeResizeOsd.connect(sigc::mem_fun(osd_menu, &CMenuWidget::hide));
#endif
	int res = osd_menu->exec(NULL, "");
#ifdef ENABLE_CHANGE_OSD_RESOLUTION
	hide_before_switch.disconnect();
#endif

	delete osd_menu;
	return res;
}

// A colour row with the preview the chooser draws it on, if it has one.
static void addColorSetting(CMenuWidget *menu, const char *key, int gradient = CColorChooser::gradient_none)
{
	CMenuItem *item = addSetting(menu, key);
	if (item != NULL && gradient != CColorChooser::gradient_none)
		static_cast<CSettingColorItem *>(item)->colorChooser()->setGradient(gradient);
}

// menue colors
void COsdSetup::showOsdMenueColorSetup(CMenuWidget *menu_colors)
{
	menu_colors->addIntroItems(LOCALE_COLORMENU_MENUCOLORS);

	CMenuForwarder * mf = new CMenuForwarder(LOCALE_COLORMENU_THEMESELECT, true, NULL, CThemes::getInstance(), NULL, CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_THEME);
	menu_colors->addItem(mf);

	sigc::slot0<void> slot_repaint = sigc::mem_fun(menu_colors, &CMenuWidget::paint); //we want to repaint after changed Option

	CMenuOptionChooser *oj;

	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUHEAD));

	addColorSetting(menu_colors, "theme.menu_Head", CColorChooser::gradient_head_body);
	addColorSetting(menu_colors, "theme.menu_Head_Text", CColorChooser::gradient_head_text);

	// head color gradient //TODO: disable sub options if head gradient is disabled
	oj = addChoiceSetting(menu_colors, "menu_Head_gradient");
	oj->OnAfterChangeOption.connect(slot_repaint);

	// head color gradient direction
	oj = addChoiceSetting(menu_colors, "menu_Head_gradient_direction");
	oj->OnAfterChangeOption.connect(slot_repaint);

	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUSUBTITLE_BAR));

	// sub head color gradient
	oj = addChoiceSetting(menu_colors, "menu_SubHead_gradient");
	oj->OnAfterChangeOption.connect(slot_repaint);

	// sub head color gradient direction
	oj = addChoiceSetting(menu_colors, "menu_SubHead_gradient_direction");
	oj->OnAfterChangeOption.connect(slot_repaint);

	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUCONTENT));

	addColorSetting(menu_colors, "theme.menu_Content");
	addColorSetting(menu_colors, "theme.menu_Content_Text");

	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUCONTENT_INACTIVE));
	addColorSetting(menu_colors, "theme.menu_Content_inactive");
	addColorSetting(menu_colors, "theme.menu_Content_inactive_Text");

	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUCONTENT_SELECTED));
	addColorSetting(menu_colors, "theme.menu_Content_Selected");
	addColorSetting(menu_colors, "theme.menu_Content_Selected_Text");

	// footer
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUFOOT));
	addColorSetting(menu_colors, "theme.menu_Foot");

	// footer text
	addColorSetting(menu_colors, "theme.menu_Foot_Text");

	// hintbox color gradient
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORTHEMEMENU_MENU_HINTS));
	oj = addChoiceSetting(menu_colors, "menu_Hint_gradient");
	oj->OnAfterChangeOption.connect(slot_repaint);

	// hintbox color gradient direction
	oj = addChoiceSetting(menu_colors, "menu_Hint_gradient_direction");
	oj->OnAfterChangeOption.connect(slot_repaint);

	// infoviewer color
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORSTATUSBAR_TEXT));
	addColorSetting(menu_colors, "theme.infobar");
	addColorSetting(menu_colors, "theme.infobar_Text");

	// infoviewer gradient top
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::EMPTY));
	addChoiceSetting(menu_colors, "infobar_gradient_top");

	// infoviewer gradient top direction
	addChoiceSetting(menu_colors, "infobar_gradient_top_direction");

	// infoviewer gradient body
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::EMPTY));
	addChoiceSetting(menu_colors, "infobar_gradient_body");

	// infoviewer gradient body direction
	addChoiceSetting(menu_colors, "infobar_gradient_body_direction");

	// infoviewer gradient bottom
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::EMPTY));
	addChoiceSetting(menu_colors, "infobar_gradient_bottom");

	// infoviewer gradient bottom direction
	addChoiceSetting(menu_colors, "infobar_gradient_bottom_direction");

	// ca bar
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::EMPTY));
	addColorSetting(menu_colors, "theme.infobar_casystem");

	// channellist
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MAINMENU_CHANNELS));
	addColorSetting(menu_colors, "theme.channellist_Description_Text");

	// colored events
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MISCSETTINGS_COLORED_EVENTS));
	addColorSetting(menu_colors, "theme.colored_events");

	// colored events channellist
	addChoiceSetting(menu_colors, "colored_events_channellist");

	// colored events infobar
	addChoiceSetting(menu_colors, "colored_events_infobar");

	// progressbar
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MISCSETTINGS_PROGRESSBAR));

	// progressbar passive
	addColorSetting(menu_colors, "theme.progressbar_passive");

	// progressbar active
	addColorSetting(menu_colors, "theme.progressbar_active");

	// shadow
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE| CMenuSeparator::STRING, LOCALE_COLORTHEMEMENU_MISC));

	addColorSetting(menu_colors, "theme.shadow");

	// menue separator line gradient enable
	oj = addChoiceSetting(menu_colors, "menu_Separator_gradient_enable");
	oj->OnAfterChangeOption.connect(slot_repaint);

	// message frame
	addChoiceSetting(menu_colors, "message_frame_enable");

	// round corners
	oj = addChoiceSetting(menu_colors, "rounded_corners", true, this);
	oj->OnAfterChangeOption.connect(sigc::mem_fun(menu_colors, &CMenuWidget::hide));
}

/* for font size setup */
class CMenuNumberInput : public CMenuForwarder, CMenuTarget, CChangeObserver
{
private:
	CChangeObserver * observer;
	CConfigFile     * configfile;
	int32_t           defaultvalue;
	std::string       value;

protected:

	std::string getOption(fb_pixel_t * bgcol __attribute__((unused)) = NULL)
	{
		return to_string(configfile->getInt32(locale_real_names[name], defaultvalue));
	}

	virtual bool changeNotify(const neutrino_locale_t OptionName, void * Data)
	{
		configfile->setInt32(locale_real_names[name], atoi(value.c_str()));
		return observer->changeNotify(OptionName, Data);
	}


public:
	CMenuNumberInput(const neutrino_locale_t Text, const int32_t DefaultValue, CChangeObserver * const Observer, CConfigFile * const Configfile) : CMenuForwarder(Text, true, NULL, this)
	{
		observer     = Observer;
		configfile   = Configfile;
		defaultvalue = DefaultValue;
	}

	int exec(CMenuTarget * parent, const std::string & action_Key)
	{
		value = getOption();
		while (value.length() < 3)
			value = " " + value;
		CStringInput input(name, &value, 3, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2, "0123456789 ", this);
		input.forceSaveScreen(true);
		return input.exec(parent, action_Key);
	}

	std::string &getValue(void)
	{
		value = getOption();
		return value;
	}
};

void COsdSetup::AddFontSettingItem(CMenuWidget &font_Settings, const SNeutrinoSettings::FONT_TYPES number_of_fontsize_entry)
{
	CMenuNumberInput *ni = new CMenuNumberInput(neutrino_font[number_of_fontsize_entry].name, neutrino_font[number_of_fontsize_entry].defaultsize, fontsizenotifier, CNeutrinoApp::getInstance()->getConfigFile());
	font_Settings.addItem(ni);
}

// font settings menu
void COsdSetup::showOsdFontSizeSetup(CMenuWidget *menu_fonts)
{
	CMenuWidget *fontSettings = menu_fonts;
	CMenuForwarder * mf;

	fontSettings->addIntroItems(LOCALE_FONTMENU_HEAD);

	addSetting(fontSettings, "font_file", true, NULL, CRCInput::RC_red);
	addSetting(fontSettings, "font_file_monospace", true, NULL, CRCInput::RC_green);

	fontSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_FONTMENU_SIZES));

	// submenu font scaling
	CMenuWidget *fontScaling = new CMenuWidget(LOCALE_FONTMENU_HEAD, NEUTRINO_ICON_COLORS, width, MN_WIDGET_ID_OSDSETUP_FONTSCALE);
	fontScaling->addIntroItems(LOCALE_FONTMENU_SCALING);
	/* The rebuild drops the header and footer of every menu and measures the items with the
	   new fonts, so the menu that is running is painted whole again after it. */
	CMenuOptionNumberChooser *scaling = addNumberSetting(fontScaling, "font_scaling_x", true, NULL, CRCInput::RC_nokey, false, true);
	if (scaling != NULL)
		afterApply(scaling, []() { return true; });
	scaling = addNumberSetting(fontScaling, "font_scaling_y", true, NULL, CRCInput::RC_nokey, false, true);
	if (scaling != NULL)
		afterApply(scaling, []() { return true; });
	mf = new CMenuDForwarder(LOCALE_FONTMENU_SCALING, true, NULL, fontScaling, NULL, CRCInput::RC_blue);
	mf->setHint("", LOCALE_MENU_HINT_FONT_SCALING);
	fontSettings->addItem(mf);

	mn_widget_id_t w_index = MN_WIDGET_ID_OSDSETUP_FONTSIZE_MENU;
	for (unsigned int i = 0; i < FONT_GROUP_COUNT; i++)
	{
		CMenuWidget *fontSettingsSubMenu = new CMenuWidget(LOCALE_FONTMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, w_index);

		fontSettingsSubMenu->addIntroItems(font_sizes_groups[i].groupname);

		for (unsigned int j = 0; j < font_sizes_groups[i].count; j++)
		{
			AddFontSettingItem(*fontSettingsSubMenu, font_sizes_groups[i].content[j]);
		}
		fontSettingsSubMenu->addItem(GenericMenuSeparatorLine);
		fontSettingsSubMenu->addItem(new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, font_sizes_groups[i].actionkey));

		mf = new CMenuDForwarder(font_sizes_groups[i].groupname, true, NULL, fontSettingsSubMenu, "", CRCInput::convertDigitToKey(i+1));
		mf->setHint("", font_sizes_groups[i].hint);
		fontSettings->addItem(mf);
		w_index++;
	}
	g_InfoViewer->ResetModules();
}

// osd timeouts
void COsdSetup::showOsdTimeoutSetup(CMenuWidget* menu_timeout)
{
	menu_timeout->addIntroItems(LOCALE_COLORMENU_TIMING);

	for (size_t i = 0; i < sizeof(kTimingKeys) / sizeof(kTimingKeys[0]); i++)
		addNumberSetting(menu_timeout, kTimingKeys[i]);

	menu_timeout->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_TIMING_INFOBAR));

	for (size_t i = 0; i < sizeof(kInfobarTimingKeys) / sizeof(kInfobarTimingKeys[0]); i++)
	{
		// A row names a number only at its floor, and nought is above this one.
		CMenuOptionNumberChooser *ch = addNumberSetting(menu_timeout, kInfobarTimingKeys[i]);
		if (ch != NULL)
			ch->setLocalizedValue(0, LOCALE_TIMING_OFF);
	}

	menu_timeout->addItem(GenericMenuSeparatorLine);
	menu_timeout->addItem(new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, "osd.def", CRCInput::RC_red));
}

// menus
void COsdSetup::showOsdMenusSetup(CMenuWidget *menu_menus)
{
	submenu_menus = menu_menus;

	submenu_menus->addIntroItems(LOCALE_SETTINGS_MENUS);
	// menu position
	addSetting(submenu_menus, "menu_pos", true, this);

	// menu hints
	addSetting(submenu_menus, "show_menu_hints", true, this);

	// menu hints line (details_line) should always be last entry here
	CMenuItem *mc = addSetting(submenu_menus, "show_menu_hints_line", true, this);
	if (mc != NULL)
		mc->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_MENU_HINTS_LINE);
}

// channellogos
void COsdSetup::showOsdChannellogosSetup(CMenuWidget *menu_channellogos)
{
	menu_channellogos->addIntroItems(LOCALE_MISCSETTINGS_CHANNELLOGOS);

	// logo directory
	addSetting(menu_channellogos, "logo_hdd_dir");

	menu_channellogos->addItem(GenericMenuSeparatorLine);

	// show channellogos
	addSetting(menu_channellogos, "channellist_show_channellogo");

	// show eventlogos
	addSetting(menu_channellogos, "channellist_show_eventlogo");
}

// infobar
void COsdSetup::showOsdInfobarSetup(CMenuWidget *menu_infobar)
{
	menu_infobar->addIntroItems(LOCALE_MISCSETTINGS_INFOBAR);

	// show on epg change
	addSetting(menu_infobar, "infobar_show");

	// buttons usertitle
	addSetting(menu_infobar, "infobar_buttons_usertitle");

	// analog clock
	addSetting(menu_infobar, "infobar_analogclock");

	// weather
	addSetting(menu_infobar, "infobar_weather");

	menu_infobar->addItem(GenericMenuSeparator);

	// display options
	addSetting(menu_infobar, "infobar_show_channellogo");

	// satellite/cable provider
	addSetting(menu_infobar, "infobar_sat_display");

	menu_infobar->addItem(GenericMenuSeparator);

	// CA system
	addSetting(menu_infobar, "infobar_casystem_display");

	// CA system frame
	addSetting(menu_infobar, "infobar_casystem_frame");

	// ecm-Info
	CMenuItem *mc = addSetting(menu_infobar, "show_ecm_pos");
	if (mc != NULL)
		mc->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_INFOBAR_ECMINFO);

	menu_infobar->addItem(GenericMenuSeparator);

	// flash/hdd statfs
	addSetting(menu_infobar, "infobar_show_sysfs_hdd");

	// hdd statfs update
	addSetting(menu_infobar, "hdd_statfs_mode");

	// tuner icon
	addSetting(menu_infobar, "infobar_show_tuner");

	// resolution
	addSetting(menu_infobar, "infobar_show_res");

	// DD icon
	addSetting(menu_infobar, "infobar_show_dd_available");

	menu_infobar->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MISCSETTINGS_PROGRESSBAR));

	// progressbar position
	addSetting(menu_infobar, "infobar_progressbar");
}

// channellist
void COsdSetup::showOsdChanlistSetup(CMenuWidget *menu_chanlist)
{
	menu_chanlist->addIntroItems(LOCALE_MISCSETTINGS_CHANNELLIST);

	// channellist additional
	addSetting(menu_chanlist, "channellist_additional");

	// epg align
	addSetting(menu_chanlist, "channellist_epgtext_alignment");

	// show resolution icon
	addSetting(menu_chanlist, "channellist_show_res_icon");

	// extended channel list
	addSetting(menu_chanlist, "progressbar_design_channellist");

	// show infobox
	addSetting(menu_chanlist, "channellist_show_infobox");

	// foot
	addSetting(menu_chanlist, "channellist_foot");

	// show numbers
	addSetting(menu_chanlist, "channellist_show_numbers");
}

// eventlist
void COsdSetup::showOsdEventlistSetup(CMenuWidget *menu_eventlist)
{
	menu_eventlist->addIntroItems(LOCALE_EVENTLIST_NAME);

	// eventlist additional
	addSetting(menu_eventlist, "eventlist_additional");

	// epgplus in eventlist
	addSetting(menu_eventlist, "eventlist_epgplus");
}

// volume
void COsdSetup::showOsdVolumeSetup(CMenuWidget *menu_volume)
{
	menu_volume->addIntroItems(LOCALE_MISCSETTINGS_VOLUME);

	// volume position
	addSetting(menu_volume, "volume_pos");

	// volume size
	int vMin = CVolumeHelper::getInstance()->getVolIconHeight();
	g_settings.volume_size = std::max(g_settings.volume_size, vMin);
	CMenuOptionNumberChooser * nc = new CMenuOptionNumberChooser(LOCALE_EXTRA_VOLUME_SIZE, &g_settings.volume_size, true, vMin, 50, this);
	nc->setHint("", LOCALE_MENU_HINT_VOLUME_SIZE);
	menu_volume->addItem(nc);

	// volume digits
	addSetting(menu_volume, "volume_digits");

	// show mute at volume 0
	addSetting(menu_volume, "show_mute_icon");
}

// info clock
void COsdSetup::showOsdInfoclockSetup(CMenuWidget *menu_infoclock)
{
	menu_infoclock->addIntroItems(LOCALE_MISCSETTINGS_INFOCLOCK);

	addSetting(menu_infoclock, "mode_clock", true, NULL, CRCInput::RC_red);

	menu_infoclock->addItem(GenericMenuSeparatorLine);

	// size of info clock
	addSetting(menu_infoclock, "infoClockFontSize");

	// clock with seconds
	addSetting(menu_infoclock, "infoClockSeconds");

	// clock with background
	addSetting(menu_infoclock, "infoClockBackground");

	// digit color
	addSetting(menu_infoclock, "theme.clock_Digit");
}

/* The hide clears the hint only while hints are on, and the item has written the new
   flag by the time its observer is told, so the old one is put back for the hide. */
void COsdSetup::hideAtOldValue(int &flag)
{
	const int now = flag;
	flag = now ? 0 : 1;
	submenu_menus->hide();
	flag = now;
}

bool COsdSetup::changeNotify(const neutrino_locale_t OptionName, void * data)
{
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_SETTINGS_MENU_POS))
	{
		submenu_menus->hide();
		return true;
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_SETTINGS_MENU_HINTS))
	{
		hideAtOldValue(g_settings.show_menu_hints);
		return true;
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_COLORMENU_OSD_PRESET))
	{
		// Hidden at the area it was drawn in, before the group moves the corners.
		osd_menu->hide();
		return true;
	}
#ifdef ENABLE_CHANGE_OSD_RESOLUTION
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_COLORMENU_OSD_RESOLUTION))
	{
		// The write through the declaration has already kept the copy and
		// switched the size, hiding the menu first where the size changes.
#if 0
		if (frameBuffer->fullHdAvailable()) {
			if (frameBuffer->osd_resolutions.empty())
				return true;

			size_t index = (size_t)*(int*)data;
			size_t resCount = frameBuffer->osd_resolutions.size();
			if (index >= resCount)
				index = 0;

			uint32_t resW = frameBuffer->osd_resolutions[index].xRes;
			uint32_t resH = frameBuffer->osd_resolutions[index].yRes;
			uint32_t bpp  = frameBuffer->osd_resolutions[index].bpp;
			int switchFB = frameBuffer->setMode(resW, resH, bpp);

			if (switchFB == 0) {
//printf("\n>>>>>[%s:%d] New res: %dx%dx%d\n \n", __func__, __LINE__, resW, resH, bpp);
				osd_menu->hide();
				frameBuffer->Clear();
				CNeutrinoApp::getInstance()->setScreenSettings();
				CNeutrinoApp::getInstance()->SetupFonts(CNeutrinoFonts::FONTSETUP_NEUTRINO_FONT);
				CVolumeHelper::getInstance()->refresh();
				CInfoClock::getInstance()->ClearDisplay();
				FileTimeOSD->Init();
				if (CNeutrinoApp::getInstance()->channelList)
					CNeutrinoApp::getInstance()->channelList->ResetModules();
				if (g_InfoViewer)
					g_InfoViewer->ResetModules();
			}
		}
#endif
		return true;
	}
#endif
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_EXTRA_ROUNDED_CORNERS))
	{
		osd_menu->hide();
		return true;
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_SCART_OSD_FIX))
	{
		/* The fix moves the area and the fonts with it, which the settings layer states as a
		   coupling of the flag, so the flag goes in as a write of the layer. */
		std::vector<std::pair<std::string, std::string> > members(1, std::make_pair(std::string("flag_scart_osd_fix"), std::string(*(int *) data ? "1" : "0")));
		coreapi::settings::Refusals failed;
		coreapi::settings::writeBatch(members, failed, true);
		// The screen is cleared once the corners are moved, as the notifier did.
		frameBuffer->Clear();
		return true;
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_EXTRA_VOLUME_SIZE))
	{
		// A hand-built item, since its floor is the height of the icon the box loaded.
		const coreapi::Status st = coreapi::applyKey("volume_size");
		if (st != coreapi::Status::Ok && st != coreapi::Status::Busy)
			dprintf(DEBUG_NORMAL, "[COsdSetup] volume_size: apply failed\n");
		return false;
	}
	// menu_hints_line
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_SETTINGS_MENU_HINTS_LINE))
	{
		hideAtOldValue(g_settings.show_menu_hints_line);
		return true;
	}
	return false;
}


int COsdSetup::showContextChanlistMenu(CChannelList *parent_channellist)
{
	static int cselected = -1;

	CMenuWidget * menu_chanlist = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width);

	// using native callback to ensure stop header clock in parent channellist before paint this menu window
	if (parent_channellist && parent_channellist->getHeaderObject()->getClockObject())
		menu_chanlist->OnBeforePaint.connect(sigc::mem_fun(parent_channellist->getHeaderObject()->getClockObject(), &CComponentsFrmClock::block));

	menu_chanlist->enableSaveScreen(true);
	menu_chanlist->enableFade(false);
	menu_chanlist->setSelected(cselected);

	showOsdChanlistSetup(menu_chanlist);
	menu_chanlist->addItem(new CMenuSeparator(CMenuSeparator::LINE));

	CMenuWidget *fontSettingsSubMenu = new CMenuWidget(LOCALE_FONTMENU_HEAD, NEUTRINO_ICON_KEYBINDING);
	fontSettingsSubMenu->enableSaveScreen(true);
	fontSettingsSubMenu->enableFade(false);

	int i = 1;
	fontSettingsSubMenu->addIntroItems(font_sizes_groups[i].groupname);//, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL);

	for (unsigned int j = 0; j < font_sizes_groups[i].count; j++)
	{
		AddFontSettingItem(*fontSettingsSubMenu, font_sizes_groups[i].content[j]);
	}
	fontSettingsSubMenu->addItem(GenericMenuSeparatorLine);
	fontSettingsSubMenu->addItem(new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, font_sizes_groups[i].actionkey));

	CMenuForwarder * mf = new CMenuDForwarder(LOCALE_FONTMENU_HEAD, true, NULL, fontSettingsSubMenu, NULL, CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_FONTS);
	menu_chanlist->addItem(mf);

	int res = menu_chanlist->exec(NULL, "");
	cselected = menu_chanlist->getSelected();
	delete menu_chanlist;
	return res;
}

void COsdSetup::showOsdScreenSetup(CMenuWidget *menu_screen)
{
	CMenuForwarder *mf = NULL;

	menu_screen->addIntroItems(LOCALE_SCREEN_MENU);

	// screen
	mf = new CMenuForwarder(LOCALE_VIDEOMENU_SCREENSETUP, true, NULL, new CScreenSetup, NULL, CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_SCREENSETUP);
	menu_screen->addItem(mf);

	// monitor
	addSetting(menu_screen, "screen_preset", true, this, CRCInput::RC_green);
}

#ifdef SCREENSHOT
//screenshot
void COsdSetup::showOsdScreenShotSetup(CMenuWidget *menu_screenshot)
{
	menu_screenshot->addIntroItems(LOCALE_SCREENSHOT_MENU);

	if ((uint)g_settings.key_screenshot == CRCInput::RC_nokey)
		menu_screenshot->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_SCREENSHOT_INFO));

	addSetting(menu_screenshot, "screenshot_dir");

	addSetting(menu_screenshot, "screenshot_count");

	addSetting(menu_screenshot, "screenshot_format");

	addSetting(menu_screenshot, "screenshot_mode");

	addSetting(menu_screenshot, "screenshot_video");

	addSetting(menu_screenshot, "screenshot_scale");

	addSetting(menu_screenshot, "screenshot_cover");
}
#endif


void COsdSetup::showOsdScreensaverSetup(CMenuWidget *menu_screensaver)
{
	menu_screensaver->addIntroItems(LOCALE_SCREENSAVER_MENU);

	// screensaver delay
	addSetting(menu_screensaver, "screensaver_delay");

	// screensaver mode
	addSetting(menu_screensaver, "screensaver_mode");

	// screensaver timeout
	addSetting(menu_screensaver, "screensaver_timeout");

	// screensaver_dir
	addSetting(menu_screensaver, "screensaver_dir");

	// screensaver random mode
	addSetting(menu_screensaver, "screensaver_random");
}

// The size is kept to the bounds of its row, which are the ones a write from outside is held to.
static int withinRow(const char *key, int value)
{
	coreapi::Result<coreapi::MenuItemSpec> row = coreapi::menuItem(key);
	if (!row.ok())
		return value;
	return std::min((int) row.value().max, std::max((int) row.value().min, value));
}

void COsdSetup::paintWindowSize(int w, int h)
{
	if (win_demo == NULL)
	{
		win_demo = new CComponentsShapeSquare(0, 0, 0, 0);
		win_demo->setFrameThickness(OFFSET_INNER_MID);
		win_demo->disableShadow();
		win_demo->setColorBody(COL_BACKGROUND);
		win_demo->setColorFrame(COL_RED);
		win_demo->doPaintBg(true);
	}
	else
	{
		if (win_demo->isPainted())
			win_demo->kill();
	}

	g_settings.window_width = withinRow("window_width", w);
	g_settings.window_height = withinRow("window_height", h);

	win_demo->setWidth(frameBuffer->getWindowWidth());
	win_demo->setHeight(frameBuffer->getWindowHeight());
	win_demo->setXPos(getScreenStartX(win_demo->getWidth()));
	win_demo->setYPos(getScreenStartY(win_demo->getHeight()));

	win_demo->paint(CC_SAVE_SCREEN_NO);
}
