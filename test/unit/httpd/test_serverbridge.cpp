/*
 * test_serverbridge.cpp - tests for the bridge from the server to the box
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
#include "httpd/doc/openapi.h"
#include "httpd/router.h"
#include "httpd/server.h"

#include "jsoncpp/json/json.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

using namespace httpd;

namespace
{

/* What the handlers below record, so that a case can say the handler never ran
   rather than only that the answer was a refusal. The count is never reset: a
   case that forgot to clear the flag is still caught by a number that only
   goes up. */
bool handler_ran = false;
int  handler_calls = 0;
std::string seen_body;
std::string seen_peer;

/* Every case leaves the server stopped and the shipped tables back in place,
   whichever line it left through. A failed check unwinds past everything after
   it, and a case that left a daemon bound would take the port and the table
   with it for the rest of the run. */
struct Serving
{
	// Declared first, so the daemon starts after the seams are filled and stops
	// before they are cleared.
	InstalledDependencies wired_;

	Serving() {}
	~Serving()
	{
		stop();
		setRoutesForTest(NULL);
	}

private:
	Serving(const Serving &);
	Serving &operator=(const Serving &);
};

// A descriptor given up by leaving the scope, for the cases that speak to the
// socket themselves because the client beside them answers one request per
// connection and closes.
struct Held
{
	int fd;

	explicit Held(int f) : fd(f) {}
	~Held()
	{
		if (fd >= 0)
			::close(fd);
	}

private:
	Held(const Held &);
	Held &operator=(const Held &);
};

ServerConfig loopback()
{
	ServerConfig c = defaultConfig();
	c.port = 0;                       // let the kernel choose
	c.bind_address = "127.0.0.1";
	return c;
}

std::vector<std::pair<std::string, std::string> > jsonHeaders()
{
	std::vector<std::pair<std::string, std::string> > h;
	h.push_back(std::make_pair(std::string("Content-Type"), std::string("application/json")));
	return h;
}

/* A credential worth a write, because the route that hands a body back changes
   something as far as the table check is concerned. A bearer token and not a session
   cookie: a bearer needs no second token, and these cases are about what happens to a
   body. Drawn once, so the record it leaves is one for the whole run. */
const std::string &writeToken()
{
	static std::string token;
	if (token.empty())
	{
		token = randomToken();
		addApiToken(tokenLookupPrefix(token), hashSecret(token), AuthLevel::Write);
	}
	return token;
}

std::string writeHeaderLine()
{
	return "Authorization: Bearer " + writeToken() + "\r\n";
}

std::vector<std::pair<std::string, std::string> > jsonHeadersThatMayWrite()
{
	std::vector<std::pair<std::string, std::string> > h = jsonHeaders();
	h.push_back(std::make_pair(std::string("Authorization"), "Bearer " + writeToken()));
	return h;
}

Response fine(const Request &)
{
	handler_ran = true;
	handler_calls++;
	Response out;
	out.code = StatusOk;
	out.content_type = "application/json";
	out.body = "{\"fine\":true}";
	return out;
}

// Hands back what it was given, which is what makes a body that arrived in
// several pieces visible from the far end.
Response echo(const Request &r)
{
	handler_ran = true;
	handler_calls++;
	seen_body = r.body();
	Response out;
	out.code = StatusOk;
	out.content_type = "application/json";
	out.body = r.body();
	return out;
}

Response peerBack(const Request &r)
{
	handler_ran = true;
	handler_calls++;
	seen_peer = r.peer();
	Response out;
	out.code = StatusOk;
	out.content_type = "text/plain";
	out.body = r.peer();
	return out;
}

Response boom(const Request &)
{
	handler_ran = true;
	handler_calls++;
	throw std::runtime_error("the handler gave up");
}

// Not derived from the exception type, so the clause that names that type
// cannot be what answers for it.
Response oddThrow(const Request &)
{
	handler_ran = true;
	handler_calls++;
	throw 42;
}

// Answers the code a response carries before anything writes one, which is the
// number a handler that forgot would send.
Response silent(const Request &)
{
	handler_ran = true;
	handler_calls++;
	return Response();
}

Response secret(const Request &)
{
	handler_ran = true;
	handler_calls++;
	Response out;
	out.code = StatusOk;
	out.content_type = "application/json";
	out.body = "{}";
	return out;
}

const Endpoint wired_endpoints[] = {
	{ Method::Get,  "/api/v1/fine",      AuthLevel::Read,   "answers",        NULL, 0, NULL, &fine, false },
	{ Method::Post, "/api/v1/echo",      AuthLevel::Write,  "hands back",     NULL, 0, NULL, &echo, false },
	{ Method::Get,  "/api/v1/peer",      AuthLevel::Read,   "names the peer", NULL, 0, NULL, &peerBack, false },
	{ Method::Get,  "/api/v1/throw",     AuthLevel::Read,   "gives up",       NULL, 0, NULL, &boom, false },
	{ Method::Get,  "/api/v1/oddthrow",  AuthLevel::Read,   "gives up oddly", NULL, 0, NULL, &oddThrow, false },
	{ Method::Get,  "/api/v1/nocode",    AuthLevel::Read,   "answers no code", NULL, 0, NULL, &silent, false },
	{ Method::Get,  "/api/v1/secret",    AuthLevel::System, "needs the most", NULL, 0, NULL, &secret, false },
};

const RouteTable wired_table = { HTTPD_TABLE("wired", wired_endpoints) };

