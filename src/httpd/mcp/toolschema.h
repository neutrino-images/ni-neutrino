/*
 * toolschema.h - JSON Schema for the arguments and answers of a tool
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

#ifndef __httpd_mcp_toolschema_h__
#define __httpd_mcp_toolschema_h__

#include "httpd/endpoint.h"
#include "httpd/schema.h"

#include <string>

namespace httpd
{
namespace mcp
{

void appendInputSchema(std::string &out, const Endpoint &ep);
void appendOutputSchema(std::string &out, const Schema &s);

// The answer of a route that answers no document.
void appendDoneSchema(std::string &out);
// The same, narrowed to the words these success codes produce.
void appendDoneSchema(std::string &out, unsigned answers);

} // namespace mcp
} // namespace httpd

#endif
