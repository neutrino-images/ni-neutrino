/*
 * ratelimit.h - requests per client of the MCP endpoint
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

#ifndef __httpd_mcp_ratelimit_h__
#define __httpd_mcp_ratelimit_h__

#include <cstddef>
#include <string>

#include <time.h>

namespace httpd
{
namespace mcp
{

// rate_burst at once, refilled at rate_per_minute; retry_after gets the seconds to wait, 0 on a pass.
bool rateAllows(const std::string &client_id, unsigned *retry_after);

// Nought puts the real clock back.
void setRateClockForTest(time_t now);
void forgetRatesForTest();
size_t rateClientsForTest();

} // namespace mcp
} // namespace httpd

#endif
