/*
	$port: parentallock_setup.cpp,v 1.4 2010/10/01 15:59:10 tuxbox-cvs Exp $

	parentallock setup implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2009 T. Graf 'dbt'
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
	along with this program; if not, write to the 
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "parentallock_setup.h"

#include <gui/widget/icons.h>
#include <gui/widget/settingitem.h>
#include <gui/widget/stringinput.h>


#include <system/debug.h>


//constructor is definied in parentallock_setup.h

CParentalSetup::~CParentalSetup()
{

}

int CParentalSetup::exec(CMenuTarget* parent, const std::string &/*actionKey*/)
{
	dprintf(DEBUG_DEBUG, "init parental setup\n");
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	if (check())
		res = showParentalSetup();

	return res;
}


int CParentalSetup::showParentalSetup()
{
	//menue init
	CMenuWidget* plock = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_LOCK, width, MN_WIDGET_ID_PLOCKSETUP);

	// intros with sub head
	plock->addIntroItems(LOCALE_PARENTALLOCK_PARENTALLOCK);

	CMenuForwarder * mf;

	addSetting(plock, "parentallock_prompt");
	addSetting(plock, "parentallock_lockage");
	addSetting(plock, "parentallock_defaultlocked");
	addSetting(plock, "parentallock_zaptime");

	CPINChangeWidget pinChangeWidget(LOCALE_PARENTALLOCK_CHANGEPIN, &g_settings.parentallock_pincode, 4, LOCALE_PARENTALLOCK_CHANGEPIN_HINT1);
	mf = new CMenuForwarder(LOCALE_PARENTALLOCK_CHANGEPIN, true, g_settings.parentallock_pincode, &pinChangeWidget);
	mf->setHint("", LOCALE_MENU_HINT_PARENTALLOCK_CHANGEPIN);
	plock->addItem(mf);

	int res = plock->exec(NULL, "");
	delete plock;
	return res;
}
