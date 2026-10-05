/*
 * test_aiguides.cpp - what the KI tab hands a person to paste into a tunnel or a client
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

#include "support/catch.hpp"
#include "support/fakes.h"
#include "httpd/endpoint.h"
#include "httpd/mcp/aiguides.h"
#include "httpd/mcp/exposure.h"
#include "httpd/router.h"
#include "httpd/webconfig.h"

#include <regex>
#include <string>
#include <vector>

using httpd::exposure::ClientGuide;
using httpd::exposure::GuidePlace;
using httpd::exposure::TunnelGuide;
using httpd::exposure::clientGuides;
using httpd::exposure::tunnelGuides;

namespace
{

GuidePlace place()
{
	GuidePlace p;
	p.public_url = "https://tv.example.org";
	p.box = "http://192.168.1.20";
	p.paths.push_back("/mcp");
	p.paths.push_back("/oauth/");
	p.paths.push_back("/.well-known/");
	p.enabled = true;
	p.allow_lan = true;
	return p;
}

const TunnelGuide &tunnel(const std::vector<TunnelGuide> &all, const char *id)
{
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].id == id)
			return all[i];
	}
	FAIL(std::string("no tunnel ") + id);
	return all[0];
}

const ClientGuide &client(const std::vector<ClientGuide> &all, const char *id)
{
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].id == id)
			return all[i];
	}
	FAIL(std::string("no client ") + id);
	return all[0];
}

size_t count(const std::string &haystack, const std::string &needle)
{
	size_t n = 0;
	for (size_t at = haystack.find(needle); at != std::string::npos; at = haystack.find(needle, at + 1))
		++n;
	return n;
}

} // namespace

TEST_CASE("the guides name five tunnels and four clients by fixed ids", "[exposure]")
{
	const std::vector<TunnelGuide> t = tunnelGuides(place());
	REQUIRE(t.size() == 5);
	CHECK(t[0].id == "cloudflare");
	CHECK(t[1].id == "tailscale");
	CHECK(t[2].id == "caddy");
	CHECK(t[3].id == "nginx");
	CHECK(t[4].id == "dyndns");
	const std::vector<ClientGuide> c = clientGuides(place());
	REQUIRE(c.size() == 4);
	CHECK(c[0].id == "claude");
	CHECK(c[1].id == "claude-code");
	CHECK(c[2].id == "chatgpt");
	CHECK(c[3].id == "home-assistant");
}

TEST_CASE("the cloudflared rule forwards the three prefixes and nothing else", "[exposure]")
{
	const std::vector<TunnelGuide> all = tunnelGuides(place());
	const TunnelGuide &g = tunnel(all, "cloudflare");
	CHECK(g.ready);
	CHECK(g.file == "config.yml");
	const size_t at = g.snippet.find("    path: ");
	REQUIRE(at != std::string::npos);
	const size_t end = g.snippet.find('\n', at);
	const std::regex rule(g.snippet.substr(at + 10, end - at - 10), std::regex::ECMAScript);

	const char *const forwarded[] = { "/mcp", "/oauth/token", "/.well-known/oauth-authorization-server" };
	for (size_t i = 0; i < sizeof(forwarded) / sizeof(forwarded[0]); ++i)
	{
		INFO(forwarded[i]);
		CHECK(std::regex_search(std::string(forwarded[i]), rule));
	}
	const char *const kept[] = { "/", "/mcpx", "/mcp/x", "/oauth", "/oauth/", "/api/v1/system/info",
	                             "/index.html", "/xmcp", "/.well-knownx/a", "/a/mcp", "/xwell-known/a" };
	for (size_t i = 0; i < sizeof(kept) / sizeof(kept[0]); ++i)
	{
		INFO(kept[i]);
		CHECK_FALSE(std::regex_search(std::string(kept[i]), rule));
	}
	CHECK(g.snippet.find("  - hostname: tv.example.org\n") != std::string::npos);
	CHECK(g.snippet.find("    service: http://192.168.1.20\n") != std::string::npos);
	CHECK(g.snippet.compare(g.snippet.size() - 29, 29, "  - service: http_status:404\n") == 0);
	REQUIRE(g.warnings.size() == 2);
	CHECK(g.warnings[1] == "cloudflare-ratelimit");
}

TEST_CASE("the funnel mounts the three paths and nothing else", "[exposure]")
{
	GuidePlace p = place();
	p.public_url = "https://box.tail1234.ts.net";
	const std::vector<TunnelGuide> all = tunnelGuides(p);
	const TunnelGuide &g = tunnel(all, "tailscale");
	CHECK(g.snippet ==
	      "tailscale funnel --bg --https=443 --set-path=/mcp http://192.168.1.20/mcp\n"
	      "tailscale funnel --bg --https=443 --set-path=/oauth http://192.168.1.20/oauth\n"
	      "tailscale funnel --bg --https=443 --set-path=/.well-known http://192.168.1.20/.well-known\n");
	REQUIRE(g.warnings.size() == 1);
	CHECK(g.warnings[0] == "device");

	p.public_url = "https://tv.example.org:8081";
	const std::vector<TunnelGuide> odd_all = tunnelGuides(p);
	const TunnelGuide &odd = tunnel(odd_all, "tailscale");
	REQUIRE(odd.warnings.size() == 3);
	CHECK(odd.warnings[1] == "tailscale-name");
	CHECK(odd.warnings[2] == "tailscale-port");
	CHECK(odd.snippet.find("--https=8081 ") != std::string::npos);
}

TEST_CASE("the caddy block forwards the three prefixes and answers 404 to the rest", "[exposure]")
{
	const std::vector<TunnelGuide> all = tunnelGuides(place());
	const TunnelGuide &g = tunnel(all, "caddy");
	CHECK(g.file == "Caddyfile");
	CHECK(g.snippet ==
	      "tv.example.org {\n"
	      "\t@ai path /mcp /oauth/* /.well-known/*\n"
	      "\thandle @ai {\n"
	      "\t\treverse_proxy http://192.168.1.20\n"
	      "\t}\n"
	      "\thandle {\n"
	      "\t\trespond 404\n"
	      "\t}\n"
	      "}\n");

	GuidePlace p = place();
	p.public_url = "https://tv.example.org:8443";
	const std::vector<TunnelGuide> odd = tunnelGuides(p);
	CHECK(tunnel(odd, "caddy").snippet.compare(0, 21, "tv.example.org:8443 {") == 0);
}

TEST_CASE("the nginx block forwards three locations and answers 404 to the rest", "[exposure]")
{
	const std::vector<TunnelGuide> all = tunnelGuides(place());
	const TunnelGuide &g = tunnel(all, "nginx");
	CHECK(count(g.snippet, "proxy_pass ") == 5);
	const std::string zone = "limit_req_zone $binary_remote_addr zone=ni_signin:1m rate=10r/m;\n";
	CHECK(g.snippet.compare(0, zone.size(), zone) == 0);
	CHECK(g.snippet.find("\tlocation = /oauth/register {\n\t\tlimit_req zone=ni_signin burst=5 nodelay;\n") != std::string::npos);
	CHECK(g.snippet.find("\tlocation = /oauth/authorize {\n\t\tlimit_req zone=ni_signin burst=5 nodelay;\n") != std::string::npos);
	CHECK(count(g.snippet, "limit_req ") == 2);
	CHECK(count(g.snippet, "\t\tlimit_req_status 429;\n") == 2);
	CHECK(g.snippet.find("\tlocation = /mcp {\n") != std::string::npos);
	CHECK(g.snippet.find("\tlocation /oauth/ {\n") != std::string::npos);
	CHECK(g.snippet.find("\tlocation /.well-known/ {\n") != std::string::npos);
	CHECK(g.snippet.find("\tserver_name tv.example.org;\n") != std::string::npos);
	CHECK(g.snippet.find("\tlisten 443 ssl;\n") != std::string::npos);
	CHECK(count(g.snippet, "proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;") == 5);
	const std::string tail = "\tlocation / {\n\t\treturn 404;\n\t}\n}\n";
	CHECK(g.snippet.compare(g.snippet.size() - tail.size(), tail.size(), tail) == 0);
	REQUIRE(g.warnings.size() == 2);
	CHECK(g.warnings[1] == "nginx-acme");
}

TEST_CASE("the dyndns guide puts caddy behind a port forward and warns about the box", "[exposure]")
{
	const std::vector<TunnelGuide> all = tunnelGuides(place());
	const TunnelGuide &g = tunnel(all, "dyndns");
	CHECK(g.ready);
	CHECK(g.file == "Caddyfile");
	CHECK(g.snippet == tunnel(all, "caddy").snippet);
	REQUIRE(g.warnings.size() == 3);
	CHECK(g.warnings[0] == "device");
	CHECK(g.warnings[1] == "dyndns-fixed-address");
	CHECK(g.warnings[2] == "dyndns-no-direct");
}

TEST_CASE("a snippet forwards only the paths the box names", "[exposure]")
{
	GuidePlace p = place();
	p.paths.resize(1);
	const std::vector<TunnelGuide> one = tunnelGuides(p);
	for (size_t i = 0; i < one.size(); ++i)
	{
		INFO(one[i].id);
		CHECK(one[i].snippet.find("oauth") == std::string::npos);
		CHECK(one[i].snippet.find("well-known") == std::string::npos);
		CHECK(one[i].ready);
	}
	p.paths.clear();
	const std::vector<TunnelGuide> none = tunnelGuides(p);
	for (size_t i = 0; i < none.size(); ++i)
		CHECK_FALSE(none[i].ready);
}

TEST_CASE("an unset address leaves a token in its place and nothing ready", "[exposure]")
{
	GuidePlace p = place();
	p.public_url.clear();
	p.box.clear();
	const std::vector<TunnelGuide> t = tunnelGuides(p);
	for (size_t i = 0; i < t.size(); ++i)
	{
		INFO(t[i].id);
		CHECK_FALSE(t[i].ready);
		CHECK(t[i].snippet.find("BOX-ADDRESS") != std::string::npos);
		if (t[i].id != "tailscale")
			CHECK(t[i].snippet.find("YOUR-DOMAIN") != std::string::npos);
	}
	const std::vector<ClientGuide> c = clientGuides(p);
	CHECK_FALSE(client(c, "claude").ready);
	CHECK(client(c, "claude").url == "https://YOUR-DOMAIN/mcp");
	CHECK_FALSE(client(c, "claude-code").ready);
	CHECK(client(c, "claude-code").url == "http://BOX-ADDRESS/mcp");

	p = place();
	p.box.clear();
	const std::vector<TunnelGuide> nobox = tunnelGuides(p);
	for (size_t i = 0; i < nobox.size(); ++i)
		CHECK_FALSE(nobox[i].ready);
	CHECK(client(clientGuides(p), "claude").ready);

	p = place();
	p.enabled = false;
	const std::vector<TunnelGuide> off = tunnelGuides(p);
	for (size_t i = 0; i < off.size(); ++i)
		CHECK_FALSE(off[i].ready);
	CHECK_FALSE(client(clientGuides(p), "claude").ready);
	CHECK_FALSE(client(clientGuides(p), "home-assistant").ready);
}

TEST_CASE("each client is given the address and the credential it reaches the box with", "[exposure]")
{
	const std::vector<ClientGuide> c = clientGuides(place());
	CHECK(client(c, "claude").needs == "public");
	CHECK(client(c, "claude").url == "https://tv.example.org/mcp");
	CHECK(client(c, "claude").command.empty());
	CHECK(client(c, "claude").ready);
	CHECK(client(c, "chatgpt").needs == "public");
	CHECK(client(c, "chatgpt").url == "https://tv.example.org/mcp");
	CHECK(client(c, "claude-code").needs == "token");
	CHECK(client(c, "claude-code").url == "http://192.168.1.20/mcp");
	CHECK(client(c, "claude-code").command ==
	      "claude mcp add --transport http neutrino http://192.168.1.20/mcp --header \"Authorization: Bearer YOUR-TOKEN\"");
	CHECK(client(c, "claude-code").ready);
	CHECK(client(c, "home-assistant").needs == "token");
	CHECK(client(c, "home-assistant").url == "http://192.168.1.20/mcp");
	CHECK(client(c, "home-assistant").command.empty());
	CHECK(client(c, "home-assistant").ready);

	GuidePlace p = place();
	p.allow_lan = false;
	CHECK_FALSE(client(clientGuides(p), "claude-code").ready);
	CHECK_FALSE(client(clientGuides(p), "home-assistant").ready);
	CHECK(client(clientGuides(p), "claude").ready);
}

TEST_CASE("the guides route fills the snippets from the settings and the page's own address", "[exposure]")
{
	InstalledDependencies wired;
	httpd::WebConfig c = httpd::defaultWebConfig();
	c.ai_enabled = true;
	c.ai_named = true;
	c.ai_public_url = "https://tv.example.org";
	httpd::setConfigForTest(c);

	const httpd::Response r = httpd::dispatch(httpd::Get, "/api/v1/ai/guides", "", "", "192.168.1.9",
	                                          httpd::AuthLevel::System, "", "", "192.168.1.20:8081",
	                                          "", httpd::Origin::Lan);
	REQUIRE(r.code == 200);
	CHECK(r.body.find("{\"mcp_url\":\"https://tv.example.org/mcp\",\"lan_mcp_url\":\"http://192.168.1.20:8081/mcp\",") == 0);
	CHECK(r.body.find("\"paths\":[\"/mcp\",\"/oauth/\",\"/.well-known/\"]") != std::string::npos);
	CHECK(r.body.find("\"id\":\"cloudflare\"") != std::string::npos);
	CHECK(r.body.find("{\"id\":\"home-assistant\",\"needs\":\"token\",\"url\":\"http://192.168.1.20:8081/mcp\",\"command\":\"\",\"ready\":true}") != std::string::npos);
	CHECK(r.body.find("--header \\\"Authorization: Bearer YOUR-TOKEN\\\"") != std::string::npos);
	CHECK(r.body.find("reverse_proxy http://192.168.1.20:8081\\n") != std::string::npos);
	CHECK(r.body.find("\"warnings\":[\"device\",\"nginx-acme\"]") != std::string::npos);

	const httpd::Response nameless = httpd::dispatch(httpd::Get, "/api/v1/ai/guides", "", "", "192.168.1.9",
	                                                 httpd::AuthLevel::System, "", "", "",
	                                                 "", httpd::Origin::Lan);
	REQUIRE(nameless.code == 200);
	CHECK(nameless.body.find("\"lan_mcp_url\":\"\",") != std::string::npos);

	CHECK(httpd::dispatch(httpd::Get, "/api/v1/ai/guides", "", "", "192.168.1.9",
	                      httpd::AuthLevel::Write).code == 403);

	httpd::setConfigForTest(httpd::defaultWebConfig());
}
