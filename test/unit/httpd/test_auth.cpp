/*
 * test_auth.cpp - tests for who may do what
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
#include "httpd/webconfig.h"
#include "httpd/credentials.h"
#include "httpd/endpoint.h"
#include "httpd/netmatch.h"
#include "httpd/router.h"
#include "httpd/server.h"

#include <limits>
#include <string>
#include <vector>

#include <time.h>

using namespace httpd;

namespace
{

/* Puts the policy and the walk interval back where the next case finds them.
   What is in effect is one value for the whole program, and a case that left a
   proxy list standing would turn the exemption off for every case after it. */
struct Policy
{
	Policy() {}
	~Policy()
	{
		setConfigForTest(defaultWebConfig());
		setSweepIntervalForTest(60);
	}

private:
	Policy(const Policy &);
	Policy &operator=(const Policy &);
};

// One line of configuration installed as the whole proxy list, and an empty
// line as no list at all.
void setTrustedProxies(const std::string &line)
{
	WebConfig c = config();
	c.trusted_proxies.clear();
	if (!line.empty())
	{
		NetPrefix p;
		REQUIRE(parsePrefix(line, &p));
		c.trusted_proxies.push_back(p);
	}
	setConfigForTest(c);
}

Credentials fromPeer(const std::string &peer)
{
	Credentials c;
	c.peer = peer;
	return c;
}

Credentials withCookie(const std::string &peer, const std::string &token)
{
	Credentials c = fromPeer(peer);
	c.cookie_token = token;
	return c;
}

Credentials withBearer(const std::string &peer, const std::string &token)
{
	Credentials c = fromPeer(peer);
	c.bearer_token = token;
	return c;
}

Credentials withQueryToken(const std::string &peer, const std::string &token)
{
	Credentials c = fromPeer(peer);
	c.query_token = token;
	return c;
}

// A token that stands for one part of the box, written down the way the route that
// mints one writes it down.
std::string scopedToken(const std::string &scope, AuthLevel level)
{
	const std::string token = randomToken();
	REQUIRE_FALSE(token.empty());
	addApiToken(tokenLookupPrefix(token), hashSecret(token), level, scope, 0);
	return token;
}

/* A session that was over the moment it was opened, which is what a cookie
   from last week is. The lifetime is put back before the case runs, so the
   session is dead and everything after it is the shipped policy. */
std::string expiredSession()
{
	WebConfig c = defaultWebConfig();
	c.session_lifetime_s = 0;
	setConfigForTest(c);
	const std::string token = openSession("root");
	setConfigForTest(defaultWebConfig());
	return token;
}

/* Puts the walk out of reach for the length of a case. One pass through the gate leaves
   the mark at now, and an interval this long leaves it not due again before the case
   ends. Both are needed: the interval alone does not stop a walk that has never
   happened, and a case about what a lookup refuses would otherwise be answered by the
   entry having been given back. */
void quietTheWalk()
{
	setSweepIntervalForTest(86400);
	granted(Credentials());
}

// A token this server never minted that begins with the same characters a
// minted one does, which is what a lookup by prefix has to survive.
std::string sharingThePrefixOf(const std::string &token)
{
	REQUIRE(token.size() > 8);
	std::string other = token;
	const size_t last = other.size() - 1;
	other[last] = (other[last] == '0') ? '1' : '0';
	return other;
}

} // namespace

