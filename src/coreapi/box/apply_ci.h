/*
 * apply_ci.h - what makes a changed common interface setting take effect
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

#ifndef __coreapi_apply_ci_h__
#define __coreapi_apply_ci_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

namespace coreapi
{

/* What the ci group tells the module slots and the stack that feeds them. One
   call per thing the driver takes, so the group decides what is sent and when
   and this only carries it. A call a build has no driver for answers Ok and
   does nothing, as the driver's own stub does. */
struct CiControl
{
	virtual ~CiControl() {}

	// The transport stream clock of a slot, in megahertz as the setting holds it.
	virtual Status setClock(int slot, int mhz) = 0;
	// How long the module is waited for, in the steps the setting offers.
	virtual Status setDelay(int delay) = 0;
	virtual Status setRelevantPidsRouting(int slot, int on) = 0;
	virtual Status setOperator(int slot, int on) = 0;
	virtual Status setCheckLiveSlot(int on) = 0;
	/* The tuner the module reads, minus one for none. The stack that picks the
	   module for a channel keeps it, so it is told even where the driver has no
	   input to switch. */
	virtual Status setTuner(int tuner) = 0;
};

/* NotSupported for every call while nothing is installed, so a group run before
   the seam exists fails and says so. */
CiControl &ciControl();
void setCiControl(CiControl *c);

// Binds the accessor above to the program's module driver and the stack behind it.
void installRealCiControl();

// Forgets what was sent, for a case that needs the first run again.
void resetSentCi();

/* The module slots' clock, delay, routing, operator mode and live check, and the
   tuner a module is bound to. First in startup, with the box source, because the
   channel daemon reads the tuner as it zaps the first channel. */
extern const ApplyGroup kCiApplyGroup;

} // namespace coreapi

#endif
