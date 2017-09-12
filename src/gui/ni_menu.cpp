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

#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>

#include <gui/ni_menu.h>

#include <system/helpers.h>
#include <system/setting_helpers.h>

#include <driver/screen_max.h>

CNIMenu::CNIMenu()
{
	width = 40;
}

CNIMenu::~CNIMenu()
{
}

int CNIMenu::exec(CMenuTarget* parent, const std::string &actionkey)
{
	printf("CNIMenu::exec: actionkey %s\n", actionkey.c_str());
	int res = menu_return::RETURN_REPAINT;

        if (parent)
                parent->hide();

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
	CMenuWidget* ni_menu = new CMenuWidget(LOCALE_NIMENU_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_NI_MENU);
	ni_menu->addIntroItems();

	int res = ni_menu->exec(NULL, "");

	ni_menu->hide();
	delete ni_menu;

	return res;
}