// Two routes that answer one request equally well, which is what the table
// check refuses and what the daemon must refuse to start on.
const Endpoint duplicate_endpoints[] = {
	{ Method::Get, "/api/v1/two/{id}",   AuthLevel::Read, "one", NULL, 0, NULL, &fine, false },
	{ Method::Get, "/api/v1/two/{name}", AuthLevel::Read, "two", NULL, 0, NULL, &fine, false },
};

const RouteTable duplicate_table = { HTTPD_TABLE_N("duplicate", duplicate_endpoints, 2) };

int connectLoopback(int port)
{
	struct sockaddr_in a;
	std::memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_port = htons((uint16_t) port);
	if (::inet_pton(AF_INET, "127.0.0.1", &a.sin_addr) != 1)
		return -1;

	const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	// A read that waits for a peer that is never going to answer has to end the
	// case rather than the run.
	struct timeval tv;
	std::memset(&tv, 0, sizeof(tv));
	tv.tv_sec = 5;
	::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	if (::connect(fd, (const struct sockaddr *) &a, sizeof(a)) != 0)
	{
		::close(fd);
		return -1;
	}
	return fd;
}

/* The signal a write to a closed connection raises is asked for as an error
   code, because its default action would end the run in the middle of the case
   that provoked it rather than failing it. Defensive: every case here reads
   what the far end said before it writes any further, so none of them has been
   seen to need it. */
bool sendAll(int fd, const std::string &data)
{
	size_t sent = 0;
	while (sent < data.size())
	{
		const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
		if (n <= 0)
			return false;
		sent += (size_t) n;
	}
	return true;
}

// Reads until the buffer holds want bytes or the peer stops talking, so that a
// case waiting for two answers on one connection does not depend on how the
// two were split across reads.
std::string readAtLeast(int fd, size_t want)
{
	std::string got;
	char chunk[4096];
	while (got.size() < want)
	{
		const ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
		if (n <= 0)
			break;
		got.append(chunk, (size_t) n);
	}
	return got;
}

/* Writes a request in pieces and stops as soon as the far end has said
   something, which is what reading an answer to a request the server refuses
   part way through takes: a caller that writes the whole of it first is still
   writing when the connection is closed under it, and the answer already sent
   can be lost with the reset that follows. */
std::string sendUntilAnswered(int fd, const std::string &request)
{
	std::string got;
	char chunk[4096];
	size_t sent = 0;
	while (sent < request.size())
	{
		const size_t piece = (request.size() - sent < 4096) ? (request.size() - sent) : 4096;
		const ssize_t n = ::send(fd, request.data() + sent, piece, MSG_NOSIGNAL);
		if (n <= 0)
			break;
		sent += (size_t) n;

		const ssize_t r = ::recv(fd, chunk, sizeof(chunk), MSG_DONTWAIT);
		if (r > 0)
		{
			got.append(chunk, (size_t) r);
			break;
		}
	}
	if (got.empty())
	{
		const ssize_t r = ::recv(fd, chunk, sizeof(chunk), 0);
		if (r > 0)
			got.append(chunk, (size_t) r);
	}
	return got;
}

/* Waits for the number of requests the server is holding state for to reach
   what is wanted, and answers what it reached. A number read once would be read
   before the daemon's own thread had got to the connection, so what a case
   asserts is the number and how long it took to get there. */
size_t waitForOpenRequests(size_t want, int budget_ms)
{
	for (int waited = 0; waited < budget_ms; waited += 20)
	{
		if (openRequestsForTest() == want)
			return want;
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = 20 * 1000 * 1000;
		nanosleep(&ts, NULL);
	}
	return openRequestsForTest();
}

size_t countOf(const std::string &haystack, const std::string &needle)
{
	size_t n = 0;
	size_t at = 0;
	for (;;)
	{
		at = haystack.find(needle, at);
		if (at == std::string::npos)
			return n;
		++n;
		at += needle.size();
	}
}

std::string chunked(const std::string &body, size_t piece)
{
	std::string out;
	size_t at = 0;
	while (at < body.size())
	{
		const size_t n = (body.size() - at < piece) ? (body.size() - at) : piece;
		char head[32];
		std::snprintf(head, sizeof(head), "%lx\r\n", (unsigned long) n);
		out += head;
		out.append(body, at, n);
		out += "\r\n";
		at += n;
	}
	out += "0\r\n\r\n";
	return out;
}

} // namespace

TEST_CASE("a handler that throws answers 500 and the server keeps serving", "[bridge]")
{
	/* The tree below this is built without exceptions and a throw that escaped
	   the callback would end the process, so this case is the boundary's only
	   proof. */
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));
	const int port = boundPort();

	handler_ran = false;
	testhttp::Reply bad = testhttp::request(port, "GET", "/api/v1/throw");
	REQUIRE(bad.transport_ok);
	REQUIRE(bad.code == 500);
	REQUIRE(bad.header("Content-Type") == "application/problem+json");
	REQUIRE(handler_ran);

	testhttp::Reply good = testhttp::request(port, "GET", "/api/v1/fine");
	REQUIRE(good.transport_ok);
	REQUIRE(good.code == 200);
	REQUIRE(good.body == "{\"fine\":true}");
}

