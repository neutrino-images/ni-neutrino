/*
 * hddcontrol_real.cpp - the disk power group's seam on the running box
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

#include "coreapi/box/apply_hdd.h"
#include "coreapi/box/storage_disks.h"

#include <system/helpers.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

namespace coreapi
{

namespace
{

class RealHddControl : public HddControl
{
	public:
		bool hasIdleDaemon()
		{
			return !find_executable("hd-idle").empty();
		}

		Status restartIdleDaemon(int seconds)
		{
			const std::string tool = find_executable("hd-idle");
			if (tool.empty())
				return Status::NotSupported;
			if (system("kill $(pidof hd-idle)") == -1)
				return Status::Internal;
			char text[24];
			snprintf(text, sizeof(text), "%d", seconds);
			my_system(3, tool.c_str(), "-i", text);
			return Status::Ok;
		}

		bool hasHdparm()
		{
			return !find_executable("hdparm").empty();
		}

		bool hdparmTakesNoise()
		{
			// The busybox applet is a link and takes no -M.
			const std::string tool = find_executable("hdparm");
			struct stat st;
			return !tool.empty() && !::lstat(tool.c_str(), &st) && !S_ISLNK(st.st_mode);
		}

		std::vector<std::string> diskNames()
		{
			std::vector<std::string> names;
			const std::vector<storage::DiskInfo> disks = storage::disks();
			for (size_t i = 0; i < disks.size(); ++i)
				names.push_back(disks[i].name);
			return names;
		}

		Status setDisk(const std::string &disk, int noise, int sleep, bool with_noise)
		{
			const std::string tool = find_executable("hdparm");
			if (tool.empty())
				return Status::NotSupported;
			char sleep_opt[50], noise_opt[50], dev[261];
			snprintf(sleep_opt, sizeof(sleep_opt), "-S%d", sleep);
			snprintf(noise_opt, sizeof(noise_opt), "-M%d", noise);
			snprintf(dev, sizeof(dev), "/dev/%s", disk.c_str());
			if (with_noise)
				my_system(4, tool.c_str(), noise_opt, sleep_opt, dev);
			else
				my_system(3, tool.c_str(), sleep_opt, dev);
			return Status::Ok;
		}
};

RealHddControl g_real_hdd_control;

} // namespace

void installRealHddControl()
{
	setHddControl(&g_real_hdd_control);
}

} // namespace coreapi
