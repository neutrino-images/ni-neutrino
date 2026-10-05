/*
 * headers.h - header rules of the MCP endpoint
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

#ifndef __httpd_mcp_headers_h__
#define __httpd_mcp_headers_h__

#include "httpd/endpoint.h"

#include <string>

namespace httpd
{
namespace mcp
{

// Visible ASCII as it stands, or =?base64?...?= decoded to UTF-8; false for anything else.
bool decodeMirrored(const std::string &header, std::string &out);

bool isJsonMediaType(const std::string &content_type);

// Lower case without the default port, so an Origin and a URL compare as text; empty if not http(s).
std::string originOf(const std::string &url);

// RFC 6750; each part is left out when empty or NULL.
std::string bearerChallenge(const std::string &metadata_url, const char *error,
                            const std::string &scope);

const char *scopeFor(AuthLevel level);

} // namespace mcp
} // namespace httpd

#endif