TEST_CASE("what each way of arriving is granted", "[auth]")
{
	Policy back;

	/* A walk between the session below being opened and the row that reads it
	   would answer that row by the session no longer being there rather than
	   by its expiry. */
	quietTheWalk();

	struct Case
	{
		const char *what;
		Credentials c;
		AuthLevel   expect;
	};

	const std::string session = openSession("root");
	REQUIRE_FALSE(session.empty());

	const std::string token = randomToken();
	REQUIRE_FALSE(token.empty());
	addApiToken(tokenLookupPrefix(token), hashSecret(token), AuthLevel::Write);

	const std::string dead = expiredSession();
	REQUIRE_FALSE(dead.empty());

	const Case cases[] = {
		{ "nothing, from outside",   fromPeer("8.8.8.8"),               AuthLevel::Public },
		{ "nothing, from the LAN",   fromPeer("192.168.1.9"),           AuthLevel::Read   },
		{ "nothing, loopback",       fromPeer("127.0.0.1"),             AuthLevel::Read   },
		{ "nothing, loopback in v6", fromPeer("::1"),                   AuthLevel::Read   },
		{ "a session cookie",        withCookie("8.8.8.8", session),    AuthLevel::System },
		{ "a write token",           withBearer("8.8.8.8", token),      AuthLevel::Write  },
		{ "a token that is not one", withBearer("8.8.8.8", "deadbeef"), AuthLevel::Public },
		{ "an expired cookie",       withCookie("8.8.8.8", dead),       AuthLevel::Public },
		{ "no address at all",       fromPeer(""),                      AuthLevel::Public },
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
	{
		INFO(cases[i].what);
		REQUIRE(granted(cases[i].c) == cases[i].expect);
	}

	REQUIRE(closeSession(session));
}

TEST_CASE("a token that shares the first characters of a minted one is not that one", "[auth]")
{
	/* The record is found by the first characters of a token and confirmed
	   against the whole of it. A lookup that stopped at the prefix would hand
	   out the level of a token anybody could guess the front of, and the front
	   of it is short enough to guess. */
	Policy back;

	const std::string token = randomToken();
	REQUIRE_FALSE(token.empty());
	addApiToken(tokenLookupPrefix(token), hashSecret(token), AuthLevel::System);

	const std::string nearly = sharingThePrefixOf(token);
	REQUIRE(tokenLookupPrefix(nearly) == tokenLookupPrefix(token));
	REQUIRE(nearly != token);

	REQUIRE(granted(withBearer("8.8.8.8", token)) == AuthLevel::System);
	REQUIRE(granted(withBearer("8.8.8.8", nearly)) == AuthLevel::Public);
}

TEST_CASE("the LAN exemption never reaches a write", "[auth]")
{
	/* It raises to Read and to nothing else; a box on a shared network would
	   otherwise be one broadcast away from being reprogrammed. */
	Policy back;

	const Credentials lan = fromPeer("192.168.1.9");
	Response refusal;

	REQUIRE(allowed(AuthLevel::Read, granted(lan), Get, lan, &refusal));
	REQUIRE(refusal.code == 0);

	REQUIRE_FALSE(allowed(AuthLevel::Write, granted(lan), Post, lan, &refusal));
	REQUIRE(refusal.code == 403);
	REQUIRE(refusal.body.find("not-permitted") != std::string::npos);

	REQUIRE_FALSE(allowed(AuthLevel::System, granted(lan), Post, lan, &refusal));
	REQUIRE(refusal.code == 403);
	REQUIRE(refusal.body.find("not-permitted") != std::string::npos);
}

TEST_CASE("the exemption is off entirely once a proxy list exists", "[auth]")
{
	// Behind a proxy every request arrives from the loopback, so an exemption
	// would grant the whole internet a read.
	Policy back;

	setTrustedProxies("127.0.0.1/32");
	REQUIRE(granted(fromPeer("192.168.1.9")) == AuthLevel::Public);
	REQUIRE(granted(fromPeer("127.0.0.1")) == AuthLevel::Public);
	REQUIRE(granted(fromPeer("::1")) == AuthLevel::Public);

	setTrustedProxies("");
	REQUIRE(granted(fromPeer("192.168.1.9")) == AuthLevel::Read);
}

TEST_CASE("a forwarded address is believed only from a trusted peer", "[auth]")
{
	Policy back;

	setTrustedProxies("127.0.0.1/32");

	Credentials spoof = fromPeer("8.8.8.8");
	spoof.forwarded_for = "192.168.1.9";
	// The address the request is judged to have come from is the socket's, and
	// a caller that names another one in a header has named nothing.
	REQUIRE(clientAddress(spoof) == "8.8.8.8");
	REQUIRE(granted(spoof) == AuthLevel::Public);

	Credentials through = fromPeer("127.0.0.1");
	through.forwarded_for = "192.168.1.9";
	REQUIRE(clientAddress(through) == "192.168.1.9");

	/* Believed for what it is worth and no further: the exemption is off while
	   a proxy list exists, so an address a proxy vouched for is still nobody
	   until it presents something. */
	REQUIRE(granted(through) == AuthLevel::Public);
}

TEST_CASE("a forwarded header is read from its end", "[auth]")
{
	/* Every element in front of the last is whatever the client wrote: a proxy
	   appends what it saw to what it was given, so only the final element was
	   written by something on the list. */
	Policy back;

	setTrustedProxies("127.0.0.1/32");

	Credentials c = fromPeer("127.0.0.1");
	c.forwarded_for = "10.9.9.9, 8.8.8.8, 192.168.1.9";
	REQUIRE(clientAddress(c) == "192.168.1.9");

	// Two headers of that name are the same list, and the end of it is the end
	// of the second one.
	c.forwarded_for = "10.9.9.9,203.0.113.7";
	REQUIRE(clientAddress(c) == "203.0.113.7");
}

TEST_CASE("a forwarded element that is not an address is not believed", "[auth]")
{
	/* And the list is not walked back for an earlier element that does parse.
	   A client able to append one unusable element would otherwise choose
	   which of its own elements is believed. */
	Policy back;

	setTrustedProxies("127.0.0.1/32");

	Credentials c = fromPeer("127.0.0.1");
	c.forwarded_for = "192.168.1.9, not-an-address";
	REQUIRE(clientAddress(c) == "127.0.0.1");

	c.forwarded_for = "192.168.1.9, ";
	REQUIRE(clientAddress(c) == "127.0.0.1");

	c.forwarded_for = "";
	REQUIRE(clientAddress(c) == "127.0.0.1");

	// A port beside a v4 address is not part of the address, and a v6 address
	// written bare is all colons and must not be cut at one.
	c.forwarded_for = "192.168.1.9:51234";
	REQUIRE(clientAddress(c) == "192.168.1.9");

	c.forwarded_for = "fc00::5";
	REQUIRE(clientAddress(c) == "fc00::5");

	c.forwarded_for = "[fc00::5]:443";
	REQUIRE(clientAddress(c) == "fc00::5");
}

TEST_CASE("a forwarded address is read by nobody while there is no proxy list", "[auth]")
{
	// The shipped policy names no proxy, and under it the header is not a
	// thing this server reads at all.
	Policy back;

	Credentials c = fromPeer("8.8.8.8");
	c.forwarded_for = "192.168.1.9";
	REQUIRE(clientAddress(c) == "8.8.8.8");
	REQUIRE(granted(c) == AuthLevel::Public);
}

TEST_CASE("a cookie write without the double submit token is refused", "[auth]")
{
	Policy back;

	const std::string s = openSession("root");
	REQUIRE_FALSE(s.empty());

	Credentials c = withCookie("192.168.1.9", s);
	Response refusal;

	REQUIRE_FALSE(allowed(AuthLevel::Write, AuthLevel::System, Post, c, &refusal));
	REQUIRE(refusal.code == 403);
	REQUIRE(refusal.body.find("not-permitted") != std::string::npos);

	// Nothing that goes back names either secret.
	REQUIRE(refusal.body.find(s) == std::string::npos);
	REQUIRE(refusal.body.find(csrfFor(s)) == std::string::npos);

	c.csrf_header = csrfFor(s);
	REQUIRE_FALSE(c.csrf_header.empty());
	REQUIRE(allowed(AuthLevel::Write, AuthLevel::System, Post, c, &refusal));
	// An answer that says the request may go on carries no refusal to send.
	REQUIRE(refusal.code == 0);
	REQUIRE(refusal.body.empty());

	REQUIRE(closeSession(s));
}

TEST_CASE("a second token that is not this session's is no token", "[auth]")
{
	// The pair is a cookie and a header that belong to each other. A header
	// that is a token of some session would otherwise be a token of every one.
	Policy back;

	const std::string mine = openSession("root");
	const std::string theirs = openSession("root");
	REQUIRE_FALSE(mine.empty());
	REQUIRE_FALSE(theirs.empty());

	Credentials c = withCookie("8.8.8.8", mine);
	c.csrf_header = csrfFor(theirs);
	REQUIRE_FALSE(c.csrf_header.empty());

	Response refusal;
	REQUIRE_FALSE(allowed(AuthLevel::Write, AuthLevel::System, Post, c, &refusal));
	REQUIRE(refusal.code == 403);

	REQUIRE(closeSession(mine));
	REQUIRE(closeSession(theirs));
}

TEST_CASE("every method that changes something asks for the second token", "[auth]")
{
	// Named one by one, because a method added to the enum has to be one that
	// changes something until somebody says otherwise.
	Policy back;

	const std::string s = openSession("root");
	const Credentials c = withCookie("192.168.1.9", s);

	const Method changing[] = { Post, Put, Patch, Delete, UnknownMethod };
	for (size_t i = 0; i < sizeof(changing) / sizeof(changing[0]); ++i)
	{
		INFO(i);
		Response refusal;
		REQUIRE_FALSE(allowed(AuthLevel::Public, AuthLevel::System, changing[i], c, &refusal));
		REQUIRE(refusal.code == 403);
	}

	const Method asking[] = { Get, Head, Options };
	for (size_t i = 0; i < sizeof(asking) / sizeof(asking[0]); ++i)
	{
		INFO(i);
		Response refusal;
		REQUIRE(allowed(AuthLevel::Public, AuthLevel::System, asking[i], c, &refusal));
	}

	REQUIRE(closeSession(s));
}

TEST_CASE("a cookie read needs no token", "[auth]")
{
	Policy back;

	const std::string s = openSession("root");
	const Credentials c = withCookie("192.168.1.9", s);
	Response refusal;

	REQUIRE(allowed(AuthLevel::Read, AuthLevel::System, Get, c, &refusal));

	REQUIRE(closeSession(s));
}

TEST_CASE("a cookie naming no session leaves an anonymous change where it was", "[auth]")
{
	/* A cookie that resolves to nothing granted nothing and there is nothing to
	   protect. Asking for a second token there would refuse every anonymous
	   request from a browser still holding a cookie from last week. */
	Policy back;

	const Credentials c = withCookie("192.168.1.9", "not-a-session");
	Response refusal;

	REQUIRE(allowed(AuthLevel::Public, granted(c), Post, c, &refusal));
	REQUIRE(granted(c) == AuthLevel::Read);
}

TEST_CASE("the bearer path needs no double submit token", "[auth]")
{
	// A header nobody can set cross site is not forgeable cross site either.
	Policy back;

	const std::string t = randomToken();
	addApiToken(tokenLookupPrefix(t), hashSecret(t), AuthLevel::System);

	const Credentials c = withBearer("8.8.8.8", t);
	Response refusal;
	REQUIRE(allowed(AuthLevel::Write, AuthLevel::System, Post, c, &refusal));
}

TEST_CASE("a token for one level does not reach the next", "[auth]")
{
	Policy back;

	const std::string t = randomToken();
	addApiToken(tokenLookupPrefix(t), hashSecret(t), AuthLevel::Read);

	const Credentials c = withBearer("8.8.8.8", t);
	Response refusal;

	REQUIRE(granted(c) == AuthLevel::Read);
	REQUIRE(allowed(AuthLevel::Read, granted(c), Get, c, &refusal));
	REQUIRE_FALSE(allowed(AuthLevel::Write, granted(c), Post, c, &refusal));
	REQUIRE(refusal.code == 403);
}

TEST_CASE("an Authorization header that is not a bearer one carries no token", "[auth]")
{
	/* A password a browser was talked into putting in a URL arrives as a Basic
	   header. Reading the part after the scheme name whatever the scheme was
	   would turn that into a token to look up, and would also skip the second
	   token check on the strength of it. */
	REQUIRE(bearerToken("Bearer abc123") == "abc123");
	REQUIRE(bearerToken("bearer abc123") == "abc123");
	REQUIRE(bearerToken("BEARER   abc123  ") == "abc123");
	REQUIRE(bearerToken("Bearer\tabc123") == "abc123");

	REQUIRE(bearerToken("Basic YWJjOjEyMw==").empty());
	REQUIRE(bearerToken("Bearerabc123").empty());
	REQUIRE(bearerToken("Bearer").empty());
	REQUIRE(bearerToken("Bearer ").empty());
	REQUIRE(bearerToken("").empty());
	REQUIRE(bearerToken("abc123").empty());
}

TEST_CASE("a closed session stops working at once", "[auth]")
{
	Policy back;

	const std::string s = openSession("root");
	REQUIRE(granted(withCookie("8.8.8.8", s)) == AuthLevel::System);
	REQUIRE(closeSession(s));
	REQUIRE(granted(withCookie("8.8.8.8", s)) == AuthLevel::Public);

	// And there is nothing left under it to close a second time.
	REQUIRE_FALSE(closeSession(s));
	REQUIRE(csrfFor(s).empty());
}

TEST_CASE("a session that has run out is refused before anything walks the table", "[auth]")
{
	/* The walk happens on a timer. A session that stayed usable until the next
	   walk came round would be a session whose lifetime was the setting plus
	   however long that timer had left. */
	Policy back;

	// Otherwise the request below walks the table itself, and the session is
	// refused for having been given back rather than for having run out.
	quietTheWalk();

	const std::string dead = expiredSession();
	REQUIRE_FALSE(dead.empty());

	// Still in the table, and already refused.
	REQUIRE(sessionCount() > 0);
	REQUIRE(granted(withCookie("127.0.0.1", dead)) == AuthLevel::Read);
	REQUIRE(granted(withCookie("8.8.8.8", dead)) == AuthLevel::Public);
	REQUIRE(csrfFor(dead).empty());
}

TEST_CASE("sessions are dropped when they expire rather than accumulating", "[auth]")
{
	Policy back;

	const size_t before = sessionCount();
	REQUIRE_FALSE(openSession("root").empty());
	REQUIRE_FALSE(openSession("root").empty());
	REQUIRE(sessionCount() == before + 2);

	expireSessions(time(NULL) + 86400 + 1);
	REQUIRE(sessionCount() == 0);
}

TEST_CASE("the gate is what gives back what has run out", "[auth]")
{
	/* Nothing here has a thread of its own, so the walk happens on the way
	   through the gate or it happens never, and a table that only grows is a
	   leak anybody who can reach the port can pull on. */
	Policy back;

	expireSessions(time(NULL) + 86400 + 1);
	REQUIRE(sessionCount() == 0);

	const std::string dead = expiredSession();
	REQUIRE_FALSE(dead.empty());
	REQUIRE(sessionCount() == 1);

	setSweepIntervalForTest(0);
	REQUIRE(granted(fromPeer("8.8.8.8")) == AuthLevel::Public);
	REQUIRE(sessionCount() == 0);
}

TEST_CASE("a session still standing survives the walk", "[auth]")
{
	// The other direction of the same walk: one that dropped everything would
	// pass the case above and log everybody out.
	Policy back;

	const std::string s = openSession("root");
	REQUIRE_FALSE(s.empty());

	setSweepIntervalForTest(0);
	REQUIRE(granted(fromPeer("8.8.8.8")) == AuthLevel::Public);

	REQUIRE(granted(withCookie("8.8.8.8", s)) == AuthLevel::System);
	REQUIRE(closeSession(s));
}

TEST_CASE("two sessions opened together get different tokens", "[auth]")
{
	Policy back;

	const std::string a = openSession("root");
	const std::string b = openSession("root");
	REQUIRE_FALSE(a.empty());
	REQUIRE(a != b);
	REQUIRE(csrfFor(a) != csrfFor(b));
	REQUIRE(csrfFor(a) != a);

	REQUIRE(closeSession(a));
	REQUIRE(closeSession(b));
}

/* Everything above drives the gate where it is decided. What follows drives it
   where it is reached, because a gate nothing calls refuses nothing. */

namespace
{

bool ran = false;

Response answer(const Request &)
{
	ran = true;
	Response out;
	out.code = StatusOk;
	out.content_type = "application/json";
	out.body = "{}";
	return out;
}

// Both addresses a handler is given, so that a case can say which of the two
// carries what rather than only that one of them does.
Response bothAddresses(const Request &r)
{
	ran = true;
	Response out;
	out.code = StatusOk;
	out.content_type = "text/plain";
	out.body = r.peer() + "|" + r.reportedPeer();
	return out;
}

const Endpoint gated_endpoints[] = {
	{ Get,  "/api/v1/read",   AuthLevel::Read,   "asks",       NULL, 0, NULL, &answer, false },
	{ Get,  "/api/v1/secret", AuthLevel::System, "asks a lot", NULL, 0, NULL, &answer, false },
	{ Get,  "/api/v1/where",  AuthLevel::Read,   "names both", NULL, 0, NULL, &bothAddresses, false },
	{ Post, "/api/v1/change", AuthLevel::Write,  "changes",    NULL, 0, NULL, &answer, false },
};

const RouteTable gated_table = { HTTPD_TABLE("gated", gated_endpoints) };

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
		setConfigForTest(defaultWebConfig());
	}

private:
	Serving(const Serving &);
	Serving &operator=(const Serving &);
};

