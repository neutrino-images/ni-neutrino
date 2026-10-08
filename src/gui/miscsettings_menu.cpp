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
#include <gui/widget/stringinput.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/keyboard_input.h>

#include <driver/screen_max.h>
#include <driver/scanepg.h>
#include <driver/streamts.h>

#include <zapit/femanager.h>
#include <eitd/sectionsd.h>

#include <hardware/video.h>

#include <sectionsdclient/sectionsdclient.h>

extern CPlugins *g_Plugins;
extern cVideo *videoDecoder;

CMiscMenue::CMiscMenue()
{
	width = 50;

	fanNotifier = NULL;
	cpuNotifier = NULL;
	sectionsdConfigNotifier = NULL;

	epg_save = NULL;
	epg_save_standby = NULL;
	epg_save_frequently = NULL;
	epg_read = NULL;
	epg_read_now = NULL;
	epg_read_frequently = NULL;
	epg_dir = NULL;
	epg_scan = NULL;
}

CMiscMenue::~CMiscMenue()
{
}

int CMiscMenue::exec(CMenuTarget *parent, const std::string &actionKey)
{
	printf("init extended settings menu...\n");

	if ((parent != NULL) && (actionKey.find("epg_read_now") == std::string::npos))
		parent->hide();

	if (actionKey == "epgdir")
	{
		const char *action_str = "epg";
		if (chooserDir(g_settings.epg_dir, true, action_str))
			CNeutrinoApp::getInstance()->SendSectionsdConfig();

		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "plugin_dir")
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
	if (sectionsdConfigNotifier == NULL)
		sectionsdConfigNotifier = new CSectionsdConfigNotifier();
	CMenuWidget misc_menue(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP);

	misc_menue.addIntroItems(LOCALE_MISCSETTINGS_HEAD);

	// general
	CMenuWidget misc_menue_general(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP_GENERAL);
	showMiscSettingsMenuGeneral(&misc_menue_general);
	CMenuForwarder *mf = new CMenuForwarder(LOCALE_MISCSETTINGS_GENERAL, true, NULL, &misc_menue_general, NULL, CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_MISC_GENERAL);
	misc_menue.addItem(mf);

	// energy, shutdown
	if (g_info.hw_caps->can_shutdown)
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
	if (g_info.hw_caps->can_cec)
	{
		mf = new CMenuForwarder(LOCALE_VIDEOMENU_HDMI_CEC, true, NULL, &cecsetup, NULL, CRCInput::convertDigitToKey(shortcut++));
		mf->setHint("", LOCALE_MENU_HINT_MISC_CEC);
		misc_menue.addItem(mf);
	}

	if (!g_info.hw_caps->can_shutdown)
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
	if (g_info.hw_caps->can_cpufreq)
	{
		if (cpuNotifier == NULL)
			cpuNotifier = new CCpuFreqNotifier();
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

	delete fanNotifier;
	fanNotifier = NULL;
	delete sectionsdConfigNotifier;
	sectionsdConfigNotifier = NULL;
	delete cpuNotifier;
	cpuNotifier = NULL;

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
	if (fanNotifier == NULL)
		fanNotifier = new CFanControlNotifier();
	addSetting(ms_general, "fan_speed", true, fanNotifier);

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

	COnOffNotifier *miscNotifier = new COnOffNotifier(1);
	addSetting(ms_energy, "shutdown_real", true, miscNotifier);

	miscNotifier->addItem(addSetting(ms_energy, "shutdown_real_rcdelay", !g_settings.shutdown_real));

	std::string shutdown_count = to_string(g_settings.shutdown_count);
	if (shutdown_count.length() < 3)
		shutdown_count.insert(0, 3 - shutdown_count.length(), ' ');
	CStringInput *miscSettings_shutdown_count = new CStringInput(LOCALE_MISCSETTINGS_SHUTDOWN_COUNT, &shutdown_count, 3, LOCALE_MISCSETTINGS_SHUTDOWN_COUNT_HINT1, LOCALE_MISCSETTINGS_SHUTDOWN_COUNT_HINT2, "0123456789 ");
	CMenuForwarder *m2 = new CMenuDForwarder(LOCALE_MISCSETTINGS_SHUTDOWN_COUNT, !g_settings.shutdown_real, shutdown_count, miscSettings_shutdown_count);
	m2->setHint("", LOCALE_MENU_HINT_SHUTDOWN_COUNT);
	miscNotifier->addItem(m2);
	ms_energy->addItem(m2);

	// keep box in soft-standby while recordings are pending (deep-standby boxes only)
	addSetting(ms_energy, "shutdown_block_while_recording");

	m2 = new CMenuDForwarder(LOCALE_MISCSETTINGS_SLEEPTIMER, true, NULL, new CSleepTimerWidget(true));
	m2->setHint("", LOCALE_MENU_HINT_INACT_TIMER);
	ms_energy->addItem(m2);

	addSetting(ms_energy, "sleeptimer_min");

	int res = ms_energy->exec(NULL, "");

	g_settings.shutdown_count = atoi(shutdown_count.c_str());

	delete ms_energy;
	delete miscNotifier;
	return res;
}

// EPG settings
void CMiscMenue::showMiscSettingsMenuEpg(CMenuWidget *ms_epg)
{
	ms_epg->addIntroItems(LOCALE_MISCSETTINGS_EPG_HEAD);
	ms_epg->addKey(CRCInput::RC_help, this, "info");
	ms_epg->addKey(CRCInput::RC_info, this, "info");

	epg_save = addSetting(ms_epg, "epg_save", true, this);
	epg_save_standby = addSetting(ms_epg, "epg_save_standby", g_settings.epg_save);
	epg_save_frequently = addSetting(ms_epg, "epg_save_frequently", g_settings.epg_save, this);
	ms_epg->addItem(GenericMenuSeparator);

	epg_read = addSetting(ms_epg, "epg_read", true, this);
	epg_read_frequently = addSetting(ms_epg, "epg_read_frequently", g_settings.epg_read, this);

	epg_read_now = new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_READ_NOW, g_settings.epg_read, NULL, this, "epg_read_now");
	epg_read_now->setHint("", LOCALE_MENU_HINT_EPG_READ_NOW);
	ms_epg->addItem(epg_read_now);
	ms_epg->addItem(GenericMenuSeparator);

	epg_dir = new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_DIR, (g_settings.epg_save || g_settings.epg_read), g_settings.epg_dir, this, "epgdir");
	epg_dir->setHint("", LOCALE_MENU_HINT_EPG_DIR);
	ms_epg->addItem(epg_dir);
	ms_epg->addItem(GenericMenuSeparatorLine);

	epg_cache = to_string(g_settings.epg_cache);
	if (epg_cache.length() < 2)
		epg_cache.insert(0, 2 - epg_cache.length(), ' ');
	CStringInput *miscSettings_epg_cache = new CStringInput(LOCALE_MISCSETTINGS_EPG_CACHE, &epg_cache, 2, LOCALE_MISCSETTINGS_EPG_CACHE_HINT1, LOCALE_MISCSETTINGS_EPG_CACHE_HINT2, "0123456789 ", sectionsdConfigNotifier);
	CMenuForwarder *mf = new CMenuDForwarder(LOCALE_MISCSETTINGS_EPG_CACHE, true, epg_cache, miscSettings_epg_cache);
	mf->setHint("", LOCALE_MENU_HINT_EPG_CACHE);
	ms_epg->addItem(mf);

	epg_extendedcache = to_string(g_settings.epg_extendedcache);
	if (epg_extendedcache.length() < 3)
		epg_extendedcache.insert(0, 3 - epg_extendedcache.length(), ' ');
	CStringInput *miscSettings_epg_cache_e = new CStringInput(LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE, &epg_extendedcache, 3, LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE_HINT1, LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE_HINT2, "0123456789 ", sectionsdConfigNotifier);
	CMenuForwarder *mf1 = new CMenuDForwarder(LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE, true, epg_extendedcache, miscSettings_epg_cache_e);
	mf1->setHint("", LOCALE_MENU_HINT_EPG_EXTENDEDCACHE);
	ms_epg->addItem(mf1);

	epg_old_events = to_string(g_settings.epg_old_events);
	if (epg_old_events.length() < 3)
		epg_old_events.insert(0, 3 - epg_old_events.length(), ' ');
	CStringInput *miscSettings_epg_old_events = new CStringInput(LOCALE_MISCSETTINGS_EPG_OLD_EVENTS, &epg_old_events, 3, LOCALE_MISCSETTINGS_EPG_OLD_EVENTS_HINT1, LOCALE_MISCSETTINGS_EPG_OLD_EVENTS_HINT2, "0123456789 ", sectionsdConfigNotifier);
	CMenuForwarder *mf2 = new CMenuDForwarder(LOCALE_MISCSETTINGS_EPG_OLD_EVENTS, true, epg_old_events, miscSettings_epg_old_events);
	mf2->setHint("", LOCALE_MENU_HINT_EPG_OLD_EVENTS);
	ms_epg->addItem(mf2);

	epg_max_events = to_string(g_settings.epg_max_events);
	if (epg_max_events.length() < 6)
		epg_max_events.insert(0, 6 - epg_max_events.length(), ' ');
	CStringInput *miscSettings_epg_max_events = new CStringInput(LOCALE_MISCSETTINGS_EPG_MAX_EVENTS, &epg_max_events, 6, LOCALE_MISCSETTINGS_EPG_MAX_EVENTS_HINT1, LOCALE_MISCSETTINGS_EPG_MAX_EVENTS_HINT2, "0123456789 ", sectionsdConfigNotifier);
	CMenuForwarder *mf3 = new CMenuDForwarder(LOCALE_MISCSETTINGS_EPG_MAX_EVENTS, true, epg_max_events, miscSettings_epg_max_events);
	mf3->setHint("", LOCALE_MENU_HINT_EPG_MAX_EVENTS);
	ms_epg->addItem(mf3);

	addSetting(ms_epg, "epg_save_mode", true, this);
	ms_epg->addItem(GenericMenuSeparatorLine);

	addSetting(ms_epg, "epg_scan_mode", true, this);

	epg_scan = addSetting(ms_epg, "epg_scan");
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

	bool make_hd_list = g_settings.make_hd_list;
	bool make_webtv_list = g_settings.make_webtv_list;
	bool make_webradio_list = g_settings.make_webradio_list;
	bool show_empty_favorites = g_settings.show_empty_favorites;

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
	addSetting(ms_chanlist, "enable_sdt", true, this);

	int res = ms_chanlist->exec(NULL, "");
	delete ms_chanlist;
	if (
		   make_hd_list != g_settings.make_hd_list
		|| make_webtv_list != g_settings.make_webtv_list
		|| make_webradio_list != g_settings.make_webradio_list
		|| show_empty_favorites != g_settings.show_empty_favorites
	)
		g_RCInput->postMsg(NeutrinoMessages::EVT_SERVICESCHANGED, 0);
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
	tmdb_onoff = addSetting(ms_oservices, "tmdb_enabled", CApiKey::check_tmdb_api_key());
	tmdb_onoff->hintIcon = NEUTRINO_ICON_HINT_SETTINGS;

