/*
 * verify.cpp - the bearer token check the mcp endpoint asks
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

#include "httpd/oauth/verify.h"

#include "httpd/oauth/scopes.h"
#include "httpd/oauth/tokens.h"

namespace httpd
{
namespace oauth
{

namespace
{

// One answer for every cause, so a refusal tells a caller nothing.
coreapi::Result<mcp::Caller> refused()
{
	return coreapi::Result<mcp::Caller>::failure(
		coreapi::Error(coreapi::Status::Denied, coreapi::ErrorCode::NotPermitted,
		               "the token is not valid here"));
}

coreapi::Result<mcp::Caller> unreadable()
{
	return coreapi::Result<mcp::Caller>::failure(
		coreapi::Error(coreapi::Status::Internal, coreapi::ErrorCode::BoxUnreadable,
		               "the token store could not be read"));
}

} // namespace

coreapi::Result<mcp::Caller> verifyAccessTokenIn(Store &s, const std::string &bearer, Origin origin,
                                                 const std::string &resource)
{
	if (origin == Origin::Refused)
		return refused();
	// Static tokens are LAN only, access tokens tunnel only.
	const TokenKind kind = (origin == Origin::Lan) ? TokenKind::Static : TokenKind::Access;
	if (!looksLike(bearer, kind))
		return refused();

	TokenFacts facts;
	bool failed = false;
	if (!s.checkToken(bearer, &facts, &failed))
		return failed ? unreadable() : refused();
	if (origin == Origin::Tunnel && (resource.empty() || facts.resource != resource))
		return refused();
	const AuthLevel level = levelFor(facts.scopes);
	if (level == AuthLevel::Public)
		return refused();

	mcp::Caller c;
	c.client_id = facts.key;
	c.user = facts.user;
	c.level = level;
	c.external = (origin == Origin::Tunnel);
	c.groups = facts.groups;
	c.connection = facts.grant_id.empty() ? facts.key : facts.grant_id;
	return coreapi::Result<mcp::Caller>::success(c);
}

coreapi::Result<mcp::Caller> verifyAccessToken(const std::string &bearer, Origin origin,
                                               const std::string &resource)
{
	return verifyAccessTokenIn(store(), bearer, origin, resource);
}

} // namespace oauth
} // namespace httpd
