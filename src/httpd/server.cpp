/*
 * server.cpp - the HTTP server and its lifetime
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

#include "server.h"

/* Whether this build carries the copied control API at all, and so whether
   /control/ is mounted below (configure.ac, src/httpd/Makefile.am). */
#include <config.h>

#include "apppaths.h"
#include "auth.h"
#include "byterange.h"
#ifndef DISABLE_LEGACY_API
#include "compat/mount.h"
#endif
#include "webconfig.h"
#include "endpoint.h"
#include "events.h"
#include "livestream.h"
#include "http.h"
#include "doc/openapi.h"
#include "router.h"
#include "static.h"
#include "status.h"
#include "webtv.h"

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

#include <stdint.h>
#include <strings.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <microhttpd.h>

namespace httpd
{

namespace
{

struct MHD_Daemon *daemon_ = NULL;
int bound_port_ = 0;

/* The ceiling a body is held to, copied out of the configuration the daemon was
   started with. A number and not the configuration itself, so nothing here needs
   a constructor run before main. Written under the lock below before the daemon
   exists and read by the threads it starts, which never overlap. */
size_t max_body_ = 1u << 20;

// How many requests the server is holding state for, so a request whose state
// was never given back is visible as a number.
size_t open_requests_ = 0;

// Every byte of a body this server has taken into memory since it started.
size_t body_bytes_kept_ = 0;

// Held by everything that reads or writes the daemon and the port. Without it
// two shutdown paths can both see a daemon and both destroy it. Built on first
// use, so nothing here needs a constructor run before main.
OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

// A second lock and not the one above: the count changes twice per request and
// on threads the daemon owns, and taking the lock that start and stop hold would
// put every request behind the one that tears the daemon down.
OpenThreads::Mutex &countLock()
{
	static OpenThreads::Mutex m;
	return m;
}

/* A third, over the bound port alone, and it is not a nicety.

   A handler runs on a thread the daemon owns, and tearing the daemon down waits
   for every one of those threads. So a handler taking the lock above while a
   shutdown held it would wait for a shutdown waiting for the handler, and the
   box answers nothing again until it is restarted. There is such a handler
   (ep_webserver.cpp asks which port this server is really on), and there is a
   shutdown that runs while requests are in flight.

   Taken for the three lines that read or write the number and never held across
   anything. Always innermost where both are held, which is start below. */
OpenThreads::Mutex &portLock()
{
	static OpenThreads::Mutex m;
	return m;
}

void countOpened()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(countLock());
	++open_requests_;
}

void countBodyKept(size_t n)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(countLock());
	body_bytes_kept_ += n;
}

void countClosed()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(countLock());
	if (open_requests_ > 0)
		--open_requests_;
}

/* What one request needs while it is being read, which is more than one call of
   the access handler can hold: the library calls that handler again for every
   piece of a body, and a handler run on the first piece would answer out of a
   body that stops early.

   The target is kept as it arrived and never as the library hands it back: the
   library decodes what it parses and the router splits a path before decoding.
   Decoded first, an encoded separator is a separator, and a request could reach
   a route by spelling its way past the segment that was going to be checked. */
struct Pending
{
	std::string target;      // the request target, escapes and query and all
	std::string body;
	size_t      ceiling;
	size_t      seen;        // how much body has arrived, kept or not
	bool        started;     // the first call of the handler has been seen
	bool        too_large;

	/* Whether the body is worth keeping, decided before any of it has arrived. A
	   body is kept for a request a route is going to answer for this caller and
	   for nothing else: an unrouted path, a method the path does not take, and a
	   caller the route will refuse are all answered without looking at a body.
	   Without this an anonymous caller pays for nothing and this box pays a
	   megabyte a connection for it. */
	bool        keep_body;

	// What the gate answered before the body, for the caller it is not going to
	// let through. Sent once the request is whole, so an answer is not written
	// into a connection the caller is still writing to.
	bool        refused;
	Response    refusal;

	/* Who is asking, resolved once off the head.

	   The library calls the handler at least twice per request, and this used to
	   be worked out on each call: for a caller holding a bearer token that is two
	   derivations of ten thousand rounds each. Measured at 2 721 us a request
	   against 52 us for a caller with no token.

	   Filled on the call before the body and read at the answer. A request whose
	   declared body is over the ceiling never gets here. */
	Credentials cred;
	AuthLevel   have;

	/* Whether the route this request would reach takes a token with a scope, and what
	   the token that answered stands for.

	   Both settled with the level above, off the head, because both are read again at
	   the answer: the route is matched a second time there, and asking the table twice
	   for one request is two walks over every table this server has. */
	bool        query_token_ok;
	std::string scope;

	/* Whether the cookie named a live session when the head was read, the one
	   part of the answer above that can stop being true while a body is still
	   arriving. Asked again at the answer, and only when this says there is
	   something to lose. */
	bool        session_live;

	/* The configuration this server is to be put on once this request is
	   finished with.

	   Kept per request and not in one place for the server, because the
	   notification below is made for every request and a mark left where any of
	   them could pick it up would be a restart begun by whichever unrelated
	   request finished first. The answer saying where the box moved to has to
	   reach the caller that asked before the box moves. */
	std::string reload_after;

	Pending(const char *uri, size_t max)
		: target((uri != NULL) ? uri : ""), ceiling(max), seen(0), started(false),
		  too_large(false), keep_body(false), refused(false), have(AuthLevel::Public),
		  query_token_ok(false), session_live(false)
	{
	}
};

/* Reads the file again and puts the running server on it, off a detached thread.

   A thread, because putting the server on a new configuration stops the daemon
   and the library joins its own threads while it does: begun on one of them,
   which is where the notification below runs, it would be a thread waiting for
   itself. Detached, because there is nobody to wait for it.

   The path is this thread's own copy and is given back here, so the state of the
   request it came from is free to go the moment the notification returns.

   Nothing may leave here by being thrown: the caller is the C library's thread
   start, and a throw out of one of those ends the process. */
void *reloadThread(void *cls)
{
	std::string *path = (std::string *) cls;
	try
	{
		reloadAndRestart(*path);
	}
	catch (...)
	{
	}
	delete path;
	return NULL;
}

