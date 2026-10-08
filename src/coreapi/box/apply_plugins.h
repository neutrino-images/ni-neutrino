/*
 * apply_plugins.h - what makes a changed plugin setting take effect
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

#ifndef __coreapi_apply_plugins_h__
#define __coreapi_apply_plugins_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

namespace coreapi
{

/* The list of installed plugins the program keeps, read again from the folders
   and from the settings that name a folder or a plugin's type. A seam so the
   group runs where no plugin list exists. */
struct PluginLoader
{
	virtual ~PluginLoader() {}

	virtual Status reload() = 0;
};

// NotSupported while nothing is installed, so a run before the list exists says so.
PluginLoader &pluginLoader();
void setPluginLoader(PluginLoader *l);

// Binds the accessor above to the program's plugin list.
void installRealPluginLoader();

/* The one way anything but the group asks for the list to be read again, for a
   change that is not a setting: the hide flag a plugin keeps in its own file. */
Status reloadPlugins();

/* Defined by the application, because the list is an object whose header reaches
   the GUI and this layer must not: reads the list again. Nothing to do, and Ok,
   while the program has not made it yet. */
Status applicationReloadPlugins();

/* The folder on the disk that is searched for plugins and the five lists that
   say what type a plugin is. */
extern const ApplyGroup kPluginsApplyGroup;

} // namespace coreapi

#endif
