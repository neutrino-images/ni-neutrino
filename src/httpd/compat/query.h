//=============================================================================
// The old server's query parser, taken out of CWebserverRequest so the bytes
// it produces can be held to on their own. Copied out of
// yhttpd_core/yrequest.cpp:155-178 (parseParams, there ParseParams) and
// :214-243 (analyzeURL) rather than rewritten: every consumer in
// tuxboxapi/controlapi.cpp reads what these two functions leave behind, and
// three things they do are not what a reader would guess. See query.cpp.
//=============================================================================

#ifndef __httpd_compat_query_h__
#define __httpd_compat_query_h__

#include "httpd/compat/helper.h"

namespace httpd
{
namespace compat
{

// Splits param_string on "&" and each piece on "=", filling ParamList. A
// piece with no "=" is stored only under its position, not under its own
// text; a repeated name is joined with a comma; and the position written
// after each piece counts the whole list, not where in the query the piece
// stood. See query.cpp for what depends on each of those.
bool parseParams(std::string param_string, CStringList &ParamList);

// True only for a key parseParams writes purely to record a token's position
// ("1", "2", ...), never for one a caller named itself: a submitted field
// name is never made only of digits. ConfigCGI's submit path
// (controlapi.cpp) uses this to leave such an echo out of the config file it
// writes, the same way it already leaves out "config", "action" and the rest
// of the names it recognises by hand.
bool isPositionKey(const std::string &key);

// Splits url on the first "?" before decoding either half, then fills UrlData
// with fullurl, url, path, filename, filenamepure and fileext, and ParamList
// through parseParams. Decoding before splitting would cut a query value's
// own encoded "?" as if it were the one separating the query from the path.
void analyzeURL(const std::string &url, CStringList &UrlData, CStringList &ParamList);

} // namespace compat
} // namespace httpd

#endif /* __httpd_compat_query_h__ */
