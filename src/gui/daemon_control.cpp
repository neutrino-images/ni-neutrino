/*
	daemon_control

	(C) 2017 NI-Team

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sstream>

#include <gui/daemon_control.h>

#include <global.h>
#include <neutrino.h>

#include <gui/widget/menue_options.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/settingitem.h>

#include <system/helpers.h>

#include <coreapi/box/applyworker.h>

CDaemonControlMenu::CDaemonControlMenu()
{
	width = 40;
}

CDaemonControlMenu::~CDaemonControlMenu()
{
}

int CDaemonControlMenu::exec(CMenuTarget *parent, const std::string & /*actionKey*/)
{
	if (parent)
		parent->hide();

	return show();
}

typedef struct daemons_data_t
{
	neutrino_locale_t name;
	neutrino_locale_t desc;
	const char *icon;
	const char *daemon;
	int daemon_exist;
	const char *flag;
	int flag_exist;
}
daemons_data_struct;

daemons_data_t daemons_data[] =
{
	{LOCALE_DAEMON_ITEM_FCM_NAME,		LOCALE_DAEMON_ITEM_FCM_DESC,		NEUTRINO_ICON_HINT_FCM,		"FritzCallMonitor",	0, "fritzcallmonitor",	0},
	{LOCALE_DAEMON_ITEM_NFSSERVER_NAME,	LOCALE_DAEMON_ITEM_NFSSERVER_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"rpc.nfsd",		0, "nfsd",		0},
	{LOCALE_DAEMON_ITEM_SAMBASERVER_NAME,	LOCALE_DAEMON_ITEM_SAMBASERVER_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"smbd",			0, "samba",		0},
	{LOCALE_DAEMON_ITEM_TUXCALD_NAME,	LOCALE_DAEMON_ITEM_TUXCALD_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"tuxcald",		0, "tuxcald",		0},
	{LOCALE_DAEMON_ITEM_TUXMAILD_NAME,	LOCALE_DAEMON_ITEM_TUXMAILD_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"tuxmaild",		0, "tuxmaild",		0},
	{LOCALE_DAEMON_ITEM_EMMREMIND_NAME,	LOCALE_DAEMON_ITEM_EMMREMIND_DESC,	NEUTRINO_ICON_HINT_EMMRD,	"emmrd",		0, "emmrd",		0},
	{LOCALE_DAEMON_ITEM_INADYN_NAME,	LOCALE_DAEMON_ITEM_INADYN_DESC,		NEUTRINO_ICON_HINT_IMAGELOGO,	"inadyn",		0, "inadyn",		0},
	{LOCALE_DAEMON_ITEM_DROPBEAR_NAME,	LOCALE_DAEMON_ITEM_DROPBEAR_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"dropbear",		0, "dropbear",		0},
	{LOCALE_DAEMON_ITEM_DJMOUNT_NAME,	LOCALE_DAEMON_ITEM_DJMOUNT_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"djmount",		0, "djmount",		0},
	{LOCALE_DAEMON_ITEM_USHARE_NAME,	LOCALE_DAEMON_ITEM_USHARE_DESC,		NEUTRINO_ICON_HINT_IMAGELOGO,	"ushare",		0, "ushare",		0},
	{LOCALE_DAEMON_ITEM_MINIDLNA_NAME,	LOCALE_DAEMON_ITEM_MINIDLNA_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"minidlnad",		0, "minidlnad",		0},
	{LOCALE_DAEMON_ITEM_XUPNPD_NAME,	LOCALE_DAEMON_ITEM_XUPNPD_DESC,		NEUTRINO_ICON_HINT_IMAGELOGO,	"xupnpd",		0, "xupnpd",		0},
	{LOCALE_DAEMON_ITEM_CROND_NAME,		LOCALE_DAEMON_ITEM_CROND_DESC,		NEUTRINO_ICON_HINT_IMAGELOGO,	"crond",		0, "crond",		0}
};
#define DAEMONS_COUNT (sizeof(daemons_data)/sizeof(struct daemons_data_t))

int CDaemonControlMenu::show()
{
	int daemon_shortcut = 0;

	CMenuWidget *daemonControlMenu = new CMenuWidget(LOCALE_DAEMON_CONTROL, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_PLUGINS_HIDE);
	daemonControlMenu->addIntroItems();

	for (unsigned int i = 0; i < DAEMONS_COUNT; i++)
	{
		std::string flagfile = FLAGDIR;
		flagfile += "/.";
		flagfile += daemons_data[i].flag;

		daemons_data[i].flag_exist = file_exists(flagfile.c_str());
		daemons_data[i].daemon_exist = !find_executable(daemons_data[i].daemon).empty();

		// A flag left behind by a program that is gone would start nothing and show on.
		if (!daemons_data[i].daemon_exist && daemons_data[i].flag_exist)
		{
			remove(flagfile.c_str());
			daemons_data[i].flag_exist = 0;
		}

		std::string key = "flag_daemon_";
		key += daemons_data[i].flag;
		CMenuItem *mc = addSetting(daemonControlMenu, key.c_str(), true, NULL, CRCInput::convertDigitToKey(daemon_shortcut));
		if (mc == NULL)
			continue;
		daemon_shortcut++;
		mc->setHint(daemons_data[i].icon, daemons_data[i].desc);
	}

	int res = daemonControlMenu->exec(NULL, "");
	daemonControlMenu->hide();
	delete daemonControlMenu;
	return res;
}

