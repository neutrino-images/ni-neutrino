/*
 * apply_ci.cpp - what makes a changed common interface setting take effect
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

#include "coreapi/base/deps.h"
#include "coreapi/box/apply_ci.h"
#include "coreapi/box/sentstate.h"

#include <system/settings.h>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

// The settings struct holds four slots.
const unsigned kSlots = 4;

class NoCiControl : public CiControl
{
	public:
		Status setClock(int, int) { return Status::NotSupported; }
		Status setDelay(int) { return Status::NotSupported; }
		Status setRelevantPidsRouting(int, int) { return Status::NotSupported; }
		Status setOperator(int, int) { return Status::NotSupported; }
		Status setCheckLiveSlot(int) { return Status::NotSupported; }
		Status setTuner(int) { return Status::NotSupported; }
};

NoCiControl g_no_ci_control;
CiControl *g_ci_control = 0;

/* What the group last put on the box, so a change of one key re-sends nothing
   else: the delay and the routing are proc files the driver rewrites, and the
   tuner switches the module's input. */
struct CiState
{
	Sent<int> clock[kSlots];
	Sent<int> rpr[kSlots];
	Sent<int> op[kSlots];
	Sent<int> delay;
	Sent<int> check_live;
	Sent<int> tuner;
};

CiState g_state;

template <class Call>
void send(Status &first, Sent<int> &sent, int v, Call call)
{
	SentFlags none;
	sendChanged(first, none, 0, sent, v, call);
}

/* Slots the box has, capped at what the settings hold. A box that cannot say has
   none: a clock written to a slot that is not there is a file that does not exist. */
unsigned slotsFitted()
{
	unsigned n = 0;
	if (systemSource().ciSlotCount(n) != Status::Ok)
		return 0;
	return n < kSlots ? n : kSlots;
}

Status runCi()
{
	CiControl &ci = ciControl();
	Status first = Status::Ok;

	/* The tuner first: the module binds to it before it is asked for a clock. */
	const int tuner = g_settings.ci_tuner;
	send(first, g_state.tuner, tuner, [&]() { return ci.setTuner(tuner); });

	const unsigned slots = slotsFitted();
	for (unsigned i = 0; i < slots; i++)
	{
		const int slot = (int) i;
		const int mhz = g_settings.ci_clock[i];
		send(first, g_state.clock[i], mhz, [&]() { return ci.setClock(slot, mhz); });
	}

#if BOXMODEL_VUPLUS_ALL
	const int delay = g_settings.ci_delay;
	send(first, g_state.delay, delay, [&]() { return ci.setDelay(delay); });
	for (unsigned i = 0; i < slots; i++)
	{
		const int slot = (int) i;
		const int on = g_settings.ci_rpr[i];
		send(first, g_state.rpr[i], on, [&]() { return ci.setRelevantPidsRouting(slot, on); });
	}
#endif

#if HAVE_LIBSTB_HAL
	for (unsigned i = 0; i < slots; i++)
	{
		const int slot = (int) i;
		const int on = g_settings.ci_op[i];
		send(first, g_state.op[i], on, [&]() { return ci.setOperator(slot, on); });
	}
	const int check = g_settings.ci_check_live;
	send(first, g_state.check_live, check, [&]() { return ci.setCheckLiveSlot(check); });
#endif

	return first;
}

/* Every key the group answers for, whichever box declares it: a key this box
   lacks is never written, so listing it costs nothing. */
const char *const kCiKeys[] =
{
	"ci_tuner",
	"ci_check_live",
	"ci_delay",
	"ci_clock_0",
	"ci_clock_1",
	"ci_clock_2",
	"ci_clock_3",
	"ci_rpr_0",
	"ci_rpr_1",
	"ci_rpr_2",
	"ci_rpr_3",
	"ci_op_0",
	"ci_op_1",
	"ci_op_2",
	"ci_op_3"
};

} // namespace

CiControl &ciControl()
{
	if (!g_ci_control)
		return g_no_ci_control;
	return *g_ci_control;
}

void setCiControl(CiControl *c) { g_ci_control = c; }

void resetSentCi()
{
	g_state = CiState();
}

/* Ahead of the channel daemon: it reads the tuner for the first channel, and
   the clocks were set inside its start before the module driver was started. */
const ApplyGroup kCiApplyGroup = { "ci", ApplyPhase::Framebuffer, COREAPI_KEYS(kCiKeys), &runCi };

} // namespace coreapi
