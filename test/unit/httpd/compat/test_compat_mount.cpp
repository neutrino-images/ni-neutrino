/*
 * test_compat_mount.cpp - tests for the legacy mount calls
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

// compat/ is not compiled with --disable-legacy-api (src/httpd/Makefile.am).
#include <config.h>

#ifndef DISABLE_LEGACY_API

#include "support/fakes.h"
#include "support/httpclient.h"

#include "httpd/compat/mount.h"

#include "httpd/auth.h"
#include "httpd/credentials.h"
#include "httpd/http.h"
#include "httpd/router.h"
#include "httpd/server.h"
#include "httpd/webconfig.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>

#include <csignal>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

using httpd::AuthLevel;
using httpd::Credentials;
using httpd::Get;
using httpd::Head;
using httpd::Method;
using httpd::Post;
using httpd::Response;
using httpd::StatusForbidden;
using httpd::StatusNotFound;
using httpd::compat::CyhookHandler;
using httpd::compat::Dispatch;
using httpd::compat::HTTP_NOT_IMPLEMENTED;
using httpd::compat::HTTP_OK;
using httpd::compat::M_GET;
using httpd::compat::M_HEAD;
using httpd::compat::M_POST;
using httpd::compat::THttp_Method;

namespace
{

// The three status codes this suite can prove without a real CControlAPI behind it:
// 404, 501 for a legacy name nobody answers for, and whatever a test's own recorder
// below sets, to check what the mount point does with an answer rather than what a
// particular legacy endpoint's table row says.

struct Reply
{
	int         code;
	std::string content_type;
	std::string body;

	Reply() : code(0) {}
};

// Split the way server.cpp's own callback splits the raw target, and take
// the same branch it takes: handles() first, the router's tables otherwise.
// This is what a request off a real socket does at this decision, minus the
// socket, matching how the other endpoint suites in this tree exercise the
// router directly rather than through a running daemon.
Reply call(const std::string &target, Method m, const std::string &host,
           const std::string &body, const std::string &content_type)
{
	std::string path = target;
	std::string query;
	const size_t q = target.find('?');
	if (q != std::string::npos)
	{
		path = target.substr(0, q);
		query = target.substr(q + 1);
	}

	Response r;
	if (httpd::compat::handles(path))
	{
		Credentials cred;
		cred.peer = "127.0.0.1";
		r = httpd::compat::answer(m, path, query, body, content_type, host, cred,
		                          AuthLevel::Read);
	}
	else
		r = httpd::dispatch(m, path, query, body, "127.0.0.1", AuthLevel::Read);

	Reply out;
	out.code = r.code;
	out.content_type = r.content_type;
	out.body = r.body;
	return out;
}

Reply get(const std::string &target, Method m = Get, const std::string &host = "")
{
	return call(target, m, host, "", "");
}

// The content type the old server takes a body apart for, spelled out once
// rather than in every case below, and spelled exactly: what it accepts is
// this string and not a prefix of it (compat/mount.cpp).
const char kForm[] = "application/x-www-form-urlencoded";

Reply post(const std::string &target, const std::string &body,
           const std::string &content_type = kForm)
{
	return call(target, Post, "", body, content_type);
}

Reply head(const std::string &target)
{
	return get(target, Head);
}

// installDispatch takes a plain function pointer, so a case that needs one installed
// and later removed cannot close over local state; a recorder therefore keeps what it
// saw in a function-local static, cleared before use. RAII around install/NULL, so a
// REQUIRE that fails partway leaves no dispatcher behind for the next case.
struct DispatchGuard
{
	explicit DispatchGuard(Dispatch fn) { httpd::compat::installDispatch(fn); }
	~DispatchGuard() { httpd::compat::installDispatch(NULL); }

private:
	DispatchGuard(const DispatchGuard &);
	DispatchGuard &operator=(const DispatchGuard &);
};

THttp_Method &lastMethodSeen()
{
	static THttp_Method m = httpd::compat::M_UNKNOWN;
	return m;
}

size_t &lastParamListSize()
{
	static size_t n = 0;
	return n;
}

/* The whole list a dispatcher was handed, kept so a case can ask what a
   handler would have read rather than what a size says. installDispatch takes
   a plain function pointer, so this is a function-local static for the same
   reason lastMethodSeen() is one. */
httpd::compat::CStringList &lastParams()
{
	static httpd::compat::CStringList p;
	return p;
}

// What a legacy handler reads for a name it did not get: the empty string
// operator[] leaves behind. Asked through find() so that a case cannot make a
// name appear by asking about it.
std::string param(const char *name)
{
	const httpd::compat::CStringList::const_iterator i = lastParams().find(name);
	return (i == lastParams().end()) ? std::string() : i->second;
}

// The value of a header a Response carries, and empty for one it does not -
// the same shape test_endpoints_write.cpp's own headerOf uses, kept local
// rather than shared because the two files build against different fakes.
std::string headerOf(const Response &r, const char *name)
{
	for (std::size_t i = 0; i < r.headers.size(); ++i)
	{
		if (r.headers[i].first == name)
			return r.headers[i].second;
	}
	return std::string();
}

// Stands in for CControlAPI::Execute without claiming to be it: a real one
// is not reachable here (see the file comment in mount.h). What it checks is
// only what answer() itself is responsible for handing a dispatcher and
// doing with what one hands back, not anything a legacy endpoint's own table
// row would say.
void recordingDispatch(CyhookHandler &hh)
{
	lastMethodSeen() = hh.Method;
	lastParamListSize() = hh.ParamList.size();
	lastParams() = hh.ParamList;
	hh.SetHeader(HTTP_OK, "text/plain");
	if (hh.Method == M_HEAD)
		return;
	hh.printf("body for %s", hh.UrlData["filename"].c_str());
}

// Stands in for the one path build_live_url and build_playlist's own
// missing-parameter branch actually takes: controlapi.cpp's SendError() with no
// argument, reached by a handler Execute() never calls SetHeader for, both being
// registered with an empty mime type. hh.httpStatus is left exactly as answer()
// handed it in; that is the one thing this dispatcher must not touch.
void noStatusDispatch(CyhookHandler &hh)
{
	hh.Write("error");
}

// Stands in for what encodeString() leaves in a body: it appends a zero byte
// to every value it returns (compat/helper.cpp:236, verbatim from the
// original), and the handlers that call it go on writing after that.
// getbouquets?encode=true and epg?bouquetnr= both answer down this shape.
void embeddedZeroDispatch(CyhookHandler &hh)
{
	hh.SetHeader(HTTP_OK, "text/plain");
	hh.Write(std::string("1 Dev\0\n2 Dev\n", 13));
}

// Stands in for a handler whose whole answer is the status line: SetHeader
// with nothing ever written to yresult afterward. yhook.cpp's BuildHeader
// gates Last-Modified on GetContentLength() > 0, and this is the shape that
// makes that false without also making the status one of the three that gate
// it on their own.
void emptyBodyDispatch(CyhookHandler &hh)
{
	hh.SetHeader(HTTP_OK, "text/plain");
}

// Stands in for a handler that writes a body and then calls SetError(), which
// controlapi.cpp's own SetError does too: it only assigns httpStatus and
// ResponseMimeType and never clears what a handler wrote. yhook.cpp excludes
// HTTP_NOT_FOUND from Last-Modified regardless of body length, so this is the one
// shape that tells that status check apart from the body-length check above.
void notFoundWithBodyDispatch(CyhookHandler &hh)
{
	hh.Write("still here");
	hh.SetError(httpd::compat::HTTP_NOT_FOUND);
}

// Stands in for the three handlers that write the request's own host into
// the url they answer with (controlapi.cpp's xmltvm3uCGI, build_live_url and
// build_playlist), each of them reading it out of HeaderList exactly like
// this.
void hostEchoDispatch(CyhookHandler &hh)
{
	hh.SetHeader(HTTP_OK, "text/plain");
	hh.Write("http://" + hh.HeaderList["Host"] + "/control/xmltv.xml");
}