#if ENABLE_TMDB_KEY_MANAGE
	changeNotify(LOCALE_TMDB_API_KEY, NULL);
	CKeyboardInput tmdb_api_key_input(LOCALE_TMDB_API_KEY, &g_settings.tmdb_api_key, 32, this);
	CMenuForwarder *mf_tm = new CMenuForwarder(LOCALE_TMDB_API_KEY, true, tmdb_api_key_short, &tmdb_api_key_input);
	mf_tm->setHint(NEUTRINO_ICON_HINT_SETTINGS, LOCALE_MENU_HINT_TMDB_API_KEY);
	ms_oservices->addItem(mf_tm);
#endif

	ms_oservices->addItem(GenericMenuSeparator);

	// omdb
	omdb_onoff = addSetting(ms_oservices, "omdb_enabled", CApiKey::check_omdb_api_key());
	omdb_onoff->hintIcon = NEUTRINO_ICON_HINT_SETTINGS;

#if ENABLE_OMDB_KEY_MANAGE
	changeNotify(LOCALE_OMDB_API_KEY, NULL);
	CKeyboardInput omdb_api_key_input(LOCALE_OMDB_API_KEY, &g_settings.omdb_api_key, 8, this);
	CMenuForwarder *mf_om = new CMenuForwarder(LOCALE_OMDB_API_KEY, true, omdb_api_key_short, &omdb_api_key_input);
	mf_om ->setHint(NEUTRINO_ICON_HINT_SETTINGS, LOCALE_MENU_HINT_OMDB_API_KEY);
	ms_oservices->addItem(mf_om);
