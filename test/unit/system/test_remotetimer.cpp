/*
 * test_remotetimer.cpp - tests for what this box asks another box's API for
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

#include <system/remotetimer.h>

/* The progress window the HTTP client draws on is answered by the cases for that
   client, which are linked into this same binary. Nothing here draws, so there is
   nothing to answer twice. */

namespace
{

// One request as the other end read it.
struct Seen
{
	std::string line;
	std::vector<std::string> headers;
	std::string body;

	std::string method() const
	{
		const size_t space = line.find(' ');
		return space == std::string::npos ? line : line.substr(0, space);
	}

	std::string path() const
	{
		const size_t start = line.find(' ');
		if (start == std::string::npos)
			return "";
		const size_t stop = line.find(' ', start + 1);
		return line.substr(start + 1, stop == std::string::npos ? std::string::npos : stop - start - 1);
	}

	bool hasHeader(const std::string &name) const { return find(name) != (size_t) -1; }

	std::string header(const std::string &name) const
	{
		const size_t at = find(name);
		if (at == (size_t) -1)
			return "";

		size_t start = headers[at].find(':') + 1;
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

/* An answer machine on the loopback. The answers are queued before it starts, it
   hands out one per connection in the order they were queued, and what it read is
   compared once it has been stopped, so nothing is read while the thread writes. */
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

		unsigned int port() const { return (unsigned int) bound_port; }

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
						: std::string("HTTP/1.1 500 Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
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

std::string reply(const char *status, const std::string &body, const std::string &extra = "")
{
	char head[256];
	::snprintf(head, sizeof(head),
		"HTTP/1.1 %s\r\nContent-Type: application/json\r\nContent-Length: %u\r\nConnection: close\r\n",
		status, (unsigned) body.length());
	return std::string(head) + extra + "\r\n" + body;
}

const char kLoginAnswer[] =
	"{\"authenticated\":true,\"level\":\"write\",\"user\":\"ni\","
	"\"csrf\":\"second-token\",\"csrf_header\":\"X-CSRF-Token\",\"expires_in\":900}";

remotebox_address_t boxAt(unsigned int port)
{
	remotebox_address_t box;
	box.address = "127.0.0.1";
	box.port = port;
	box.user = "ni";
	box.pass = "ni";
	return box;
}

} // namespace

TEST_CASE("a remote box is read out of the one text both callers spell it in", "[remotetimer]")
{
	remotebox_address_t box;

	REQUIRE(parseRemoteBoxAddress("192.168.1.5", box));
	CHECK(box.address == "192.168.1.5");
	CHECK(box.port == 80);
	CHECK(box.user.empty());
	CHECK(box.pass.empty());

	box = remotebox_address_t();
	REQUIRE(parseRemoteBoxAddress("192.168.1.5:8080", box));
	CHECK(box.address == "192.168.1.5");
	CHECK(box.port == 8080);

	box = remotebox_address_t();
	REQUIRE(parseRemoteBoxAddress("ni:se:cret@box.local:81", box));
	CHECK(box.address == "box.local");
	CHECK(box.port == 81);
	CHECK(box.user == "ni");
	// The credential splits at its first colon, so a password may carry one.
	CHECK(box.pass == "se:cret");

	box = remotebox_address_t();
	REQUIRE(parseRemoteBoxAddress("ni@box.local", box));
	CHECK(box.user == "ni");
	CHECK(box.pass.empty());
	CHECK(box.address == "box.local");

	// A bare address of colons keeps every one of them.
	box = remotebox_address_t();
	REQUIRE(parseRemoteBoxAddress("fe80::1", box));
	CHECK(box.address == "fe80::1");
	CHECK(box.port == 80);

	box = remotebox_address_t();
	REQUIRE(parseRemoteBoxAddress("[fe80::1]:81", box));
	CHECK(box.address == "[fe80::1]");
	CHECK(box.port == 81);

	box = remotebox_address_t();
	CHECK_FALSE(parseRemoteBoxAddress("", box));
	CHECK_FALSE(parseRemoteBoxAddress("ni:ni@", box));
}

TEST_CASE("the two margins arrive in minutes and are handed on in seconds", "[remotetimer]")
{
	const std::string body =
		"{\"items\":["
		"{\"id\":\"recording_type\",\"value\":\"1\"},"
		"{\"id\":\"record_safety_time_before\",\"value\":\"3\"},"
		"{\"id\":\"record_safety_time_after\",\"value\":\"10\"}"
		"]}";

	int pre = -1, post = -1;
	REQUIRE(CRemoteTimerClient::readRecordingSafety(body, pre, post));
	CHECK(pre == 180);
	CHECK(post == 600);

	// A section that names only one of them says nothing about the other, and a
	// margin this guessed would shorten every recording fetched over.
	const std::string half = "{\"items\":[{\"id\":\"record_safety_time_before\",\"value\":\"3\"}]}";
	pre = -1;
	post = -1;
	CHECK_FALSE(CRemoteTimerClient::readRecordingSafety(half, pre, post));

	CHECK_FALSE(CRemoteTimerClient::readRecordingSafety("not json at all", pre, post));
	CHECK_FALSE(CRemoteTimerClient::readRecordingSafety("{\"items\":{}}", pre, post));
}

TEST_CASE("a timer list is read as this box's own rows", "[remotetimer]")
{
	const std::string body =
		"{\"items\":["
		"{\"id\":7,\"kind\":\"record\",\"channel_id\":\"361d03f300012b66\",\"start\":1700000000,"
		 "\"stop\":1700003600,\"title\":\"Tagesschau\",\"repeat\":770,\"repeat_count\":4,"
		 "\"state\":1,\"announce\":1699999880,\"epg_id\":\"12b6600004d2\",\"epg_start\":1700000100,"
		 "\"standby_on\":false,\"recording_dir\":\"/media/movies\"},"
		"{\"id\":8,\"kind\":\"zapto\",\"channel_id\":\"361d03f300012b67\",\"start\":1700000000,"
		 "\"stop\":0,\"title\":\"nope\",\"repeat\":0,\"repeat_count\":0,\"state\":0,"
		 "\"announce\":0,\"epg_id\":\"0\",\"epg_start\":0,\"standby_on\":false,\"recording_dir\":\"\"}"
		"]}";

	CTimerd::TimerList out;
	REQUIRE(CRemoteTimerClient::readTimers(body, "Wohnzimmer", out));

	// Recordings alone: the other kinds a box holds cannot be fetched here.
	REQUIRE(out.size() == 1);

	const CTimerd::responseGetTimer &t = out[0];
	CHECK(t.eventID == 7);
	CHECK(t.eventType == CTimerd::TIMER_REMOTEBOX);
	CHECK(t.eventState == CTimerd::TIMERSTATE_PREANNOUNCE);
	CHECK(std::string(t.remotebox_name) == "Wohnzimmer");
	CHECK(t.channel_id == 0x361d03f300012b66ULL);
	CHECK(t.epg_id == 0x12b6600004d2ULL);
	CHECK(t.alarmTime == 1700000000);
	CHECK(t.stopTime == 1700003600);
	CHECK(t.announceTime == 1699999880);
	CHECK(t.epg_starttime == 1700000100);
	CHECK(std::string(t.epgTitle) == "Tagesschau");
	CHECK(t.repeatCount == 4u);
	/* The whole number and not the lower nine bits of it: 770 is the weekday
	   repeat with Monday set, which the old wire carried as a masked number and
	   seven letters beside it. */
	CHECK((int) t.eventRepeat == 770);
	// The API neither reports nor takes a per timer audio selection.
	CHECK(t.apids == TIMERD_APIDS_CONF);

	CTimerd::TimerList none;
	CHECK_FALSE(CRemoteTimerClient::readTimers("{", "Wohnzimmer", none));
	CHECK_FALSE(CRemoteTimerClient::readTimers("{\"items\":7}", "Wohnzimmer", none));
	CHECK(none.empty());
}

TEST_CASE("what a timer is written as depends on whether it is being made", "[remotetimer]")
{
	remotetimer_t t;
	t.channel_id = 0x361d03f300012b66ULL;
	t.alarm = 100;
	t.stop = 200;
	t.announce = 40;
	t.epg_start = 110;
	t.repeat = 770;
	t.repeat_count = 3;

	/* The two flags are what the old wire spelt rs and aj: the other box widens
	   the recording by its own margins and follows its own guide. */
	const std::string made = CRemoteTimerClient::timerBody(t, true);
	CHECK(made == "{\"kind\":\"record\",\"channel_id\":\"361d03f300012b66\",\"start\":100,"
		"\"stop\":200,\"announce\":40,\"repeat\":770,\"repeat_count\":3,\"epg_start\":110,"
		"\"recording_safety\":true,\"auto_adjust\":true}");

	/* The route that corrects a timer takes neither the channel nor the guide
	   entry nor either flag, so naming any of them would be a member it refuses
	   the whole body over. */
	const std::string changed = CRemoteTimerClient::timerBody(t, false);
	CHECK(changed == "{\"start\":100,\"stop\":200,\"announce\":40,\"repeat\":770,\"repeat_count\":3}");
}

TEST_CASE("a channel the other box does not hold is an answer and not a fault", "[remotetimer]")
{
	StubServer server;
	server.answer(reply("404 Not Found", "{\"title\":\"no such channel\"}"));
	REQUIRE(server.start());

	CRemoteTimerClient client(boxAt(server.port()));
	std::string name;
	CHECK(client.getChannelName(0x1234ULL, name) == CRemoteTimerClient::RESULT_NOT_THERE);

	server.stop();
	REQUIRE(server.seen.size() == 1);
	CHECK(server.seen[0].method() == "GET");
	CHECK(server.seen[0].path() == "/api/v1/channels/1234");
}

TEST_CASE("a box that answers nothing is not a box that refused", "[remotetimer]")
{
	/* A port nothing is listening on. The connect timeout is short so the case
	   does not wait on it. */
	StubServer server;
	REQUIRE(server.start());
	const unsigned int port = server.port();
	server.stop();

	CRemoteTimerClient client(boxAt(port), 300);
	std::string name;
	CHECK(client.getChannelName(0x1234ULL, name) == CRemoteTimerClient::RESULT_NO_ANSWER);
}

TEST_CASE("a call refused for want of a session is retried behind a login", "[remotetimer]")
{
	StubServer server;
	server.answer(reply("401 Unauthorized", "{\"title\":\"no session\"}"));
	server.answer(reply("200 OK", kLoginAnswer,
		"Set-Cookie: ni-web-session=abc123; HttpOnly; SameSite=Lax; Path=/\r\n"));
	server.answer("HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
	REQUIRE(server.start());

	CRemoteTimerClient client(boxAt(server.port()));
	CHECK(client.removeTimer(7) == CRemoteTimerClient::RESULT_OK);

	server.stop();
	REQUIRE(server.seen.size() == 3);

	// The call as it stands first: a box that grants this without a session is
	// never asked for a credential.
	CHECK(server.seen[0].method() == "DELETE");
	CHECK(server.seen[0].path() == "/api/v1/timers/7");
	CHECK_FALSE(server.seen[0].hasHeader("Cookie"));
	CHECK_FALSE(server.seen[0].hasHeader("X-CSRF-Token"));

	CHECK(server.seen[1].method() == "POST");
	CHECK(server.seen[1].path() == "/api/v1/login");
	CHECK(server.seen[1].header("Content-Type") == "application/json");
	CHECK(server.seen[1].body == "{\"user\":\"ni\",\"password\":\"ni\"}");
	// The login itself carries no session, so the gate asks it for no token.
	CHECK_FALSE(server.seen[1].hasHeader("Cookie"));

	// And the retry carries both halves: the cookie the browser would keep, and
	// the second token in the header the answer itself named.
	CHECK(server.seen[2].method() == "DELETE");
	CHECK(server.seen[2].path() == "/api/v1/timers/7");
	CHECK(server.seen[2].header("Cookie") == "ni-web-session=abc123");
	CHECK(server.seen[2].header("X-CSRF-Token") == "second-token");
}

TEST_CASE("a second refusal is reported rather than logged in for again", "[remotetimer]")
{
	StubServer server;
	server.answer(reply("401 Unauthorized", "{\"title\":\"no session\"}"));
	server.answer(reply("200 OK", kLoginAnswer,
		"Set-Cookie: ni-web-session=abc123; HttpOnly; SameSite=Lax; Path=/\r\n"));
	server.answer(reply("401 Unauthorized", "{\"title\":\"still no\"}"));
	REQUIRE(server.start());

	CRemoteTimerClient client(boxAt(server.port()));
	CHECK(client.removeTimer(7) == CRemoteTimerClient::RESULT_NOT_PERMITTED);

	server.stop();
	CHECK(server.seen.size() == 3);
}

TEST_CASE("a login the box turns down is the refusal the caller is told about", "[remotetimer]")
{
	StubServer server;
	server.answer(reply("401 Unauthorized", "{\"title\":\"no session\"}"));
	server.answer(reply("401 Unauthorized", "{\"title\":\"wrong password\"}"));
	REQUIRE(server.start());

	CRemoteTimerClient client(boxAt(server.port()));
	CHECK(client.removeTimer(7) == CRemoteTimerClient::RESULT_NOT_PERMITTED);

	server.stop();
	CHECK(server.seen.size() == 2);
	CHECK(client.httpCode() == 401);
}

TEST_CASE("a timer sent over goes with this box's margins off and the other box told to put its own on", "[remotetimer]")
{
	StubServer server;
	server.answer(reply("201 Created", "{\"id\":\"9\"}"));
	REQUIRE(server.start());

	CTimerd::responseGetTimer timer = CTimerd::responseGetTimer();
	timer.eventType = CTimerd::TIMER_RECORD;
	timer.channel_id = 0x361d03f300012b66ULL;
	timer.alarmTime = 1000;
	timer.stopTime = 2000;
	timer.announceTime = 940;
	timer.epg_starttime = 1300;
	timer.eventRepeat = CTimerd::TIMERREPEAT_ONCE;
	timer.repeatCount = 0;

	CRemoteTimerClient client(boxAt(server.port()));
	// This box adds five minutes before and ten after.
	CHECK(client.addRecordTimer(timer, 300, 600) == CRemoteTimerClient::RESULT_OK);

	server.stop();
	/* One call and no second: what the other box's margins are is its own to
	   apply, so it is never asked for them on the way in. */
	REQUIRE(server.seen.size() == 1);

	/* The programme's own times, which is this box's five and ten minutes taken
	   back off: 1000 + 300, 940 + 300, 2000 - 600. The two flags are what the
	   old wire spelt rs and aj. */
	CHECK(server.seen[0].method() == "POST");
	CHECK(server.seen[0].path() == "/api/v1/timers");
	CHECK(server.seen[0].body ==
		"{\"kind\":\"record\",\"channel_id\":\"361d03f300012b66\",\"start\":1300,"
		"\"stop\":1400,\"announce\":1240,\"repeat\":0,\"repeat_count\":0,\"epg_start\":1300,"
		"\"recording_safety\":true,\"auto_adjust\":true}");
}

TEST_CASE("a timer off a box with no margins of its own goes over as it stands", "[remotetimer]")
{
	StubServer server;
	server.answer(reply("201 Created", "{\"id\":\"9\"}"));
	REQUIRE(server.start());

	CTimerd::responseGetTimer timer = CTimerd::responseGetTimer();
	timer.channel_id = 0x2bULL;
	timer.alarmTime = 1000;
	timer.stopTime = 2000;
	timer.announceTime = 940;

	CRemoteTimerClient client(boxAt(server.port()));
	CHECK(client.addRecordTimer(timer, 0, 0) == CRemoteTimerClient::RESULT_OK);

	server.stop();
	REQUIRE(server.seen.size() == 1);
	// Nothing to take off, and the flags are asked for all the same: whether the
	// recording is widened at the far end is that box's own setting.
	CHECK(server.seen[0].body ==
		"{\"kind\":\"record\",\"channel_id\":\"2b\",\"start\":1000,"
		"\"stop\":2000,\"announce\":940,\"repeat\":0,\"repeat_count\":0,\"epg_start\":0,"
		"\"recording_safety\":true,\"auto_adjust\":true}");
}

TEST_CASE("a list read stamps the other box's name and margins onto every row", "[remotetimer]")
{
	StubServer server;
	server.answer(reply("200 OK",
		"{\"items\":[{\"id\":7,\"kind\":\"record\",\"channel_id\":\"2b\",\"start\":100,"
		"\"stop\":200,\"title\":\"x\",\"repeat\":0,\"repeat_count\":0,\"state\":0,"
		"\"announce\":40,\"epg_id\":\"0\",\"epg_start\":0,\"standby_on\":false,"
		"\"recording_dir\":\"\"}]}"));
	server.answer(reply("200 OK",
		"{\"items\":["
		"{\"id\":\"record_safety_time_before\",\"value\":\"2\"},"
		"{\"id\":\"record_safety_time_after\",\"value\":\"5\"}"
		"]}"));
	REQUIRE(server.start());

	CTimerd::TimerList list;
	// A row that was already there is left alone.
	CTimerd::responseGetTimer local = CTimerd::responseGetTimer();
	local.eventType = CTimerd::TIMER_RECORD;
	list.push_back(local);

	CRemoteTimerClient client(boxAt(server.port()));
	CHECK(client.getTimers("Wohnzimmer", list) == CRemoteTimerClient::RESULT_OK);

	server.stop();
	REQUIRE(list.size() == 2);
	CHECK(list[0].eventType == CTimerd::TIMER_RECORD);
	CHECK(list[0].rem_pre == 0);
	CHECK(list[1].eventType == CTimerd::TIMER_REMOTEBOX);
	CHECK(std::string(list[1].remotebox_name) == "Wohnzimmer");
	CHECK(list[1].rem_pre == 120);
	CHECK(list[1].rem_post == 300);

	// The margins are read once and held, so a second timer costs no second call.
	int pre = 0, post = 0;
	CHECK(client.getRecordingSafety(pre, post) == CRemoteTimerClient::RESULT_OK);
	CHECK(pre == 120);
	CHECK(server.seen.size() == 2);
}

TEST_CASE("a password that would break the body goes out escaped", "[remotetimer]")
{
	StubServer server;
	server.answer(reply("401 Unauthorized", "{\"title\":\"no session\"}"));
	server.answer(reply("200 OK", kLoginAnswer,
		"Set-Cookie: ni-web-session=abc123; Path=/\r\n"));
	server.answer("HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
	REQUIRE(server.start());

	remotebox_address_t box = boxAt(server.port());
	box.pass = "a\"b\\c";
	CRemoteTimerClient client(box);
	CHECK(client.removeTimer(7) == CRemoteTimerClient::RESULT_OK);

	server.stop();
	REQUIRE(server.seen.size() == 3);
	CHECK(server.seen[1].body == "{\"user\":\"ni\",\"password\":\"a\\\"b\\\\c\"}");
}
