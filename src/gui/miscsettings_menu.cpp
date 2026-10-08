/*
	miscsettings_menu implementation - Neutrino-GUI

	Copyright (C) 2010, 2018 T. Graf 'dbt'
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
	along with this program. If not, see <http://www.gnu.org/licenses/>.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>
#include <system/setting_helpers.h>
#include <system/helpers.h>
#include <system/debug.h>
#include <gui/miscsettings_menu.h>
#include <gui/weather_setup.h>
#include <gui/cec_setup.h>
#include <gui/filebrowser.h>
#include <gui/infoicons_setup.h>
#include <gui/keybind_setup.h>
#include <gui/plugins.h>
#include <gui/plugins_hide.h>
#include <gui/sleeptimer.h>
#include <gui/zapit_setup.h>

#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/settingitem.h>
#include <gui/widget/msgbox.h>

#include <driver/screen_max.h>

#include <eitd/sectionsd.h>

#include <coreapi/settings/predicates.h>

extern CPlugins *g_Plugins;

CMiscMenue::CMiscMenue()
{
	width = 50;


}

CMiscMenue::~CMiscMenue()
{
}

int CMiscMenue::exec(CMenuTarget *parent, const std::string &actionKey)
{
	printf("init extended settings menu...\n");

	if ((parent != NULL) && (actionKey.find("epg_read_now") == std::string::npos))
		parent->hide();

	if (actionKey == "plugin_dir")
	{
		const char *action_str = "plugin";
		if (chooserDir(g_settings.plugin_hdd_dir, false, action_str))
			g_Plugins->loadPlugins();

		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "movieplayer_plugin")
	{
		CMenuWidget MoviePluginSelector(LOCALE_MOVIEPLAYER_PLUGIN, NEUTRINO_ICON_FEATURES);
		MoviePluginSelector.addItem(GenericMenuSeparator);
		MoviePluginSelector.addItem(new CMenuForwarder(LOCALE_PLUGINS_NO_PLUGIN, true, NULL, new CMoviePluginChangeExec(), "---", CRCInput::RC_red));
		MoviePluginSelector.addItem(GenericMenuSeparatorLine);
		char id[5];
		int enabled_count = 0;
		for (unsigned int count = 0; count < (unsigned int) g_Plugins->getNumberOfPlugins(); count++)
		{
			if (!g_Plugins->isHidden(count))
			{
				sprintf(id, "%d", count);
				enabled_count++;
				MoviePluginSelector.addItem(new CMenuForwarder(g_Plugins->getName(count), true, NULL, new CMoviePluginChangeExec(), id, CRCInput::convertDigitToKey(count)));
			}
		}

		MoviePluginSelector.exec(NULL, "");
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "info")
	{
		unsigned num = CEitManager::getInstance()->getEventsCount();
		char str[128];
		sprintf(str, "Event count: %d", num);
		ShowMsg(LOCALE_MESSAGEBOX_INFO, str, CMsgBox::mbrBack, CMsgBox::mbBack);
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "energy")
	{
		return showMiscSettingsMenuEnergy(LOCALE_MISCSETTINGS_HEAD, LOCALE_MISCSETTINGS_ENERGY);
	}
	else if (actionKey == "energy_power")
	{
		return showMiscSettingsMenuEnergy(LOCALE_MAINMENU_SETTINGS, LOCALE_MISCSETTINGS_ENERGY);
	}
	else if (actionKey == "channellist")
	{
		return showMiscSettingsMenuChanlist();
	}
	else if (actionKey == "onlineservices")
	{
		return showMiscSettingsMenuOnlineServices();
	}
	else if (actionKey == "plugins")
	{
		return showMiscSettingsMenuPlugins();
	}
	else if(actionKey == "streaming")
	{
		return showMiscSettingsMenuStreaming();
	}
	else if (actionKey == "epg_read_now" || actionKey == "epg_read_now_usermenu")
	{
		CLoaderHint *lh = new CLoaderHint(LOCALE_MISCSETTINGS_EPG_READ);
		lh->paint();

		struct stat my_stat;
		if (stat(g_settings.epg_dir.c_str(), &my_stat) == 0)
		{
			printf("Reading epg cache from %s ...\n", g_settings.epg_dir.c_str());
			g_Sectionsd->readSIfromXML(g_settings.epg_dir.c_str());
		}

		std::list<std::string> xmltv = settingsCopy(g_settings.xmltv_xml);
		for (std::list<std::string>::iterator it = xmltv.begin(); it != xmltv.end(); ++it)
		{
			printf("Reading xmltv epg from %s ...\n", (*it).c_str());
			g_Sectionsd->readSIfromXMLTV((*it).c_str());
		}

		delete lh;

		if (actionKey == "epg_read_now_usermenu")
			return menu_return::RETURN_EXIT_ALL;
		else
			return menu_return::RETURN_REPAINT;
	}

	return showMiscSettingsMenu();
}

// show misc settings menue
int CMiscMenue::showMiscSettingsMenu()
{
	int shortcut = 1;

	// misc settings
	CMenuWidget misc_menue(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP);

	misc_menue.addIntroItems(LOCALE_MISCSETTINGS_HEAD);

	// general
	CMenuWidget misc_menue_general(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP_GENERAL);
	showMiscSettingsMenuGeneral(&misc_menue_general);
	CMenuForwarder *mf = new CMenuForwarder(LOCALE_MISCSETTINGS_GENERAL, true, NULL, &misc_menue_general, NULL, CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_MISC_GENERAL);
	misc_menue.addItem(mf);

	// energy, shutdown
	if (coreapi::canShutdown())
	{
		mf = new CMenuForwarder(LOCALE_MISCSETTINGS_ENERGY, true, NULL, this, "energy", CRCInput::RC_green);
		mf->setHint("", LOCALE_MENU_HINT_MISC_ENERGY);
		misc_menue.addItem(mf);
	}

	// epg
	CMenuWidget misc_menue_epg(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP_EPG);
	showMiscSettingsMenuEpg(&misc_menue_epg);
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_HEAD, true, NULL, &misc_menue_epg, NULL, CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_MISC_EPG);
	misc_menue.addItem(mf);

	// filebrowser settings
	CMenuWidget misc_menue_fbrowser(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP_FILEBROWSER);
	showMiscSettingsMenuFBrowser(&misc_menue_fbrowser);
	mf = new CMenuForwarder(LOCALE_FILEBROWSER_HEAD, true, NULL, &misc_menue_fbrowser, NULL, CRCInput::RC_blue);
	mf->setHint("", LOCALE_MENU_HINT_MISC_FILEBROWSER);
	misc_menue.addItem(mf);

	misc_menue.addItem(GenericMenuSeparatorLine);

	// cec settings
	CCECSetup cecsetup;
	if (coreapi::canCec())
	{
		mf = new CMenuForwarder(LOCALE_VIDEOMENU_HDMI_CEC, true, NULL, &cecsetup, NULL, CRCInput::convertDigitToKey(shortcut++));
		mf->setHint("", LOCALE_MENU_HINT_MISC_CEC);
		misc_menue.addItem(mf);
	}

	if (!coreapi::canShutdown())
	{
		/* we don't have the energy menu, but put the sleeptimer directly here */
		mf = new CMenuDForwarder(LOCALE_MISCSETTINGS_SLEEPTIMER, true, NULL, new CSleepTimerWidget(true), NULL, CRCInput::convertDigitToKey(shortcut++));
		mf->setHint("", LOCALE_MENU_HINT_INACT_TIMER);
		misc_menue.addItem(mf);
	}

	// channellist
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_CHANNELLIST, true, NULL, this, "channellist", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_MISC_CHANNELLIST);
	misc_menue.addItem(mf);

	// start channels
	CZapitSetup zapitsetup;
	mf = new CMenuForwarder(LOCALE_ZAPITSETUP_HEAD, true, NULL, &zapitsetup, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_MISC_ZAPIT);
	misc_menue.addItem(mf);

	// onlineservices
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_ONLINESERVICES, true, NULL, this, "onlineservices", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_MISC_ONLINESERVICES);
	misc_menue.addItem(mf);

	// CPU
	CMenuWidget misc_menue_cpu(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width);
	if (coreapi::canCpufreq())
	{
		showMiscSettingsMenuCPUFreq(&misc_menue_cpu);
		mf = new CMenuForwarder(LOCALE_MISCSETTINGS_CPU, true, NULL, &misc_menue_cpu, NULL, CRCInput::convertDigitToKey(shortcut++));
		mf->setHint("", LOCALE_MENU_HINT_MISC_CPUFREQ);
		misc_menue.addItem(mf);
	}

	// Infoicons Setup
	CInfoIconsSetup infoicons_setup;
	mf = new CMenuForwarder(LOCALE_INFOICONS_HEAD, true, NULL, &infoicons_setup, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_INFOICONS_HEAD);
	misc_menue.addItem(mf);

	// plugins
	mf = new CMenuForwarder(LOCALE_PLUGINS_CONTROL, true, NULL, this, "plugins", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_PLUGINS_CONTROL);
	misc_menue.addItem(mf);

	// streaming
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_STREAMING, true, NULL, this, "streaming", CRCInput::convertDigitToKey(shortcut++));
	//mf->setHint("", LOCALE_MENU_HINT_MISC_STREAMING);
	misc_menue.addItem(mf);
	int res = misc_menue.exec(NULL, "");


	return res;
}

