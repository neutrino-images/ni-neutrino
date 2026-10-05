/*
 * composed.h - tools composed on the coreAPI rather than one route each
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

#ifndef __httpd_mcp_composed_h__
#define __httpd_mcp_composed_h__

#include "httpd/endpoint.h"

#include <time.h>

namespace httpd
{
namespace mcp
{

// Routes of a table no server answers from.
const RouteTable &composedTable();

time_t toolNow();
// Nought puts the real clock back.
void setToolClockForTest(time_t now);

} // namespace mcp
} // namespace httpd

#endif