/* Starts the above, and says nothing when it cannot be started: the answer has
   already gone out and said what the file now holds. A box whose thread could
   not be started runs on what it ran on before while carrying a file that says
   something else, which its next start puts right. */
void startReload(const std::string &path)
{
	std::string *held = NULL;
	try
	{
		held = new std::string(path);
	}
	catch (...)
	{
		return;
	}

	pthread_attr_t attr;
	if (pthread_attr_init(&attr) != 0)
	{
		delete held;
		return;
	}
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_t id;
	if (pthread_create(&id, &attr, &reloadThread, held) != 0)
		delete held;
	pthread_attr_destroy(&attr);
}

/* Called once per request with the target as it arrived, before the library
   parses it, and what it answers becomes the state the access handler is handed.
   Made here because this is the only place the undecoded target exists.

   Nothing may leave here by being thrown: the caller is C. */
void *uriSeen(void *cls, const char *uri, struct MHD_Connection *connection)
{
	(void) connection;

	const size_t max = (cls != NULL) ? *(const size_t *) cls : (size_t) (1u << 20);
	Pending *p = NULL;
	try
	{
		p = new Pending(uri, max);
	}
	catch (...)
	{
		// Answered by the handler, which finds no state and says so. Counted
		// nowhere, because nothing was made.
		return NULL;
	}
	countOpened();
	return p;
}

/* The library is done with a request, whether it answered one or the caller went
   away mid send. Freed here and never on the way out of the handler, because a
   connection abandoned mid body never reaches the handler a last time. */
void requestDone(void *cls, struct MHD_Connection *connection,
                 void **con_cls, enum MHD_RequestTerminationCode toe)
{
	(void) cls;
	(void) connection;
	(void) toe;

	if (con_cls == NULL || *con_cls == NULL)
		return;

	/* Cleared before anything is given back, so the one path out of here that
	   could leave the state half freed cannot leave the library holding a
	   pointer to it either. */
	Pending *p = (Pending *) *con_cls;
	*con_cls = NULL;

	/* Nothing below throws as it stands. The boundary is here for what a later
	   edit does: a throw leaving a callback C called unwinds into a frame built
	   without exceptions, and on the target that ends the process rather than
	   the request; the host this is developed on does not show it. */
	try
	{
	/* Here and nowhere earlier. This is the library saying it is done with the
	   request, which for an answer it sent is after the bytes have gone, and
	   that is the whole of what the field is for.

	   Whatever the request came to. A caller that went away mid read still asked
	   for the change, and the file it asked for is already written. */
		const std::string reload = p->reload_after;

		delete p;
		countClosed();

		if (!reload.empty())
			startReload(reload);
	}
	catch (...)
	{
	}
}

// The textual address the request came from, for a handler that answers
// differently to the box itself. Never parsed here.
std::string peerOf(struct MHD_Connection *connection)
{
	const union MHD_ConnectionInfo *i =
		MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
	if (i == NULL || i->client_addr == NULL)
		return std::string();

	char text[INET6_ADDRSTRLEN];
	text[0] = '\0';
	if (i->client_addr->sa_family == AF_INET)
	{
		const struct sockaddr_in *a = (const struct sockaddr_in *) i->client_addr;
		if (inet_ntop(AF_INET, &a->sin_addr, text, sizeof(text)) == NULL)
			return std::string();
	}
	else if (i->client_addr->sa_family == AF_INET6)
	{
		const struct sockaddr_in6 *a = (const struct sockaddr_in6 *) i->client_addr;
		if (inet_ntop(AF_INET6, &a->sin6_addr, text, sizeof(text)) == NULL)
			return std::string();
	}
	else
	{
		return std::string();
	}
	return std::string(text);
}

/* The length the request says its body will be, or none when it says nothing or
   says something that is not a number. Read so a body known to be over the
   ceiling is refused before a byte of it is kept. */
bool declaredLength(struct MHD_Connection *connection, unsigned long long &out)
{
	const char *v = MHD_lookup_connection_value(connection, MHD_HEADER_KIND,
	                                            MHD_HTTP_HEADER_CONTENT_LENGTH);
	if (v == NULL || v[0] == '\0')
		return false;

	char *end = NULL;
	const unsigned long long n = std::strtoull(v, &end, 10);
	if (end == v || (end != NULL && *end != '\0'))
		return false;
	out = n;
	return true;
}

/* The last answer this server has, and the only one it can still send when
   building an answer is what went wrong. Nothing here allocates through the
   language: the document is a literal the transport is told not to copy, and the
   two calls that reach for memory are the library's own and say so by answering
   nothing. Nothing here can throw: it is called from inside a handler for a
   throw, where a second one would leave the callback. */
/* Whether the caller has said it will wait to be told before sending a body.
   Case insensitively, the value being a token. */
bool asksBeforeSending(struct MHD_Connection *connection)
{
	const char *v = MHD_lookup_connection_value(connection, MHD_HEADER_KIND,
	                                            MHD_HTTP_HEADER_EXPECT);
	return v != NULL && strcasecmp(v, "100-continue") == 0;
}

MHD_Result queueFixedProblem(struct MHD_Connection *connection)
{
	const char *doc = fixedProblem();
	struct MHD_Response *response =
		MHD_create_response_from_buffer(std::strlen(doc), (void *) doc, MHD_RESPMEM_PERSISTENT);
	if (response == NULL)
		return MHD_NO;

	if (MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE,
	                            problemContentType()) != MHD_YES)
	{
		MHD_destroy_response(response);
		return MHD_NO;
	}

	const MHD_Result queued =
		MHD_queue_response(connection, StatusInternalServerError, response);
	MHD_destroy_response(response);
	return queued;
}

/* The descriptor an answer carries, held for as long as this side of the hand
   over owns it and given back by leaving the scope.

   Every way out of the function below other than the one hand over has to be
   covered, the ones an allocation inside the library fails on included, and
   those are the ways out nobody writes a test through by accident. A file left
   open on one of them is held for the life of the program. */
struct HandedDescriptor
{
	int fd;

	explicit HandedDescriptor(int f) : fd(f) {}
	~HandedDescriptor()
	{
		if (fd >= 0)
			::close(fd);
	}

