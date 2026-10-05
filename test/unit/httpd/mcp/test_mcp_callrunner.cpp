/*
 * test_mcp_callrunner.cpp - tests for running tool calls with a deadline
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

#include "httpd/mcp/callrunner.h"
#include "httpd/mcp/mcpfakes.h"
#include "httpd/mcp/wiring.h"

#include <time.h>

namespace
{

httpd::mcp::Caller reader()
{
	httpd::mcp::Caller c;
	c.client_id = "client-main";
	c.level = httpd::AuthLevel::Read;
	return c;
}

struct SlowFor
{
	explicit SlowFor(unsigned ms) { mcpfake::slowMs() = ms; }
	~SlowFor() { mcpfake::slowMs() = 0; }
};

// Leaves calls admitted again for the cases after a drain.
struct Reopened
{
	~Reopened()
	{
		const httpd::mcp::Wiring w = { &mcpfake::tools(), &mcpfake::verify };
		httpd::mcp::install(w);
		httpd::mcp::uninstall();
	}
};

void ignoreAnswer(void *, const httpd::mcp::CallAnswer &)
{
}

long msSince(const struct timespec &from)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec - from.tv_sec) * 1000L + (now.tv_nsec - from.tv_nsec) / 1000000L;
}

} // namespace

TEST_CASE("a call that returns in time is answered with what it returned", "[mcp]")
{
	const httpd::mcp::CallAnswer a = httpd::mcp::runCall(&mcpfake::tools(), reader(), "echo", "{\"x\":1}", 2000, 4);
	REQUIRE(a.outcome == httpd::mcp::CallOutcome::Done);
	REQUIRE(a.ok);
	REQUIRE(a.value == "{\"x\":1}");
	REQUIRE(mcpfake::waitForCalls());
}

TEST_CASE("a tool's refusal comes back as its error", "[mcp]")
{
	const httpd::mcp::CallAnswer a = httpd::mcp::runCall(&mcpfake::tools(), reader(), "standby", "{}", 2000, 4);
	REQUIRE(a.outcome == httpd::mcp::CallOutcome::Done);
	REQUIRE_FALSE(a.ok);
	REQUIRE_FALSE(a.thrown);
	REQUIRE(a.error.code == coreapi::ErrorCode::BoxInStandby);
	REQUIRE(a.error.message == "the box is in standby");
	REQUIRE(mcpfake::waitForCalls());
}

TEST_CASE("a tool that throws is reported and not passed on", "[mcp]")
{
	const httpd::mcp::CallAnswer a = httpd::mcp::runCall(&mcpfake::tools(), reader(), "throws", "{}", 2000, 4);
	REQUIRE(a.outcome == httpd::mcp::CallOutcome::Done);
	REQUIRE_FALSE(a.ok);
	REQUIRE(a.thrown);
	REQUIRE(mcpfake::waitForCalls());
}

TEST_CASE("a call that does not return in time is given up on and finishes alone", "[mcp]")
{
	SlowFor slow(400);
	const int before = mcpfake::tools().calls();
	const httpd::mcp::CallAnswer a = httpd::mcp::runCall(&mcpfake::tools(), reader(), "slow", "{}", 50, 4);
	REQUIRE(a.outcome == httpd::mcp::CallOutcome::TimedOut);
	REQUIRE_FALSE(a.ok);
	REQUIRE(httpd::mcp::runningCalls() == 1);
	REQUIRE(mcpfake::waitForCalls());
	REQUIRE(mcpfake::tools().calls() == before + 1);
}

TEST_CASE("no new call starts while as many as allowed are still running", "[mcp]")
{
	SlowFor slow(400);
	const httpd::mcp::CallAnswer first = httpd::mcp::runCall(&mcpfake::tools(), reader(), "slow", "{}", 20, 1);
	REQUIRE(first.outcome == httpd::mcp::CallOutcome::TimedOut);

	const int before = mcpfake::tools().calls();
	const httpd::mcp::CallAnswer second = httpd::mcp::runCall(&mcpfake::tools(), reader(), "echo", "{}", 2000, 1);
	REQUIRE(second.outcome == httpd::mcp::CallOutcome::Busy);
	REQUIRE(mcpfake::tools().calls() == before);

	REQUIRE(mcpfake::waitForCalls());
	const httpd::mcp::CallAnswer third = httpd::mcp::runCall(&mcpfake::tools(), reader(), "echo", "{}", 2000, 1);
	REQUIRE(third.outcome == httpd::mcp::CallOutcome::Done);
	REQUIRE(mcpfake::waitForCalls());
}

TEST_CASE("an allowance of nought runs nothing", "[mcp]")
{
	const int before = mcpfake::tools().calls();
	const httpd::mcp::CallAnswer a = httpd::mcp::runCall(&mcpfake::tools(), reader(), "echo", "{}", 2000, 0);
	REQUIRE(a.outcome == httpd::mcp::CallOutcome::Busy);
	REQUIRE(mcpfake::tools().calls() == before);
}

TEST_CASE("a drain returns once the tool call still running has finished", "[mcp][mcp-drain]")
{
	Reopened after;
	SlowFor slow(300);
	const httpd::mcp::CallAnswer a = httpd::mcp::runCall(&mcpfake::tools(), reader(), "slow", "{}", 20, 4);
	REQUIRE(a.outcome == httpd::mcp::CallOutcome::TimedOut);
	REQUIRE(httpd::mcp::runningCalls() == 1);

	CHECK(httpd::mcp::drain(5000));
	CHECK(httpd::mcp::runningCalls() == 0);
	REQUIRE(mcpfake::waitForCalls());
}

TEST_CASE("a drain gives up at its bound on a call that hangs", "[mcp][mcp-drain]")
{
	Reopened after;
	SlowFor slow(1500);
	const httpd::mcp::CallAnswer a = httpd::mcp::runCall(&mcpfake::tools(), reader(), "slow", "{}", 20, 4);
	REQUIRE(a.outcome == httpd::mcp::CallOutcome::TimedOut);

	struct timespec start;
	clock_gettime(CLOCK_MONOTONIC, &start);
	const bool drained = httpd::mcp::drain(200);
	const long took = msSince(start);
	CHECK_FALSE(drained);
	CHECK(took >= 200);
	CHECK(took < 1000);
	CHECK(httpd::mcp::runningCalls() == 1);
	REQUIRE(mcpfake::waitForCalls());
}

TEST_CASE("no call starts after a drain until the next install", "[mcp][mcp-drain]")
{
	Reopened after;
	const httpd::mcp::Wiring w = { &mcpfake::tools(), &mcpfake::verify };
	httpd::mcp::install(w);
	REQUIRE(httpd::mcp::drain(1000));
	CHECK_FALSE(httpd::mcp::installed(NULL));

	const int before = mcpfake::tools().calls();
	const httpd::mcp::CallAnswer a = httpd::mcp::runCall(&mcpfake::tools(), reader(), "echo", "{}", 2000, 4);
	CHECK(a.outcome == httpd::mcp::CallOutcome::Busy);
	CHECK_FALSE(httpd::mcp::startCall(&mcpfake::tools(), reader(), "echo", "{}", 2000, 4,
	                                         &ignoreAnswer, NULL));
	CHECK(mcpfake::tools().calls() == before);

	httpd::mcp::install(w);
	const httpd::mcp::CallAnswer again = httpd::mcp::runCall(&mcpfake::tools(), reader(), "echo", "{}", 2000, 4);
	CHECK(again.outcome == httpd::mcp::CallOutcome::Done);
	httpd::mcp::uninstall();
	REQUIRE(mcpfake::waitForCalls());
}
