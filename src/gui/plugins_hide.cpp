/*
	plugins_hide

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

#include <gui/plugins_hide.h>

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <neutrino_menue.h>

#include <driver/screen_max.h>

#include "gui/pluginlist.h"
#include "gui/plugins.h"

extern CPlugins *g_Plugins;

const CMenuOptionChooser::keyval PLUGIN_HIDE_OPTIONS[] =
{
	{ 0, LOCALE_PLUGIN_HIDE_NO },
	{ 1, LOCALE_PLUGIN_HIDE_YES }
};
#define PLUGIN_HIDE_OPTIONS_COUNT (sizeof(PLUGIN_HIDE_OPTIONS)/sizeof(CMenuOptionChooser::keyval))

CPluginsHideMenu::CPluginsHideMenu()
{
	width = 40;
}

CPluginsHideMenu::~CPluginsHideMenu()
{
}

int CPluginsHideMenu::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	if (parent)
		parent->hide();

	return menu();
}

int CPluginsHideMenu::menu()
{
	CMenuWidget* pluginsHideMenu = new CMenuWidget(LOCALE_PLUGINS_HIDE, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_PLUGINS_HIDE);
	pluginsHideMenu->addIntroItems();

	unsigned int maxPlugins = g_Plugins->getNumberOfPlugins();
	int isHidden[maxPlugins];
	std::string file[maxPlugins];
	CPluginsHideNotifier * pluginsNotifier[maxPlugins];
	for (unsigned int i = 0; i < maxPlugins; i++)
	{
		std::string tmp = g_Plugins->getName(i);
		file[i] = g_Plugins->getCfgFile(i);
		isHidden[i] = g_Plugins->isHidden(i);

		pluginsNotifier[i] = new CPluginsHideNotifier(file[i].c_str());
		CMenuOptionChooser * mc = new CMenuOptionChooser(tmp.c_str(), &isHidden[i], PLUGIN_HIDE_OPTIONS, PLUGIN_HIDE_OPTIONS_COUNT, true, pluginsNotifier[i]);
		mc->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_PLUGINS_HIDE);
		pluginsHideMenu->addItem(mc);
	}

	int res = pluginsHideMenu->exec(NULL,"");
	pluginsHideMenu->hide();
	for (unsigned int i = 0; i < maxPlugins; i++)
	{
		if(pluginsNotifier[i])
			delete pluginsNotifier[i];
	}
	delete pluginsHideMenu;
	return res;
}

bool CPluginsHideNotifier::changeNotify(const neutrino_locale_t, void * data)
{
	std::fstream f;
	std::ostringstream os;
	std::string line;
	std::string cfg;
	bool exist = false;

	printf("CPluginsHideNotifier::changeNotify %s\n",filename);

	os << "hide=" << (((*(int *)data) != 0) ? 1 : 0) << std::endl;
	std::string hide = os.str();

	f.open(filename, std::ios::in);
	while (!f.eof())
	{
		getline(f, line);
		if (line.substr(0,1) == "")
			continue;

		if (line.substr(0,4) == "hide")
		{
			cfg += (std::string) hide;
			exist = true;
		}
		else
		{
			cfg += line;
			cfg += "\n";
		}
	}
	f.close();

	if (!exist)
		cfg += hide;

	//printf("%s",cfg.c_str());

	f.open(filename, std::ios::out);
	f << cfg << std::endl;
	f.close();

	g_Plugins->loadPlugins();

	return menu_return::RETURN_REPAINT;
}