	// Says the descriptor is somebody else's from here on. Answers it so the
	// hand over and the giving up of ownership are one statement.
	int release()
	{
		const int taken = fd;
		fd = -1;
		return taken;
	}

private:
	HandedDescriptor(const HandedDescriptor &);
	HandedDescriptor &operator=(const HandedDescriptor &);
};

/* What a Content-Range says, in the two spellings it has: the stretch that was
   sent, and, for the refusal, the length of the file the caller named nothing
   inside of. Written here so the two sites cannot disagree about the syntax.

   Cast to the widest the format has rather than printed as what it is: the type
   is sixty four bits wide and long is four bytes where this runs, so a length
   printed under a format for a long would be read out of the wrong half of the
   argument. */
std::string sentRange(uint64_t first, uint64_t last, uint64_t length)
{
	char buf[96];
	std::snprintf(buf, sizeof(buf), "bytes %llu-%llu/%llu",
	              (unsigned long long) first, (unsigned long long) last,
	              (unsigned long long) length);
	return std::string(buf);
}

std::string noSuchRange(uint64_t length)
{
	char buf[64];
	std::snprintf(buf, sizeof(buf), "bytes */%llu", (unsigned long long) length);
	return std::string(buf);
}

MHD_Result queueResponse(struct MHD_Connection *connection, const Response &r)
{
	/* Taken over the moment this is entered, and not only once the branch that
	   sends it is reached: the answer below may become a different one, and the
	   descriptor the caller wrote would then be a file nothing sends and nothing
	   closes. */
	HandedDescriptor held(r.fd);

	/* A response carries no code until something writes one, and a handler that
	   answered without writing one would put a number on the wire that is not a
	   status. */
	Response fixed;
	const Response *out = &r;
	if (r.code < 100 || r.code > 599)
	{
		fixed = problemResponse(StatusInternalServerError, coreapi::ErrorCode::BadTable,
		                        "this route answered with no status");
		out = &fixed;
	}

	/* ONE STRETCH OF A FILE WHERE THE CALLER ASKED FOR ONE, AND HERE RATHER THAN
	   AT A HANDLER.

	   Every answer this server builds out of a descriptor is the bytes of one
	   regular file, whose length was taken off that same descriptor a floor
	   below, and that is everything a range needs. A handler would have to work
	   the arithmetic out again per route, and the routes that did not would be
	   the ones a caller cannot resume a download from.

	   Only on an answer that succeeded whole: a refusal and a redirection each
	   carry a document about themselves rather than a file.

	   The descriptor is left where it is on the refusal below: the answer that
	   replaces it carries a body of its own, so the holder above closes it. */
	int code = out->code;
	uint64_t offset = 0;
	uint64_t length = out->length;
	std::string content_range;
	Response no_such_range;
	bool takes_ranges = out->fd >= 0 && out->code == StatusOk;
	if (takes_ranges)
	{
		const char *want = MHD_lookup_connection_value(connection, MHD_HEADER_KIND,
		                                               MHD_HTTP_HEADER_RANGE);
		uint64_t first = 0;
		uint64_t last = 0;
		const RangeAnswer asked = byteRangeOf((want != NULL) ? want : "", out->length, first, last);
		if (asked == RangeAnswer::Partial)
		{
			offset = first;
			length = last - first + 1;
			code = StatusPartialContent;
			content_range = sentRange(first, last, out->length);
		}
		else if (asked == RangeAnswer::Unsatisfiable)
		{
			no_such_range = problemResponse(StatusRangeNotSatisfiable,
			                                coreapi::ErrorCode::RangeOutsideFile,
			                                "the stretch that was asked for is not in this file");
			addApiHeaders(no_such_range);
			/* The length is in the refusal and the bytes are not, which lets a
			   caller that asked past the end work out what to ask for next
			   without fetching anything. */
			no_such_range.headers.push_back(
				std::make_pair(std::string(MHD_HTTP_HEADER_CONTENT_RANGE), noSuchRange(out->length)));
			out = &no_such_range;
			code = out->code;
			takes_ranges = false;
		}
	}

	/* Out of the file itself where the answer names one, so its bytes go from the
	   kernel to the socket without this process reading them into a string first,
	   and with a length the caller has already taken off that same descriptor: a
	   stated length is what keeps the answer off chunked transfer, and an answer
	   sent in chunks is one a client that does not decode them cannot read.

	   The library takes the descriptor with the response and gives it back when
	   the response is destroyed. Answering nothing is the one way out of the call
	   that leaves it here, so the hand over is written on the answer and not
	   before it. */
	struct MHD_Response *response = NULL;
	if (out->fd >= 0)
	{
		response = MHD_create_response_from_fd_at_offset64(length, out->fd, offset);
		if (response != NULL)
			held.release();
	}
	else
	{
		response = MHD_create_response_from_buffer(out->body.size(), (void *) out->body.data(),
		                                           MHD_RESPMEM_MUST_COPY);
	}
	if (response == NULL)
		return MHD_NO;

	/* A length written beside an answer is sent instead of the one the library
	   would count, and only on the answer that carries no body by definition.

	   A 304 may state a length only when it is the length the 200 would have
	   carried, and what the library counts is the length of what it was handed,
	   which for that answer is nothing. So the handler states it and the library
	   is told to stand aside. On every other answer a stated length could only
	   disagree with the counted one and is dropped rather than sent: an answer
	   whose stated length and body disagree is a connection the next request on
	   it cannot be found in. */
	const bool states_length = out->code == StatusNotModified;
	if (states_length)
		MHD_set_response_options(response, MHD_RF_INSANITY_HEADER_CONTENT_LENGTH, MHD_RO_END);

	/* A header the library will not take leaves the answer one header short and
	   says nothing about it, and the one that goes missing is as likely to be the
	   media type as anything else. Answered as a fault here instead. */
	bool added = true;
	if (!out->content_type.empty())
		added = MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE,
		                                out->content_type.c_str()) == MHD_YES;
	/* Said on every file backed answer and not only where a range was asked for,
	   because it states a property of the answer rather than of the request: a
	   client that has to find out by asking and being refused pays a whole
	   request to learn it. */
	if (added && takes_ranges)
		added = MHD_add_response_header(response, MHD_HTTP_HEADER_ACCEPT_RANGES, "bytes") == MHD_YES;
	if (added && !content_range.empty())
		added = MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_RANGE,
		                                content_range.c_str()) == MHD_YES;
	for (size_t i = 0; added && i < out->headers.size(); ++i)
	{
		if (!states_length &&
		    strcasecmp(out->headers[i].first.c_str(), MHD_HTTP_HEADER_CONTENT_LENGTH) == 0)
			continue;
		added = MHD_add_response_header(response, out->headers[i].first.c_str(),
		                                out->headers[i].second.c_str()) == MHD_YES;
	}
	if (!added)
	{
		MHD_destroy_response(response);
		return queueFixedProblem(connection);
	}

	const MHD_Result queued = MHD_queue_response(connection, (unsigned int) code, response);
	MHD_destroy_response(response);
	return queued;
}

