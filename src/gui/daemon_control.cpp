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
#include <mymenu.h>

#include <system/helpers.h>

CDaemonControlMenu::CDaemonControlMenu()
{
	width = 40;
}

CDaemonControlMenu::~CDaemonControlMenu()
{
}

int CDaemonControlMenu::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	if (parent)
		parent->hide();

	return show();
}

typedef struct daemons_data_t
{
	neutrino_locale_t name;
	neutrino_locale_t desc;
	const char * icon;
	const char * flag;
	int daemon_exist; // currently unused
	int flag_exist;
}
daemons_data_struct;

daemons_data_t daemons_data[]=
{
	{LOCALE_DAEMON_ITEM_FCM_NAME,		LOCALE_DAEMON_ITEM_FCM_DESC,		NEUTRINO_ICON_HINT_FCM,		"fritzcallmonitor",	0 ,0},
	{LOCALE_DAEMON_ITEM_NFSSERVER_NAME,	LOCALE_DAEMON_ITEM_NFSSERVER_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"nfsd",			0 ,0},
	{LOCALE_DAEMON_ITEM_SAMBASERVER_NAME,	LOCALE_DAEMON_ITEM_SAMBASERVER_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"samba",		0 ,0},
	{LOCALE_DAEMON_ITEM_TUXCALD_NAME,	LOCALE_DAEMON_ITEM_TUXCALD_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"tuxcald",		0 ,0},
	{LOCALE_DAEMON_ITEM_TUXMAILD_NAME,	LOCALE_DAEMON_ITEM_TUXMAILD_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"tuxmaild",		0 ,0},
	{LOCALE_DAEMON_ITEM_EMMREMIND_NAME,	LOCALE_DAEMON_ITEM_EMMREMIND_DESC,	NEUTRINO_ICON_HINT_EMMRD,	"emmrd",		0 ,0},
	{LOCALE_DAEMON_ITEM_INADYN_NAME,	LOCALE_DAEMON_ITEM_INADYN_DESC,		NEUTRINO_ICON_HINT_IMAGELOGO,	"inadyn",		0 ,0},
	{LOCALE_DAEMON_ITEM_DROPBEAR_NAME,	LOCALE_DAEMON_ITEM_DROPBEAR_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"dropbear",		0 ,0},
	{LOCALE_DAEMON_ITEM_DJMOUNT_NAME,	LOCALE_DAEMON_ITEM_DJMOUNT_DESC,	NEUTRINO_ICON_HINT_IMAGELOGO,	"djmount",		0 ,0},
	{LOCALE_DAEMON_ITEM_USHARE_NAME,	LOCALE_DAEMON_ITEM_USHARE_DESC,		NEUTRINO_ICON_HINT_IMAGELOGO,	"ushare",		0 ,0},
	{LOCALE_DAEMON_ITEM_XUPNPD_NAME,	LOCALE_DAEMON_ITEM_XUPNPD_DESC,		NEUTRINO_ICON_HINT_IMAGELOGO,	"xupnpd",		0 ,0},
	{LOCALE_DAEMON_ITEM_CROND_NAME,		LOCALE_DAEMON_ITEM_CROND_DESC,		NEUTRINO_ICON_HINT_IMAGELOGO,	"crond",		0 ,0}
};
#define DAEMONS_COUNT (sizeof(daemons_data)/sizeof(struct daemons_data_t))

int CDaemonControlMenu::show()
{
	int daemon_shortcut = 0;

	std::ostringstream buf;

	CMenuWidget* daemonControlMenu = new CMenuWidget(LOCALE_DAEMON_CONTROL, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_PLUGINS_HIDE);
	daemonControlMenu->addIntroItems();

	CMenuOptionChooser *mc;
	for (unsigned int i = 0; i < DAEMONS_COUNT; i++)
	{
		buf.str("");
		buf << FLAGDIR << "/." << daemons_data[i].flag;
		const char *flagfile = buf.str().c_str();

		daemons_data[i].flag_exist = file_exists(flagfile);

		CFlagFileNotifier * flagFileNotifier = new CFlagFileNotifier(daemons_data[i].flag);

		mc = new CMenuOptionChooser(daemons_data[i].name, &daemons_data[i].flag_exist, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, flagFileNotifier, CRCInput::convertDigitToKey(daemon_shortcut++));
		mc->setHint(daemons_data[i].icon, daemons_data[i].desc);
		daemonControlMenu->addItem(mc);
	}

	int res = daemonControlMenu->exec(NULL,"");
	daemonControlMenu->hide();
	return res;
}