const CMenuOptionChooser::keyval DEBUG_MODE_OPTIONS[DEBUG_MODES] =
{
	{ DEBUG_NORMAL	, LOCALE_DEBUG_LEVEL_1	},
	{ DEBUG_INFO	, LOCALE_DEBUG_LEVEL_2	},
	{ DEBUG_DEBUG	, LOCALE_DEBUG_LEVEL_3	}
};

// general settings
void CMiscMenue::showMiscSettingsMenuGeneral(CMenuWidget *ms_general)
{
	ms_general->addIntroItems(LOCALE_MISCSETTINGS_GENERAL);

	// standby after boot
	addSetting(ms_general, "power_standby");
	addSetting(ms_general, "cacheTXT");

	// fan speed
	addSetting(ms_general, "fan_speed");

	// set debug level
	ms_general->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_DEBUG));
	CMenuOptionChooser *md = new CMenuOptionChooser(LOCALE_DEBUG_LEVEL, &debug, DEBUG_MODE_OPTIONS, DEBUG_MODES, true);
	//mc->setHint("", LOCALE_MENU_HINT_START_TOSTANDBY);
	ms_general->addItem(md);
}

// energy and shutdown settings
int CMiscMenue::showMiscSettingsMenuEnergy(neutrino_locale_t title, neutrino_locale_t sub_title)
{
	CMenuWidget *ms_energy = new CMenuWidget(title, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP_ENERGY);
	ms_energy->addIntroItems(sub_title);

	addSetting(ms_energy, "shutdown_real");
	addSetting(ms_energy, "shutdown_real_rcdelay");
	addNumberSetting(ms_energy, "shutdown_count", true, NULL, CRCInput::RC_nokey, false, true);

	// keep box in soft-standby while recordings are pending (deep-standby boxes only)
	addSetting(ms_energy, "shutdown_block_while_recording");

	CMenuForwarder *m2 = new CMenuDForwarder(LOCALE_MISCSETTINGS_SLEEPTIMER, true, NULL, new CSleepTimerWidget(true));
	m2->setHint("", LOCALE_MENU_HINT_INACT_TIMER);
	ms_energy->addItem(m2);

	addSetting(ms_energy, "sleeptimer_min");

	int res = ms_energy->exec(NULL, "");

	delete ms_energy;
	return res;
}

