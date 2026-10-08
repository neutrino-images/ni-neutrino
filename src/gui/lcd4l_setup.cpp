/*
	lcd4l setup

	Copyright (C) 2012 'defans'
	Homepage: http://www.bluepeercrew.us/

	Copyright (C) 2012-2021 'vanhofen'
	Homepage: http://www.neutrino-images.de/

	Copyright (C) 2016-2018 'TangoCash'
		  (C) 2021, Thilo Graf 'dbt'

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
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <iostream>
#include <fstream>
#include <sstream>
#include <signal.h>
#include <unistd.h>

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <gui/filebrowser.h>
#include <gui/widget/icons.h>
#include <gui/widget/menue.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/settingitem.h>

#include <coreapi/base/apply.h>
#include <coreapi/box/apply_lcd4l.h>
#include <coreapi/box/applyworker.h>
#include <coreapi/settings/settings.h>

#include <gui/lcd4l_setup.h>

#include <system/debug.h>
#include <system/helpers.h>

#include <driver/screen_max.h>

#include "driver/lcd4l.h"

const CMenuOptionChooser::keyval LCD4L_DPF_SKIN_OPTIONS[] =
{
	{ 0, LOCALE_LCD4L_SKIN_0 },
	{ 1, LOCALE_LCD4L_SKIN_1 },
	{ 2, LOCALE_LCD4L_SKIN_2 },
	{ 3, LOCALE_LCD4L_SKIN_3 },
	{ 4, LOCALE_LCD4L_SKIN_4 },
	{ 100, LOCALE_LCD4L_SKIN_100 }
};
#define LCD4L_DPF_SKIN_OPTION_COUNT (sizeof(LCD4L_DPF_SKIN_OPTIONS)/sizeof(CMenuOptionChooser::keyval))

const CMenuOptionChooser::keyval LCD4L_SPF_SKIN_OPTIONS[] =
{
	{ 0, LOCALE_LCD4L_SKIN_0 },
	{ 4, LOCALE_LCD4L_SKIN_4 },
	{ 100, LOCALE_LCD4L_SKIN_100 }
};
#define LCD4L_SPF_SKIN_OPTION_COUNT (sizeof(LCD4L_SPF_SKIN_OPTIONS)/sizeof(CMenuOptionChooser::keyval))

using namespace sigc;

/* These three run on the apply worker, so the restart leaves the hints alone: they
   are painted, and only the program's loop paints. The menu shows its own. */
bool coreapi::applicationRestartLcd4l(int mode)
{
	return CLCD4l::getInstance()->Restart(mode);
}

void coreapi::applicationReinitLcd4l()
{
	CLCD4l::getInstance()->InitLCD4l();
}

void coreapi::applicationForceRunLcd4l()
{
	CLCD4l::getInstance()->ForceRun();
}

CLCD4lSetup::CLCD4lSetup()
{
	width = 40;
	hint = NULL;

	sl_start = bind(mem_fun(*this, &CLCD4lSetup::showHint), "Starting lcd service...");
	sl_stop = bind(mem_fun(*this, &CLCD4lSetup::showHint), "Stopping lcd service...");
	sl_restart = bind(mem_fun(*this, &CLCD4lSetup::showHint), "Restarting lcd service...");
	sl_remove = mem_fun(*this, &CLCD4lSetup::removeHint);
	connectSlots();
}

CLCD4lSetup::~CLCD4lSetup()
{
	removeHint();
}

CLCD4lSetup* CLCD4lSetup::getInstance()
{
	static CLCD4lSetup* me = NULL;

	if(!me)
		me = new CLCD4lSetup();

	return me;
}

int CLCD4lSetup::exec(CMenuTarget *parent, const std::string &actionkey)
{
	printf("CLCD4lSetup::exec: actionkey %s\n", actionkey.c_str());
	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	if (actionkey == "typeSetup")
	{
		return showTypeSetup();
	}

	res = show();

	return res;
}

bool CLCD4lSetup::changeNotify(const neutrino_locale_t OptionName, void * /*data*/)
{
	/* The items built by hand edit the setting itself, so the group is all that is left to tell. */
	const char *key = NULL;
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCD4L_SKIN))
		key = "lcd4l_skin";

	if (key != NULL)
	{
		const coreapi::Status st = coreapi::applyKey(key);
		if (st != coreapi::Status::Ok && st != coreapi::Status::Busy)
			dprintf(DEBUG_NORMAL, "[lcd4l] %s was not applied\n", key);
	}
	return false;
}

