/*
 * cicontrol_real.cpp - the ci group's seam bound to the module driver
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

#include "coreapi/box/apply_ci.h"

#include <hardware/ca.h>
#include <zapit/capmt.h>

namespace coreapi
{

namespace
{

class RealCiControl : public CiControl
{
	public:
		Status setClock(int slot, int mhz)
		{
#if HAVE_LIBSTB_HAL
			cCA::GetInstance()->SetTSClock(mhz * 1000000, slot);
#else
			(void) slot;
			cCA::GetInstance()->SetTSClock(mhz * 1000000);
#endif
			return Status::Ok;
		}

		Status setDelay(int delay)
		{
#if BOXMODEL_VUPLUS_ALL
			cCA::GetInstance()->SetCIDelay(delay);
#else
			(void) delay;
#endif
			return Status::Ok;
		}

		Status setRelevantPidsRouting(int slot, int on)
		{
#if BOXMODEL_VUPLUS_ALL
			cCA::GetInstance()->SetCIRelevantPidsRouting(on, slot);
#else
			(void) slot;
			(void) on;
#endif
			return Status::Ok;
		}

		Status setOperator(int slot, int on)
		{
#if HAVE_LIBSTB_HAL
			cCA::GetInstance()->SetCIOperator(on, slot);
#else
			(void) slot;
			(void) on;
#endif
			return Status::Ok;
		}

		Status setCheckLiveSlot(int on)
		{
#if HAVE_LIBSTB_HAL
			cCA::GetInstance()->setCheckLiveSlot(on);
#else
			(void) on;
#endif
			return Status::Ok;
		}

		Status setTuner(int tuner)
		{
			CCamManager::getInstance()->SetCITuner(tuner);
			return Status::Ok;
		}
};

RealCiControl g_real_ci_control;

} // namespace

void installRealCiControl()
{
	setCiControl(&g_real_ci_control);
}

} // namespace coreapi
