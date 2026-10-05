/*
 * wiring.h - what the MCP endpoint is connected to
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

#ifndef __httpd_mcp_wiring_h__
#define __httpd_mcp_wiring_h__

#include "httpd/mcp/contract.h"

namespace httpd
{
namespace mcp
{

struct Wiring
{
	ToolSource  *tools;
	VerifyToken  verify;
};

// Before httpd::start; the tool source outlives the server.
void install(const Wiring &w);
void uninstall();

// False until every member is set; out is left alone then.
bool installed(Wiring *out);

/* Uninstalls, refuses every new call until the next install and waits up to bound_ms
   for the running ones; false when one is still running then. */
bool drain(unsigned bound_ms);

} // namespace mcp
} // namespace httpd

#endif
