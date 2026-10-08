/*
	$similar port: lcd_setup.cpp,v 1.1 2010/07/30 20:52:16 tuxbox-cvs Exp $

	vfd setup implementation, similar to lcd_setup.cpp of tuxbox-cvs - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2010 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/


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

#include "vfd_setup.h"

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/settingitem.h>

#include <coreapi/base/apply.h>
#include <coreapi/box/apply_vfd.h>
#include <coreapi/settings/predicates.h>
#include <coreapi/settings/settings.h>

#ifdef ENABLE_GRAPHLCD
#include <gui/glcdsetup.h>
#endif

#ifdef ENABLE_LCD4LINUX
#include "gui/lcd4l_setup.h"
#endif

#include <driver/display.h>
#include <driver/screen_max.h>

#include <system/debug.h>
#include <system/helpers.h>

#include <string>
#include <vector>

namespace
{

/* Shows the dim brightness on the display without making it the brightness: the
   driver's own dimming does the same and puts the setting back. */
void previewDimBrightness()
{
	const int kept = g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS];
	CVFD::getInstance()->setBrightness(g_settings.lcd_setting_dim_brightness);
	g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = kept;
}

} // namespace

coreapi::Status coreapi::applicationVfdBrightness(int which, int value)
{
	switch (which)
	{
		case VfdPanel::Normal:
			CVFD::getInstance()->setBrightness(value);
			break;
		case VfdPanel::Standby:
			CVFD::getInstance()->setBrightnessStandby(value);
			break;
		case VfdPanel::DeepStandby:
			CVFD::getInstance()->setBrightnessDeepStandby(value);
			break;
		default:
			return Status::InvalidArgument;
	}
	return Status::Ok;
}

coreapi::Status coreapi::applicationVfdScroll(int repeats)
{
	CVFD::getInstance()->setScrollMode(repeats);
	return Status::Ok;
}

coreapi::Status coreapi::applicationVfdLeds()
{
	CVFD::getInstance()->setled();
	return Status::Ok;
}

coreapi::Status coreapi::applicationVfdParameters()
{
	CVFD::getInstance()->setlcdparameter();
	return Status::Ok;
}

coreapi::Status coreapi::applicationVfdBacklight(int on)
{
#ifndef ENABLE_LCD
	CVFD::getInstance()->setBacklight(on != 0);
#else
	(void) on;
#endif
	return Status::Ok;
}

coreapi::Status coreapi::applicationVfdStatusline(int mode, int volume)
{
	// Only a panel with a second line has the icons and the volume it shows.
	if (!CVFD::getInstance()->has_lcd || !g_info.hw_caps->display_has_statusline)
		return Status::Ok;

	if (mode == 2 /* off */)
	{
		// to lazy for a loop. the effect is the same.
		CVFD::getInstance()->ShowIcon(FP_ICON_BAR8, false);
		CVFD::getInstance()->ShowIcon(FP_ICON_BAR7, false);
		CVFD::getInstance()->ShowIcon(FP_ICON_BAR6, false);
		CVFD::getInstance()->ShowIcon(FP_ICON_BAR5, false);
		CVFD::getInstance()->ShowIcon(FP_ICON_BAR4, false);
		CVFD::getInstance()->ShowIcon(FP_ICON_BAR3, false);
		CVFD::getInstance()->ShowIcon(FP_ICON_BAR2, false);
		CVFD::getInstance()->ShowIcon(FP_ICON_BAR1, false);
		CVFD::getInstance()->ShowIcon(FP_ICON_FRAME, false);
	}
	else
	{
		CVFD::getInstance()->ShowIcon(FP_ICON_FRAME, true);
		CVFD::getInstance()->showVolume(volume);
		//CVFD::getInstance()->showPercentOver(???);
	}
	return Status::Ok;
}

CVfdSetup::CVfdSetup()
{
	width = 40;
}

CVfdSetup::~CVfdSetup()
{
}

int CVfdSetup::exec(CMenuTarget *parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init lcd setup\n");
	if (parent != NULL)
		parent->hide();

	if (actionKey == "def")
	{
		/* Every brightness is a row, so its default is the row's and not a number kept here. The
		   dim brightness default is nought on every box. */
		std::vector<std::string> reset;
		reset.push_back("lcd_brightness");
		reset.push_back("lcd_standbybrightness");
		reset.push_back("lcd_deepbrightness");
		reset.push_back("lcd_dim_brightness");
		coreapi::settings::Refusals refused;
		coreapi::settings::resetDefaults(reset, refused, true);
		for (size_t i = 0; i < refused.size(); i++)
			dprintf(DEBUG_NORMAL, "[vfd] %s not reset: %s\n", refused[i].first.c_str(), refused[i].second.message.c_str());
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "brightness")
	{
		return showBrightnessSetup();
	}

	int res = showSetup();

	return res;
}

