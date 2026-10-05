/*
 * uri.h - URLs, redirect URIs and form bodies for the OAuth surface
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

#ifndef __httpd_oauth_uri_h__
#define __httpd_oauth_uri_h__

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace httpd
{
namespace oauth
{

struct Url
{
	std::string scheme;
	std::string host;
	std::string port;
	std::string path;
	std::string query;
	bool        has_query;
	bool        userinfo;
	bool        fragment;

	Url() : has_query(false), userinfo(false), fragment(false) {}
};

const size_t kMaxRedirectUriBytes = 512;

// Absolute URLs only. No character outside printable ASCII is accepted.
bool parseUrl(const std::string &text, Url *out);

bool isLoopbackHost(const std::string &host);

// MCP "Communication Security": https, or http to the loopback.
bool redirectUriAcceptable(const std::string &uri);

// Exact string match (OAuth 2.1 section 4.1.1), except a loopback http
// registration matches any port (RFC 8252 section 7.3).
bool redirectUriMatches(const std::string &registered, const std::string &requested);

// draft-ietf-oauth-client-id-metadata-document section 3.
bool cimdUrlAcceptable(const std::string &url);

typedef std::map<std::string, std::string> Form;

// application/x-www-form-urlencoded; also how an authorization request's query is read.
bool parseForm(const std::string &text, Form *out);
std::string formEncode(const std::string &s);
std::string withQuery(const std::string &base,
                      const std::vector<std::pair<std::string, std::string> > &params);

// Empty string reference if name is absent, never a dangling one.
const std::string &formValue(const Form &f, const char *name);

} // namespace oauth
} // namespace httpd

#endif
