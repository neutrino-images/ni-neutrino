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

#include <gui/netfs_setup.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>

#include <gui/ni_menu.h>

#include <system/helpers.h>
#include <system/setting_helpers.h>

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
	{LOCALE_CAMD_ITEM_NCAM_NAME,	LOCALE_CAMD_ITEM_NCAM_HINT,	"NCAM",		"ncam",		0, 0},
	{LOCALE_CAMD_ITEM_OSCAM_NAME,	LOCALE_CAMD_ITEM_OSCAM_HINT,	"OSCAM",	"oscam",	0, 0},
	{LOCALE_CAMD_ITEM_OSEMU_NAME,	LOCALE_CAMD_ITEM_OSEMU_HINT,	"OSEMU",	"osemu",	0, 0},
	{LOCALE_CAMD_ITEM_NEWCS_NAME,	LOCALE_CAMD_ITEM_NEWCS_HINT,	"NEWCS",	"newcs",	0, 0},
	{LOCALE_CAMD_ITEM_GBOX_NAME,	LOCALE_CAMD_ITEM_GBOX_HINT,	"GBOX.NET",	"gbox",		0, 0},
	{LOCALE_CAMD_ITEM_CS2GBOX_NAME,	LOCALE_CAMD_ITEM_CS2GBOX_HINT,	"CS2GBOX",	"cs2gbox",	0, 0}
};
#define MAXEMU (sizeof(emu_menu)/sizeof(struct emu_menu_data_t))

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

        if (parent)
                parent->hide();

	if (actionkey == "camd_reset")
	{
		CHintBox hintbox(LOCALE_CAMD_CONTROL, g_Locale->getText(LOCALE_CAMD_MSG_RESET));
		hintbox.paint();

		printf("[ni_menu.cpp] executing \"service emu restart\"\n");
		if (my_system(3, "service", "emu", "restart") != 0)
			printf("[ni_menu.cpp] executing failed\n");
		sleep(1);

		hintbox.hide();

		return menu_return::RETURN_EXIT_ALL;
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

int CNIMenu::show()
{
	int shortcut = 1;
	int cam_shortcut = 1;

	std::ostringstream buf;
	char *buffer;
	ssize_t read;
	size_t len;
	FILE *fh;

	CMenuWidget* ni_menu = new CMenuWidget(LOCALE_NIMENU_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_NI_MENU);
	ni_menu->addIntroItems();

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

			mc = new CMenuOptionChooser(emu_menu[i].name, &emu_menu[i].flag_exist, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, emu_menu[i].cam_exist, new CFlagFileNotifier(emu_menu[i].cam_file), CRCInput::convertDigitToKey(cam_shortcut++));
			mc->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, hint);
			emuMenu->addItem(mc);
		}

	mf = new CMenuForwarder(LOCALE_CAMD_CONTROL, true, NULL, emuMenu, "", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_CAMD);
	ni_menu->addItem(mf);

	// --- Special settings ---
	ni_menu->addItem(new CMenuSeparator(CMenuSeparator::ALIGN_CENTER | CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_NIMENU_HEAD_SPECIAL));

	// NetFS Setup
	CNETFSSetup netfs_setup;
	mf = new CMenuForwarder(LOCALE_NETFS_MENU_MAIN_HEAD, true, NULL, &netfs_setup, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_NETFS_MENU_MAIN_HEAD);
	ni_menu->addItem(mf);

	int res = ni_menu->exec(NULL, "");

	ni_menu->hide();
	delete ni_menu;
	delete emuMenu;

	return res;
}
