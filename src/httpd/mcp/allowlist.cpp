/*
 * allowlist.cpp - what an AI client may start and may change
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

#include "httpd/mcp/allowlist.h"

#include "httpd/json.h"

#include "coreapi/settings/settings.h"

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace httpd
{
namespace mcp
{

namespace
{

OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

Allowlists &held()
{
	static Allowlists a;
	return a;
}

bool listed(const std::vector<std::string> &list, const std::string &name)
{
	for (size_t i = 0; i < list.size(); ++i)
	{
		if (list[i] == name)
			return true;
	}
	return false;
}

/* Keys no AI client may write whatever section the owner ticks. The flag that makes
   the box answer a module's pin enquiry from the pin it saved guards a credential
   as the pin does: set from outside it, the box unlocks the module without anyone
   asking, and the section it is in is one the owner may well have opened. */
bool guardsCredential(const std::string &key)
{
	return key.compare(0, 16, "ci_save_pincode_") == 0;
}

/* The flag files that switch a program on at boot: the services, which a client could use to
   export the box's disks or to close its login, and the softcams, which hold the keys that
   descramble, and the one that moves the screen corners for a scart output. Named for good,
   whatever the owner ticks and whatever the rows come to do. */
bool switchesAProgram(const std::string &key)
{
	return key.compare(0, 12, "flag_daemon_") == 0 || key.compare(0, 10, "flag_camd_") == 0 ||
	       key == "flag_scart_osd_fix";
}

// Never writable by an AI client, whatever the owner ticks.
const char *const kDeniedByName[] = { "network", "parental", "update" };

} // namespace

Allowlists currentAllowlists()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> h(lock());
	return held();
}

void installAllowlists(const Allowlists &a)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> h(lock());
	held() = a;
}

std::string sectionDenial(const std::string &section)
{
	for (size_t i = 0; i < sizeof(kDeniedByName) / sizeof(kDeniedByName[0]); ++i)
	{
		if (section == kDeniedByName[i])
			return kDeniedByName[i];
	}
	return coreapi::settings::sectionHoldsSecret(section) ? "secret" : std::string();
}

bool pluginAllowed(const std::string &name)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> h(lock());
	return listed(held().plugins, name);
}

bool sectionAllowed(const std::string &section)
{
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> h(lock());
		if (!listed(held().sections, section))
			return false;
	}
	return sectionDenial(section).empty();
}

// Why a key is denied, the class it is in; NULL for one that is not.
static const char *denialOf(const std::string &key)
{
	if (guardsCredential(key))
		return "is a module's PIN in effect: it lets the box answer the CI module's PIN enquiry by itself";
	if (switchesAProgram(key))
		return "switches a service or a softcam on at boot, or moves the screen of a scart output";
	coreapi::Result<coreapi::Descriptor> d = coreapi::settings::describe(key);
	if (!d.ok())
		return NULL;
	if (d.value().secret)
		return "is a credential";
	if (coreapi::settings::holdsPath(d.value()))
		return "names a place on the box's disk";
	return NULL;
}

std::string deniedKeyIn(const std::string &settings_json, std::string *why)
{
	std::vector<JsonMember> members;
	if (!readFlatObject(settings_json, members))
		return std::string();
	for (size_t i = 0; i < members.size(); ++i)
	{
		const char *said = denialOf(members[i].name);
		if (said == NULL)
			continue;
		if (why != NULL)
			*why = said;
		return members[i].name;
	}
	return std::string();
}

} // namespace mcp
} // namespace httpd