ServerConfig loopback()
{
	ServerConfig c = defaultConfig();
	c.port = 0;
	c.bind_address = "127.0.0.1";
	return c;
}

std::vector<std::pair<std::string, std::string> > oneHeader(const std::string &name,
                                                            const std::string &value)
{
	std::vector<std::pair<std::string, std::string> > h;
	h.push_back(std::make_pair(name, value));
	return h;
}

std::string cookieFor(const std::string &token)
{
	return std::string(sessionCookieName()) + "=" + token;
}

} // namespace

TEST_CASE("a session cookie off the wire reaches what the loopback cannot", "[auth]")
{
	/* The loopback is granted a read by the exemption, so the endpoint that
	   asks for everything is what says a credential was resolved at all rather
	   than the address having answered for it. */
	Serving guard;
	setRoutesForTest(&gated_table);
	REQUIRE(start(loopback()));

	ran = false;
	testhttp::Reply anonymous = testhttp::request(boundPort(), "GET", "/api/v1/secret");
	REQUIRE(anonymous.transport_ok);
	REQUIRE(anonymous.code == 403);
	REQUIRE(anonymous.body.find("not-permitted") != std::string::npos);
	REQUIRE_FALSE(ran);

	const std::string s = openSession("root");
	REQUIRE_FALSE(s.empty());

	testhttp::Reply named = testhttp::request(boundPort(), "GET", "/api/v1/secret",
	                                          oneHeader("Cookie", cookieFor(s)));
	REQUIRE(named.transport_ok);
	REQUIRE(named.code == 200);
	REQUIRE(ran);

	REQUIRE(closeSession(s));
}

