/*
 * limits.h - how much the MCP endpoint takes
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

#ifndef __httpd_mcp_limits_h__
#define __httpd_mcp_limits_h__

#include <cstddef>

namespace httpd
{
namespace mcp
{

struct Limits
{
	size_t   max_body_bytes;
	size_t   max_json_depth;
	unsigned rate_burst;
	unsigned rate_per_minute;     // nought turns the rate limit off
	size_t   max_rate_clients;
	unsigned call_timeout_ms;
	unsigned max_running_calls;   // abandoned calls included
};

Limits defaultLimits();

// Read by the server's threads; set before httpd::start.
void setLimits(const Limits &l);
Limits limits();

} // namespace mcp
} // namespace httpd

#endif
