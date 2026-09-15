//=============================================================================
// The access levels the legacy /control/ surface answers under.
//
// Read by mount.cpp before a name reaches CControlAPI::Execute, so a caller
// without the level a name needs never enters a handler that reaches
// NeutrinoAPI, CEitManager, CServiceManager, CZapit or CNeutrinoApp - none of
// which this suite's own host binary links (see mount.h).
//=============================================================================

#ifndef __httpd_compat_levels_h__
#define __httpd_compat_levels_h__

#include "httpd/auth.h"
#include "httpd/compat/helper.h"
#include "httpd/endpoint.h"

#include <cstddef>
#include <string>

namespace httpd
{
namespace compat
{

/* How many names this table carries, and the name at index. Read the table
   itself rather than kept anywhere else: a name added to the legacy surface
   without a level assigned here is a name the case that walks these two
   fails on, and a name copied into some other list instead of read through
   these could drift from what this table actually holds. */
size_t endpointCount();
const char *endpointName(size_t index);

// Whether name carries an assigned level. False for anything this table
// does not carry, an unknown name included.
bool hasLevel(const std::string &name);

/* The level a call to name needs. name is the lowercase form Execute itself
   compares (controlapi.cpp:216-217).

   Most names answer the same level whatever parameters they carry. A handful
   read them the same way their own handler does and answer differently for it
   - standby without a parameter reports, standby?on switches - and levelFor
   reproduces exactly that reading ahead of a handler no host build of this
   table can call.

   WHICH IS WHY THE FIRST FORM TAKES THE PARSED LIST AND NOT THE QUERY STRING.
   A handler reads ParamList, and since POST bodies reach one (mount.cpp) that
   list is the query and the body together: a caller may put action=remove in
   the body of a POST to /control/timer and leave the query empty. Deciding
   from the query alone there would hand a Read caller a handler that removes
   a timer. So the caller that has the list a handler will read passes that
   list, and the two cannot come apart.

   The second form is for a caller that has only a query - every case in this
   tree, and the levels of a GET - and builds the same list the first form
   would have been given for it, through the same parser (query.cpp), so a
   request with no body decides identically whichever form is asked.

   A name this table does not carry answers System, which is the reading
   that refuses a call rather than the one that would assume a level for a
   name nobody classified. */
AuthLevel levelFor(const std::string &name, const CStringList &params);
AuthLevel levelFor(const std::string &name, const std::string &query);

/* Whether the legacy surface lets this caller make this call. params and
   query mean what they mean for levelFor above.

   have is what the general gate (httpd::granted) already resolved for this
   request: a bearer token's own level, or Read for the address ranges 6.6
   seeds for everyone else. Never recomputed here, because neither of those
   answers can have moved between that gate asking and this one asking again.

   A LIVE BROWSER SESSION IS THE ONE THING have MAY CARRY THAT THIS SURFACE
   DOES NOT HONOUR, and it is taken back off here. httpd::granted answers
   System for one, which is right for the routes under /api/: those are
   POSTs, and a POST arriving on a cookie alone is refused for want of the
   second token (auth.cpp, csrfSatisfied). Every legacy route that changes
   something is a GET, no safe method is ever asked for that token, and the
   cookie is written Path=/ SameSite=Lax, which a browser attaches to a
   top-level navigation another site started. A link on any page would
   otherwise be /control/shutdown in the name of whoever is logged in. The
   old server had no session at all (mod_auth.cpp:40, authenticate=false), so
   nothing that ever worked here worked through one; what the caller is left
   with is what the same request would have been granted without the cookie,
   and never more than have already was.

   c is read for the two things that gate does not decide. Its peer is
   compared against loopback on its own terms (webconfig.h's
   isLoopbackPeer), because have's own Read for the seeded ranges does not
   say which of them a caller is in, and loopback answers for every level
   here where the rest of those ranges answer only for Read - the old
   server let loopback through unconditionally too (mod_auth.cpp:19), and
   the plugins that call in over it are what still depends on that. Its basic is checked against the one account
   this box has, which is what a caller carries when it wants more than
   Read and holds neither a bearer token nor a session; httpd::granted
   never reads that field itself, so this is the only place Basic ever
   answers for anything. */
bool legacyAllows(const std::string &name, const CStringList &params,
                   AuthLevel have, const Credentials &c);
bool legacyAllows(const std::string &name, const std::string &query,
                   AuthLevel have, const Credentials &c);

} // namespace compat
} // namespace httpd

#endif /* __httpd_compat_levels_h__ */
