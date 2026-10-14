//=============================================================================
// What the legacy calls read out of a settings file and write into the box
// settings, kept apart from controlapi.cpp so a case can drive it.
//=============================================================================

#ifndef __httpd_compat_settingsfile_h__
#define __httpd_compat_settingsfile_h__

#include "httpd/compat/helper.h"

#include "coreapi/settings/settings.h"

namespace httpd
{
namespace compat
{

// Whether /control/config leaves this entry of the named file out. For
// "neutrino" the declaration decides, and the name only for an entry nothing
// declares; for the other files the name alone.
bool legacyConfigWithheld(const std::string &config, const std::string &key);

// The entries of a loaded file that /control/config hands out.
CStringList legacyConfigReadable(const std::string &config, const CStringList &all);

// The webtv, webradio and xmltv parameters of /control/xmltvlist, written as
// the three list rows in one batch. A missing or empty parameter empties its list.
void legacyWriteWebLists(const CStringList &params, coreapi::settings::Refusals &failed);

} // namespace compat
} // namespace httpd

#endif /* __httpd_compat_settingsfile_h__ */
