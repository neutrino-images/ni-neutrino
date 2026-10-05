/*
 * scopes.h - what an OAuth client may be granted on this box
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

#ifndef __httpd_oauth_scopes_h__
#define __httpd_oauth_scopes_h__

#include "httpd/endpoint.h"

#include <string>
#include <vector>

namespace httpd
{
namespace oauth
{

// offline_access only decides whether a refresh token is issued.
const unsigned ScopeRead    = 1u;
const unsigned ScopeWrite   = 2u;
const unsigned ScopeSystem  = 4u;
const unsigned ScopeOffline = 8u;
const unsigned ScopeLevels  = ScopeRead | ScopeWrite | ScopeSystem;

// Space separated (RFC 6749 section 3.3). bits is left alone on refusal.
bool parseScopes(const std::string &text, unsigned *bits);

unsigned withImplied(unsigned bits);

std::string scopeString(unsigned bits);
std::vector<std::string> scopeNames(unsigned bits);

// Public when no level scope is set.
AuthLevel levelFor(unsigned bits);

// MCP 2026-07-28 keeps offline_access out of what the resource advertises.
std::vector<std::string> resourceScopes();
std::vector<std::string> serverScopes();

} // namespace oauth
} // namespace httpd

#endif
