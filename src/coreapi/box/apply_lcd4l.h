/*
 * apply_lcd4l.h - what makes a changed LCD4Linux setting take effect
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

#ifndef __coreapi_apply_lcd4l_h__
#define __coreapi_apply_lcd4l_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

namespace coreapi
{

/* The service that feeds an LCD4Linux panel: a thread writing the files the
   panel's daemon reads, and the daemon's own start script. A seam so the group
   runs where no service exists. */
struct Lcd4lControl
{
	virtual ~Lcd4lControl() {}

	// Stops the service, and starts it again when the mode is not nought.
	virtual Status restartService(int mode) = 0;
	// Writes every file again, for a panel, a skin or a brightness that changed.
	virtual Status reinit() = 0;
	// Tells the automatic mode not to wait for the daemon to show up.
	virtual Status forceRun() = 0;
	// All three are called on the apply worker.
};

// NotSupported while nothing is installed.
Lcd4lControl &lcd4lControl();
void setLcd4lControl(Lcd4lControl *c);

/* Defined by the application, in the screen that owns the service's messages:
   the driver's header reaches libraries this layer does not link. Only where the
   build has LCD4Linux. */
// False when the service's script failed.
bool applicationRestartLcd4l(int mode);
void applicationReinitLcd4l();
void applicationForceRunLcd4l();

// Binds the accessor above to the program's service.
void installRealLcd4lControl();

// Forgets what was sent, for a case that needs the first run again.
void resetSentLcd4l();

/* The mode (off, automatic, on), the panel, the skin, the brightness and the
   screenshot switch. The application starts the service itself at the end of
   its startup, since the thread reads objects that exist only then, so the first
   run only learns the mode and sends nothing; every later run starts, stops or
   rewrites what differs from what the last one saw, on the apply worker. */
extern const ApplyGroup kLcd4lApplyGroup;

} // namespace coreapi

#endif
