//=============================================================================
// See mount.h.
//=============================================================================

#include "httpd/compat/mount.h"

#include "httpd/compat/levels.h"
#include "httpd/compat/query.h"

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <cctype>
#include <cstring>
#include <ctime>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace httpd
{
namespace compat
{

namespace
{

THttp_Method toLegacyMethod(Method m)
{
	switch (m)
	{
		case Get:  return M_GET;
		case Head: return M_HEAD;
		case Post: return M_POST;
		default:   return M_UNKNOWN;
	}
}

/* The one content type the old server takes a body apart for, compared whole
   and not by prefix because that is how it compares it: yrequest.cpp:295 is
   HeaderList["Content-Type"].compare("application/x-www-form-urlencoded"),
   so a request naming a charset beside the type parses to nothing there and
   its handler sees the query alone. Measured against both servers on one box
   before this was written rather than decided here.

   Trimmed first because ParseHeader trims every value it stores
   (yrequest.cpp:201). The header NAME is matched however the library matched
   it on the way in, without regard to case, where the old server keys a plain
   map on the bytes that arrived and would find nothing under "content-type".
   That one difference is left standing on purpose: what it would reproduce is
   that parser's own accident, and the direction it errs in is a handler
   seeing what its caller actually sent. */
bool bodyIsForm(const std::string &content_type)
{
	return trim(content_type) == "application/x-www-form-urlencoded";
}

/* The one other content type HandlePost ever branches on, compared the same
   way it is there: a prefix and not the whole value, because a boundary is
   part of it and never the same twice (yrequest.cpp:277-278,
   Content-Type.compare(0, t.length(), t) against "multipart/form-data;
   boundary="). Trimmed first for the same reason bodyIsForm's own value is:
   ParseHeader trims every header value it stores before either server ever
   compares one.

   The old server takes this one apart too - HandlePostBoundary writes an
   uploaded file to a fixed path and lets the query's own script run against
   it, real state on a real filesystem this layer has never needed a second
   body parser and a second file writer to reproduce, because nothing asked
   it to until now. Both of this prefix's live callers
   (data/y-web/Y_Settings_Backup.yhtm, Y_Tools_Installer.yhtm) carry their
   real arguments in the query string and only the file itself in the part
   this cannot read, so letting such a request reach a dispatcher would run
   that query's own script for real, against whatever an earlier upload left
   behind or against nothing at all - measured on a running box: the exact
   same query answers 200 with an empty body whether or not this ever
   receives a byte of what a caller actually meant to send. answer() below
   refuses a name it cannot take a body apart for outright instead, rather
   than let a handler act on a request this could not finish reading. */
bool bodyIsMultipart(const std::string &content_type)
{
	static const char kPrefix[] = "multipart/form-data; boundary=";
	return trim(content_type).compare(0, sizeof(kPrefix) - 1, kPrefix) == 0;
}

/* The name Execute compares and the parameters a handler reads, out of one
   function because two callers need them: answer() below, and wouldDispatch()
   for the transport asking the same question before a body has arrived. Two
   readings of the same request are two things that can come apart, and what
   they would come apart about here is whether a caller is let through. */
std::string legacyRequest(const std::string &path, const std::string &query,
                          CStringList &UrlData, CStringList &ParamList)
{
	// The router has already split the query off the path; a path with no
	// query and a path with a "?" and nothing after it both arrive here as an
	// empty query string, and query.cpp's own analyzeURL only clears
	// ParamList through parseParams when it sees the "?" itself. Passing the
	// path alone for the first case, rather than reattaching a "?" that was
	// never there, keeps that distinction rather than manufacturing it.
	const std::string raw = query.empty() ? path : (path + "?" + query);
	analyzeURL(raw, UrlData, ParamList);

	// The same name Execute itself compares (controlapi.cpp:216-217),
	// lowercased the same way.
	std::string name = UrlData["filename"];
	for (std::string::size_type i = 0; i < name.size(); ++i)
		name[i] = (char) tolower((unsigned char) name[i]);
	return name;
}

Dispatch g_dispatch = NULL;

// Held by both the increment below and the walk usage() does over the same
// two containers, the same reason server.cpp keeps a lock of its own beside
// the one that starts and stops the daemon: this changes on every request,
// on threads the library itself owns.
OpenThreads::Mutex &usageLock()
{
	static OpenThreads::Mutex m;
	return m;
}

// One counter per name levels.h's table knows, in that table's own order,
// built once on first use rather than at namespace scope so nothing here
// needs to have run before main.
std::vector<unsigned long> &knownUsage()
{
	static std::vector<unsigned long> counts(endpointCount(), 0);
	return counts;
}

// Names outside that table, in the order each was first seen. A vector and
// not a map: kMaxUnknownLegacyNames keeps this from ever growing past a
// couple of hundred entries, short enough that a linear scan of it costs
// nothing next to building an answer, so nothing here has to pay a tree node
// for every distinct name a caller happens to try.
std::vector<Usage> &unknownUsage()
{
	static std::vector<Usage> names;
	return names;
}

void countCall(const std::string &name)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(usageLock());

	for (size_t i = 0; i < endpointCount(); ++i)
	{
		if (name == endpointName(i))
		{
			++knownUsage()[i];
			return;
		}
	}

	std::vector<Usage> &u = unknownUsage();
	for (size_t i = 0; i < u.size(); ++i)
	{
		if (u[i].name == name)
		{
			++u[i].calls;
			return;
		}
	}
	if (u.size() < kMaxUnknownLegacyNames)
	{
		Usage next;
		next.name = name;
		next.calls = 1;
		u.push_back(next);
	}
}

// The ceiling readSentFile checks below is declared further down, outside
// this anonymous namespace: a definition in here would carry this block's
// own internal linkage regardless of what mount.h declares, the same
// anonymous-namespace trap kMaxUnknownLegacyNames' own placement (past this
// namespace's closing brace) already avoids. The extern declaration mount.h
// carries is already in scope for readSentFile below by the time this file
// reaches it, since that header is included above this namespace opens.

// The exact format yhttpd_core/ytypes_globals.h's RFC1123FMT names, copied
// rather than pulled in from that tree: the old server's own BuildHeader
// stamps every timestamp header, Last-Modified included, through this one
// string, and gmtime/strftime taking it apart is the format wire compat
// promises byte identity on, not a header name pulled from a shared constant.
const char kRFC1123Fmt[] = "%a, %d %b %Y %H:%M:%S GMT";

/* What HANDLED_SENDFILE means once a legacy handler has set it: the file at
   hh.NewURL, read whole into the body. This is not the old server's own
   confinement machinery (mod_sendfile.cpp's GetFileName, an access() chain
   against a document root) - that guards a name a caller's own request path
   picked, and every legacy handler that reaches this (controlapi.cpp's
   GetServicesxmlCGI, GetBouquetsxmlCGI, GetUBouquetsxmlCGI, VersionCGI, and
   InfoCGI's own "version" branch - the five call sites that call SendFile at
   all) names a fixed path a macro supplies at compile time (ZAPITDIR,
   IMAGE_VERSION_FILE), never a request parameter. There is
   nothing here for a caller to steer and nothing to check the name against,
   only the one thing worth doing right regardless: open it once and act on
   the descriptor from there, the same discipline static.cpp's own
   openChecked holds to (static.cpp:539) for the opposite reason - a path
   resolved and then reopened by name leaves a window between the two a
   rename can land in, which is what once let a static file reader here
   answer 4945 of 400000 requests from outside its root. Nothing here
   resolves a name and asks the filesystem for it again: one open, one fstat
   on that descriptor, one read loop on that descriptor.

   The old server's own SendFile() (hook.h) never sets a status or a content
   type, so this does not invent one either: a caller sees exactly what
   session_init()'s 200 default and whatever ResponseMimeType already held
   leave standing, which for four of the five names is the empty string
   mod_sendfile's own GetContentType() is never asked to fill in for a path
   that never goes through mod_sendfile at all. A mime type guessed from the
   file's own name would be a plausible answer and not the old server's. */
bool readSentFile(const std::string &path, std::string &body)
{
	const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0)
		return false;

	struct stat st;
	std::memset(&st, 0, sizeof(st));
	bool ok = (::fstat(fd, &st) == 0) && S_ISREG(st.st_mode) && st.st_size >= 0
		&& (unsigned long long) st.st_size <= (unsigned long long) kMaxSendfileBytes;

	if (ok)
	{
		body.clear();
		body.reserve((size_t) st.st_size);
		char buf[16384];
		for (;;)
		{
			const ssize_t got = ::read(fd, buf, sizeof(buf));
			if (got < 0)
			{
				ok = false;
				break;
			}
			if (got == 0)
				break;
			body.append(buf, (size_t) got);
		}
	}

	::close(fd);
	return ok;
}

} // namespace

