//=============================================================================
// See legacybridge.h.
//=============================================================================

#include "httpd/compat/legacybridge.h"

#include "httpd/compat/controlapi.h"
#include "httpd/compat/legacyconfig.h"
#include "httpd/compat/neutrinoapi.h"

#include <global.h>

namespace httpd
{
namespace compat
{

void realDispatch(CyhookHandler &hh)
{
	// Function-local, not namespace-level: constructed on the first request
	// that actually reaches this function, which is never, on a host with no
	// CNeutrinoAPI behind it. A namespace-level instance would need a
	// constructor to run before main and would run it in every program that
	// links this object, including one that has nothing for it to talk to.
	static CNeutrinoAPI neutrinoAPI;
	static CControlAPI api(&neutrinoAPI);

	// CControlAPI::init() (compat/controlapi.cpp) reads
	// WebserverConfigList["WebsiteMain.override_directory"] and
	// ["WebsiteMain.directory"] on the one call that still finds
	// PLUGIN_DIRS[0] empty, and hh here is a fresh CyhookHandler that never
	// carried either key before now. Read once and kept, not once per
	// request, for the same reason init() itself only does its own
	// computation once: the file answers the same way for the life of the
	// program. NI_WEB_OLD_SETTINGS_FILE is the removed server's own
	// configuration file, the same one neutrino.cpp reads for the credential
	// migration. A box upgraded from an older image still has it and is read
	// out of it, so an owner who moved those directories keeps them; a box
	// that never had it falls back to the two directories this image installs
	// rather than to "", which is what legacyconfig.cpp exists to refuse.
	static const CStringList config = legacyWebsiteMainConfig(NI_WEB_OLD_SETTINGS_FILE);

	hh.WebserverConfigList = config;

	api.init(&hh);
	api.Execute(&hh);
}

} // namespace compat
} // namespace httpd