#endif

	ms_oservices->addItem(GenericMenuSeparator);

	// shoutcast
	shoutcast_onoff = addSetting(ms_oservices, "shoutcast_enabled", CApiKey::check_shoutcast_dev_id());
	shoutcast_onoff->hintIcon = NEUTRINO_ICON_HINT_SETTINGS;

#if ENABLE_SHOUTCAST_ID_MANAGE
	changeNotify(LOCALE_SHOUTCAST_DEV_ID, NULL);
	CKeyboardInput shoutcast_dev_id_input(LOCALE_SHOUTCAST_DEV_ID, &g_settings.shoutcast_dev_id, 16, this);
	CMenuForwarder *mf_sc = new CMenuForwarder(LOCALE_SHOUTCAST_DEV_ID, true, shoutcast_dev_id_short, &shoutcast_dev_id_input);
	mf_sc->setHint(NEUTRINO_ICON_HINT_SETTINGS, LOCALE_MENU_HINT_SHOUTCAST_DEV_ID);
	ms_oservices->addItem(mf_sc);
#endif

	ms_oservices->addItem(GenericMenuSeparator);

	// youtube
	youtube_onoff = addSetting(ms_oservices, "youtube_enabled", CApiKey::check_youtube_api_key());
	youtube_onoff->hintIcon = NEUTRINO_ICON_HINT_SETTINGS;

