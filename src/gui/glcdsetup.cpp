/*
	Neutrino graphlcd menue

	(c) 2012 by martii


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

#include "filebrowser.h"
#include <stdio.h>
#include <global.h>
#include <neutrino.h>
#include <zapit/channel.h>
#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <daemonc/remotecontrol.h>
#include <driver/glcd/glcd.h>
#include <driver/screen_max.h>
#include <system/debug.h>
#include <system/helpers.h>
#include "glcdsetup.h"
#include <gui/widget/menue_options.h>
#include <gui/widget/colorchooser.h>
#include <gui/widget/settingitem.h>
#include <neutrino_menue.h>
#include "glcdthemes.h"

#include <coreapi/base/apply.h>
#include <coreapi/box/apply_glcd.h>
#include <coreapi/settings/settings.h>

#include <string>
#include <vector>

static const CMenuOptionChooser::keyval STANDBY_CLOCK_OPTIONS[] =
{
	{ cGLCD::CLOCK_OFF,	LOCALE_OPTIONS_OFF },
	{ cGLCD::CLOCK_SIMPLE,	LOCALE_GLCD_STANDBY_CLOCK_SIMPLE },
	{ cGLCD::CLOCK_LED,	LOCALE_GLCD_STANDBY_CLOCK_LED },
	{ cGLCD::CLOCK_LCD,	LOCALE_GLCD_STANDBY_CLOCK_LCD },
	{ cGLCD::CLOCK_DIGITAL,	LOCALE_GLCD_STANDBY_CLOCK_DIGITAL },
	{ cGLCD::CLOCK_ANALOG,	LOCALE_GLCD_STANDBY_CLOCK_ANALOG }
};
#define STANDBY_CLOCK_OPTION_COUNT (sizeof(STANDBY_CLOCK_OPTIONS)/sizeof(CMenuOptionChooser::keyval))

static const CMenuOptionChooser::keyval ALIGNMENT_OPTIONS[] =
{
	{ cGLCD::ALIGN_NONE,	LOCALE_GLCD_ALIGN_NONE },
	{ cGLCD::ALIGN_LEFT,	LOCALE_GLCD_ALIGN_LEFT },
	{ cGLCD::ALIGN_CENTER,	LOCALE_GLCD_ALIGN_CENTER },
	{ cGLCD::ALIGN_RIGHT,	LOCALE_GLCD_ALIGN_RIGHT }
};
#define ALIGNMENT_OPTION_COUNT (sizeof(ALIGNMENT_OPTIONS)/sizeof(CMenuOptionChooser::keyval))

#if 0
#define KEY_GLCD_BLACK			0
#define KEY_GLCD_WHITE			1
#define KEY_GLCD_RED			2
#define KEY_GLCD_PINK			3
#define KEY_GLCD_PURPLE			4
#define KEY_GLCD_DEEPPURPLE		5
#define KEY_GLCD_INDIGO			6
#define KEY_GLCD_BLUE			7
#define KEY_GLCD_LIGHTBLUE		8
#define KEY_GLCD_CYAN			9
#define KEY_GLCD_TEAL			10
#define KEY_GLCD_GREEN			11
#define KEY_GLCD_LIGHTGREEN		12
#define KEY_GLCD_LIME			13
#define KEY_GLCD_YELLOW			14
#define KEY_GLCD_AMBER			15
#define KEY_GLCD_ORANGE			16
#define KEY_GLCD_DEEPORANGE		17
#define KEY_GLCD_BROWN			18
#define KEY_GLCD_GRAY			19
#define KEY_GLCD_BLUEGRAY		20

#define GLCD_COLOR_OPTION_COUNT		21

static const CMenuOptionChooser::keyval GLCD_COLOR_OPTIONS[GLCD_COLOR_OPTION_COUNT] =
{
	{ KEY_GLCD_BLACK,	LOCALE_GLCD_COLOR_BLACK },
	{ KEY_GLCD_WHITE,	LOCALE_GLCD_COLOR_WHITE },
	{ KEY_GLCD_RED,		LOCALE_GLCD_COLOR_RED },
	{ KEY_GLCD_PINK,	LOCALE_GLCD_COLOR_PINK },
	{ KEY_GLCD_PURPLE,	LOCALE_GLCD_COLOR_PURPLE },
	{ KEY_GLCD_DEEPPURPLE,	LOCALE_GLCD_COLOR_DEEPPURPLE },
	{ KEY_GLCD_INDIGO,	LOCALE_GLCD_COLOR_INDIGO },
	{ KEY_GLCD_BLUE,	LOCALE_GLCD_COLOR_BLUE },
	{ KEY_GLCD_LIGHTBLUE,	LOCALE_GLCD_COLOR_LIGHTBLUE },
	{ KEY_GLCD_CYAN,	LOCALE_GLCD_COLOR_CYAN },
	{ KEY_GLCD_TEAL,	LOCALE_GLCD_COLOR_TEAL },
	{ KEY_GLCD_GREEN,	LOCALE_GLCD_COLOR_GREEN },
	{ KEY_GLCD_LIGHTGREEN,	LOCALE_GLCD_COLOR_LIGHTGREEN },
	{ KEY_GLCD_LIME,	LOCALE_GLCD_COLOR_LIME },
	{ KEY_GLCD_YELLOW,	LOCALE_GLCD_COLOR_YELLOW },
	{ KEY_GLCD_AMBER,	LOCALE_GLCD_COLOR_AMBER },
	{ KEY_GLCD_ORANGE,	LOCALE_GLCD_COLOR_ORANGE },
	{ KEY_GLCD_DEEPORANGE,	LOCALE_GLCD_COLOR_DEEPORANGE },
	{ KEY_GLCD_BROWN,	LOCALE_GLCD_COLOR_BROWN },
	{ KEY_GLCD_GRAY,	LOCALE_GLCD_COLOR_GRAY },
	{ KEY_GLCD_BLUEGRAY,	LOCALE_GLCD_COLOR_BLUEGRAY },
};

static const uint32_t colormap[GLCD_COLOR_OPTION_COUNT] =
{
	GLCD::cColor::Black,
	GLCD::cColor::White,
	GLCD::cColor::Red,
	GLCD::cColor::Pink,
	GLCD::cColor::Purple,
	GLCD::cColor::DeepPurple,
	GLCD::cColor::Indigo,
	GLCD::cColor::Blue,
	GLCD::cColor::LightBlue,
	GLCD::cColor::Cyan,
	GLCD::cColor::Teal,
	GLCD::cColor::Green,
	GLCD::cColor::LightGreen,
	GLCD::cColor::Lime,
	GLCD::cColor::Yellow,
	GLCD::cColor::Amber,
	GLCD::cColor::Orange,
	GLCD::cColor::DeepOrange,
	GLCD::cColor::Brown,
	GLCD::cColor::Gray,
	GLCD::cColor::BlueGray
};

int GLCD_Menu::color2index(uint32_t color)
{
	for (int i = 0; i < GLCD_COLOR_OPTION_COUNT; i++)
		if (colormap[i] == color)
			return i;
	return KEY_GLCD_BLACK;
}

uint32_t GLCD_Menu::index2color(int i)
{
	return (i < GLCD_COLOR_OPTION_COUNT) ? colormap[i] : GLCD::cColor::ERRCOL;
}
#endif

namespace
{
void noteSize();
}

coreapi::Status coreapi::applicationGlcd(int what, int value)
{
	switch (what)
	{
		case GlcdEnable:
			if (value)
				cGLCD::Resume();
			else
				cGLCD::Suspend();
			break;
		case GlcdMirrorOsd:
			cGLCD::MirrorOSD(value != 0);
			break;
		case GlcdRespawn:
			cGLCD::Respawn();
			break;
		case GlcdReinitFont:
			cGLCD::getInstance()->ReInitFont();
			break;
		case GlcdBrightness:
			cGLCD::getInstance()->UpdateBrightness();
			break;
		case GlcdUpdate:
			cGLCD::Update();
			break;
		case GlcdRecordSize:
			noteSize();
			break;
		default:
			return Status::InvalidArgument;
	}
	return Status::Ok;
}

namespace
{

/* The loop records the panel's size where it sees the service, since the checks of a
   written position run on another thread and must not reach it. */
