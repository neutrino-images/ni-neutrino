/*
 * test_server.cpp - tests for the server itself
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
#include "httpd/server.h"
#include "httpd/webconfig.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace
{

// A failed check unwinds past whatever follows it, so the descriptor is given
// up by leaving the scope. A case that leaked a bound port on its way out would
// take the port with it for the rest of the run.
struct HeldSocket
{
	int fd;

	explicit HeldSocket(int f) : fd(f) {}
	~HeldSocket()
	{
		if (fd >= 0)
			::close(fd);
	}

private:
	HeldSocket(const HeldSocket &);
	HeldSocket &operator=(const HeldSocket &);
};

} // namespace

TEST_CASE("the defaults are the ones the box is configured with", "[httpd]")
{
	// Every other case overwrites the two it cares about and compares none of
	// them, and the daemon accepts a zero for the three it is handed, so a
	// default quietly emptied would turn nothing else red.
	const httpd::ServerConfig c = httpd::defaultConfig();
	REQUIRE(c.port == 80);
	REQUIRE(c.bind_address == "0.0.0.0");
	REQUIRE(c.thread_pool == 4u);
	REQUIRE(c.connection_limit == 64u);
	REQUIRE(c.connection_timeout_s == 30u);
	REQUIRE(c.max_body_bytes == (size_t) (1u << 20));
}

TEST_CASE("the server binds, answers and stops", "[httpd]")
{
	// Only to get past the check the server makes before it starts.
	InstalledDependencies wired;

	httpd::ServerConfig c = httpd::defaultConfig();
	c.port = 0;                        // let the kernel choose
	c.bind_address = "127.0.0.1";

	REQUIRE(httpd::start(c));
	REQUIRE(httpd::running());
	const int port = httpd::boundPort();
	REQUIRE(port > 0);

	testhttp::Reply r = testhttp::request(port, "GET", "/nothing-is-here");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 404);

	httpd::stop();
	REQUIRE_FALSE(httpd::running());

	testhttp::Reply gone = testhttp::request(port, "GET", "/nothing-is-here");
	REQUIRE_FALSE(gone.transport_ok);
}

TEST_CASE("the server binds an IPv6 address and answers on it", "[httpd]")
{
	InstalledDependencies wired;

	// The field offers an address of either family, so the one that is not the
	// default has to be reached by something rather than only offered.
	httpd::ServerConfig c = httpd::defaultConfig();
	c.port = 0;
	c.bind_address = "::1";

	// Catch2 here has no way to skip, and a case that cannot run must not be
	// written as one that passes, so on a kernel without IPv6 this goes red.
	// The note is so that the red line says which of the two it is.
	INFO("this needs IPv6 on the machine running it: /proc/net/if_inet6 lists ::1 when there is");
	REQUIRE(httpd::start(c));
	REQUIRE(httpd::running());
	const int port = httpd::boundPort();
	REQUIRE(port > 0);

	testhttp::Reply r = testhttp::requestOn("::1", port, "GET", "/nothing-is-here");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 404);

	httpd::stop();
	REQUIRE_FALSE(httpd::running());
}

TEST_CASE("a port the protocol has no room for is refused", "[httpd]")
{
	// Wired up, so that the refusals below are refusals about the port.
	InstalledDependencies wired;

	httpd::ServerConfig c = httpd::defaultConfig();
	c.bind_address = "127.0.0.1";

	// Cast into sixteen bits these become 8081 and 65535, and the daemon would
	// bind one of those and report it as the port that was asked for.
	c.port = 73617;
	REQUIRE_FALSE(httpd::start(c));
	REQUIRE_FALSE(httpd::running());

	c.port = -1;
	REQUIRE_FALSE(httpd::start(c));
	REQUIRE_FALSE(httpd::running());

	c.port = 65536;
	REQUIRE_FALSE(httpd::start(c));
	REQUIRE_FALSE(httpd::running());
}

TEST_CASE("an address that is not one is refused", "[httpd]")
{
	// Wired up, so that the refusal below is a refusal about the address.
	InstalledDependencies wired;

	httpd::ServerConfig c = httpd::defaultConfig();
	c.port = 0;
	c.bind_address = "localhost";

	// A name rather than a literal, which nothing here resolves.
	REQUIRE_FALSE(httpd::start(c));
	REQUIRE_FALSE(httpd::running());
}

TEST_CASE("stopping twice is not an error", "[httpd]")
{
	InstalledDependencies wired;

	// The daemon is started first, so that the first stop is one that really
	// tears a daemon down and only the second takes the early return. Without
	// it both calls would leave immediately and nothing anywhere would stop a
	// running daemon twice.
	httpd::ServerConfig c = httpd::defaultConfig();
	c.port = 0;
	c.bind_address = "127.0.0.1";
	REQUIRE(httpd::start(c));
	REQUIRE(httpd::running());

	httpd::stop();
	REQUIRE_FALSE(httpd::running());

	httpd::stop();
	REQUIRE_FALSE(httpd::running());
}

TEST_CASE("a port already taken is reported, not asserted", "[httpd]")
{
	/* The listener is opened here rather than by a second daemon of this
	   server's own, because a second one is refused for there already being one
	   and would answer the case without the bind ever being attempted. Nothing
	   of this server runs for the length of the case, and the seams it reads
	   through are filled, which together leave the failed bind as the only
	   thing that can produce the answer. */
	InstalledDependencies wired;

	REQUIRE_FALSE(httpd::running());

	HeldSocket listener(::socket(AF_INET, SOCK_STREAM, 0));
	REQUIRE(listener.fd >= 0);

	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = 0;
	REQUIRE(::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);
	REQUIRE(::bind(listener.fd, (const struct sockaddr *) &addr, sizeof(addr)) == 0);
	REQUIRE(::listen(listener.fd, 1) == 0);

	struct sockaddr_in bound;
	socklen_t bound_len = sizeof(bound);
	std::memset(&bound, 0, sizeof(bound));
	REQUIRE(::getsockname(listener.fd, (struct sockaddr *) &bound, &bound_len) == 0);
	const int taken = (int) ntohs(bound.sin_port);
	REQUIRE(taken > 0);

	httpd::ServerConfig c = httpd::defaultConfig();
	c.port = taken;
	c.bind_address = "127.0.0.1";

	// The box may already be running something on the port, and that is a
	// reason to go without a server rather than to end the program.
	REQUIRE_FALSE(httpd::start(c));
	REQUIRE_FALSE(httpd::running());
}

