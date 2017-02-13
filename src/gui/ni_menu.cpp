/*
	ni_menu

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
#include <iostream>
#include <fstream>
#include <sstream>
#include <signal.h>
#include <unistd.h>

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <neutrino_menue.h>

#include <gui/infoicons_setup.h>
#include <gui/lcd4l_setup.h>
#include <gui/netfs_setup.h>
#include <gui/filebrowser.h>
#include <gui/plugins_hide.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>
#include <gui/widget/menue.h>

#include <gui/ni_menu.h>

#include <system/helpers.h>

#include <driver/screen_max.h>

typedef struct emu_menu_data_t
{
	neutrino_locale_t name;
	neutrino_locale_t hint;
	const char * cam_name;
	const char * cam_file;
	int cam_exist;
	int flag_exist;
} emu_menu_data_struct;

emu_menu_data_t emu_menu[]=
{
	{LOCALE_CAMD_ITEM_MGCAMD_NAME,	LOCALE_CAMD_ITEM_MGCAMD_HINT,	"MGCAMD",	"mgcamd",	0, 0},
	{LOCALE_CAMD_ITEM_DOSCAM_NAME,	LOCALE_CAMD_ITEM_DOSCAM_HINT,	"DOSCAM",	"doscam",	0, 0},
	{LOCALE_CAMD_ITEM_OSCAM_NAME,	LOCALE_CAMD_ITEM_OSCAM_HINT,	"OSCAM",	"oscam",	0, 0},
	{LOCALE_CAMD_ITEM_OSEMU_NAME,	LOCALE_CAMD_ITEM_OSEMU_HINT,	"OSEMU",	"osemu",	0, 0},
	{LOCALE_CAMD_ITEM_NEWCS_NAME,	LOCALE_CAMD_ITEM_NEWCS_HINT,	"NEWCS",	"newcs",	0, 0},
	{LOCALE_CAMD_ITEM_GBOX_NAME,	LOCALE_CAMD_ITEM_GBOX_HINT,	"GBOX.NET",	"gbox",		0, 0},
	{LOCALE_CAMD_ITEM_CS2GBOX_NAME,	LOCALE_CAMD_ITEM_CS2GBOX_HINT,	"CS2GBOX",	"cs2gbox",	0, 0}
};
#define MAXEMU (sizeof(emu_menu)/sizeof(struct emu_menu_data_t))

typedef struct plugin_menu_data_t
{
	neutrino_locale_t name;
	neutrino_locale_t desc;
	const char * icon;
	const char * flag;
	int plugin_exist;
	int flag_exist;
} plugin_menu_data_struct;

plugin_menu_data_t plugin_menu[]=
{
	{LOCALE_PLUGIN_ITEM_FCM_NAME,		LOCALE_PLUGIN_ITEM_FCM_DESC,		NEUTRINO_ICON_HINT_FCM,		"fritzcallmonitor",	0, 0},
	{LOCALE_PLUGIN_ITEM_NFSSERVER_NAME,	LOCALE_PLUGIN_ITEM_NFSSERVER_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"nfsd",			0, 0},
	{LOCALE_PLUGIN_ITEM_SAMBASERVER_NAME,	LOCALE_PLUGIN_ITEM_SAMBASERVER_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"samba",		0, 0},
	{LOCALE_PLUGIN_ITEM_TUXCALD_NAME,	LOCALE_PLUGIN_ITEM_TUXCALD_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"tuxcald",		0, 0},
	{LOCALE_PLUGIN_ITEM_TUXMAILD_NAME,	LOCALE_PLUGIN_ITEM_TUXMAILD_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"tuxmaild",		0, 0},
	{LOCALE_PLUGIN_ITEM_EMMREMIND_NAME,	LOCALE_PLUGIN_ITEM_EMMREMIND_DESC,	NEUTRINO_ICON_HINT_EMMRD,	"emmrd",		0, 0},
	{LOCALE_PLUGIN_ITEM_INADYN_NAME,	LOCALE_PLUGIN_ITEM_INADYN_DESC,		NEUTRINO_ICON_HINT_IMAGELOGO,	"inadyn",		0, 0},
	{LOCALE_PLUGIN_ITEM_DROPBEAR_NAME,	LOCALE_PLUGIN_ITEM_DROPBEAR_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"dropbear",		0, 0},
	{LOCALE_PLUGIN_ITEM_DJMOUNT_NAME,	LOCALE_PLUGIN_ITEM_DJMOUNT_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"djmount",		0, 0},
	{LOCALE_PLUGIN_ITEM_USHARE_NAME,	LOCALE_PLUGIN_ITEM_USHARE_DESC,		NEUTRINO_ICON_HINT_IMAGELOGO,	"ushare",		0, 0},
	{LOCALE_PLUGIN_ITEM_XUPNPD_NAME,	LOCALE_PLUGIN_ITEM_XUPNPD_DESC,		NEUTRINO_ICON_HINT_IMAGELOGO,	"xupnpd",		0, 0},
	{LOCALE_PLUGIN_ITEM_CROND_NAME,		LOCALE_PLUGIN_ITEM_CROND_DESC,		NEUTRINO_ICON_HINT_IMAGELOGO,	"crond",		0, 0}
};
#define MAXPLUGIN (sizeof(plugin_menu)/sizeof(struct plugin_menu_data_t))

CNIMenu::CNIMenu()
{
	width = 40;
}

CNIMenu::~CNIMenu()
{
}

CNIMenu* CNIMenu::getInstance()
{
	static CNIMenu* NIMenu = NULL;
	if (!NIMenu)
		NIMenu = new CNIMenu();
	return NIMenu;
}

int CNIMenu::exec(CMenuTarget* parent, const std::string &actionkey)
{
	printf("CNIMenu::exec: actionkey %s\n", actionkey.c_str());
	int res = menu_return::RETURN_REPAINT;
	char *buffer;
	ssize_t read;
	size_t len;
	FILE *fh;
	std::ostringstream buf;

        if (parent)
                parent->hide();

	if (actionkey == "camd_reset")
	{
		CHintBox hintbox(LOCALE_CAMD_CONTROL, g_Locale->getText(LOCALE_CAMD_MSG_RESET));
		hintbox.paint();

		buf.str("");
		buf << EMU_START_SCRIPT << " restart";

		printf("[ni_menu.cpp] executing %s\n", buf.str().c_str());
		if (my_system(2, EMU_START_SCRIPT, "restart") != 0)
			printf("[ni_menu.cpp] executing %s failed\n", buf.str().c_str());
		sleep(1);

		hintbox.hide();

		return menu_return::RETURN_EXIT_ALL;
	}
	else if(actionkey == "savesettings")
	{
		CHintBox hintbox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_MAINSETTINGS_SAVESETTINGSNOW_HINT));
		hintbox.paint();

		CNeutrinoApp::getInstance()->saveSetup(NEUTRINO_SETTINGS_FILE);

		hintbox.hide();

		return res;
	}
	else if(actionkey == "ecmInfo")
	{
		buffer=NULL;
		CFileHelpers fhlp;
		if (fhlp.copyFile("/tmp/ecm.info", "/tmp/ecm.info.tmp", 0644))
		{
			if ((fh = fopen("/tmp/ecm.info.tmp", "r")))
			{
				std::string str = "";
				while ((read = getline(&buffer, &len, fh)) != -1)
				{
					str += buffer;
				}
				fclose(fh);
				remove("/tmp/ecm.info.tmp");
				if(buffer)
					free(buffer);
				ShowHint(LOCALE_ECMINFO, str.c_str(), 450, 20);
			}
		}
		else
			ShowHint(LOCALE_ECMINFO, LOCALE_ECMINFO_NULL, 450, 20);

		return menu_return::RETURN_EXIT_ALL;
	}

	res = show();

	return res;
}

#if 0
bool CNIMenu::changeNotify(const neutrino_locale_t OptionName, void * /*data*/)
{
#if 0
	int val = 0;

	if (data)
		val = (*(int *)data);
#endif
	return false;
}
#endif

