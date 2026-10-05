/*
 * toolgen.h - describing a flagged route as a tool
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

#ifndef __httpd_mcp_toolgen_h__
#define __httpd_mcp_toolgen_h__

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/mcp/contract.h"

#include <string>

namespace httpd
{
namespace mcp
{

const Endpoint *flaggedRoute(const RouteTable &t, const ToolFlag &f);

std::string derivedToolName(const Endpoint &ep);
std::string toolName(const Endpoint &ep, const ToolFlag &f);
std::string toolDescription(const Endpoint &ep, const ToolFlag &f);
std::string describedTool(const Endpoint &ep, const ToolFlag &f);
std::string toolTitle(const std::string &name);

void hintsFor(Method m, AuthLevel level, bool &read_only, bool &destructive, bool &idempotent);

ToolDef toolFor(const Endpoint &ep, const ToolFlag &f);

} // namespace mcp
} // namespace httpd

#endif
