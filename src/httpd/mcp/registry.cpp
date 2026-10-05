/*
 * registry.cpp - the tools this box offers, built from its own route tables
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

#include "httpd/mcp/contract.h"

#include "httpd/router.h"
#include "httpd/mcp/composed.h"
#include "httpd/mcp/routetools.h"

#include <cstddef>
#include <string>

namespace httpd
{
namespace mcp
{

namespace
{

// Built on first use, so nothing runs before main.
RouteTools &tools()
{
	size_t count = 0;
	const RouteTable *const *tables = allRoutes(&count);
	static RouteTools built(tables, count, composedTable());
	return built;
}

} // namespace

ToolSource &boxTools()
{
	return tools();
}

const std::string &boxToolsRefusal()
{
	return tools().refusal();
}

} // namespace mcp
} // namespace httpd
