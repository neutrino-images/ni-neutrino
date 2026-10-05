/*
 * toolguard.h - what may never become a tool, and what a tool must say
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

#ifndef __httpd_mcp_toolguard_h__
#define __httpd_mcp_toolguard_h__

#include "httpd/endpoint.h"
#include "httpd/http.h"

#include <cstddef>
#include <string>

namespace httpd
{
namespace mcp
{

const size_t kMaxComposedTools = 8;

struct NeverExposed
{
	Method      method;
	const char *path;
};

const NeverExposed *neverExposedRoutes(size_t *count);
bool neverExposed(Method m, const char *path);

// Offered only through the owner's allowlists.
const NeverExposed *gatedRoutes(size_t *count);
bool gated(Method m, const char *path);

// why names the first rule broken.
bool toolsAreSane(const RouteTable *const *tables, size_t table_count,
                  const RouteTable &composed, std::string *why = NULL);

} // namespace mcp
} // namespace httpd

#endif
