/*
 * test_mcp_ratelimit.cpp - tests for the MCP endpoint's limits
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

#include "httpd/mcp/limits.h"
#include "httpd/mcp/ratelimit.h"

#include <string>

namespace
{

struct RateCase
{
	RateCase(unsigned burst, unsigned per_minute, size_t clients)
	{
		httpd::mcp::Limits l = httpd::mcp::defaultLimits();
		l.rate_burst = burst;
		l.rate_per_minute = per_minute;
		l.max_rate_clients = clients;
		httpd::mcp::setLimits(l);
		httpd::mcp::forgetRatesForTest();
		httpd::mcp::setRateClockForTest(1000);
	}
	~RateCase()
	{
		httpd::mcp::setLimits(httpd::mcp::defaultLimits());
		httpd::mcp::forgetRatesForTest();
		httpd::mcp::setRateClockForTest(0);
	}
};

} // namespace

TEST_CASE("the limits the box runs with", "[mcp]")
{
	const httpd::mcp::Limits l = httpd::mcp::defaultLimits();
	REQUIRE(l.max_body_bytes == 65536u);
	REQUIRE(l.max_json_depth == 32u);
	REQUIRE(l.rate_burst == 30u);
	REQUIRE(l.rate_per_minute == 60u);
	REQUIRE(l.max_rate_clients == 256u);
	REQUIRE(l.call_timeout_ms == 15000u);
	REQUIRE(l.max_running_calls == 4u);
}

TEST_CASE("the limits set are the limits read", "[mcp]")
{
	httpd::mcp::Limits l = httpd::mcp::defaultLimits();
	l.call_timeout_ms = 123;
	httpd::mcp::setLimits(l);
	REQUIRE(httpd::mcp::limits().call_timeout_ms == 123u);
	httpd::mcp::setLimits(httpd::mcp::defaultLimits());
	REQUIRE(httpd::mcp::limits().call_timeout_ms == 15000u);
}

TEST_CASE("a client may send a burst and then one a second", "[mcp]")
{
	RateCase rc(3, 60, 256);
	unsigned retry = 99;
	REQUIRE(httpd::mcp::rateAllows("a", &retry));
	REQUIRE(retry == 0);
	REQUIRE(httpd::mcp::rateAllows("a", &retry));
	REQUIRE(httpd::mcp::rateAllows("a", &retry));
	REQUIRE_FALSE(httpd::mcp::rateAllows("a", &retry));
	REQUIRE(retry == 1);
	httpd::mcp::setRateClockForTest(1001);
	REQUIRE(httpd::mcp::rateAllows("a", &retry));
	REQUIRE_FALSE(httpd::mcp::rateAllows("a", &retry));
}

TEST_CASE("a partial refill never lifts the credit past the burst", "[mcp]")
{
	RateCase rc(3, 60, 256);
	REQUIRE(httpd::mcp::rateAllows("a", NULL));
	httpd::mcp::setRateClockForTest(1002);
	REQUIRE(httpd::mcp::rateAllows("a", NULL));
	REQUIRE(httpd::mcp::rateAllows("a", NULL));
	REQUIRE(httpd::mcp::rateAllows("a", NULL));
	REQUIRE_FALSE(httpd::mcp::rateAllows("a", NULL));
}

TEST_CASE("a slower rate asks for a longer wait rounded up", "[mcp]")
{
	RateCase rc(1, 7, 256);
	unsigned retry = 0;
	REQUIRE(httpd::mcp::rateAllows("a", &retry));
	REQUIRE_FALSE(httpd::mcp::rateAllows("a", &retry));
	// 60 units at 7 a second is 8.6 s; 8 would be refused again.
	REQUIRE(retry == 9);
	httpd::mcp::setRateClockForTest(1004);
	REQUIRE_FALSE(httpd::mcp::rateAllows("a", &retry));
	REQUIRE(retry == 5);
}

TEST_CASE("clients do not share an allowance", "[mcp]")
{
	RateCase rc(1, 60, 256);
	REQUIRE(httpd::mcp::rateAllows("a", NULL));
	REQUIRE_FALSE(httpd::mcp::rateAllows("a", NULL));
	REQUIRE(httpd::mcp::rateAllows("b", NULL));
}

TEST_CASE("a rate of nought turns the limit off", "[mcp]")
{
	RateCase rc(1, 0, 256);
	for (int i = 0; i < 100; ++i)
		REQUIRE(httpd::mcp::rateAllows("a", NULL));
}

TEST_CASE("the table of clients stays bounded and forgets the quietest first", "[mcp]")
{
	RateCase rc(1, 6, 2);
	REQUIRE(httpd::mcp::rateAllows("a", NULL));
	httpd::mcp::setRateClockForTest(1001);
	REQUIRE(httpd::mcp::rateAllows("b", NULL));
	httpd::mcp::setRateClockForTest(1002);
	REQUIRE(httpd::mcp::rateAllows("c", NULL));
	REQUIRE(httpd::mcp::rateClientsForTest() == 2);
	REQUIRE_FALSE(httpd::mcp::rateAllows("b", NULL));
	REQUIRE(httpd::mcp::rateAllows("a", NULL));
}

TEST_CASE("a clock that jumps neither locks a client out nor overflows", "[mcp]")
{
	RateCase rc(2, 60, 256);
	unsigned retry = 0;
	httpd::mcp::setRateClockForTest(10);
	REQUIRE(httpd::mcp::rateAllows("a", &retry));
	REQUIRE(httpd::mcp::rateAllows("a", &retry));
	REQUIRE_FALSE(httpd::mcp::rateAllows("a", &retry));

	httpd::mcp::setRateClockForTest(1790000000);
	REQUIRE(httpd::mcp::rateAllows("a", &retry));
	REQUIRE(httpd::mcp::rateAllows("a", &retry));
	REQUIRE_FALSE(httpd::mcp::rateAllows("a", &retry));
	REQUIRE(retry == 1);

	httpd::mcp::setRateClockForTest(20);
	REQUIRE_FALSE(httpd::mcp::rateAllows("a", &retry));
	REQUIRE(retry == 1);
	httpd::mcp::setRateClockForTest(21);
	REQUIRE(httpd::mcp::rateAllows("a", &retry));
}
