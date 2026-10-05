/*
 * registration.cpp - dynamic client registration (RFC 7591)
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

#include "httpd/oauth/registration.h"

#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/oauth/clientmeta.h"
#include "httpd/oauth/store.h"
#include "httpd/oauth/surface.h"

#include <utility>

#include <time.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace httpd
{
namespace oauth
{

namespace
{

struct Window
{
	time_t   start;
	unsigned count;
};

OpenThreads::Mutex &limitLock()
{
	static OpenThreads::Mutex m;
	return m;
}

Window &window()
{
	static Window w = { 0, 0 };
	return w;
}

bool admitted(time_t now)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(limitLock());
	Window &w = window();
	if (now < w.start || now - w.start >= 60)
	{
		w.start = now;
		w.count = 0;
	}
	if (w.count >= kRegistrationsPerMinute)
		return false;
	++w.count;
	return true;
}

Response busy(const char *description, const char *retry_after)
{
	Response r = oauthError(StatusTooManyRequests, "temporarily_unavailable", description);
	r.headers.push_back(std::make_pair(std::string("Retry-After"), std::string(retry_after)));
	return r;
}

Response invalid(const char *description)
{
	return oauthError(StatusBadRequest, "invalid_client_metadata", description);
}

} // namespace

void resetRegistrationLimitForTest()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(limitLock());
	window().start = 0;
	window().count = 0;
}

Response answerRegister(const std::string &body, const std::string &content_type)
{
	if (!contentTypeIs(content_type, "application/json"))
		return invalid("the registration must be sent as application/json");
	if (body.size() > kMaxRegisterBytes)
		return invalid("the registration is too large");
	if (!admitted(realClock()))
		return busy("too many registrations; try again in a minute", "60");

	ClientMetadata m;
	switch (readClientMetadata(body, &m))
	{
		case MetaRead::Ok:
			break;
		case MetaRead::NotAnObject:
			return invalid("the registration is not one JSON object");
		case MetaRead::BadRedirectUris:
			return oauthError(StatusBadRequest, "invalid_redirect_uri",
			                  "redirect_uris must list 1 to 8 https or loopback http URIs");
		case MetaRead::BadValue:
			return invalid("a metadata value is not acceptable");
	}
	if (!flowsAcceptable(m))
		return invalid("only the authorization code flow with refresh tokens is offered");
	if (m.has_auth_method && m.token_endpoint_auth_method != "none" &&
	    m.token_endpoint_auth_method != "client_secret_basic" &&
	    m.token_endpoint_auth_method != "client_secret_post")
		return invalid("only public clients are registered here");

	Client c;
	if (!store().registerClient(displayName(m), m.redirect_uris, &c))
		return busy("this box holds as many registrations as it keeps", "3600");

	std::string out;
	Json j(out, 512);
	j.beginObject();
	j.key("client_id");
	j.value(c.client_id);
	j.key("client_id_issued_at");
	j.value((long long) c.created);
	j.key("client_name");
	j.value(c.name);
	j.key("redirect_uris");
	j.beginArray();
	for (size_t i = 0; i < c.redirect_uris.size(); ++i)
		j.value(c.redirect_uris[i]);
	j.endArray();
	j.key("grant_types");
	j.beginArray();
	j.value("authorization_code");
	j.value("refresh_token");
	j.endArray();
	j.key("response_types");
	j.beginArray();
	j.value("code");
	j.endArray();
	j.key("token_endpoint_auth_method");
	j.value("none");
	j.endObject();
	return oauthJson(StatusCreated, out);
}

} // namespace oauth
} // namespace httpd