TEST_CASE("a throw of something that is not an exception is answered too", "[bridge]")
{
	// The clause that names the exception type cannot answer for this one, and
	// what is left is the clause that names nothing.
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));
	const int port = boundPort();

	handler_ran = false;
	testhttp::Reply r = testhttp::request(port, "GET", "/api/v1/oddthrow");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 500);
	REQUIRE(r.header("Content-Type") == "application/problem+json");
	REQUIRE(handler_ran);

	testhttp::Reply good = testhttp::request(port, "GET", "/api/v1/fine");
	REQUIRE(good.code == 200);
}

TEST_CASE("a body over the ceiling is 413 and the handler never runs", "[bridge]")
{
	Serving guard;
	ServerConfig c = loopback();
	c.max_body_bytes = 1024;
	setRoutesForTest(&wired_table);
	REQUIRE(start(c));

	handler_ran = false;
	const int before = handler_calls;
	const std::string big(4096, 'x');
	testhttp::Reply r = testhttp::request(boundPort(), "POST", "/api/v1/echo", jsonHeaders(), big);
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 413);
	REQUIRE(r.header("Content-Type") == "application/problem+json");
	REQUIRE(r.body.find("body-too-large") != std::string::npos);
	REQUIRE_FALSE(handler_ran);
	REQUIRE(handler_calls == before);
}

TEST_CASE("a body at the ceiling is accepted whole", "[bridge]")
{
	// The boundary is off by one in one of the two directions unless it is
	// tested from both.
	Serving guard;
	ServerConfig c = loopback();
	c.max_body_bytes = 1024;
	setRoutesForTest(&wired_table);
	REQUIRE(start(c));

	const std::string exact(1024, 'y');
	testhttp::Reply r = testhttp::request(boundPort(), "POST", "/api/v1/echo", jsonHeadersThatMayWrite(), exact);
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.body.size() == exact.size());
	REQUIRE(r.body == exact);
}

TEST_CASE("one byte past the ceiling is refused", "[bridge]")
{
	// The other side of the same boundary, so that a ceiling read as one less
	// or one more than it says is caught by one of the two.
	Serving guard;
	ServerConfig c = loopback();
	c.max_body_bytes = 1024;
	setRoutesForTest(&wired_table);
	REQUIRE(start(c));

	handler_ran = false;
	const std::string over(1025, 'y');
	testhttp::Reply r = testhttp::request(boundPort(), "POST", "/api/v1/echo", jsonHeaders(), over);
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 413);
	REQUIRE_FALSE(handler_ran);
}

TEST_CASE("a body already refused is not invited", "[bridge]")
{
	/* A caller that says it will wait to be told is told the answer, rather
	   than being sent the go ahead for a body this server has already decided
	   not to take. Read off the socket rather than through the client beside
	   this, because the go ahead is itself an answer and a client reading one
	   answer would stop at it. */
	Serving guard;
	ServerConfig c = loopback();
	c.max_body_bytes = 1024;
	setRoutesForTest(&wired_table);
	REQUIRE(start(c));

	handler_ran = false;
	std::string answer;
	{
		Held s(connectLoopback(boundPort()));
		REQUIRE(s.fd >= 0);
		std::string req = "POST /api/v1/echo HTTP/1.1\r\n";
		req += "Host: 127.0.0.1\r\n";
		req += "Content-Type: application/json\r\n";
		req += "Content-Length: 5000\r\n";
		req += "Expect: 100-continue\r\n";
		req += "Connection: close\r\n\r\n";
		REQUIRE(sendAll(s.fd, req));
		// No body follows, which is the whole of it: this caller is waiting.
		answer = readAtLeast(s.fd, 1u << 16);
	}

	INFO(answer.substr(0, 200));
	REQUIRE(answer.compare(0, 12, "HTTP/1.1 413") == 0);
	REQUIRE(answer.find("100 Continue") == std::string::npos);
	REQUIRE(answer.find("body-too-large") != std::string::npos);
	REQUIRE_FALSE(handler_ran);
}

TEST_CASE("every answer the API sends says how it may be kept and by whom", "[bridge]")
{
	/* Three headers, on an answer a handler built, on a refusal the gate made and on a
	   path nobody has, because all three come back from one URL depending on who asks
	   and what is there. Driven over a socket rather than off the response object,
	   because a header set on an answer the transport then builds its own head from
	   would be a header a client never sees. */
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));

	struct Case
	{
		const char *target;
		int         code;
	};

	// A handler's answer, a refusal from the gate at a route this caller may
	// not reach, and a path this server does not have.
	static const Case cases[] = {
		{ "/api/v1/fine",   200 },
		{ "/api/v1/secret", 403 },
		{ "/api/v1/gone",   404 }
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
	{
		INFO(cases[i].target);
		testhttp::Reply r = testhttp::request(boundPort(), "GET", cases[i].target);
		REQUIRE(r.transport_ok);
		REQUIRE(r.code == cases[i].code);

		// Several of these answers were built for one caller and one of them
		// is the credential itself.
		REQUIRE(r.header("Cache-Control") == "no-store");
		// One URL, two answers, and what tells them apart is a header nothing
		// keeping answers looks at unless it is told to.
		REQUIRE(r.header("Vary").find("Cookie") != std::string::npos);
		REQUIRE(r.header("Vary").find("Authorization") != std::string::npos);
		// The type stated is the type meant.
		REQUIRE(r.header("X-Content-Type-Options") == "nosniff");
	}
}

