/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	progressbar_setup menu
	Suggested by tomworld

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
	Foundation, Inc., 51 Franklin Street, Fifth Floor, 
	Boston, MA  02110-1301, USA.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "osd_progressbar_setup.h"

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <gui/widget/menue_options.h>
#include <gui/widget/settingitem.h>

#include <coreapi/settings/settings.h>
#include <driver/screen_max.h>

#include <system/debug.h>

CProgressbarSetup::CProgressbarSetup()
{
	width = 40;
}

CProgressbarSetup::~CProgressbarSetup()
{

}

bool CProgressbarSetup::changeNotify(const neutrino_locale_t /* OptionName */, void * /* data */)
{
	return true; // repaint
}

int CProgressbarSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	printf("[neutrino] init progressbar menu setup...\n");

	if (actionKey == "reset") {
		// The defaults are the rows' own, written like any other change.
		std::vector<std::string> keys;
		keys.push_back("progressbar_timescale_red");
		keys.push_back("progressbar_timescale_green");
		keys.push_back("progressbar_timescale_yellow");
		keys.push_back("progressbar_timescale_invert");
		coreapi::settings::Refusals refused;
		coreapi::settings::resetDefaults(keys, refused, true);
		return menu_return::RETURN_REPAINT;
	}

	if (parent)
		parent->hide();

	return showMenu();
}

int CProgressbarSetup::showMenu()
{
	CMenuWidget m(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_PROGRESSBAR);

	m.addIntroItems(LOCALE_MISCSETTINGS_PROGRESSBAR, LOCALE_MISCSETTINGS_PROGRESSBAR_GLOBAL);

	// general progress bar design
	addChoiceSetting(&m, "progressbar_design", true, this);

	// progress bar gradient
	addChoiceSetting(&m, "progressbar_gradient", true, this);

	// preview
	CMenuProgressbar *mb = new CMenuProgressbar(LOCALE_MISCSETTINGS_PROGRESSBAR_PREVIEW);
	mb->setHint("", LOCALE_MENU_HINT_PROGRESSBAR_PREVIEW);
	m.addItem(mb);
	m.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MISCSETTINGS_PROGRESSBAR_TIMESCALE));

	addNumberSetting(&m, "progressbar_timescale_red", true, this, CRCInput::RC_nokey, false, true);
	addNumberSetting(&m, "progressbar_timescale_yellow", true, this, CRCInput::RC_nokey, false, true);
	addNumberSetting(&m, "progressbar_timescale_green", true, this, CRCInput::RC_nokey, false, true);
	addChoiceSetting(&m, "progressbar_timescale_invert", true, this);

	mb = new CMenuProgressbar(LOCALE_MISCSETTINGS_PROGRESSBAR_PREVIEW);
	mb->setHint("", LOCALE_MENU_HINT_PROGRESSBAR_PREVIEW);
	mb->getScale()->setType(CProgressBar::PB_TIMESCALE);
	m.addItem(mb);

	CMenuForwarder* mf = new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, "reset", CRCInput::RC_red);
	mf->setHint("", LOCALE_OPTIONS_HINT_DEFAULT);
	m.addItem(mf);

	// extended channel list (progressbars)
	m.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MAINMENU_CHANNELS));

	addChoiceSetting(&m, "progressbar_design_channellist", true, this);

	mb = new CMenuProgressbar(LOCALE_MISCSETTINGS_PROGRESSBAR_PREVIEW);
	mb->setHint("", LOCALE_MENU_HINT_PROGRESSBAR_PREVIEW);
	mb->getScale()->setType(CProgressBar::PB_TIMESCALE);
	mb->getScale()->setDesign(g_settings.theme.progressbar_design_channellist);
	mb->getScale()->doPaintBg(false);
	m.addItem(mb);

	return m.exec(NULL, "");
}