bool CNITouchFileNotifier::changeNotify(const neutrino_locale_t, void * data)
{
	std::ostringstream buf;

	buf << FLAGDIR << "/." << filename;
	std::string flag = buf.str();

	if ((*(int *)data) != 0)
	{
		FILE * fd = fopen(flag.c_str(), "w");
		if (fd)
		{
			fclose(fd);
			if (strstr(filename, "scart_osd_fix"))
			{
				//change to scart-osd-fix values
				g_settings.screen_StartX_crt_0 = 29;
				g_settings.screen_StartY_crt_0 = 43;
				g_settings.screen_EndX_crt_0 = 693;
				g_settings.screen_EndY_crt_0 = 535;
				g_settings.screen_preset = 0;

				//set values
				g_settings.screen_StartX = g_settings.screen_preset ? g_settings.screen_StartX_lcd_0 : g_settings.screen_StartX_crt_0;
				g_settings.screen_StartY = g_settings.screen_preset ? g_settings.screen_StartY_lcd_0 : g_settings.screen_StartY_crt_0;
				g_settings.screen_EndX = g_settings.screen_preset ? g_settings.screen_EndX_lcd_0 : g_settings.screen_EndX_crt_0;
				g_settings.screen_EndY = g_settings.screen_preset ? g_settings.screen_EndY_lcd_0 : g_settings.screen_EndY_crt_0;

				CFrameBuffer::getInstance()->Clear();

				g_settings.font_scaling_x = 100;
				g_settings.font_scaling_y = 100;
			}
			else if (strstr(filename, "mgcamd")	||
				strstr(filename, "newcs")	||
				strstr(filename, "osemu")	||
				strstr(filename, "doscam")	||
				strstr(filename, "oscam")	||
				strstr(filename, "cs2gbox")	||
				strstr(filename, "gbox"))
			{
				CHintBox hintbox(LOCALE_CAMD_CONTROL, g_Locale->getText(LOCALE_CAMD_MSG_START));
				hintbox.paint();

				buf.str("");
				buf << EMU_START_SCRIPT << " start " << filename;

				printf("[ni_menu.cpp] executing %s\n", buf.str().c_str());
				if (my_system(3, EMU_START_SCRIPT, "start", filename) != 0)
					printf("[ni_menu.cpp] executing %s failed\n", buf.str().c_str());
				sleep(1);

				hintbox.hide();
			}
			else
			{
				buf.str("");
				buf << "service " << filename << " start";
				printf("[ni_menu.cpp] executing %s\n", buf.str().c_str());

				if (my_system(3, "service", filename, "start") != 0)
					printf("[ni_menu.cpp] executing %s failed\n", buf.str().c_str());
			}
		}
	}
	else
	{
		buf.str("");
		if (strstr(filename, "scart_osd_fix"))
		{
			//reset to defaults
			g_settings.screen_StartX_crt_0 = 60;
			g_settings.screen_StartY_crt_0 = 20;
			g_settings.screen_EndX_crt_0 = 1220;
			g_settings.screen_EndY_crt_0 = 560;

			//set values
			g_settings.screen_StartX = g_settings.screen_preset ? g_settings.screen_StartX_lcd_0 : g_settings.screen_StartX_crt_0;
			g_settings.screen_StartY = g_settings.screen_preset ? g_settings.screen_StartY_lcd_0 : g_settings.screen_StartY_crt_0;
			g_settings.screen_EndX = g_settings.screen_preset ? g_settings.screen_EndX_lcd_0 : g_settings.screen_EndX_crt_0;
			g_settings.screen_EndY = g_settings.screen_preset ? g_settings.screen_EndY_lcd_0 : g_settings.screen_EndY_crt_0;

			CFrameBuffer::getInstance()->Clear();

			g_settings.font_scaling_x = 105;
			g_settings.font_scaling_y = 105;
		}
		else if (strstr(filename, "mgcamd")	||
			strstr(filename, "newcs")	||
			strstr(filename, "osemu")	||
			strstr(filename, "doscam")	||
			strstr(filename, "oscam")	||
			strstr(filename, "cs2gbox")	||
			strstr(filename, "gbox"))
		{
			CHintBox hintbox(LOCALE_CAMD_CONTROL, g_Locale->getText(LOCALE_CAMD_MSG_STOP));
			hintbox.paint();

			buf << EMU_START_SCRIPT << " stop " << filename;
			printf("[ni_menu.cpp] executing %s\n", buf.str().c_str());
			if (my_system(3, EMU_START_SCRIPT, "stop", filename) != 0)
				printf("[ni_menu.cpp] executing %s failed\n", buf.str().c_str());
			sleep(1);

			hintbox.hide();
		}
		else
		{
			buf << "service " << filename << " stop";
			printf("[ni_menu.cpp] executing %s\n", buf.str().c_str());

			if (my_system(3, "service", filename, "stop") != 0)
				printf("[ni_menu.cpp] executing %s failed\n", buf.str().c_str());
		}
		remove(flag.c_str());
	}
	return menu_return::RETURN_REPAINT;
}