int CVfdSetup::showSetup()
{
	CMenuWidget *vfds = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_LCD, width, MN_WIDGET_ID_VFDSETUP);
	vfds->addIntroItems(LOCALE_LCDMENU_HEAD);

	int initial_count = vfds->getItemsCount();

	CMenuForwarder *mf;

	// led menu
	if (coreapi::hasLedMenu())
	{
		CMenuWidget *ledMenu = new CMenuWidget(LOCALE_LCDMENU_HEAD, NEUTRINO_ICON_LCD, width, MN_WIDGET_ID_VFDSETUP_LED_SETUP);
		showLedSetup(ledMenu);
		mf = new CMenuDForwarder(LOCALE_LEDCONTROLER_MENU, true, NULL, ledMenu, NULL, CRCInput::RC_red);
		mf->setHint("", LOCALE_MENU_HINT_POWER_LEDS);
		vfds->addItem(mf);
	}

	if (coreapi::canSetBrightness())
	{
		// vfd brightness menu
		mf = new CMenuForwarder(LOCALE_LCDMENU_LCDCONTROLER, coreapi::vfdEnabled(), NULL, this, "brightness", CRCInput::RC_green);
		mf->setHint("", LOCALE_MENU_HINT_VFD_BRIGHTNESS_SETUP);
		vfds->addItem(mf);
	}

	if (CVFD::getInstance()->has_lcd)
	{
		if (coreapi::hasBacklight())
		{
			// backlight menu
			CMenuWidget *blMenu = new CMenuWidget(LOCALE_LCDMENU_HEAD, NEUTRINO_ICON_LCD, width, MN_WIDGET_ID_VFDSETUP_BACKLIGHT);
			showBacklightSetup(blMenu);
			mf = new CMenuDForwarder(LOCALE_LEDCONTROLER_BACKLIGHT, true, NULL, blMenu, NULL, CRCInput::RC_yellow);
			mf->setHint("", LOCALE_MENU_HINT_BACKLIGHT);
			vfds->addItem(mf);

			vfds->addItem(GenericMenuSeparatorLine);
		}

#ifdef ENABLE_LCD
		CMenuOptionChooser *oj;
#if 0
		// option power
		oj = new CMenuOptionChooser("Power LCD"/*LOCALE_LCDMENU_POWER*/, &g_settings.lcd_setting[SNeutrinoSettings::LCD_POWER], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, new CLCDNotifier("lcd_power"), CRCInput::RC_nokey);
		vfds->addItem(oj);
#endif
		// option invert
		oj = new CMenuOptionChooser("Invert LCD"/*LOCALE_LCDMENU_INVERSE*/, &g_settings.lcd_setting[SNeutrinoSettings::LCD_INVERSE], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, new CLCDNotifier("lcd_inverse"), CRCInput::RC_nokey);
		vfds->addItem(oj);
#endif
		if (g_info.hw_caps->display_has_statusline)
		{
			// status line options
			addSetting(vfds, "lcd_show_volume", coreapi::vfdEnabled);
		}

#ifndef ENABLE_LCD
		// info line options
		addSetting(vfds, "lcd_info_line");

		// scroll options: a count, or an on and an off where the panel takes none
		addSetting(vfds, "lcd_scroll");

		// notify rc-lock
		addSetting(vfds, "lcd_notify_rclock");
#endif // ENABLE_LCD
	}

	if (coreapi::hasNumericPanel())
	{
		// LED NUM info line options
		addSetting(vfds, "lcd_info_line");
	}

	CMenuItem *glcd_setup = NULL;
#ifdef ENABLE_GRAPHLCD
	int glcdKey = CRCInput::RC_nokey;
	if (vfds->getItemsCount() == initial_count) // first item
		glcdKey = CRCInput::RC_red;

	GLCD_Menu glcdMenu;
	glcd_setup = new CMenuForwarder(LOCALE_GLCD_HEAD, true, NULL, &glcdMenu, NULL, glcdKey);
	glcd_setup->setHint(NEUTRINO_ICON_HINT_GRAPHLCD, LOCALE_MENU_HINT_GLCD_SUPPORT);
	vfds->addItem(glcd_setup);
#endif