TEST_CASE("nothing of a body is kept for a request nothing will answer", "[bridge]")
{
	/* The gate is asked once the head is whole and before the body is accepted, so a
	   caller who may not reach a route never gets a byte of the box for what it sends.
	   Before that was so, sixty connections to a route that refused every one of them
	   held seventy megabytes.

	   Watched through the count of bytes taken in, because from a socket the two are
	   the same answer word for word: what differs is only what this box is holding
	   while it writes it, and a case that watched memory would pass wherever there is
	   enough of it.

	   The allowed request comes first and is what makes the rest mean something:
	   without it a count that never moved would read as proof whether the gate ran
	   early, late, or not at all. It is also what holds the assignment against every
	   spelling that leaves the line written where it belongs and never runs it. */
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));

	const std::string body(64000, 'z');

	const size_t before_allowed = bodyBytesKeptForTest();
	testhttp::Reply ok = testhttp::request(boundPort(), "POST", "/api/v1/echo",
	                                       jsonHeadersThatMayWrite(), body);
	REQUIRE(ok.transport_ok);
	REQUIRE(ok.code == 200);
	REQUIRE(bodyBytesKeptForTest() == before_allowed + body.size());

	// A caller with no credential at a route that changes something. The
	// answer is the route's own refusal and the body is read and dropped.
	const size_t held = bodyBytesKeptForTest();
	testhttp::Reply refused = testhttp::request(boundPort(), "POST", "/api/v1/echo",
	                                            jsonHeaders(), body);
	REQUIRE(refused.transport_ok);
	REQUIRE(refused.code == 403);
	REQUIRE(bodyBytesKeptForTest() == held);

	// A path this server does not have, and a method this path does not take.
	// Both are answered without a handler, so both are answered without a body.
	testhttp::Reply nowhere = testhttp::request(boundPort(), "POST", "/api/v1/no-such-thing",
	                                            jsonHeadersThatMayWrite(), body);
	REQUIRE(nowhere.transport_ok);
	REQUIRE(nowhere.code == 404);
	REQUIRE(bodyBytesKeptForTest() == held);

	testhttp::Reply wrong = testhttp::request(boundPort(), "PUT", "/api/v1/echo",
	                                          jsonHeadersThatMayWrite(), body);
	REQUIRE(wrong.transport_ok);
	REQUIRE(wrong.code == 405);
	REQUIRE(bodyBytesKeptForTest() == held);
}

TEST_CASE("a session lost while a body arrives is not answered on", "[bridge]")
{
	/* The caller is resolved once, off the head, because the library calls the handler
	   at least twice for every request. What that leaves is one thing that can stop
	   being true in between: a session runs out on the clock and is closed by a logout
	   on another connection, and a body can take longer to arrive than either.

	   So the session is asked about again at the answer, which is a lookup. Driven with
	   the body held open and the session closed from here in the middle of it, which is
	   what a logout on a second connection does. */
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));

	const std::string session = openSession("root");
	REQUIRE_FALSE(session.empty());
	const std::string csrf = csrfFor(session);
	REQUIRE_FALSE(csrf.empty());

	const std::string body(20, 'x');
	std::string answer;
	{
		Held s(connectLoopback(boundPort()));
		REQUIRE(s.fd >= 0);

		std::string head = "POST /api/v1/echo HTTP/1.1\r\n";
		head += "Host: 127.0.0.1\r\n";
		head += "Content-Type: application/json\r\n";
		head += std::string("Cookie: ") + sessionCookieName() + "=" + session + "\r\n";
		head += std::string(csrfHeaderName()) + ": " + csrf + "\r\n";
		head += "Content-Length: 20\r\n";
		head += "Connection: close\r\n\r\n";
		REQUIRE(sendAll(s.fd, head));
		// Half of it, so the head is read and the request is not whole.
		const size_t kept_before = bodyBytesKeptForTest();
		REQUIRE(sendAll(s.fd, body.substr(0, 10)));

		/* Waited for rather than assumed. Closing the session before the gate has run
		   would refuse the request at the head, which is the answer this case is
		   looking for reached by the wrong road, and it is a race the case would lose
		   often enough to look like it passed. The count of body bytes taken in is the
		   rendezvous. */
		bool gate_ran = false;
		for (int i = 0; i < 400 && !gate_ran; ++i)
		{
			if (bodyBytesKeptForTest() >= kept_before + 10)
				gate_ran = true;
			else
				usleep(5000);
		}
		REQUIRE(gate_ran);

		// What a logout on another connection does, while this one is still
		// writing.
		REQUIRE(closeSession(session));

		REQUIRE(sendAll(s.fd, body.substr(10)));
		answer = readAtLeast(s.fd, 1u << 16);
	}

	INFO(answer.substr(0, 200));
	// The route asks for a write and what is left of this caller is the read
	// its address is worth, so the answer is the route's refusal and the body
	// it sent is not handed back to it.
	REQUIRE(answer.compare(0, 12, "HTTP/1.1 403") == 0);
	REQUIRE(answer.find("not-permitted") != std::string::npos);
	REQUIRE(answer.find(body) == std::string::npos);
}

