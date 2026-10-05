/*
 * callrunner.cpp - tool calls with a deadline
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

#include "httpd/mcp/callrunner.h"

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <cerrno>
#include <new>

#include <pthread.h>
#include <time.h>

namespace httpd
{
namespace mcp
{

namespace
{

OpenThreads::Mutex &countLock()
{
	static OpenThreads::Mutex m;
	return m;
}

size_t &running()
{
	static size_t n = 0;
	return n;
}

bool &closed()
{
	static bool c = false;
	return c;
}

void finished()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(countLock());
	--running();
}

// Held by the request and by the worker; whichever lets go last frees it.
struct Job
{
	pthread_mutex_t mutex;
	pthread_cond_t  cond;
	int             refs;
	bool            done;
	ToolSource     *tools;
	Caller          caller;
	std::string     name;
	JsonText        args;
	CallAnswer      answer;
};

void release(Job *j)
{
	pthread_mutex_lock(&j->mutex);
	const bool last = (--j->refs == 0);
	pthread_mutex_unlock(&j->mutex);
	if (!last)
		return;
	pthread_cond_destroy(&j->cond);
	pthread_mutex_destroy(&j->mutex);
	delete j;
}

void callInto(Job *j)
{
	const coreapi::Result<JsonText> r = j->tools->call(j->caller, j->name, j->args);
	if (r.ok())
	{
		j->answer.ok = true;
		j->answer.value = r.value();
	}
	else
	{
		j->answer.error = r.error();
	}
}

void *work(void *cls)
{
	Job *j = (Job *) cls;
	try
	{
		callInto(j);
	}
	catch (...)
	{
		j->answer.ok = false;
		j->answer.thrown = true;
	}

	finished();

	pthread_mutex_lock(&j->mutex);
	j->done = true;
	pthread_cond_signal(&j->cond);
	pthread_mutex_unlock(&j->mutex);

	release(j);
	return NULL;
}

struct timespec deadlineAfter(unsigned ms)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	t.tv_sec += (time_t) (ms / 1000);
	t.tv_nsec += (long) (ms % 1000) * 1000000L;
	if (t.tv_nsec >= 1000000000L)
	{
		t.tv_sec += 1;
		t.tv_nsec -= 1000000000L;
	}
	return t;
}

bool admit(unsigned max_running)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(countLock());
	if (closed() || running() >= max_running)
		return false;
	++running();
	return true;
}

// Only after admit(); gives the place back however it ends.
CallAnswer await(ToolSource *tools, const Caller &c, const std::string &name,
                 const JsonText &args, unsigned timeout_ms)
{
	CallAnswer busy;
	busy.outcome = CallOutcome::Busy;

	Job *j = new (std::nothrow) Job;
	if (j == NULL)
	{
		finished();
		return busy;
	}
	try
	{
		j->tools = tools;
		j->caller = c;
		j->name = name;
		j->args = args;
	}
	catch (...)
	{
		delete j;
		finished();
		throw;
	}
	j->refs = 2;
	j->done = false;
	pthread_mutex_init(&j->mutex, NULL);
	pthread_condattr_t ca;
	pthread_condattr_init(&ca);
	// The wait must not stretch or shrink when the box sets its clock.
	pthread_condattr_setclock(&ca, CLOCK_MONOTONIC);
	pthread_cond_init(&j->cond, &ca);
	pthread_condattr_destroy(&ca);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_t id;
	const int created = pthread_create(&id, &attr, &work, j);
	pthread_attr_destroy(&attr);
	if (created != 0)
	{
		j->refs = 1;
		release(j);
		finished();
		return busy;
	}

	const struct timespec deadline = deadlineAfter(timeout_ms);
	pthread_mutex_lock(&j->mutex);
	int waited = 0;
	while (!j->done && waited == 0)
		waited = pthread_cond_timedwait(&j->cond, &j->mutex, &deadline);
	const bool done = j->done;
	pthread_mutex_unlock(&j->mutex);

	CallAnswer out;
	out.outcome = CallOutcome::TimedOut;
	if (done)
	{
		try
		{
			out = j->answer;
		}
		catch (...)
		{
			release(j);
			throw;
		}
	}
	release(j);
	return out;
}

struct Waiter
{
	ToolSource   *tools;
	Caller        caller;
	std::string   name;
	JsonText      args;
	unsigned      timeout_ms;
	CallFinished  finished;
	void         *cls;
};

void *waitFor(void *cls)
{
	Waiter *w = (Waiter *) cls;
	CallAnswer out;
	try
	{
		out = await(w->tools, w->caller, w->name, w->args, w->timeout_ms);
	}
	catch (...)
	{
		out = CallAnswer();
		out.thrown = true;
	}
	w->finished(w->cls, out);
	delete w;
	return NULL;
}

} // namespace

CallAnswer runCall(ToolSource *tools, const Caller &c, const std::string &name,
                   const JsonText &args, unsigned timeout_ms, unsigned max_running)
{
	if (!admit(max_running))
	{
		CallAnswer busy;
		busy.outcome = CallOutcome::Busy;
		return busy;
	}
	return await(tools, c, name, args, timeout_ms);
}

bool startCall(ToolSource *tools, const Caller &c, const std::string &name, const JsonText &args,
               unsigned timeout_ms, unsigned max_running, CallFinished done, void *cls)
{
	if (!admit(max_running))
		return false;

	Waiter *w = new (std::nothrow) Waiter;
	if (w == NULL)
	{
		finished();
		return false;
	}
	try
	{
		w->tools = tools;
		w->caller = c;
		w->name = name;
		w->args = args;
	}
	catch (...)
	{
		delete w;
		finished();
		return false;
	}
	w->timeout_ms = timeout_ms;
	w->finished = done;
	w->cls = cls;

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_t id;
	const int created = pthread_create(&id, &attr, &waitFor, w);
	pthread_attr_destroy(&attr);
	if (created != 0)
	{
		delete w;
		finished();
		return false;
	}
	return true;
}

void closeCalls(bool close)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(countLock());
	closed() = close;
}

size_t runningCalls()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(countLock());
	return running();
}

} // namespace mcp
} // namespace httpd
