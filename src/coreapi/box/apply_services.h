/*
 * apply_services.h - what makes a switched daemon or softcam start or stop
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

#ifndef __coreapi_apply_services_h__
#define __coreapi_apply_services_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

namespace coreapi
{

/* Starts and stops the programs the box runs as services, by the name their flag
   file carries. A softcam is started through the camd script, which stops the one
   running before it, and every other daemon through its own. A start or stop
   that fails answers Internal. Start and stop are called on the apply worker. */
struct ServiceControl
{
	virtual ~ServiceControl() {}

	// Whether the name's flag file is there now.
	virtual bool flagIsSet(const char *name) const = 0;
	// Whether the program is there to start: a softcam is a file of that name, a daemon is found by program on the path.
	virtual bool installed(const char *program, bool softcam) const = 0;
	virtual Status start(const char *name, bool softcam) = 0;
	virtual Status stop(const char *name, bool softcam) = 0;
};

// NotSupported for every call while nothing is installed.
ServiceControl &serviceControl();
void setServiceControl(ServiceControl *c);

// Binds the accessor above to the box's service scripts.
void installRealServiceControl();

// Forgets what was seen of the flag files, for a case that needs the first run again.
void resetSentServices();

/* The flag files of the daemons and the softcams: each file's appearance starts the
   program and its removal stops it. The first run, at startup, only notes which
   files are there, since the boot scripts started those programs already. The
   scripts run on the apply worker, so a run returns before they are done and a
   failure is tried again by the run after it. */
extern const ApplyGroup kServicesApplyGroup;

} // namespace coreapi

#endif
