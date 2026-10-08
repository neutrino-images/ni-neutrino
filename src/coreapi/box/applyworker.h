/*
 * applyworker.h - where a group's slow work runs, off the program's loop
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

#ifndef __coreapi_applyworker_h__
#define __coreapi_applyworker_h__

#include "coreapi/base/result.h"
#include "coreapi/box/sentstate.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>

#include <pthread.h>

namespace coreapi
{

/* Runs what a group would otherwise do on the program's loop and may take long: a
   service script, hdparm on a sleeping disk, the restart of a display service. A web
   write runs its group on the loop, and the box must not stand still for it.

   One job at a time, in the order posted, on a thread that ends when nothing is
   left. A job posted under the slot of one that has not started replaces it at the
   back of the queue, so only the newest request for the same thing is carried out,
   and after what was asked for before it. */
class ApplyWorker
{
public:
	typedef std::function<void()> Job;

	ApplyWorker();
	~ApplyWorker();

	/* False when the job will not run: the worker is closed, or no thread could be made.
	   dropped runs instead of the job when close() throws it away before it started;
	   it must not call the worker. */
	bool post(const std::string &slot, const Job &job, const Job &dropped = Job());
	// Returns once nothing runs and nothing waits.
	void wait();
	/* Whether a job whose slot begins with prefix runs or waits, and the wait for
	   none to be left: a screen does not wait for work queued after its own. The
	   queue is one line, so work queued ahead of it, a disk spinning up for somebody
	   else, is still waited for. */
	bool pending(const std::string &prefix);
	void waitFor(const std::string &prefix);
	/* Drops what waits, waits for what runs and refuses every later post. For the
	   end of the program, ahead of tearing down what a job reaches. A job still
	   running after seconds is not waited for any longer, so a hung script cannot
	   hold up a shutdown; false says so. A close of a worker closed already does not
	   wait. */
	bool close(unsigned seconds = 10);
	// Takes posts again. For a test.
	void reopen();

private:
	static void *main(void *arg);
	void loop();
	bool pendingLocked(const std::string &prefix) const;
	void joinLocked();

	struct Entry
	{
		std::string slot;
		Job         job;
		Job         dropped;
	};

	std::mutex              m_;
	std::condition_variable idle_;
	std::deque<Entry>       queue_;
	// The slot of the job that runs, empty between jobs.
	std::string             current_;
	pthread_t               thread_;
	bool                    joinable_;
	bool                    running_;
	bool                    closed_;
};

/* The program's one worker. Never destroyed, so a job still running when the
   process ends is not waited for. */
ApplyWorker &applyWorker();

/* The HTTP status a write route answers s with, which is the value an apply failure
   event carries. */
int applyFailureCode(Status s);

/* Says on the event bus that the job for keys, separated by spaces, failed with s. A
   write that queued it was answered ok before it ran, and nothing else tells the
   writer; the next run of the group sends it again. initiator is Event::initiator. */
void publishApplyFailed(const std::string &keys, Status s, const std::string &initiator);

/* Who started the writes the group run in progress puts in force, as Event::initiator
   names it: "box" unless a drain of written settings says otherwise. Read and set on
   the loop only, where every group runs. */
const std::string &applyInitiator();

class ApplyInitiatorScope
{
public:
	explicit ApplyInitiatorScope(const std::string &who);
	~ApplyInitiatorScope();

private:
	ApplyInitiatorScope(const ApplyInitiatorScope &);
	ApplyInitiatorScope &operator=(const ApplyInitiatorScope &);
	std::string was_;
};

/* sendChanged for a send that runs on the worker. v is taken as sent once the job
   is queued, and the run that superseded a waiting job has already recorded the
   newer value. A job that fails marks what in flags from the worker's thread, and
   the next run, which takes the marks first, sends it again. A job that cannot be
   queued is a failed send. keys are the settings the job puts in force. */
template <class T, class Call>
void postChanged(Status &first, SentFlags &flags, unsigned what, Sent<T> &sent, const T &v,
		 const std::string &slot, const std::string &keys, Call call)
{
	if (flags.isHeld(what))
	{
		sent.known = false;
		return;
	}
	if (!sent.differs(v))
		return;
	SentFlags *marks = &flags;
	const std::string who = applyInitiator();
	// A job dropped by close() never sent v, so the next run sends it as after a failure.
	const bool queued = applyWorker().post(slot, [marks, what, keys, who, call]()
	{
		const Status done = call();
		if (done == Status::Ok)
			return;
		marks->mark(what);
		publishApplyFailed(keys, done, who);
	}, [marks, what]() { marks->mark(what); });
	if (!queued)
	{
		noteFirst(first, Status::Internal);
		return;
	}
	sent.known = true;
	sent.value = v;
}

} // namespace coreapi

#endif