TEST_CASE("starting a second time while one runs is refused", "[httpd]")
{
	InstalledDependencies wired;

	httpd::ServerConfig c = httpd::defaultConfig();
	c.port = 0;
	c.bind_address = "127.0.0.1";

	REQUIRE(httpd::start(c));
	const int first = httpd::boundPort();

	// One at a time, because stopping names no daemon and a second one would
	// leave the first with nothing that can reach it.
	REQUIRE_FALSE(httpd::start(c));
	REQUIRE(httpd::running());
	REQUIRE(httpd::boundPort() == first);

	httpd::stop();
	REQUIRE_FALSE(httpd::running());
}

TEST_CASE("a server the program has not wired up does not start", "[httpd]")
{
	/* Nothing is installed here, which is the state of the program before its startup
	   has run the installs. Every other case in this file fills them, so this is the one
	   that holds the guard.

	   It matters because the accessors behind those seams end the process rather than
	   answering: a daemon that came up here would answer its first read of the box by
	   taking the program down, and the port would be open for as long as it took
	   somebody to send one. */
	REQUIRE_FALSE(coreapi::dependenciesInstalled());
	REQUIRE_FALSE(httpd::running());

	httpd::ServerConfig c = httpd::defaultConfig();
	c.port = 0;
	c.bind_address = "127.0.0.1";

	REQUIRE_FALSE(httpd::start(c));
	REQUIRE_FALSE(httpd::running());
	REQUIRE(httpd::boundPort() == 0);

	// The same configuration with the seams filled, so that the refusal above
	// is shown to be about them and not about the configuration.
	{
		InstalledDependencies wired;
		REQUIRE(coreapi::dependenciesInstalled());
		REQUIRE(httpd::start(c));
		REQUIRE(httpd::running());
		httpd::stop();
	}
	REQUIRE_FALSE(httpd::running());
}