void noteSize()
{
	cGLCD *cglcd = cGLCD::getInstance();
	if (cglcd != NULL && cglcd->lcd != NULL)
		coreapi::noteGlcdPanelSize(cglcd->lcd->Width(), cglcd->lcd->Height());
}

void applyGlcd(const char *key)
{
	const coreapi::Status st = coreapi::applyKey(key);
	if (st != coreapi::Status::Ok && st != coreapi::Status::Busy)
		dprintf(DEBUG_NORMAL, "[glcd] %s was not applied\n", key);
}

/* While the position of the channel name is edited, the display shows the name of the
   channel that is on. */
bool previewChannelName()
{
	cGLCD *cglcd = cGLCD::getInstance();
	cglcd->unlockChannel();
	cglcd->lockChannel(CNeutrinoApp::getInstance()->channelList->getActiveChannelName());
	return false;
}

} // namespace

GLCD_Menu::GLCD_Menu()
{
	width = 40;

	select_driver = NULL;
}

int GLCD_Menu::exec(CMenuTarget *parent, const std::string &actionKey)
{
	int res = menu_return::RETURN_REPAINT;
	cGLCD *cglcd = cGLCD::getInstance();
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;

	if (parent)
		parent->hide();

	noteSize();

	if (actionKey == "rescan")
	{
		cglcd->Rescan();
		return res;
	}
	else if (actionKey == "select_font")
	{
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("ttf");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(FONTDIR) == true)
		{
			setSettingsText(t.glcd_font, fileBrowser.getSelectedFile()->Name);
			applyGlcd("glcd_font");
		}
		return res;
	}
	else if (actionKey == "select_background")
	{
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("jpg");
		fileFilter.addFilter("jpeg");
		fileFilter.addFilter("png");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(THEMESDIR"/oled") == true)
			setSettingsText(t.glcd_background_image, fileBrowser.getSelectedFile()->Name);
		else
			setSettingsText(t.glcd_background_image, "");
		applyGlcd("glcd_background_image");
		return res;
	}
	else if (actionKey == "brightness_default")
	{
		std::vector<std::string> reset;
		reset.push_back("glcd_brightness");
		reset.push_back("glcd_brightness_standby");
		reset.push_back("glcd_brightness_dim");
		reset.push_back("glcd_brightness_dim_time");
		coreapi::settings::Refusals refused;
		coreapi::settings::resetDefaults(reset, refused, true);
		for (size_t i = 0; i < refused.size(); i++)
			dprintf(DEBUG_NORMAL, "[glcd] %s not reset: %s\n", refused[i].first.c_str(), refused[i].second.message.c_str());
		return res;
	}
	else if (actionKey == "select_driver")
	{
		return GLCD_Menu_Select_Driver();
	}
	else if (actionKey == "theme_settings")
	{
		return GLCD_Theme_Settings();
	}
	else if (actionKey == "brightness_settings")
	{
		return GLCD_Brightness_Settings();
	}
	else if (actionKey == "standby_settings")
	{
		return GLCD_Standby_Settings();
	}
	else
	{
		return GLCD_Menu_Settings();
	}

	return res;
}

