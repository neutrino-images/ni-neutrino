/*
 * surface.cpp - the OAuth endpoints as one mount in front of the router
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

#include "httpd/oauth/surface.h"

#include "httpd/json.h"
#include "httpd/oauth/authorize.h"
#include "httpd/oauth/consent.h"
#include "httpd/oauth/metadata.h"
#include "httpd/oauth/registration.h"
#include "httpd/oauth/sourcelimit.h"
#include "httpd/oauth/token.h"
#include "httpd/status.h"

#include <cstdio>
#include <utility>

namespace httpd
{
namespace oauth
{

namespace
{

const char kAsPath[]   = "/.well-known/oauth-authorization-server";
const char kPrmRoot[]  = "/.well-known/oauth-protected-resource";
const char kPrmMcp[]   = "/.well-known/oauth-protected-resource/mcp";
const char kAuthorize[] = "/oauth/authorize";
const char kRegister[] = "/oauth/register";
const char kToken[]    = "/oauth/token";
const char kRevoke[]   = "/oauth/revoke";
const char kConsent[]  = "/oauth/consent";

void header(Response &r, const char *name, const std::string &value)
{
	r.headers.push_back(std::make_pair(std::string(name), value));
}

bool isMetadata(const std::string &p)
{
	return p == kAsPath || p == kPrmRoot || p == kPrmMcp;
}

Response notServed()
{
	return oauthError(StatusNotFound, "not_found", "nothing is served here");
}

Response wrongMethod(const char *allow)
{
	Response r = oauthError(StatusMethodNotAllowed, "invalid_request", "this method is not served here");
	header(r, "Allow", allow);
	return r;
}

Response metadata(const Exchange &x, const std::string &body)
{
	if (x.method != Get && x.method != Head)
		return wrongMethod("GET, HEAD");
	return oauthJson(StatusOk, body);
}

// The page a browser shows for authorize and consent, the JSON error for a client's registration.
Response tooMany(bool page, unsigned retry_after)
{
	Response r;
	if (page)
	{
		r.code = StatusTooManyRequests;
		r.content_type = "text/plain; charset=utf-8";
		r.body = "Zu viele Anfragen von dieser Adresse. Bitte gleich noch einmal versuchen.\n"
		         "Too many requests from this address. Try again in a moment.\n";
		header(r, "Cache-Control", "no-store");
		header(r, "X-Frame-Options", "DENY");
		addNoSniff(r);
	}
	else
		r = oauthError(StatusTooManyRequests, "temporarily_unavailable",
		               "too many requests from this address; try again in a minute");
	char seconds[24];
	std::snprintf(seconds, sizeof(seconds), "%u", retry_after);
	header(r, "Retry-After", seconds);
	return r;
}

// Only the requests nobody signed for: a registration, an authorize, a consent post.
bool limited(const Exchange &x, Response *out)
{
	Limited what = Limited::Register;
	if (x.path == kRegister && x.method == Post)
		what = Limited::Register;
	else if (x.path == kAuthorize && (x.method == Get || x.method == Head))
		what = Limited::Authorize;
	else if (x.path == kConsent && x.method == Post)
		what = Limited::Consent;
	else
		return false;
	unsigned retry_after = 0;
	if (sourceAllows(what, x.source.empty() ? x.peer : x.source, &retry_after))
		return false;
	*out = tooMany(what != Limited::Register, retry_after);
	return true;
}

Response route(const Exchange &x)
{
	Response busy;
	if (limited(x, &busy))
		return busy;
	if (x.path == kAsPath)
		return metadata(x, authorizationServerMetadata(x.base));
	if (x.path == kPrmRoot || x.path == kPrmMcp)
		return metadata(x, protectedResourceMetadata(x.base));
	if (x.path == kRegister)
		return (x.method == Post) ? answerRegister(x.body, x.content_type) : wrongMethod("POST");
	if (x.path == kAuthorize)
		return (x.method == Get || x.method == Head) ? answerAuthorize(x.query, x.base) : wrongMethod("GET, HEAD");
	if (x.path == kConsent)
	{
		ConsentInput c;
		c.method = x.method;
		c.query = x.query;
		c.body = x.body;
		c.content_type = x.content_type;
		c.cookie = x.consent_cookie;
		c.accept_language = x.accept_language;
		c.peer = x.peer;
		return answerConsent(c);
	}
	if (x.path == kToken)
		return (x.method == Post) ? answerToken(x.body, x.content_type, x.base) : wrongMethod("POST");
	if (x.path == kRevoke)
		return (x.method == Post) ? answerRevoke(x.body, x.content_type) : wrongMethod("POST");
	return notServed();
}

} // namespace

bool handles(const std::string &path)
{
	return isMetadata(path) || path.compare(0, 7, "/oauth/") == 0;
}

bool keepsBody(Method m, const std::string &path)
{
	return m == Post && (path == kRegister || path == kToken || path == kRevoke || path == kConsent);
}

Response answer(const Exchange &x)
{
	if (x.origin != Origin::Tunnel || x.base.empty())
		return notServed();
	return route(x);
}

const char *consentCookieName()
{
	return "ni_oauth_consent";
}

Response oauthJson(int code, const std::string &body)
{
	Response r;
	r.code = code;
	r.content_type = "application/json";
	r.body = body;
	header(r, "Cache-Control", "no-store");
	header(r, "Pragma", "no-cache");
	addNoSniff(r);
	return r;
}

bool contentTypeIs(const std::string &header, const char *type)
{
	std::string media = header.substr(0, header.find(';'));
	while (!media.empty() && (media[media.size() - 1] == ' ' || media[media.size() - 1] == '\t'))
		media.erase(media.size() - 1);
	size_t start = 0;
	while (start < media.size() && (media[start] == ' ' || media[start] == '\t'))
		++start;
	media = media.substr(start);
	const std::string want = type;
	if (media.size() != want.size())
		return false;
	for (size_t i = 0; i < media.size(); ++i)
	{
		char a = media[i];
		if (a >= 'A' && a <= 'Z')
			a = (char) (a - 'A' + 'a');
		if (a != want[i])
			return false;
	}
	return true;
}

Response oauthError(int code, const char *error, const std::string &description)
{
	std::string body;
	Json j(body, 128);
	j.beginObject();
	j.key("error");
	j.value(error);
	if (!description.empty())
	{
		j.key("error_description");
		j.value(description);
	}
	j.endObject();
	return oauthJson(code, body);
}

} // namespace oauth
} // namespace httpd
