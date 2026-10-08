/*
 * sentstate.h - what an apply group last put on the box, so a run sends only changes
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

#ifndef __coreapi_sentstate_h__
#define __coreapi_sentstate_h__

#include "coreapi/base/result.h"

#include <atomic>

namespace coreapi
{

/* A group runs for every key it holds, so a change of one key would send every
   primitive again. Where a primitive is not shown to be harmless when sent again
   with the same value, the group keeps what it last sent of it here and sends
   only what differs. Nothing is known before the first run, which is startup, so
   that one sends everything. */
template <class T>
struct Sent
{
	bool known;
	T    value;
	Sent() : known(false), value() {}
	bool differs(const T &v) const { return !known || !(value == v); }
};

/* What other writers changed and what a holder keeps for itself, one bit per
   primitive in the group's own numbering. Another writer of the same state marks
   it from any thread and the next run sends it again; a held state is left alone
   by every run and sent again once let go. Everything but mark() on the loop
   only. */
class SentFlags
{
	public:
		SentFlags() : stale(0), held(0) {}

		void mark(unsigned what) { stale.fetch_or(what); }

		// The bits marked since the last call, cleared.
		unsigned take() { return stale.exchange(0); }

		void hold(unsigned what, bool on)
		{
			if (on)
				held |= what;
			else
				held &= ~what;
		}

		bool isHeld(unsigned what) const { return (held & what) != 0; }

		void reset()
		{
			stale = 0;
			held = 0;
		}

	private:
		std::atomic<unsigned> stale;
		unsigned held;
};

// The first of several answers that was not Ok, so every call is still made.
inline void noteFirst(Status &first, Status s)
{
	if (first == Status::Ok && s != Status::Ok)
		first = s;
}

/* Sends v through call where it differs from what was last sent and nobody holds
   that state. A held one is forgotten, so it is sent again once let go. A send
   that fails is not taken as sent and is tried again by the next run. */
template <class T, class Call>
void sendChanged(Status &first, const SentFlags &flags, unsigned what, Sent<T> &sent, const T &v, Call call)
{
	if (flags.isHeld(what))
	{
		sent.known = false;
		return;
	}
	if (!sent.differs(v))
		return;
	const Status s = call();
	noteFirst(first, s);
	if (s == Status::Ok)
	{
		sent.known = true;
		sent.value = v;
	}
}

} // namespace coreapi

#endif