void GLCD_Menu::hide()
{
}

int GLCD_Menu::GLCD_Menu_Settings()
{
	int shortcut = 1;

	CMenuWidget *gms = new CMenuWidget(LOCALE_MAINSETTINGS_LCD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_GLCD_SETTINGS);
	gms->addIntroItems(LOCALE_GLCD_HEAD);

	addSetting(gms, "glcd_enable", true, NULL, CRCInput::RC_red);

	select_driver = new CMenuForwarder(LOCALE_GLCD_DISPLAY, (cGLCD::getInstance()->GetConfigSize() > 1), cGLCD::getInstance()->GetConfigName(g_settings.glcd_selected_config).c_str(), this, "select_driver", CRCInput::RC_green);
	gms->addItem(select_driver);

	gms->addItem(new CMenuForwarder(LOCALE_GLCD_THEME_SETTINGS, true, NULL, this, "theme_settings", CRCInput::RC_yellow));

	gms->addItem(GenericMenuSeparatorLine);

	addSetting(gms, "glcd_logodir", true, NULL, CRCInput::convertDigitToKey(shortcut++));

	gms->addItem(GenericMenuSeparator);

	gms->addItem(new CMenuForwarder(LOCALE_GLCD_BRIGHTNESS_SETTINGS, true, NULL, this, "brightness_settings", CRCInput::convertDigitToKey(shortcut++)));

	addSetting(gms, "glcd_scroll", true, NULL, CRCInput::convertDigitToKey(shortcut++));

	addSetting(gms, "glcd_scroll_speed");

	addSetting(gms, "glcd_mirror_osd", true, NULL, CRCInput::convertDigitToKey(shortcut++));

	addSetting(gms, "glcd_mirror_video", true, NULL, CRCInput::convertDigitToKey(shortcut++));

	gms->addItem(GenericMenuSeparatorLine);

	gms->addItem(new CMenuForwarder(LOCALE_GLCD_RESTART, true, NULL, this, "rescan", CRCInput::RC_blue));

	int res = gms->exec(NULL, "");
	delete gms;
	select_driver = NULL;
	cGLCD::getInstance()->StandbyMode(false);
	return res;
}