// SendFile takes a plain path and installDispatch a plain function pointer;
// what path the next dispatch below hands to SendFile has to live somewhere
// neither of them carries, the same reason lastMethodSeen() above is a
// function-local static rather than a closure.
std::string &sendFilePath()
{
	static std::string p;
	return p;
}

void writeFile(const std::string &path, const std::string &bytes)
{
	std::ofstream f(path.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
	REQUIRE(f.good());
	f.write(bytes.data(), (std::streamsize) bytes.size());
}

// Named with the process, so two runs of this suite writing under the same
// bare name do not read the file the other one is mid-write on.
std::string sendfilePath(const std::string &suffix)
{
	return "/tmp/coreapi-mount-sendfile-" + std::to_string(getpid()) + "-" + suffix;
}

/* Takes away whichever file a case leaves through, the same reason
   DispatchGuard exists: a REQUIRE that fails partway through a case must not
   leave a stray file for the next one to trip over. */
struct SendFileGuard
{
	explicit SendFileGuard(const std::string &path) : path_(path) {}
	~SendFileGuard() { ::unlink(path_.c_str()); }

private:
	std::string path_;

	SendFileGuard(const SendFileGuard &);
	SendFileGuard &operator=(const SendFileGuard &);
};

// Stands in for the five call sites in controlapi.cpp that actually call
// SendFile (GetServicesxmlCGI, GetBouquetsxmlCGI, GetUBouquetsxmlCGI,
// VersionCGI, InfoCGI's own "version" branch), none of them reachable from
// this suite (see mount.h). What is testable here is only what answer() does
// with the status a dispatcher leaves it in, not any of their own table rows.
void sendFileDispatch(CyhookHandler &hh)
{
	hh.SendFile(sendFilePath());
}

// The same call, but after the handler already wrote something of its own -
// standing in for nothing in particular, since no real legacy handler mixes
// the two, but proving that a body written before SendFile is not what comes
// back is exactly what tells this branch in answer() apart from every other
// one in this file, all of which answer from yresult.
void sendFileAfterWriteDispatch(CyhookHandler &hh)
{
	hh.Write("whatever the handler wrote before calling SendFile");
	hh.SendFile(sendFilePath());
}

/* Puts the login policy back where the next case finds it, the same pattern
   test_auth.cpp's and test_compat_levels.cpp's own Policy use for the same
   reason: what is in effect is one value for the whole program, and the one
   case below that sets an account must not leave it standing for whichever
   case Catch2 runs next. */
struct Policy
{
	Policy() {}
	~Policy() { httpd::setConfigForTest(httpd::defaultWebConfig()); }

private:
	Policy(const Policy &);
	Policy &operator=(const Policy &);
};

/* The process's own time zone, held at one value for the length of a case and put back
   after it.

   A case that reads the host's zone is only as sharp as the machine it runs on: this
   image carries Etc/UTC, and a header built with localtime rather than gmtime reads
   exactly the same there. Measured: the Last-Modified case below passes with localtime
   under Etc/UTC and fails under a zone that is not UTC, with the same binary.

   A POSIX offset string rather than a zone name, so nothing depends on a zoneinfo
   database being installed. Five hours is far enough that no reading of a clock in the
   second this case spans can land on the other one by accident. */
struct Zone
{
	explicit Zone(const char *tz) : had(getenv("TZ") != NULL)
	{
		if (had)
			was = getenv("TZ");
		setenv("TZ", tz, 1);
		tzset();
	}

	~Zone()
	{
		if (had)
			setenv("TZ", was.c_str(), 1);
		else
			unsetenv("TZ");
		tzset();
	}

private:
	bool        had;
	std::string was;

	Zone(const Zone &);
	Zone &operator=(const Zone &);
};

void setLegacyAccount(const std::string &user, const std::string &pass)
{
	httpd::WebConfig c = httpd::defaultWebConfig();
	c.username = user;
	c.password_hash = httpd::hashSecret(pass);
	httpd::setConfigForTest(c);
}

/* defaultWebConfig() leaves legacy_enabled on, so a case below could get either
   reading of it by leaving the field alone. Both of the pair set it by hand instead:
   the point of each is which value server.cpp's own callback was actually driven
   with, not which one the default happened to leave standing. */
void setLegacyEnabled()
{
	httpd::WebConfig c = httpd::defaultWebConfig();
	c.legacy_enabled = true;
	httpd::setConfigForTest(c);
}

void setLegacyDisabled()
{
	httpd::WebConfig c = httpd::defaultWebConfig();
	c.legacy_enabled = false;
	httpd::setConfigForTest(c);
}

} // namespace

//=============================================================================
// handles(): a pure prefix test, the one part of this file that needs
// nothing installed and answers the same on every host this suite runs on.
//=============================================================================

TEST_CASE("handles owns exactly the /control/ prefix", "[compat][mount]")
{
	REQUIRE(httpd::compat::handles("/control/getmode"));
	REQUIRE(httpd::compat::handles("/control/"));
	REQUIRE(httpd::compat::handles("/control/a/b/c"));

	REQUIRE_FALSE(httpd::compat::handles("/control"));
	REQUIRE_FALSE(httpd::compat::handles("/"));
	REQUIRE_FALSE(httpd::compat::handles(""));
	REQUIRE_FALSE(httpd::compat::handles("/api/control/x"));
	REQUIRE_FALSE(httpd::compat::handles("/CONTROL/x"));
	REQUIRE_FALSE(httpd::compat::handles("/controlx"));
}

//=============================================================================
// What the mount point answers with nothing installed. This suite can never link
// CControlAPI::Execute (see mount.h), so every name here reads as "nobody answers for
// it" the same way an actually unknown name would to Execute itself: the two are
// indistinguishable from this side of the pointer, which is why installDispatch
// exists rather than a direct call.
//=============================================================================

TEST_CASE("an unknown legacy name answers 501, a path this server does not have answers 404", "[compat][mount]")
{
	REQUIRE(get("/control/gibtesnicht").code == 501);
	REQUIRE(get("/gibtesnicht").code == StatusNotFound);
}

TEST_CASE("a legacy path never reaches the file server", "[compat][mount]")
{
	// The unknown name is used on purpose: it proves the mount point took
	// the request without needing a real dispatcher installed, and the same
	// proof holds once a real one answers instead.
	REQUIRE(get("/control/gibtesnicht").code != StatusNotFound);
}

//=============================================================================
// What answer() does with what a dispatcher hands back, and what it hands a
// dispatcher going in. Both are this file's own plumbing and both hold whether or not
// a real CControlAPI is ever linked in.
//=============================================================================

TEST_CASE("GET and HEAD reach the dispatcher as the method they were asked as", "[compat][mount]")
{
	DispatchGuard guard(&recordingDispatch);

	lastMethodSeen() = httpd::compat::M_UNKNOWN;
	get("/control/getmode");
	REQUIRE(lastMethodSeen() == M_GET);

	lastMethodSeen() = httpd::compat::M_UNKNOWN;
	head("/control/getmode");
	REQUIRE(lastMethodSeen() == M_HEAD);
}

