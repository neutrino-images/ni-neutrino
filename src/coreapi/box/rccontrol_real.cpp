/*
 * rccontrol_real.cpp - the input driver behind the remote control group
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

#include <driver/rcinput.h>

extern CRCInput *g_RCInput;

namespace coreapi
{

namespace
{

/* Its own translation unit for the reason every other adapter here has one: a
   binary that only wants the group must not have to link the input driver. The
   driver is reached when a group runs, not when this is installed, because the
   seam is installed long before the driver is built. */
class RealRcControl : public RcControl
{
	public:
		Status setRepeat(int block_ms, int generic_ms)
		{
			if (!g_RCInput)
				return Status::Internal;
			g_RCInput->repeat_block = (uint64_t) block_ms * 1000;
			g_RCInput->repeat_block_generic = (uint64_t) generic_ms * 1000;
			g_RCInput->setKeyRepeatDelay(block_ms, generic_ms);
			return Status::Ok;
		}

		Status selectHardware()
		{
			if (!g_RCInput)
				return Status::Internal;
			g_RCInput->set_rc_hw();
			return Status::Ok;
		}
};

RealRcControl g_real_rc_control;

} // anonymous namespace

void installRealRcControl() { setRcControl(&g_real_rc_control); }

} // namespace coreapi
