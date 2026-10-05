/*
 * callrunner.h - tool calls with a deadline
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

#ifndef __httpd_mcp_callrunner_h__
#define __httpd_mcp_callrunner_h__

#include "httpd/mcp/contract.h"

#include "coreapi/base/result.h"

#include <cstddef>
#include <string>

namespace httpd
{
namespace mcp
{

enum class CallOutcome
{
	Done,
	TimedOut,
	Busy
};

struct CallAnswer
{
	CallOutcome    outcome;
	bool           ok;
	bool           thrown;   // nothing more is known
	JsonText       value;
	coreapi::Error error;

	CallAnswer() : outcome(CallOutcome::Done), ok(false), thrown(false) {}
};

/* One call on a thread of its own, waited for at most timeout_ms; one still running then
   finishes alone. Busy when max_running calls, abandoned ones included, are running. */
CallAnswer runCall(ToolSource *tools, const Caller &c, const std::string &name,
                   const JsonText &args, unsigned timeout_ms, unsigned max_running);

typedef void (*CallFinished)(void *cls, const CallAnswer &a);

/* As runCall, waited for on a thread of its own, which hands the answer to finished(cls, ...)
   exactly once. False when Busy; finished is never called then. */
bool startCall(ToolSource *tools, const Caller &c, const std::string &name, const JsonText &args,
               unsigned timeout_ms, unsigned max_running, CallFinished finished, void *cls);

// While closed every new call is Busy; running ones go on.
void closeCalls(bool close);

size_t runningCalls();

} // namespace mcp
} // namespace httpd

#endif
