/*
 * apply_plugins.cpp - what makes a changed plugin setting take effect
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>

#include "coreapi/box/apply_plugins.h"

namespace coreapi
{

namespace
{

class NoPluginLoader : public PluginLoader
{
	public:
		Status reload() { return Status::NotSupported; }
};

NoPluginLoader g_no_plugin_loader;
PluginLoader *g_plugin_loader = 0;

/* The list is read from the folders and the settings every time, so a run owes
   nothing to the last one and a sibling key re-reading it is harmless: a read
   of a few directories, no daemon and no screen. */
Status runPlugins()
{
	return reloadPlugins();
}

const char *const kPluginsKeys[] =
{
	"plugin_hdd_dir",
	"plugins_disabled",
	"plugins_game",
	"plugins_lua",
	"plugins_script",
	"plugins_tool"
};

} // namespace

PluginLoader &pluginLoader()
{
	if (!g_plugin_loader)
		return g_no_plugin_loader;
	return *g_plugin_loader;
}

void setPluginLoader(PluginLoader *l) { g_plugin_loader = l; }

Status reloadPlugins()
{
	return pluginLoader().reload();
}

/* The list exists once the program has made it and mounted what it searches,
   which is the last point of startup the groups have. */
const ApplyGroup kPluginsApplyGroup = { "plugins", ApplyPhase::Network, COREAPI_KEYS(kPluginsKeys), &runPlugins };

} // namespace coreapi
