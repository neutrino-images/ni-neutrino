/*
 * test_exposure.cpp - the rules the exposure gate decides by
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
#include "httpd/endpoint.h"
#include "httpd/mcp/exposure.h"
#include "httpd/netmatch.h"

#include <string>
#include <vector>

namespace
{

std::string canonical(const std::string &in)
{
	std::string out = "stale";
	std::string why;
	REQUIRE(httpd::exposure::normalisePublicUrl(in, &out, &why));
	return out;
}

bool urlRefused(const std::string &in)
{
	std::string out = "stale";
	std::string why;
	const bool ok = httpd::exposure::normalisePublicUrl(in, &out, &why);
	if (!ok)
	{
		CHECK(out.empty());
		CHECK_FALSE(why.empty());
	}
	return !ok;
}

std::string proxies(const std::string &in)
{
	std::vector<httpd::NetPrefix> out;
	std::string why;
	REQUIRE(httpd::exposure::readTrustedProxies(in, &out, &why));
	return httpd::exposure::trustedProxyText(out);
}

bool proxiesRefused(const std::string &in)
{
	std::vector<httpd::NetPrefix> out(1);
	std::string why;
	const bool ok = httpd::exposure::readTrustedProxies(in, &out, &why);
	if (!ok)
	{
		CHECK(out.empty());
		CHECK_FALSE(why.empty());
	}
	return !ok;
}

} // namespace

TEST_CASE("a public address is kept in one spelling", "[exposure]")
{
	CHECK(canonical("") == "");
	CHECK(canonical("https://tv.example.org") == "https://tv.example.org");
	CHECK(canonical("HTTPS://TV.Example.ORG/") == "https://tv.example.org");
	CHECK(canonical("https://tv.example.org:443") == "https://tv.example.org");
	CHECK(canonical("https://tv.example.org:8443/") == "https://tv.example.org:8443");
	CHECK(canonical("https://box-1.tail1234.ts.net") == "https://box-1.tail1234.ts.net");
	CHECK(canonical("https://9tv.example.org") == "https://9tv.example.org");
}

TEST_CASE("a public address that is not a bare https origin is refused", "[exposure]")
{
	const char *const bad[] = {
		"http://tv.example.org", "tv.example.org", "https://", "https:///",
		"https://tv.example.org//", "https://tv.example.org/mcp", "https://tv.example.org/?a=b",
		"https://tv.example.org#x", "https://user@tv.example.org", "https://tv.example.org:0",
		"https://tv.example.org:65536", "https://tv.example.org:0443", "https://tv.example.org:",
		"https://tv.example.org:443:1", "https://localhost", "https://192.168.1.20",
		"https://[::1]", "https://tv..example.org", "https://-tv.example.org",
		"https://tv-.example.org", "https://tv.example.org.", "https://tv_x.example.org",
		"https://tv.example.org\\x", "https://tv.ex%61mple.org", " https://tv.example.org",
		"https://tv.example.org ", "https://t\xc3\xa4v.example.org", "https://tv.example.123"
	};
	size_t compared = 0;
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		INFO(bad[i]);
		CHECK(urlRefused(bad[i]));
		++compared;
	}
	CHECK(compared == 28);
	CHECK(urlRefused(std::string("https://tv.example.org\0x", 24)));
	CHECK(urlRefused("https://" + std::string(64, 'a') + ".example.org"));
	CHECK(urlRefused("https://" + std::string(300, 'a')));
}

TEST_CASE("a trusted proxy is a host or a narrow network", "[exposure]")
{
	CHECK(proxies("") == "");
	CHECK(proxies("  ") == "");
	CHECK(proxies("192.168.1.5") == "192.168.1.5/32");
	CHECK(proxies(" 192.168.1.5 , 10.0.0.0/24") == "192.168.1.5/32,10.0.0.0/24");
	CHECK(proxies("10.0.0.9/24") == "10.0.0.0/24");
	CHECK(proxies("::ffff:192.168.1.5") == "192.168.1.5/32");
	CHECK(proxies("2001:db8::7") == "2001:db8::7/128");
	CHECK(proxies("2001:db8:1:2::/64") == "2001:db8:1:2::/64");
	CHECK(proxies("fd00::1/64") == "fd00::/64");

	std::string sixteen;
	for (int i = 1; i <= 16; ++i)
		sixteen += (i == 1 ? "" : ",") + std::string("10.0.0.") + std::to_string(i);
	CHECK_FALSE(proxiesRefused(sixteen));
	CHECK(proxiesRefused(sixteen + ",10.0.0.17"));
}

TEST_CASE("a trusted proxy list that would hand the tunnel too much is refused", "[exposure]")
{
	const char *const bad[] = {
		"127.0.0.1", "127.0.0.0/24", "::1", "::", "0.0.0.0", "0.0.0.1/32",
		"::ffff:127.0.0.1", "10.0.0.0/23", "0.0.0.0/0", "2001:db8::/63", "::/0",
		"192.168.1.5,", ",192.168.1.5", "192.168.1.5,,10.0.0.1", "192.168.1.256",
		"host.example.org", "192.168.1.5/", "192.168.1.5/33", "fe80::1%eth0"
	};
	size_t compared = 0;
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		INFO(bad[i]);
		CHECK(proxiesRefused(bad[i]));
		++compared;
	}
	CHECK(compared == 19);
	CHECK(proxiesRefused(std::string("192.168.1.5\0", 12)));
}

namespace
{

httpd::NetPrefix net(const char *text)
{
	httpd::NetPrefix p;
	REQUIRE(httpd::parsePrefix(text, &p));
	return p;
}

httpd::exposure::Policy exposed()
{
	httpd::exposure::Policy p = httpd::exposure::closedPolicy();
	p.enabled = true;
	p.strict = true;
	p.allow_lan = true;
	p.public_url = "https://tv.example.org";
	p.tunnels.push_back(net("192.168.1.5/32"));
	p.tunnels.push_back(net("2001:db8::5/128"));
	p.web_proxies.push_back(net("192.168.1.7/32"));
	return p;
}

httpd::exposure::Seen seen(const std::string &peer, const std::string &xff = std::string(),
                           bool forwarding = false)
{
	httpd::exposure::Seen s;
	s.peer = peer;
	s.forwarded_for = xff;
	s.carries_forwarding = forwarding;
	return s;
}

} // namespace

using httpd::Origin;
using httpd::exposure::Admit;
using httpd::exposure::admit;
using httpd::exposure::classify;

TEST_CASE("a request handlers build without a transport counts as the far side", "[exposure]")
{
	httpd::Request r;
	CHECK(r.origin() == Origin::Tunnel);
	r.setOrigin(Origin::Lan);
	CHECK(r.origin() == Origin::Lan);
}

TEST_CASE("the closed policy admits no tunnel and refuses no forwarder", "[exposure]")
{
	const httpd::exposure::Policy p = httpd::exposure::closedPolicy();
	CHECK_FALSE(p.enabled);
	CHECK_FALSE(p.strict);
	CHECK_FALSE(p.allow_lan);
	CHECK(p.tunnels.empty());
	CHECK(classify(seen("192.168.1.6", "203.0.113.9", true), p) == Origin::Lan);
}

TEST_CASE("the peer alone makes a request a tunnel request", "[exposure]")
{
	const httpd::exposure::Policy p = exposed();
	CHECK(classify(seen("192.168.1.5"), p) == Origin::Tunnel);
	CHECK(classify(seen("::ffff:192.168.1.5"), p) == Origin::Tunnel);
	CHECK(classify(seen("2001:db8::5"), p) == Origin::Tunnel);
	CHECK(classify(seen("192.168.1.5", "192.168.1.20", true), p) == Origin::Tunnel);
	CHECK(classify(seen("192.168.1.5", "garbage", true), p) == Origin::Tunnel);
	CHECK(classify(seen("192.168.1.6"), p) == Origin::Lan);
	CHECK(classify(seen("::ffff:192.168.1.6"), p) == Origin::Lan);
	CHECK(classify(seen("2001:db8::6"), p) == Origin::Lan);
	CHECK(classify(seen("127.0.0.1"), p) == Origin::Lan);
	CHECK(classify(seen("::1"), p) == Origin::Lan);
}

TEST_CASE("a forwarded header names neither a tunnel nor a local caller", "[exposure]")
{
	httpd::exposure::Policy p = exposed();
	CHECK(classify(seen("192.168.1.6", "192.168.1.5", true), p) == Origin::Refused);
	CHECK(classify(seen("192.168.1.6", "", true), p) == Origin::Refused);
	CHECK(classify(seen("127.0.0.1", "203.0.113.9", true), p) == Origin::Refused);
	CHECK(classify(seen("::1", "", true), p) == Origin::Refused);
	CHECK(classify(seen("::ffff:192.168.1.6", "203.0.113.9", true), p) == Origin::Refused);
	CHECK(classify(seen("192.168.1.7", "203.0.113.9", true), p) == Origin::Lan);
	CHECK(classify(seen("::ffff:192.168.1.7", "203.0.113.9", true), p) == Origin::Lan);
	p.web_proxies.push_back(net("192.168.1.5/32"));
	CHECK(classify(seen("192.168.1.5", "203.0.113.9", true), p) == Origin::Tunnel);
	p.strict = false;
	CHECK(classify(seen("192.168.1.6", "192.168.1.5", true), p) == Origin::Lan);
}

TEST_CASE("an unreadable peer is refused while the surface is configured", "[exposure]")
{
	httpd::exposure::Policy p = exposed();
	CHECK(classify(seen(""), p) == Origin::Refused);
	p.strict = false;
	CHECK(classify(seen(""), p) == Origin::Lan);
}

TEST_CASE("the forwarded client is the last address the tunnel appended", "[exposure]")
{
	CHECK(httpd::exposure::forwardedClient(seen("192.168.1.5", "198.51.100.1, 203.0.113.9", true)) == "203.0.113.9");
	CHECK(httpd::exposure::forwardedClient(seen("192.168.1.5", "203.0.113.9:4711", true)) == "203.0.113.9");
	CHECK(httpd::exposure::forwardedClient(seen("192.168.1.5", "[2001:db8::9]:443", true)) == "2001:db8::9");
	CHECK(httpd::exposure::forwardedClient(seen("192.168.1.5", "198.51.100.1, unknown", true)) == "");
	CHECK(httpd::exposure::forwardedClient(seen("192.168.1.5", "", false)) == "");
}

TEST_CASE("a tunnel reaches the three prefixes and nothing else", "[exposure]")
{
	const char *const allowed[] = {
		"/mcp", "/oauth/token", "/oauth/authorize", "/oauth/register",
		"/.well-known/oauth-authorization-server", "/.well-known/oauth-protected-resource/mcp"
	};
	for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); ++i)
	{
		INFO(allowed[i]);
		CHECK(httpd::exposure::tunnelPathAllowed(allowed[i]));
	}

	const char *const hidden[] = {
		"", "mcp", "/", "/mcp/", "/mcp/x", "/MCP", "/mcpx", "//mcp", "/oauth", "/oauth/",
		"/oauthx/token", "/.well-known", "/.well-known/", "/.well-knownx/a", "/%6dcp",
		"/oauth/%2e%2e/api/v1/settings", "/oauth/../api/v1/settings", "/oauth/./token",
		"/.well-known/../index.html", "/oauth//token", "/oauth/token/", "/oauth\\token",
		"/api/v1/ai/settings", "/control/info", "/index.html", "/user.css", "/oauth/to;ken"
	};
	size_t compared = 0;
	for (size_t i = 0; i < sizeof(hidden) / sizeof(hidden[0]); ++i)
	{
		INFO(hidden[i]);
		CHECK_FALSE(httpd::exposure::tunnelPathAllowed(hidden[i]));
		++compared;
	}
	CHECK(compared == 27);
	CHECK_FALSE(httpd::exposure::tunnelPathAllowed(std::string("/mcp\0", 5)));

	size_t n = 0;
	const char *const *paths = httpd::exposure::tunnelPaths(&n);
	REQUIRE(n == 3);
	CHECK(std::string(paths[0]) == "/mcp");
	CHECK(std::string(paths[1]) == "/oauth/");
	CHECK(std::string(paths[2]) == "/.well-known/");
}

TEST_CASE("an AI path is named by its decoded first segment", "[exposure]")
{
	const char *const ai[] = { "/mcp", "/mcp/x", "/%6dcp", "/%6Dcp", "/oauth", "/oauth/token",
	                           "/o%61uth/token", "/.well-known/x", "/%2Ewell-known/x" };
	for (size_t i = 0; i < sizeof(ai) / sizeof(ai[0]); ++i)
	{
		INFO(ai[i]);
		CHECK(httpd::exposure::isAiPath(ai[i]));
	}
	const char *const other[] = { "", "/", "//mcp", "/mcpx", "/api/v1/ai/settings", "/index.html",
	                              "/%6d", "/x/mcp" };
	for (size_t i = 0; i < sizeof(other) / sizeof(other[0]); ++i)
	{
		INFO(other[i]);
		CHECK_FALSE(httpd::exposure::isAiPath(other[i]));
	}
}

TEST_CASE("the gate passes mcp to the LAN only when the owner allows it", "[exposure]")
{
	httpd::exposure::Policy p = exposed();
	CHECK(admit(Origin::Lan, "/mcp", p) == Admit::Pass);
	p.allow_lan = false;
	CHECK(admit(Origin::Lan, "/mcp", p) == Admit::Hidden);
	p = exposed();
	p.enabled = false;
	CHECK(admit(Origin::Lan, "/mcp", p) == Admit::Hidden);
	p = exposed();
	p.public_url.clear();
	CHECK(admit(Origin::Lan, "/mcp", p) == Admit::Pass);

	p = exposed();
	const char *const spelt[] = { "/mcp/", "/mcp/x", "/%6dcp", "/%6Dcp" };
	for (size_t i = 0; i < sizeof(spelt) / sizeof(spelt[0]); ++i)
	{
		INFO(spelt[i]);
		CHECK(admit(Origin::Lan, spelt[i], p) == Admit::Hidden);
	}
}

TEST_CASE("the gate never passes oauth or discovery to the LAN", "[exposure]")
{
	const httpd::exposure::Policy p = exposed();
	const char *const hidden[] = {
		"/oauth/token", "/oauth/authorize", "/oauth", "/o%61uth/token", "/%6fauth/token",
		"/.well-known/oauth-authorization-server", "/.well-known/oauth-protected-resource/mcp",
		"/%2Ewell-known/x"
	};
	size_t compared = 0;
	for (size_t i = 0; i < sizeof(hidden) / sizeof(hidden[0]); ++i)
	{
		INFO(hidden[i]);
		CHECK(admit(Origin::Lan, hidden[i], p) == Admit::Hidden);
		++compared;
	}
	CHECK(compared == 8);
}

TEST_CASE("the gate passes everything else to the LAN whatever the AI settings say", "[exposure]")
{
	httpd::exposure::Policy p = exposed();
	const char *const lan[] = { "/api/v1/ai/settings", "/api/v1/ai/guides", "/api/v1/system/info",
	                            "/index.html", "/mcpx", "//mcp", "/control/info" };
	for (int round = 0; round < 2; ++round)
	{
		for (size_t i = 0; i < sizeof(lan) / sizeof(lan[0]); ++i)
		{
			INFO(lan[i]);
			CHECK(admit(Origin::Lan, lan[i], p) == Admit::Pass);
		}
		p = httpd::exposure::closedPolicy();
	}
}

TEST_CASE("the gate passes the three AI prefixes to the tunnel only when the box is published", "[exposure]")
{
	httpd::exposure::Policy p = exposed();
	CHECK(admit(Origin::Tunnel, "/mcp", p) == Admit::Pass);
	CHECK(admit(Origin::Tunnel, "/oauth/token", p) == Admit::Pass);
	CHECK(admit(Origin::Tunnel, "/.well-known/oauth-authorization-server", p) == Admit::Pass);
	p.allow_lan = false;
	CHECK(admit(Origin::Tunnel, "/mcp", p) == Admit::Pass);

	p = exposed();
	p.enabled = false;
	CHECK(admit(Origin::Tunnel, "/mcp", p) == Admit::Hidden);
	CHECK(admit(Origin::Tunnel, "/oauth/token", p) == Admit::Hidden);
	CHECK(admit(Origin::Tunnel, "/.well-known/oauth-authorization-server", p) == Admit::Hidden);

	p = exposed();
	p.public_url.clear();
	CHECK(admit(Origin::Tunnel, "/mcp", p) == Admit::Hidden);
	CHECK(admit(Origin::Tunnel, "/oauth/token", p) == Admit::Hidden);
	CHECK(admit(Origin::Tunnel, "/.well-known/oauth-authorization-server", p) == Admit::Hidden);
}

TEST_CASE("the gate hides everything but the three prefixes from the tunnel", "[exposure]")
{
	const httpd::exposure::Policy p = exposed();
	const char *const hidden[] = { "/api/v1/ai/settings", "/api/v1/ai/guides", "/api/v1/system/info",
	                               "/index.html", "/control/info", "/%6dcp", "/mcp/x", "//mcp" };
	for (size_t i = 0; i < sizeof(hidden) / sizeof(hidden[0]); ++i)
	{
		INFO(hidden[i]);
		CHECK(admit(Origin::Tunnel, hidden[i], p) == Admit::Hidden);
	}
}

TEST_CASE("the gate refuses every path to an unlisted forwarder", "[exposure]")
{
	const httpd::exposure::Policy p = exposed();
	const char *const all[] = { "/mcp", "/oauth/token", "/.well-known/oauth-authorization-server",
	                            "/api/v1/ai/settings", "/api/v1/system/info", "/index.html" };
	for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i)
	{
		INFO(all[i]);
		CHECK(admit(Origin::Refused, all[i], p) == Admit::Refused);
	}
	CHECK(admit(static_cast<Origin>(7), "/api/v1/system/info", p) == Admit::Hidden);
}
