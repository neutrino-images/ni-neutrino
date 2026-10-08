/*
 * apply_misc.h - what makes a changed miscellaneous setting take effect
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

#ifndef __coreapi_apply_misc_h__
#define __coreapi_apply_misc_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

#include <string>
#include <vector>

namespace coreapi
{

/* What the miscellaneous groups tell the daemons and drivers. One call per thing
   they take, so the groups decide what is sent and when and this only carries it.
   Nothing here asks anybody anything.

   A seam rather than the objects themselves, so the groups can run where no
   channel daemon, stream manager or teletext cache exists. */
struct MiscOutput
{
	virtual ~MiscOutput() {}

	// How the channel daemon follows the service description table, in the setting's numbering.
	virtual Status setScanSdt(int mode) = 0;
	// The port the box streams on; a port equal to the one in use changes nothing.
	virtual Status setStreamPort(int port) = 0;
	/* Whether the teletext pages of the running channel are collected. Asking for
	   the state it is in changes nothing. */
	virtual Status setTeletextCache(bool on) = 0;
	// Which channels the guide collects for, taken from the favourites.
	virtual Status configureEpgFilter() = 0;
	// The background guide scan, started or stopped.
	virtual Status startEpgScan() = 0;
	virtual Status clearEpgScan() = 0;
	// The clock of the processor, in MHz. Ok where the box cannot change it: there is nothing to send.
	virtual Status setCpuFreq(int mhz) = 0;
	// The speed of the fan, in the setting's steps. Ok where the box has none.
	virtual Status setFanSpeed(int speed) = 0;
};

MiscOutput &miscOutput();

/* Defined by the application, because the objects behind them belong to a header that
   reaches the GUI. Both answer Ok on a box that has nothing to set: every startup runs
   the groups. */
Status applicationSetCpuFreq(int mhz);
Status applicationSetFanSpeed(int speed);
void setMiscOutput(MiscOutput *o);

// Binds the seam to the running box's daemons and drivers.
void installRealMiscOutput();

// How the channel daemon follows channel changes of the broadcaster.
extern const ApplyGroup kScanSdtApplyGroup;

// The port of the stream server.
extern const ApplyGroup kStreamPortApplyGroup;

// The teletext cache, switched on and off with the setting.
extern const ApplyGroup kTuxtxtApplyGroup;

/* The background guide scan: its mode and bouquets, and the guide filter that
   follows from saving only the favourites. */
extern const ApplyGroup kEpgScanApplyGroup;

/* The processor clock and the fan speed. Both are the box's own business while it
   sleeps: standby sets the low values itself, so it holds the group first and the
   group sends none until let go, and a write made meanwhile is kept for the wake.
   Holding forgets what was sent, since the box no longer runs at it. On the loop only. */
extern const ApplyGroup kCpuFreqApplyGroup;
extern const ApplyGroup kFanApplyGroup;
void holdCpuFreq(bool held);
void holdFanSpeed(bool held);

// Forgets what the groups last sent, for a case that needs the first run again.
void resetSentMisc();

} // namespace coreapi

#endif