TEST_CASE("a method the legacy side never had a name for answers 501 before a dispatcher is ever asked", "[compat][mount]")
{
	// yrequest.cpp answers PUT, DELETE, TRACE and anything else its own method switch
	// never named (:88-97,127-138) before it reads even the rest of the header line:
	// no name is split out of the path, no dispatcher runs. A router method with no
	// legacy counterpart is answered the same way, ahead of the pointer neither can
	// see past. lastMethodSeen holding still at a value recordingDispatch never sets
	// is what proves the dispatcher was skipped.
	DispatchGuard guard(&recordingDispatch);
	lastMethodSeen() = httpd::compat::M_GET;

	const Reply put = get("/control/getmode", httpd::Put);
	REQUIRE(put.code == 501);
	REQUIRE(put.content_type == "text/html");
	REQUIRE(put.body.empty());
	REQUIRE(lastMethodSeen() == httpd::compat::M_GET);

	lastMethodSeen() = httpd::compat::M_GET;
	REQUIRE(get("/control/getmode", httpd::Delete).code == 501);
	REQUIRE(lastMethodSeen() == httpd::compat::M_GET);

	lastMethodSeen() = httpd::compat::M_GET;
	REQUIRE(get("/control/getmode", httpd::Patch).code == 501);
	REQUIRE(lastMethodSeen() == httpd::compat::M_GET);

	// TRACE has no member of its own in httpd::Method either, so a real TRACE request
	// and a genuinely malformed method both arrive here as UnknownMethod, answered the
	// same way, which is what yrequest.cpp's own method switch does too.
	lastMethodSeen() = httpd::compat::M_GET;
	REQUIRE(get("/control/getmode", httpd::UnknownMethod).code == 501);
	REQUIRE(lastMethodSeen() == httpd::compat::M_GET);
}

TEST_CASE("a multipart POST answers 501 before a dispatcher is ever asked", "[compat][mount]")
{
	// data/y-web/Y_Settings_Backup.yhtm and Y_Tools_Installer.yhtm both post this to
	// /control/exec, with their real arguments in the query and only the uploaded file
	// in the part bodyIsMultipart cannot take apart. A dispatcher that never ran is
	// the proof this does not run that query's own script blind.
	DispatchGuard guard(&recordingDispatch);
	lastMethodSeen() = httpd::compat::M_GET;

	const Reply r = post("/control/exec?Y_Tools&installer",
	                     "--x\r\nContent-Disposition: form-data; name=\"filename\"; "
	                     "filename=\"a\"\r\n\r\nhi\r\n--x--\r\n",
	                     "multipart/form-data; boundary=x");
	REQUIRE(r.code == 501);
	REQUIRE(r.content_type == "text/html");
	REQUIRE(r.body.empty());
	REQUIRE(lastMethodSeen() == httpd::compat::M_GET);
}

TEST_CASE("a form-urlencoded POST still reaches a legacy handler once multipart is refused", "[compat][mount]")
{
	// Guards the boundary of the check above: bodyIsMultipart matching more
	// than the one prefix it names would take the one content type this
	// layer does parse down with it.
	DispatchGuard guard(&recordingDispatch);

	const Reply r = post("/control/getmode", "");
	REQUIRE(r.code == HTTP_OK);
	REQUIRE(lastMethodSeen() == M_POST);
}

TEST_CASE("a content type nobody parses still reaches a dispatcher, never nothing at all", "[compat][mount]")
{
	// Measured against the real box: POST .../getbouquet with a "text/plain" body gets
	// curl "Connection reset by peer" there, not a status of any kind. yrequest.cpp reads
	// the body for no content type but the two it recognizes, so the bytes sit unread
	// when the old server closes the socket, and Linux resets a connection closed with
	// data still in its receive queue. Copying that unanswered silence here would be a
	// worse failure than the one being copied, so this dispatches instead.
	DispatchGuard guard(&recordingDispatch);

	const Reply r = post("/control/getbouquet?bouquet=1", "bouquet=2", "text/plain");
	REQUIRE(r.code == HTTP_OK);
	REQUIRE(lastMethodSeen() == M_POST);
	REQUIRE(param("bouquet") == "1");
}

TEST_CASE("HEAD carries the head a dispatcher stated and no body", "[compat][mount]")
{
	DispatchGuard guard(&recordingDispatch);

	const Reply plain = head("/control/getmode");
	REQUIRE(plain.body.empty());
	REQUIRE(plain.content_type == "text/plain");
	REQUIRE(plain.code == HTTP_OK);
}

TEST_CASE("a dispatcher's answer becomes the response verbatim", "[compat][mount]")
{
	DispatchGuard guard(&recordingDispatch);

	const Reply r = get("/control/getmode");
	REQUIRE(r.code == HTTP_OK);
	REQUIRE(r.content_type == "text/plain");
	REQUIRE(r.body == "body for getmode");
}

// yhttpd_core/yhook.cpp's session_init() sets httpStatus = HTTP_OK before any hook or
// handler runs there, so an old-server handler that answers without touching the
// response head still gets 200. CyhookHandler here starts at HTTP_NIL instead, and
// answer() is what has to supply the old server's default once nothing else set it:
// twenty of the 74 legacy names are registered with an empty mime type and can return
// down a path exactly like this one.
TEST_CASE("a dispatcher that never sets a status still answers 200, like the old server's own default", "[compat][mount]")
{
	DispatchGuard guard(&noStatusDispatch);

	const Reply r = get("/control/build_playlist");
	REQUIRE(r.code == HTTP_OK);
	REQUIRE(r.body == "error");
}

// The old server writes yresult to the socket as a C string (yresponse.cpp:191), so a
// body with a zero byte in it is sent only as far as that byte, whatever the
// Content-Length it announced says. Measured against both servers on one box:
// getbouquets?encode=true announces 101 bytes there and sends 32.
TEST_CASE("a body carrying a zero byte ends there, where the old server's own write ends", "[compat][mount]")
{
	DispatchGuard guard(&embeddedZeroDispatch);

	const Reply r = get("/control/getbouquets?encode=true");
	REQUIRE(r.code == HTTP_OK);
	REQUIRE(r.body == "1 Dev");
	REQUIRE(r.body.size() == 5);
}

// Three legacy handlers answer with a url naming the box a player is to fetch
// the stream from, and all three build it out of the request's Host header.
// Nothing between the socket and the dispatcher used to carry it, so all
// three answered "http://:31339/..." and "http:///control/xmltv.xml", which
// is a url no client can follow.
TEST_CASE("the Host the request carried reaches the legacy handlers that answer with a url", "[compat][mount]")
{
	DispatchGuard guard(&hostEchoDispatch);

	const Reply named = get("/control/xmltv.m3u", Get, "box.local:8081");
	REQUIRE(named.code == HTTP_OK);
	REQUIRE(named.body == "http://box.local:8081/control/xmltv.xml");

	// A request without the header leaves the handler with nothing, which is
	// what the old server's handlers get from such a request as well.
	const Reply bare = get("/control/xmltv.m3u");
	REQUIRE(bare.body == "http:///control/xmltv.xml");
}

//=============================================================================
// HANDLED_SENDFILE. The old server's own SendFile() never sets a status or a content
// type either, so a caller sees exactly the 200 default above and whatever
// ResponseMimeType already held; what changes here is only where the body comes from.
//=============================================================================

TEST_CASE("HANDLED_SENDFILE answers with the named file's own bytes, byte for byte", "[compat][mount]")
{
	const std::string path = sendfilePath("test.xml");
	const std::string content = "<?xml version=\"1.0\"?>\n<zapit api=\"4\">\n</zapit>\n";
	writeFile(path, content);
	SendFileGuard file(path);
	sendFilePath() = path;

	DispatchGuard guard(&sendFileDispatch);
	const Reply r = get("/control/getservicesxml");

	REQUIRE(r.code == HTTP_OK);
	REQUIRE(r.content_type == "");
	// A prefix or a length would both stay true if the read stopped short or
	// ran long; only an exact match proves every byte arrived.
	REQUIRE(r.body == content);
	REQUIRE(r.body.size() == content.size());
}

TEST_CASE("HANDLED_SENDFILE ignores whatever the handler already wrote to yresult", "[compat][mount]")
{
	const std::string path = sendfilePath("leftover-test.xml");
	const std::string content = "only the file content, never the leftover write";
	writeFile(path, content);
	SendFileGuard file(path);
	sendFilePath() = path;

	DispatchGuard guard(&sendFileAfterWriteDispatch);
	const Reply r = get("/control/getservicesxml");

	REQUIRE(r.body == content);
}

