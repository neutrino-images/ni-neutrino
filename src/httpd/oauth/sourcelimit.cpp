/*
 * sourcelimit.cpp - how often one address may ask the anonymous OAuth endpoints
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

#include "httpd/oauth/sourcelimit.h"

#include "httpd/oauth/store.h"

#include <cstring>
#include <map>
#include <utility>

#include <arpa/inet.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace httpd
{
namespace oauth
{

namespace
{

typedef OpenThreads::ScopedLock<OpenThreads::Mutex> Held;
typedef std::pair<int, std::string> Key;

// An address is at most 45 characters; anything longer is cut, never trusted to be short.
const size_t kMaxSourceBytes = 64;

struct Window
{
	time_t   start;
	unsigned count;
};

OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

std::map<Key, Window> &table()
{
	static std::map<Key, Window> t;
	return t;
}

time_t &fixedNow()
{
	static time_t t = 0;
	return t;
}

// One holder of a /64 has every address in it, so that is the source; a mapped IPv4 address is that address.
std::string sourceKey(const std::string &source)
{
	const std::string text = source.substr(0, kMaxSourceBytes);
	unsigned char a[16];
	if (inet_pton(AF_INET6, text.c_str(), a) != 1)
		return text;
	static const unsigned char kMapped[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };
	char out[INET6_ADDRSTRLEN];
	if (std::memcmp(a, kMapped, sizeof(kMapped)) == 0)
		return inet_ntop(AF_INET, a + 12, out, sizeof(out)) != NULL ? std::string(out) : text;
	std::memset(a + 8, 0, 8);
	return inet_ntop(AF_INET6, a, out, sizeof(out)) != NULL ? std::string(out) + "/64" : text;
}

unsigned allowanceOf(Limited what)
{
	switch (what)
	{
		case Limited::Register:
			return kRegisterPerSource;
		case Limited::Authorize:
			return kAuthorizePerSource;
		case Limited::Consent:
			return kConsentPerSource;
	}
	return 0;
}

void makeRoomLocked(time_t now)
{
	std::map<Key, Window> &t = table();
	for (std::map<Key, Window>::iterator it = t.begin(); it != t.end();)
	{
		if (now < it->second.start || now - it->second.start >= kSourceWindow)
			t.erase(it++);
		else
			++it;
	}
	if (t.size() < kMaxSources)
		return;
	std::map<Key, Window>::iterator oldest = t.begin();
	for (std::map<Key, Window>::iterator it = t.begin(); it != t.end(); ++it)
	{
		if (it->second.start < oldest->second.start)
			oldest = it;
	}
	t.erase(oldest);
}

} // namespace

bool sourceAllows(Limited what, const std::string &source, unsigned *retry_after)
{
	if (retry_after != NULL)
		*retry_after = 0;
	Held h(lock());
	const time_t now = fixedNow() != 0 ? fixedNow() : realClock();
	const Key key((int) what, sourceKey(source));
	std::map<Key, Window>::iterator it = table().find(key);
	if (it != table().end() && (now < it->second.start || now - it->second.start >= kSourceWindow))
	{
		table().erase(it);
		it = table().end();
	}
	if (it == table().end())
	{
		if (table().size() >= kMaxSources)
			makeRoomLocked(now);
		const Window w = { now, 0 };
		it = table().insert(std::make_pair(key, w)).first;
	}
	if (it->second.count >= allowanceOf(what))
	{
		if (retry_after != NULL)
			*retry_after = (unsigned) (kSourceWindow - (now - it->second.start));
		return false;
	}
	++it->second.count;
	return true;
}

void setSourceClockForTest(time_t now)
{
	Held h(lock());
	fixedNow() = now;
}

void forgetSourceLimitsForTest()
{
	Held h(lock());
	table().clear();
}

size_t sourceCountForTest()
{
	Held h(lock());
	return table().size();
}

} // namespace oauth
} // namespace httpd
