/*
 * test_exposure_gate.cpp - the exposure gate, over a socket
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
#include "support/httpclient.h"
#include "httpd/auth.h"
#include "httpd/credentials.h"
#include "httpd/endpoint.h"
#include "httpd/mcp/exposure.h"
#include "httpd/mcp/mcpfakes.h"
#include "httpd/netmatch.h"
#include "httpd/router.h"
#include "httpd/server.h"
#include "httpd/webconfig.h"

#include <string>
#include <utility>
#include <vector>

namespace
{

typedef std::vector<std::pair<std::string, std::string> > Headers;

httpd::NetPrefix net(const char *text)
{
	httpd::NetPrefix p;
	REQUIRE(httpd::parsePrefix(text, &p));
	return p;
}

httpd::WebConfig exposedConfig(const char *tunnel)
{
	httpd::WebConfig c = httpd::defaultWebConfig();
	c.ai_enabled = true;
	c.ai_named = true;
	c.ai_allow_lan = true;
	c.ai_public_url = "https://tv.example.org";
	c.ai_trusted_proxies.push_back(net(tunnel));
	return c;
}

struct Exposed
{
	InstalledDependencies wired;
	int port;

	Exposed(const httpd::WebConfig &c, const char *bind, size_t max_body) : port(0)
	{
		httpd::setConfigForTest(c);
		httpd::ServerConfig s = httpd::defaultConfig();
		s.port = 0;
		s.bind_address = bind;
		s.max_body_bytes = max_body;
		REQUIRE(httpd::start(s));
		port = httpd::boundPort();
	}

	~Exposed()
	{
		httpd::stop();
		httpd::setRoutesForTest(NULL);
		httpd::setConfigForTest(httpd::defaultWebConfig());
	}
};

std::string concrete(const char *pattern)
{
	const std::string p(pattern);
	std::string out;
	size_t i = 0;
	while (i < p.size())
	{
		if (p[i] == '{')
		{
			const size_t close = p.find('}', i);
			out += "1";
			i = (close == std::string::npos) ? p.size() : close + 1;
		}
		else
		{
			out += p[i];
			++i;
		}
	}
	return out;
}

httpd::Response probe(const httpd::Request &r)
{
	httpd::Response out;
	out.code = httpd::StatusOk;
	out.content_type = "text/plain";
	const char *what = "refused";
	if (r.origin() == httpd::Origin::Lan)
		what = "lan";
	else if (r.origin() == httpd::Origin::Tunnel)
		what = "tunnel";
	out.body = std::string(what) + " " + r.reportedPeer();
	return out;
}

// Under a tunnel prefix the OAuth server does not claim.
const char kTunnelProbe[] = "/.well-known/ni-probe";

const httpd::Endpoint kProbeEndpoints[] = {
	{ httpd::Get, kTunnelProbe, httpd::AuthLevel::Read, "a probe under a tunnel prefix", NULL,
	  NULL, 0, NULL, &probe, false, httpd::Answers200, HTTPD_NO_REFUSALS },
	{ httpd::Get, "/api/v1/probe", httpd::AuthLevel::Read, "a probe outside them", NULL,
	  NULL, 0, NULL, &probe, false, httpd::Answers200, HTTPD_NO_REFUSALS },
};

const httpd::RouteTable kProbeTable = { HTTPD_TABLE("probe", kProbeEndpoints) };

Headers cookie(const std::string &token)
{
	return Headers(1, std::make_pair(std::string("Cookie"),
	                                 std::string(httpd::sessionCookieName()) + "=" + token));
}

// 1: the gate answered 404; 0: the request went past the gate; -1: anything else.
int gateAnswer(int port, const char *path)
{
	const size_t before = httpd::exposure::turnedAwayForTest();
	const testhttp::Reply r = testhttp::request(port, "GET", path);
	const size_t after = httpd::exposure::turnedAwayForTest();
	if (!r.transport_ok)
		return -1;
	if (after == before)
		return 0;
	return (after == before + 1 && r.code == 404) ? 1 : -1;
}

} // namespace

TEST_CASE("the router hands the origin it was given to the handler", "[exposure]")
{
	const httpd::Response asis = httpd::dispatchIn(kProbeTable, httpd::Get, kTunnelProbe, "", "",
	                                               "192.0.2.1", httpd::AuthLevel::Read);
	CHECK(asis.body == "tunnel 192.0.2.1");
	const httpd::Response lan = httpd::dispatchIn(kProbeTable, httpd::Get, kTunnelProbe, "", "",
	                                              "192.0.2.1", httpd::AuthLevel::Read, "", "", "", "",
	                                              httpd::Origin::Lan);
	CHECK(lan.body == "lan 192.0.2.1");
}

TEST_CASE("a loopback tunnel entry installed for a test is honoured", "[exposure]")
{
	InstalledDependencies wired;
	httpd::WebConfig c = exposedConfig("127.0.0.1/32");
	c.ai_trusted_proxies.push_back(net("::1/128"));
	httpd::setConfigForTest(c);

	const httpd::exposure::Policy p = httpd::exposure::policyFrom(httpd::config());
	httpd::exposure::Seen s;
	s.peer = "127.0.0.1";
	s.carries_forwarding = false;
	CHECK(httpd::exposure::classify(s, p) == httpd::Origin::Tunnel);
	s.peer = "::1";
	CHECK(httpd::exposure::classify(s, p) == httpd::Origin::Tunnel);

	std::vector<httpd::NetPrefix> read;
	std::string why;
	CHECK_FALSE(httpd::exposure::readTrustedProxies("127.0.0.1,::1", &read, &why));

	httpd::setConfigForTest(httpd::defaultWebConfig());
}

TEST_CASE("a tunnel caller reaches no route of any table outside the three prefixes", "[exposure]")
{
	Exposed box(exposedConfig("127.0.0.1/32"), "127.0.0.1", 1u << 20);

	size_t table_count = 0;
	const httpd::RouteTable *const *tables = httpd::allRoutes(&table_count);
	size_t expected = 0;
	for (size_t t = 0; t < table_count; ++t)
		expected += tables[t]->count;
	REQUIRE(expected > 0);

	size_t hidden = 0;
	size_t passed = 0;
	for (size_t t = 0; t < table_count; ++t)
	{
		for (size_t e = 0; e < tables[t]->count; ++e)
		{
			const httpd::Endpoint &ep = tables[t]->endpoints[e];
			const std::string path = concrete(ep.path);
			const bool carries = ep.method == httpd::Post || ep.method == httpd::Put ||
			                     ep.method == httpd::Patch;
			const size_t before = httpd::exposure::turnedAwayForTest();
			const testhttp::Reply r = testhttp::request(box.port, httpd::methodName(ep.method), path,
			                                            Headers(), carries ? "{}" : "");
			INFO(httpd::methodName(ep.method) << " " << path);
			REQUIRE(r.transport_ok);
			if (httpd::exposure::tunnelPathAllowed(path))
			{
				CHECK(httpd::exposure::turnedAwayForTest() == before);
				++passed;
			}
			else
			{
				CHECK(r.code == 404);
				CHECK(httpd::exposure::turnedAwayForTest() == before + 1);
				++hidden;
			}
		}
	}
	CHECK(hidden + passed == expected);
	CHECK(hidden > 0);
}

TEST_CASE("a tunnel caller reaches no page and no spelling around the prefixes", "[exposure]")
{
	// Wired, so /mcp is claimed by its mount and passes.
	mcpfake::Wired wired;
	Exposed box(exposedConfig("127.0.0.1/32"), "127.0.0.1", 1u << 20);

	const char *const hidden[] = {
		"/", "/index.html", "/user.css", "/swagger/", "/control/info", "/control/exec?a=b",
		"/mcp/../api/v1/system/info", "/oauth/%2e%2e/api/v1/system/info",
		"/.well-known/../index.html", "//mcp", "/%6dcp", "/oauth", "/MCP",
		"/api/v1/ai/settings", "/channels/12ab", "/.well-known/security.txt"
	};
	size_t compared = 0;
	for (size_t i = 0; i < sizeof(hidden) / sizeof(hidden[0]); ++i)
	{
		INFO(hidden[i]);
		CHECK(gateAnswer(box.port, hidden[i]) == 1);
		++compared;
	}
	CHECK(compared == 16);

	const char *const passed[] = { "/mcp", "/oauth/token", "/.well-known/oauth-authorization-server" };
	for (size_t i = 0; i < sizeof(passed) / sizeof(passed[0]); ++i)
	{
		INFO(passed[i]);
		CHECK(gateAnswer(box.port, passed[i]) == 0);
	}
}

TEST_CASE("a hidden route answers 404 before a body over the ceiling is judged", "[exposure]")
{
	const std::string big(4096, 'x');
	{
		Exposed box(exposedConfig("127.0.0.1/32"), "127.0.0.1", 1024);
		CHECK(testhttp::request(box.port, "PUT", "/api/v1/system/webserver", Headers(), big).code == 404);
	}
	{
		Exposed box(exposedConfig("192.0.2.1/32"), "127.0.0.1", 1024);
		CHECK(testhttp::request(box.port, "PUT", "/api/v1/system/webserver", Headers(), big).code == 413);
	}
}

TEST_CASE("a tunnel caller reaches no route under a prefix that no mount claims", "[exposure]")
{
	httpd::setRoutesForTest(&kProbeTable);
	Exposed box(exposedConfig("127.0.0.1/32"), "127.0.0.1", 1u << 20);

	CHECK(gateAnswer(box.port, kTunnelProbe) == 1);

	const std::string token = httpd::openSession("root");
	REQUIRE_FALSE(token.empty());
	Headers h = cookie(token);
	h.push_back(std::make_pair(std::string("X-Forwarded-For"), std::string("198.51.100.1, 203.0.113.9")));
	const size_t before = httpd::exposure::turnedAwayForTest();
	const testhttp::Reply in = testhttp::request(box.port, "GET", kTunnelProbe, h);
	CHECK(in.code == 404);
	CHECK(httpd::exposure::turnedAwayForTest() == before + 1);
	CHECK(in.body.find("tunnel") == std::string::npos);
	httpd::closeSession(token);
}

TEST_CASE("a local caller reaches the router with the read its network gets", "[exposure]")
{
	httpd::setRoutesForTest(&kProbeTable);
	Exposed box(exposedConfig("192.0.2.1/32"), "127.0.0.1", 1u << 20);
	const testhttp::Reply r = testhttp::request(box.port, "GET", "/api/v1/probe");
	CHECK(r.code == 200);
	CHECK(r.body == "lan 127.0.0.1");
}

TEST_CASE("a local caller never reaches oauth or discovery", "[exposure]")
{
	httpd::setRoutesForTest(&kProbeTable);
	Exposed box(exposedConfig("192.0.2.1/32"), "127.0.0.1", 1u << 20);
	const char *const hidden[] = { kTunnelProbe, "/oauth/token", "/%6fauth/token",
	                               "/.well-known/oauth-authorization-server", "/mcp/x", "/%6dcp" };
	for (size_t i = 0; i < sizeof(hidden) / sizeof(hidden[0]); ++i)
	{
		INFO(hidden[i]);
		CHECK(gateAnswer(box.port, hidden[i]) == 1);
	}
	CHECK(gateAnswer(box.port, "/mcp") == 0);
	CHECK(gateAnswer(box.port, "/api/v1/ai/settings") == 0);
}

TEST_CASE("switching the surface or the local network off hides mcp and nothing else", "[exposure]")
{
	httpd::WebConfig c = exposedConfig("192.0.2.1/32");
	c.ai_allow_lan = false;
	{
		httpd::setRoutesForTest(&kProbeTable);
		Exposed box(c, "127.0.0.1", 1u << 20);
		CHECK(gateAnswer(box.port, "/mcp") == 1);
		CHECK(testhttp::request(box.port, "GET", "/api/v1/probe").code == 200);
		CHECK(gateAnswer(box.port, "/api/v1/ai/settings") == 0);
	}
	c.ai_allow_lan = true;
	c.ai_enabled = false;
	{
		httpd::setRoutesForTest(&kProbeTable);
		Exposed box(c, "127.0.0.1", 1u << 20);
		CHECK(gateAnswer(box.port, "/mcp") == 1);
		CHECK(testhttp::request(box.port, "GET", "/api/v1/probe").code == 200);
		CHECK(gateAnswer(box.port, "/api/v1/ai/settings") == 0);
	}
}

TEST_CASE("a forwarded header from a peer nobody listed is refused once the surface is named", "[exposure]")
{
	httpd::setRoutesForTest(&kProbeTable);
	Exposed box(exposedConfig("192.0.2.1/32"), "127.0.0.1", 1u << 20);

	const char *const names[] = { "X-Forwarded-For", "Forwarded", "X-Real-IP", "x-forwarded-host" };
	const char *const paths[] = { "/api/v1/probe", "/mcp", "/oauth/token" };
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
	{
		for (size_t k = 0; k < sizeof(paths) / sizeof(paths[0]); ++k)
		{
			INFO(names[i] << " " << paths[k]);
			const size_t before = httpd::exposure::turnedAwayForTest();
			const testhttp::Reply r = testhttp::request(box.port, "GET", paths[k],
				Headers(1, std::make_pair(std::string(names[i]), std::string("192.0.2.1"))));
			CHECK(r.code == 403);
			CHECK(r.body.find("forwarded-by-untrusted-peer") != std::string::npos);
			CHECK(httpd::exposure::turnedAwayForTest() == before + 1);
		}
	}
	CHECK(testhttp::request(box.port, "GET", "/api/v1/probe").code == 200);
}

TEST_CASE("a box that never named the surface ignores forwarded headers as before", "[exposure]")
{
	httpd::setRoutesForTest(&kProbeTable);
	Exposed box(httpd::defaultWebConfig(), "127.0.0.1", 1u << 20);
	const testhttp::Reply r = testhttp::request(box.port, "GET", "/api/v1/probe",
		Headers(1, std::make_pair(std::string("X-Forwarded-For"), std::string("203.0.113.9"))));
	CHECK(r.code == 200);
	CHECK(r.body == "lan 127.0.0.1");
}

TEST_CASE("a proxy the web interface already trusts may forward to it", "[exposure]")
{
	httpd::WebConfig c = exposedConfig("192.0.2.1/32");
	c.trusted_proxies.push_back(net("127.0.0.1/32"));
	c.trusted_proxies_named = true;
	httpd::setRoutesForTest(&kProbeTable);
	Exposed box(c, "127.0.0.1", 1u << 20);
	const std::string token = httpd::openSession("root");
	Headers h = cookie(token);
	h.push_back(std::make_pair(std::string("X-Forwarded-For"), std::string("203.0.113.9")));
	const testhttp::Reply r = testhttp::request(box.port, "GET", "/api/v1/probe", h);
	CHECK(r.code == 200);
	CHECK(r.body == "lan 203.0.113.9");
	httpd::closeSession(token);
}

TEST_CASE("a tunnel peer of the second family is a tunnel peer", "[exposure]")
{
	Exposed box(exposedConfig("::1/128"), "::1", 1u << 20);
	const size_t before = httpd::exposure::turnedAwayForTest();
	const testhttp::Reply r = testhttp::requestOn("::1", box.port, "GET", "/api/v1/system/info");
	CHECK(r.code == 404);
	CHECK(httpd::exposure::turnedAwayForTest() == before + 1);
}

TEST_CASE("the base address of a tunnel caller comes from the settings", "[exposure]")
{
	InstalledDependencies wired;
	httpd::WebConfig c = httpd::defaultWebConfig();
	c.ai_public_url = "https://tv.example.org";
	httpd::setConfigForTest(c);

	CHECK(httpd::exposure::baseUrl(httpd::Origin::Tunnel, "evil.example.net") == "https://tv.example.org");
	CHECK(httpd::exposure::baseUrl(httpd::Origin::Tunnel, "") == "https://tv.example.org");
	CHECK(httpd::exposure::baseUrl(httpd::Origin::Refused, "x.example.net") == "");

	c.ai_public_url.clear();
	httpd::setConfigForTest(c);
	CHECK(httpd::exposure::baseUrl(httpd::Origin::Tunnel, "evil.example.net") == "");

	httpd::setConfigForTest(httpd::defaultWebConfig());
}

TEST_CASE("the base address of a local caller is the authority it named", "[exposure]")
{
	CHECK(httpd::exposure::baseUrl(httpd::Origin::Lan, "box.example.net") == "http://box.example.net");
	CHECK(httpd::exposure::baseUrl(httpd::Origin::Lan, "192.168.1.20:8081") == "http://192.168.1.20:8081");
	CHECK(httpd::exposure::baseUrl(httpd::Origin::Lan, "[fd00::20]:80") == "http://[fd00::20]:80");
	CHECK(httpd::exposure::baseUrl(httpd::Origin::Lan, "a b") == "");
	CHECK(httpd::exposure::baseUrl(httpd::Origin::Lan, "x/y") == "");
	CHECK(httpd::exposure::baseUrl(httpd::Origin::Lan, "x@y") == "");
	CHECK(httpd::exposure::baseUrl(httpd::Origin::Lan, "") == "");
	CHECK(httpd::exposure::baseUrl(httpd::Origin::Lan, std::string(262, 'a')) == "");
}

TEST_CASE("the base address of a request is the one its origin and authority give", "[exposure]")
{
	InstalledDependencies wired;
	httpd::WebConfig c = httpd::defaultWebConfig();
	c.ai_public_url = "https://tv.example.org";
	httpd::setConfigForTest(c);

	httpd::Request r;
	r.setHost("evil.example.net");
	CHECK(httpd::exposure::baseUrl(r) == "https://tv.example.org");
	r.setOrigin(httpd::Origin::Lan);
	CHECK(httpd::exposure::baseUrl(r) == "http://evil.example.net");
	r.setHost("x/y");
	CHECK(httpd::exposure::baseUrl(r) == "");
	r.setHost("x.example.net");
	r.setOrigin(httpd::Origin::Refused);
	CHECK(httpd::exposure::baseUrl(r) == "");

	httpd::setConfigForTest(httpd::defaultWebConfig());
}

TEST_CASE("the tunnel stays shut while the box login is the shipped password", "[exposure]")
{
	mcpfake::Wired wired;
	httpd::WebConfig c = exposedConfig("127.0.0.1/32");
	c.username = "root";
	c.password_hash = httpd::hashSecret("ni", 1000);
	Exposed box(c, "127.0.0.1", 1u << 20);

	const char *const paths[] = { "/mcp", "/oauth/token", "/.well-known/oauth-authorization-server" };
	for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
	{
		INFO(paths[i]);
		CHECK(gateAnswer(box.port, paths[i]) == 1);
	}

	// A changed password opens it on the configuration in effect, the server running on.
	c.password_hash = httpd::hashSecret("sofa-2026", 1000);
	httpd::setConfigForTest(c);
	for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
	{
		INFO(paths[i]);
		CHECK(gateAnswer(box.port, paths[i]) == 0);
	}
}

TEST_CASE("the shipped password leaves the local network as it was", "[exposure]")
{
	httpd::WebConfig c = exposedConfig("192.0.2.1/32");
	c.username = "root";
	c.password_hash = httpd::hashSecret("ni", 1000);
	Exposed box(c, "127.0.0.1", 1u << 20);
	CHECK(gateAnswer(box.port, "/mcp") == 0);
	CHECK(gateAnswer(box.port, "/oauth/token") == 1);
}
