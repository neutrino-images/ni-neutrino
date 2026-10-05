/*
 * metadata.h - what the OAuth server and the mcp resource say about themselves
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

#ifndef __httpd_oauth_metadata_h__
#define __httpd_oauth_metadata_h__

#include <string>

namespace httpd
{
namespace oauth
{

// RFC 8414 section 2, issuer base.
std::string authorizationServerMetadata(const std::string &base);
// RFC 9728 section 2, resource mcp::resourceOf(base).
std::string protectedResourceMetadata(const std::string &base);

} // namespace oauth
} // namespace httpd

#endif
