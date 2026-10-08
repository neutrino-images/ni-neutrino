/*
 * servicecontrol_real.cpp - the services group's seam bound to the box's service scripts
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

#include "coreapi/base/flagfile.h"
#include "coreapi/box/apply_services.h"

#include <cstdio>
#include <string>
#include <unistd.h>

#include <system/helpers.h>

namespace coreapi
{

namespace
{

class RealServiceControl : public ServiceControl
{
	public:
		bool flagIsSet(const char *name) const
		{
			std::string path = FLAGDIR;
			path += "/.";
			path += name;
			return flagFileIsSet(path.c_str());
		}

		bool installed(const char *program, bool softcam) const
		{
			if (softcam)
				return access((std::string("/var/bin/") + program).c_str(), F_OK) == 0;
			return !find_executable(program).empty();
		}

		Status start(const char *name, bool softcam) { return run("start", name, softcam); }
		Status stop(const char *name, bool softcam) { return run("stop", name, softcam); }

	private:
		Status run(const char *what, const char *name, bool softcam)
		{
			int rc;
			if (softcam)
			{
				printf("[services] executing \"service camd %s %s\"\n", what, name);
				rc = my_system(4, "service", "camd", what, name);
				// The screen shows its message for this long, and a softcam needs it to come up.
				sleep(1);
			}
			else
			{
				printf("[services] executing \"service %s %s\"\n", name, what);
				rc = my_system(3, "service", name, what);
			}
			if (rc != 0)
			{
				printf("[services] executing failed\n");
				return Status::Internal;
			}
			return Status::Ok;
		}
};

RealServiceControl g_real_service_control;

} // namespace

void installRealServiceControl()
{
	setServiceControl(&g_real_service_control);
}

} // namespace coreapi