namespace
{

/* A port nothing is on, found by asking the kernel for one and giving it back.
   Between the two another program could take it, which is why the case below
   reports a bind that failed rather than asserting the port was free: the same
   reason the case above it gives. */
int aFreePort()
{
	const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return 0;

	HeldSocket held(fd);
	struct sockaddr_in a;
	std::memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	a.sin_port = 0;
	if (::bind(fd, (struct sockaddr *) &a, sizeof(a)) != 0)
		return 0;

	socklen_t len = sizeof(a);
	if (::getsockname(fd, (struct sockaddr *) &a, &len) != 0)
		return 0;
	return (int) ntohs(a.sin_port);
}

} // namespace

TEST_CASE("the answer that says where the box is going reaches the caller first", "[httpd]")
{
	/* The one case that measures the order rather than describing it.

	   Putting this server on a new configuration stops the daemon, so a route that did
	   it before answering would cut the connection it is answering on: the caller that
	   asked to move the port would be left without the answer saying where the box went.
	   Every other case about this route reads a field of a response; this one reads a
	   socket, and a server that moved too early leaves the reply short. */
	InstalledDependencies wired;

	const int first = aFreePort();
	const int second = aFreePort();
	REQUIRE(first > 0);
	REQUIRE(second > 0);
	REQUIRE(first != second);

	char name[160];
	std::snprintf(name, sizeof(name), "/tmp/ni-webmove-%d", (int) getpid());
	{
		std::FILE *f = std::fopen(name, "w");
		REQUIRE(f != NULL);
		std::fprintf(f, "port=%d\nbind_address=127.0.0.1\nusername=root\n", first);
		REQUIRE(std::fclose(f) == 0);
	}

	const httpd::WebConfig before = httpd::config();
	REQUIRE(httpd::load(name));
	REQUIRE(httpd::config().server.port == first);
	REQUIRE(httpd::start(httpd::config().server));
	REQUIRE(httpd::boundPort() == first);

	/* A bearer token rather than a session, because a bearer needs no second
	   token and what this case is about is the order of two things and not what
	   the gate asks for. Drawn after the read above, which empties the table
	   this puts it in. */
	const std::string token = httpd::randomToken();
	httpd::addApiToken(httpd::tokenLookupPrefix(token), httpd::hashSecret(token, 2000),
	                   httpd::AuthLevel::System);

	std::vector<std::pair<std::string, std::string> > head;
	head.push_back(std::make_pair(std::string("Content-Type"), std::string("application/json")));
	head.push_back(std::make_pair(std::string("Authorization"), "Bearer " + token));

	char body[64];
	std::snprintf(body, sizeof(body), "{\"port\":%d}", second);

	const testhttp::Reply moved =
		testhttp::request(first, "PUT", "/api/v1/system/webserver", head, body);
	REQUIRE(moved.transport_ok);
	REQUIRE(moved.code == 200);
	REQUIRE(moved.body.find("port_moved") != std::string::npos);

	/* And then it goes, on a thread of its own, which is why this waits rather
	   than asserting straight away. A ceiling of ten seconds because a case that
	   waits for ever is a case that hangs a build. */
	bool there = false;
	for (int i = 0; i < 200 && !there; ++i)
	{
		::usleep(50 * 1000);
		there = httpd::running() && httpd::boundPort() == second;
	}
	REQUIRE(there);

	const testhttp::Reply now = testhttp::request(second, "GET", "/nothing-is-here");
	REQUIRE(now.transport_ok);
	REQUIRE(now.code == 404);

	const testhttp::Reply old = testhttp::request(first, "GET", "/nothing-is-here");
	REQUIRE_FALSE(old.transport_ok);

	httpd::stop();
	httpd::forgetApiTokens();
	httpd::setConfigForTest(before);
	::unlink(name);
}
