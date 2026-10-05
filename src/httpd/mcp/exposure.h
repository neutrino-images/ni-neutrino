/*
 * exposure.h - who reaches the AI surface, and through which door
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

#ifndef __httpd_mcp_exposure_h__
#define __httpd_mcp_exposure_h__

#include "httpd/endpoint.h"
#include "httpd/netmatch.h"

#include <cstddef>
#include <string>
#include <vector>

namespace httpd
{

struct WebConfig;

namespace exposure
{

// Lower case, no default port, no trailing slash, no path; why gets a sentence on refusal.
bool normalisePublicUrl(const std::string &in, std::string *out, std::string *why);

const size_t kMaxTrustedProxies = 16;

// At most /24 or /64, never loopback or unspecified; out is empty on refusal.
bool readTrustedProxies(const std::string &text, std::vector<NetPrefix> *out, std::string *why);

std::string trustedProxyText(const std::vector<NetPrefix> &list);

// What the transport saw, as text.
struct Seen
{
	std::string peer;
	std::string forwarded_for;      // every X-Forwarded-For value, joined
	bool        carries_forwarding;
};

struct Policy
{
	bool                   enabled;
	bool                   strict;       // refuse forwarding headers from unlisted peers
	bool                   allow_lan;
	std::string            public_url;
	std::vector<NetPrefix> tunnels;
	std::vector<NetPrefix> web_proxies;  // ni-web.conf trusted_proxies
};

Policy closedPolicy();

Origin classify(const Seen &s, const Policy &p);

// A trailing slash marks a prefix.
const char *const *tunnelPaths(size_t *count);

const char *const *forwardingHeaders(size_t *count);

// The raw path in its one plain spelling under one of tunnelPaths.
bool tunnelPathAllowed(const std::string &raw_path);

// Decoded as the router decodes it, so an escaped spelling is caught too.
bool isAiPath(const std::string &raw_path);

enum class Admit
{
	Pass,
	Hidden,
	Refused
};

Admit admit(Origin o, const std::string &raw_path, const Policy &p);

std::string forwardedClient(const Seen &s);

Policy policyFrom(const WebConfig &c);

// Word for word the router's 404, so a hidden route reads as no route.
Response hiddenResponse();

Response refusedResponse();

// Tells a case the gate's 404 from a router's.
void   noteTurnedAway();
size_t turnedAwayForTest();

// Tunnel: ai_public_url; Lan: "http://" + host when host is a plain authority; else empty.
std::string baseUrl(Origin o, const std::string &host);
std::string baseUrl(const Request &r);   // baseUrl(r.origin(), r.host())

} // namespace exposure

} // namespace httpd

#endif