TEST_CASE("a caller that will be refused is not invited to send", "[bridge]")
{
	/* The other half of the same decision, from the caller's side. One that says it
	   will wait to be told is told the refusal instead of being sent the go ahead for a
	   body this server has already decided not to read. Read off the socket rather than
	   through the client beside this, because the go ahead is itself an answer and a
	   client reading one answer would stop at it. */
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));

	handler_ran = false;
	std::string answer;
	{
		Held s(connectLoopback(boundPort()));
		REQUIRE(s.fd >= 0);
		std::string req = "POST /api/v1/echo HTTP/1.1\r\n";
		req += "Host: 127.0.0.1\r\n";
		req += "Content-Type: application/json\r\n";
		req += "Content-Length: 5000\r\n";
		req += "Expect: 100-continue\r\n";
		req += "Connection: close\r\n\r\n";
		REQUIRE(sendAll(s.fd, req));
		// No body follows, and no credential was carried. This caller is
		// waiting to be told whether to send one.
		answer = readAtLeast(s.fd, 1u << 16);
	}

	INFO(answer.substr(0, 200));
	REQUIRE(answer.compare(0, 12, "HTTP/1.1 403") == 0);
	REQUIRE(answer.find("100 Continue") == std::string::npos);
	REQUIRE(answer.find("not-permitted") != std::string::npos);
	REQUIRE_FALSE(handler_ran);
}

TEST_CASE("a body arriving in several parts is assembled before the handler", "[bridge]")
{
	/* The access handler is called repeatedly for one request, and a handler
	   run on the first piece would see a body that stops early. Once, and not
	   once per piece, is the other half of it. */
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));

	const std::string body(64000, 'z');
	seen_body.clear();
	const int before = handler_calls;
	testhttp::Reply r = testhttp::request(boundPort(), "POST", "/api/v1/echo", jsonHeadersThatMayWrite(), body);
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.body.size() == body.size());
	REQUIRE(r.body == body);
	REQUIRE(seen_body == body);
	REQUIRE(handler_calls == before + 1);
}

TEST_CASE("a body carrying a zero byte reaches the handler whole", "[bridge]")
{
	// Counted rather than terminated: a body read as a C string would stop at
	// the first of these and the answer would be shorter than what was sent.
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));

	std::string body = "before";
	body += '\0';
	body += "after";
	seen_body.clear();
	testhttp::Reply r = testhttp::request(boundPort(), "POST", "/api/v1/echo", jsonHeadersThatMayWrite(), body);
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(seen_body.size() == body.size());
	REQUIRE(seen_body == body);
}

TEST_CASE("a body sent in chunks with no declared length is assembled", "[bridge]")
{
	// The other way a body arrives, and the one where nothing states its size
	// in advance, so the ceiling can only be read off what has arrived.
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));

	const std::string body(9000, 'c');
	std::vector<std::pair<std::string, std::string> > h = jsonHeadersThatMayWrite();
	h.push_back(std::make_pair(std::string("Transfer-Encoding"), std::string("chunked")));

	seen_body.clear();
	const int before = handler_calls;
	testhttp::Reply r = testhttp::request(boundPort(), "POST", "/api/v1/echo", h, chunked(body, 1300));
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(seen_body == body);
	REQUIRE(handler_calls == before + 1);
}

TEST_CASE("a chunked body over the ceiling is refused without a declared length", "[bridge]")
{
	/* Nothing states the size of one of these before it arrives, so the only
	   thing that can refuse it is the running total. A ceiling read off the
	   declared length alone would let this one through. */
	Serving guard;
	ServerConfig c = loopback();
	c.max_body_bytes = 1024;
	setRoutesForTest(&wired_table);
	REQUIRE(start(c));

	const std::string body(8000, 'c');
	std::vector<std::pair<std::string, std::string> > h = jsonHeaders();
	h.push_back(std::make_pair(std::string("Transfer-Encoding"), std::string("chunked")));

	handler_ran = false;
	testhttp::Reply r = testhttp::request(boundPort(), "POST", "/api/v1/echo", h, chunked(body, 512));
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 413);
	REQUIRE_FALSE(handler_ran);
}

TEST_CASE("the server refuses to start on a table that is not sane", "[bridge]")
{
	/* The check that tables are sane is what makes a table this layer wrote
	   wrong a refusal to serve rather than a coin toss on every request that
	   reaches both routes. */
	Serving guard;
	setRoutesForTest(&duplicate_table);
	REQUIRE_FALSE(start(loopback()));
	REQUIRE_FALSE(running());
	REQUIRE(boundPort() == 0);
}

TEST_CASE("a HEAD answers the headers of the GET and no body", "[bridge]")
{
	/* Sent as the head of the answer the GET would have given, so a client
	   that asks what a resource is before fetching it is told the same thing
	   twice. A body dropped before the answer was built would say the length
	   is nothing. */
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));
	const int port = boundPort();

	testhttp::Reply get = testhttp::request(port, "GET", "/api/v1/fine");
	REQUIRE(get.transport_ok);
	REQUIRE(get.code == 200);

	testhttp::Reply head = testhttp::request(port, "HEAD", "/api/v1/fine");
	REQUIRE(head.transport_ok);
	REQUIRE(head.code == 200);
	REQUIRE(head.body.empty());
	REQUIRE(head.header("Content-Type") == get.header("Content-Type"));
	REQUIRE(head.header("Content-Length") == get.header("Content-Length"));
	char stated[32];
	std::snprintf(stated, sizeof(stated), "%lu", (unsigned long) get.body.size());
	REQUIRE(head.header("Content-Length") == stated);
}

TEST_CASE("a method the path does not answer names HEAD beside the GET", "[bridge]")
{
	// A route that offers GET answers HEAD, so a list of what the path takes
	// that leaves HEAD out sends a client away from an answer it would get.
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));

	testhttp::Reply r = testhttp::request(boundPort(), "DELETE", "/api/v1/fine");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 405);
	REQUIRE(r.header("Allow").find("GET") != std::string::npos);
	REQUIRE(r.header("Allow").find("HEAD") != std::string::npos);
}

