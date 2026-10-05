/*
 * test_mcp_server.cpp - tests for the MCP endpoint over a socket
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
#include "httpd/mcp/jsonrpc.h"
#include "httpd/mcp/limits.h"
#include "httpd/mcp/mcpfakes.h"
#include "httpd/netmatch.h"
#include "httpd/server.h"
#include "httpd/webconfig.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

namespace
{

typedef std::vector<std::pair<std::string, std::string> > Headers;

httpd::WebConfig aiConfig(bool tunnel)
{
	httpd::WebConfig c = httpd::defaultWebConfig();
	c.ai_enabled = true;
	c.ai_named = true;
	c.ai_allow_lan = true;
	if (tunnel)
	{
		c.ai_public_url = "https://tv.example.org";
		const char *const proxies[] = { "127.0.0.1/32", "::1/128" };
		for (size_t i = 0; i < sizeof(proxies) / sizeof(proxies[0]); ++i)
		{
			httpd::NetPrefix p;
			REQUIRE(httpd::parsePrefix(proxies[i], &p));
			c.ai_trusted_proxies.push_back(p);
		}
	}
	return c;
}

// Stops the daemon before the configuration and the seams it reads are put back.
struct Serving
{
	InstalledDependencies wired_;

	// A pool of one puts every connection on the thread a request could hold.
	explicit Serving(const httpd::WebConfig &c, unsigned pool = 0)
	{
		httpd::setConfigForTest(c);
		httpd::ServerConfig s = httpd::defaultConfig();
		s.port = 0;
		s.bind_address = "127.0.0.1";
		if (pool != 0)
			s.thread_pool = pool;
		REQUIRE(httpd::start(s));
	}

	~Serving()
	{
		httpd::stop();
		httpd::setConfigForTest(httpd::defaultWebConfig());
	}

private:
	Serving(const Serving &);
	Serving &operator=(const Serving &);
};

Headers modernHeaders(const std::string &method, const std::string &token = "tok-read")
{
	Headers h;
	h.push_back(std::make_pair(std::string("Host"), std::string("box.test")));
	h.push_back(std::make_pair(std::string("Content-Type"), std::string("application/json")));
	h.push_back(std::make_pair(std::string("Accept"), std::string("application/json, text/event-stream")));
	h.push_back(std::make_pair(std::string("MCP-Protocol-Version"), std::string("2026-07-28")));
	h.push_back(std::make_pair(std::string("Mcp-Method"), method));
	if (!token.empty())
		h.push_back(std::make_pair(std::string("Authorization"), "Bearer " + token));
	return h;
}

testhttp::Reply post(const Headers &h, const std::string &body, const std::string &path = "/mcp")
{
	return testhttp::request(httpd::boundPort(), "POST", path, h, body);
}

long nowMs()
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (long) t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

void sleepMs(long ms)
{
	struct timespec t;
	t.tv_sec = ms / 1000;
	t.tv_nsec = (ms % 1000) * 1000000L;
	nanosleep(&t, NULL);
}

bool callsReach(size_t want, long budget_ms)
{
	for (long waited = 0; waited < budget_ms && httpd::mcp::runningCalls() != want; waited += 10)
		sleepMs(10);
	return httpd::mcp::runningCalls() == want;
}

size_t openRequestsReach(size_t want, long budget_ms)
{
	for (long waited = 0; waited < budget_ms && httpd::openRequestsForTest() != want; waited += 10)
		sleepMs(10);
	return httpd::openRequestsForTest();
}

Headers slowCallHeaders()
{
	Headers h = modernHeaders("tools/call");
	h.push_back(std::make_pair(std::string("Mcp-Name"), std::string("slow")));
	return h;
}

std::string slowCallBody()
{
	return mcpfake::modernBody("1", "tools/call", "\"name\":\"slow\"");
}

std::string textOf(const std::string &body)
{
	return mcpfake::parsed(body)["result"]["content"][0]["text"].asString();
}

// The slow call over the wire, on a thread of its own so the case can do something meanwhile.
struct SlowCall
{
	pthread_t       thread;
	bool            started;
	testhttp::Reply reply;
	long            took_ms;

	SlowCall() : took_ms(0) { started = pthread_create(&thread, NULL, &run, this) == 0; }
	~SlowCall() { join(); }

	void join()
	{
		if (started)
			pthread_join(thread, NULL);
		started = false;
	}

	static void *run(void *cls)
	{
		SlowCall *s = (SlowCall *) cls;
		const long from = nowMs();
		s->reply = post(slowCallHeaders(), slowCallBody());
		s->took_ms = nowMs() - from;
		return NULL;
	}

private:
	SlowCall(const SlowCall &);
	SlowCall &operator=(const SlowCall &);
};

// The slow call written to a socket the case holds, so the case decides when the client goes.
int sendSlowCall()
{
	const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;
	struct sockaddr_in a;
	std::memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_port = htons((uint16_t) httpd::boundPort());
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (::connect(fd, (const struct sockaddr *) &a, sizeof(a)) != 0)
	{
		::close(fd);
		return -1;
	}
	const std::string body = slowCallBody();
	std::string req = "POST /mcp HTTP/1.1\r\n";
	const Headers h = slowCallHeaders();
	for (size_t i = 0; i < h.size(); ++i)
		req += h[i].first + ": " + h[i].second + "\r\n";
	char length[32];
	snprintf(length, sizeof(length), "%u", (unsigned) body.size());
	req += std::string("Content-Length: ") + length + "\r\n\r\n" + body;
	size_t sent = 0;
	while (sent < req.size())
	{
		const ssize_t n = ::send(fd, req.data() + sent, req.size() - sent, 0);
		if (n <= 0)
		{
			::close(fd);
			return -1;
		}
		sent += (size_t) n;
	}
	return fd;
}

std::string readAll(int fd)
{
	std::string got;
	char chunk[4096];
	ssize_t n;
	while ((n = ::recv(fd, chunk, sizeof(chunk), 0)) > 0)
		got.append(chunk, (size_t) n);
	return got;
}

coreapi::Result<httpd::mcp::Caller> throwingVerify(const std::string &, httpd::Origin, const std::string &)
{
	throw std::runtime_error("the token store failed");
}

} // namespace

TEST_CASE("the endpoint answers over the wire once it is wired", "[mcp]")
{
	mcpfake::Wired wired;
	Serving serving(aiConfig(false));
	const testhttp::Reply r = post(modernHeaders("server/discover"), mcpfake::modernBody("1", "server/discover"));
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.header("Content-Type") == "application/json");
	REQUIRE(r.header("Cache-Control") == "no-store");
	REQUIRE(r.header("Mcp-Session-Id").empty());
	REQUIRE(r.header("Access-Control-Allow-Origin").empty());
	REQUIRE(mcpfake::errorCode(r.body) == 0);
	REQUIRE(mcpfake::parsed(r.body)["result"]["resultType"].asString() == "complete");
}

TEST_CASE("over the wire a LAN request without a token is challenged without metadata", "[mcp]")
{
	mcpfake::Wired wired;
	Serving serving(aiConfig(false));
	const testhttp::Reply r = post(modernHeaders("server/discover", ""), mcpfake::modernBody("1", "server/discover"));
	REQUIRE(r.code == 401);
	REQUIRE(r.header("WWW-Authenticate") == "Bearer");
}

TEST_CASE("through the tunnel a request without a token is told where the metadata is", "[mcp]")
{
	mcpfake::Wired wired;
	Serving serving(aiConfig(true));
	const testhttp::Reply r = post(modernHeaders("server/discover", ""), mcpfake::modernBody("1", "server/discover"));
	REQUIRE(r.code == 401);
	REQUIRE(r.header("WWW-Authenticate") ==
	        "Bearer resource_metadata=\"https://tv.example.org/.well-known/oauth-protected-resource/mcp\", "
	        "scope=\"read\"");
}

TEST_CASE("through the tunnel the tool sees an external caller", "[mcp]")
{
	mcpfake::Wired wired;
	Serving serving(aiConfig(true));
	Headers h = modernHeaders("tools/call");
	h.push_back(std::make_pair(std::string("Mcp-Name"), std::string("whoami")));
	const testhttp::Reply r = post(h, mcpfake::modernBody("1", "tools/call", "\"name\":\"whoami\""));
	REQUIRE(r.code == 200);
	REQUIRE(mcpfake::errorCode(r.body) == 0);
	REQUIRE(mcpfake::parsed(r.body)["result"]["structuredContent"]["external"].asBool() == true);
	REQUIRE(mcpfake::seenOrigin() == httpd::Origin::Tunnel);
	REQUIRE(mcpfake::seenResource() == "https://tv.example.org/mcp");
}

TEST_CASE("a token in the query is no token", "[mcp]")
{
	mcpfake::Wired wired;
	Serving serving(aiConfig(false));
	const testhttp::Reply r = post(modernHeaders("server/discover", ""), mcpfake::modernBody("1", "server/discover"),
	                               "/mcp?access_token=tok-read");
	REQUIRE(r.code == 401);
}

TEST_CASE("a ni-web session is not a credential here", "[mcp]")
{
	mcpfake::Wired wired;
	Serving serving(aiConfig(false));
	const std::string session = httpd::openSession("root");
	REQUIRE_FALSE(session.empty());
	Headers h = modernHeaders("server/discover", "");
	h.push_back(std::make_pair(std::string("Cookie"), std::string(httpd::sessionCookieName()) + "=" + session));
	const testhttp::Reply r = post(h, mcpfake::modernBody("1", "server/discover"));
	httpd::closeSession(session);
	REQUIRE(r.code == 401);
}

TEST_CASE("only POST reaches the endpoint over the wire", "[mcp]")
{
	mcpfake::Wired wired;
	Serving serving(aiConfig(false));
	const testhttp::Reply r = testhttp::request(httpd::boundPort(), "GET", "/mcp", modernHeaders("server/discover"));
	REQUIRE(r.code == 405);
	REQUIRE(r.header("Allow") == "POST");
	const testhttp::Reply o = testhttp::request(httpd::boundPort(), "OPTIONS", "/mcp", modernHeaders("server/discover"));
	REQUIRE(o.code == 405);
	REQUIRE(o.header("Access-Control-Allow-Origin").empty());
}

TEST_CASE("a mirrored header sent twice is refused", "[mcp]")
{
	mcpfake::Wired wired;
	Serving serving(aiConfig(false));
	Headers h = modernHeaders("server/discover");
	h.push_back(std::make_pair(std::string("Mcp-Method"), std::string("tools/list")));
	const testhttp::Reply r = post(h, mcpfake::modernBody("1", "server/discover"));
	REQUIRE(r.code == 400);
	REQUIRE(mcpfake::errorCode(r.body) == httpd::mcp::kHeaderMismatch);
}

TEST_CASE("an Origin of another site is refused over the wire", "[mcp]")
{
	mcpfake::Wired wired;
	Serving serving(aiConfig(false));
	Headers h = modernHeaders("server/discover");
	h.push_back(std::make_pair(std::string("Origin"), std::string("http://box.test")));
	REQUIRE(post(h, mcpfake::modernBody("1", "server/discover")).code == 200);
	h.back().second = "http://evil.example";
	REQUIRE(post(h, mcpfake::modernBody("1", "server/discover")).code == 403);
}

TEST_CASE("what older clients carry along changes nothing", "[mcp]")
{
	mcpfake::Wired wired;
	Serving serving(aiConfig(false));
	Headers h = modernHeaders("server/discover");
	h.push_back(std::make_pair(std::string("Mcp-Session-Id"), std::string("abc")));
	h.push_back(std::make_pair(std::string("Last-Event-ID"), std::string("5")));
	const testhttp::Reply r = post(h, mcpfake::modernBody("1", "server/discover"));
	REQUIRE(r.code == 200);
	REQUIRE(mcpfake::errorCode(r.body) == 0);
	REQUIRE(mcpfake::parsed(r.body)["result"]["resultType"].asString() == "complete");
	REQUIRE(r.header("Mcp-Session-Id").empty());
}

TEST_CASE("the endpoint keeps a body only for a caller it admitted", "[mcp]")
{
	mcpfake::Wired wired;
	Serving serving(aiConfig(false));
	const std::string body = mcpfake::modernBody("1", "server/discover");

	const size_t before = httpd::bodyBytesKeptForTest();
	testhttp::Reply r = post(modernHeaders("server/discover"), body);
	REQUIRE(r.code == 200);
	REQUIRE(httpd::bodyBytesKeptForTest() == before + body.size());

	const size_t held = httpd::bodyBytesKeptForTest();
	r = post(modernHeaders("server/discover", "tok-nobody"), body);
	REQUIRE(r.code == 401);
	REQUIRE(httpd::bodyBytesKeptForTest() == held);
}

TEST_CASE("a body over the endpoint's own ceiling is refused", "[mcp]")
{
	mcpfake::Wired wired;
	Serving serving(aiConfig(false));
	const std::string big(httpd::mcp::defaultLimits().max_body_bytes + 1, ' ');
	REQUIRE(post(modernHeaders("server/discover"), big).code == 413);
}

TEST_CASE("a verifier that throws costs the request and not the server", "[mcp]")
{
	mcpfake::Wired wired;
	Serving serving(aiConfig(false));
	const httpd::mcp::Wiring w = { &mcpfake::tools(), &throwingVerify };
	httpd::mcp::install(w);
	const testhttp::Reply r = post(modernHeaders("server/discover"), mcpfake::modernBody("1", "server/discover"));
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 500);
	REQUIRE(r.body.find("box-unreadable") != std::string::npos);

	const httpd::mcp::Wiring good = { &mcpfake::tools(), &mcpfake::verify };
	httpd::mcp::install(good);
	REQUIRE(post(modernHeaders("server/discover"), mcpfake::modernBody("1", "server/discover")).code == 200);
}

TEST_CASE("an endpoint nothing wired is a path nobody has", "[mcp]")
{
	Serving serving(aiConfig(false));
	REQUIRE(post(modernHeaders("server/discover"), mcpfake::modernBody("1", "server/discover")).code == 404);
}

TEST_CASE("with AI access off a wired endpoint is still a path nobody has", "[mcp]")
{
	mcpfake::Wired wired;
	Serving serving(httpd::defaultWebConfig());
	REQUIRE(post(modernHeaders("server/discover"), mcpfake::modernBody("1", "server/discover")).code == 404);
}

TEST_CASE("a tool call still running leaves the server free for other requests", "[mcp]")
{
	mcpfake::Wired wired;
	mcpfake::slowMs() = 1500;
	Serving serving(aiConfig(false), 1);
	SlowCall slow;
	REQUIRE(slow.started);
	REQUIRE(callsReach(1, 2000));

	const long from = nowMs();
	const testhttp::Reply r = post(modernHeaders("server/discover"), mcpfake::modernBody("1", "server/discover"));
	const long took = nowMs() - from;
	REQUIRE(r.code == 200);
	REQUIRE(took < 500);

	slow.join();
	REQUIRE(slow.reply.code == 200);
	REQUIRE(mcpfake::parsed(slow.reply.body)["result"]["structuredContent"]["done"].asBool());
}

TEST_CASE("a tool call past its deadline is answered with the timeout over the wire", "[mcp]")
{
	mcpfake::Wired wired;
	httpd::mcp::Limits l = httpd::mcp::limits();
	l.call_timeout_ms = 200;
	httpd::mcp::setLimits(l);
	mcpfake::slowMs() = 1500;
	Serving serving(aiConfig(false), 1);
	SlowCall slow;
	REQUIRE(slow.started);
	slow.join();
	REQUIRE(slow.reply.code == 200);
	REQUIRE(mcpfake::parsed(slow.reply.body)["result"]["isError"].asBool());
	REQUIRE(textOf(slow.reply.body).find("did not finish this within 1 s") != std::string::npos);
	REQUIRE(slow.took_ms < 1000);
	// Given up on, not stopped.
	REQUIRE(httpd::mcp::runningCalls() == 1);
}

TEST_CASE("a client gone while its tool call runs costs neither the server nor a request", "[mcp]")
{
	mcpfake::Wired wired;
	mcpfake::slowMs() = 800;
	Serving serving(aiConfig(false), 1);
	const size_t before = httpd::openRequestsForTest();

	const int fd = sendSlowCall();
	REQUIRE(fd >= 0);
	REQUIRE(callsReach(1, 2000));
	REQUIRE(openRequestsReach(before + 1, 2000) == before + 1);
	::close(fd);

	REQUIRE(mcpfake::waitForCalls());
	REQUIRE(openRequestsReach(before, 3000) == before);
	REQUIRE(post(modernHeaders("server/discover"), mcpfake::modernBody("1", "server/discover")).code == 200);
}

TEST_CASE("stopping the server does not wait for a running tool call", "[mcp]")
{
	mcpfake::Wired wired;
	mcpfake::slowMs() = 1500;
	Serving serving(aiConfig(false), 1);

	const int fd = sendSlowCall();
	REQUIRE(fd >= 0);
	REQUIRE(callsReach(1, 2000));
	sleepMs(50);

	const long from = nowMs();
	httpd::stop();
	const long took = nowMs() - from;
	const std::string reply = readAll(fd);
	::close(fd);
	REQUIRE(took < 1000);
	REQUIRE(reply.compare(0, 12, "HTTP/1.1 200") == 0);
	REQUIRE(reply.find("did not finish") != std::string::npos);
	REQUIRE(mcpfake::waitForCalls());
}