// EPG settings
void CMiscMenue::showMiscSettingsMenuEpg(CMenuWidget *ms_epg)
{
	ms_epg->addIntroItems(LOCALE_MISCSETTINGS_EPG_HEAD);
	ms_epg->addKey(CRCInput::RC_help, this, "info");
	ms_epg->addKey(CRCInput::RC_info, this, "info");

	addSetting(ms_epg, "epg_save");
	addSetting(ms_epg, "epg_save_standby");
	addSetting(ms_epg, "epg_save_frequently");
	ms_epg->addItem(GenericMenuSeparator);

	addSetting(ms_epg, "epg_read");
	addSetting(ms_epg, "epg_read_frequently");

	/* Not a setting, so no row says when it can be used: it follows the switch
	   that makes the guide worth reading, whoever moves it. */
	CFollowForwarder *epg_read_now = new CFollowForwarder(LOCALE_MISCSETTINGS_EPG_READ_NOW, g_settings.epg_read, NULL, this, "epg_read_now");
	epg_read_now->follow(ms_epg, "epg_read", std::vector<std::string>(), []() { return g_settings.epg_read != 0; });
	epg_read_now->setHint("", LOCALE_MENU_HINT_EPG_READ_NOW);
	ms_epg->addItem(epg_read_now);
	ms_epg->addItem(GenericMenuSeparator);

	addSetting(ms_epg, "epg_dir");
	ms_epg->addItem(GenericMenuSeparatorLine);

	addNumberSetting(ms_epg, "epg_cache_time", true, NULL, CRCInput::RC_nokey, false, true);
	addNumberSetting(ms_epg, "epg_extendedcache_time", true, NULL, CRCInput::RC_nokey, false, true);
	addNumberSetting(ms_epg, "epg_old_events", true, NULL, CRCInput::RC_nokey, false, true);
	addNumberSetting(ms_epg, "epg_max_events", true, NULL, CRCInput::RC_nokey, false, true);

	addSetting(ms_epg, "epg_save_mode");
	ms_epg->addItem(GenericMenuSeparatorLine);

	addSetting(ms_epg, "epg_scan_mode");

	addSetting(ms_epg, "epg_scan");
}