/* The way out of a handler that threw. The document every other refusal is
   written through is tried first, because it says which fault this was, and the
   one that is already written answers when writing that one throws as well.

   The one written already carries no headers of its own and is not given any: it
   is the answer for the moment an allocation has just failed, and every one of
   those headers is an allocation.

   The inner try is what makes this safe to call from inside a catch clause: a
   throw there is not caught by the sibling clause of the same try, so it would
   leave the callback, and the callback is reached from C. */
MHD_Result refuseAfterAThrow(struct MHD_Connection *connection)
{
	try
	{
		Response r = problemResponse(StatusInternalServerError,
		                             coreapi::ErrorCode::BoxUnreadable,
		                             "this server failed to answer the request");
		addApiHeaders(r);
		return queueResponse(connection, r);
	}
	catch (...)
	{
		return queueFixedProblem(connection);
	}
}

// The one refusal the transport makes about a body, written once because it is
// answered from two places.
Response tooLarge()
{
	Response r = problemResponse(StatusPayloadTooLarge,
	                             coreapi::ErrorCode::BodyTooLarge,
	                             "the body is longer than this server accepts");
	addApiHeaders(r);
	return r;
}

/* Every value a request carried under one header name, in the order they arrived
   and joined with commas.

   The library's lookup answers the first of several values under one name, and
   the first forwarded-for of several is the one written furthest from this
   server, which is to say the one the client wrote. Reading it that way would be
   reading exactly the value nothing here may believe, so every occurrence is
   collected and the gate reads the end of the list. */
struct Joined
{
	const char *name;
	std::string out;
};

/* Nothing may leave here by being thrown: the caller is C, and both appends
   below allocate.

   Where this runs the throw has to unwind through a frame of the library, and
   the compiler the boxes are built with emits no unwind information for C at the
   flags the build uses, so unwinding into it ends the process rather than the
   request. On the machine the suite runs on the library carries the information
   and the throw is caught by the boundary in the access handler, so this is a
   boundary the suite cannot show the need for.

   Answering MHD_NO stops the walk and leaves the string holding what was
   collected before the failure, which is a header read short. That is the same
   answer this gives for a header the request did not carry, and every reader of
   these values already treats a short read as the narrow case. */
MHD_Result collectHeader(void *cls, enum MHD_ValueKind kind,
                         const char *key, const char *value)
{
	(void) kind;
	Joined *j = (Joined *) cls;
	if (j == NULL || key == NULL || value == NULL)
		return MHD_YES;
	if (strcasecmp(key, j->name) != 0)
		return MHD_YES;

	try
	{
		if (!j->out.empty())
			j->out += ",";
		j->out += value;
	}
	catch (...)
	{
		return MHD_NO;
	}
	return MHD_YES;
}

/* Every value under one header name as one string, which is what the collector
   above leaves behind. A header a request carried twice is one header carrying
   both values, for every list valued name, so reading only the first would be
   reading half of what the caller said. */
std::string joinedHeader(struct MHD_Connection *connection, const char *name)
{
	Joined j;
	j.name = name;
	MHD_get_connection_values(connection, MHD_HEADER_KIND, &collectHeader, &j);
	return j.out;
}

/* What the request carried that could say who is asking, read off the connection
   and handed to the gate as text. Nothing here decides anything: the header
   names live in one place and this is the one call that turns a connection into
   the values under them. */
Credentials credentialsOf(struct MHD_Connection *connection)
{
	Credentials c;
	c.peer = peerOf(connection);

	const char *cookie = MHD_lookup_connection_value(connection, MHD_COOKIE_KIND,
	                                                 sessionCookieName());
	if (cookie != NULL)
		c.cookie_token = cookie;

	const char *authz = MHD_lookup_connection_value(connection, MHD_HEADER_KIND,
	                                                MHD_HTTP_HEADER_AUTHORIZATION);
	if (authz != NULL)
	{
		c.bearer_token = bearerToken(authz);

		/* Read whether or not the header turns out to carry the Bearer scheme
		   instead: granted() never looks at this member, and the legacy
		   surface under /control/ is the only reader
		   (httpd::compat::legacyAllows). */
		c.basic = basicCredential(authz);
	}

	const char *csrf = MHD_lookup_connection_value(connection, MHD_HEADER_KIND,
	                                               csrfHeaderName());
	if (csrf != NULL)
		c.csrf_header = csrf;

	/* The one credential a program that cannot set a header can present. Read whatever
	   the route turns out to be, the way the Basic header beside it is: what a value
	   here is worth is the gate's to decide, and it is worth nothing at all unless the
	   route says it takes one.

	   Only a token, and never a session cookie: a cookie in an address outlives the
	   request in a history list, in a proxy's log and in whatever the address was
	   pasted into, which is the whole of what a cookie is for avoiding. */
	const char *from_query = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND,
	                                                     queryTokenName());
	if (from_query != NULL)
		c.query_token = from_query;

	c.forwarded_for = joinedHeader(connection, "X-Forwarded-For");

	return c;
}

