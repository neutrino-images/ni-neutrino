/*
 * apply_sectionsd.cpp - what makes a changed guide cache or time setting take effect
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

#include "coreapi/box/apply_sectionsd.h"

namespace coreapi
{

namespace
{

class NoSectionsdOutput : public SectionsdOutput
{
	public:
		Status sendConfig() { return Status::NotSupported; }
};

NoSectionsdOutput g_no_sectionsd_output;
SectionsdOutput *g_sectionsd_output = 0;

/* The message carries every value at once, so one run covers any of the keys.
   Which of them changed is the daemon's to find out. */
Status runSectionsdConfig()
{
	return sectionsdOutput().sendConfig();
}

const char *const kSectionsdConfigKeys[] =
{
	"epg_cache_time",
	"epg_extendedcache_time",
	"epg_max_events",
	"epg_old_events",
	"epg_save",
	"epg_read",
	"epg_save_frequently",
	"epg_read_frequently",
	"epg_dir",
	"network_ntpenable",
	"network_ntpserver",
	"network_ntprefresh"
};

} // namespace

SectionsdOutput &sectionsdOutput()
{
	if (!g_sectionsd_output)
		return g_no_sectionsd_output;
	return *g_sectionsd_output;
}

void setSectionsdOutput(SectionsdOutput *o) { g_sectionsd_output = o; }

/* The daemon is started before the network phase and is given the same values
   by CEitManager::SetConfig at its start, so the send at startup repeats what
   it already holds, now that the mounts that may hold the guide are made. */
const ApplyGroup kSectionsdConfigApplyGroup = { "sectionsdConfig", ApplyPhase::Network, COREAPI_KEYS(kSectionsdConfigKeys), &runSectionsdConfig };

} // namespace coreapi
