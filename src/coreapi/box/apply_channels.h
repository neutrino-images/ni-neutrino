/*
 * apply_channels.h - what makes the channel lists follow the settings they are built from
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

#ifndef __coreapi_apply_channels_h__
#define __coreapi_apply_channels_h__

#include "coreapi/base/apply.h"

namespace coreapi
{

/* The one group that rebuilds the box's channel lists when a setting they are
   built from changed: which extra lists exist and whether empty favourites are
   listed. The lists are built from these settings in one place, so the group
   asks the program to build them again and does not build anything itself.

   Shared by every area whose settings decide what the lists hold. Such a setting
   is added to kChannelReloadKeys in apply_channels.cpp and to ChannelShape there,
   which is what a run compares; a key that is in no shape never causes a rebuild.
   The group is idempotent: a run whose shape equals the one the lists were last
   built from asks for nothing, so a hotkey, a sibling key or a second writer of
   the same value costs nothing.

   Startup records the shape and asks for nothing, since the first build of the
   lists is made from the loaded settings later in startup. */
extern const ApplyGroup kChannelReloadApplyGroup;

// Forgets the shape the lists were built from, for a case that needs the first run again.
void resetChannelReload();

} // namespace coreapi

#endif