TEST_CASE("a cookie that was closed stops working on the wire too", "[auth]")
{
	Serving guard;
	setRoutesForTest(&gated_table);
	REQUIRE(start(loopback()));

	const std::string s = openSession("root");
	REQUIRE(closeSession(s));

	testhttp::Reply r = testhttp::request(boundPort(), "GET", "/api/v1/secret",
	                                      oneHeader("Cookie", cookieFor(s)));
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 403);
}

TEST_CASE("a change on a cookie is refused on the wire without the second token", "[auth]")
{
	Serving guard;
	setRoutesForTest(&gated_table);
	REQUIRE(start(loopback()));

	const std::string s = openSession("root");
	REQUIRE_FALSE(s.empty());

	ran = false;
	testhttp::Reply without = testhttp::request(boundPort(), "POST", "/api/v1/change",
	                                            oneHeader("Cookie", cookieFor(s)), "{}");
	REQUIRE(without.transport_ok);
	REQUIRE(without.code == 403);
	REQUIRE(without.body.find("not-permitted") != std::string::npos);
	REQUIRE(without.body.find(s) == std::string::npos);
	REQUIRE(without.body.find(csrfFor(s)) == std::string::npos);
	REQUIRE_FALSE(ran);

	std::vector<std::pair<std::string, std::string> > both = oneHeader("Cookie", cookieFor(s));
	both.push_back(std::make_pair(std::string(csrfHeaderName()), csrfFor(s)));

	testhttp::Reply with = testhttp::request(boundPort(), "POST", "/api/v1/change", both, "{}");
	REQUIRE(with.transport_ok);
	REQUIRE(with.code == 200);
	REQUIRE(ran);

	REQUIRE(closeSession(s));
}