MHD_Result serve(struct MHD_Connection *connection, const char *method,
                 const char *upload_data, size_t *upload_data_size, void **con_cls)
{
	Pending *st = (Pending *) *con_cls;
	if (st == NULL)
	{
		/* Nothing was made for this request, which means the box had no memory
		   for a string and a struct. The answer is the one that asks for none. */
		return queueFixedProblem(connection);
	}

	/* Never an answer on the first call. The library calls the handler once
	   before it reads a body, so an answer here would be an answer to a request
	   whose body has not arrived. */
	if (!st->started)
	{
		st->started = true;

		unsigned long long declared = 0;
		if (declaredLength(connection, declared) && declared > (unsigned long long) st->ceiling)
		{
			st->too_large = true;
			/* A caller that asked whether to send is told the answer instead of
			   being invited to send what is already refused. One that did not
			   ask is writing already, and an answer sent into a connection
			   closed under it can be lost with the reset that follows, so that
			   one is read to the end and answered after. */
			if (asksBeforeSending(connection))
				return queueResponse(connection, tooLarge());
		}

		/* THE GATE, ASKED BEFORE THE BODY IS ACCEPTED.

		   The head is whole here and the body has not started, so this is the
		   last moment at which refusing costs the caller's memory rather than
		   this box's. Measured before this was here: sixty connections to a
		   route that answered every one of them 403 held seventy megabytes of a
		   box that has a hundred and twenty eight, and the timeout that would
		   have ended them is an idle one, which a caller writing a byte a minute
		   never reaches.

		   The level comes off the route the request would reach, so the answer
		   is word for word the answer that route would have given. Asked only
		   where a route matched: an unrouted path and a method a path does not
		   take are answered without a body being read either.

		   After the ceiling and never before it, so a body too long is still too
		   long whoever sent it. */
		if (!st->too_large)
		{
			const size_t q = st->target.find('?');
			const Method m = methodFromString((method != NULL) ? method : "");

			const std::string gate_path = st->target.substr(0, q);

			/* THE ROUTE BEFORE THE CREDENTIAL, because one of the things the route
			   declares is which credentials count. A token with a scope is worth its
			   level only where the route reads one, and only there may one arrive in
			   the query at all, so what the request carries cannot be judged before
			   it is known what it would reach. False for a path no route answers and
			   for the legacy prefix below, neither of which was written to hold a
			   caller to a scope. */
			AuthLevel need = AuthLevel::Public;
			const bool routed = routeLevelFor(m, gate_path, &need, &st->query_token_ok);

			st->cred = credentialsOf(connection);
			st->have = granted(st->cred, st->query_token_ok, &st->scope);
			st->session_live = !st->cred.cookie_token.empty() &&
			                   sessionIsLive(st->cred.cookie_token);

			/* ONE OF THE TWO PLACES THIS SERVER ASKS THE GATE. The other is at
			   the answer below, where the request is whole, and it asks the
			   method half of the same question again because a session can have
			   gone in between. Loosening either one alone changes nothing a
			   caller can see: whichever is still asking refuses the request.
			   Anybody narrowing what the gate covers has both to change. */
			if (routed)
			{
				Response refusal;
				if (!allowed(need, st->have, m, st->cred, &refusal))
				{
					// The same answer the route would have given, headers
					// included, so a caller cannot tell which of the two
					// refused it.
					addApiHeaders(refusal);
					st->refused = true;
					st->refusal = refusal;
				}
				else
				{
					/* Kept for every route that will answer this caller and
					   not only for one whose table declares a member carried
					   in a body, because a handler may read the whole of one
					   without the table saying so and one of them does. */
					st->keep_body = true;
				}
			}
#ifndef DISABLE_LEGACY_API
			/* The same decision for the one prefix that has no route table. Its
			   handlers read a posted form out of the very list a query goes into
			   (compat/mount.h), so a body dropped here is a form that reaches
			   its handler as no parameters at all: answered 200, and for
			   xmltvlist by emptying three of the box's lists on the way.

			   Asked here rather than kept and sorted out later, for the reason
			   the branch above keeps bodies at all. No answer is written here,
			   only the decision not to pay for one; what the refusal says stays
			   compat::answer()'s to say.

			   POST and no other method, because compat::answer() reads a body
			   for no other one (compat/mount.cpp) and the old server read one
			   for no other one either. Measured before this said so: all six
			   methods held a 4096 byte body to the end of the request, four of
			   them for nothing at all. */
			else if (config().legacy_enabled && compat::handles(gate_path))
			{
				const std::string gate_query = (q == std::string::npos)
					? std::string() : st->target.substr(q + 1);
				if (m == Post && compat::wouldDispatch(gate_path, gate_query, st->cred, st->have))
					st->keep_body = true;
			}
#endif

			// The same reasoning as the ceiling above, and the same two ways
			// out of it.
			if (st->refused && asksBeforeSending(connection))
				return queueResponse(connection, st->refusal);
		}
		return MHD_YES;
	}

	if (*upload_data_size > 0)
	{
		if (!st->too_large)
		{
			/* Counted whether or not it is kept, so a body that arrives without
			   saying how long it would be is held to the same ceiling as one
			   that said.

			   Subtracted rather than added, because the two are the width of an
			   address and their sum is not: four bytes where this runs, so a
			   ceiling near the top of that range would wrap and read as room.
			   Nothing is ever counted past the ceiling. */
			if (*upload_data_size > st->ceiling - st->seen)
			{
				st->too_large = true;
				// What was kept is given back rather than carried to an answer
				// that is not going to read it.
				std::string().swap(st->body);
			}
			else
			{
				st->seen += *upload_data_size;
				if (st->keep_body)
				{
					st->body.append(upload_data, *upload_data_size);
					countBodyKept(*upload_data_size);
				}
			}
		}
		// Read and dropped rather than left: the rest of a body nobody is going
		// to look at still has to come off the connection for the answer to
		// reach the caller that is still sending it.
		*upload_data_size = 0;
		return MHD_YES;
	}

	if (st->too_large)
		return queueResponse(connection, tooLarge());

	// The answer the gate arrived at before the body, sent now that there is
	// nothing left coming in to be reset under it.
	if (st->refused)
		return queueResponse(connection, st->refusal);

	// The query is split off here and decoded by nothing on the way: what
	// separates the two is the first question mark, and everything after it is
	// the router's to take apart.
	const size_t q = st->target.find('?');
	const std::string path = st->target.substr(0, q);
	const std::string query = (q == std::string::npos) ? std::string() : st->target.substr(q + 1);

	const Method m = methodFromString((method != NULL) ? method : "");

	/* Resolved once, off the head, on the call that ran before the body. What is
	   done here is the one part of that answer which can have moved since: a
	   session runs out on the clock and is closed by a logout on another
	   connection, and a body can take longer to arrive than either. Asking that
	   is a lookup; asking the whole question again would be a derivation for
	   every caller holding a token, twice per request, for an answer that cannot
	   change. */
	if (st->session_live && !sessionIsLive(st->cred.cookie_token))
	{
		st->have = granted(st->cred, st->query_token_ok, &st->scope);
		st->session_live = false;
	}

	const Credentials &cred = st->cred;
	const AuthLevel have = st->have;

	/* The half of the gate the transport can settle. What a request needs is not
	   known until a route is matched, so this asks with the least any route can
	   need, leaving the check that depends on the method: a request that changes
	   something and arrived on a cookie alone has to carry the second token.

	   Refusing here rather than demoting is deliberate. A caller reaching this
	   holds a live session and the request is one another site made in that
	   caller's name, so letting it through at a lower level would be letting it
	   through.

	   THE SECOND OF THE TWO PLACES THIS SERVER ASKS THE GATE. The first is ahead
	   of the body and asks the same method half. An exception written at one of
	   the two does nothing: the other still refuses. */
	Response refusal;
	if (!allowed(AuthLevel::Public, have, m, cred, &refusal))
	{
		addApiHeaders(refusal);
		return queueResponse(connection, refusal);
	}

	/* The address off the socket is what the handler is given as the peer, and
	   the one the gate resolved travels beside it. A caller behind a proxy writes
	   its own forwarded header, and the last element of that header is only as
	   good as the proxy that appended to it, so a handler comparing an address
	   has to be comparing the one the transport observed. */
	/* The token off the request travels with it, for the one route whose act is
	   on the session itself. Read here and nowhere else, so the session a handler
	   can close is the session the transport observed and never one a caller
	   named. */
	/* The authority the request named, which both surfaces below are given and
	   neither reads to decide anything: it is how this caller reached the box, so
	   it is the only name right for an address handed back to it.

	   Read with the library's own lookup and not through joinedHeader: that one
	   joins every value under a name with commas, which is wrong here, where a
	   second value is a malformed request and not a longer answer. A request
	   without the header leaves this empty, and nothing here puts a name of the
	   box in its place: one made up on this side reads to a caller exactly like
	   the one it gave. */
	const char *named_host = MHD_lookup_connection_value(connection, MHD_HEADER_KIND,
	                                                     MHD_HTTP_HEADER_HOST);
	const std::string authority = (named_host != NULL) ? named_host : "";

	/* The legacy surface, checked ahead of the router's own tables rather than
	   through one of them: those tables refuse a parameter they did not declare,
	   and a legacy endpoint carries arbitrary and positional names. A path under
	   this prefix used to fall through to the file server below and end in a 404;
	   after this it never reaches that fallback.

	   Answered without addApiHeaders, unlike every path below this one. Those
	   headers describe the router's own auth model: a cache told not to store
	   because a cookie or token could change what the same path answers, and a
	   Vary naming the two credentials that model reads. A legacy endpoint never
	   joined that model; CControlAPI reads neither the level nor the credential,
	   and answer() reads both ahead of it to decide whether CControlAPI is
	   reached at all (httpd::compat::legacyAllows). Adding the headers here would
	   describe the router's response on a path the router never sees. That cuts
	   both ways: answer() itself (mount.cpp) still writes Cache-Control: no-cache
	   on its own, because the old server writes exactly that on every one of
	   these answers.

	   Two switches, at two different times. DISABLE_LEGACY_API is a build asked
	   to leave compat/ out of the program altogether (src/httpd/Makefile.am):
	   with it set there is no answer() here to call. legacy_enabled is the same
	   surface still linked in, read from a box that carries it: on by default,
	   because turning it off by default would turn off the plugins, the second
	   box syncing timers over /control/ and every other caller this migration
	   has not counted out. A box that wants the surface gone rather than quiet
	   wants the build switch. Off either way falls /control/ through to the
	   router's own tables below. */
#ifndef DISABLE_LEGACY_API
	if (config().legacy_enabled && compat::handles(path))
	{
		/* What the body says it is, which is the whole of what decides whether
		   there is anything in it to take apart: the old server parses one
		   content type and hands every other body to nobody (mount.cpp's own
		   bodyIsForm). st->body is empty unless the gate above decided to keep
		   it. */
		const char *ctype = MHD_lookup_connection_value(connection, MHD_HEADER_KIND,
		                                                MHD_HTTP_HEADER_CONTENT_TYPE);
		return queueResponse(connection, compat::answer(m, path, query,
		                                               st->body,
		                                               (ctype != NULL) ? ctype : "",
		                                               authority,
		                                               cred, have));
	}
#endif

	const Response r = dispatch(m, path, query, st->body, cred.peer, have,
	                            clientAddress(cred), cred.cookie_token, authority,
	                            st->scope);

	/* Carried out of the answer and into the state of this request, which is what
	   outlives the answer: the notification that says the bytes have gone is
	   handed this state and nothing else. Taken here rather than beside the line
	   that sends the answer, because there are several of those. */
	st->reload_after = r.reload_after;

	/* A route that answers with a stream rather than with a document. The handler
	   has already been through the gate and the table, so what is left is the
	   half a handler cannot do: a handler answers with bytes and returns, and a
	   stream is a connection that stays open after it has.

	   Only for the method that reads one. A HEAD is answered by the route that
	   answers the GET, and a HEAD that opened a stream would hold a connection
	   for a caller that is not going to read a body. */
	if (m == Get && events::isStream(r))
	{
		Response no_room;
		switch (events::openStream(connection, r, no_room))
		{
			case events::StreamOpened:
				// The connection is the stream's now, and nothing else here
				// may queue an answer on it.
				return MHD_YES;
			case events::StreamRefused:
				return queueResponse(connection, no_room);
			case events::StreamFailed:
				break;
		}
		return queueFixedProblem(connection);
	}

	/* The other kind, and the same bargain: a handler that answered with a
	   command rather than with bytes. Only for the method that reads one, for the
	   reason above, so a HEAD says what a GET would send without starting a
	   program nobody is going to read. */
	if (m == Get && livestream::isStream(r))
	{
		Response no_room;
		switch (livestream::open(connection, r, no_room))
		{
			case livestream::StreamOpened:
				return MHD_YES;
			case livestream::StreamRefused:
				return queueResponse(connection, no_room);
			case livestream::StreamFailed:
				break;
		}
		return queueFixedProblem(connection);
	}

	/* A route that answers with a stream this box is fetching from somewhere
	   else. The same shape as the stream above and for the same reason.

	   Only for the method that reads one: a HEAD that opened a pass through would
	   hold a connection and a socket out to a far server for a caller that is not
	   going to read a body. */
	if (m == Get && webtv::isRelay(r))
	{
		Response no_room;
		switch (webtv::open(connection, r, no_room))
		{
			case webtv::RelayOpened:
				// The connection is the pass through's now, and nothing else
				// here may queue an answer on it.
				return MHD_YES;
			case webtv::RelayRefused:
				return queueResponse(connection, no_room);
			case webtv::RelayFailed:
				break;
		}
		return queueFixedProblem(connection);
	}

	/* The one answer this server keeps a second form of.

	   Here and not at the handler because a handler is given a request read for
	   the values its own row declares and is given no headers at all, and which
	   form a caller takes is a header.

	   The answer is what is asked about and not the path it came from. A path is
	   not decoded until the router splits it, so an escaped spelling reaches the
	   same route and would read as a different path here.

	   The head of an answer is asked for as well, so what a HEAD says about the
	   length is the length of what a GET would send. */
	if ((m == Get || m == Head) && openapi::isDocument(r))
	{
		return queueResponse(
			connection,
			openapi::withEncoding(
				r, joinedHeader(connection, MHD_HTTP_HEADER_ACCEPT_ENCODING)));
	}

	/* What the router does not have is offered to the pages before the answer
	   that says nobody has it.

	   Read on the router's answer rather than on a second walk of the tables. A
	   path under the API is never offered: everything there is a route, a 404
	   from one of them was written by a handler that looked, and a directory
	   holding a file by that name would answer over it.

	   Only what a page is fetched with: a method that changes something is not a
	   method a file answers.

	   What the pages answer is what goes back, their own absence included. The
	   two answers for a path nobody has are the same document word for word, and
	   the one from here carries a directive saying how long it may be kept, which
	   a 404 needs: it is cacheable on a guess, and it is the answer for a page
	   that is not installed yet. */
	if (r.code == StatusNotFound && (m == Get || m == Head) &&
	    path.compare(0, 5, "/api/") != 0)
	{
		/* Read once for both answers below, both being a file out of a directory
		   negotiated the same way. */
		const std::string accept = joinedHeader(connection, MHD_HTTP_HEADER_ACCEPT_ENCODING);
		const std::string inm = joinedHeader(connection, MHD_HTTP_HEADER_IF_NONE_MATCH);

		/* The one name answered out of the directory a box may write into. It is
		   a list of one and it is closed, for the same reason the area list is: a
		   second root that can be configured is a way out of the first that
		   nobody sees. The pages are installed read only, so a look of a person's
		   own has nowhere else to live.

		   Compared whole and not as a prefix, because a prefix would make
		   /user.cssX a second name reaching the same directory. */
		if (path == "/user.css")
			return queueResponse(connection, serveStaticFrom(publicRoot(), path, accept, inm));

		Response served = serveStatic(path, accept, inm);

		/* What the directory does not hold is offered to the page, but only under
		   a name the page has an area for. The two 404s stay one answer
		   everywhere else, which is what static.h keeps them as. */
		if (served.code == StatusNotFound && answersWithPage(path))
			served = serveStatic("/index.html", accept, inm);
		return queueResponse(connection, served);
	}

	return queueResponse(connection, r);
}

