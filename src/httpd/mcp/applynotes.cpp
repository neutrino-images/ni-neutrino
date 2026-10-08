/*
 * applynotes.cpp - settings an AI client wrote that the box could not put in force
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

#include "httpd/mcp/applynotes.h"

#include "httpd/events.h"

#include "coreapi/base/eventbus.h"

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <cstdio>
#include <deque>
#include <utility>

namespace httpd
{
namespace mcp
{

namespace
{

// A client that never calls again must not grow this.
const size_t kMaxConnections = 32;
const size_t kMaxLinesEach = 8;

typedef std::pair<std::string, std::deque<std::string> > Pending;

OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

std::deque<Pending> &pending()
{
	static std::deque<Pending> p;
	return p;
}

void keep(const std::string &connection, const std::string &line)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	std::deque<Pending> &all = pending();
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].first != connection)
			continue;
		all[i].second.push_back(line);
		if (all[i].second.size() > kMaxLinesEach)
			all[i].second.pop_front();
		return;
	}
	if (all.size() >= kMaxConnections)
		all.pop_front();
	all.push_back(Pending(connection, std::deque<std::string>(1, line)));
}

class Watcher : public coreapi::Subscriber
{
	public:
		void onEvent(const coreapi::Event &e)
		{
			if (e.type != coreapi::EventType::SettingApplyFailed)
				return;
			std::string keys = e.text;
			for (size_t i = 0; i < keys.size(); ++i)
				keys[i] = keys[i] == ' ' ? ',' : keys[i];
			char status[16];
			std::snprintf(status, sizeof(status), "%d", e.value);
			const std::string line = "- " + keys + ": " + events::applyFailureDetail(e.value) + " (" + status + ")";
			// Only the AI clients that wrote it; a web session or the box menu is never named here.
			for (size_t at = 0; at < e.initiator.size();)
			{
				size_t end = e.initiator.find(' ', at);
				if (end == std::string::npos)
					end = e.initiator.size();
				if (end - at > 4 && e.initiator.compare(at, 4, "mcp:") == 0)
					keep(e.initiator.substr(at + 4, end - at - 4), line);
				at = end + 1;
			}
		}
};

} // namespace

void watchApplyFailures()
{
	// Never freed: the bus may still deliver while the process ends.
	static Watcher *watcher = NULL;
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	if (watcher != NULL)
		return;
	watcher = new Watcher;
	coreapi::EventBus::instance().subscribe(watcher);
}

std::string takeApplyNote(const std::string &connection)
{
	if (connection.empty())
		return std::string();
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	std::deque<Pending> &all = pending();
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].first != connection)
			continue;
		std::string note = "Note: settings this connection wrote earlier were stored, but the box could not put "
		                   "them in force; the stored value is kept and the next write of it tries again.";
		for (size_t k = 0; k < all[i].second.size(); ++k)
			note += "\n" + all[i].second[k];
		all.erase(all.begin() + i);
		return note;
	}
	return std::string();
}

void forgetApplyNotesForTest()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	pending().clear();
}

} // namespace mcp
} // namespace httpd