TEST_CASE("HANDLED_SENDFILE naming a file that is not there answers 200 with an empty body, like the old server's own", "[compat][mount]")
{
	// yresponse.cpp's own Sendfile() writes the 200 header before it ever opens the
	// file, so a name that has gone missing between the two leaves that server with a
	// claimed 200 and nothing behind it. Matching that exactly, rather than a 404 or
	// 500 of this layer's own choosing, is the point.
	sendFilePath() = "/tmp/coreapi-mount-sendfile-does-not-exist-4711.xml";

	DispatchGuard guard(&sendFileDispatch);
	const Reply r = get("/control/getservicesxml");

	REQUIRE(r.code == HTTP_OK);
	REQUIRE(r.body.empty());
}

TEST_CASE("HANDLED_SENDFILE refuses a name that resolves through a symlink", "[compat][mount]")
{
	// None of the five real call sites ever names anything a symlink could stand in
	// for, every one being a fixed path a macro supplies, but the discipline this
	// reads by (open once, O_NOFOLLOW, act on the descriptor) is one a later change
	// can quietly drop if nothing holds it.
	const std::string target = sendfilePath("target.xml");
	const std::string link = sendfilePath("link.xml");
	writeFile(target, "should never be read through the link");
	SendFileGuard targetFile(target);
	::unlink(link.c_str());
	REQUIRE(::symlink(target.c_str(), link.c_str()) == 0);
	SendFileGuard linkFile(link);
	sendFilePath() = link;

	DispatchGuard guard(&sendFileDispatch);
	const Reply r = get("/control/getservicesxml");

	REQUIRE(r.code == HTTP_OK);
	REQUIRE(r.body.empty());
}

TEST_CASE("HANDLED_SENDFILE never reads through a name that is not a regular file", "[compat][mount]")
{
	// None of the five real call sites can name anything but a regular file either,
	// but readSentFile checks S_ISREG for its own sake and not only through
	// O_NOFOLLOW: a FIFO resolves through no symlink at all and would still hang the
	// read loop this layer builds the whole answer in memory with, forever, on a
	// caller only asking for /control/getservicesxml.
	const std::string path = sendfilePath("fifo.xml");
	::unlink(path.c_str());
	REQUIRE(::mkfifo(path.c_str(), 0600) == 0);
	SendFileGuard file(path);

	// Opened O_RDWR from here so the FIFO already has a writer end before
	// readSentFile's own O_RDONLY open ever runs: without one, that open
	// blocks for a writer that this case would never send, which is a
	// deadlock and not a red case.
	const int keepOpen = ::open(path.c_str(), O_RDWR);
	REQUIRE(keepOpen >= 0);

	sendFilePath() = path;
	DispatchGuard guard(&sendFileDispatch);

	// The call below is exactly the one that can block on the FIFO's read end forever
	// if S_ISREG's short circuit is ever lost, and nothing on this thread can bound
	// that read from the outside once it is blocked. A forked child runs it instead,
	// the same escape test_basicclient.cpp uses; installDispatch was already called
	// above, so the child inherits that pointer from the fork. The outcome comes back
	// over a pipe.
	int pipefd[2];
	REQUIRE(::pipe(pipefd) == 0);

	std::fflush(0);
	const pid_t pid = fork();
	REQUIRE(pid >= 0);
	if (pid == 0)
	{
		::close(pipefd[0]);
		const Reply r = get("/control/getservicesxml");
		const int result[2] = { r.code, r.body.empty() ? 1 : 0 };
		const ssize_t written = ::write(pipefd[1], result, sizeof(result));
		(void) written;
		_exit(0);
	}
	::close(pipefd[1]);

	// Wide enough that a loaded machine never trips it, tight enough that a real hang
	// still ends this run with a named failure instead of the run ending it.
	int status = 0;
	bool exited = false;
	for (int i = 0; i < 800; ++i)
	{
		if (::waitpid(pid, &status, WNOHANG) == pid)
		{
			exited = true;
			break;
		}
		::usleep(10000);
	}

	if (!exited)
	{
		::kill(pid, SIGKILL);
		::waitpid(pid, &status, 0);
		::close(pipefd[0]);
		::close(keepOpen);
		FAIL("readSentFile hung reading a FIFO instead of refusing it through S_ISREG");
	}

	int result[2] = { -1, 0 };
	REQUIRE(::read(pipefd[0], result, sizeof(result)) == (ssize_t) sizeof(result));
	::close(pipefd[0]);
	::close(keepOpen);

	REQUIRE(result[0] == HTTP_OK);
	REQUIRE(result[1] == 1);
}