int CNIMenu::show()
{
	int shortcut = 1;
	int cam_shortcut = 1;
	int plugin_shortcut = 0;

	std::ostringstream buf;
	char *buffer;
	ssize_t read;
	size_t len;
	FILE *fh;

	CMenuWidget* ni_menu = new CMenuWidget(LOCALE_NIMENU_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_NI_MENU);
	ni_menu->addIntroItems();

	//apply settings
	mf = new CMenuForwarder(LOCALE_MAINSETTINGS_SAVESETTINGSNOW, true, NULL, this, "savesettings", CRCInput::RC_red);
	mf->setHint(NEUTRINO_ICON_HINT_SAVE_SETTINGS, LOCALE_MENU_HINT_SAVE_SETTINGS);
	ni_menu->addItem(mf);

	// --- camd settings ---
	ni_menu->addItem(new CMenuSeparator(CMenuSeparator::ALIGN_CENTER | CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_CAMD_CONTROL));

	// camd reset
	mf =  new CMenuForwarder(LOCALE_CAMD_RESET, true, NULL, this, "camd_reset", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_CAMD_RESET);
	ni_menu->addItem(mf);

	// camd settings
	CMenuWidget* emuMenu = new CMenuWidget(LOCALE_CAMD_CONTROL, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_CAMD_CONTROL);
		emuMenu->addIntroItems();

		for (unsigned int i = 0; i < MAXEMU; i++)
		{
			std::string vinfo = "";

			buf.str("");
			buf << "/var/bin/" << emu_menu[i].cam_file;

			emu_menu[i].cam_exist = file_exists(buf.str().c_str());

			if (emu_menu[i].cam_exist)
			{
				buf.str("");
				buf << "vinfo " << emu_menu[i].cam_name << " /var/bin/" << emu_menu[i].cam_file;

				buffer=NULL;
				if ((fh = popen(buf.str().c_str(), "r")))
				{
					while ((read = getline(&buffer, &len, fh)) != -1)
						vinfo += buffer;
					pclose(fh);
					if (buffer)
						free(buffer);
				}
				else
					printf("[vinfo] popen error\n" );
			}

			if (getpidof(emu_menu[i].cam_file))
				emu_menu[i].flag_exist = 1;
			else
				emu_menu[i].flag_exist = 0;

			//remove linebreaks from vinfo output
			std::string::size_type spos = vinfo.find_first_of("\r\n");
			while (spos != std::string::npos)
			{
				vinfo.replace(spos, 1, " ");
				spos = vinfo.find_first_of("\r\n");
			}


			std::string hint(g_Locale->getText(emu_menu[i].hint));
			hint.append("\nvinfo: " + vinfo);

			mc = new CMenuOptionChooser(emu_menu[i].name, &emu_menu[i].flag_exist, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, emu_menu[i].cam_exist, new CNITouchFileNotifier(emu_menu[i].cam_file), CRCInput::convertDigitToKey(cam_shortcut++));
			mc->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, hint);
			emuMenu->addItem(mc);
		}

	mf = new CMenuForwarder(LOCALE_CAMD_CONTROL, true, NULL, emuMenu, "", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_CAMD);
	ni_menu->addItem(mf);

	// --- Special settings ---
	ni_menu->addItem(new CMenuSeparator(CMenuSeparator::ALIGN_CENTER | CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_NIMENU_HEAD_SPECIAL));

	// Infoicons Setup
	CInfoIconsSetup infoicons_setup;
	mf = new CMenuForwarder(LOCALE_INFOICONS_HEAD, true, NULL, &infoicons_setup, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_INFOICONS_HEAD);
	ni_menu->addItem(mf);

	// NetFS Setup
	CNETFSSetup netfs_setup;
	mf = new CMenuForwarder(LOCALE_NETFS_MENU_MAIN_HEAD, true, NULL, &netfs_setup, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_NETFS_MENU_MAIN_HEAD);
	ni_menu->addItem(mf);

	// Plugin Menu
	CMenuWidget* pluginMenu = new CMenuWidget(LOCALE_PLUGINS_CONTROL, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_PLUGINS_CONTROL);
		pluginMenu->addIntroItems();

		// Plugins for usermenu
		CPluginsHideMenu pluginsHideMenu;
		mf = new CMenuForwarder(LOCALE_PLUGINS_HIDE, true, NULL, &pluginsHideMenu, NULL, CRCInput::RC_blue);
		mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_PLUGINS_HIDE);
		pluginMenu->addItem(mf);
		pluginMenu->addItem(GenericMenuSeparatorLine);

		// Plugin service
		for (unsigned int i = 0; i < MAXPLUGIN; i++)
		{
			buf.str("");
			buf << "/var/etc/." << plugin_menu[i].flag;

			plugin_menu[i].flag_exist=file_exists(buf.str().c_str());
			CNITouchFileNotifier * pluginFileNotifier = new CNITouchFileNotifier(plugin_menu[i].flag);

			mc = new CMenuOptionChooser(plugin_menu[i].name, &plugin_menu[i].flag_exist, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, pluginFileNotifier, CRCInput::convertDigitToKey(plugin_shortcut++));
			mc->setHint(plugin_menu[i].icon, plugin_menu[i].desc);
			pluginMenu->addItem(mc);
		}

	mf = new CMenuForwarder(LOCALE_PLUGINS_CONTROL, true, NULL, pluginMenu, "", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_PLUGINS_CONTROL);
	ni_menu->addItem(mf);

	CLCD4lSetup lcd4lSetup;
	mf = new CMenuForwarder(LOCALE_LCD4L_SUPPORT, true, NULL, &lcd4lSetup, "", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_LCD4L, LOCALE_MENU_HINT_LCD4L_SUPPORT);
	ni_menu->addItem(mf);

	//add I_TYPE_NI_MENU plugins
	ni_menu->integratePlugins(CPlugins::I_TYPE_NI_MENU, shortcut);

	int res = ni_menu->exec(NULL, "");

	ni_menu->hide();
	delete ni_menu;
	delete emuMenu;
	delete pluginMenu;

	return res;
}
