/*
 * mode43.h - whether a 4:3 mode was set past the video group
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

#ifndef __coreapi_mode43_h__
#define __coreapi_mode43_h__

namespace coreapi
{

/* The video group sends the setting's own 4:3 mode to the channel daemon, so a
   mode that is not the setting's was set by someone else, past the group, and
   the group has to send its own over again. */
inline bool mode43PastGroup(int sent, int setting)
{
	return sent != setting;
}

} // namespace coreapi

#endif
