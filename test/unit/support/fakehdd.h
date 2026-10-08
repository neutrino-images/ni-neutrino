/*
 * fakehdd.h - a disk power seam that records what the group sends
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

#ifndef __support_fakehdd_h__
#define __support_fakehdd_h__

#include "coreapi/box/apply_hdd.h"

#include <functional>
#include <string>
#include <vector>

struct FakeHdd : public coreapi::HddControl
{
	bool idle_daemon;
	bool hdparm;
	bool full_hdparm;
	std::vector<std::string> disks;
	coreapi::Status idle_answer;

	// Seconds of each restart of the idle daemon, and one entry per disk set as "name:noise:sleep:withnoise".
	std::vector<int> restarts;
	std::vector<std::string> set;
	// Called first in every setDisk, on the thread that makes it.
	std::function<void()> before;

	FakeHdd() : idle_daemon(false), hdparm(false), full_hdparm(true), idle_answer(coreapi::Status::Ok) {}

	bool hasIdleDaemon() { return idle_daemon; }
	coreapi::Status restartIdleDaemon(int seconds)
	{
		restarts.push_back(seconds);
		return idle_answer;
	}
	bool hasHdparm() { return hdparm; }
	bool hdparmTakesNoise() { return full_hdparm; }
	std::vector<std::string> diskNames() { return disks; }
	coreapi::Status setDisk(const std::string &disk, int noise, int sleep, bool with_noise)
	{
		if (before)
			before();
		set.push_back(disk + ":" + std::to_string(noise) + ":" + std::to_string(sleep) + ":" + (with_noise ? "1" : "0"));
		return coreapi::Status::Ok;
	}

	void forget()
	{
		restarts.clear();
		set.clear();
	}
};

#endif