#if ENABLE_YOUTUBE_KEY_MANAGE
	changeNotify(LOCALE_YOUTUBE_API_KEY, NULL);
	CKeyboardInput youtube_api_key_input(LOCALE_YOUTUBE_API_KEY, &g_settings.youtube_api_key, 39, this);
	CMenuForwarder *mf_yt = new CMenuForwarder(LOCALE_YOUTUBE_API_KEY, true, youtube_api_key_short, &youtube_api_key_input);
	mf_yt->setHint(NEUTRINO_ICON_HINT_SETTINGS, LOCALE_MENU_HINT_YOUTUBE_API_KEY);
	ms_oservices->addItem(mf_yt);
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

	// port
	CIntInput * miscSettings_streamingport = new CIntInput(LOCALE_STREAMING_PORT, &g_settings.streaming_port, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, this);
	CMenuForwarder * mf = new CMenuDForwarder(LOCALE_STREAMING_PORT, true, to_string(g_settings.streaming_port), miscSettings_streamingport);
	ms_sservices->addItem(mf);

	// ecm
	ecm_onoff = addSetting(ms_sservices, "streaming_ecmmode");
	//ecm_onoff->setHint(NEUTRINO_ICON_HINT_SETTINGS, LOCALE_MENU_HINT_ECMMODE_ENABLED);

	// ecm
	dec_onoff = addSetting(ms_sservices, "streaming_decryptmode");
	//dec_onoff->setHint(NEUTRINO_ICON_HINT_SETTINGS, LOCALE_MENU_HINT_DECRYPTMODE_ENABLED);

	int res = ms_sservices->exec(NULL, "");
	delete ms_sservices;
	return res;
}

// CPU
void CMiscMenue::showMiscSettingsMenuCPUFreq(CMenuWidget *ms_cpu)
{
	ms_cpu->addIntroItems(LOCALE_MISCSETTINGS_CPU);

	addSetting(ms_cpu, "cpufreq", true, cpuNotifier);
	addSetting(ms_cpu, "standby_cpufreq");
}

