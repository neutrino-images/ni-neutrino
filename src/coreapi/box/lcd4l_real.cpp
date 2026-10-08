/*
 * lcd4l_real.cpp - the LCD4Linux group's seam bound to the program's service
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

#include "coreapi/box/apply_lcd4l.h"

namespace coreapi
{

namespace
{

/* Where the build has no LCD4Linux there is no service and the settings of it
   are never written, so a run finds nothing to do. */
class RealLcd4lControl : public Lcd4lControl
{
	public:
		Status restartService(int mode)
		{
#ifdef ENABLE_LCD4LINUX
			// The worker has no other way to tell anybody, the service's messages being silent there.
			if (!applicationRestartLcd4l(mode))
				return Status::Internal;
#else
			(void) mode;
#endif
			return Status::Ok;
		}

		Status reinit()
		{
#ifdef ENABLE_LCD4LINUX
			applicationReinitLcd4l();
#endif
			return Status::Ok;
		}

		Status forceRun()
		{
#ifdef ENABLE_LCD4LINUX
			applicationForceRunLcd4l();
#endif
			return Status::Ok;
		}
};

RealLcd4lControl g_real_lcd4l_control;

} // namespace

void installRealLcd4lControl()
{
	setLcd4lControl(&g_real_lcd4l_control);
}

} // namespace coreapi
