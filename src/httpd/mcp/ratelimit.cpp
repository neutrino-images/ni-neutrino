/*
 * ratelimit.cpp - requests per client of the MCP endpoint
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

#include "httpd/mcp/ratelimit.h"

#include "httpd/mcp/limits.h"

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <map>
#include <string>
#include <utility>

namespace httpd
{
namespace mcp
{

namespace
{

// A request costs this many units and a second refills rate_per_minute of them: integers only.
const unsigned long kCost = 60;

struct Bucket
{
	unsigned long credit;
	time_t        last;
};

typedef std::map<std::string, Bucket> Buckets;

OpenThreads::Mutex &rateLock()
{
	static OpenThreads::Mutex m;
	return m;
}

Buckets &buckets()
{
	static Buckets b;
	return b;
}

time_t &testClock()
{
	static time_t t = 0;
	return t;
}

// Caller holds rateLock().
time_t now()
{
	const time_t t = testClock();
	return (t != 0) ? t : ::time(NULL);
}

void refill(Bucket &b, time_t t, unsigned long capacity, unsigned per_minute)
{
	if (t > b.last)
	{
		const unsigned long seconds = (unsigned long) (t - b.last);
		// Past this any rate has filled the bucket, and the product could overflow.
		if (seconds > capacity / per_minute)
			b.credit = capacity;
		else if (capacity - b.credit < seconds * per_minute)
			b.credit = capacity;
		else
			b.credit += seconds * per_minute;
	}
	// Also backwards, or a clock set back refills nothing until it catches up.
	b.last = t;
}

void forgetQuietest(Buckets &m)
{
	Buckets::iterator quietest = m.begin();
	for (Buckets::iterator it = m.begin(); it != m.end(); ++it)
	{
		if (it->second.last < quietest->second.last)
			quietest = it;
	}
	if (quietest != m.end())
		m.erase(quietest);
}

} // namespace

bool rateAllows(const std::string &client_id, unsigned *retry_after)
{
	if (retry_after != NULL)
		*retry_after = 0;
	const Limits l = limits();
	if (l.rate_per_minute == 0)
		return true;

	const unsigned long capacity = (unsigned long) l.rate_burst * kCost;

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(rateLock());
	// Read under the lock, or a late thread moves last back and a second refills twice.
	const time_t t = now();
	Buckets &m = buckets();
	Buckets::iterator it = m.find(client_id);
	if (it == m.end())
	{
		if (!m.empty() && m.size() >= l.max_rate_clients)
			forgetQuietest(m);
		Bucket fresh;
		fresh.credit = capacity;
		fresh.last = t;
		it = m.insert(std::make_pair(client_id, fresh)).first;
	}

	Bucket &b = it->second;
	refill(b, t, capacity, l.rate_per_minute);
	if (b.credit >= kCost)
	{
		b.credit -= kCost;
		return true;
	}
	if (retry_after != NULL)
		*retry_after = (unsigned) ((kCost - b.credit + l.rate_per_minute - 1) / l.rate_per_minute);
	return false;
}

void setRateClockForTest(time_t t)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(rateLock());
	testClock() = t;
}

void forgetRatesForTest()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(rateLock());
	buckets().clear();
}

size_t rateClientsForTest()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(rateLock());
	return buckets().size();
}

} // namespace mcp
} // namespace httpd