int GLCD_Menu::GLCD_Standby_Settings()
{
	cGLCD::getInstance()->StandbyMode(true);

	CMenuWidget *gss = new CMenuWidget(LOCALE_GLCD_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_GLCD_STANDBY_SETTINGS);
	gss->addIntroItems(LOCALE_GLCD_STANDBY_SETTINGS);

	addSetting(gss, "glcd_standby_clock");
	addSetting(gss, "glcd_standby_clock_digital_y_position");
	addSetting(gss, "glcd_standby_clock_simple_size");
	addSetting(gss, "glcd_standby_clock_simple_y_position");

	gss->addItem(GenericMenuSeparatorLine);

	addSetting(gss, "glcd_standby_weather");
	addSetting(gss, "glcd_standby_weather_percent");
	addSetting(gss, "glcd_standby_weather_curr_temp_x_position");
	addSetting(gss, "glcd_standby_weather_curr_icon_x_position");
	addSetting(gss, "glcd_standby_weather_next_temp_x_position");
	addSetting(gss, "glcd_standby_weather_next_icon_x_position");
	addSetting(gss, "glcd_standby_weather_y_position");

	int res = gss->exec(NULL, "");
	delete gss;
	cGLCD::getInstance()->StandbyMode(false);
	return res;
}

int GLCD_Menu::GLCD_Brightness_Settings()
{
	CMenuWidget *gbs = new CMenuWidget(LOCALE_GLCD_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_GLCD_BRIGHTNESS_SETTINGS);
	gbs->addIntroItems(LOCALE_GLCD_BRIGHTNESS_SETTINGS);

	CMenuForwarder *mf;

	addSetting(gbs, "glcd_brightness", true, NULL, CRCInput::RC_nokey, true);

	addSetting(gbs, "glcd_brightness_standby", true, NULL, CRCInput::RC_nokey, true);

	gbs->addItem(GenericMenuSeparatorLine);

	addSetting(gbs, "glcd_brightness_dim", true, NULL, CRCInput::RC_nokey, true);

	addSetting(gbs, "glcd_brightness_dim_time");

	gbs->addItem(GenericMenuSeparatorLine);

	mf = new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, "brightness_default", CRCInput::RC_nokey);
	//mf->setHint("", LOCALE_TODO);
	gbs->addItem(mf);

	int res = gbs->exec(NULL, "");
	delete gbs;
	cGLCD::getInstance()->StandbyMode(false);
	return res;
}

