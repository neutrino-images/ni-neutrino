/*
	Neutrino-GUI  -   DBoxII-Project

	Update settings  implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2012 T. Graf 'dbt'
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

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>
#include <gui/update_check.h>
#if ENABLE_PKG_MANAGEMENT
#include <gui/opkg_manager.h>
#include <gui/update_check_packages.h>
#endif
#include <gui/update_ext.h>
#include <gui/update_settings.h>
#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/settingitem.h>
#include <driver/screen_max.h>
#include <system/debug.h>

#include <coreapi/box/apply_update.h>
#include <coreapi/settings/menuspec.h>

CUpdateSettings::CUpdateSettings()
{
	width = 40;
}

CUpdateSettings::~CUpdateSettings()
{
}

int CUpdateSettings::exec(CMenuTarget *parent, const std::string &/*actionKey*/)
{
	dprintf(DEBUG_DEBUG, "init software-update settings\n");

	if (parent)
		parent->hide();

	return initMenu();
}

// init options for software update
int CUpdateSettings::initMenu()
{
	CMenuWidget w_upsettings(LOCALE_SERVICEMENU_UPDATE, NEUTRINO_ICON_UPDATE, width, MN_WIDGET_ID_SOFTWAREUPDATE_SETTINGS);
	w_upsettings.addIntroItems(LOCALE_FLASHUPDATE_SETTINGS);

	addSetting(&w_upsettings, "update_dir", true, NULL, CRCInput::RC_red);
	addSetting(&w_upsettings, "softupdate_url_file", true, NULL, CRCInput::RC_green);
#if ENABLE_EXTUPDATE
	addSetting(&w_upsettings, "softupdate_name_mode_backup");
	// Only where the box offers the settings to be applied, so the line is not left alone.
	if (coreapi::menuItem("apply_settings").ok())
		w_upsettings.addItem(GenericMenuSeparatorLine);
	addSetting(&w_upsettings, "apply_settings");
	addSetting(&w_upsettings, "softupdate_name_mode_apply");
#endif
	addSetting(&w_upsettings, "softupdate_autocheck");
#if ENABLE_PKG_MANAGEMENT
	if (COPKGManager::hasOpkgSupport())
		addSetting(&w_upsettings, "softupdate_autocheck_packages");
#endif

	return w_upsettings.exec(NULL, "");
}

coreapi::Status coreapi::applicationPackageCheck(bool on)
{
#if ENABLE_PKG_MANAGEMENT
	CUpdateCheckPackages::getInstance()->stopTimer();
	if (on)
		CUpdateCheckPackages::getInstance()->startThread();
	return coreapi::Status::Ok;
#else
	(void) on;
	return coreapi::Status::NotSupported;
#endif
}
