/*
 * aiguides.cpp - what a person pastes into a tunnel or a client to reach the box
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

#include "httpd/mcp/aiguides.h"

namespace httpd
{

namespace exposure
{

namespace
{

const char kNoHost[]  = "YOUR-DOMAIN";
const char kNoBox[]   = "http://BOX-ADDRESS";
const char kNoToken[] = "YOUR-TOKEN";

struct PublicName
{
	bool        known;
	std::string host;
	std::string port;   // empty for 443
};

// The settings hold a canonical origin, so a split at the scheme and the colon is enough.
PublicName publicOf(const std::string &url)
{
	PublicName o;
	o.known = url.compare(0, 8, "https://") == 0 && url.size() > 8;
	if (!o.known)
	{
		o.host = kNoHost;
		return o;
	}
	const std::string rest = url.substr(8);
	const size_t colon = rest.find(':');
	o.host = rest.substr(0, colon);
	if (colon != std::string::npos)
		o.port = rest.substr(colon + 1);
	return o;
}

bool isPrefix(const std::string &p)
{
	return p.size() > 1 && p[p.size() - 1] == '/';
}

// The gate's paths hold letters, digits and -._~/ only; a dot is the one byte to escape.
std::string ruleFor(const std::string &p)
{
	std::string out;
	for (size_t i = 1; i < p.size(); ++i)
	{
		if (p[i] == '.')
			out += "\\.";
		else
			out += p[i];
	}
	if (isPrefix(p))
		out += ".+";
	return out;
}

std::string cloudflare(const PublicName &o, const std::vector<std::string> &paths, const std::string &box)
{
	std::string alts;
	for (size_t i = 0; i < paths.size(); ++i)
	{
		if (i != 0)
			alts += "|";
		alts += ruleFor(paths[i]);
	}
	return std::string("tunnel: TUNNEL-ID\n"
	                   "credentials-file: /etc/cloudflared/TUNNEL-ID.json\n"
	                   "ingress:\n"
	                   "  - hostname: ") + o.host + "\n"
	       "    path: ^/(" + alts + ")$\n"
	       "    service: " + box + "\n"
	       "  - service: http_status:404\n";
}

std::string tailscale(const PublicName &o, const std::vector<std::string> &paths, const std::string &box)
{
	const std::string port = o.port.empty() ? std::string("443") : o.port;
	std::string out;
	for (size_t i = 0; i < paths.size(); ++i)
	{
		const std::string mount = isPrefix(paths[i]) ? paths[i].substr(0, paths[i].size() - 1) : paths[i];
		out += "tailscale funnel --bg --https=" + port + " --set-path=" + mount + " " + box + mount + "\n";
	}
	return out;
}

std::string caddy(const PublicName &o, const std::vector<std::string> &paths, const std::string &box)
{
	std::string match;
	for (size_t i = 0; i < paths.size(); ++i)
		match += " " + paths[i] + (isPrefix(paths[i]) ? "*" : "");
	const std::string site = o.port.empty() ? o.host : o.host + ":" + o.port;
	return site + " {\n"
	       "\t@ai path" + match + "\n"
	       "\thandle @ai {\n"
	       "\t\treverse_proxy " + box + "\n"
	       "\t}\n"
	       "\thandle {\n"
	       "\t\trespond 404\n"
	       "\t}\n"
	       "}\n";
}

std::string nginxLocation(const std::string &match, const std::string &box, bool limited)
{
	return "\tlocation " + match + " {\n" +
	       (limited ? "\t\tlimit_req zone=ni_signin burst=5 nodelay;\n\t\tlimit_req_status 429;\n" : "") +
	       "\t\tproxy_pass " + box + ";\n"
	       "\t\tproxy_set_header Host $host;\n"
	       "\t\tproxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;\n"
	       "\t\tproxy_set_header X-Forwarded-Proto $scheme;\n"
	       "\t\tproxy_buffering off;\n"
	       "\t}\n";
}

std::string nginx(const PublicName &o, const std::vector<std::string> &paths, const std::string &box)
{
	bool oauth = false;
	for (size_t i = 0; i < paths.size(); ++i)
		oauth = oauth || paths[i] == "/oauth/";
	// The two endpoints anyone may call without a credential, limited per address.
	std::string out = oauth ? "limit_req_zone $binary_remote_addr zone=ni_signin:1m rate=10r/m;\n\n" : "";
	out += "server {\n"
	                  "\tlisten " + (o.port.empty() ? std::string("443") : o.port) + " ssl;\n"
	                  "\tserver_name " + o.host + ";\n"
	                  "\tssl_certificate /etc/letsencrypt/live/" + o.host + "/fullchain.pem;\n"
	                  "\tssl_certificate_key /etc/letsencrypt/live/" + o.host + "/privkey.pem;\n"
	                  "\n";
	for (size_t i = 0; i < paths.size(); ++i)
		out += nginxLocation(isPrefix(paths[i]) ? paths[i] : "= " + paths[i], box, false);
	if (oauth)
	{
		out += nginxLocation("= /oauth/register", box, true);
		out += nginxLocation("= /oauth/authorize", box, true);
	}
	out += "\tlocation / {\n"
	       "\t\treturn 404;\n"
	       "\t}\n"
	       "}\n";
	return out;
}

bool endsWith(const std::string &s, const char *tail)
{
	const std::string t(tail);
	return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0;
}

TunnelGuide make(const char *id, const char *file, const std::string &snippet, bool ready)
{
	TunnelGuide t;
	t.id = id;
	t.file = file;
	t.snippet = snippet;
	t.ready = ready;
	// The agent's own machine loses the web interface.
	t.warnings.push_back("device");
	return t;
}

} // namespace

std::vector<TunnelGuide> tunnelGuides(const GuidePlace &p)
{
	const PublicName o = publicOf(p.public_url);
	const std::string box = p.box.empty() ? std::string(kNoBox) : p.box;
	const bool ready = o.known && !p.box.empty() && p.enabled && !p.paths.empty();

	std::vector<TunnelGuide> out;
	TunnelGuide cf = make("cloudflare", "config.yml", cloudflare(o, p.paths, box), ready);
	cf.warnings.push_back("cloudflare-ratelimit");
	out.push_back(cf);

	TunnelGuide funnel = make("tailscale", "", tailscale(o, p.paths, box), ready);
	if (o.known && !endsWith(o.host, ".ts.net"))
		funnel.warnings.push_back("tailscale-name");
	if (o.known && !o.port.empty() && o.port != "8443" && o.port != "10000")
		funnel.warnings.push_back("tailscale-port");
	out.push_back(funnel);

	out.push_back(make("caddy", "Caddyfile", caddy(o, p.paths, box), ready));

	TunnelGuide n = make("nginx", "sites-enabled/ni-box.conf", nginx(o, p.paths, box), ready);
	n.warnings.push_back("nginx-acme");
	out.push_back(n);

	// A port forward on the router to Caddy in the home network, under a dynamic DNS name.
	TunnelGuide dyn = make("dyndns", "Caddyfile", caddy(o, p.paths, box), ready);
	dyn.warnings.push_back("dyndns-fixed-address");
	dyn.warnings.push_back("dyndns-no-direct");
	out.push_back(dyn);
	return out;
}

std::vector<ClientGuide> clientGuides(const GuidePlace &p)
{
	const PublicName o = publicOf(p.public_url);
	const std::string public_mcp = (o.known ? p.public_url : std::string("https://") + kNoHost) + "/mcp";
	const std::string lan_mcp = (p.box.empty() ? std::string(kNoBox) : p.box) + "/mcp";

	const char *const ids[] = { "claude", "claude-code", "chatgpt", "home-assistant" };
	const bool token[] = { false, true, false, true };

	std::vector<ClientGuide> out;
	for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i)
	{
		ClientGuide c;
		c.id = ids[i];
		c.needs = token[i] ? "token" : "public";
		c.url = token[i] ? lan_mcp : public_mcp;
		if (c.id == "claude-code")
			c.command = "claude mcp add --transport http neutrino " + lan_mcp +
			            " --header \"Authorization: Bearer " + kNoToken + "\"";
		c.ready = p.enabled && (token[i] ? (p.allow_lan && !p.box.empty()) : o.known);
		out.push_back(c);
	}
	return out;
}

} // namespace exposure

} // namespace httpd
