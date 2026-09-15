//=============================================================================
// The one function that actually calls CControlAPI::Execute, kept apart from
// mount.cpp so that referencing it is optional. See mount.h for why.
//=============================================================================

#ifndef __httpd_compat_legacybridge_h__
#define __httpd_compat_legacybridge_h__

#include "httpd/compat/hook.h"

namespace httpd
{
namespace compat
{

// Runs the real legacy dispatcher against an already-populated request.
// Matches the Dispatch type in mount.h; install it there rather than calling
// it directly, so a program with no working CNeutrinoAPI behind it never has
// to name this function to build.
void realDispatch(CyhookHandler &hh);

} // namespace compat
} // namespace httpd

#endif /* __httpd_compat_legacybridge_h__ */
