/*
 * test_counts.cpp - the figures a run is held to, read from one file or two
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
#include "support/counts.h"

#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <unistd.h>

namespace
{

std::string writeTemp(const char *body)
{
	char name[] = "/tmp/counts-XXXXXX";
	const int fd = mkstemp(name);
	REQUIRE(fd >= 0);
	const size_t n = std::strlen(body);
	REQUIRE(write(fd, body, n) == (ssize_t) n);
	close(fd);
	return name;
}

} // namespace

TEST_CASE("an overlay replaces a figure and adds one and leaves the rest", "[counts]")
{
	const std::string base = writeTemp("# base\nalpha\t3\nbeta\t5\n");
	const std::string over = writeTemp("# over\nbeta\t7\ngamma\t1\n");
	std::map<std::string, size_t> got;
	std::string why;
	REQUIRE(readExpectedCounts(base.c_str(), over.c_str(), got, why));
	REQUIRE(got.size() == 3);
	REQUIRE(got["alpha"] == 3);
	REQUIRE(got["beta"] == 7);
	REQUIRE(got["gamma"] == 1);
	unlink(base.c_str());
	unlink(over.c_str());
}

TEST_CASE("without an overlay the base is the whole of it", "[counts]")
{
	const std::string base = writeTemp("alpha\t3\n");
	std::map<std::string, size_t> got;
	std::string why;
	REQUIRE(readExpectedCounts(base.c_str(), NULL, got, why));
	REQUIRE(got.size() == 1);
	REQUIRE(got["alpha"] == 3);
	unlink(base.c_str());
}

TEST_CASE("an overlay that cannot be read is a failure and not an empty overlay", "[counts]")
{
	const std::string base = writeTemp("alpha\t3\n");
	std::map<std::string, size_t> got;
	std::string why;
	REQUIRE_FALSE(readExpectedCounts(base.c_str(), "/nonexistent/counts-mcp.txt", got, why));
	REQUIRE(why.find("/nonexistent/counts-mcp.txt") != std::string::npos);
	unlink(base.c_str());
}
