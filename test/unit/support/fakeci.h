/*
 * fakeci.h - the ci group's seam, recording what it was told
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

#ifndef __support_fakeci_h__
#define __support_fakeci_h__

#include "coreapi/box/apply_ci.h"

#include <string>
#include <utility>
#include <vector>

/* Every call the ci group makes, in order, with the numbers it carried, so a case
   can say what one run sent and that it sent nothing else. */
struct FakeCiControl : public coreapi::CiControl
{
	std::vector<std::string> calls;
	std::vector<std::pair<int, int> > args;

	coreapi::Status note(const char *what, int a, int b)
	{
		calls.push_back(what);
		args.push_back(std::make_pair(a, b));
		return coreapi::Status::Ok;
	}

	// What the clock call answers, so a case can make one send fail.
	coreapi::Status clock_answer;
	FakeCiControl() : clock_answer(coreapi::Status::Ok) {}

	coreapi::Status setClock(int slot, int mhz)
	{
		note("clock", slot, mhz);
		return clock_answer;
	}
	coreapi::Status setDelay(int delay) { return note("delay", delay, 0); }
	coreapi::Status setRelevantPidsRouting(int slot, int on) { return note("rpr", slot, on); }
	coreapi::Status setOperator(int slot, int on) { return note("op", slot, on); }
	coreapi::Status setCheckLiveSlot(int on) { return note("check", on, 0); }
	coreapi::Status setTuner(int tuner) { return note("tuner", tuner, 0); }

	size_t count(const std::string &what) const
	{
		size_t n = 0;
		for (size_t i = 0; i < calls.size(); ++i)
			n += (calls[i] == what) ? 1 : 0;
		return n;
	}

	void forget()
	{
		calls.clear();
		args.clear();
	}
};

#endif
