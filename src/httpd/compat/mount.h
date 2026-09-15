//=============================================================================
// Where /control/ enters the new server. Everything under that prefix is
// answered by the legacy dispatcher (CControlAPI::Execute) rather than by a
// route table, because the legacy surface carries arbitrary and positional
// parameter names that a declared route would refuse.
//
// answer() does not call CControlAPI::Execute itself. That call lives in
// legacybridge.cpp, reached only through the function pointer below, because
// CControlAPI::Execute dispatches through a table of seventy-four member
// functions and the linker resolves an archive member whole: asking for
// Execute at all pulls in every one of those seventy-four bodies, and they
// reach real channel, EPG, zapit and GUI singletons this program's own host
// test binary never links. Nothing here calls a handler on the host, so
// answer() has to stay able to answer without Execute ever being asked for,
// and the pointer is how a caller that does have those singletons plugs the
// real dispatcher in without moving that requirement onto every caller.
//=============================================================================

#ifndef __httpd_compat_mount_h__
#define __httpd_compat_mount_h__

#include "httpd/auth.h"
#include "httpd/compat/hook.h"
#include "httpd/endpoint.h"
#include "httpd/http.h"

#include <cstddef>
#include <string>
#include <vector>

namespace httpd
{
namespace compat
{

// True for any path this layer answers. Checked before the router's own
// tables are asked, so a caller under /control/ never falls through to the
// file server the way it did before this layer existed.
bool handles(const std::string &path);

/* Runs the legacy dispatcher for a path handles() has already accepted, once
   cred and have clear the levels this prefix's own table requires (levels.h,
   httpd::compat::legacyAllows). A caller this gate refuses never reaches
   g_dispatch at all, on this binary or on the real one.

   cred is the same Credentials the general gate already read off the
   connection, basic included: server.cpp is the only caller, and it is what
   turns a live Authorization header into cred.basic before this is ever
   asked. A caller building one by hand (a case, chiefly) leaves basic empty
   unless it sets it, which is a caller presenting nothing.

   host is the request's own Host header, which three legacy handlers write
   into the urls they answer with (controlapi.cpp's xmltvm3uCGI,
   build_live_url and build_playlist): those urls name the box a player is to
   fetch the stream from, and a handler that is handed nothing there answers
   "http://:31339/..." to a caller that has no way of knowing what was meant.
   It is a parameter rather than a default, because a caller that forgets it
   is the one thing this cannot afford to look like a caller that has none.

   body and content_type are the request's own, and are read for exactly one
   thing: the old server takes a POST body apart into the same ParamList the
   query went into and hands the result to the same handlers, for the one
   content type it recognizes as a form (yrequest.cpp:295-307). Without them a
   form posted to this prefix reaches its handler with an empty parameter list
   and is answered 200 with whatever that handler does about no parameters -
   which for xmltvlist is emptying three of the box's own lists and answering
   "ok". Both empty for a request that carries no body, which is the same
   nothing a GET leaves.

   Two other things content_type can say are answered before any of that:
   a POST naming the one other body the old server takes apart, a multipart
   upload, is refused outright rather than parsed (mount.cpp's own
   bodyIsMultipart has the why - the short form is that nobody here can
   finish reading it, and letting a handler run on the rest of such a request
   would run a real script for real on a file that never arrived). And m
   itself, ahead of everything above: a method neither GET, HEAD nor POST is
   refused before path or query are even looked at, the same point
   yrequest.cpp itself refuses PUT, DELETE and TRACE. */
Response answer(Method m, const std::string &path, const std::string &query,
                 const std::string &body, const std::string &content_type,
                 const std::string &host, const Credentials &cred, AuthLevel have);

/* Whether a request under this prefix is going to reach a legacy handler for
   this caller, decided from the head alone.

   Asked by the transport before it accepts a body, for the one thing it has
   to decide there: whether the bytes are worth keeping (server.cpp). A body
   kept for a caller this surface is going to refuse is a megabyte a stranger
   with no credential can make this box hold per connection, and a body
   dropped for a caller it is going to answer is the very bug the parameter
   above exists to fix.

   Decided through the same name and the same gate answer() itself asks, so
   that for a request with no body - which is what every request looks like
   at the moment this is asked - the two cannot answer differently. A body
   that then arrives can only add parameters, and both ways that can move the
   answer are safe: a body kept for a request answer() then refuses costs
   only memory the ceiling already bounds, and a body dropped leaves answer()
   deciding on exactly the head this decided on, so it refuses too rather
   than running a handler on parameters that were thrown away. */
bool wouldDispatch(const std::string &path, const std::string &query,
                    const Credentials &cred, AuthLevel have);

// One name /control/ has been asked for since this program started, and how
// many times: a name the level table knows, or one it does not (see usage()
// below for what tells the two apart in the list it returns).
struct Usage
{
	std::string   name;
	unsigned long calls;
};

/* Every name this layer has counted a call for: levels.h's own
   endpointCount() names, in that table's order, each carried even at a count
   of zero, followed by every name outside that table this layer has kept a
   count for, in the order each was first seen, up to
   kMaxUnknownLegacyNames of them.

   Counted where a name enters this mount point, inside answer() after a
   request's method has already cleared the one gate answer() checks
   first, but still before legacyAllows is asked: GET, HEAD and POST are
   the only methods a name is ever split out of at all, so counting can
   only happen once one of those three is already known, not where a
   handler leaves. What this is for is the demand a name has seen from a
   caller answer() was going to look a name up for in the first place,
   whether or not that demand was ever let through, so a GET or POST from
   a stranger with no credential still shows a true count rather than a
   silent zero, and a HEAD counts exactly like a GET for the same name. A
   PUT, DELETE, TRACE or any other method answer() refuses outright never
   reaches a name to count, the same way a path handles() never accepted
   does; that caller sees the refusal itself rather than a silent count,
   so this still never understates what a credentialed GET, HEAD or POST
   caller has actually asked for. Read by /api/v1/system/legacy-usage
   (ep_system.cpp) and by nothing else that ships. */
std::vector<Usage> usage();

/* The most distinct names outside the known table this layer will ever hold
   a count for at once; the entry that would be the next one past it is
   simply never added. A name already being counted, known or not, is never
   turned away by this, since it never needs a new entry to keep being
   counted - only a name nobody has asked for yet does, and asking for one
   costs a caller no credential at all, which is the whole reason this has a
   ceiling rather than growing with whatever a caller can be bothered to
   type into a path. */
extern const std::size_t kMaxUnknownLegacyNames;

/* The most bytes HANDLED_SENDFILE will ever read into memory for one answer:
   a size a real caller can name, checked against the file's own fstat before
   any of it is read. None of the five call sites this actually serves can
   name anything past a handful of kilobytes, so this is a backstop and not a
   limit anyone is expected to reach. extern rather than local to mount.cpp
   so a test can mutate the file this ceiling actually gates and still see
   its own recompiled value, the same reason kMaxUnknownLegacyNames above is
   extern and not a copy kept beside it. */
extern const std::size_t kMaxSendfileBytes;

// What installDispatch is given: a fully populated legacy request, answered
// in place on the same object (status, httpStatus, ResponseMimeType and
// yresult are all written there, the same way Execute always has).
typedef void (*Dispatch)(CyhookHandler &hh);

// Plugs the real dispatcher in. Called once, from whatever brings this
// server up in a program that actually has a CNeutrinoAPI behind it; never
// called by this suite's own host binary, which is the point. A plain
// function pointer and not std::function, for the same reason the router's
// own handler type is one: nothing here may need a constructor to run before
// main just to hold this.
void installDispatch(Dispatch fn);

} // namespace compat
} // namespace httpd

#endif /* __httpd_compat_mount_h__ */
