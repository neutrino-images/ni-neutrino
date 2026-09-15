//=============================================================================
// See legacyconfig.h.
//=============================================================================

#include <config.h>

#include "httpd/compat/legacyconfig.h"

#include <configfile.h>

namespace httpd
{
namespace compat
{

namespace
{

std::string readOrFallback(CConfigFile &old_conf, const char *key, const char *fallback)
{
	const std::string value = old_conf.getString(key, fallback);
	if (value.empty())
		return fallback;
	return value;
}

} // namespace

CStringList legacyWebsiteMainConfig(const char *conf_path)
{
	CConfigFile old_conf('=', false);
	// The return value only says whether the file was there to read; a
	// missing or unparsable file leaves old_conf empty, and getString's own
	// default below answers for that exactly like a present-but-blank key
	// does. Nothing here has to branch on it separately.
	old_conf.loadConfig(conf_path);

	CStringList config;
	config["WebsiteMain.override_directory"] = readOrFallback(old_conf, "WebsiteMain.override_directory", PUBLIC_HTTPDDIR);
	config["WebsiteMain.directory"]           = readOrFallback(old_conf, "WebsiteMain.directory", PRIVATE_HTTPDDIR);
	return config;
}

} // namespace compat
} // namespace httpd