/* The boundary. This is reached from C, the rest of the tree is built without
   exceptions, and a throw that left here would end the process rather than the
   request. Everything a request does happens under it. */
MHD_Result accessHandler(void *cls, struct MHD_Connection *connection,
                         const char *url, const char *method, const char *version,
                         const char *upload_data, size_t *upload_data_size,
                         void **con_cls)
{
	(void) cls;
	// The library hands back a target it has already decoded, and the router
	// splits a path before it decodes one, so what is answered is the target as
	// it arrived and kept beside the request.
	(void) url;
	(void) version;

	try
	{
		return serve(connection, method, upload_data, upload_data_size, con_cls);
	}
	catch (const std::exception &)
	{
		/* Both clauses are empty on purpose and nothing may be written into
		   either. A throw from inside a clause is not caught by the clause beside
		   it, and this is a callback C called, so anything done in here would be
		   done where a second throw ends the process.

		   Which of the two caught it is not read: what an exception carries was
		   written for whoever reads a log and is not sent to a caller, because it
		   says how this server is put together. */
	}
	catch (...)
	{
		// Not everything that can be thrown is an exception, and the clause
		// above answers for none of the rest.
	}

	// Reached only by way of one of the two clauses, the try above having
	// returned on every path that did not throw.
	return refuseAfterAThrow(connection);
}

} // namespace