// filebrowser settings
void CMiscMenue::showMiscSettingsMenuFBrowser(CMenuWidget *ms_fbrowser)
{
	ms_fbrowser->addIntroItems(LOCALE_FILEBROWSER_HEAD);

	addSetting(ms_fbrowser, "filesystem_is_utf8");
	addSetting(ms_fbrowser, "filebrowser_showrights");
	addSetting(ms_fbrowser, "filebrowser_denydirectoryleave");
}

// channellist
int CMiscMenue::showMiscSettingsMenuChanlist()
{
	CMenuWidget *ms_chanlist = new CMenuWidget(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP_CHANNELLIST);
	ms_chanlist->addIntroItems(LOCALE_MISCSETTINGS_CHANNELLIST);

	addSetting(ms_chanlist, "make_hd_list");
	addSetting(ms_chanlist, "make_webtv_list");
	addSetting(ms_chanlist, "make_webradio_list");
	addSetting(ms_chanlist, "make_new_list");
	addSetting(ms_chanlist, "make_removed_list");
	addSetting(ms_chanlist, "keep_channel_numbers");
	addSetting(ms_chanlist, "zap_cycle");
	addSetting(ms_chanlist, "channellist_new_zap_mode");
	addSetting(ms_chanlist, "channellist_numeric_adjust");
	addSetting(ms_chanlist, "show_empty_favorites");
	addSetting(ms_chanlist, "enable_sdt");

	int res = ms_chanlist->exec(NULL, "");
	delete ms_chanlist;
	return res;
}