int GLCD_Menu::GLCD_Theme_Settings()
{
	CMenuWidget *gts = new CMenuWidget(LOCALE_GLCD_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_GLCD_THEME_SETTINGS);
	gts->addIntroItems(LOCALE_GLCD_THEME_SETTINGS);

	cGLCD::getInstance()->SetCfgMode(true);

	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;

	// choose theme

	gts->addItem(new CMenuForwarder(LOCALE_GLCD_THEME, true, NULL, CGLCDThemes::getInstance(), NULL, CRCInput::RC_red));

	gts->addItem(GenericMenuSeparatorLine);

	// standby settings

	gts->addItem(new CMenuForwarder(LOCALE_GLCD_STANDBY_SETTINGS, true, NULL, this, "standby_settings", CRCInput::RC_green));

	gts->addItem(GenericMenuSeparatorLine);

	// font, background image

	gts->addItem(new CMenuForwarder(LOCALE_GLCD_FONT, true, t.glcd_font, this, "select_font", CRCInput::RC_yellow));

	gts->addItem(new CMenuForwarder(LOCALE_GLCD_BACKGROUND, true, t.glcd_background_image, this, "select_background", CRCInput::RC_blue));

	gts->addItem(GenericMenuSeparatorLine);

	// colors

	addSetting(gts, "glcd_theme.glcd_foreground_color");
	addSetting(gts, "glcd_theme.glcd_background_color");

	gts->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_GLCD_POSITION_SETTINGS));

	// channel name

	CMenuItem *item;
	const char *const channel[] = { "glcd_channel_percent", "glcd_channel_align", "glcd_channel_x_position", "glcd_channel_y_position" };
	for (size_t i = 0; i < sizeof(channel) / sizeof(channel[0]); i++)
	{
		item = addSetting(gts, channel[i]);
		if (item)
			afterApply(item, previewChannelName);
	}

	gts->addItem(GenericMenuSeparator);

	// the parts of the layout, each with its switch where it has one, in the order they are drawn
	const char *const layout[] =
	{
		// channel logo
		"glcd_logo", "glcd_logo_percent", "glcd_logo_width_percent", "glcd_logo_x_position", "glcd_logo_y_position", NULL,
		// event
		"glcd_epg_percent", "glcd_epg_align", "glcd_epg_x_position", "glcd_epg_y_position", NULL,
		// event duration
		"glcd_duration", "glcd_duration_percent", "glcd_duration_align", "glcd_duration_x_position", "glcd_duration_y_position", NULL,
		// event start
		"glcd_start", "glcd_start_percent", "glcd_start_align", "glcd_start_x_position", "glcd_start_y_position", NULL,
		// event end
		"glcd_end", "glcd_end_percent", "glcd_end_align", "glcd_end_x_position", "glcd_end_y_position", NULL,
		// progress bar
		"glcd_progressbar", "glcd_progressbar_percent", "glcd_progressbar_width", "glcd_progressbar_x_position", "glcd_progressbar_y_position", "glcd_theme.glcd_progressbar_color", NULL,
		// time
		"glcd_time", "glcd_time_percent", "glcd_time_align", "glcd_time_x_position", "glcd_time_y_position", NULL,
		// weather
		"glcd_weather", "glcd_weather_percent", "glcd_weather_curr_temp_x_position", "glcd_weather_curr_icon_x_position",
		"glcd_weather_next_temp_x_position", "glcd_weather_next_icon_x_position", "glcd_weather_y_position", NULL,
		// status markers
		"glcd_icons_percent", "glcd_icons_y_position", "glcd_icon_cam_x_position", "glcd_icon_dd_x_position",
		"glcd_icon_ecm_x_position", "glcd_icon_mute_x_position", "glcd_icon_rec_x_position", "glcd_icon_timer_x_position",
		"glcd_icon_ts_x_position", "glcd_icon_txt_x_position", NULL
	};
	for (size_t i = 0; i < sizeof(layout) / sizeof(layout[0]); i++)
	{
		if (layout[i] == NULL)
		{
			// The last part has no line after it.
			if (i + 1 < sizeof(layout) / sizeof(layout[0]))
				gts->addItem(GenericMenuSeparatorLine);
		}
		else
			addSetting(gts, layout[i]);
	}

	int res = gts->exec(NULL, "");
	delete gts;
	cGLCD::getInstance()->StandbyMode(false);
	cGLCD::getInstance()->SetCfgMode(false);
	return res;
}

int GLCD_Menu::GLCD_Menu_Select_Driver()
{
	int select = 0;
	int res = menu_return::RETURN_NONE;

	if (cGLCD::getInstance()->GetConfigSize() > 1)
	{
		CMenuWidget *m = new CMenuWidget(LOCALE_GLCD_HEAD, NEUTRINO_ICON_SETTINGS);
		m->addIntroItems(LOCALE_GLCD_DISPLAY);
		CMenuSelectorTarget *selector = new CMenuSelectorTarget(&select);

		CMenuForwarder *mf;
		for (int i = 0; i < cGLCD::getInstance()->GetConfigSize(); i++)
		{
			mf = new CMenuForwarder(cGLCD::getInstance()->GetConfigName(i), true, NULL, selector, to_string(i).c_str());
			mf->setInfoIconRight(i == g_settings.glcd_selected_config ? NEUTRINO_ICON_MARKER_DIALOG_OK : NULL);
			m->addItem(mf);
		}

		m->enableSaveScreen();
		res = m->exec(NULL, "");

		delete selector;

		if (!m->gotAction() || g_settings.glcd_selected_config == select)
			return res;
	}
	g_settings.glcd_selected_config = select;
	if (select_driver)
		select_driver->setOption(cGLCD::getInstance()->GetConfigName(g_settings.glcd_selected_config).c_str());
	applyGlcd("glcd_selected_config");
	return menu_return::RETURN_REPAINT;
}