TEST_CASE("a bearer token off the wire is worth what it was minted at", "[auth]")
{
	Serving guard;
	setRoutesForTest(&gated_table);
	REQUIRE(start(loopback()));

	const std::string t = randomToken();
	addApiToken(tokenLookupPrefix(t), hashSecret(t), AuthLevel::Write);

	// It changes something with no second token, which is the whole of what a
	// header nobody can set from another site buys.
	ran = false;
	testhttp::Reply change = testhttp::request(boundPort(), "POST", "/api/v1/change",
	                                           oneHeader("Authorization", "Bearer " + t), "{}");
	REQUIRE(change.transport_ok);
	REQUIRE(change.code == 200);
	REQUIRE(ran);

	// And it does not reach the level above the one it was minted at.
	ran = false;
	testhttp::Reply above = testhttp::request(boundPort(), "GET", "/api/v1/secret",
	                                          oneHeader("Authorization", "Bearer " + t));
	REQUIRE(above.transport_ok);
	REQUIRE(above.code == 403);
	REQUIRE_FALSE(ran);
}

TEST_CASE("a path this server does not have says so to a caller carrying nothing", "[auth]")
{
	/* The gate answers before a route is matched, so a caller carrying nothing is told
	   the path is not there by the router and not by the gate.

	   That the two answers differ, and that a caller can therefore tell a route it may
	   not reach from a path this server does not have, is decided and not overlooked.
	   What has to stay secret is the credential and not the shape of the surface: the
	   firmware is open source and carries these tables, and answering nothing to
	   everything would cost every later reading of a log the difference between a
	   caller who may not and a caller who mistyped. */
	Serving guard;
	setRoutesForTest(&gated_table);
	REQUIRE(start(loopback()));

	testhttp::Reply missing = testhttp::request(boundPort(), "POST", "/api/v1/nowhere",
	                                            std::vector<std::pair<std::string, std::string> >(), "{}");
	REQUIRE(missing.transport_ok);
	REQUIRE(missing.code == 404);
	REQUIRE(missing.body.find("no-such-route") != std::string::npos);

	// And so does one that exists but is above what the caller has, which is
	// the same answer the router gives for a path it does have.
	testhttp::Reply there = testhttp::request(boundPort(), "POST", "/api/v1/change",
	                                          std::vector<std::pair<std::string, std::string> >(), "{}");
	REQUIRE(there.transport_ok);
	REQUIRE(there.code == 403);
}

