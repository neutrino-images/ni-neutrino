/*
 * apply_hdd.h - what makes a changed hard disk power setting take effect
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

#ifndef __coreapi_apply_hdd_h__
#define __coreapi_apply_hdd_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

#include <string>
#include <vector>

namespace coreapi
{

/* What the disk power group asks of the box: which tools it has, the disks it
   may put to sleep, and the two ways to tell them how long to wait. A seam
   rather than the tools, so the group can run where there are none. */
struct HddControl
{
	virtual ~HddControl() {}

	// The idle daemon is installed.
	virtual bool hasIdleDaemon() = 0;
	// Stops a running idle daemon and starts one that spins disks down after this many seconds.
	virtual Status restartIdleDaemon(int seconds) = 0;
	virtual bool hasHdparm() = 0;
	// The full hdparm, which takes the noise level; the busybox one does not.
	virtual bool hdparmTakesNoise() = 0;
	// The kernel's names of the disks a user can put files on.
	virtual std::vector<std::string> diskNames() = 0;
	virtual Status setDisk(const std::string &disk, int noise, int sleep, bool with_noise) = 0;
	// The two sends above are called on the apply worker, everything else on the loop.
};

/* NotSupported for every call while nothing is installed, no tool found and no
   disk, so a group run before the seam exists does nothing and says so. */
HddControl &hddControl();
void setHddControl(HddControl *c);

// Binds the seam to the tools and disks of the running box.
void installRealHddControl();

// Forgets what was sent, for a case that needs the first run again.
void resetSentHdd();

extern const ApplyGroup kHdIdleApplyGroup;

} // namespace coreapi

#endif
