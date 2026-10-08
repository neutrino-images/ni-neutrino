/*
 * apply_keys.h - what the remote control settings tell the input driver
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

#ifndef __coreapi_apply_keys_h__
#define __coreapi_apply_keys_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

namespace coreapi
{

/* What the remote control group tells the input driver. One call per thing the
   driver takes, so the group decides what is sent and when and this only
   carries it. Nothing here asks anybody anything.

   A seam rather than the driver itself, so the group can run where no input
   driver exists. */
struct RcControl
{
	virtual ~RcControl() {}

	/* How long a key press is held off after the last one, for the keys that
	   repeat and for the others, in milliseconds, and the same two as the
	   repeat the kernel generates. Zero is no blocking. */
	virtual Status setRepeat(int block_ms, int generic_ms) = 0;
	// Programs the receiver for the remote control the setting names.
	virtual Status selectHardware() = 0;
};

/* NotSupported for every call while nothing is installed, so a group run before
   the input driver exists fails and says so instead of touching nothing. */
RcControl &rcControl();
void setRcControl(RcControl *c);

// Binds the accessor above to the program's input driver.
void installRealRcControl();

/* The remote control the receiver was programmed for before the last run of the
   group, which is the same as now when that run did not change it, and -1 before
   any run. What a question about a new remote control puts back, so a value
   written from elsewhere in between is what returns. */
int remoteHardwareBeforeLastChange();

// Forgets what was sent, for a case that needs the first run again.
void resetSentKeys();

/* The key repeat blocking and the receiver's remote control. The group is in
   the zapit phase because the input driver is built just before it. */
extern const ApplyGroup kRcApplyGroup;

} // namespace coreapi

#endif