TEST_CASE("the address the request came from reaches the handler", "[bridge]")
{
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));

	seen_peer.clear();
	testhttp::Reply r = testhttp::request(boundPort(), "GET", "/api/v1/peer");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(seen_peer == "127.0.0.1");
	REQUIRE(r.body == "127.0.0.1");
}

TEST_CASE("an endpoint above what the transport grants is refused", "[bridge]")
{
	/* The caller is the loopback and carries nothing, so what it is granted is
	   the read the exemption raises an address on the box's own network to, and
	   an endpoint above that is refused before its handler. The refusal is the
	   router's, which is what this reaches through the wire. */
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));

	handler_ran = false;
	testhttp::Reply r = testhttp::request(boundPort(), "GET", "/api/v1/secret");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 403);
	REQUIRE(r.body.find("not-permitted") != std::string::npos);
	REQUIRE_FALSE(handler_ran);
}

TEST_CASE("a path no route names is 404 through the table rather than by default", "[bridge]")
{
	// The daemon answered every request with this code before anything routed
	// one, so the body is what says the answer came from the table.
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));

	testhttp::Reply r = testhttp::request(boundPort(), "GET", "/api/v1/nowhere");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 404);
	REQUIRE(r.header("Content-Type") == "application/problem+json");
	REQUIRE(r.body.find("no-such-route") != std::string::npos);
}

TEST_CASE("a handler that answers no code at all is answered as a fault here", "[bridge]")
{
	// A response carries no code until something writes one, and a number
	// nobody wrote is not a status a client can read.
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));

	testhttp::Reply r = testhttp::request(boundPort(), "GET", "/api/v1/nocode");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 500);
	REQUIRE(r.header("Content-Type") == "application/problem+json");
}

TEST_CASE("a request that declares a body and stops short leaves nothing behind", "[bridge]")
{
	/* The daemon may abandon a connection without ever calling the handler a
	   last time, so per request state freed on the way out of the handler is
	   state that leaks whenever a client goes away mid body. */
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));
	const int port = boundPort();

	// The count is one number for the whole program, so what this case says
	// about it is only true if nothing earlier left a request open.
	REQUIRE(openRequestsForTest() == 0);

	{
		Held s(connectLoopback(port));
		REQUIRE(s.fd >= 0);
		std::string req = "POST /api/v1/echo HTTP/1.1\r\n";
		req += "Host: 127.0.0.1\r\n";
		req += "Content-Length: 4096\r\n";
		req += "\r\n";
		req += std::string(16, 'q');
		REQUIRE(sendAll(s.fd, req));
		// Taken by the daemon before the connection is dropped, so that what
		// is measured below is a request it had started on.
		REQUIRE(waitForOpenRequests(1, 5000) == 1);
	}

	/* Given back when the connection went away and not when the daemon was
	   torn down, which is the difference a count read after a stop cannot see.
	   The budget is far under the thirty second idle timeout, so a state only
	   the timeout releases fails here rather than passing late. */
	REQUIRE(waitForOpenRequests(0, 5000) == 0);

	// The server is still there and still answering after the abandoned one.
	testhttp::Reply r = testhttp::request(port, "GET", "/api/v1/fine");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);

	stop();
	REQUIRE(openRequestsForTest() == 0);
}

TEST_CASE("two requests on one connection are answered and neither is left open", "[bridge]")
{
	/* State kept per connection rather than per request would answer the
	   second of these out of what the first left, and state freed per
	   connection would free it once for two requests. */
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));
	const int port = boundPort();

	{
		Held s(connectLoopback(port));
		REQUIRE(s.fd >= 0);

		std::string first = "POST /api/v1/echo HTTP/1.1\r\nHost: 127.0.0.1\r\n" +
		                    writeHeaderLine() + "Content-Length: 5\r\n\r\nfirst";
		std::string second = "GET /api/v1/fine HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
		REQUIRE(sendAll(s.fd, first));
		REQUIRE(sendAll(s.fd, second));

		// Both answers, whatever the reads they arrive in: the second one asks
		// for the connection to be closed, so this ends at the far end's close.
		const std::string got = readAtLeast(s.fd, 1u << 20);
		INFO(got);
		REQUIRE(countOf(got, "HTTP/1.1 200") == 2);
		REQUIRE(got.find("first") != std::string::npos);
		REQUIRE(got.find("{\"fine\":true}") != std::string::npos);
	}

	stop();
	REQUIRE(openRequestsForTest() == 0);
}

TEST_CASE("a query too long for the daemon never reaches a handler", "[bridge]")
{
	/* The router decodes a query into memory before it refuses any of it, so
	   what stops a caller from spending the box's memory on one request line
	   is the daemon's own pool. This is what says which of the two answers
	   first, and that the answer is a refusal rather than a handler. */
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));

	handler_ran = false;
	const int before = handler_calls;

	/* Written to the socket here rather than through the client beside this,
	   because the daemon answers and closes long before four megabytes have
	   been sent, and a client that writes the whole request before it reads
	   anything would only ever see the close. */
	std::string answer;
	{
		Held s(connectLoopback(boundPort()));
		REQUIRE(s.fd >= 0);
		std::string head = "GET /api/v1/fine?x=";
		head += std::string(4u << 20, 'q');
		head += " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
		answer = sendUntilAnswered(s.fd, head);
	}

	INFO(answer.substr(0, 200));
	REQUIRE(answer.compare(0, 9, "HTTP/1.1 ") == 0);
	/* The daemon's own pool for one connection is what refuses it, and it does
	   so at about thirty two kilobytes of request head. The route asked for
	   declares no parameter of any name, so a query the router had seen would
	   have been refused as one it does not take, and 400 here rather than 414
	   would say the whole four megabytes had been decoded into memory first. */
	REQUIRE(answer.compare(0, 12, "HTTP/1.1 414") == 0);
	REQUIRE(answer.find("400") == std::string::npos);
	REQUIRE_FALSE(handler_ran);
	REQUIRE(handler_calls == before);

	// Whatever it answered, the daemon is still serving afterwards.
	testhttp::Reply good = testhttp::request(boundPort(), "GET", "/api/v1/fine");
	REQUIRE(good.transport_ok);
	REQUIRE(good.code == 200);
}

