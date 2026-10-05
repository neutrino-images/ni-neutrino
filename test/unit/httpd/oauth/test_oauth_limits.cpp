/*
 * test_oauth_limits.cpp - per source limits on the anonymous OAuth endpoints
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
#include "support/httpclient.h"
#include "httpd/oauth/authorize.h"
#include "httpd/oauth/oauthtest.h"
#include "httpd/oauth/registration.h"
#include "httpd/oauth/sourcelimit.h"
#include "httpd/oauth/surface.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using namespace httpd;
using namespace httpd::oauth;

namespace
{

typedef std::vector<std::pair<std::string, std::string> > Headers;

struct Limits
{
	Limits()
	{
		forgetSourceLimitsForTest();
		resetRegistrationLimitForTest();
		setSourceClockForTest(5000);
	}
	~Limits()
	{
		setSourceClockForTest(0);
		forgetSourceLimitsForTest();
	}
};

Exchange tunnel(Method m, const char *path, const std::string &source)
{
	Exchange x;
	x.method = m;
	x.path = path;
	x.origin = Origin::Tunnel;
	x.base = kPublicUrl;
	x.peer = "192.168.1.5";
	x.source = source;
	// Refused before any work, so only the limit is measured.
	x.content_type = "text/plain";
	return x;
}

std::string header(const Response &r, const char *name)
{
	for (size_t i = 0; i < r.headers.size(); ++i)
	{
		if (r.headers[i].first == name)
			return r.headers[i].second;
	}
	return std::string();
}

// How many in a row are answered before the first 429.
int answeredBeforeLimit(Method m, const char *path, const std::string &source)
{
	for (int i = 0; i < 100; ++i)
	{
		if (answer(tunnel(m, path, source)).code == StatusTooManyRequests)
			return i;
	}
	return 100;
}

} // namespace

TEST_CASE("registrations are limited per source address", "[oauth-limits]")
{
	Limits l;
	CHECK(answeredBeforeLimit(Post, "/oauth/register", "198.51.100.1") == (int) kRegisterPerSource);
	const Response r = answer(tunnel(Post, "/oauth/register", "198.51.100.1"));
	CHECK(r.code == StatusTooManyRequests);
	CHECK(r.body.find("temporarily_unavailable") != std::string::npos);
	CHECK(header(r, "Retry-After") == "60");
	CHECK(answer(tunnel(Post, "/oauth/register", "198.51.100.2")).code != StatusTooManyRequests);

	setSourceClockForTest(5030);
	CHECK(header(answer(tunnel(Post, "/oauth/register", "198.51.100.1")), "Retry-After") == "30");
	setSourceClockForTest(5060);
	CHECK(answer(tunnel(Post, "/oauth/register", "198.51.100.1")).code != StatusTooManyRequests);
}

TEST_CASE("authorize requests are limited per source address", "[oauth-limits]")
{
	Limits l;
	CHECK(answeredBeforeLimit(Get, "/oauth/authorize", "203.0.113.7") == (int) kAuthorizePerSource);
	const Response r = answer(tunnel(Get, "/oauth/authorize", "203.0.113.7"));
	CHECK(r.code == StatusTooManyRequests);
	CHECK(header(r, "Retry-After") == "60");
	CHECK(header(r, "X-Frame-Options") == "DENY");
	CHECK(answer(tunnel(Get, "/oauth/authorize", "203.0.113.8")).code != StatusTooManyRequests);
}

TEST_CASE("consent posts are limited per source address and the page is not", "[oauth-limits]")
{
	Limits l;
	CHECK(answeredBeforeLimit(Post, "/oauth/consent", "203.0.113.7") == (int) kConsentPerSource);
	CHECK(header(answer(tunnel(Post, "/oauth/consent", "203.0.113.7")), "Retry-After") == "60");
	CHECK(answeredBeforeLimit(Get, "/oauth/consent", "203.0.113.7") == 100);
}

TEST_CASE("the three limits are kept apart", "[oauth-limits]")
{
	Limits l;
	CHECK(answeredBeforeLimit(Post, "/oauth/register", "203.0.113.7") == (int) kRegisterPerSource);
	CHECK(answeredBeforeLimit(Get, "/oauth/authorize", "203.0.113.7") == (int) kAuthorizePerSource);
}

TEST_CASE("a request without a forwarded address is limited under its peer", "[oauth-limits]")
{
	Limits l;
	CHECK(answeredBeforeLimit(Post, "/oauth/register", "") == (int) kRegisterPerSource);
	Exchange other = tunnel(Post, "/oauth/register", "");
	other.peer = "192.168.1.6";
	CHECK(answer(other).code != StatusTooManyRequests);
}

TEST_CASE("an IPv6 source is counted by its /64 and a mapped address as the IPv4 one", "[oauth-limits]")
{
	Limits l;
	CHECK(answeredBeforeLimit(Post, "/oauth/register", "2001:db8:1:2::1") == (int) kRegisterPerSource);
	CHECK(answer(tunnel(Post, "/oauth/register", "2001:db8:1:2:ffff::9")).code == StatusTooManyRequests);
	CHECK(answer(tunnel(Post, "/oauth/register", "2001:db8:1:3::1")).code != StatusTooManyRequests);

	CHECK(answeredBeforeLimit(Post, "/oauth/register", "198.51.100.9") == (int) kRegisterPerSource);
	CHECK(answer(tunnel(Post, "/oauth/register", "::ffff:198.51.100.9")).code == StatusTooManyRequests);
}

TEST_CASE("the table of sources stays bounded and drops the oldest window", "[oauth-limits]")
{
	Limits l;
	for (int i = 0; i < (int) kRegisterPerSource; ++i)
		answer(tunnel(Post, "/oauth/register", "198.51.100.1"));
	CHECK(answer(tunnel(Post, "/oauth/register", "198.51.100.1")).code == StatusTooManyRequests);

	setSourceClockForTest(5001);
	char source[32];
	for (size_t i = 0; i < kMaxSources; ++i)
	{
		std::snprintf(source, sizeof(source), "10.1.%u.%u", (unsigned) (i / 256), (unsigned) (i % 256));
		answer(tunnel(Post, "/oauth/register", source));
	}
	CHECK(sourceCountForTest() == kMaxSources);
	CHECK(answer(tunnel(Post, "/oauth/register", "198.51.100.1")).code != StatusTooManyRequests);
}

TEST_CASE("the forwarded address of a tunnel request is the source the limit counts", "[oauth-limits]")
{
	Limits l;
	TunnelConfigured config;
	RunningServer srv;
	const Headers a(1, std::make_pair(std::string("X-Forwarded-For"), std::string("203.0.113.7")));
	const Headers b(1, std::make_pair(std::string("X-Forwarded-For"), std::string("203.0.113.8")));
	int answered = 0;
	while (answered < 100 && testhttp::request(srv.port, "GET", "/oauth/authorize", a).code != StatusTooManyRequests)
		++answered;
	CHECK(answered == (int) kAuthorizePerSource);
	CHECK(testhttp::request(srv.port, "GET", "/oauth/authorize", b).code == StatusBadRequest);
}
