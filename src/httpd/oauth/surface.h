/*
 * surface.h - the OAuth endpoints as one mount in front of the router
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

#ifndef __httpd_oauth_surface_h__
#define __httpd_oauth_surface_h__

#include "httpd/endpoint.h"
#include "httpd/http.h"

#include <string>

namespace httpd
{
namespace oauth
{

const int kStatusFound = 302;

struct Exchange
{
	Method      method;
	std::string path;
	std::string query;
	std::string body;
	std::string content_type;
	std::string accept_language;
	std::string consent_cookie;
	std::string peer;
	// Whom the per-source limits count: the forwarded client through the tunnel, else the peer.
	std::string source;
	Origin      origin;
	std::string base;

	Exchange() : method(UnknownMethod), origin(Origin::Refused)
	{
	}
};

bool handles(const std::string &path);
bool keepsBody(Method m, const std::string &path);

// 404 unless the request came through the tunnel and the public URL is set.
Response answer(const Exchange &x);

const char *consentCookieName();

// RFC 6749 section 5.2 shape, used by every endpoint here.
Response oauthError(int code, const char *error, const std::string &description);
Response oauthJson(int code, const std::string &body);

// The media type of a Content-Type value, compared without case and parameters.
bool contentTypeIs(const std::string &header, const char *type);

} // namespace oauth
} // namespace httpd

#endif
