/*
 * apply.cpp - what makes a changed setting take effect, held in one place
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

#include "coreapi/base/apply.h"
#include "coreapi/settings/settings.h"

#include <algorithm>
#include <cstdio>

#include <pthread.h>

namespace coreapi
{

namespace
{

/* No lock: groups are registered while the program starts and changes arrive on
   the loop that owns the settings. A lock held across run() would also be one
   a driver's own callback could wait on. */
std::vector<const ApplyGroup *> &groups()
{
	static std::vector<const ApplyGroup *> list;
	return list;
}

bool (&reached())[static_cast<size_t>(ApplyPhase::Count)]
{
	static bool flags[static_cast<size_t>(ApplyPhase::Count)] = { false };
	return flags;
}

bool phaseReached(ApplyPhase p)
{
	return reached()[static_cast<size_t>(p)];
}

bool &loopBound()
{
	static bool bound = false;
	return bound;
}

pthread_t &loopThread()
{
	static pthread_t thread;
	return thread;
}

bool holdsKey(const ApplyGroup &g, const std::string &key)
{
	for (size_t i = 0; i < g.key_count; ++i)
	{
		if (g.keys[i] != NULL && key == g.keys[i])
			return true;
	}
	return false;
}

} // namespace

namespace
{

// The registration itself; why is what a refusal is reported with.
Status admit(const ApplyGroup *g, const char *&why)
{
	if (g == NULL || g->name == NULL || g->run == NULL || g->phase >= ApplyPhase::Count
	    || (g->key_count > 0 && g->keys == NULL))
	{
		why = "it is not a whole group";
		return Status::InvalidArgument;
	}

	if (phaseReached(g->phase))
	{
		why = "its phase was reached already";
		return Status::InvalidArgument;
	}

	std::vector<const ApplyGroup *> &list = groups();
	for (size_t k = 0; k < g->key_count; ++k)
	{
		if (g->keys[k] == NULL)
		{
			why = "it lists a key that is no key";
			return Status::InvalidArgument;
		}
		// Within the new group as well: a key listed twice would be one a later
		// lookup could not tell from a clash.
		for (size_t j = 0; j < k; ++j)
		{
			if (std::string(g->keys[j]) == g->keys[k])
			{
				why = "it lists a key twice";
				return Status::Conflict;
			}
		}
		for (size_t i = 0; i < list.size(); ++i)
		{
			if (holdsKey(*list[i], g->keys[k]))
			{
				why = "another group holds one of its keys";
				return Status::Conflict;
			}
		}
	}

	list.push_back(g);
	return Status::Ok;
}

} // namespace

Status registerApplyGroup(const ApplyGroup *g)
{
	/* Said here because the callers register a fixed list at startup and look at no answer:
	   a refused group is every key of it landing with nothing to put it in force. */
	const char *why = "";
	const Status s = admit(g, why);
	if (s != Status::Ok)
		std::fprintf(stderr, "coreapi: apply group %s was refused: %s\n",
			     g != NULL && g->name != NULL ? g->name : "(unnamed)", why);
	return s;
}

const ApplyGroup *groupOf(const std::string &key)
{
	const std::vector<const ApplyGroup *> &list = groups();
	for (size_t i = 0; i < list.size(); ++i)
	{
		if (holdsKey(*list[i], key))
			return list[i];
	}
	return NULL;
}

std::vector<const ApplyGroup *> applyGroups()
{
	return groups();
}

namespace
{

Status runGroup(const ApplyGroup &g)
{
	if (!phaseReached(g.phase))
		return Status::Busy;
	return g.run();
}

// A group that failed is named here and nowhere else, so the name is the only
// trace a startup failure leaves.
void noteFailure(const ApplyGroup &g, Status s, Status &first)
{
	std::fprintf(stderr, "coreapi: apply group %s failed\n", g.name);
	if (first == Status::Ok)
		first = s;
}

} // namespace

namespace
{

/* A row only a restart applies runs no group once the program is up, however it was
   changed: the drain, a key, a batch and a loaded file all follow this one rule. Starting
   up is the other way round and runs every group of the phase, as it must. */
bool appliedByRestart(const std::string &key)
{
	const Descriptor *d = settings::findRow(key);
	return d != NULL && d->needs_restart;
}

} // namespace

Status applyKey(const std::string &key)
{
	const ApplyGroup *g = groupOf(key);
	if (g == NULL || appliedByRestart(key))
		return Status::Ok;
	return runGroup(*g);
}

namespace
{
// A pointer, so the thread local needs no constructor of its own.
thread_local const std::string *g_writer = NULL;
}

const std::string &currentWriter()
{
	static const std::string none;
	return g_writer != NULL ? *g_writer : none;
}

WriterScope::WriterScope(const std::string &who) : who_(who), was_(g_writer)
{
	g_writer = &who_;
}

WriterScope::~WriterScope()
{
	g_writer = was_;
}

void bindApplyLoop()
{
	loopThread() = pthread_self();
	loopBound() = true;
}

bool onApplyLoop()
{
	return !loopBound() || pthread_equal(loopThread(), pthread_self());
}

Status applyBatch(const std::vector<std::string> &keys)
{
	// The registry is unlocked, so a call from another thread is not made at all.
	if (!onApplyLoop())
	{
		std::fprintf(stderr, "coreapi: applying settings was asked on a thread that is not the loop and was refused\n");
		return Status::Denied;
	}
	Status first = Status::Ok;
	std::vector<const ApplyGroup *> done;
	for (size_t i = 0; i < keys.size(); ++i)
	{
		const ApplyGroup *g = groupOf(keys[i]);
		if (g == NULL || appliedByRestart(keys[i]) || std::find(done.begin(), done.end(), g) != done.end())
			continue;
		done.push_back(g);
		const Status s = runGroup(*g);
		if (s != Status::Ok && s != Status::Busy)
			noteFailure(*g, s, first);
	}
	return first;
}

Status runPhase(ApplyPhase p)
{
	if (p >= ApplyPhase::Count)
		return Status::InvalidArgument;
	Status first = Status::Ok;
	reached()[static_cast<size_t>(p)] = true;

	// A copy, so a run() that registers nothing today cannot invalidate the walk
	// if one ever does.
	const std::vector<const ApplyGroup *> list = groups();
	for (size_t i = 0; i < list.size(); ++i)
	{
		if (list[i]->phase != p)
			continue;
		const Status s = list[i]->run();
		if (s != Status::Ok)
			noteFailure(*list[i], s, first);
	}
	return first;
}

void resetApplyRegistry()
{
	loopBound() = false;
	groups().clear();
	for (size_t i = 0; i < static_cast<size_t>(ApplyPhase::Count); ++i)
		reached()[i] = false;
}

} // namespace coreapi