bool answerFromDescriptor(Response &r, int fd)
{
	if (fd < 0)
		return false;

	/* Off the descriptor and never off a name. The name a file was opened under
	   is a question about the directory and can be answered by a different file a
	   moment later, and on a box that directory is writable; the descriptor stays
	   the file that was opened however the name is rearranged underneath. A
	   length taken from the name would state the size of one file and send the
	   bytes of another. */
	struct stat st;
	std::memset(&st, 0, sizeof(st));
	if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0)
	{
		/* Given back here rather than left to the caller. A call that answers
		   both a failure and a descriptor the caller may still own is one where
		   the two answers can be read apart. */
		::close(fd);
		return false;
	}

	r.fd = fd;
	r.length = (uint64_t) st.st_size;
	return true;
}

ServerConfig defaultConfig()
{
	ServerConfig c;
	/* Eighty, because that is where the server this one replaces answered, and a
	   port is the one setting of a box written down outside the box: in a
	   bookmark, in a plugin, in whatever panel somebody pointed at it. None of
	   those is asked before an upgrade, so a default anywhere else is a box that
	   comes back unreachable to all of them at once.

	   A default and not a fixture: the file names a port like any other key. */
	c.port = 80;
	c.bind_address = "0.0.0.0";
	c.thread_pool = 4;
	c.connection_limit = 64;
	c.connection_timeout_s = 30;
	c.max_body_bytes = 1u << 20;
	return c;
}

