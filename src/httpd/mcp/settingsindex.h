/*
 * settingsindex.h - settings_schema without a section: the sections for one connection
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

#ifndef __httpd_mcp_settingsindex_h__
#define __httpd_mcp_settingsindex_h__

#include "httpd/endpoint.h"
#include "httpd/mcp/contract.h"

#include "coreapi/base/result.h"

#include <string>

namespace httpd
{
namespace mcp
{

bool isSettingsSchemaRoute(const Endpoint &ep);

// Neither section nor keys given.
bool asksSettingsIndex(const JsonText &args);

// The route's answer shape with the index beside items, so either answer fits it.
std::string withSettingsIndex(const std::string &output);

/* {"sections": [{name, label, count, readable, writable}]}; can_read and can_write say
   whether the connection is offered read_settings and write_settings. */
coreapi::Result<JsonText> settingsIndex(bool can_read, bool can_write);

} // namespace mcp
} // namespace httpd

#endif
