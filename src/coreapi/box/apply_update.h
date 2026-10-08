/*
 * apply_update.h - what makes a changed automatic update check take effect
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

#ifndef __coreapi_apply_update_h__
#define __coreapi_apply_update_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

namespace coreapi
{

/* The two background checks for a newer image and for newer packages. */
struct UpdateCheck
{
	virtual ~UpdateCheck() {}

	virtual Status setFlashCheck(bool on) = 0;
	virtual Status setPackageCheck(bool on) = 0;
};

// NotSupported for every call while nothing is installed.
UpdateCheck &updateCheck();
void setUpdateCheck(UpdateCheck *c);

// Binds the accessor above to the program's checks.
void installRealUpdateCheck();

/* The checks start once the program has finished starting, because they reach out
   over the network and did so last of all. Until the program calls this, the group
   notes nothing and does nothing, and this call applies the settings as they are
   then, which is why a change made while the program was starting is not lost. */
void startUpdateChecks();

// Forgets what was sent and the start, for a case that needs the first run again.
void resetSentUpdateChecks();

/* Defined by the application, which owns the objects: start or stop the check
   for a newer image, and the check for newer packages. Starting what runs and
   stopping what does not are no-ops. */
Status applicationFlashCheck(bool on);
Status applicationPackageCheck(bool on);

extern const ApplyGroup kUpdateApplyGroup;

} // namespace coreapi

#endif
