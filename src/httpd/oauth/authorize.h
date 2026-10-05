/*
 * authorize.h - the authorization endpoint, requests awaiting consent, and codes
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

#ifndef __httpd_oauth_authorize_h__
#define __httpd_oauth_authorize_h__

#include "httpd/endpoint.h"
#include "httpd/mcp/toolgroups.h"
#include "httpd/oauth/store.h"

#include <cstddef>
#include <string>
#include <vector>

#include <time.h>

namespace httpd
{
namespace oauth
{

const long   kPendingLifetime = 600;
const long   kCodeLifetime    = 60;
const long   kUsedCodeMemory  = 600;
// Answered request ids remembered so a form cannot be sent twice.
const size_t kMaxAnswered     = 256;
const size_t kMaxCodes        = 64;
const size_t kMaxStateBytes   = 1024;

struct PendingView
{
	std::string id;
	std::string client_id;
	std::string client_name;
	std::string redirect_uri;
	std::string redirect_host;
	std::string form_token;
	std::string base;
	std::string resource;
	ClientKind  kind;
	bool        loopback_only;
	unsigned    requested;

	PendingView() : kind(ClientKind::Registered), loopback_only(false), requested(0) {}
};

struct CodeGrant
{
	std::string client_id;
	std::string client_name;
	std::string redirect_uri;
	std::string challenge;
	std::string resource;
	std::string user;
	std::vector<std::string> redirect_uris;
	ClientKind  kind;
	unsigned    scopes;
	unsigned    groups;

	CodeGrant() : kind(ClientKind::Registered), scopes(0), groups(mcp::kDefaultGroups) {}
};

enum class Decided
{
	Redirect,
	NoSuchRequest,
	BadScopes,
	Failed
};

enum class Redeem
{
	Fresh,
	Replayed,
	Unknown
};

Response answerAuthorize(const std::string &query, const std::string &base);

bool viewRequest(const std::string &id, PendingView *out);

// granted may narrow the request and never widen it; on Redirect the request is gone.
Decided decideRequest(const std::string &id, bool approve, unsigned granted,
                      const std::string &user, std::string *redirect, unsigned groups = mcp::kDefaultGroups);

// Marks the code spent. A second use answers Replayed with the grant it produced.
Redeem redeemCode(const std::string &code, CodeGrant *out, std::string *grant_of_replay);
void bindCodeToGrant(const std::string &code, const std::string &grant_id);

void forgetAuthorizationStateForTest();
void setAuthorizeClockForTest(time_t (*clock)());

} // namespace oauth
} // namespace httpd

#endif