// A ceiling for the same reason static.cpp's own kMaxServedBytes is one: an
// answer is built whole in memory before any of it goes out, so a name that
// somehow named something enormous would otherwise ask for all of it. None of
// the five call sites readSentFile above actually serves today can name
// anything past a handful of kilobytes, so this is a backstop and not a
// limit anyone is expected to reach. Declared extern in mount.h, so this
// definition already carries external linkage without repeating the keyword
// here, the same way kMaxUnknownLegacyNames below is defined.
const size_t kMaxSendfileBytes = 16u * 1024u * 1024u;

bool handles(const std::string &path)
{
	static const char PREFIX[] = "/control/";
	return path.compare(0, sizeof(PREFIX) - 1, PREFIX) == 0;
}

void installDispatch(Dispatch fn)
{
	g_dispatch = fn;
}

bool wouldDispatch(const std::string &path, const std::string &query,
                   const Credentials &cred, AuthLevel have)
{
	CStringList UrlData;
	CStringList ParamList;
	const std::string name = legacyRequest(path, query, UrlData, ParamList);
	return legacyAllows(name, ParamList, have, cred);
}

Response answer(Method m, const std::string &path, const std::string &query,
                const std::string &body, const std::string &content_type,
                const std::string &host, const Credentials &cred, AuthLevel have)
{
	const THttp_Method legacyMethod = toLegacyMethod(m);

	/* yrequest.cpp answers PUT, DELETE, TRACE, and anything else its own
	   method switch never grew a name for (PATCH, OPTIONS, a caller's typo)
	   with 501 before it reads even the rest of the header line
	   (yrequest.cpp:88-97,127-138): the request line alone decides it, and
	   nothing about the path, the query, or who is asking is ever looked at.
	   toLegacyMethod above never grew a case for any of these either, so
	   M_UNKNOWN here names exactly the same set that source answers this way,
	   and answering it here - before a name is split out of the path, before
	   anything is counted, before a credential is asked - keeps this from
	   being handed to a dispatcher that has no more of an opinion about a
	   method than Execute() itself does (Execute only ever compares against
	   M_HEAD, controlapi.cpp:358).

	   Measured against the running box rather than trusted from the source
	   alone, because the source only says what it computes, not what a
	   caller receives: yrequest.cpp reads a header block ahead of this answer
	   for GET and HEAD alone, so for PUT/DELETE/TRACE the socket still holds
	   every byte the client already sent - Host, User-Agent, the blank line
	   that ends a header block - unread, when SendError()'s own Write() and
	   the connection's own close() run back to back a few lines later. A
	   socket closed with data still sitting in its receive queue gets a
	   reset instead of the graceful shutdown a client is waiting to read an
	   answer out of, so a caller of the real box never sees the 501 that
	   source computes at all: curl reports "Connection reset by peer" and no
	   status, every time this was tried, not once in a while. Reproducing
	   that reset itself, rather than the status it was always supposed to
	   carry, would mean this server sometimes answering nothing at all -
	   which under libmicrohttpd's own persistent HTTP/1.1 connections costs
	   more than one caller waiting on one answer, the same trade already
	   refused once for a Content-Length the old server lies about in the
	   other direction (this file's own embedded-zero comment further down).
	   A clean 501 is the answer that source was always trying to send. */
	if (legacyMethod == M_UNKNOWN)
	{
		Response refusal;
		refusal.code = StatusNotImplemented;
		refusal.content_type = "text/html";
		return refusal;
	}

	CyhookHandler hh;
	hh.Method = legacyMethod;

	/* The one request header the legacy surface reads (see mount.h). The old
	   server hands its handlers every header the request carried
	   (yrequest.cpp:201 into yhook.cpp:198); this hands them the one they
	   read, because a header a handler never looks at is a header this layer
	   would be inventing a policy for. */
	hh.HeaderList["Host"] = host;

	// Read before g_dispatch is asked for rather than after: a caller without
	// the level this name needs must never reach a handler, on this binary or
	// on the real one, and the only way to promise that is to decide it here,
	// ahead of the pointer neither of them can see past.
	const std::string name = legacyRequest(path, query, hh.UrlData, hh.ParamList);

	// Counted here and not after legacyAllows or after g_dispatch returns:
	// what usage() answers for is the demand a name has seen, not whether
	// that demand was ever let through, so a caller with no credential at
	// all still leaves a true count behind rather than a silent zero.
	countCall(name);

	/* The body, into the list the query already went into and after it, which
	   is the container and the order the old server uses: analyzeURL fills
	   ParameterList out of the query while the start line is parsed
	   (yrequest.cpp:118, through ParseStartLine) and HandlePost calls the same
	   ParseParams on the body afterwards without clearing anything
	   (yrequest.cpp:307). Everything that follows is that server's and not a
	   choice made here: a name carried by both is joined with a comma, and the
	   positional numbers the body's pieces are filed under carry on counting
	   from the size the query left behind. A POST with no query numbers its
	   pieces off an empty list, exactly as a GET numbers a query's; a POST
	   that has one goes on counting from where that query stopped. Eleven
	   handlers in controlapi.cpp read a position.

	   POST and no other method. The old server reads a body for no other one
	   (yrequest.cpp:97-101: GET and HEAD read a head and stop, PUT, DELETE and
	   TRACE are answered 501), so a GET carrying one leaves its handler with
	   the query alone here as well.

	   Before legacyAllows and after countCall, because the level a Mixed name
	   needs is read off the parameters a handler will read (levels.h) and one
	   of those parameters may be in here: action=remove reaches TimerCGI from
	   a body just as it does from a query, and a gate that had already decided
	   would have decided on half the request. */
	if (hh.Method == M_POST && bodyIsForm(content_type))
		parseParams(body, hh.ParamList);

	if (!legacyAllows(name, hh.ParamList, have, cred))
	{
		Response refusal;
		refusal.code = StatusForbidden;
		refusal.content_type = "text/plain";
		refusal.body = notPermittedDetail();
		return refusal;
	}

	/* A body this layer cannot take apart at all, refused outright rather
	   than let a handler run on half a request. bodyIsMultipart's own
	   comment has the why; this is only where. Placed after legacyAllows
	   and not before it, unlike the body-parsing branch above: a caller
	   legacyAllows was always going to turn away sees exactly the refusal
	   it always has, name and level untouched by what this cannot read, and
	   this only ever answers for the caller legacyAllows has already let
	   through - the one case where letting the request reach g_dispatch
	   below would run a handler on half of it. */
	if (hh.Method == M_POST && bodyIsMultipart(content_type))
	{
		Response refusal;
		refusal.code = StatusNotImplemented;
		refusal.content_type = "text/html";
		return refusal;
	}

	if (g_dispatch != NULL)
		g_dispatch(hh);
	else
		hh.SetError(HTTP_NOT_IMPLEMENTED, HANDLED_NOT_IMPLEMENTED);

	// The old server's session_init() (yhttpd_core/yhook.cpp) sets this
	// default before any hook or handler runs, so a handler that answers
	// without ever calling SetHeader/outStart/SendRedirect/SetError still
	// gets 200 there. hh starts at HTTP_NIL instead (hook.cpp's own
	// constructor, same starting value as the old one's), and nothing
	// between that and here carried the old server's per-request default
	// forward: a handler with such a path leaves httpStatus at HTTP_NIL,
	// which reads as no status at all to whatever answers this call.
	// Twenty of the 74 legacy names are registered with an empty mime type
	// ("decide in function") and can return down such a path; matching the
	// old server's default here, once, is what makes all of them agree with
	// it, instead of a status fixed inside each one.
	if (hh.httpStatus == HTTP_NIL)
		hh.httpStatus = HTTP_OK;

	Response r;
	r.code = (int) hh.httpStatus;
	r.content_type = hh.ResponseMimeType;
	// HANDLED_SENDFILE means the handler's own answer is not in yresult at
	// all, it is the file named in NewURL (see readSentFile above); reading
	// hh.result() here instead would answer with whatever the handler
	// happened to leave in yresult before calling SendFile, which for every
	// one of the five call sites this serves is nothing at all. A read that
	// fails leaves the body empty, which is what the old server answers too:
	// yresponse.cpp's own Sendfile() already wrote the 200 header before
	// trying to open the file, so a name that vanished between the two
	// leaves that server with a claimed 200 and nothing behind it as well.
	if (hh.status == HANDLED_SENDFILE)
		readSentFile(hh.NewURL, r.body);
	else
	{
		/* Where the old server's body ends: it hands yresult to the socket as
		   a C string (yresponse.cpp:191, Write(text.c_str()) and strlen on
		   it), so an answer carrying an embedded zero byte goes out only as
		   far as that byte. encodeString() appends exactly such a byte to
		   every value it returns (compat/helper.cpp:236, verbatim from the
		   original), which is how getbouquets?encode=true and epg?bouquetnr=
		   get one into the middle of theirs. Answering with the whole string
		   here would answer with bytes no caller of the old server has ever
		   been sent - measured on this box, the old server announces 101
		   bytes for getbouquets?encode=true and sends 32. */
		r.body.assign(hh.result().c_str());

		/* CyhookHandler::BuildHeader (yhook.cpp) writes this on every answer
		   whose cache argument is false and whose CacheCategory is empty, and
		   none of the 74 legacy names ever sets CacheCategory (that only
		   happens in mod_yparser.cpp, which nothing under /control/ runs
		   through) or passes a true cache argument (that is yresponse.cpp's
		   own SendResponse, for the HANDLED_SENDFILE branch above, not this
		   one) - so this is not a guess at what the old server might send,
		   it is what every one of its non-SendFile answers sends, measured
		   directly against it: getmode, an unknown name that falls through to
		   its own 404, and a HEAD all carry it. Read literally rather than
		   through addApiHeaders (status.cpp), which writes "no-store" for a
		   reason that has nothing to do with this surface (see the long
		   comment above this branch's caller in server.cpp) and would answer
		   with a value the old server has never sent. */
		r.headers.push_back(std::make_pair(std::string("Cache-Control"),
		                                   std::string("no-cache")));

		/* Same function, same branch, one condition further down
		   (yhook.cpp:322-333): the old server writes this whenever the status
		   is none of HTTP_NOT_MODIFIED/HTTP_NOT_FOUND/
		   HTTP_REQUEST_RANGE_NOT_SATISFIABLE and GetContentLength() is
		   positive, which for this non-SendFile branch is yresult's own
		   length - a body written but not yet truncated at an embedded zero
		   byte, the same quantity the Cache-Control comment above already
		   reads GetContentLength() through.

		   mod_time follows the same two-source split BuildHeader reads:
		   time(NULL) unless LastModified carries something other than its
		   -1 sentinel. Checked directly against controlapi.cpp rather than
		   assumed: none of its 74 registered handlers ever assigns
		   hh->LastModified themselves, and the one place in the old tree
		   that does (mod_sendfile.cpp's Hook_PrepareResponse, stamping a
		   real file's st_mtime) is a hook for the document-root file server,
		   never installed for anything under /control/ - the five
		   SendFile() call sites this mount point does serve reach it
		   through the trivial inline setter (hook.h), which only flips
		   NewURL and status, and land in the branch above, never here. So
		   on the whole reachable surface this function answers for, the
		   sentinel never actually flips: every dynamic answer here gets the
		   moment it answered, exactly what the old server's own
		   Prepare()-set default sends for these same names, and the field
		   is still read rather than skipped so a future handler that did
		   stamp a real time into it would not silently stop working. */
		if (hh.httpStatus != HTTP_NOT_MODIFIED
		    && hh.httpStatus != HTTP_NOT_FOUND
		    && hh.httpStatus != HTTP_REQUEST_RANGE_NOT_SATISFIABLE
		    && hh.GetContentLength() > 0)
		{
			time_t mod_time = time(NULL);
			if (hh.LastModified != (time_t) -1)
				mod_time = hh.LastModified;

			char timeStr[80];
			strftime(timeStr, sizeof(timeStr), kRFC1123Fmt, gmtime(&mod_time));
			r.headers.push_back(std::make_pair(std::string("Last-Modified"),
			                                   std::string(timeStr)));
		}
	}
	return r;
}

// 200: generous next to the 74 names this table actually holds, and still a
// hard ceiling on what asking for a name nobody registered can cost this
// program, at no charge to whoever asks.
const std::size_t kMaxUnknownLegacyNames = 200;

std::vector<Usage> usage()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(usageLock());

	std::vector<Usage> out;
	out.reserve(endpointCount() + unknownUsage().size());
	for (size_t i = 0; i < endpointCount(); ++i)
	{
		Usage row;
		row.name = endpointName(i);
		row.calls = knownUsage()[i];
		out.push_back(row);
	}
	const std::vector<Usage> &u = unknownUsage();
	out.insert(out.end(), u.begin(), u.end());
	return out;
}

} // namespace compat
} // namespace httpd
