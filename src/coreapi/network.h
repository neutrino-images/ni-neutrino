/*
 * network.h - what the box knows of its network interfaces
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

#ifndef __coreapi_network_h__
#define __coreapi_network_h__

#include "coreapi/base/schema.h"

#include <string>
#include <vector>

namespace coreapi
{
namespace network
{

/* The interfaces the box has, by name in alphabetical order, as the system lists
   them. The loopback and hidden entries are not interfaces anybody selects, so
   they are left out. Empty when the box lists none or cannot be read. */
std::vector<std::string> interfaces();

// The same for a directory other than the system's, for a case that needs its own.
std::vector<std::string> interfacesIn(const std::string &directory);

/* The interface setting's choices. The name is the label; the value is the
   position in the list, since a choice carries no text of its own yet. False
   when there are none. */
bool interfaceChoices(std::vector<SettingChoice> &out);

} // namespace network
} // namespace coreapi

#endif
