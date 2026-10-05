/*
 * uri.cpp - URLs, redirect URIs and form bodies for the OAuth surface
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

#include "httpd/oauth/uri.h"

namespace httpd
{
namespace oauth
{

namespace
{

const size_t kMaxUrlBytes = 2048;
const size_t kMaxFormPairs = 64;

char lower(char c)
{
	return (c >= 'A' && c <= 'Z') ? (char) (c - 'A' + 'a') : c;
}

std::string lowered(const std::string &s)
{
	std::string out(s);
	for (size_t i = 0; i < out.size(); ++i)
		out[i] = lower(out[i]);
	return out;
}

bool isDigit(char c)
{
	return c >= '0' && c <= '9';
}

bool isAlpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool portAcceptable(const std::string &p)
{
	if (p.empty() || p.size() > 5)
		return false;
	unsigned long v = 0;
	for (size_t i = 0; i < p.size(); ++i)
	{
		if (!isDigit(p[i]))
			return false;
		v = v * 10 + (unsigned long) (p[i] - '0');
	}
	return v >= 1 && v <= 65535;
}

bool nameAcceptable(const std::string &h)
{
	if (h.empty() || h.size() > 253)
		return false;
	for (size_t i = 0; i < h.size(); ++i)
	{
		const char c = h[i];
		if (!isAlpha(c) && !isDigit(c) && c != '-' && c != '.')
			return false;
	}
	return true;
}

bool v6LiteralAcceptable(const std::string &h)
{
	if (h.size() < 4 || h[0] != '[' || h[h.size() - 1] != ']')
		return false;
	for (size_t i = 1; i + 1 < h.size(); ++i)
	{
		const char c = lower(h[i]);
		if (!isDigit(c) && !(c >= 'a' && c <= 'f') && c != ':' && c != '.')
			return false;
	}
	return true;
}

// host[:port] or [v6][:port]
bool splitAuthority(const std::string &a, std::string *host, std::string *port)
{
	host->clear();
	port->clear();
	if (a.empty())
		return false;
	if (a[0] == '[')
	{
		const size_t close = a.find(']');
		if (close == std::string::npos)
			return false;
		*host = a.substr(0, close + 1);
		if (!v6LiteralAcceptable(*host))
			return false;
		if (close + 1 == a.size())
			return true;
		if (a[close + 1] != ':')
			return false;
		*port = a.substr(close + 2);
		return portAcceptable(*port);
	}
	const size_t colon = a.find(':');
	if (colon == std::string::npos)
	{
		*host = a;
		return nameAcceptable(*host);
	}
	*host = a.substr(0, colon);
	*port = a.substr(colon + 1);
	return nameAcceptable(*host) && portAcceptable(*port);
}

int hexValue(char c)
{
	if (isDigit(c))
		return c - '0';
	c = lower(c);
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

bool decodeComponent(const std::string &in, std::string *out)
{
	out->clear();
	for (size_t i = 0; i < in.size(); ++i)
	{
		const char c = in[i];
		if (c == '+')
			*out += ' ';
		else if (c == '%')
		{
			if (i + 2 >= in.size())
				return false;
			const int a = hexValue(in[i + 1]);
			const int b = hexValue(in[i + 2]);
			if (a < 0 || b < 0)
				return false;
			*out += (char) ((a << 4) | b);
			i += 2;
		}
		else
			*out += c;
	}
	return true;
}

} // namespace

bool parseUrl(const std::string &text, Url *out)
{
	*out = Url();
	if (text.empty() || text.size() > kMaxUrlBytes)
		return false;
	for (size_t i = 0; i < text.size(); ++i)
	{
		const unsigned char c = (unsigned char) text[i];
		if (c <= 0x20 || c >= 0x7f)
			return false;
	}
	const size_t sep = text.find("://");
	if (sep == std::string::npos || sep == 0 || !isAlpha(text[0]))
		return false;
	for (size_t i = 0; i < sep; ++i)
	{
		const char c = text[i];
		if (!isAlpha(c) && !isDigit(c) && c != '+' && c != '-' && c != '.')
			return false;
	}
	out->scheme = lowered(text.substr(0, sep));

	const size_t start = sep + 3;
	size_t end = text.find_first_of("/?#", start);
	if (end == std::string::npos)
		end = text.size();
	std::string authority = text.substr(start, end - start);
	const size_t at = authority.rfind('@');
	if (at != std::string::npos)
	{
		out->userinfo = true;
		authority = authority.substr(at + 1);
	}
	if (!splitAuthority(authority, &out->host, &out->port))
		return false;
	out->host = lowered(out->host);

	const size_t hash = text.find('#', end);
	const std::string rest = text.substr(end, (hash == std::string::npos ? text.size() : hash) - end);
	out->fragment = (hash != std::string::npos);
	const size_t q = rest.find('?');
	out->path = rest.substr(0, q);
	if (q != std::string::npos)
	{
		out->has_query = true;
		out->query = rest.substr(q + 1);
	}
	return true;
}

bool isLoopbackHost(const std::string &host)
{
	return host == "localhost" || host == "127.0.0.1" || host == "[::1]";
}

bool redirectUriAcceptable(const std::string &uri)
{
	if (uri.size() > kMaxRedirectUriBytes)
		return false;
	Url u;
	if (!parseUrl(uri, &u) || u.userinfo || u.fragment)
		return false;
	if (u.scheme == "https")
		return true;
	return u.scheme == "http" && isLoopbackHost(u.host);
}

bool redirectUriMatches(const std::string &registered, const std::string &requested)
{
	if (!redirectUriAcceptable(requested))
		return false;
	if (registered == requested)
		return true;
	Url r;
	Url q;
	if (!parseUrl(registered, &r) || !parseUrl(requested, &q))
		return false;
	if (r.scheme != "http" || !isLoopbackHost(r.host))
		return false;
	// localhost and 127.0.0.1 stay two registrations; only the port is free.
	return q.scheme == "http" && q.host == r.host &&
	       q.path == r.path && q.has_query == r.has_query && q.query == r.query &&
	       !q.userinfo && !q.fragment;
}

bool cimdUrlAcceptable(const std::string &url)
{
	if (url.size() > kMaxRedirectUriBytes)
		return false;
	Url u;
	if (!parseUrl(url, &u) || u.scheme != "https" || u.userinfo || u.fragment || u.has_query)
		return false;
	if (u.path.size() < 2)
		return false;
	const std::string low = lowered(u.path);
	if (low.find("%2e") != std::string::npos)
		return false;
	size_t i = 1;
	while (i <= u.path.size())
	{
		size_t end = u.path.find('/', i);
		if (end == std::string::npos)
			end = u.path.size();
		const std::string seg = u.path.substr(i, end - i);
		if (seg == "." || seg == "..")
			return false;
		i = end + 1;
	}
	return true;
}

bool parseForm(const std::string &text, Form *out)
{
	out->clear();
	size_t pairs = 0;
	size_t i = 0;
	while (i < text.size())
	{
		size_t end = text.find('&', i);
		if (end == std::string::npos)
			end = text.size();
		const std::string piece = text.substr(i, end - i);
		i = end + 1;
		if (piece.empty())
			continue;
		if (++pairs > kMaxFormPairs)
			return false;
		const size_t eq = piece.find('=');
		std::string name;
		std::string value;
		if (!decodeComponent(piece.substr(0, eq), &name) || name.empty())
			return false;
		if (eq != std::string::npos && !decodeComponent(piece.substr(eq + 1), &value))
			return false;
		if (out->count(name) != 0)
			return false;
		(*out)[name] = value;
	}
	return true;
}

std::string formEncode(const std::string &s)
{
	static const char d[] = "0123456789ABCDEF";
	std::string out;
	for (size_t i = 0; i < s.size(); ++i)
	{
		const unsigned char c = (unsigned char) s[i];
		if (isAlpha((char) c) || isDigit((char) c) || c == '-' || c == '.' || c == '_' || c == '~')
			out += (char) c;
		else
		{
			out += '%';
			out += d[c >> 4];
			out += d[c & 15];
		}
	}
	return out;
}

std::string withQuery(const std::string &base,
                      const std::vector<std::pair<std::string, std::string> > &params)
{
	std::string out = base;
	char sep = (base.find('?') == std::string::npos) ? '?' : '&';
	for (size_t i = 0; i < params.size(); ++i)
	{
		out += sep;
		out += formEncode(params[i].first);
		out += '=';
		out += formEncode(params[i].second);
		sep = '&';
	}
	return out;
}

const std::string &formValue(const Form &f, const char *name)
{
	static const std::string empty;
	const Form::const_iterator it = f.find(name);
	return it == f.end() ? empty : it->second;
}

} // namespace oauth
} // namespace httpd
