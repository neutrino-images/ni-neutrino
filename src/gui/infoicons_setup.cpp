/*
	infoicons_setup

	(C) 2009-2016 NG-Team
	(C) 2016 NI-Team

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

#include <gui/infoicons_setup.h>

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <neutrino_menue.h>
#include <gui/widget/hintbox.h>
#include <system/helpers.h>

#include <driver/screen_max.h>

#include <gui/infoicons.h>

typedef struct menu_data_t
{
	neutrino_locale_t caption;
	neutrino_locale_t caption_hint;
	const char * action;
} menu_data_struct;

const menu_data_t menu_settings[MODE_ICONS_NR_OF_ENTRIES]=
{
	{LOCALE_INFOICONS_FLAG_NAME0, LOCALE_MENU_HINT_INFOICONS_FLAG_NAME0, "del_flag0"},
	{LOCALE_INFOICONS_FLAG_NAME1, LOCALE_MENU_HINT_INFOICONS_FLAG_NAME1, "del_flag1"},
	{LOCALE_INFOICONS_FLAG_NAME2, LOCALE_MENU_HINT_INFOICONS_FLAG_NAME2, "del_flag2"},
	{LOCALE_INFOICONS_FLAG_NAME3, LOCALE_MENU_HINT_INFOICONS_FLAG_NAME3, "del_flag3"},
	{LOCALE_INFOICONS_FLAG_NAME4, LOCALE_MENU_HINT_INFOICONS_FLAG_NAME4, "del_flag4"},
	{LOCALE_INFOICONS_FLAG_NAME5, LOCALE_MENU_HINT_INFOICONS_FLAG_NAME5, "del_flag5"},
	{LOCALE_INFOICONS_FLAG_NAME6, LOCALE_MENU_HINT_INFOICONS_FLAG_NAME6, "del_flag6"},
	{LOCALE_INFOICONS_FLAG_NAME7, LOCALE_MENU_HINT_INFOICONS_FLAG_NAME7, "del_flag7"}
};

#define OPTIONS_INFOICONS_MODE_OPTION_COUNT 3
const CMenuOptionChooser::keyval OPTIONS_INFOICONS_MODE_OPTIONS[OPTIONS_INFOICONS_MODE_OPTION_COUNT] =
{
	{ INFOICONS_STATIC, 	LOCALE_INFOICONS_STATIC		},
	{ INFOICONS_INFOVIEWER, LOCALE_INFOICONS_INFOVIEWER 	},
	{ INFOICONS_POPUP,	LOCALE_INFOICONS_POPUP 		}
};

CInfoIconsSetup::CInfoIconsSetup()
{
	width = 40;
}

CInfoIconsSetup::~CInfoIconsSetup()
{
}

int CInfoIconsSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	std::string str;
	int res = menu_return::RETURN_REPAINT;


	if(parent != NULL)
		parent->hide();

	if (actionKey.empty())
	{
		res = menu();
	}
	else if(actionKey.substr(0,7)=="del_all")
	{
		for(int i=0 ; i < MODE_ICONS_NR_OF_ENTRIES; i++)
		{
			remove(g_settings.mode_icons_flag[i].c_str());
		}
		ShowHint(LOCALE_INFOICONS_FLAG_DEL_HEAD, g_Locale->getText(LOCALE_INFOICONS_FLAG_DELALL), 450,3);
		res = menu_return::RETURN_EXIT;
	}
	else if(actionKey.substr(0,8)=="del_flag")
	{
		int i = atoi(actionKey.substr(8,1).c_str());
		remove(g_settings.mode_icons_flag[i].c_str());
		ShowHint(LOCALE_INFOICONS_FLAG_DEL_HEAD, g_Locale->getText(LOCALE_INFOICONS_FLAG_DEL), 450,3);
		res = menu_return::RETURN_EXIT;
	}
	else if(actionKey.substr(0,18) == "infoicons_switch") {
		CInfoIcons::getInstance()->switchInfoIconsOnOff();
		res = menu_return::RETURN_EXIT_ALL;
	}

	return res;
}

bool CInfoIconsSetup::changeNotify(const neutrino_locale_t OptionName, void *data)
{
	int val = 0;
	if(data)
		val = (*(int *)data);

	if(ARE_LOCALES_EQUAL(OptionName, LOCALE_INFOICONS_SKIN))
	{
		if(val == INFOICONS_INFOVIEWER)
			oj_modeicon->setActive(false);
		else
			oj_modeicon->setActive(true);
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_INFOICONS_MODEICON))
	{
		if(val == 0) {
			oj_skin->setActive(true);
			printf("CInfoIconsSetup::changeNotify: stop InfoIcons\n");
		}
		else {
			oj_skin->setActive(false);
			printf("CInfoIconsSetup::changeNotify: start InfoIcons\n");
		}
	}

	return true;
}

int CInfoIconsSetup::menu()
{
	int shortcut = 1;
	int shortcut_delflag = 0;
	int shortcut_flagname = 0;

	CMenuWidget* infoiconsMenu = new CMenuWidget(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_INFOICONS);
	infoiconsMenu->addIntroItems(LOCALE_INFOICONS_HEAD);

	//FIXME if Iconbar is active, set selected from LOCALE_INFOICONS_SKIN to LOCALE_INFOICONS_MODEICON
	if(g_settings.mode_icons && (infoiconsMenu->getSelected() == 4))
		infoiconsMenu->setSelected(5);

	oj_skin = new CMenuOptionChooser(LOCALE_INFOICONS_SKIN, &g_settings.mode_icons_skin, OPTIONS_INFOICONS_MODE_OPTIONS, OPTIONS_INFOICONS_MODE_OPTION_COUNT, !g_settings.mode_icons, this, CRCInput::convertDigitToKey(shortcut++));
	oj_skin->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_INFOICONS_SKIN);
	infoiconsMenu->addItem(oj_skin);

	oj_modeicon = new CMenuOptionChooser(LOCALE_INFOICONS_MODEICON, &g_settings.mode_icons, OPTIONS_START0_STOP1_OPTIONS, OPTIONS_START0_STOP1_OPTION_COUNT, g_settings.mode_icons_skin != INFOICONS_INFOVIEWER, this, CRCInput::convertDigitToKey(shortcut++));
	oj_modeicon->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_INFOICONS_MODEICON);
	infoiconsMenu->addItem(oj_modeicon);

	infoiconsMenu->addItem(GenericMenuSeparatorLine);

		CMenuWidget* delFlagMenu = new CMenuWidget(LOCALE_INFOICONS_MODEICON, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_INFOICONS_DELFLAG);
		delFlagMenu->addIntroItems(LOCALE_INFOICONS_FLAG_DEL_HEAD);

		CMenuForwarder * mf = new CMenuForwarder(LOCALE_INFOICONS_FLAG_ALL, true, NULL, this , "del_all", CRCInput::RC_red);
		mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_INFOICONS_FLAG_ALL);
		delFlagMenu->addItem(mf);

		for(int i=0 ; i < MODE_ICONS_NR_OF_ENTRIES; i++)
		{
			mf = new CMenuForwarder(menu_settings[i].caption, file_exists(g_settings.mode_icons_flag[i]), NULL, this , menu_settings[i].action, CRCInput::convertDigitToKey(shortcut_delflag++));
			mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, menu_settings[i].caption_hint);
			delFlagMenu->addItem(mf);
		}

	mf = new CMenuForwarder(LOCALE_INFOICONS_FLAG_DEL_HEAD, true, NULL, delFlagMenu, "", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_INFOICONS_FLAG_DEL_HEAD);
	infoiconsMenu->addItem(mf);

		for(int i=0 ; i < MODE_ICONS_NR_OF_ENTRIES; i++) {
			flagchooser[i] = new CStringInputSMS(menu_settings[i].caption, &g_settings.mode_icons_flag[i], 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE,"abcdefghijklmnopqrstuvwxyz0123456789-_.:/ ");
		}

		CMenuWidget* flagMenu = new CMenuWidget(LOCALE_INFOICONS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_INFOICONS_SETFLAG);
		flagMenu->addIntroItems(LOCALE_INFOICONS_FLAG_HEAD);

		for(int i=0 ; i < MODE_ICONS_NR_OF_ENTRIES; i++)
		{
			mforwarder[i] = new CMenuForwarder(menu_settings[i].caption , true , g_settings.mode_icons_flag[i], flagchooser[i], "", CRCInput::convertDigitToKey(shortcut_flagname++));
			mforwarder[i]->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, menu_settings[i].caption_hint);
			flagMenu->addItem(mforwarder[i]);
		}

	mf = new CMenuForwarder(LOCALE_INFOICONS_FLAG_HEAD, true, NULL, flagMenu, "", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_INFOICONS_FLAG_HEAD);
	infoiconsMenu->addItem(mf);

	infoiconsMenu->addItem(GenericMenuSeparatorLine);

	CMenuOptionChooser *mc = new CMenuOptionChooser(LOCALE_INFOICONS_BACKGROUND, &g_settings.mode_icons_background, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this, CRCInput::convertDigitToKey(shortcut++));
	mc->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_INFOICONS_BACKGROUND);
	infoiconsMenu->addItem(mc);

	int res = infoiconsMenu->exec(NULL,"");
	infoiconsMenu->hide();
	delete infoiconsMenu;
	delete delFlagMenu;
	for (int i = 0 ; i < MODE_ICONS_NR_OF_ENTRIES; i++)
		delete flagchooser[i];
	delete flagMenu;
	return res;
}
