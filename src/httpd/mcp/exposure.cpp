/*
 * exposure.cpp - who reaches the AI surface, and through which door
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

#include "httpd/mcp/exposure.h"

#include "httpd/auth.h"
#include "httpd/http.h"
#include "httpd/status.h"
#include "httpd/webconfig.h"
#include "coreapi/base/errors.h"

#include <cstdio>
#include <cstring>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <sys/socket.h>

namespace httpd
{

namespace exposure
{

namespace
{

const size_t kMaxUrlBytes = 300;
const int    kNarrowestV4 = 24;
const int    kNarrowestV6 = 64;

char lowered(char c)
{
	return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool isDigit(char c)
{
	return c >= '0' && c <= '9';
}

void say(std::string *why, const char *what)
{
	if (why != NULL)
		*why = what;
}

// A name, never an address literal: a client checks a certificate issued for a name.
bool isHostName(const std::string &h)
{
	if (h.empty() || h.size() > 253)
		return false;

	size_t labels = 0;
	bool last_all_digits = false;
	size_t start = 0;
	while (start <= h.size())
	{
		size_t dot = h.find('.', start);
		if (dot == std::string::npos)
			dot = h.size();
		if (dot == start || dot - start > 63)
			return false;
		if (h[start] == '-' || h[dot - 1] == '-')
			return false;

		bool all_digits = true;
		for (size_t i = start; i < dot; ++i)
		{
			const char c = h[i];
			if (!(c >= 'a' && c <= 'z') && !isDigit(c) && c != '-')
				return false;
			if (!isDigit(c))
				all_digits = false;
		}
		last_all_digits = all_digits;
		++labels;
		start = dot + 1;
	}
	return labels >= 2 && !last_all_digits;
}

// No leading zero, so one port has one spelling.
bool readPort(const std::string &text, unsigned long &out)
{
	if (text.empty() || text.size() > 5 || text[0] == '0')
		return false;

	unsigned long v = 0;
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (!isDigit(text[i]))
			return false;
		v = (v * 10) + static_cast<unsigned long>(text[i] - '0');
	}
	if (v < 1 || v > 65535)
		return false;
	out = v;
	return true;
}

std::string trimmedEntry(const std::string &v)
{
	size_t from = 0;
	size_t to = v.size();
	while (from < to && (v[from] == ' ' || v[from] == '\t'))
		++from;
	while (to > from && (v[to - 1] == ' ' || v[to - 1] == '\t'))
		--to;
	return v.substr(from, to - from);
}

bool readEntry(const std::string &entry, NetPrefix &p)
{
	if (entry.find('/') != std::string::npos)
		return parsePrefix(entry, &p);
	if (parsePrefix(entry + "/32", &p) && p.family == AF_INET)
		return true;
	return parsePrefix(entry + "/128", &p) && p.family == AF_INET6;
}

// Plugins talk to the box over loopback; a tunnel peer there would cut them off.
bool namesTheBox(const NetPrefix &p)
{
	if (p.family == AF_INET)
		return p.bits[0] == 127 || p.bits[0] == 0;
	for (size_t i = 0; i < 8; ++i)
	{
		if (p.bits[i] != 0)
			return false;
	}
	return true;
}

const char kMcpPath[] = "/mcp";

const char *const kTunnelPaths[] = { kMcpPath, "/oauth/", "/.well-known/" };

const size_t kTunnelPathCount = sizeof(kTunnelPaths) / sizeof(kTunnelPaths[0]);

const char *const kForwardingHeaders[] =
{
	"Forwarded", "X-Forwarded-For", "X-Forwarded-Host", "X-Forwarded-Proto",
	"X-Forwarded-Port", "X-Real-IP", "CF-Connecting-IP", "True-Client-IP"
};

bool startsWith(const std::string &s, const char *prefix)
{
	const size_t n = std::strlen(prefix);
	return s.size() >= n && s.compare(0, n, prefix) == 0;
}

int hexValue(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

std::string firstSegment(const std::string &raw)
{
	if (raw.empty() || raw[0] != '/')
		return std::string();

	const size_t end = raw.find('/', 1);
	const std::string seg = raw.substr(1, ((end == std::string::npos) ? raw.size() : end) - 1);

	std::string out;
	for (size_t i = 0; i < seg.size(); ++i)
	{
		if (seg[i] == '%' && i + 2 < seg.size() && hexValue(seg[i + 1]) >= 0 &&
		    hexValue(seg[i + 2]) >= 0)
		{
			out += static_cast<char>((hexValue(seg[i + 1]) * 16) + hexValue(seg[i + 2]));
			i += 2;
		}
		else
		{
			out += seg[i];
		}
	}
	return out;
}

bool isPlainPathByte(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || isDigit(c) ||
	       c == '-' || c == '_' || c == '.' || c == '~' || c == '/';
}

const size_t kMaxAuthorityBytes = 261;

size_t turned_away_ = 0;

OpenThreads::Mutex &turnedAwayLock()
{
	static OpenThreads::Mutex m;
	return m;
}

bool isAuthorityByte(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || isDigit(c) ||
	       c == '.' || c == '-' || c == ':' || c == '[' || c == ']';
}

} // namespace

bool normalisePublicUrl(const std::string &in, std::string *out, std::string *why)
{
	if (out == NULL)
		return false;
	out->clear();

	if (in.empty())
		return true;
	if (in.size() > kMaxUrlBytes)
	{
		say(why, "the address is longer than 300 bytes");
		return false;
	}

	const char kScheme[] = "https://";
	const size_t scheme_len = sizeof(kScheme) - 1;
	bool scheme_ok = in.size() > scheme_len;
	for (size_t i = 0; scheme_ok && i < scheme_len; ++i)
		scheme_ok = lowered(in[i]) == kScheme[i];
	if (!scheme_ok)
	{
		say(why, "the address does not begin with https://");
		return false;
	}

	std::string rest = in.substr(scheme_len);
	if (!rest.empty() && rest[rest.size() - 1] == '/')
		rest.erase(rest.size() - 1);
	if (rest.find_first_of("/?#@\\%") != std::string::npos)
	{
		say(why, "the address carries a path, a query, a fragment or a user");
		return false;
	}

	std::string host = rest;
	std::string port_text;
	const size_t colon = rest.find(':');
	if (colon != std::string::npos)
	{
		host = rest.substr(0, colon);
		port_text = rest.substr(colon + 1);
	}
	for (size_t i = 0; i < host.size(); ++i)
		host[i] = lowered(host[i]);
	if (!isHostName(host))
	{
		say(why, "the host is not a name of two labels or more");
		return false;
	}

	unsigned long port = 443;
	if (colon != std::string::npos && !readPort(port_text, port))
	{
		say(why, "the port is not a number from 1 to 65535");
		return false;
	}

	std::string result = std::string(kScheme) + host;
	if (port != 443)
	{
		char tail[8];
		std::snprintf(tail, sizeof(tail), ":%lu", port);
		result += tail;
	}
	*out = result;
	return true;
}

bool readTrustedProxies(const std::string &text, std::vector<NetPrefix> *out, std::string *why)
{
	if (out == NULL)
		return false;
	out->clear();

	if (trimmedEntry(text).empty())
		return true;

	std::vector<NetPrefix> read;
	size_t start = 0;
	while (start <= text.size())
	{
		size_t comma = text.find(',', start);
		if (comma == std::string::npos)
			comma = text.size();
		const std::string entry = trimmedEntry(text.substr(start, comma - start));
		start = comma + 1;

		NetPrefix p;
		if (entry.empty() || !readEntry(entry, p))
		{
			say(why, "an entry is not an address or a network");
			return false;
		}
		if ((p.family == AF_INET && p.len < kNarrowestV4) ||
		    (p.family == AF_INET6 && p.len < kNarrowestV6))
		{
			say(why, "an entry is wider than /24 or, for the second family, /64");
			return false;
		}
		if (namesTheBox(p))
		{
			say(why, "an entry names the box itself or no address at all");
			return false;
		}
		if (read.size() == kMaxTrustedProxies)
		{
			say(why, "the list names more than 16 entries");
			return false;
		}
		read.push_back(p);
	}

	*out = read;
	return true;
}

std::string trustedProxyText(const std::vector<NetPrefix> &list)
{
	std::string out;
	for (size_t i = 0; i < list.size(); ++i)
	{
		const std::string one = formatPrefix(list[i]);
		if (one.empty())
			continue;
		if (!out.empty())
			out += ",";
		out += one;
	}
	return out;
}

Policy closedPolicy()
{
	Policy p;
	p.enabled = false;
	p.strict = false;
	p.allow_lan = false;
	return p;
}

Origin classify(const Seen &s, const Policy &p)
{
	if (addressInAnyPrefix(s.peer, p.tunnels))
		return Origin::Tunnel;
	if (!p.strict)
		return Origin::Lan;
	// A tunnel peer whose address could not be read would otherwise be a local caller.
	if (s.peer.empty())
		return Origin::Refused;
	if (s.carries_forwarding && !addressInAnyPrefix(s.peer, p.web_proxies))
		return Origin::Refused;
	return Origin::Lan;
}

const char *const *tunnelPaths(size_t *count)
{
	if (count != NULL)
		*count = kTunnelPathCount;
	return kTunnelPaths;
}

const char *const *forwardingHeaders(size_t *count)
{
	if (count != NULL)
		*count = sizeof(kForwardingHeaders) / sizeof(kForwardingHeaders[0]);
	return kForwardingHeaders;
}

bool tunnelPathAllowed(const std::string &raw)
{
	if (raw.empty() || raw[0] != '/')
		return false;
	for (size_t i = 0; i < raw.size(); ++i)
	{
		if (!isPlainPathByte(raw[i]))
			return false;
	}

	size_t start = 1;
	while (start <= raw.size())
	{
		size_t slash = raw.find('/', start);
		if (slash == std::string::npos)
			slash = raw.size();
		const std::string seg = raw.substr(start, slash - start);
		if (seg.empty() || seg == "." || seg == "..")
			return false;
		start = slash + 1;
	}

	for (size_t i = 0; i < kTunnelPathCount; ++i)
	{
		const std::string p(kTunnelPaths[i]);
		const bool prefix = p[p.size() - 1] == '/';
		if (prefix ? startsWith(raw, kTunnelPaths[i]) : raw == p)
			return true;
	}
	return false;
}

bool isAiPath(const std::string &raw)
{
	const std::string first = firstSegment(raw);
	for (size_t i = 0; i < kTunnelPathCount; ++i)
	{
		std::string name(kTunnelPaths[i] + 1);
		if (!name.empty() && name[name.size() - 1] == '/')
			name.erase(name.size() - 1);
		if (first == name)
			return true;
	}
	return false;
}

Admit admit(Origin o, const std::string &raw_path, const Policy &p)
{
	if (o == Origin::Refused)
		return Admit::Refused;
	if (o == Origin::Tunnel)
	{
		if (!p.enabled || p.public_url.empty() || !tunnelPathAllowed(raw_path))
			return Admit::Hidden;
		return Admit::Pass;
	}
	if (o != Origin::Lan)
		return Admit::Hidden;
	if (!isAiPath(raw_path))
		return Admit::Pass;
	// OAuth needs the https public URL, so the LAN never reaches it.
	if (raw_path != kMcpPath)
		return Admit::Hidden;
	return (p.enabled && p.allow_lan) ? Admit::Pass : Admit::Hidden;
}

std::string forwardedClient(const Seen &s)
{
	return lastForwarded(s.forwarded_for);
}

Policy policyFrom(const WebConfig &c)
{
	Policy p = closedPolicy();
	p.enabled = c.ai_enabled;
	p.strict = c.ai_named || c.ai_enabled;
	p.allow_lan = c.ai_allow_lan;
	// No public URL is the closed tunnel; it opens with the reload a password change makes.
	if (!shippedPasswordInEffect(c))
		p.public_url = c.ai_public_url;
	p.tunnels = c.ai_trusted_proxies;
	p.web_proxies = c.trusted_proxies;
	return p;
}

Response hiddenResponse()
{
	return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchRoute,
	                       "this server has no such path");
}

Response refusedResponse()
{
	return problemResponse(StatusForbidden, coreapi::ErrorCode::ForwardedByUntrustedPeer,
	                       "this request was forwarded by a machine this box does not list as a proxy");
}

void noteTurnedAway()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(turnedAwayLock());
	++turned_away_;
}

size_t turnedAwayForTest()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(turnedAwayLock());
	return turned_away_;
}

std::string baseUrl(Origin o, const std::string &host)
{
	if (o == Origin::Tunnel)
		return policyFrom(config()).public_url;
	if (o != Origin::Lan)
		return std::string();

	if (host.empty() || host.size() > kMaxAuthorityBytes)
		return std::string();
	for (size_t i = 0; i < host.size(); ++i)
	{
		if (!isAuthorityByte(host[i]))
			return std::string();
	}
	return "http://" + host;
}

std::string baseUrl(const Request &r)
{
	return baseUrl(r.origin(), r.host());
}

} // namespace exposure

} // namespace httpd
