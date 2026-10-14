/*
 * test_defaultroute.cpp - tests for reading the default route
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

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "libnet/libnet.h"

namespace
{

const char kHeader[] = "Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU\tWindow\tIRTT\n";
const char kNoDefault[] = "eth0\t0001A8C0\t00000000\t0001\t0\t0\t0\t00FFFFFF\t0\t0\t0\n";
const char kDefault[] = "wlan0\t00000000\t0101A8C0\t0003\t0\t0\t600\t00000000\t0\t0\t0\n";
const char kLoopback6[] = "00000000000000000000000000000000 00 00000000000000000000000000000000 00 "
	"00000000000000000000000000000000 ffffffff 00000001 00000000 00200200       lo\n";
const char kDefault6[] = "00000000000000000000000000000000 00 00000000000000000000000000000000 00 "
	"fe800000000000000000000000000001 00000400 00000001 00000000 00000003     eth0\n";

struct TempFile
{
	std::string path;

	explicit TempFile(const std::string &text)
	{
		char name[] = "/tmp/routeXXXXXX";
		const int fd = mkstemp(name);
		REQUIRE(fd >= 0);
		REQUIRE(write(fd, text.data(), text.size()) == (ssize_t) text.size());
		close(fd);
		path = name;
	}
	~TempFile() { unlink(path.c_str()); }
};

} // namespace

TEST_CASE("the default route is the first entry to every destination", "[libnet][route]")
{
	TempFile table(std::string(kHeader) + kNoDefault + kDefault);
	std::string dev;
	std::string gw;
	REQUIRE(netDefaultRouteIn(table.path.c_str(), dev, gw));
	CHECK(dev == "wlan0");
	// the kernel writes the gateway in host order, and the suite runs little endian
	CHECK(gw == "192.168.1.1");

	TempFile none(std::string(kHeader) + kNoDefault);
	CHECK_FALSE(netDefaultRouteIn(none.path.c_str(), dev, gw));
	CHECK(dev.empty());
	CHECK(gw.empty());
	CHECK_FALSE(netDefaultRouteIn("/nonexistent/route", dev, gw));
}

// A box that is online over IPv6 alone is still online, the loopback's catch-all entry is not.
TEST_CASE("a default route over either protocol counts and the loopback's does not", "[libnet][route]")
{
	TempFile none4(std::string(kHeader) + kNoDefault);
	TempFile with4(std::string(kHeader) + kDefault);
	TempFile lo6(kLoopback6);
	TempFile with6(std::string(kLoopback6) + kDefault6);

	CHECK(netHaveDefaultRouteIn(with4.path.c_str(), "/nonexistent/route6"));
	CHECK(netHaveDefaultRouteIn(none4.path.c_str(), with6.path.c_str()));
	CHECK_FALSE(netHaveDefaultRouteIn(none4.path.c_str(), lo6.path.c_str()));
	CHECK_FALSE(netHaveDefaultRouteIn("/nonexistent/route", "/nonexistent/route6"));
}