TEST_CASE("a forwarded address off the wire reaches the handler and not the exemption", "[auth]")
{
	Serving guard;
	setRoutesForTest(&gated_table);

	WebConfig c = defaultWebConfig();
	NetPrefix p;
	REQUIRE(parsePrefix("127.0.0.1/32", &p));
	c.trusted_proxies.push_back(p);
	setConfigForTest(c);

	REQUIRE(start(loopback()));

	/* The exemption is off while a proxy list exists, so the request that
	   would have been granted a read by its address is granted nothing, and
	   that is true of the proxy's own address as much as of the one it
	   forwarded. */
	testhttp::Reply r = testhttp::request(boundPort(), "GET", "/api/v1/read",
	                                      oneHeader("X-Forwarded-For", "192.168.1.9"));
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 403);
}

TEST_CASE("a forwarded header sent twice is one list read from its end", "[auth]")
{
	/* Two header lines of that name are one list, and the library's own lookup answers
	   the first of them, which is the element written furthest from this server and so
	   the one the caller wrote. This is what says the transport collects them all.
	   Driven through a socket and not against the resolver, because what is pinned is
	   how the request is taken apart. */
	Serving guard;
	setRoutesForTest(&gated_table);

	WebConfig cfg = defaultWebConfig();
	NetPrefix p;
	REQUIRE(parsePrefix("127.0.0.1/32", &p));
	cfg.trusted_proxies.push_back(p);
	setConfigForTest(cfg);

	REQUIRE(start(loopback()));

	// The exemption is off while a proxy list exists, so the route is reached
	// on a credential and not on the address.
	const std::string t = randomToken();
	addApiToken(tokenLookupPrefix(t), hashSecret(t), AuthLevel::Read);

	std::vector<std::pair<std::string, std::string> > h =
		oneHeader("Authorization", "Bearer " + t);
	h.push_back(std::make_pair(std::string("X-Forwarded-For"), std::string("1.1.1.1")));
	h.push_back(std::make_pair(std::string("X-Forwarded-For"), std::string("2.2.2.2")));

	ran = false;
	testhttp::Reply two = testhttp::request(boundPort(), "GET", "/api/v1/where", h);
	REQUIRE(two.transport_ok);
	REQUIRE(two.code == 200);
	REQUIRE(ran);
	REQUIRE(two.body == "127.0.0.1|2.2.2.2");

	// And one line carrying two elements is the same list read the same way.
	std::vector<std::pair<std::string, std::string> > one =
		oneHeader("Authorization", "Bearer " + t);
	one.push_back(std::make_pair(std::string("X-Forwarded-For"),
	                             std::string("1.1.1.1, 3.3.3.3")));

	testhttp::Reply joined = testhttp::request(boundPort(), "GET", "/api/v1/where", one);
	REQUIRE(joined.transport_ok);
	REQUIRE(joined.code == 200);
	REQUIRE(joined.body == "127.0.0.1|3.3.3.3");
}

TEST_CASE("what a handler is told the request came from is what the socket said", "[auth]")
{
	/* A caller behind a proxy writes its own forwarded header and the proxy
	   appends to it, so the resolved address is only ever as good as that
	   proxy. A handler that answers differently to the box itself compares an
	   address, and the address it compares must be the one no caller wrote. */
	Serving guard;
	setRoutesForTest(&gated_table);

	WebConfig cfg = defaultWebConfig();
	NetPrefix p;
	REQUIRE(parsePrefix("127.0.0.1/32", &p));
	cfg.trusted_proxies.push_back(p);
	setConfigForTest(cfg);

	REQUIRE(start(loopback()));

	const std::string t = randomToken();
	addApiToken(tokenLookupPrefix(t), hashSecret(t), AuthLevel::Read);

	// Every one of these is a caller naming an address it is not at, and the
	// loopback is the one that would buy something.
	const char *chosen[] = { "127.0.0.1", "::1", "10.0.0.1" };
	for (size_t i = 0; i < sizeof(chosen) / sizeof(chosen[0]); ++i)
	{
		INFO(chosen[i]);
		std::vector<std::pair<std::string, std::string> > h =
			oneHeader("Authorization", "Bearer " + t);
		h.push_back(std::make_pair(std::string("X-Forwarded-For"), std::string(chosen[i])));

		testhttp::Reply r = testhttp::request(boundPort(), "GET", "/api/v1/where", h);
		REQUIRE(r.transport_ok);
		REQUIRE(r.code == 200);
		REQUIRE(r.body == std::string("127.0.0.1|") + chosen[i]);
	}
}

TEST_CASE("the address a handler is told about is the socket's when nothing forwards", "[auth]")
{
	// The other half: with no proxy list the two are the same address, and a
	// handler reading either of them reads the socket.
	Serving guard;
	setRoutesForTest(&gated_table);
	REQUIRE(start(loopback()));

	testhttp::Reply r = testhttp::request(boundPort(), "GET", "/api/v1/where",
	                                      oneHeader("X-Forwarded-For", "10.0.0.1"));
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.body == "127.0.0.1|127.0.0.1");
}