// online services
int CMiscMenue::showMiscSettingsMenuOnlineServices()
{
	CMenuWidget *ms_oservices = new CMenuWidget(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP_ONLINESERVICES);
	ms_oservices->addIntroItems(LOCALE_MISCSETTINGS_ONLINESERVICES);

	// weather
	CMenuForwarder *mf = new CMenuForwarder(LOCALE_WEATHER_ENABLED, true, NULL, new CWeatherSetup());
	mf->setHint(NEUTRINO_ICON_HINT_SETTINGS, LOCALE_MENU_HINT_WEATHER_ENABLED);
	ms_oservices->addItem(mf);

	ms_oservices->addItem(GenericMenuSeparator);

	// tmdb
	CMenuItem *tmdb_onoff = addSetting(ms_oservices, "tmdb_enabled");
	if (tmdb_onoff != NULL)
		tmdb_onoff->hintIcon = NEUTRINO_ICON_HINT_SETTINGS;

#if ENABLE_TMDB_KEY_MANAGE
	CMenuItem *tmdb_key = addSetting(ms_oservices, "tmdb_api_key");
	if (tmdb_key != NULL)
		tmdb_key->hintIcon = NEUTRINO_ICON_HINT_SETTINGS;
#endif

	ms_oservices->addItem(GenericMenuSeparator);

	// omdb
	CMenuItem *omdb_onoff = addSetting(ms_oservices, "omdb_enabled");
	if (omdb_onoff != NULL)
		omdb_onoff->hintIcon = NEUTRINO_ICON_HINT_SETTINGS;

#if ENABLE_OMDB_KEY_MANAGE
	CMenuItem *omdb_key = addSetting(ms_oservices, "omdb_api_key");
	if (omdb_key != NULL)
		omdb_key->hintIcon = NEUTRINO_ICON_HINT_SETTINGS;
#endif

	ms_oservices->addItem(GenericMenuSeparator);

	// shoutcast
	CMenuItem *shoutcast_onoff = addSetting(ms_oservices, "shoutcast_enabled");
	if (shoutcast_onoff != NULL)
		shoutcast_onoff->hintIcon = NEUTRINO_ICON_HINT_SETTINGS;

#if ENABLE_SHOUTCAST_ID_MANAGE
	CMenuItem *shoutcast_key = addSetting(ms_oservices, "shoutcast_dev_id");
	if (shoutcast_key != NULL)
		shoutcast_key->hintIcon = NEUTRINO_ICON_HINT_SETTINGS;
#endif

	ms_oservices->addItem(GenericMenuSeparator);

	// youtube
	CMenuItem *youtube_onoff = addSetting(ms_oservices, "youtube_enabled");
	if (youtube_onoff != NULL)
		youtube_onoff->hintIcon = NEUTRINO_ICON_HINT_SETTINGS;

#if ENABLE_YOUTUBE_KEY_MANAGE
	CMenuItem *youtube_key = addSetting(ms_oservices, "youtube_api_key");
	if (youtube_key != NULL)
		youtube_key->hintIcon = NEUTRINO_ICON_HINT_SETTINGS;
#endif

	int res = ms_oservices->exec(NULL, "");
	delete ms_oservices;
	return res;
}

// plugins
int CMiscMenue::showMiscSettingsMenuPlugins()
{
	CMenuWidget *ms_plugins = new CMenuWidget(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP_PLUGINS);
	ms_plugins->addIntroItems(LOCALE_PLUGINS_CONTROL);

	CMenuForwarder *mf = new CMenuForwarder(LOCALE_PLUGINS_HDD_DIR, true, g_settings.plugin_hdd_dir, this, "plugin_dir");
	mf->setHint("", LOCALE_MENU_HINT_PLUGINS_HDD_DIR);
	ms_plugins->addItem(mf);

	mf = new CMenuForwarder(LOCALE_MPKEY_PLUGIN, true, g_settings.movieplayer_plugin, this, "movieplayer_plugin");
	mf->setHint("", LOCALE_MENU_HINT_MOVIEPLAYER_PLUGIN);
	ms_plugins->addItem(mf);

	ms_plugins->addItem(GenericMenuSeparatorLine);

	CPluginsHideMenu pluginsHideMenu;
	mf = new CMenuForwarder(LOCALE_PLUGINS_HIDE, true, NULL, &pluginsHideMenu, NULL, CRCInput::RC_red);
	mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_PLUGINS_HIDE);
	ms_plugins->addItem(mf);

	int res = ms_plugins->exec(NULL, "");
	delete ms_plugins;
	return res;
}

// streaming
int CMiscMenue::showMiscSettingsMenuStreaming()
{
	CMenuWidget *ms_sservices = new CMenuWidget(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP_STREAMING);
	ms_sservices->addIntroItems(LOCALE_MISCSETTINGS_STREAMING);

	addNumberSetting(ms_sservices, "streaming_port", true, NULL, CRCInput::RC_nokey, false, true);

	addSetting(ms_sservices, "streaming_ecmmode");

	addSetting(ms_sservices, "streaming_decryptmode");

	int res = ms_sservices->exec(NULL, "");
	delete ms_sservices;
	return res;
}

// CPU
void CMiscMenue::showMiscSettingsMenuCPUFreq(CMenuWidget *ms_cpu)
{
	ms_cpu->addIntroItems(LOCALE_MISCSETTINGS_CPU);

	addSetting(ms_cpu, "cpufreq");
	addSetting(ms_cpu, "standby_cpufreq");
}
