/*
 * test_httptool.cpp - tests for the box's HTTP client
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

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <system/httptool.h>

// The client is driven rather than copied, so what goes out on the wire is what
// the shipped object writes. The progress window is the only thing it needs
// besides, and it is declared here rather than included, so that no screen
// header reaches this binary. Only the names have to match, and a signature
// that changes breaks the link rather than a case.
class CProgressWindow
{
	public:
		unsigned int getGlobalStatus(void);
		void showLocalStatus(const unsigned int prog, const unsigned int max, const std::string &statusText);
		void showGlobalStatus(const unsigned int prog, const unsigned int max, const std::string &statusText);
};
unsigned int CProgressWindow::getGlobalStatus(void) { return 0; }
void CProgressWindow::showLocalStatus(const unsigned int, const unsigned int, const std::string &) {}
void CProgressWindow::showGlobalStatus(const unsigned int, const unsigned int, const std::string &) {}

namespace
{

// One request as the other end read it.
struct Seen
{
	std::string line;
	std::vector<std::string> headers;
	std::string body;

	bool hasHeader(const std::string &name) const
	{
		return find(name) != (size_t) -1;
	}

	// A header that is there but empty and a header that is not there are two
	// different things on this wire, which is why the two are asked separately.
	std::string header(const std::string &name) const
	{
		const size_t at = find(name);
		if (at == (size_t) -1)
			return "";

		const size_t colon = headers[at].find(':');
		size_t start = colon + 1;
		while (start < headers[at].length() && (headers[at][start] == ' ' || headers[at][start] == '\t'))
			++start;
		return headers[at].substr(start);
	}

	size_t find(const std::string &name) const
	{
		for (size_t i = 0; i < headers.size(); ++i)
		{
			const size_t colon = headers[i].find(':');
			if (colon == std::string::npos || colon != name.length())
				continue;

			bool same = true;
			for (size_t c = 0; c < name.length() && same; ++c)
				same = std::tolower((unsigned char) headers[i][c]) == std::tolower((unsigned char) name[c]);
			if (same)
				return i;
		}
		return (size_t) -1;
	}
};

// An answer machine on the loopback: the answers are queued before it starts,
// it hands out one per connection, and what it read is compared after it is
// stopped, so nothing is read while the thread still writes it.
class StubServer
{
	public:
		StubServer() : listen_fd(-1), bound_port(0), started(false), stopping(false), next(0) {}
		~StubServer() { stop(); }

		void answer(const std::string &raw) { replies.push_back(raw); }

		bool start()
		{
			listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
			if (listen_fd < 0)
				return false;

			int on = 1;
			::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

			struct sockaddr_in addr;
			::memset(&addr, 0, sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
			addr.sin_port = 0;
			if (::bind(listen_fd, (struct sockaddr *) &addr, sizeof(addr)) != 0)
				return false;
			if (::listen(listen_fd, 4) != 0)
				return false;

			socklen_t len = sizeof(addr);
			if (::getsockname(listen_fd, (struct sockaddr *) &addr, &len) != 0)
				return false;
			bound_port = ::ntohs(addr.sin_port);

			started = ::pthread_create(&thread, NULL, &StubServer::run, this) == 0;
			return started;
		}

		void stop()
		{
			if (started)
			{
				stopping = true;
				::pthread_join(thread, NULL);
				started = false;
			}
			if (listen_fd >= 0)
			{
				::close(listen_fd);
				listen_fd = -1;
			}
		}

		int port() const { return bound_port; }

		// Read after stop().
		std::vector<Seen> seen;

	private:
		StubServer(const StubServer &);
		StubServer &operator=(const StubServer &);

		static void *run(void *self)
		{
			((StubServer *) self)->serve();
			return NULL;
		}

		void serve()
		{
			while (!stopping)
			{
				struct pollfd p;
				p.fd = listen_fd;
				p.events = POLLIN;
				p.revents = 0;
				if (::poll(&p, 1, 50) <= 0)
					continue;

				const int fd = ::accept(listen_fd, NULL, NULL);
				if (fd < 0)
					continue;

				Seen got;
				if (read_request(fd, got))
				{
					seen.push_back(got);
					const std::string reply = next < replies.size()
						? replies[next]
						: std::string("HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
					++next;
					send_all(fd, reply);
				}
				::close(fd);
			}
		}

		static bool read_all(int fd, std::string &into, size_t want)
		{
			char buf[4096];
			while (into.length() < want)
			{
				const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
				if (n <= 0)
					return false;
				into.append(buf, (size_t) n);
			}
			return true;
		}

		static bool read_request(int fd, Seen &got)
		{
			std::string raw;
			char buf[4096];
			size_t head = std::string::npos;
			while (head == std::string::npos)
			{
				const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
				if (n <= 0)
					return false;
				raw.append(buf, (size_t) n);
				head = raw.find("\r\n\r\n");
			}

			const std::string headBlock = raw.substr(0, head);
			size_t at = headBlock.find("\r\n");
			got.line = headBlock.substr(0, at == std::string::npos ? headBlock.length() : at);
			while (at != std::string::npos)
			{
				const size_t start = at + 2;
				const size_t stop = headBlock.find("\r\n", start);
				got.headers.push_back(headBlock.substr(start, stop == std::string::npos ? std::string::npos : stop - start));
				at = stop;
			}

			got.body = raw.substr(head + 4);
			const std::string length = got.header("Content-Length");
			if (!length.empty())
			{
				const size_t want = (size_t) ::atol(length.c_str());
				if (!read_all(fd, got.body, want))
					return false;
			}
			return true;
		}

		static void send_all(int fd, const std::string &data)
		{
			size_t sent = 0;
			while (sent < data.length())
			{
				const ssize_t n = ::send(fd, data.data() + sent, data.length() - sent, MSG_NOSIGNAL);
				if (n <= 0)
					return;
				sent += (size_t) n;
			}
		}

		int listen_fd;
		int bound_port;
		bool started;
		volatile bool stopping;
		std::vector<std::string> replies;
		size_t next;
		pthread_t thread;
};

std::string urlFor(int port, const std::string &path)
{
	char buf[64];
	::snprintf(buf, sizeof(buf), "http://127.0.0.1:%d", port);
	return std::string(buf) + path;
}

std::string reply(const char *status, const std::string &body, const std::string &extra = "")
{
	char head[256];
	::snprintf(head, sizeof(head), "HTTP/1.1 %s\r\nContent-Type: application/json\r\nContent-Length: %u\r\nConnection: close\r\n",
		status, (unsigned) body.length());
	return std::string(head) + extra + "\r\n" + body;
}

} // namespace

TEST_CASE("a call carries its method, its type and its body", "[httptool]")
{
	StubServer server;
	server.answer(reply("201 Created", "{\"id\":7}"));
	REQUIRE(server.start());

	// Longer than one read at the other end, so that a body handed over in
	// pieces is what the case compares.
	const std::string sent = "{\"note\":\"" + std::string(6000, 'x') + "\"}";

	CHTTPTool tool;
	std::string got;
	REQUIRE(tool.sendRequest("POST", urlFor(server.port(), "/api/timers"), got,
		sent, "application/json"));
	REQUIRE(tool.getLastHttpCode() == 201);
	REQUIRE(got == "{\"id\":7}");

	server.stop();
	REQUIRE(server.seen.size() == 1);
	REQUIRE(server.seen[0].line == "POST /api/timers HTTP/1.1");
	REQUIRE(server.seen[0].header("Content-Type") == "application/json");
	REQUIRE(server.seen[0].header("Content-Length") == "6011");
	REQUIRE(server.seen[0].body == sent);
}

TEST_CASE("the other verbs reach the wire as themselves", "[httptool]")
{
	StubServer server;
	server.answer(reply("200 OK", "{}"));
	server.answer(reply("204 No Content", ""));
	server.answer(reply("200 OK", "{}"));
	REQUIRE(server.start());

	CHTTPTool tool;
	std::string got;
	REQUIRE(tool.sendRequest("PATCH", urlFor(server.port(), "/api/timers/7"), got, "{\"stop\":9}", "application/json"));
	REQUIRE(tool.sendRequest("DELETE", urlFor(server.port(), "/api/timers/7"), got));
	REQUIRE(tool.getLastHttpCode() == 204);
	REQUIRE(tool.sendRequest("GET", urlFor(server.port(), "/api/timers"), got));

	server.stop();
	REQUIRE(server.seen.size() == 3);
	REQUIRE(server.seen[0].line == "PATCH /api/timers/7 HTTP/1.1");
	REQUIRE(server.seen[0].body == "{\"stop\":9}");
	REQUIRE(server.seen[1].line == "DELETE /api/timers/7 HTTP/1.1");
	REQUIRE(server.seen[1].body == "");
	REQUIRE(server.seen[2].line == "GET /api/timers HTTP/1.1");

	// A GET is the one call that carries nothing, and a length of zero on it is
	// still a body as far as the other end is concerned.
	REQUIRE_FALSE(server.seen[2].hasHeader("Content-Length"));
}

TEST_CASE("a refusal keeps its status and its body", "[httptool]")
{
	// The part of an answer that says what was wrong with the call is its body,
	// and the transfer that carried it worked. Only the status says otherwise.
	StubServer server;
	server.answer(reply("409 Conflict", "{\"error\":\"overlaps\"}"));
	REQUIRE(server.start());

	CHTTPTool tool;
	std::string got;
	REQUIRE(tool.sendRequest("POST", urlFor(server.port(), "/api/timers"), got, "{}", "application/json"));
	REQUIRE(tool.getLastHttpCode() == 409);
	REQUIRE(got == "{\"error\":\"overlaps\"}");

	server.stop();
}

TEST_CASE("a call that never reaches anyone says so", "[httptool]")
{
	StubServer server;
	REQUIRE(server.start());
	const int port = server.port();
	server.stop();

	CHTTPTool tool;
	std::string got = "left over";
	REQUIRE_FALSE(tool.sendRequest("GET", urlFor(port, "/api/timers"), got));
	REQUIRE(tool.getLastHttpCode() == 0);
	REQUIRE(got == "");
}

TEST_CASE("the session is kept and handed back", "[httptool]")
{
	StubServer server;
	server.answer(reply("200 OK", "{\"csrf\":\"t0k\"}", "Set-Cookie: nisession=abc123; Path=/; HttpOnly\r\n"));
	server.answer(reply("200 OK", "{}"));
	REQUIRE(server.start());

	CHTTPTool tool;
	std::string got;
	REQUIRE(tool.sendRequest("POST", urlFor(server.port(), "/api/session"), got, "{}", "application/json"));
	REQUIRE(tool.sendRequest("GET", urlFor(server.port(), "/api/timers"), got));

	server.stop();
	REQUIRE(server.seen.size() == 2);

	// The first call has nothing to send yet, which is what makes the second
	// one evidence of something rather than of a header that is always there.
	REQUIRE_FALSE(server.seen[0].hasHeader("Cookie"));
	REQUIRE(server.seen[1].header("Cookie") == "nisession=abc123");
}

TEST_CASE("a session handed back empty is dropped", "[httptool]")
{
	StubServer server;
	server.answer(reply("200 OK", "{}", "Set-Cookie: nisession=abc123; Path=/\r\n"));
	server.answer(reply("200 OK", "{}", "Set-Cookie: nisession=; Max-Age=0\r\n"));
	server.answer(reply("200 OK", "{}"));
	REQUIRE(server.start());

	CHTTPTool tool;
	std::string got;
	REQUIRE(tool.sendRequest("POST", urlFor(server.port(), "/api/session"), got, "{}", "application/json"));
	REQUIRE(tool.sendRequest("DELETE", urlFor(server.port(), "/api/session"), got));
	REQUIRE(tool.sendRequest("GET", urlFor(server.port(), "/api/timers"), got));

	server.stop();
	REQUIRE(server.seen.size() == 3);
	REQUIRE(server.seen[1].header("Cookie") == "nisession=abc123");
	REQUIRE_FALSE(server.seen[2].hasHeader("Cookie"));
}

TEST_CASE("the session can be thrown away", "[httptool]")
{
	StubServer server;
	server.answer(reply("200 OK", "{}", "Set-Cookie: nisession=abc123\r\n"));
	server.answer(reply("200 OK", "{}"));
	REQUIRE(server.start());

	CHTTPTool tool;
	std::string got;
	REQUIRE(tool.sendRequest("POST", urlFor(server.port(), "/api/session"), got, "{}", "application/json"));
	tool.clearCookies();
	REQUIRE(tool.sendRequest("GET", urlFor(server.port(), "/api/timers"), got));

	server.stop();
	REQUIRE(server.seen.size() == 2);
	REQUIRE_FALSE(server.seen[1].hasHeader("Cookie"));
}

TEST_CASE("the headers the caller names go out as named", "[httptool]")
{
	StubServer server;
	server.answer(reply("200 OK", "{}"));
	REQUIRE(server.start());

	std::vector<std::string> headers;
	headers.push_back("X-CSRF-Token: t0k");
	headers.push_back("Accept: application/json");

	CHTTPTool tool;
	std::string got;
	REQUIRE(tool.sendRequest("DELETE", urlFor(server.port(), "/api/timers/7"), got, "", "", headers));

	server.stop();
	REQUIRE(server.seen.size() == 1);
	REQUIRE(server.seen[0].header("X-CSRF-Token") == "t0k");
	REQUIRE(server.seen[0].header("Accept") == "application/json");
}

TEST_CASE("a header or a method that cannot go out as written is refused", "[httptool]")
{
	// A line break in a header value is a second header, and a method with a
	// space in it is a request line of the caller's choosing. Either one sends
	// something nobody asked for, so nothing is sent at all.
	StubServer server;
	REQUIRE(server.start());

	std::vector<std::string> smuggled;
	smuggled.push_back("X-Thing: fine\r\nX-Other: not");

	CHTTPTool tool;
	std::string got;
	REQUIRE_FALSE(tool.sendRequest("POST", urlFor(server.port(), "/api/timers"), got, "{}", "application/json", smuggled));
	REQUIRE(tool.getLastHttpCode() == 0);

	REQUIRE_FALSE(tool.sendRequest("GET IT", urlFor(server.port(), "/api/timers"), got));
	REQUIRE_FALSE(tool.sendRequest("", urlFor(server.port(), "/api/timers"), got));
	REQUIRE_FALSE(tool.sendRequest("get", urlFor(server.port(), "/api/timers"), got));

	server.stop();
	REQUIRE(server.seen.empty());
}

TEST_CASE("a redirect is handed back rather than followed", "[httptool]")
{
	// Where the answer points is not where the body and the session may go
	// without the caller knowing, so the status is the answer.
	StubServer server;
	server.answer(reply("302 Found", "", "Location: /api/elsewhere\r\n"));
	REQUIRE(server.start());

	CHTTPTool tool;
	std::string got;
	REQUIRE(tool.sendRequest("POST", urlFor(server.port(), "/api/timers"), got, "{}", "application/json"));
	REQUIRE(tool.getLastHttpCode() == 302);

	server.stop();
	REQUIRE(server.seen.size() == 1);
}

TEST_CASE("the download calls read what the answer carried", "[httptool]")
{
	StubServer server;
	server.answer(reply("200 OK", "the body"));
	server.answer(reply("200 OK", "the other body"));
	REQUIRE(server.start());

	CHTTPTool tool;
	REQUIRE(tool.downloadString(urlFor(server.port(), "/one")) == "the body");
	REQUIRE(tool.getLastHttpCode() == 200);

	char target[] = "/tmp/httptool-XXXXXX";
	const int fd = ::mkstemp(target);
	REQUIRE(fd >= 0);
	::close(fd);
	REQUIRE(tool.downloadFile(urlFor(server.port(), "/two"), target));

	std::string written;
	FILE *f = ::fopen(target, "r");
	REQUIRE(f != NULL);
	char buf[64];
	size_t n;
	while ((n = ::fread(buf, 1, sizeof(buf), f)) > 0)
		written.append(buf, n);
	::fclose(f);
	::remove(target);
	REQUIRE(written == "the other body");

	server.stop();
	REQUIRE(server.seen.size() == 2);
	REQUIRE(server.seen[0].line == "GET /one HTTP/1.1");
	REQUIRE(server.seen[1].line == "GET /two HTTP/1.1");
}

TEST_CASE("the download calls still treat a refusal as a failure", "[httptool]")
{
	// The opposite of what a REST call does with the same answer, and the
	// callers of these two have counted on it since before there was one.
	StubServer server;
	server.answer(reply("404 Not Found", "no such thing"));
	server.answer(reply("404 Not Found", "no such thing"));
	REQUIRE(server.start());

	CHTTPTool tool;
	REQUIRE(tool.downloadString(urlFor(server.port(), "/gone")) == "");
	REQUIRE(tool.getLastHttpCode() == 404);

	char target[] = "/tmp/httptool-XXXXXX";
	const int fd = ::mkstemp(target);
	REQUIRE(fd >= 0);
	::close(fd);
	REQUIRE_FALSE(tool.downloadFile(urlFor(server.port(), "/gone"), target));
	::remove(target);

	server.stop();
}

TEST_CASE("the extra header follows a redirect only while the origin holds", "[httptool]")
{
	// The extra header is a key, and it is the reason these two do not let curl
	// follow on its own: a hop to somewhere else must not take the key along.
	StubServer far;
	far.answer(reply("200 OK", "far body"));
	REQUIRE(far.start());

	StubServer near_by;
	near_by.answer(reply("302 Found", "", "Location: /second\r\n"));
	near_by.answer(reply("302 Found", "", "Location: " + urlFor(far.port(), "/third") + "\r\n"));
	REQUIRE(near_by.start());

	CHTTPTool tool;
	tool.setExtraHeader("X-Tuxbox-Service-Key: secret");
	REQUIRE(tool.downloadString(urlFor(near_by.port(), "/first")) == "far body");

	near_by.stop();
	far.stop();

	REQUIRE(near_by.seen.size() == 2);
	REQUIRE(near_by.seen[0].header("X-Tuxbox-Service-Key") == "secret");
	REQUIRE(near_by.seen[1].line == "GET /second HTTP/1.1");
	REQUIRE(near_by.seen[1].header("X-Tuxbox-Service-Key") == "secret");

	REQUIRE(far.seen.size() == 1);
	REQUIRE(far.seen[0].line == "GET /third HTTP/1.1");
	REQUIRE_FALSE(far.seen[0].hasHeader("X-Tuxbox-Service-Key"));
}
