/*
 * apply_sectionsd.h - what makes a changed guide cache or time setting take effect
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

#ifndef __coreapi_apply_sectionsd_h__
#define __coreapi_apply_sectionsd_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

namespace coreapi
{

/* What the section daemon is told about the guide cache and the time source.
   The daemon takes one message for all of it and compares what it already holds,
   so a repeat of the same values changes nothing there: it reloads its time
   thread only when the server, the interval or the switch differ.

   A seam rather than the daemon's client, so the group can run where no daemon
   exists. */
struct SectionsdOutput
{
	virtual ~SectionsdOutput() {}

	/* Sends the settings the daemon is configured by. The daemon's client reports
	   no failure of its own, so the real side answers Ok once the message was
	   handed over. */
	virtual Status sendConfig() = 0;
};

SectionsdOutput &sectionsdOutput();
void setSectionsdOutput(SectionsdOutput *o);

// Binds the seam to the running box's section daemon.
void installRealSectionsdOutput();

/* Defined by the application, because the configuration is assembled from the
   settings by the object that also hands it to the daemon at its start, and this
   layer must not reach that object's header. Sends the configuration to the
   daemon, through the program's client once it exists and through a client of
   its own before. */
Status applicationSendSectionsdConfig();

/* The guide cache sizes and how long events are kept, whether the guide is saved
   and read and how often, where it is kept, and the time server with its
   interval and switch. */
extern const ApplyGroup kSectionsdConfigApplyGroup;

} // namespace coreapi

#endif