TEST_CASE("a bearer header that resolves to nothing does not excuse a cookie", "[auth]")
{
	/* The exemption is worth having because no other site can make a browser
	   send that header. Presence alone would have been the wider rule: any
	   bytes at all in it, from anywhere, and the second token stops being
	   asked for. What excuses the request is a token that was confirmed. */
	Policy back;

	const std::string s = openSession("root");
	REQUIRE_FALSE(s.empty());

	Credentials c = withCookie("192.168.1.9", s);
	c.bearer_token = "00";

	Response refusal;
	REQUIRE_FALSE(allowed(AuthLevel::Write, AuthLevel::System, Post, c, &refusal));
	REQUIRE(refusal.code == 403);

	// A token that does resolve excuses it, which is the rule doing its work
	// rather than refusing everything.
	const std::string t = randomToken();
	addApiToken(tokenLookupPrefix(t), hashSecret(t), AuthLevel::Write);
	c.bearer_token = t;
	REQUIRE(allowed(AuthLevel::Write, AuthLevel::System, Post, c, &refusal));

	// And a token that looks like nothing this could have issued is no better
	// than the junk one.
	c.bearer_token = "not hex at all";
	REQUIRE_FALSE(allowed(AuthLevel::Write, AuthLevel::System, Post, c, &refusal));

	REQUIRE(closeSession(s));
}

TEST_CASE("a cookie beside a worthless bearer is refused on the wire too", "[auth]")
{
	Serving guard;
	setRoutesForTest(&gated_table);
	REQUIRE(start(loopback()));

	const std::string s = openSession("root");
	REQUIRE_FALSE(s.empty());

	std::vector<std::pair<std::string, std::string> > h = oneHeader("Cookie", cookieFor(s));
	h.push_back(std::make_pair(std::string("Authorization"), std::string("Bearer 00")));

	ran = false;
	testhttp::Reply r = testhttp::request(boundPort(), "POST", "/api/v1/change", h, "{}");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 403);
	REQUIRE_FALSE(ran);

	REQUIRE(closeSession(s));
}

TEST_CASE("a session opened on a clock before the epoch is already over", "[auth]")
{
	/* Every input this file cannot use answers the least it can mean. A clock
	   reading before the epoch is one the arithmetic cannot be done against,
	   and the answer is a session already over rather than one that never
	   runs out. */
	REQUIRE(expiryForTest(-1, 86400) <= -1);
	REQUIRE(expiryForTest(0, 86400) == 86400);

	// The ceiling is still the ceiling and not a wrap into the past.
	const time_t max = std::numeric_limits<time_t>::max();
	REQUIRE(expiryForTest(max - 10, 86400) == max);
	REQUIRE(expiryForTest(max, 1) == max);
}

TEST_CASE("both floors turn a caller down with one document", "[auth]")
{
	/* The level is refused by the gate at the transport and by the router at
	   the route, and the two sentences are one sentence. Two copies would be
	   two answers a caller can tell apart the moment one of them is reworded,
	   and telling them apart is being told which floor said no. */
	Policy back;

	const Credentials lan = fromPeer("192.168.1.9");
	Response fromGate;
	REQUIRE_FALSE(allowed(AuthLevel::System, granted(lan), Get, lan, &fromGate));

	const Response fromRouter = dispatchIn(gated_table, Get, "/api/v1/secret", "", "",
	                                       "192.168.1.9", AuthLevel::Read);

	REQUIRE(fromGate.code == 403);
	REQUIRE(fromRouter.code == fromGate.code);
	REQUIRE(fromRouter.content_type == fromGate.content_type);
	REQUIRE(fromRouter.body == fromGate.body);
}

TEST_CASE("a route that does not take one is not reached by a token in the query", "[auth]")
{
	/* THE CASE THAT MATTERS MOST HERE, and the one the success below is worth nothing
	   without: the flag is the whole of what keeps a credential out of the address of
	   every other route this server has, so what has to be shown is a refusal and not a
	   grant. A token in the query is read off every request, because what a request
	   carries is not decided by what it asked for; what decides is the route. */
	Policy back;
	forgetApiTokens();

	const std::string token = scopedToken("media", AuthLevel::System);
	const Credentials c = withQueryToken("8.8.8.8", token);

	// The default, which is what every route that says nothing gets.
	REQUIRE(granted(c) == AuthLevel::Public);
	REQUIRE(granted(c, false) == AuthLevel::Public);

	std::string scope = "left over";
	REQUIRE(granted(c, false, &scope) == AuthLevel::Public);
	REQUIRE(scope.empty());

	// And on a route that does take one, the same request is the token's level.
	REQUIRE(granted(c, true, &scope) == AuthLevel::System);
	REQUIRE(scope == "media");

	forgetApiTokens();
}

