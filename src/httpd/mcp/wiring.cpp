/*
 * wiring.cpp - what the MCP endpoint is connected to
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

#include "httpd/mcp/wiring.h"

#include "httpd/mcp/applynotes.h"
#include "httpd/mcp/callrunner.h"

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <cstddef>

#include <unistd.h>

namespace httpd
{
namespace mcp
{

namespace
{

OpenThreads::Mutex &wiringLock()
{
	static OpenThreads::Mutex m;
	return m;
}

Wiring &current()
{
	static Wiring w = { NULL, NULL };
	return w;
}

void set(const Wiring &w)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(wiringLock());
	current() = w;
}

} // namespace

void install(const Wiring &w)
{
	watchApplyFailures();
	set(w);
	closeCalls(false);
}

void uninstall()
{
	const Wiring none = { NULL, NULL };
	set(none);
}

bool installed(Wiring *out)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(wiringLock());
	const Wiring &w = current();
	if (w.tools == NULL || w.verify == NULL)
		return false;
	if (out != NULL)
		*out = w;
	return true;
}

bool drain(unsigned bound_ms)
{
	uninstall();
	closeCalls(true);
	for (unsigned waited = 0; runningCalls() > 0 && waited < bound_ms; waited += 10)
		usleep(10000);
	return runningCalls() == 0;
}

} // namespace mcp
} // namespace httpd
