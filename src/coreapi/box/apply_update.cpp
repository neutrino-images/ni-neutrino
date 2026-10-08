/*
 * apply_update.cpp - what makes a changed automatic update check take effect
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

#include <config.h>

#include "coreapi/box/apply_update.h"
#include "coreapi/box/sentstate.h"

#include <system/settings.h>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

class NoUpdateCheck : public UpdateCheck
{
	public:
		Status setFlashCheck(bool) { return Status::NotSupported; }
		Status setPackageCheck(bool) { return Status::NotSupported; }
};

NoUpdateCheck g_no_update_check;
UpdateCheck *g_update_check = 0;

/* The package check is restarted on every change of its setting, which also chooses
   the hours between checks, so a run that finds the setting as it was does nothing. */
Sent<int> g_flash;
Sent<int> g_packages;
bool g_started = false;

Status runUpdate()
{
	if (!g_started)
		return Status::Ok;

	UpdateCheck &check = updateCheck();
	Status first = Status::Ok;
	SentFlags none;

	const int flash = g_settings.softupdate_autocheck ? 1 : 0;
	sendChanged(first, none, 0, g_flash, flash, [&]() { return check.setFlashCheck(flash != 0); });
#if ENABLE_PKG_MANAGEMENT
	const int packages = g_settings.softupdate_autocheck_packages;
	sendChanged(first, none, 0, g_packages, packages, [&]() { return check.setPackageCheck(packages != 0); });
#endif
	return first;
}

const char *const kUpdateKeys[] =
{
	"softupdate_autocheck",
	"softupdate_autocheck_packages"
};

} // namespace

UpdateCheck &updateCheck()
{
	if (!g_update_check)
		return g_no_update_check;
	return *g_update_check;
}

void setUpdateCheck(UpdateCheck *c) { g_update_check = c; }

void startUpdateChecks()
{
	/* Nothing runs yet, so a check that is off needs no call: switching off would build
	   the package check with the interval of a setting that is not on, and the image check
	   would remove the flag file the boot left alone. */
	g_flash.known = true;
	g_flash.value = 0;
	g_packages.known = true;
	g_packages.value = 0;
	g_started = true;
	runUpdate();
}

void resetSentUpdateChecks()
{
	g_flash = Sent<int>();
	g_packages = Sent<int>();
	g_started = false;
}

const ApplyGroup kUpdateApplyGroup = { "update", ApplyPhase::Network, COREAPI_KEYS(kUpdateKeys), &runUpdate };

} // namespace coreapi