bool CMiscMenue::changeNotify(const neutrino_locale_t OptionName, void */*data*/)
{
	int ret = menu_return::RETURN_NONE;

	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_HDMI_CEC))
	{
		printf("[neutrino CEC Settings] %s set CEC settings...\n", __FUNCTION__);
		g_settings.hdmi_cec_standby = 0;
		g_settings.hdmi_cec_view_on = 0;
		if (g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF)
		{
			g_settings.hdmi_cec_standby = 1;
			g_settings.hdmi_cec_view_on = 1;
			g_settings.hdmi_cec_mode = VIDEO_HDMI_CEC_MODE_TUNER;
		}
		videoDecoder->SetCECAutoStandby(g_settings.hdmi_cec_standby == 1);
		videoDecoder->SetCECAutoView(g_settings.hdmi_cec_view_on == 1);
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
		videoDecoder->SetAudioDestination(g_settings.hdmi_cec_volume);
#endif
		videoDecoder->SetCECMode((VIDEO_HDMI_CEC_MODE)g_settings.hdmi_cec_mode);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_MISCSETTINGS_CHANNELLIST_ENABLESDT))
	{
		CZapit::getInstance()->SetScanSDT(g_settings.enable_sdt);

		ret = menu_return::RETURN_REPAINT;
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_MISCSETTINGS_EPG_SAVE))
	{
		if (g_settings.epg_save)
			g_settings.epg_read = true;
		epg_save_standby->setActive(g_settings.epg_save);
		epg_save_frequently->setActive(g_settings.epg_save);
		epg_dir->setActive(g_settings.epg_save || g_settings.epg_read);

		CNeutrinoApp::getInstance()->SendSectionsdConfig();

		ret = menu_return::RETURN_REPAINT;
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_MISCSETTINGS_EPG_READ))
	{
		epg_read_frequently->setActive(g_settings.epg_read);
		epg_read_now->setActive(g_settings.epg_read);
		epg_dir->setActive(g_settings.epg_read || g_settings.epg_save);

		CNeutrinoApp::getInstance()->SendSectionsdConfig();

		ret = menu_return::RETURN_REPAINT;
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_MISCSETTINGS_EPG_READ_FREQUENTLY))
	{
		g_settings.epg_read_frequently = g_settings.epg_read ? g_settings.epg_read_frequently : 0;

		CNeutrinoApp::getInstance()->SendSectionsdConfig();

		ret = menu_return::RETURN_REPAINT;
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_MISCSETTINGS_EPG_SAVE_FREQUENTLY))
	{
		g_settings.epg_save_frequently = g_settings.epg_save ? g_settings.epg_save_frequently : 0;

		CNeutrinoApp::getInstance()->SendSectionsdConfig();

		ret = menu_return::RETURN_REPAINT;
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_MISCSETTINGS_EPG_SCAN))
	{
		epg_scan->setActive(g_settings.epg_scan_mode != EPG_SCAN_MODE_OFF /*&& g_settings.epg_save_mode == 0*/);
	}
#if 0
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_MISCSETTINGS_EPG_SAVE_MODE))
	{
		g_settings.epg_scan = EPG_SCAN_FAV;
		epg_scan->setActive(g_settings.epg_scan_mode != EPG_SCAN_MODE_OFF && g_settings.epg_save_mode == 0);
		ret = menu_return::RETURN_REPAINT;
	}
#endif
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_TMDB_API_KEY))
	{
		g_settings.tmdb_enabled = g_settings.tmdb_enabled && CApiKey::check_tmdb_api_key();
		if (g_settings.tmdb_enabled)
			tmdb_api_key_short = g_settings.tmdb_api_key.substr(0, 8) + "...";
		else
			tmdb_api_key_short.clear();
		tmdb_onoff->setActive(CApiKey::check_tmdb_api_key());
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_OMDB_API_KEY))
	{
		g_settings.omdb_enabled = g_settings.omdb_enabled && CApiKey::check_omdb_api_key();
		if (g_settings.omdb_enabled)
			omdb_api_key_short = g_settings.omdb_api_key.substr(0, 8) + "...";
		else
			omdb_api_key_short.clear();
		omdb_onoff->setActive(CApiKey::check_omdb_api_key());
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_SHOUTCAST_DEV_ID))
	{
		g_settings.shoutcast_enabled = g_settings.shoutcast_enabled && CApiKey::check_shoutcast_dev_id();
		if (g_settings.shoutcast_enabled)
			shoutcast_dev_id_short = g_settings.shoutcast_dev_id.substr(0, 8) + "...";
		else
			shoutcast_dev_id_short.clear();
		shoutcast_onoff->setActive(CApiKey::check_shoutcast_dev_id());
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_YOUTUBE_API_KEY))
	{
		g_settings.youtube_enabled = g_settings.youtube_enabled && CApiKey::check_youtube_api_key();
		if (g_settings.youtube_enabled)
			youtube_api_key_short = g_settings.youtube_api_key.substr(0, 8) + "...";
		else
			youtube_api_key_short.clear();
		youtube_onoff->setActive(CApiKey::check_youtube_api_key());
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_STREAMING_PORT))
	{
		CStreamManager::getInstance()->SetPort(g_settings.streaming_port);
	}
	return ret;
}