int CLCD4lSetup::show()
{
	int shortcut = 1;

	CMenuItem *item;
	CMenuForwarder *mf;

	// lcd4l setup
	CMenuWidget *lcd4lSetup = new CMenuWidget(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_LCD4L_SETUP);
	lcd4lSetup->addIntroItems(LOCALE_LCD4L_SUPPORT);

	item = addSetting(lcd4lSetup, "lcd4l_support", true, NULL, CRCInput::RC_red);
	if (item)
	{
		item->hintIcon = NEUTRINO_ICON_HINT_LCD4LINUX;
		// The restart runs on the apply worker, and the menu says so while it waits.
		afterApply(item, [this]()
		{
			// Only the restart: the force that follows every run is a flag and over at once.
			if (!coreapi::applyWorker().pending("lcd4l.mode"))
				return false;
			showHint(g_settings.lcd4l_support ? "Starting lcd service..." : "Stopping lcd service...");
			coreapi::applyWorker().waitFor("lcd4l.mode");
			removeHint();
			return false;
		});
	}

	lcd4lSetup->addItem(GenericMenuSeparatorLine);

	item = addSetting(lcd4lSetup, "lcd4l_display_type", true, NULL, CRCInput::RC_green);
	if (item)
		item->hintIcon = NEUTRINO_ICON_HINT_LCD4LINUX;

	mf = new CMenuForwarder(LOCALE_LCD4L_DISPLAY_TYPE_SETUP, true, NULL, this, "typeSetup", CRCInput::RC_yellow);
	mf->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_DISPLAY_TYPE_SETUP);
	lcd4lSetup->addItem(mf);

	lcd4lSetup->addItem(GenericMenuSeparatorLine);

	item = addSetting(lcd4lSetup, "lcd4l_logodir", true, NULL, CRCInput::convertDigitToKey(shortcut++));
	if (item)
		item->hintIcon = NEUTRINO_ICON_HINT_LCD4LINUX;

	lcd4lSetup->addItem(GenericMenuSeparator);

	item = addSetting(lcd4lSetup, "flag_lcd4l_weather", true, NULL, CRCInput::convertDigitToKey(shortcut++));
	if (item)
		item->hintIcon = NEUTRINO_ICON_HINT_LCD4LINUX;

	item = addSetting(lcd4lSetup, "flag_lcd4l_clock_a", true, NULL, CRCInput::convertDigitToKey(shortcut++));
	if (item)
		item->hintIcon = NEUTRINO_ICON_HINT_LCD4LINUX;

	lcd4lSetup->addItem(GenericMenuSeparator);

	item = addSetting(lcd4lSetup, "lcd4l_convert", true, NULL, CRCInput::convertDigitToKey(shortcut++));
	if (item)
		item->hintIcon = NEUTRINO_ICON_HINT_LCD4LINUX;

	// The web interface reads this one out of the saved file, so a change is saved at once.
	CMenuOptionChooser *screenshots = addChoiceSetting(lcd4lSetup, "lcd4l_screenshots", true, NULL, CRCInput::convertDigitToKey(shortcut++));
	if (screenshots)
	{
		screenshots->hintIcon = NEUTRINO_ICON_HINT_LCD4LINUX;
		afterApply(screenshots, []() { CNeutrinoApp::getInstance()->saveSetup(NEUTRINO_SETTINGS_FILE); return false; });
	}

	int res = lcd4lSetup->exec(NULL, "");

	lcd4lSetup->hide();
	delete lcd4lSetup;

	return res;
}

int CLCD4lSetup::showTypeSetup()
{
	int shortcut = 1;

	CMenuOptionChooser *mc;

	CMenuWidget *typeSetup = new CMenuWidget(LOCALE_LCD4L_DISPLAY_TYPE_SETUP, NEUTRINO_ICON_SETTINGS, width);
	typeSetup->addIntroItems(); //FIXME: show lcd4l display type

	// Two lists of skins, by the panel: a list that depends on another setting is not one a row states yet.
	if (g_settings.lcd4l_display_type == CLCD4l::DPF320x240)
		mc = new CMenuOptionChooser(LOCALE_LCD4L_SKIN, &g_settings.lcd4l_skin, LCD4L_DPF_SKIN_OPTIONS, LCD4L_DPF_SKIN_OPTION_COUNT, true, this, CRCInput::convertDigitToKey(shortcut++));
	else
		mc = new CMenuOptionChooser(LOCALE_LCD4L_SKIN, &g_settings.lcd4l_skin, LCD4L_SPF_SKIN_OPTIONS, LCD4L_SPF_SKIN_OPTION_COUNT, true, this, CRCInput::convertDigitToKey(shortcut++));
	mc->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_SKIN);
	typeSetup->addItem(mc);

	CMenuItem *skin_radio = addSetting(typeSetup, "lcd4l_skin_radio", true, NULL, CRCInput::convertDigitToKey(shortcut++));
	if (skin_radio)
		skin_radio->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_SKIN_RADIO);

	// The rows state the panel's ceiling and when the standby value counts.
	CMenuItem *brightness = addNumberSetting(typeSetup, "lcd4l_brightness");
	if (brightness)
		brightness->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_BRIGHTNESS);

	CMenuItem *standby = addNumberSetting(typeSetup, "lcd4l_brightness_standby");
	if (standby)
		standby->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_BRIGHTNESS_STANDBY);

	return typeSetup->exec(NULL, "");
}

void CLCD4lSetup::showHint(const std::string &text)
{
	removeHint();
	hint = new CHint(text.c_str());
	hint->paint();

}

void CLCD4lSetup::removeHint()
{
	if (hint)
	{
		hint->hide();
		delete hint;
		hint = NULL;
	}
}

void CLCD4lSetup::connectSlots()
{
	CLCD4l::getInstance()->OnBeforeStart.connect(sl_start);
	CLCD4l::getInstance()->OnBeforeStop.connect(sl_stop);
	CLCD4l::getInstance()->OnBeforeRestart.connect(sl_restart);

	CLCD4l::getInstance()->OnAfterStart.connect(sl_remove);
	CLCD4l::getInstance()->OnAfterStop.connect(sl_remove);
	CLCD4l::getInstance()->OnAfterRestart.connect(sl_remove);

	CLCD4l::getInstance()->OnError.connect(sl_remove);
}
