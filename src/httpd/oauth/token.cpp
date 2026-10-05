/*
 * token.cpp - the token and revocation endpoints
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

#include "httpd/oauth/token.h"

#include "httpd/json.h"
#include "httpd/mcp/contract.h"
#include "httpd/oauth/authorize.h"
#include "httpd/oauth/scopes.h"
#include "httpd/oauth/store.h"
#include "httpd/oauth/surface.h"
#include "httpd/oauth/tokens.h"
#include "httpd/oauth/uri.h"

#include <utility>

namespace httpd
{
namespace oauth
{

namespace
{

Response bad(const char *error, const char *description)
{
	return oauthError(StatusBadRequest, error, description);
}

Response unavailable()
{
	return oauthError(StatusServiceUnavailable, "temporarily_unavailable", "no token could be issued");
}

Response issued(const Issued &out)
{
	std::string body;
	Json j(body, 384);
	j.beginObject();
	j.key("access_token");
	j.value(out.access_token);
	j.key("token_type");
	j.value("Bearer");
	j.key("expires_in");
	j.value(out.expires_in);
	j.key("scope");
	j.value(scopeString(out.scopes));
	if (!out.refresh_token.empty())
	{
		j.key("refresh_token");
		j.value(out.refresh_token);
	}
	j.endObject();
	return oauthJson(StatusOk, body);
}

bool readForm(const std::string &body, const std::string &content_type, Form *f, Response *refusal)
{
	if (!contentTypeIs(content_type, "application/x-www-form-urlencoded"))
	{
		*refusal = bad("invalid_request", "the body must be application/x-www-form-urlencoded");
		return false;
	}
	if (body.size() > kMaxTokenRequestBytes || !parseForm(body, f))
	{
		*refusal = bad("invalid_request", "a parameter is repeated, garbled or too long");
		return false;
	}
	return true;
}

Response exchangeCode(const Form &f)
{
	const std::string &code = formValue(f, "code");
	const std::string &redirect_uri = formValue(f, "redirect_uri");
	const std::string &verifier = formValue(f, "code_verifier");
	const std::string &client_id = formValue(f, "client_id");
	if (code.empty() || redirect_uri.empty() || verifier.empty() || client_id.empty())
		return bad("invalid_request", "code, redirect_uri, code_verifier and client_id are required");

	CodeGrant g;
	std::string replayed;
	switch (redeemCode(code, &g, &replayed))
	{
		case Redeem::Fresh:
			break;
		case Redeem::Replayed:
			if (!replayed.empty())
				store().revokeGrant(replayed);
			return bad("invalid_grant", "the code was already used");
		case Redeem::Unknown:
			return bad("invalid_grant", "the code is not valid");
	}
	if (g.client_id != client_id || g.redirect_uri != redirect_uri || !challengeMatches(verifier, g.challenge))
		return bad("invalid_grant", "the code does not belong to this request");
	if (f.count("resource") != 0 && formValue(f, "resource") != g.resource)
		return bad("invalid_target", "the resource is not the one the code was issued for");

	Client c;
	if (g.kind == ClientKind::Registered)
	{
		if (!store().findClient(client_id, &c))
			return oauthError(StatusUnauthorized, "invalid_client", "the client is not registered");
	}
	else
	{
		c.client_id = g.client_id;
		c.kind = ClientKind::Metadata;
		c.name = g.client_name;
		c.redirect_uris = g.redirect_uris;
	}

	Issued out;
	std::string grant_id;
	if (!store().issue(c, g.user, g.scopes, g.resource, &out, &grant_id, g.groups))
		return unavailable();
	bindCodeToGrant(code, grant_id);
	return issued(out);
}

Response refreshGrant(const Form &f, const std::string &base)
{
	const std::string &token = formValue(f, "refresh_token");
	const std::string &client_id = formValue(f, "client_id");
	if (token.empty() || client_id.empty())
		return bad("invalid_request", "refresh_token and client_id are required");
	unsigned narrow = 0;
	if (f.count("scope") != 0 && !parseScopes(formValue(f, "scope"), &narrow))
		return bad("invalid_scope", "a requested scope is not offered");
	if (f.count("resource") != 0 && formValue(f, "resource") != mcp::resourceOf(base))
		return bad("invalid_target", "the resource is not this server");

	Issued out;
	switch (store().refresh(token, client_id, mcp::resourceOf(base), narrow, &out))
	{
		case RefreshOutcome::Issued:
			return issued(out);
		case RefreshOutcome::Invalid:
		case RefreshOutcome::Reused:
			return bad("invalid_grant", "the refresh token is not valid");
		case RefreshOutcome::BadScope:
			return bad("invalid_scope", "the scope is wider than the grant");
		case RefreshOutcome::Failed:
			break;
	}
	return unavailable();
}

} // namespace

Response answerToken(const std::string &body, const std::string &content_type, const std::string &base)
{
	Form f;
	Response refusal;
	if (!readForm(body, content_type, &f, &refusal))
		return refusal;
	const std::string &type = formValue(f, "grant_type");
	if (type.empty())
		return bad("invalid_request", "grant_type is required");
	if (type == "authorization_code")
		return exchangeCode(f);
	if (type == "refresh_token")
		return refreshGrant(f, base);
	return bad("unsupported_grant_type", "only authorization_code and refresh_token are offered");
}

Response answerRevoke(const std::string &body, const std::string &content_type)
{
	Form f;
	Response refusal;
	if (!readForm(body, content_type, &f, &refusal))
		return refusal;
	const std::string &token = formValue(f, "token");
	const std::string &client_id = formValue(f, "client_id");
	if (token.empty() || client_id.empty())
		return bad("invalid_request", "token and client_id are required");
	// The hint is only a hint (RFC 7009 section 2.1); the token's own prefix decides.
	store().revokeToken(token, client_id);
	Response r = oauthJson(StatusOk, std::string());
	r.content_type.clear();
	return r;
}

} // namespace oauth
} // namespace httpd
