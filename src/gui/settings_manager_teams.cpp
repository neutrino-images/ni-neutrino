/*
	cross-team settings manager menue - Neutrino-GUI

	Copyright (C) 2011 'defans'
	Homepage: http://www.bluepeercrew.us/

	Copyright (C) 2017 'vanhofen'
	Homepage: http://www.neutrino-images.de/

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

#include "gui/settings_manager_teams.h"

#include "gui/widget/hintbox.h"
#include "gui/widget/msgbox.h"

#include <driver/screen_max.h>
#include <system/setting_helpers.h>

#include <sys/vfs.h>
#include <unistd.h>

#define TAREXT		"teams"
#define TARCHIVE	"settings." TAREXT

#define SYSCALLBACKUP	"tar -cz -f"
#define SYSCALLRESTORE	"cd /; tar -xz -f"

const char * object[] =
{
	CONFIGDIR "/cables.xml",
	CONFIGDIR "/satellites.xml",
	CONFIGDIR "/timerd.conf",
	CONFIGDIR "/zapit/",
	"/etc/network/interfaces",
	"/etc/wpa_supplicant.conf"
};
const int OBJECT_COUNT = sizeof(object) / sizeof(object[0]);

CSettingsManagerTeams::CSettingsManagerTeams()
{
	width = 40;
}

CSettingsManagerTeams::~CSettingsManagerTeams()
{
}

int CSettingsManagerTeams::exec(CMenuTarget* parent, const std::string &actionKey)
{
	//printf("[neutrino] CSettingsManagerTeams %s: init...\n",__FUNCTION__);

	if(parent)
		parent->hide();

	if(actionKey == "help")
	{
		showHelp();
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "backup")
	{
		doBackup();
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "restore")
	{
		doRestore();
		return menu_return::RETURN_REPAINT;
	}
	int res = showMenu();

	return res;
}

int CSettingsManagerTeams::showMenu()
{
	//printf("[neutrino] CSettingsManagerTeams call %s...\n", __FUNCTION__);

	CMenuWidget * teamset = new CMenuWidget(LOCALE_SETTINGS_TEAMS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_TEAMS_SETTINGS);

	teamset->addIntroItems();

	CMenuForwarder * mf;

	mf = new CMenuForwarder(LOCALE_SETTINGS_TEAMS_HELP, true, NULL, this, "help");
	mf->setHint("", LOCALE_CROSSTEAM_HINT_SETTINGS_TEAMS_HELP);
	teamset->addItem(mf);

	teamset->addItem(GenericMenuSeparatorLine);

	mf =new CMenuForwarder(LOCALE_SETTINGS_TEAMS_BACKUP, true, NULL, this, "backup", CRCInput::RC_red);
	mf->setHint("", LOCALE_CROSSTEAM_HINT_SETTINGS_TEAMS_BACKUP);
	teamset->addItem(mf);

	mf = new CMenuForwarder(LOCALE_SETTINGS_TEAMS_RESTORE, true, NULL, this, "restore", CRCInput::RC_green);
	mf->setHint("", LOCALE_CROSSTEAM_HINT_SETTINGS_TEAMS_RESTORE);
	teamset->addItem(mf);

	int res = teamset->exec(NULL, "");
	teamset->hide();
	delete teamset;

	return res;
}

void CSettingsManagerTeams::showHelp()
{
	ShowMsg(LOCALE_SETTINGS_TEAMS_HELP, LOCALE_SETTINGS_TEAMS_HELP_TEXT, CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO,
			MSGBOX_MIN_WIDTH, NO_TIMEOUT, false, (CMsgBox::AUTO_WIDTH | CMsgBox::AUTO_HIGH));
	return;
}

void CSettingsManagerTeams::doBackup()
{
	int objectCounter = 0;
	std::string objects = " ";
	for(int i=0 ; i < OBJECT_COUNT; i++)
	{
		if ( access((object[i]), F_OK) )
			continue;

		objects += object[i];
		objects += " ";

		objectCounter++;
	}

	fileBrowser.Dir_Mode = true;
	if(fileBrowser.exec("/media") == true)
	{
		std::string TDirectory = fileBrowser.getSelectedFile()->Name.c_str();
		std::string hintBoxContent =  g_Locale->getText(LOCALE_SETTINGS_TEAMS_BACKUP_TO) + TDirectory + "/" + TARCHIVE;
		struct statfs s;
		int ret = ::statfs(TDirectory.c_str(), &s);
		if(objectCounter && ret == 0 && s.f_type != 0x72b6L/* && s.f_type != 0x858458f6L*/) /*jffs2 and ramfs*/
		{
			CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, hintBoxContent.c_str());
			hintBox.paint();
			char buf[256];
			sprintf(buf, SYSCALLBACKUP " %s/" TARCHIVE "%s", TDirectory.c_str(), objects.c_str());
			//printf("CSettingsManagerTeams::doBackup: %s/%s\n", TDirectory.c_str(), TARCHIVE);
			if (system(buf) != 0)
				printf("CSettingsManagerTeams::Backup: failed\n");
			hintBox.hide();
		}
		else
		ShowMsg(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_SETTINGS_TEAMS_BACKUP_FAILED),CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_ERROR);
	}
	return;
}

void CSettingsManagerTeams::doRestore()
{
	fileFilter.addFilter(TAREXT);
	fileBrowser.Filter = &fileFilter;
	fileBrowser.Dir_Mode = false;
	if(fileBrowser.exec("/media") == true)
	{
		std::string TFile = fileBrowser.getSelectedFile()->Name.c_str();
		int result = ShowMsg(LOCALE_SETTINGS_TEAMS_RESTORE, g_Locale->getText(LOCALE_SETTINGS_TEAMS_RESTORE_WARN), CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo);
		if(result == CMsgBox::mbrYes)
		{
			char buf[256];
			sprintf(buf, SYSCALLRESTORE " %s", TFile.c_str());
			//printf("CSettingsManagerTeams::doRestore: %s\n", TFile.c_str());
			if (system(buf) != 0)
				printf("CSettingsManagerTeams::doRestore: failed\n");
			else
				doReboot();
		}
	}
	return;
}

void CSettingsManagerTeams::doReboot()
{
	CNeutrinoApp::getInstance()->exec(NULL, "reboot");
	return;
}
