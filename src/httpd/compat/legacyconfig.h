//=============================================================================
// The two WebserverConfigList keys CControlAPI::init() (compat/controlapi.h)
// reads, sourced from the old server's own configuration file rather than
// left empty. See legacyconfig.cpp for why an empty answer is never allowed.
//=============================================================================

#ifndef __httpd_compat_legacyconfig_h__
#define __httpd_compat_legacyconfig_h__

#include "httpd/compat/helper.h"

namespace httpd
{
namespace compat
{

// Reads "WebsiteMain.override_directory" and "WebsiteMain.directory" out of
// conf_path (the removed server's own nhttpd.conf, which a box upgraded from
// an older image still has; a path a case wrote itself in a test), the same
// two keys CControlAPI::init() looks up in WebserverConfigList. A key absent
// from the file and a key present but written empty both come back as the
// directory this image installs under that name, never as "": init() turns ""
// into a PLUGIN_DIRS entry, and yExecuteScript (compat/scriptrunner.cpp) then
// searches "" + "/" + script, which is the root of the filesystem, for
// whatever name a caller asked for.
//
// The two installed directories rather than a name that cannot exist, which is
// what stood here while the removed server still shipped that file: nothing
// installs it any more, so on every box that was not upgraded into this image
// both keys now take the fallback, and a fallback that cannot exist would take
// /control/exec's script directory with it. The values are the ones that file
// shipped with, so a box that has it and a box that does not look for scripts
// in the same place.
CStringList legacyWebsiteMainConfig(const char *conf_path);

} // namespace compat
} // namespace httpd

#endif /* __httpd_compat_legacyconfig_h__ */
