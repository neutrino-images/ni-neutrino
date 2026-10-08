/*
 * apply_hdd.cpp - what makes a changed hard disk power setting take effect
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
#include "coreapi/box/applyworker.h"
#include "coreapi/box/sentstate.h"

#include <system/settings.h>

#include <stdio.h>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

class NoHddControl : public HddControl
{
	public:
		bool hasIdleDaemon() { return false; }
		Status restartIdleDaemon(int) { return Status::NotSupported; }
		bool hasHdparm() { return false; }
		bool hdparmTakesNoise() { return false; }
		std::vector<std::string> diskNames() { return std::vector<std::string>(); }
		Status setDisk(const std::string &, int, int, bool) { return Status::NotSupported; }
};

NoHddControl g_no_hdd_control;
HddControl *g_hdd_control = 0;

/* What was last sent. Starting the idle daemon over restarts its count and the
   disks are told their timeout again, so a key of the group sends only what
   differs; the disks are part of what was sent, so a disk plugged in since is
   told by the next run of the group, which nothing starts on a hotplug. */
Sent<int> g_idle_seconds;
Sent<std::string> g_disks;
/* What the worker could not send. Nothing else sets the disks' power from outside
   the group, so nothing is held. */
SentFlags g_flags;

const unsigned kIdleSent = 1;
const unsigned kDisksSent = 2;

// The values 241 and 242 are the driver's codes for half an hour and an hour; the rest are units of five seconds.
int idleSeconds(int sleep)
{
	switch (sleep)
	{
		case 241:
			return 30 * 60;
		case 242:
			return 60 * 60;
		default:
			return sleep * 5;
	}
}

/* The idle daemon when there is one and a timeout is wanted, otherwise hdparm
   per disk. A timeout below the first step of the choice is taken as that step,
   as an older settings file may hold one. */
/* The idle daemon's restart and hdparm run on the apply worker: hdparm waits for a
   disk that sleeps to spin up. */
Status runHdIdle()
{
	HddControl &hdd = hddControl();
	HddControl *c = &hdd;
	Status first = Status::Ok;

	const unsigned failed = g_flags.take();
	if (failed & kIdleSent)
		g_idle_seconds.known = false;
	if (failed & kDisksSent)
		g_disks.known = false;

	int sleep = g_settings.hdd_sleep;
	if (sleep > 0 && sleep < 60)
		sleep = 60;

	if (hdd.hasIdleDaemon() && sleep > 0)
	{
		const int seconds = idleSeconds(sleep);
		postChanged(first, g_flags, kIdleSent, g_idle_seconds, seconds, "hdIdle.daemon", "hdd_sleep", [c, seconds]() { return c->restartIdleDaemon(seconds); });
		return first;
	}

	if (!hdd.hasHdparm())
		return first;

	const bool with_noise = hdd.hdparmTakesNoise();
	const int noise = g_settings.hdd_noise;
	const std::vector<std::string> disks = hdd.diskNames();

	// The noise level means nothing to the busybox tool, so it is not part of what was sent then.
	char head[48];
	snprintf(head, sizeof(head), "%d:%d", sleep, with_noise ? noise : -1);
	std::string sent = head;
	for (size_t i = 0; i < disks.size(); ++i)
		sent += ":" + disks[i];

	postChanged(first, g_flags, kDisksSent, g_disks, sent, "hdIdle.disks", "hdd_sleep hdd_noise", [c, disks, noise, sleep, with_noise]()
	{
		Status all = Status::Ok;
		// Every disk is told even after one refuses.
		for (size_t i = 0; i < disks.size(); ++i)
			noteFirst(all, c->setDisk(disks[i], noise, sleep, with_noise));
		return all;
	});
	return first;
}

const char *const kHdIdleKeys[] =
{
	"hdd_sleep",
	"hdd_noise"
};

} // namespace

HddControl &hddControl()
{
	if (!g_hdd_control)
		return g_no_hdd_control;
	return *g_hdd_control;
}

void setHddControl(HddControl *c) { g_hdd_control = c; }

void resetSentHdd()
{
	g_idle_seconds = Sent<int>();
	g_disks = Sent<std::string>();
	g_flags.reset();
}

/* After the mounts, which the disks come from. */
const ApplyGroup kHdIdleApplyGroup = { "hdIdle", ApplyPhase::Network, COREAPI_KEYS(kHdIdleKeys), &runHdIdle };

} // namespace coreapi