TEST_CASE("HANDLED_SENDFILE refuses a file at or beyond its own size ceiling", "[compat][mount]")
{
	// Read from mount.h rather than copied, the same way test_compat_usage.cpp
	// reads kMaxUnknownLegacyNames: a mutation to the real ceiling moves this
	// case's own expectation with it instead of leaving a second number here
	// to drift out of step with it unnoticed.
	const std::string::size_type kCeiling = httpd::compat::kMaxSendfileBytes;
	const std::string path = sendfilePath("over-ceiling.bin");
	{
		// A sparse file one byte past the ceiling: what readSentFile reads
		// off fstat is the size the filesystem reports, not the bytes
		// actually written, so this proves the same refusal without holding
		// sixteen megabytes of zeroes in this process to write.
		std::ofstream f(path.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
		REQUIRE(f.good());
		f.seekp((std::streamoff) kCeiling);
		f.put('\0');
		REQUIRE(f.good());
	}
	SendFileGuard file(path);
	sendFilePath() = path;

	DispatchGuard guard(&sendFileDispatch);
	const Reply r = get("/control/getservicesxml");

	REQUIRE(r.code == HTTP_OK);
	REQUIRE(r.body.empty());
}

TEST_CASE("the query is split on the router's already-split path and query, not on a rebuilt string", "[compat][mount]")
{
	DispatchGuard guard(&recordingDispatch);

	// getbouquet?format=xml is analyzeURL's own worked example (query.cpp);
	// what matters here is only that the filename UrlData ends up with is
	// the last path segment and the query never leaks into it.
	const Reply r = get("/control/getbouquet?format=xml");
	REQUIRE(r.body == "body for getbouquet");
}

TEST_CASE("a path with no query at all leaves ParamList untouched, unlike a path with a bare trailing '?'", "[compat][mount]")
{
	// analyzeURL only calls parseParams when it finds a "?" itself; reattaching one
	// here for every request would call parseParams("") on every request that never
	// had one and add the one spurious entry parseParams leaves behind for an empty
	// parameter string, corrupting ParamList["1"] for every handler that reads a
	// positional argument.
	DispatchGuard guard(&recordingDispatch);

	get("/control/getmode");
	REQUIRE(lastParamListSize() == 0);

	get("/control/getbouquet?format=xml");
	REQUIRE(lastParamListSize() > 0);
}

TEST_CASE("removing the dispatcher again answers exactly like never having installed one", "[compat][mount]")
{
	{
		DispatchGuard guard(&recordingDispatch);
		REQUIRE(get("/control/getmode").code == HTTP_OK);
	}
	REQUIRE(get("/control/getmode").code == HTTP_NOT_IMPLEMENTED);
}

//=============================================================================
// Everything above calls handles()/answer() directly, which proves this layer's own
// logic without a socket in the way. None of it proves that server.cpp's own callback
// actually reaches this mount point before its router tables. One case with a real
// daemon on a real loopback port closes that.
//=============================================================================

TEST_CASE("a real request against a running server reaches the mount point before the router's tables", "[compat][mount]")
{
	InstalledDependencies wired;
	Policy policy;
	setLegacyEnabled();
	DispatchGuard guard(&recordingDispatch);

	httpd::ServerConfig c = httpd::defaultConfig();
	c.port = 0;
	c.bind_address = "127.0.0.1";
	REQUIRE(httpd::start(c));
	const int port = httpd::boundPort();
	REQUIRE(port > 0);

	lastMethodSeen() = httpd::compat::M_UNKNOWN;
	const testhttp::Reply r = testhttp::request(port, "GET", "/control/getmode");
	httpd::stop();

	REQUIRE(r.transport_ok);
	REQUIRE(r.code == HTTP_OK);
	REQUIRE(r.body == "body for getmode");
	REQUIRE(lastMethodSeen() == M_GET);
}

// Neither of the two cases above proves what a box that never wrote the key at all
// does, because both drive legacy_enabled by hand. This one does not: defaultWebConfig()
// is the whole of what it sets, so a /control/ request reaching the dispatcher here is
// what a box nobody has configured yet answers with, which is the reason the default
// is on rather than off.
TEST_CASE("a fresh box nobody configured still answers /control/, because the default is on", "[compat][mount]")
{
	InstalledDependencies wired;
	Policy policy;
	httpd::setConfigForTest(httpd::defaultWebConfig());
	DispatchGuard guard(&recordingDispatch);

	httpd::ServerConfig c = httpd::defaultConfig();
	c.port = 0;
	c.bind_address = "127.0.0.1";
	REQUIRE(httpd::start(c));
	const int port = httpd::boundPort();
	REQUIRE(port > 0);

	lastMethodSeen() = httpd::compat::M_UNKNOWN;
	const testhttp::Reply r = testhttp::request(port, "GET", "/control/getmode");
	httpd::stop();

	REQUIRE(r.transport_ok);
	REQUIRE(r.code == HTTP_OK);
	REQUIRE(r.body == "body for getmode");
	REQUIRE(lastMethodSeen() == M_GET);
}

// The other half of the pair above: a box that had the surface and was told to turn it
// off. A /control/ path answers like any path this server does not have, and the
// dispatcher installed above is never asked, which is what a build with
// DISABLE_LEGACY_API would answer too, though for the opposite reason.
TEST_CASE("legacy_enabled off keeps a real request out of the mount point", "[compat][mount]")
{
	InstalledDependencies wired;
	Policy policy;
	setLegacyDisabled();
	DispatchGuard guard(&recordingDispatch);

	httpd::ServerConfig c = httpd::defaultConfig();
	c.port = 0;
	c.bind_address = "127.0.0.1";
	REQUIRE(httpd::start(c));
	const int port = httpd::boundPort();
	REQUIRE(port > 0);

	lastMethodSeen() = httpd::compat::M_UNKNOWN;
	const testhttp::Reply r = testhttp::request(port, "GET", "/control/getmode");
	httpd::stop();

	REQUIRE(r.transport_ok);
	REQUIRE(r.code == StatusNotFound);
	REQUIRE(lastMethodSeen() == httpd::compat::M_UNKNOWN);
}

// The case above proves the "if" taken. It does not prove the "if" is still an if: a
// build that answered every path through compat::answer() would pass it exactly the
// same way, since it never asks for anything outside /control/. This case asks the
// same running server for a path this mount point does not own and checks both ends:
// the router's own fallback still answers it, and the dispatcher never saw it.
TEST_CASE("a real request outside /control/ against a running server never reaches the legacy mount point", "[compat][mount]")
{
	InstalledDependencies wired;
	// Enabled on purpose, the same as the case above: the point here is that
	// the path decides, not that the switch happened to be off already.
	Policy policy;
	setLegacyEnabled();
	DispatchGuard guard(&recordingDispatch);

	httpd::ServerConfig c = httpd::defaultConfig();
	c.port = 0;
	c.bind_address = "127.0.0.1";
	REQUIRE(httpd::start(c));
	const int port = httpd::boundPort();
	REQUIRE(port > 0);

	lastMethodSeen() = httpd::compat::M_UNKNOWN;
	const testhttp::Reply r = testhttp::request(port, "GET", "/nothing-is-here");
	httpd::stop();

	REQUIRE(r.transport_ok);
	REQUIRE(r.code == StatusNotFound);
	REQUIRE(lastMethodSeen() == httpd::compat::M_UNKNOWN);
}

// Every path the router answers carries addApiHeaders: a Cache-Control the old server
// never sent in this shape, a Vary naming a cookie and a token this prefix's own auth
// never reads, and an X-Content-Type-Options a legacy consumer has never seen. Checked on
// a real reply rather than on what answer() builds, since addApiHeaders is applied by the
// caller in server.cpp.
//
// Cache-Control itself is not absent, only different from the router's: answer() writes
// exactly the value the old server writes on this same answer. What this one proves is
// that the router's own reasons for the header never reach this prefix.
TEST_CASE("a real reply from the legacy mount point carries none of the router's own headers", "[compat][mount]")
{
	InstalledDependencies wired;
	Policy policy;
	setLegacyEnabled();
	DispatchGuard guard(&recordingDispatch);

	httpd::ServerConfig c = httpd::defaultConfig();
	c.port = 0;
	c.bind_address = "127.0.0.1";
	REQUIRE(httpd::start(c));
	const int port = httpd::boundPort();
	REQUIRE(port > 0);

	const testhttp::Reply r = testhttp::request(port, "GET", "/control/getmode");
	httpd::stop();

	REQUIRE(r.transport_ok);
	REQUIRE(r.code == HTTP_OK);
	REQUIRE(r.header("Cache-Control") == "no-cache");
	REQUIRE(r.header("Vary") == "");
	REQUIRE(r.header("X-Content-Type-Options") == "");
	// Same shape as Cache-Control above, checked over the same real socket
	// rather than assumed to carry over from answer()'s own case below just
	// because queueResponse (server.cpp) is the one function that turns
	// either header into wire bytes. The exact value is that function's own
	// case to prove; this one only needs the RFC1123 shape to be there.
	REQUIRE(r.header("Last-Modified").size() == 29);
	REQUIRE(r.header("Last-Modified").substr(25) == " GMT");
}

// Measured directly against the old server (yhook.cpp's BuildHeader): every answer
// whose CacheCategory is empty and whose cache argument is false gets exactly this
// line, no more and no less. "no-store" is the router's own value and not this one's,
// and a caller that used to be told a legacy answer could never be cached is told the
// same thing again rather than something related but different.
TEST_CASE("answer() writes the old server's own Cache-Control on a dispatcher-driven reply", "[compat][mount]")
{
	Policy policy;
	DispatchGuard guard(&recordingDispatch);

	Credentials cred;
	cred.peer = "127.0.0.1";
	const Response r = httpd::compat::answer(Get, "/control/getmode", "", "", "", "", cred, AuthLevel::Read);

	REQUIRE(r.code == HTTP_OK);
	REQUIRE(headerOf(r, "Cache-Control") == "no-cache");
}

// The one shape of legacy answer that does not carry this, or Last-Modified beside it.
// yresponse.cpp:139 passes a true cache argument into BuildHeader for HANDLED_SENDFILE,
// which takes it down its other branch and writes Expires instead. The five call sites
// never assign hh->ContentLength either, so GetContentLength() reads 0 there and the old
// server's Last-Modified line, gated on that value being positive, is never reached for
// these five answers regardless of branch.
TEST_CASE("answer() writes no Cache-Control and no Last-Modified on a HANDLED_SENDFILE reply", "[compat][mount]")
{
	const std::string path = sendfilePath("cache-control-test.xml");
	writeFile(path, "<zapit/>\n");
	SendFileGuard file(path);
	sendFilePath() = path;

	Policy policy;
	DispatchGuard guard(&sendFileDispatch);

	Credentials cred;
	cred.peer = "127.0.0.1";
	const Response r = httpd::compat::answer(Get, "/control/getservicesxml", "", "", "", "", cred, AuthLevel::Read);

	REQUIRE(r.code == HTTP_OK);
	REQUIRE(r.headers.empty());
	REQUIRE(headerOf(r, "Last-Modified") == "");
}

// Measured directly against the old server the same way the Cache-Control case was:
// yhook.cpp's BuildHeader stamps this on every answer whose status is none of
// HTTP_NOT_MODIFIED/HTTP_NOT_FOUND/HTTP_REQUEST_RANGE_NOT_SATISFIABLE and whose
// GetContentLength() is positive, with time(NULL) unless hh->LastModified carries
// something other than its -1 sentinel. Bracketed between two readings of that clock,
// so a slow host that crosses a second boundary mid-call still passes.
TEST_CASE("answer() writes the old server's own Last-Modified on a dispatcher-driven reply, stamped with the moment it answered", "[compat][mount]")
{
	Policy policy;
	DispatchGuard guard(&recordingDispatch);

	// The header says GMT and has to be GMT, whatever the box's clock is set
	// to locally. Both sides below are built from gmtime, so on a host that
	// already is UTC they agree no matter which of the two the answer used;
	// this is what makes them disagree when it matters (see Zone).
	Zone zone("XXX-05:00");

	const time_t before = time(NULL);
	Credentials cred;
	cred.peer = "127.0.0.1";
	const Response r = httpd::compat::answer(Get, "/control/getmode", "", "", "", "", cred, AuthLevel::Read);
	const time_t after = time(NULL);

	REQUIRE(r.code == HTTP_OK);
	const std::string lm = headerOf(r, "Last-Modified");
	REQUIRE(lm.size() == 29);
	REQUIRE(lm.substr(25) == " GMT");

	// hook.cpp's own constructor is what this depends on: a fresh CyhookHandler resets
	// LastModified to its -1 "dynamic content" sentinel, and none of this suite's
	// dispatchers assign the field. Formatting the wrong sentinel (0) stamps 1 Jan
	// 1970 instead, a fixed string that will not appear in the window between "before"
	// and "after", so this fails loudly on that regression.
	bool matchedRecently = false;
	for (time_t t = before; t <= after + 1 && !matchedRecently; ++t)
	{
		const struct tm expected = *gmtime(&t);
		char want[80];
		strftime(want, sizeof(want), "%a, %d %b %Y %H:%M:%S GMT", &expected);
		matchedRecently = (lm == want);
	}
	REQUIRE(matchedRecently);
}

// The body-length half of the old server's own condition, held apart from
// the status-code half below: GetContentLength() is 0 for an answer nobody
// wrote to, which yhook.cpp's own "else if (GetContentLength() > 0)" leaves
// on the same side as a 304/404/416 without that status ever being reached.
TEST_CASE("answer() writes no Last-Modified when the body is empty", "[compat][mount]")
{
	Policy policy;
	DispatchGuard guard(&emptyBodyDispatch);

	Credentials cred;
	cred.peer = "127.0.0.1";
	const Response r = httpd::compat::answer(Get, "/control/getmode", "", "", "", "", cred, AuthLevel::Read);

	REQUIRE(r.code == HTTP_OK);
	REQUIRE(r.body.empty());
	REQUIRE(headerOf(r, "Last-Modified") == "");
}

// The status half of the same condition: a body that is not empty, on a status
// yhook.cpp excludes by name regardless of GetContentLength(). HTTP_NOT_FOUND is the
// one of the three this mount point can reach through a dispatcher; the other two are
// file-range outcomes nothing under /control/ ever sets, so proving this one exercises
// the same three-way switch rather than inventing an unreachable case.
TEST_CASE("answer() writes no Last-Modified on a 404, even with a body already written", "[compat][mount]")
{
	Policy policy;
	DispatchGuard guard(&notFoundWithBodyDispatch);

	Credentials cred;
	cred.peer = "127.0.0.1";
	const Response r = httpd::compat::answer(Get, "/control/getmode", "", "", "", "", cred, AuthLevel::Read);

	REQUIRE(r.code == (int) httpd::compat::HTTP_NOT_FOUND);
	REQUIRE_FALSE(r.body.empty());
	REQUIRE(headerOf(r, "Last-Modified") == "");
}

//=============================================================================
// Basic over this mount point. answer() now reads cred.basic, and server.cpp's
// credentialsOf() is what fills it in from a real Authorization header before this is
// ever asked; a path built and tested at the level of legacyAllows alone is a promise
// nothing calls.
//
// This cannot be shown over a live socket in this suite. A loopback peer reaches every
// level with no header at all, and every socket here is loopback. A case built that way
// was tried and measured: reverting server.cpp's one added line left every case in this
// file green. So the one case below goes through answer() itself, because answer() is
// what changed: it used to build an empty Credentials and never read cred.basic. The
// credential is decoded through the same basicCredential a real header goes through, so
// what is untested is only the one line in credentialsOf().
//=============================================================================
TEST_CASE("a real Basic header carries a private-range caller to a write endpoint through answer(), and a wrong password does not", "[compat][mount]")
{
	Policy policy;
	setLegacyAccount("ni", "ni");
	DispatchGuard guard(&recordingDispatch);

	Credentials right;
	right.peer = "192.168.1.9";
	right.basic = httpd::basicCredential("Basic bmk6bmk=");   // base64("ni:ni")
	REQUIRE(right.basic == "ni:ni");

	lastMethodSeen() = httpd::compat::M_UNKNOWN;
	const Response allowed = httpd::compat::answer(Get, "/control/standby", "on", "", "", "", right, AuthLevel::Read);
	REQUIRE(allowed.code == HTTP_OK);
	REQUIRE(allowed.body == "body for standby");
	REQUIRE(lastMethodSeen() == M_GET);

	Credentials wrong;
	wrong.peer = "192.168.1.9";
	wrong.basic = httpd::basicCredential("Basic bmk6ZmFsc2No"); // base64("ni:falsch")
	REQUIRE(wrong.basic == "ni:falsch");

	lastMethodSeen() = httpd::compat::M_UNKNOWN;
	const Response denied = httpd::compat::answer(Get, "/control/standby", "on", "", "", "", wrong, AuthLevel::Read);
	REQUIRE(denied.code == StatusForbidden);
	REQUIRE(lastMethodSeen() == httpd::compat::M_UNKNOWN);
}


//=============================================================================
// POST bodies. The old server reads one for POST and no other method, for one content
// type, into the very ParamList the query already went into and after it
// (yrequest.cpp:97-101 and :295-307). Everything below is that reading, held to the
// byte, because a handler under this prefix cannot tell where a parameter came from
// and several of them do something destructive about one that is missing.
//=============================================================================

TEST_CASE("a posted form reaches a legacy handler in the same list the query does", "[compat][mount]")
{
	DispatchGuard guard(&recordingDispatch);

	// Measured on the box this was written against: POST /control/getbouquet
	// with this body answers the bouquet on 8080 and "error=no parameter" on
	// 8081, because nothing carried the body to the handler.
	const Reply r = post("/control/getbouquet", "bouquet=1");

	REQUIRE(r.code == HTTP_OK);
	REQUIRE(lastMethodSeen() == M_POST);
	REQUIRE(param("bouquet") == "1");
}

TEST_CASE("the method a legacy handler sees for a POST is POST", "[compat][mount]")
{
	// Execute() itself only ever compares against M_HEAD (controlapi.cpp:314),
	// so a POST arriving as M_UNKNOWN still runs its handler and nothing looks
	// wrong from the outside - which is exactly why the value is worth
	// pinning: the old server sets M_POST and a handler that ever grows a
	// branch on it must see the same thing there and here.
	DispatchGuard guard(&recordingDispatch);

	REQUIRE(post("/control/getmode", "").code == HTTP_OK);
	REQUIRE(lastMethodSeen() == M_POST);

	REQUIRE(get("/control/getmode").code == HTTP_OK);
	REQUIRE(lastMethodSeen() == M_GET);
}

TEST_CASE("the query and the body meet in one list, the old server's way round", "[compat][mount]")
{
	/* ParseStartLine fills ParameterList out of the query and HandlePost calls the same
	   ParseParams on the body afterwards without clearing it, so a name in both is
	   joined with a comma in that order, and the positional numbers the body's pieces
	   are filed under carry on counting from the size the query left behind. */
	DispatchGuard guard(&recordingDispatch);

	const Reply r = post("/control/getbouquet?bouquet=1", "bouquet=2&format=xml");
	REQUIRE(r.code == HTTP_OK);

	// The query's value first, the body's second, joined with a comma.
	REQUIRE(param("bouquet") == "1,2");
	REQUIRE(param("format") == "xml");

	/* Where each piece's position is filed. parseParams writes the position after it
	   has written the pair, and numbers it off the size of the whole list at that
	   moment, so the query's one pair lands at "2" and not at "1". The body then counts
	   on from there: its first piece adds no entry of its own and is filed at "3", its
	   second adds one and is filed at "5". Eleven handlers in controlapi.cpp read a
	   position. */
	REQUIRE(param("1").empty());
	REQUIRE(param("2") == "bouquet");
	REQUIRE(param("3") == "bouquet");
	REQUIRE(param("5") == "format");
}

TEST_CASE("a POST with no query numbers its own parameters from where an empty list leaves off", "[compat][mount]")
{
	// The list starts empty for a request with no "?" at all (analyzeURL only
	// parses when it sees one), so the body's first pair goes in at "2" and a
	// bare token with no "=" at "1". Both are what the old server files them
	// under, and a positional read - controlapi.cpp asks for ParamList["1"] in
	// eleven handlers - is what depends on it.
	DispatchGuard guard(&recordingDispatch);

	post("/control/standby", "off");
	REQUIRE(param("1") == "off");

	post("/control/getbouquet", "bouquet=1");
	REQUIRE(param("bouquet") == "1");
	REQUIRE(param("2") == "bouquet");
	REQUIRE(param("1").empty());
}

TEST_CASE("a content type the old server does not take apart leaves the body unread", "[compat][mount]")
{
	/* yrequest.cpp:295 compares Content-Type whole, so a request naming a charset
	   beside the type is one that server parses nothing out of. Measured against both
	   servers: POST /control/getbouquet with "application/x-www-form-urlencoded;
	   charset=UTF-8" answers no bouquet on either. A prefix comparison here would be
	   the plausible reading and not the old server's. */
	DispatchGuard guard(&recordingDispatch);

	post("/control/getbouquet", "bouquet=1", "application/x-www-form-urlencoded; charset=UTF-8");
	REQUIRE(param("bouquet").empty());

	post("/control/getbouquet", "bouquet=1", "text/plain");
	REQUIRE(param("bouquet").empty());

	post("/control/getbouquet", "bouquet=1", "");
	REQUIRE(param("bouquet").empty());

	// The value is trimmed before it is compared, because ParseHeader trims
	// every header value it stores (yrequest.cpp:201).
	post("/control/getbouquet", "bouquet=1", "  application/x-www-form-urlencoded  ");
	REQUIRE(param("bouquet") == "1");
}

TEST_CASE("a body on a method the old server never reads one for is not read here either", "[compat][mount]")
{
	// HandleRequest reads a body for POST alone: GET and HEAD read a head and
	// stop, and PUT, DELETE and TRACE are answered 501 before any body is
	// looked at (yrequest.cpp:76-101).
	DispatchGuard guard(&recordingDispatch);

	call("/control/getbouquet", Get, "", "bouquet=1", kForm);
	REQUIRE(param("bouquet").empty());

	call("/control/getbouquet", Head, "", "bouquet=1", kForm);
	REQUIRE(param("bouquet").empty());
}

TEST_CASE("the form that costs a box its lists arrives whole", "[compat][mount]")
{
	/* data/y-web/Y_Settings_xmltv.yhtm:17 posts these three names to
	   /control/xmltvlist, and xmltvlistCGI (controlapi.cpp:3736-3788) has an else
	   branch for each one that clears the list when the parameter is empty. Reaching
	   that handler with an empty list is not a request that fails, it is the box's
	   WebTV, WebRadio and XMLTV lists emptied and "ok" answered. */
	DispatchGuard guard(&recordingDispatch);

	const Reply r = post("/control/xmltvlist",
	                     "webtv=http%3A%2F%2Fa%2Ftv.xml&webradio=http%3A%2F%2Fa%2Fradio.xml"
	                     "&xmltv=http%3A%2F%2Fa%2Fepg.xml&tmpl=Y_Settings_xmltv.yhtm");

	REQUIRE(r.code == HTTP_OK);
	REQUIRE(param("webtv") == "http://a/tv.xml");
	REQUIRE(param("webradio") == "http://a/radio.xml");
	REQUIRE(param("xmltv") == "http://a/epg.xml");
}

TEST_CASE("a parameter only the body carries still decides the level", "[compat][mount]")
{
	/* The level a Mixed name needs is read off the parameters its handler will
	   read (compat/levels.h), and since a body reaches that list, action=remove
	   reaches TimerCGI from a body exactly as it does from a query. A gate that
	   read the query alone would answer Read for this and hand a caller from
	   the private range a handler that deletes a timer. */
	DispatchGuard guard(&recordingDispatch);

	Credentials lan;
	lan.peer = "192.168.1.9";

	lastMethodSeen() = httpd::compat::M_UNKNOWN;
	const Response fromBody = httpd::compat::answer(Post, "/control/timer", "",
	                                                "action=remove&id=1", kForm, "",
	                                                lan, AuthLevel::Read);
	REQUIRE(fromBody.code == StatusForbidden);
	REQUIRE(lastMethodSeen() == httpd::compat::M_UNKNOWN);

	// The same words in the query are refused too, which is what says the
	// refusal above is about the parameter and not about the method.
	const Response fromQuery = httpd::compat::answer(Get, "/control/timer",
	                                                 "action=remove&id=1", "", "", "",
	                                                 lan, AuthLevel::Read);
	REQUIRE(fromQuery.code == StatusForbidden);

	// And a POST that asks that name for nothing but a listing still reaches
	// it, so the rule above is a rule about the parameter and not about POST.
	const Response reading = httpd::compat::answer(Post, "/control/timer", "",
	                                               "format=json", kForm, "",
	                                               lan, AuthLevel::Read);
	REQUIRE(reading.code == HTTP_OK);
	REQUIRE(lastMethodSeen() == M_POST);
}

TEST_CASE("wouldDispatch answers for the head exactly what answer() answers for it", "[compat][mount]")
{
	/* What the transport asks before it accepts a body (server.cpp). The two
	   have to agree for a request with no body, because a body dropped there
	   is a handler run on parameters that were thrown away - the very shape of
	   the bug the body parameter fixes. */
	DispatchGuard guard(&recordingDispatch);

	Credentials lan;
	lan.peer = "192.168.1.9";

	REQUIRE(httpd::compat::wouldDispatch("/control/getmode", "", lan, AuthLevel::Read));
	REQUIRE(httpd::compat::answer(Get, "/control/getmode", "", "", "", "", lan,
	                              AuthLevel::Read).code == HTTP_OK);

	REQUIRE_FALSE(httpd::compat::wouldDispatch("/control/shutdown", "", lan, AuthLevel::Read));
	REQUIRE(httpd::compat::answer(Get, "/control/shutdown", "", "", "", "", lan,
	                              AuthLevel::Read).code == StatusForbidden);

	REQUIRE_FALSE(httpd::compat::wouldDispatch("/control/setmode", "radio", lan, AuthLevel::Read));
	REQUIRE(httpd::compat::answer(Get, "/control/setmode", "radio", "", "", "", lan,
	                              AuthLevel::Read).code == StatusForbidden);

	// A loopback caller reaches every level with no credential at all, which
	// is what 22 plugin files rely on, and is what makes a body worth keeping
	// for every one of their POSTs.
	Credentials loop;
	loop.peer = "127.0.0.1";
	REQUIRE(httpd::compat::wouldDispatch("/control/shutdown", "", loop, AuthLevel::Public));
}

//=============================================================================
// The half of the same fix that lives in server.cpp: a body is only kept for a request
// something is going to read one for, and until this prefix said so for itself,
// nothing did. None of the cases above can see that half.
//=============================================================================

TEST_CASE("a real POST against a running server reaches the mount point with its body", "[compat][mount]")
{
	InstalledDependencies wired;
	Policy policy;
	setLegacyEnabled();
	DispatchGuard guard(&recordingDispatch);

	httpd::ServerConfig c = httpd::defaultConfig();
	c.port = 0;
	c.bind_address = "127.0.0.1";
	REQUIRE(httpd::start(c));
	const int port = httpd::boundPort();
	REQUIRE(port > 0);

	std::vector<std::pair<std::string, std::string> > headers;
	headers.push_back(std::make_pair(std::string("Content-Type"), std::string(kForm)));

	const std::string body = "webtv=http%3A%2F%2Fa%2Ftv.xml";
	const size_t kept_before = httpd::bodyBytesKeptForTest();

	lastMethodSeen() = httpd::compat::M_UNKNOWN;
	lastParams().clear();
	const testhttp::Reply r = testhttp::request(port, "POST", "/control/xmltvlist",
	                                            headers, body);
	httpd::stop();

	REQUIRE(r.transport_ok);
	REQUIRE(r.code == HTTP_OK);
	REQUIRE(lastMethodSeen() == M_POST);
	REQUIRE(param("webtv") == "http://a/tv.xml");
	// The bytes were taken in on purpose and not merely read past: what tells
	// a body that was kept from one that was dropped is invisible from a
	// socket (server.h, bodyBytesKeptForTest).
	REQUIRE(httpd::bodyBytesKeptForTest() == kept_before + body.size());
}

TEST_CASE("a legacy body is kept for the one method that reads one and for no other", "[compat][mount]")
{
	/* answer() reads a body for M_POST alone, so every byte held for any other method was
	   always going to be dropped unread. Measured against a real socket before this was
	   written: all six methods held a 4096 byte body to the end of the request.

	   Loopback is what makes this provable at all. The other half of the same gate, "a
	   caller this surface is going to turn away holds no body either", cannot be reached
	   from here: legacyAllows lets a loopback peer through in its first line. That half is
	   held as text (check-legacy-bodygate.sh). */
	InstalledDependencies wired;
	Policy policy;
	setLegacyEnabled();
	DispatchGuard guard(&recordingDispatch);

	httpd::ServerConfig c = httpd::defaultConfig();
	c.port = 0;
	c.bind_address = "127.0.0.1";
	REQUIRE(httpd::start(c));
	const int port = httpd::boundPort();
	REQUIRE(port > 0);

	std::vector<std::pair<std::string, std::string> > headers;
	headers.push_back(std::make_pair(std::string("Content-Type"), std::string(kForm)));

	const std::string body(4096, 'x');

	const size_t before_get = httpd::bodyBytesKeptForTest();
	const testhttp::Reply got = testhttp::request(port, "GET", "/control/getmode",
	                                              headers, body);
	const size_t after_get = httpd::bodyBytesKeptForTest();

	const size_t before_post = after_get;
	const testhttp::Reply posted = testhttp::request(port, "POST", "/control/getmode",
	                                                 headers, body);
	const size_t after_post = httpd::bodyBytesKeptForTest();
	httpd::stop();

	// The GET is answered, so the request really reached the prefix rather
	// than falling out somewhere ahead of the gate this measures.
	REQUIRE(got.transport_ok);
	REQUIRE(got.code == HTTP_OK);
	REQUIRE(after_get == before_get);

	// The same request as a POST, so that a counter that never moves at all
	// cannot read as the answer above.
	REQUIRE(posted.transport_ok);
	REQUIRE(posted.code == HTTP_OK);
	REQUIRE(after_post == before_post + body.size());
}

TEST_CASE("a legacy POST is held to the same body ceiling as every other request", "[compat][mount]")
{
	/* The prefix answers outside the router's tables, so it would have been the one way
	   past the ceiling the configuration sets. Measured against both servers: 1.2 MB
	   posted to /control/getbouquet is answered 200 by the old server, which reads all
	   of it, and 413 by this one. */
	InstalledDependencies wired;
	Policy policy;
	setLegacyEnabled();
	DispatchGuard guard(&recordingDispatch);

	httpd::ServerConfig c = httpd::defaultConfig();
	c.port = 0;
	c.bind_address = "127.0.0.1";
	c.max_body_bytes = 1024;
	REQUIRE(httpd::start(c));
	const int port = httpd::boundPort();
	REQUIRE(port > 0);

	std::vector<std::pair<std::string, std::string> > headers;
	headers.push_back(std::make_pair(std::string("Content-Type"), std::string(kForm)));

	lastMethodSeen() = httpd::compat::M_UNKNOWN;
	const std::string tooMuch = "webtv=" + std::string(4096, 'x');
	const testhttp::Reply big = testhttp::request(port, "POST", "/control/xmltvlist",
	                                              headers, tooMuch);

	// And one under the ceiling through the same daemon, so that a 413 for
	// everything would not read as the ceiling working.
	lastParams().clear();
	const testhttp::Reply small = testhttp::request(port, "POST", "/control/xmltvlist",
	                                                headers, "webtv=fits");
	httpd::stop();

	REQUIRE(big.transport_ok);
	REQUIRE(big.code == 413);
	REQUIRE(small.transport_ok);
	REQUIRE(small.code == HTTP_OK);
	REQUIRE(param("webtv") == "fits");
}

//=============================================================================
// The case that drives answer()'s host parameter directly proves only that answer()
// forwards whatever it is handed, not that serve() ever puts a real connection's Host
// header there rather than dropping it. A real socket is what closes that.
//=============================================================================

TEST_CASE("a real request's Host header reaches the legacy handlers that answer with a url", "[compat][mount]")
{
	InstalledDependencies wired;
	Policy policy;
	setLegacyEnabled();
	DispatchGuard guard(&hostEchoDispatch);

	httpd::ServerConfig c = httpd::defaultConfig();
	c.port = 0;
	c.bind_address = "127.0.0.1";
	REQUIRE(httpd::start(c));
	const int port = httpd::boundPort();
	REQUIRE(port > 0);

	std::vector<std::pair<std::string, std::string> > named_headers;
	named_headers.push_back(std::make_pair(std::string("Host"), std::string("box.local:9999")));
	const testhttp::Reply named = testhttp::request(port, "GET", "/control/xmltv.m3u", named_headers);

	// A second, differently-named box against the same daemon: not a repeat
	// of the assertion above under a different label, but proof this reads
	// whatever the connection actually carried rather than one literal this
	// case happens to send and a fix happens to special-case.
	std::vector<std::pair<std::string, std::string> > other_headers;
	other_headers.push_back(std::make_pair(std::string("Host"), std::string("192.0.2.9:1234")));
	const testhttp::Reply other = testhttp::request(port, "GET", "/control/xmltv.m3u", other_headers);

	httpd::stop();

	REQUIRE(named.transport_ok);
	REQUIRE(named.code == HTTP_OK);
	REQUIRE(named.body == "http://box.local:9999/control/xmltv.xml");

	REQUIRE(other.transport_ok);
	REQUIRE(other.code == HTTP_OK);
	REQUIRE(other.body == "http://192.0.2.9:1234/control/xmltv.xml");
}

#endif // DISABLE_LEGACY_API
