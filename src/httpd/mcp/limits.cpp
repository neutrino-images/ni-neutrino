/*
 * limits.cpp - how much the MCP endpoint takes
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

#include "httpd/mcp/limits.h"

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace httpd
{
namespace mcp
{

namespace
{

OpenThreads::Mutex &limitsLock()
{
	static OpenThreads::Mutex m;
	return m;
}

Limits &current()
{
	static Limits l = defaultLimits();
	return l;
}

} // namespace

Limits defaultLimits()
{
	Limits l;
	l.max_body_bytes = 64 * 1024;
	l.max_json_depth = 32;
	l.rate_burst = 30;
	l.rate_per_minute = 60;
	l.max_rate_clients = 256;
	// A zap or a wake from standby takes several seconds.
	l.call_timeout_ms = 15000;
	l.max_running_calls = 4;
	return l;
}

void setLimits(const Limits &l)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(limitsLock());
	current() = l;
}

Limits limits()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(limitsLock());
	return current();
}

} // namespace mcp
} // namespace httpd
