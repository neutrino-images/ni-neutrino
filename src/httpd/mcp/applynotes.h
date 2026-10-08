/*
 * applynotes.h - settings an AI client wrote that the box could not put in force
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

#ifndef __httpd_mcp_applynotes_h__
#define __httpd_mcp_applynotes_h__

#include <string>

namespace httpd
{
namespace mcp
{

// Starts listening for failures; once is enough, later calls do nothing.
void watchApplyFailures();

/* What the next tool answer of the connection tells it: the failures of settings it wrote,
   empty for none. Taking it clears it. The line of an event names every key of the applied
   group, so a connection can also read the names (never the values) another writer put in the
   same group. */
std::string takeApplyNote(const std::string &connection);

void forgetApplyNotesForTest();

} // namespace mcp
} // namespace httpd

#endif