bool start(const ServerConfig &c)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());

	if (daemon_ != NULL)
		return false;

	// The field is an int and the protocol's is sixteen bits, so a number from a
	// settings file that does not fit is refused here. Casting it would bind
	// whatever the low bits happened to spell. Zero keeps its meaning of letting
	// the kernel choose.
	if (c.port < 0 || c.port > 65535)
		return false;

	/* A table this layer wrote wrong is a fault here and not at any caller, and
	   two routes that answer one request equally well answer it by whichever the
	   linker put first. Refusing to start says so once. */
	if (!routesAreSane())
		return false;

	const uint16_t port = (uint16_t) c.port;
	unsigned flags = MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_EPOLL | MHD_ALLOW_SUSPEND_RESUME;

	struct sockaddr_storage addr;
	std::memset(&addr, 0, sizeof(addr));

	/* The address is read below through a pointer to its first byte, and that
	   reading ends at the first zero byte. A value carrying one would be judged
	   on the half of itself that suits it and bound as that half, while
	   everything upstream that checks an address refuses it whole. This call is
	   the backstop for every caller, so the promise the header makes, that
	   anything which is not a literal of either family is refused, is kept
	   here. */
	if (c.bind_address.find('\0') != std::string::npos)
		return false;

	struct in_addr  v4;
	struct in6_addr v6;
	std::memset(&v4, 0, sizeof(v4));
	std::memset(&v6, 0, sizeof(v6));

	if (inet_pton(AF_INET, c.bind_address.c_str(), &v4) == 1)
	{
		struct sockaddr_in *a = (struct sockaddr_in *) &addr;
		a->sin_family = AF_INET;
		a->sin_port = htons(port);
		a->sin_addr = v4;
	}
	else if (inet_pton(AF_INET6, c.bind_address.c_str(), &v6) == 1)
	{
		struct sockaddr_in6 *a = (struct sockaddr_in6 *) &addr;
		a->sin6_family = AF_INET6;
		a->sin6_port = htons(port);
		a->sin6_addr = v6;
		// The address carries no length of its own to the daemon, which reads
		// this flag to decide how much of it to copy. Without the flag a v6
		// address is truncated to the size of a v4 one.
		flags |= MHD_USE_IPv6;
	}
	else
	{
		return false;
	}

	/* Last of the refusals and immediately before anything is armed, because this
	   is the moment answering becomes possible and it is the only one of these
	   that is not about what the caller asked for.

	   What it stands in the way of is a request reaching a read of the box before
	   the program has installed what those reads go through. Those accessors end
	   the process rather than answering, so there is no later point at which this
	   could be reported: the first request would take the program down. Not
	   merely an ordering discipline at the one caller, because this is the single
	   door every caller goes through. */
	if (!coreapi::dependenciesInstalled())
		return false;

	// Read by the threads the daemon is about to start, and written here while
	// there are none.
	max_body_ = c.max_body_bytes;

	/* The pages come from what the configuration says and from nowhere else, and
	   this is the one line that puts it there. A second place that installed it
	   would be a second answer to where this server serves from. */
	setDocroot(config().docroot);

	// Suspend and resume is allowed from the outset because it is a property of
	// the daemon, and a daemon in a running process cannot be given it later
	// without being torn down and rebuilt.
	daemon_ = MHD_start_daemon(
		flags,
		port, NULL, NULL, &accessHandler, NULL,
		MHD_OPTION_SOCK_ADDR, (const struct sockaddr *) &addr,
		MHD_OPTION_THREAD_POOL_SIZE, c.thread_pool,
		MHD_OPTION_CONNECTION_LIMIT, c.connection_limit,
		MHD_OPTION_CONNECTION_TIMEOUT, c.connection_timeout_s,
		MHD_OPTION_URI_LOG_CALLBACK, &uriSeen, &max_body_,
		MHD_OPTION_NOTIFY_COMPLETED, &requestDone, NULL,
		MHD_OPTION_END);
	if (daemon_ == NULL)
		return false;

	// A port of zero was a request to the kernel, so what was asked for is not
	// what was bound and a caller that wants to reach this has to be told.
	const union MHD_DaemonInfo *i = MHD_get_daemon_info(daemon_, MHD_DAEMON_INFO_BIND_PORT);
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> guard(portLock());
		bound_port_ = (i != NULL) ? (int) i->port : c.port;
	}

	/* After the daemon and not before it, because what a subscription feeds is a
	   connection of this daemon. Not a reason to fail the start either: a box
	   whose event streams cannot be served still answers everything else, and the
	   streams themselves are refused rather than served unattended. */
	events::start();
	livestream::start();
	return true;
}

void stop()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());

	if (daemon_ == NULL)
		return;

	/* Before the daemon and never after it. The library says that stopping a
	   daemon while a connection of it is suspended leaks the connection and the
	   socket or does something worse, and a stream with nothing to send is a
	   suspended connection. This answers once every one of them is gone. */
	events::stop();
	livestream::stop();

	// And the pass throughs, which are suspended connections for the same
	// reason and are torn down the same way.
	webtv::stopAll();

	/* Cleared before the daemon goes and not after, because the answer to which
	   port is bound while the daemon is being torn down is none, and because a
	   handler still running asks that question: it must not be left waiting on
	   this thread, which is waiting on it. */
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> guard(portLock());
		bound_port_ = 0;
	}

	MHD_stop_daemon(daemon_);
	daemon_ = NULL;
}

bool running()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	return daemon_ != NULL;
}

int boundPort()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(portLock());
	return bound_port_;
}

size_t openRequestsForTest()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(countLock());
	return open_requests_;
}

size_t bodyBytesKeptForTest()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(countLock());
	return body_bytes_kept_;
}

} // namespace httpd
