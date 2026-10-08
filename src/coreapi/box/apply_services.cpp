/*
 * apply_services.cpp - what makes a switched daemon or softcam start or stop
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

#include "coreapi/box/apply_services.h"
#include "coreapi/box/applyworker.h"
#include "coreapi/box/sentstate.h"

#include <string>

namespace coreapi
{

namespace
{

class NoServiceControl : public ServiceControl
{
	public:
		bool flagIsSet(const char *) const { return false; }
		bool installed(const char *, bool) const { return false; }
		Status start(const char *, bool) { return Status::NotSupported; }
		Status stop(const char *, bool) { return Status::NotSupported; }
};

NoServiceControl g_no_service_control;
ServiceControl *g_service_control = 0;

struct Service
{
	const char *key;
	// The name the flag file and the service script carry.
	const char *name;
	bool        softcam;
	// The executable a daemon is looked up by on the path; a softcam is a file of its own name.
	const char *program;
};

const Service kServices[] =
{
	{ "flag_daemon_fritzcallmonitor", "fritzcallmonitor", false, "FritzCallMonitor" },
	{ "flag_daemon_nfsd", "nfsd", false, "rpc.nfsd" },
	{ "flag_daemon_samba", "samba", false, "smbd" },
	{ "flag_daemon_tuxcald", "tuxcald", false, "tuxcald" },
	{ "flag_daemon_tuxmaild", "tuxmaild", false, "tuxmaild" },
	{ "flag_daemon_emmrd", "emmrd", false, "emmrd" },
	{ "flag_daemon_inadyn", "inadyn", false, "inadyn" },
	{ "flag_daemon_dropbear", "dropbear", false, "dropbear" },
	{ "flag_daemon_djmount", "djmount", false, "djmount" },
	{ "flag_daemon_ushare", "ushare", false, "ushare" },
	{ "flag_daemon_minidlnad", "minidlnad", false, "minidlnad" },
	{ "flag_daemon_xupnpd", "xupnpd", false, "xupnpd" },
	{ "flag_daemon_crond", "crond", false, "crond" },
	{ "flag_camd_mgcamd", "mgcamd", true, NULL },
	{ "flag_camd_doscam", "doscam", true, NULL },
	{ "flag_camd_ncam", "ncam", true, NULL },
	{ "flag_camd_osmod", "osmod", true, NULL },
	{ "flag_camd_oscam", "oscam", true, NULL },
	{ "flag_camd_cccam", "cccam", true, NULL },
	{ "flag_camd_gbox", "gbox", true, NULL }
};

const size_t kServiceCount = sizeof(kServices) / sizeof(kServices[0]);
static_assert(kServiceCount <= 32, "one failure bit per service");

/* Whether each flag was there when the group last looked. A group runs for every
   key it holds, so a program is touched only where its file differs from that:
   starting a running daemon again, or a softcam, which restarts the one running,
   is not nothing. */
Sent<int> g_seen[kServiceCount];
bool g_noted = false;
// A start or stop the worker could not make, one bit per service.
SentFlags g_failed;

int flagIsThere(const Service &s)
{
	return serviceControl().flagIsSet(s.name) ? 1 : 0;
}

/* A failed start or stop is tried again by the next run. A stop of a program that
   is not installed fails for good, and asking again on every later run would repeat
   it for keys that have nothing to do with it. One that is installed may well be
   running still, so its stop is tried again. */
void takeFailed(ServiceControl &control)
{
	const unsigned m = g_failed.take();
	for (size_t i = 0; i < kServiceCount; i++)
	{
		if (!(m & (1u << i)))
			continue;
		const Service &s = kServices[i];
		if (!flagIsThere(s) && !control.installed(s.softcam ? s.name : s.program, s.softcam))
		{
			g_seen[i].known = true;
			g_seen[i].value = 0;
		}
		else
			g_seen[i].known = false;
	}
}

Status runServices()
{
	if (!g_noted)
	{
		for (size_t i = 0; i < kServiceCount; i++)
		{
			g_seen[i].known = true;
			g_seen[i].value = flagIsThere(kServices[i]);
		}
		g_noted = true;
		return Status::Ok;
	}

	ServiceControl &control = serviceControl();
	takeFailed(control);
	Status first = Status::Ok;
	// A service script runs for seconds, a softcam's one more, so they run on the worker.
	ServiceControl *c = &control;
	for (size_t i = 0; i < kServiceCount; i++)
	{
		const Service *s = &kServices[i];
		const int on = flagIsThere(*s);
		// A softcam's own prefix, which the softcam menu waits for.
		postChanged(first, g_failed, 1u << i, g_seen[i], on, std::string(s->softcam ? "softcam." : "services.") + s->name, s->key, [c, s, on]() {
			return on ? c->start(s->name, s->softcam) : c->stop(s->name, s->softcam);
		});
	}
	return first;
}

const char *const kServiceKeys[] =
{
	"flag_daemon_fritzcallmonitor",
	"flag_daemon_nfsd",
	"flag_daemon_samba",
	"flag_daemon_tuxcald",
	"flag_daemon_tuxmaild",
	"flag_daemon_emmrd",
	"flag_daemon_inadyn",
	"flag_daemon_dropbear",
	"flag_daemon_djmount",
	"flag_daemon_ushare",
	"flag_daemon_minidlnad",
	"flag_daemon_xupnpd",
	"flag_daemon_crond",
	"flag_camd_mgcamd",
	"flag_camd_doscam",
	"flag_camd_ncam",
	"flag_camd_osmod",
	"flag_camd_oscam",
	"flag_camd_cccam",
	"flag_camd_gbox"
};

} // namespace

ServiceControl &serviceControl()
{
	if (!g_service_control)
		return g_no_service_control;
	return *g_service_control;
}

void setServiceControl(ServiceControl *c) { g_service_control = c; }

void resetSentServices()
{
	for (size_t i = 0; i < kServiceCount; i++)
		g_seen[i] = Sent<int>();
	g_noted = false;
	g_failed.reset();
}

/* Last, with the network: a service the boot scripts start is up by then, and
   the first run only notes the files. */
const ApplyGroup kServicesApplyGroup = { "services", ApplyPhase::Network, COREAPI_KEYS(kServiceKeys), &runServices };

} // namespace coreapi