#ifdef ENABLE_LCD4LINUX
	mf = new CMenuForwarder(LOCALE_LCD4L_SUPPORT, !find_executable("lcd4linux").empty(), NULL, CLCD4lSetup::getInstance(), NULL, CRCInput::RC_blue);
	mf->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_SUPPORT);
	vfds->addItem(mf);
#endif

	int res;
	if (glcd_setup && (vfds->getItemsCount() == initial_count + 1))
	{
		// glcd-setup is the only item; execute directly
		res = glcd_setup->exec(NULL);
	}
	else
		res = vfds->exec(NULL, "");

	delete vfds;
	return res;
}

int CVfdSetup::showBrightnessSetup()
{
	CMenuOptionNumberChooser *nc;

	CMenuWidget *mn_widget = new CMenuWidget(LOCALE_LCDMENU_HEAD, NEUTRINO_ICON_LCD, width, MN_WIDGET_ID_VFDSETUP_LCD_SLIDERS);

	mn_widget->addIntroItems(LOCALE_LCDMENU_LCDCONTROLER);

	nc = addNumberSetting(mn_widget, "lcd_brightness", true, NULL, CRCInput::RC_nokey, true);
	if (nc)
		nc->setActivateObserver(this);

	nc = addNumberSetting(mn_widget, "lcd_standbybrightness", true, NULL, CRCInput::RC_nokey, true);
	if (nc)
		nc->setActivateObserver(this);

	if (g_info.hw_caps->display_can_deepstandby)
	{
		nc = addNumberSetting(mn_widget, "lcd_deepbrightness", true, NULL, CRCInput::RC_nokey, true);
		if (nc)
			nc->setActivateObserver(this);
	}

	nc = addNumberSetting(mn_widget, "lcd_dim_brightness", true, NULL, CRCInput::RC_nokey, true);
	if (nc)
	{
		nc->setActivateObserver(this);
		afterApply(nc, []() { previewDimBrightness(); return false; });
	}

	mn_widget->addItem(GenericMenuSeparatorLine);
	CMenuItem *dim_time = addSetting(mn_widget, "lcd_dim_time");
	if (dim_time)
		dim_time->setActivateObserver(this);

	mn_widget->addItem(GenericMenuSeparatorLine);
	CMenuForwarder *mf = new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, "def", CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_VFD_DEFAULTS);
	mf->setActivateObserver(this);
	mn_widget->addItem(mf);

	int res = mn_widget->exec(this, "");
	delete mn_widget;

	return res;
}

void CVfdSetup::showLedSetup(CMenuWidget *mn_led_widget)
{
	mn_led_widget->addIntroItems(LOCALE_LEDCONTROLER_MENU);

	addSetting(mn_led_widget, "led_tv_mode");
	addSetting(mn_led_widget, "led_standby_mode");
	addSetting(mn_led_widget, "led_deep_mode");
	addSetting(mn_led_widget, "led_rec_mode");
	addSetting(mn_led_widget, "led_blink");
}

void CVfdSetup::showBacklightSetup(CMenuWidget *mn_led_widget)
{
	mn_led_widget->addIntroItems(LOCALE_LEDCONTROLER_BACKLIGHT);

	addSetting(mn_led_widget, "backlight_tv");
	addSetting(mn_led_widget, "backlight_standby");
	addSetting(mn_led_widget, "backlight_deepstandby");
}

void CVfdSetup::activateNotify(const neutrino_locale_t OptionName)
{
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDCONTROLER_BRIGHTNESSSTANDBY))
	{
		CVFD::getInstance()->setMode(CVFD::MODE_STANDBY);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDCONTROLER_BRIGHTNESS))
	{
		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDMENU_DIM_BRIGHTNESS))
	{
		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
		previewDimBrightness();
	}
	else
	{
		CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8);
	}
}

#ifdef ENABLE_LCD
// lcd notifier
bool CLCDNotifier::changeNotify(const neutrino_locale_t, void * Data)
{
	int state = *(int *)Data;

	dprintf(DEBUG_NORMAL, "CLCDNotifier: state: %d\n", state);
#if 0
	CVFD::getInstance()->setPower(state);
#else
	CVFD::getInstance()->setPower(1);
#endif
	// Through the group, which sends the panel's parameters and keeps what it sent.
	const coreapi::Status st = coreapi::settings::menuChanged(key);
	if (st != coreapi::Status::Ok && st != coreapi::Status::Busy)
		dprintf(DEBUG_NORMAL, "CLCDNotifier: %s was not applied\n", key);

	return true;
}
#endif