TEST_CASE("an encoded separator does not become one on the way to the router", "[bridge]")
{
	/* The library hands the handler a target it has already decoded, and a
	   path decoded before it is split is a path where an escape can write a
	   separator: this one reads as the route's own path once decoded. What is
	   routed is therefore the target as it arrived, and the segment stays the
	   one segment it was sent as. */
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));

	handler_ran = false;
	const int before = handler_calls;
	testhttp::Reply r = testhttp::request(boundPort(), "GET", "/api%2fv1/fine");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 404);
	REQUIRE_FALSE(handler_ran);
	REQUIRE(handler_calls == before);
}

TEST_CASE("a request the daemon turns down itself leaves nothing behind", "[bridge]")
{
	/* State for a request is made as soon as the target is known, which is
	   before the daemon has decided the request is one it will pass on at all.
	   A request refused for the size of its head never reaches the handler, so
	   the notification the daemon makes when it is done with a request is the
	   only thing that can give that state back. */
	Serving guard;
	setRoutesForTest(&wired_table);
	REQUIRE(start(loopback()));

	handler_ran = false;
	std::string answer;
	{
		Held s(connectLoopback(boundPort()));
		REQUIRE(s.fd >= 0);
		std::string req = "GET /api/v1/fine HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n";
		req += "X-Pad: " + std::string(40000, 'p') + "\r\n\r\n";
		answer = sendUntilAnswered(s.fd, req);
	}
	INFO(answer.substr(0, 120));
	REQUIRE(answer.compare(0, 12, "HTTP/1.1 431") == 0);
	REQUIRE_FALSE(handler_ran);

	stop();
	REQUIRE(openRequestsForTest() == 0);
}

TEST_CASE("the shipped tables are the ones the server answers from again", "[bridge]")
{
	/* The seam the cases above install a table through has to put the shipped
	   list back, or every case after them is answered out of a table one of
	   them left behind. */
	setRoutesForTest(NULL);
	size_t count = 0;
	REQUIRE(allRoutes(&count) != NULL);
	REQUIRE(count > 0);

	Serving guard;
	REQUIRE(start(loopback()));
	testhttp::Reply r = testhttp::request(boundPort(), "GET", "/api/v1/fine");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 404);
}

TEST_CASE("the document is served plain, and compressed to a caller that takes that", "[bridge]")
{
	/* The transport is the only thing that sees what a caller accepts, and it
	   is where the choosing between the two forms happens, so nothing below
	   the socket can show that the right one goes out. What a handler answers
	   is the plain form; the second form only ever reaches a wire through
	   here. */
	Serving guard;
	setRoutesForTest(NULL);
	REQUIRE(start(loopback()));

	testhttp::Reply plain = testhttp::request(boundPort(), "GET", openapi::documentPath());
	REQUIRE(plain.transport_ok);
	REQUIRE(plain.code == 200);
	REQUIRE(plain.header("Content-Type") == "application/json");
	// What it names rather than how it is spelled: the answer varies on what
	// the caller takes and on what the caller presented, and the order the two
	// are named in is nobody's contract.
	REQUIRE(plain.header("Vary").find("Accept-Encoding") != std::string::npos);
	REQUIRE(plain.header("Vary").find("Authorization") != std::string::npos);
	REQUIRE(plain.header("Content-Encoding").empty());
	REQUIRE(plain.body == openapi::document());

	std::vector<std::pair<std::string, std::string> > takes_gzip;
	takes_gzip.push_back(std::make_pair(std::string("Accept-Encoding"), std::string("gzip")));

	testhttp::Reply packed =
		testhttp::request(boundPort(), "GET", openapi::documentPath(), takes_gzip);
	REQUIRE(packed.transport_ok);
	REQUIRE(packed.code == 200);
	REQUIRE(packed.header("Content-Encoding") == "gzip");
	REQUIRE(packed.body.size() < plain.body.size());
	/* The head of the plain answer, carried over rather than made again. The
	   answer varies by what the caller accepts whichever form went out, so
	   anything keeping a copy has to be told that on both of them, and it
	   varies by what the caller presented for the same reason. */
	REQUIRE(packed.header("Vary") == plain.header("Vary"));
	REQUIRE(packed.header("Vary").find("Accept-Encoding") != std::string::npos);
	REQUIRE(packed.header("Content-Type") == "application/json");
	// The bytes kept beside the document and not something compressed again
	// per request, which is the whole reason the second form is kept at all.
	REQUIRE(packed.body == openapi::documentGzip());

	// And a caller that named the coding to refuse it is answered plainly
	// rather than by what the letters of the header spell.
	std::vector<std::pair<std::string, std::string> > refuses_gzip;
	refuses_gzip.push_back(std::make_pair(std::string("Accept-Encoding"), std::string("gzip;q=0")));

	testhttp::Reply refused =
		testhttp::request(boundPort(), "GET", openapi::documentPath(), refuses_gzip);
	REQUIRE(refused.transport_ok);
	REQUIRE(refused.code == 200);
	REQUIRE(refused.header("Content-Encoding").empty());
	REQUIRE(refused.body == openapi::document());

	/* The same route reached by the other spelling of the same path. A segment
	   is decoded after the path is split, so this reaches the route, and what
	   chooses the form has to reach the same conclusion about it or the route
	   and the transport are on two different paths. */
	testhttp::Reply escaped =
		testhttp::request(boundPort(), "GET", "/api/openapi%2Ejson", takes_gzip);
	REQUIRE(escaped.transport_ok);
	REQUIRE(escaped.code == 200);
	REQUIRE(escaped.header("Content-Encoding") == "gzip");
	REQUIRE(escaped.body == openapi::documentGzip());
}

