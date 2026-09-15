//=============================================================================
// yExecuteScript, out of yhttpd_core/helper.h on its own: it is the one
// helper that reaches up into controlapi.h for PLUGIN_DIRS, and putting it in
// helper.h would have made every case that uses any helper pull controlapi.o
// (and, through it, neutrinoapi.o) into its link. This object is pulled only
// when something calls yExecuteScript, which today is controlapi.o alone.
//=============================================================================

#ifndef __httpd_compat_scriptrunner_h__
#define __httpd_compat_scriptrunner_h__

#include <string>

namespace httpd
{
namespace compat
{

//-----------------------------------------------------------------------------
// Script Helpers
//-----------------------------------------------------------------------------
std::string yExecuteScript(std::string cmd);

} // namespace compat
} // namespace httpd

#endif /* __httpd_compat_scriptrunner_h__ */
