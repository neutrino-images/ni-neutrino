/*
 * verify.h - the bearer token check the mcp endpoint asks
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

#ifndef __httpd_oauth_verify_h__
#define __httpd_oauth_verify_h__

#include "httpd/mcp/contract.h"
#include "httpd/oauth/store.h"

#include <string>

namespace httpd
{
namespace oauth
{

// verifyAccessToken against a given store.
coreapi::Result<mcp::Caller> verifyAccessTokenIn(Store &s, const std::string &bearer, Origin origin,
                                                 const std::string &resource);

} // namespace oauth
} // namespace httpd

#endif
