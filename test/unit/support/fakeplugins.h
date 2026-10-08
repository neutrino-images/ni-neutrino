/*
 * fakeplugins.h - the plugin group's plugin list as a case sees it
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

#ifndef __support_fakeplugins_h__
#define __support_fakeplugins_h__

#include "coreapi/box/apply_plugins.h"

// Counts the rereads, which is all the group asks of the list.
struct FakePluginLoader : public coreapi::PluginLoader
{
	int reloads;
	coreapi::Status answer;

	FakePluginLoader() : reloads(0), answer(coreapi::Status::Ok) {}

	coreapi::Status reload()
	{
		++reloads;
		return answer;
	}
};

#endif
