/*
 * apply_cec.h - what makes a changed HDMI CEC setting take effect
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

#ifndef __coreapi_apply_cec_h__
#define __coreapi_apply_cec_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

namespace coreapi
{

/* What the CEC group tells the HDMI link: the mode, whether the television is
   switched on and off with the box, and where the volume keys go. One call per
   thing the driver takes. A seam rather than the driver, so the group can run
   where there is no link. */
struct CecLink
{
	virtual ~CecLink() {}

	virtual Status setMode(int mode) = 0;
	virtual Status setAutoStandby(int on) = 0;
	virtual Status setAutoView(int on) = 0;
	/* Where the volume keys act: the box, the audio system or the television.
	   Only a box whose driver takes the destination is asked. */
	virtual bool takesAudioDestination() const = 0;
	virtual Status setAudioDestination(int destination) = 0;
};

/* NotSupported for every call while nothing is installed, so a group run
   before the decoder exists fails and says so instead of touching nothing. */
CecLink &cecLink();
void setCecLink(CecLink *l);

// Binds the accessor above to the program's video decoder, which carries the link.
void installRealCecLink();

/* While deferred the group sends nothing, and the run that follows the end of
   the deferral sends everything. The box that woke for a recording keeps the
   television alone until somebody wakes the box for real. */
void deferCec(bool deferred);

/* Standby leaves the television alone: going to sleep defers the group, so a write of a CEC
   setting while the box sleeps sends nothing, and waking ends the deferral and runs the
   group, which sends what changed meanwhile, or everything where nothing was sent yet
   (a box woken for a recording). Waking answers what that run answered. */
Status cecStandby(bool asleep);

/* Around one standby change: held, the auto flag the change would act on (standby when
   asleep, view on when waking) goes to 0 and no run sends it; let go, the setting goes back
   on the link. Held where the television is to be left alone, and where the setting is off
   but the link may still have it on. Nothing is held where the link is known to have 0. On
   the loop, around the change and the CEC run that follows a wake. */
void holdCecPower(bool asleep, bool held, bool leave_tv);

// Forgets what was sent and the deferral, for a case that needs the first run again.
void resetSentCec();

extern const ApplyGroup kCecApplyGroup;

} // namespace coreapi

#endif
