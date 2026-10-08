/*
 * fakeupdate.h - the update group's seam, recording what it was told
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

#ifndef __support_fakeupdate_h__
#define __support_fakeupdate_h__

#include "coreapi/box/apply_update.h"

#include <system/settings.h>

#include <string>
#include <vector>

extern SNeutrinoSettings g_settings;

#if ENABLE_PKG_MANAGEMENT
inline int packagesSetting() { return g_settings.softupdate_autocheck_packages; }
#else
inline int packagesSetting() { return 0; }
#endif

// Every call in order, as "flash:on", "flash:off", "packages:on" and "packages:off".
struct FakeUpdateCheck : public coreapi::UpdateCheck
{
	std::vector<std::string> calls;

	coreapi::Status setFlashCheck(bool on)
	{
		calls.push_back(on ? "flash:on" : "flash:off");
		return coreapi::Status::Ok;
	}
	// The setting as it stood at the last switching on, which fixes the hours between checks.
	int hours_at_on;
	FakeUpdateCheck() : hours_at_on(-1) {}

	coreapi::Status setPackageCheck(bool on)
	{
		calls.push_back(on ? "packages:on" : "packages:off");
		if (on)
			hours_at_on = packagesSetting();
		return coreapi::Status::Ok;
	}
};

#endif