namespace
{

/* A session given back whichever line the case below leaves through. The table
   is process wide, so one left behind is one the cases after this are answered
   with. */
struct OpenSession
{
	std::string token;

	explicit OpenSession(const char *user) : token(openSession(user)) {}
	~OpenSession() { (void) closeSession(token); }

	private:
		OpenSession(const OpenSession &);
		OpenSession &operator=(const OpenSession &);
};

std::string csrfOutOf(const std::string &body)
{
	::Json::CharReaderBuilder builder;
	std::unique_ptr< ::Json::CharReader> reader(builder.newCharReader());
	::Json::Value root;
	std::string errs;
	REQUIRE(reader->parse(body.data(), body.data() + body.size(), &root, &errs));
	REQUIRE(root.isMember("csrf"));
	return root["csrf"].asString();
}

} // namespace

TEST_CASE("a page holding only its cookie is refused a write, and gets back what carries one", "[bridge]")
{
	/* The whole round a page walks after it has been reloaded, over a socket and out of
	   the shipped tables, because what it turns on is that the gate stands in front of
	   every route rather than what one function answers when called by hand.

	   A reloaded page holds the cookie and holds nothing of what the login route put in
	   the body. Every request that changes something and carries a live cookie is asked
	   for the second token, and the login route is such a request, so before this route
	   existed such a page could not write, log in again or log out.

	   Excusing login from the second token would have let another site post a login of
	   its choosing from the person's browser, so the first request below stays
	   refused. */
	setRoutesForTest(NULL);
	Serving guard;
	REQUIRE(start(loopback()));
	const int port = boundPort();

	OpenSession held("root");
	REQUIRE_FALSE(held.token.empty());

	std::vector<std::pair<std::string, std::string> > cookie_only = jsonHeaders();
	cookie_only.push_back(std::make_pair(std::string("Cookie"),
	                                     std::string(sessionCookieName()) + "=" + held.token));

	const std::string credential = "{\"user\":\"root\",\"password\":\"ni\"}";
	testhttp::Reply refused = testhttp::request(port, "POST", "/api/v1/login",
	                                            cookie_only, credential);
	REQUIRE(refused.transport_ok);
	/* Refused for want of the second token and not for a wrong password: the
	   gate answers before the route is reached, which is what makes a wrong
	   password a 401 and this a 403. */
	REQUIRE(refused.code == 403);
	REQUIRE(refused.body.find("carries no matching token") != std::string::npos);

	testhttp::Reply logout_refused = testhttp::request(port, "POST", "/api/v1/logout",
	                                                   cookie_only);
	REQUIRE(logout_refused.transport_ok);
	REQUIRE(logout_refused.code == 403);
	REQUIRE(sessionIsLive(held.token));

	// The way out: a GET is a safe method, so the gate lets one through on the
	// cookie alone.
	testhttp::Reply carried = testhttp::request(port, "GET", "/api/v1/session", cookie_only);
	REQUIRE(carried.transport_ok);
	REQUIRE(carried.code == 200);
	REQUIRE(carried.header("Content-Type") == "application/json");

	/* The two headers that make it safe to answer a second token over a method
	   another site can have a browser use. Nothing here says a cross-origin
	   script may read the answer, and nothing here lets it be pulled in as a
	   script instead. Read off the wire, because a header written by the
	   transport is not in what a handler returns. */
	REQUIRE(carried.header("Access-Control-Allow-Origin").empty());
	REQUIRE(carried.header("X-Content-Type-Options") == "nosniff");
	// And no cache between the two of them may hand this answer to the next
	// caller, who would be a different caller with a different session.
	REQUIRE(carried.header("Cache-Control") == "no-store");

	const std::string csrf = csrfOutOf(carried.body);
	REQUIRE_FALSE(csrf.empty());
	// The token belongs to a session and is not a session: the cookie it goes
	// with is what a session is, and that never travels in an answer.
	REQUIRE(carried.body.find(held.token) == std::string::npos);

	std::vector<std::pair<std::string, std::string> > carries_token = cookie_only;
	carries_token.push_back(std::make_pair(std::string(csrfHeaderName()), csrf));

	testhttp::Reply out = testhttp::request(port, "POST", "/api/v1/logout", carries_token);
	REQUIRE(out.transport_ok);
	REQUIRE(out.code == 204);
	REQUIRE_FALSE(sessionIsLive(held.token));
}
