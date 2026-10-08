/*
 * apply_lang.h - what makes a changed language or time zone setting take effect
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __coreapi_apply_lang_h__
#define __coreapi_apply_lang_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

#include <map>
#include <string>
#include <vector>

namespace coreapi
{

/* What the language groups tell the program. One call per thing, so the groups
   decide what is sent and when and this only carries it. Nothing here asks
   anybody anything.

   A seam rather than the locale catalog, the plugin list, the system clock and
   the guide themselves, so the groups can run where none of them exists. */
struct Localization
{
	virtual ~Localization() {}

	/* The catalog of the named language, and the plugin list read again since
	   what a plugin is called depends on the language. NotFound for a language
	   no catalog is installed for, which leaves the catalog in use as it was. */
	virtual Status loadLanguage(const std::string &name) = 0;
	// Links the system clock's zone to the one the setting names.
	virtual Status linkTimezone() = 0;
	/* The languages the guide shows first, by the names the settings use for
	   them; the entry "none" and an empty one name no language. */
	virtual Status setGuideLanguages(const std::vector<std::string> &names) = 0;
};

/* NotSupported for every call while nothing is installed, so a group run before
   the program exists fails and says so instead of touching nothing. */
Localization &localization();
void setLocalization(Localization *l);

// Binds the accessor above to the program's catalog, clock and guide.
void installRealLocalization();

/* The program loads the language itself at startup, before it can tell whether
   the box has one at all, and falls back when it has none. Tells the group which
   language that left in use, so its first run does not load it a second time
   under the menus that already hold its texts. */
void noteLanguageLoaded(const std::string &name);

/* The guide keeps its own list of languages in a file and the three settings are
   never empty, so sending them at startup would rewrite that file at every start.
   Tells the group what the settings held as they were loaded, so its first run
   sends them only if something wrote them since, a web write before the guide is
   up. Without it the first run sends. */
void noteGuideLanguagesLoaded(const std::vector<std::string> &names);

// Forgets what was sent and what was noted, for a case that needs the first run again.
void resetSentLanguage();

/* Defined by the application, because the plugin list and the zone notifier
   belong to objects whose headers reach the GUI and this layer must not: the
   first loads the catalog of the named language and reads the plugin list
   again, NotFound for a language no catalog is installed for; the second links
   the system clock to the zone the setting names. */
Status applicationLoadLanguage(const std::string &name);
Status applicationLinkTimezone();

/* The three letter codes the guide knows the named languages by, for each name every
   code the map carries it under, in the order of the names. The entry "none" and an
   empty one name no language. */
std::vector<std::string> guideLanguageCodes(const std::vector<std::string> &names,
					    const std::map<std::string, std::string> &codes);

// The language catalog and plugin list.
extern const ApplyGroup kLanguageApplyGroup;

// The system clock's zone.
extern const ApplyGroup kTimezoneApplyGroup;

// The languages the guide prefers. The guide is up from the sectionsd phase.
extern const ApplyGroup kGuideLanguageApplyGroup;

} // namespace coreapi

#endif
