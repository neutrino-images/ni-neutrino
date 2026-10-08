/*
 * applyworker.cpp - where a group's slow work runs, off the program's loop
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

#include "coreapi/box/applyworker.h"
#include "coreapi/base/eventbus.h"

#include <stdio.h>
#include <string.h>

#include <chrono>

namespace coreapi
{

ApplyWorker::ApplyWorker() : thread_(), joinable_(false), running_(false), closed_(false) {}

ApplyWorker::~ApplyWorker()
{
	wait();
}

bool ApplyWorker::post(const std::string &slot, const Job &job, const Job &dropped)
{
	std::lock_guard<std::mutex> lock(m_);
	if (closed_)
		return false;
	for (size_t i = 0; i < queue_.size(); ++i)
	{
		if (queue_[i].slot == slot)
		{
			queue_.erase(queue_.begin() + i);
			break;
		}
	}
	Entry e;
	e.slot = slot;
	e.job = job;
	e.dropped = dropped;
	queue_.push_back(e);
	if (running_)
		return true;
	// The last thread has left its loop, so this does not wait.
	if (joinable_)
	{
		pthread_join(thread_, NULL);
		joinable_ = false;
	}
	// No thread runs, so the queue held nothing before this job.
	const int err = pthread_create(&thread_, NULL, &ApplyWorker::main, this);
	if (err != 0)
	{
		queue_.clear();
		printf("[apply] no thread for %s: %s\n", slot.c_str(), strerror(err));
		return false;
	}
	joinable_ = true;
	running_ = true;
	return true;
}

void *ApplyWorker::main(void *arg)
{
	static_cast<ApplyWorker *>(arg)->loop();
	return NULL;
}

void ApplyWorker::loop()
{
	for (;;)
	{
		Job job;
		{
			std::lock_guard<std::mutex> lock(m_);
			current_.clear();
			idle_.notify_all();
			if (queue_.empty())
			{
				running_ = false;
				idle_.notify_all();
				return;
			}
			job = queue_.front().job;
			current_ = queue_.front().slot;
			queue_.pop_front();
		}
		job();
	}
}

void ApplyWorker::joinLocked()
{
	if (joinable_)
	{
		pthread_join(thread_, NULL);
		joinable_ = false;
	}
}

void ApplyWorker::wait()
{
	std::unique_lock<std::mutex> lock(m_);
	idle_.wait(lock, [this]() { return !running_; });
	joinLocked();
}

bool ApplyWorker::pendingLocked(const std::string &prefix) const
{
	if (!current_.empty() && current_.compare(0, prefix.size(), prefix) == 0)
		return true;
	for (size_t i = 0; i < queue_.size(); ++i)
		if (queue_[i].slot.compare(0, prefix.size(), prefix) == 0)
			return true;
	return false;
}

bool ApplyWorker::pending(const std::string &prefix)
{
	std::lock_guard<std::mutex> lock(m_);
	return pendingLocked(prefix);
}

void ApplyWorker::waitFor(const std::string &prefix)
{
	std::unique_lock<std::mutex> lock(m_);
	idle_.wait(lock, [this, &prefix]() { return !pendingLocked(prefix); });
}

bool ApplyWorker::close(unsigned seconds)
{
	std::unique_lock<std::mutex> lock(m_);
	// Closed already: the first close waited its deadline, a second one does not wait again.
	if (closed_)
		seconds = 0;
	closed_ = true;
	std::deque<Entry> left;
	left.swap(queue_);
	lock.unlock();
	for (size_t i = 0; i < left.size(); ++i)
		if (left[i].dropped)
			left[i].dropped();
	lock.lock();
	if (!idle_.wait_for(lock, std::chrono::seconds(seconds), [this]() { return !running_; }))
	{
		printf("[apply] %s still runs after %u s, not waited for\n", current_.c_str(), seconds);
		return false;
	}
	joinLocked();
	return true;
}

void ApplyWorker::reopen()
{
	std::lock_guard<std::mutex> lock(m_);
	closed_ = false;
}

int applyFailureCode(Status s)
{
	switch (s)
	{
		case Status::Ok:              return 200;
		case Status::NotFound:        return 404;
		case Status::InvalidArgument: return 400;
		case Status::Conflict:        return 409;
		case Status::NotSupported:    return 501;
		case Status::Busy:            return 409;
		case Status::Denied:          return 403;
		case Status::Internal:        return 500;
	}
	return 500;
}

void publishApplyFailed(const std::string &keys, Status s, const std::string &initiator)
{
	printf("[apply] %s not put in force\n", keys.c_str());
	Event e;
	e.type = EventType::SettingApplyFailed;
	e.value = applyFailureCode(s);
	e.text = keys;
	e.initiator = initiator;
	EventBus::instance().publish(e);
}

namespace
{
std::string &initiator()
{
	static std::string *who = new std::string("box");
	return *who;
}
}

const std::string &applyInitiator() { return initiator(); }

ApplyInitiatorScope::ApplyInitiatorScope(const std::string &who) : was_(initiator()) { initiator() = who; }

ApplyInitiatorScope::~ApplyInitiatorScope() { initiator() = was_; }

ApplyWorker &applyWorker()
{
	static ApplyWorker *worker = new ApplyWorker();
	return *worker;
}

} // namespace coreapi