// ----------------------------------------------------------------------------

CCamdControlMenu::CCamdControlMenu()
{
	width = 40;
}

CCamdControlMenu::~CCamdControlMenu()
{
}

int CCamdControlMenu::exec(CMenuTarget *parent, const std::string & /*actionKey*/)
{
	if (parent)
		parent->hide();

	return show();
}

typedef struct camds_data_t
{
	neutrino_locale_t name;
	neutrino_locale_t desc;
	const char *camd_name;
	const char *camd_file;
	int camd_exist;
}
camds_data_struct;

camds_data_t camds_data[] =
{
	{LOCALE_CAMD_ITEM_MGCAMD_NAME,	LOCALE_CAMD_ITEM_MGCAMD_HINT,	"MGCAMD",	"mgcamd",	0},
	{LOCALE_CAMD_ITEM_DOSCAM_NAME,	LOCALE_CAMD_ITEM_DOSCAM_HINT,	"DOSCAM",	"doscam",	0},
	{LOCALE_CAMD_ITEM_NCAM_NAME,	LOCALE_CAMD_ITEM_NCAM_HINT,	"NCAM",		"ncam",		0},
	{LOCALE_CAMD_ITEM_OSMOD_NAME,	LOCALE_CAMD_ITEM_OSMOD_HINT,	"OSMOD",	"osmod",	0},
	{LOCALE_CAMD_ITEM_OSCAM_NAME,	LOCALE_CAMD_ITEM_OSCAM_HINT,	"OSCAM",	"oscam",	0},
	{LOCALE_CAMD_ITEM_CCCAM_NAME,	LOCALE_CAMD_ITEM_CCCAM_HINT,	"CCCAM",	"cccam",	0},
	{LOCALE_CAMD_ITEM_GBOX_NAME,	LOCALE_CAMD_ITEM_GBOX_HINT,	"GBOX.NET",	"gbox",		0}
};
#define CAMDS_COUNT (sizeof(camds_data)/sizeof(struct camds_data_t))

/* The message that stays up while a softcam is started or stopped. The observer paints it
   before the services group runs and the item's after-apply hides it again once the
   worker has run the script: the user asked for it here and sees it done. */
static CHintBox *camd_message_box = NULL;

static bool hideCamdMessage()
{
	if (camd_message_box == NULL)
		return false;
	camd_message_box->hide();
	delete camd_message_box;
	camd_message_box = NULL;
	return false;
}

class CCamdMessage : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t, void *data)
		{
			hideCamdMessage();
			const bool on = data != NULL && *(int *) data != 0;
			camd_message_box = new CHintBox(LOCALE_CAMD_CONTROL, g_Locale->getText(on ? LOCALE_CAMD_MSG_START : LOCALE_CAMD_MSG_STOP));
			camd_message_box->paint();
			return false;
		}
};

int CCamdControlMenu::show()
{
	int camd_shortcut = 0;

	char *buffer;
	ssize_t read;
	size_t len;
	FILE *fh;

	CMenuWidget *camdControlMenu = new CMenuWidget(LOCALE_CAMD_CONTROL, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_CAMD_CONTROL);
	camdControlMenu->addIntroItems();

	// camd reset
	CMenuForwarder *mf = new CMenuForwarder(LOCALE_CAMD_RESET, true, NULL, CNeutrinoApp::getInstance(), "camd_reset", CRCInput::RC_red);
	mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_CAMD_RESET);
	camdControlMenu->addItem(mf);

	camdControlMenu->addItem(GenericMenuSeparatorLine);

	CCamdMessage camd_message;
	for (unsigned int i = 0; i < CAMDS_COUNT; i++)
	{
		std::string vinfo = "";
		std::string camd_binary = "/var/bin/";
		camd_binary += camds_data[i].camd_file;

		camds_data[i].camd_exist = file_exists(camd_binary.c_str());

		if (camds_data[i].camd_exist)
		{
			std::string vinfo_call = "vinfo ";
			vinfo_call += camds_data[i].camd_name;
			vinfo_call += " /var/bin/";
			vinfo_call += camds_data[i].camd_file;

			buffer = NULL;
			if ((fh = popen(vinfo_call.c_str(), "r")))
			{
				while ((read = getline(&buffer, &len, fh)) != -1)
					vinfo += buffer;
				pclose(fh);
				if (buffer)
					free(buffer);
			}
			else
				printf("[vinfo] popen error\n");
		}

		// remove linebreaks from vinfo output
		std::string::size_type spos = vinfo.find_first_of("\r\n");
		while (spos != std::string::npos)
		{
			vinfo.replace(spos, 1, " ");
			spos = vinfo.find_first_of("\r\n");
		}
		std::string hint(g_Locale->getText(camds_data[i].desc));
		hint.append("\nvinfo: " + vinfo);

		std::string key = "flag_camd_";
		key += camds_data[i].camd_file;
		CMenuOptionChooser *mc = addChoiceSetting(camdControlMenu, key.c_str(), true, &camd_message, CRCInput::convertDigitToKey(camd_shortcut));
		if (mc == NULL)
			continue;
		camd_shortcut++;
		afterApply(mc, []()
		{
			coreapi::applyWorker().waitFor("softcam.");
			return hideCamdMessage();
		});
		mc->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, hint);
	}

	int res = camdControlMenu->exec(NULL, "");
	camdControlMenu->hide();
	delete camdControlMenu;
	return res;
}
