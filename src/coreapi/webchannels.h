/*
 * webchannels.h - the web channel lists the box loads from its own folders
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

#ifndef __coreapi_webchannels_h__
#define __coreapi_webchannels_h__

#include "coreapi/base/schema.h"

#include <string>
#include <vector>

namespace coreapi
{
namespace webchannels
{

// The folders autoloading reads for one kind, the writable one first.
std::vector<std::string> autoloadDirs(bool radio);

/* What autoloading takes from dirs: each non-empty list file, per folder in the
   alphabet. A folder reached twice is read once, and a name already in listed or
   found before is left out, since the channel daemon loads one list once. */
std::vector<std::string> autoloadIn(const std::vector<std::string> &dirs,
				    const std::vector<std::string> &listed);

/* What the channel daemon autoloads for one kind beside the user's list, empty
   while autoloading is off for it. */
std::vector<std::string> autoloaded(bool radio);

/* The two lists as the choices of the list rows, each file as text and label.
   False when there are none. */
bool webtvAutoloaded(std::vector<SettingChoice> &out);
bool webradioAutoloaded(std::vector<SettingChoice> &out);

} // namespace webchannels
} // namespace coreapi

#endif