TEST_CASE("a session token in the query is never a credential", "[auth]")
{
	/* A cookie in an address outlives the request in a history list, in a proxy's log
	   and in whatever the address was pasted into, which is what a cookie exists to
	   avoid. So the query carries tokens and nothing else, on the route that takes one
	   as much as on the routes that do not.

	   Driven by putting a live session's own token where a token out of the query goes:
	   it is a string this server minted and it resolves to System when it arrives as a
	   cookie, so a gate that read the query as a second place for one would answer
	   System here. */
	Policy back;
	forgetApiTokens();

	const std::string session = openSession("root");
	REQUIRE_FALSE(session.empty());
	REQUIRE(sessionLevel(session) == AuthLevel::System);

	const Credentials as_cookie = withCookie("8.8.8.8", session);
	REQUIRE(granted(as_cookie, true) == AuthLevel::System);

	const Credentials in_query = withQueryToken("8.8.8.8", session);
	REQUIRE(granted(in_query) == AuthLevel::Public);
	REQUIRE(granted(in_query, true) == AuthLevel::Public);

	(void) closeSession(session);
}

TEST_CASE("a token with a scope carries it whichever way it arrives, and reaches nothing without it", "[auth]")
{
	/* The scope has to bite on the header as well. A token that was narrow in an
	   address and wide in a header would be narrow for exactly as long as nobody moved
	   it, and moving it is one line of curl. So a scoped token is worth its level only
	   where a route reads a scope, and elsewhere it is a credential this request cannot
	   use rather than a credential worth everything. */
	Policy back;
	forgetApiTokens();

	const std::string scoped = scopedToken("media", AuthLevel::System);

	std::string scope;
	REQUIRE(granted(withBearer("8.8.8.8", scoped), true, &scope) == AuthLevel::System);
	REQUIRE(scope == "media");

	scope = "left over";
	REQUIRE(granted(withBearer("8.8.8.8", scoped), false, &scope) == AuthLevel::Public);
	REQUIRE(scope.empty());

	/* A token with no scope is what a configuration names, and it is unchanged: its
	   level wherever its level is enough, and no scope handed to the handler. */
	const std::string plain = randomToken();
	addApiToken(tokenLookupPrefix(plain), hashSecret(plain), AuthLevel::System);
	REQUIRE(granted(withBearer("8.8.8.8", plain), false, &scope) == AuthLevel::System);
	REQUIRE(scope.empty());
	REQUIRE(granted(withBearer("8.8.8.8", plain), true, &scope) == AuthLevel::System);
	REQUIRE(scope.empty());

	/* And the standing credential cannot be presented in an address, on the one route
	   that takes one or on any other. A caller that can set a header has one; a caller
	   that cannot gets a scope. */
	REQUIRE(granted(withQueryToken("8.8.8.8", plain), true, &scope) == AuthLevel::Public);
	REQUIRE(scope.empty());

	forgetApiTokens();
}

TEST_CASE("a scoped token is never the second token", "[auth]")
{
	/* The exemption for a bearer header rests on another site being unable to set one.
	   A scoped token is the one credential this server hands out to be carried in an
	   address, so it is the one that ends up somewhere it can be read, and a credential
	   that can be read is not a credential that says who asked. */
	Policy back;
	forgetApiTokens();

	const std::string session = openSession("root");
	REQUIRE_FALSE(session.empty());

	const std::string scoped = scopedToken("media", AuthLevel::System);
	const std::string plain = randomToken();
	addApiToken(tokenLookupPrefix(plain), hashSecret(plain), AuthLevel::System);

	Credentials c = withCookie("127.0.0.1", session);
	Response refusal;

	// The unscoped one exempts a write, which is what it has always done.
	c.bearer_token = plain;
	REQUIRE(allowed(AuthLevel::System, AuthLevel::System, Post, c, &refusal));

	// The scoped one does not, and the request is refused for want of the header.
	c.bearer_token = scoped;
	REQUIRE_FALSE(allowed(AuthLevel::System, AuthLevel::System, Post, c, &refusal));
	REQUIRE(refusal.code == 403);

	(void) closeSession(session);
	forgetApiTokens();
}

TEST_CASE("a token that has run out resolves to nothing and is given back", "[auth]")
{
	/* A token minted for a page is short lived on purpose, which is worth nothing
	   unless something refuses it afterwards. Refused where it is looked up rather
	   than when the walk comes round, so its life is the length it was given and not
	   that plus however long until the next walk. */
	Policy back;
	forgetApiTokens();

	const std::string token = randomToken();
	REQUIRE_FALSE(token.empty());
	const time_t now = time(NULL);
	addApiToken(tokenLookupPrefix(token), hashSecret(token), AuthLevel::System,
	            "media", now - 1);

	REQUIRE(granted(withQueryToken("8.8.8.8", token), true) == AuthLevel::Public);
	REQUIRE(granted(withBearer("8.8.8.8", token), true) == AuthLevel::Public);

	forgetApiTokens();
}

TEST_CASE("the route that mints one draws a token that works and says nothing without a scope", "[auth]")
{
	/* The success, last, because it is the half that a mistake anywhere else still
	   passes. A scope is the whole of what makes this token narrower than the box, so
	   asking for one without a scope answers no token rather than a token worth
	   everything. */
	Policy back;
	forgetApiTokens();

	REQUIRE(openScopedToken("", AuthLevel::System).empty());

	const std::string token = openScopedToken("media", AuthLevel::System);
	REQUIRE_FALSE(token.empty());

	std::string scope;
	REQUIRE(granted(withQueryToken("8.8.8.8", token), true, &scope) == AuthLevel::System);
	REQUIRE(scope == "media");
	REQUIRE(granted(withQueryToken("8.8.8.8", token), false, &scope) == AuthLevel::Public);

	forgetApiTokens();
}
