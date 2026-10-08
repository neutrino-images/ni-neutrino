/*
 * apply_keys.cpp - what makes a changed remote control setting take effect
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

#include "coreapi/box/apply_keys.h"
#include "coreapi/box/sentstate.h"
#include "coreapi/settings/predicates.h"

#include <system/settings.h>

#include <utility>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

class NoRcControl : public RcControl
{
	public:
		Status setRepeat(int, int) { return Status::NotSupported; }
		Status selectHardware() { return Status::NotSupported; }
};

NoRcControl g_no_rc_control;
RcControl *g_rc_control = 0;

/* What the group last put on the driver (sentstate.h): the receiver is told the
   remote control only when it changed, and the repeat blocking only when one of
   its two values did, so a change of one key re-sends nothing of the other. */
struct RcState
{
	Sent<std::pair<int, int> > repeat;
	Sent<int> hardware;
	// The remote control before the last run, -1 before any.
	int hardware_before;
	RcState() : hardware_before(-1) {}
};

RcState g_state;
// Nobody else writes this state and nobody holds it, so no bit is ever marked.
SentFlags g_flags;

enum RcSent
{
	SentRepeat   = 1u << 0,
	SentHardware = 1u << 1
};

Status runRc()
{
	RcControl &rc = rcControl();
	Status first = Status::Ok;

	const std::pair<int, int> repeat(g_settings.repeat_blocker, g_settings.repeat_genericblocker);
	sendChanged(first, g_flags, SentRepeat, g_state.repeat, repeat,
		    [&]() { return rc.setRepeat(repeat.first, repeat.second); });

	// A box whose receiver cannot be programmed has no remote control to name.
	if (canSelectRemote())
	{
		const int hw = g_settings.remote_control_hardware;
		g_state.hardware_before = g_state.hardware.known ? g_state.hardware.value : hw;
		sendChanged(first, g_flags, SentHardware, g_state.hardware, hw, [&]() { return rc.selectHardware(); });
	}

	return first;
}

/* Every key the group answers for, whichever box declares it: a key this box
   lacks is never written, so listing it costs nothing. */
const char *const kRcKeys[] =
{
	"repeat_blocker",
	"repeat_genericblocker",
	"remote_control_hardware"
};

} // namespace

const ApplyGroup kRcApplyGroup = { "rc", ApplyPhase::Zapit, COREAPI_KEYS(kRcKeys), &runRc };

RcControl &rcControl()
{
	if (!g_rc_control)
		return g_no_rc_control;
	return *g_rc_control;
}

void setRcControl(RcControl *c) { g_rc_control = c; }

int remoteHardwareBeforeLastChange() { return g_state.hardware_before; }

void resetSentKeys()
{
	g_state = RcState();
	g_flags.reset();
}

} // namespace coreapi
