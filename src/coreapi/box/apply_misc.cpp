/*
 * apply_misc.cpp - what makes a changed miscellaneous setting take effect
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

#include "coreapi/box/apply_misc.h"
#include "coreapi/box/sentstate.h"

#include <system/settings.h>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

class NoMiscOutput : public MiscOutput
{
	public:
		Status setScanSdt(int) { return Status::NotSupported; }
		Status setStreamPort(int) { return Status::NotSupported; }
		Status setTeletextCache(bool) { return Status::NotSupported; }
		Status configureEpgFilter() { return Status::NotSupported; }
		Status startEpgScan() { return Status::NotSupported; }
		Status clearEpgScan() { return Status::NotSupported; }
		Status setCpuFreq(int) { return Status::NotSupported; }
		Status setFanSpeed(int) { return Status::NotSupported; }
};

NoMiscOutput g_no_misc_output;
MiscOutput *g_misc_output = 0;

/* What the groups last put on the box where sending it again is not harmless:
   the teletext cache is set up once and torn down once, and the guide scan
   starts a pass that walks the favourites. The scan and the filter are not
   touched at startup, since the lists they read are built later from the same
   settings. */
struct MiscState
{
	// Nothing is set up before the first run.
	bool txt_on;
	bool scan_known;
	int  scan_save_mode;
	int  scan;
	int  scan_mode;
	MiscState() : txt_on(false), scan_known(false), scan_save_mode(0), scan(0), scan_mode(0) {}
};

MiscState g_state;

/* What the processor and the fan were last told, apart from the state above
   because standby holds them one by one. */
Sent<int> g_cpu_sent;
Sent<int> g_fan_sent;
SentFlags g_power_flags;
const unsigned kCpuHeld = 1u;
const unsigned kFanHeld = 2u;

/* The channel daemon reads the number as it is, a write is a plain assignment
   and a repeat costs nothing. */
Status runScanSdt()
{
	return miscOutput().setScanSdt(g_settings.enable_sdt);
}

// The stream server keeps the listening socket when the port is the one in use.
Status runStreamPort()
{
	return miscOutput().setStreamPort(g_settings.streaming_port);
}

Status runTuxtxt()
{
	const bool on = g_settings.cacheTXT != 0;
	if (g_state.txt_on == on)
		return Status::Ok;

	const Status s = miscOutput().setTeletextCache(on);
	if (s == Status::Ok)
		g_state.txt_on = on;
	return s;
}

/* The filter follows the save mode and the pass follows the bouquets and the
   mode: a mode that is off clears the pass, any other starts it again. A change
   of one does not touch the other. */
Status runEpgScan()
{
	const int save_mode = g_settings.epg_save_mode;
	const int scan = g_settings.epg_scan;
	const int mode = g_settings.epg_scan_mode;

	if (!g_state.scan_known)
	{
		g_state.scan_save_mode = save_mode;
		g_state.scan = scan;
		g_state.scan_mode = mode;
		g_state.scan_known = true;
		return Status::Ok;
	}

	MiscOutput &out = miscOutput();
	Status first = Status::Ok;
	if (save_mode != g_state.scan_save_mode)
	{
		const Status s = out.configureEpgFilter();
		noteFirst(first, s);
		if (s == Status::Ok)
			g_state.scan_save_mode = save_mode;
	}
	if (scan != g_state.scan || mode != g_state.scan_mode)
	{
		const Status s = (mode != EPG_SCAN_MODE_OFF) ? out.startEpgScan() : out.clearEpgScan();
		noteFirst(first, s);
		if (s == Status::Ok)
		{
			g_state.scan = scan;
			g_state.scan_mode = mode;
		}
	}
	return first;
}

Status runCpuFreq()
{
	Status first = Status::Ok;
	const int mhz = g_settings.cpufreq;
	sendChanged(first, g_power_flags, kCpuHeld, g_cpu_sent, mhz, [&]() { return miscOutput().setCpuFreq(mhz); });
	return first;
}

Status runFanSpeed()
{
	Status first = Status::Ok;
	const int speed = g_settings.fan_speed;
	sendChanged(first, g_power_flags, kFanHeld, g_fan_sent, speed, [&]() { return miscOutput().setFanSpeed(speed); });
	return first;
}

const char *const kCpuFreqKeys[] =
{
	"cpufreq"
};

const char *const kFanKeys[] =
{
	"fan_speed"
};

const char *const kScanSdtKeys[] =
{
	"enable_sdt"
};

const char *const kStreamPortKeys[] =
{
	"streaming_port"
};

const char *const kTuxtxtKeys[] =
{
	"cacheTXT"
};

const char *const kEpgScanKeys[] =
{
	"epg_scan",
	"epg_scan_mode",
	"epg_save_mode"
};

} // namespace

MiscOutput &miscOutput()
{
	if (!g_misc_output)
		return g_no_misc_output;
	return *g_misc_output;
}

void setMiscOutput(MiscOutput *o) { g_misc_output = o; }

void holdCpuFreq(bool held)
{
	g_power_flags.hold(kCpuHeld, held);
	// The standby value is on the processor now, so the setting is no longer what it runs at.
	g_cpu_sent.known = false;
}

void holdFanSpeed(bool held)
{
	g_power_flags.hold(kFanHeld, held);
	g_fan_sent.known = false;
}

void resetSentMisc()
{
	g_state = MiscState();
	g_cpu_sent = Sent<int>();
	g_fan_sent = Sent<int>();
	g_power_flags.reset();
}

/* The channel daemon and the stream server are up when the network is, and the
   first channel is zapped after this, so its first scan already follows the
   setting. The teletext cache is set up before that zap as well. */
const ApplyGroup kScanSdtApplyGroup = { "scanSdt", ApplyPhase::Network, COREAPI_KEYS(kScanSdtKeys), &runScanSdt };

const ApplyGroup kStreamPortApplyGroup = { "streamPort", ApplyPhase::Network, COREAPI_KEYS(kStreamPortKeys), &runStreamPort };

const ApplyGroup kTuxtxtApplyGroup = { "tuxtxt", ApplyPhase::Network, COREAPI_KEYS(kTuxtxtKeys), &runTuxtxt };

const ApplyGroup kEpgScanApplyGroup = { "epgScan", ApplyPhase::Network, COREAPI_KEYS(kEpgScanKeys), &runEpgScan };

/* The startup sets both before the network is up, so the first run only repeats what
   they were brought up with. */
const ApplyGroup kCpuFreqApplyGroup = { "cpuFreq", ApplyPhase::Network, COREAPI_KEYS(kCpuFreqKeys), &runCpuFreq };

const ApplyGroup kFanApplyGroup = { "fan", ApplyPhase::Network, COREAPI_KEYS(kFanKeys), &runFanSpeed };

} // namespace coreapi
