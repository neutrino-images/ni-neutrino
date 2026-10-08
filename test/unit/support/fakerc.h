/*
 * fakerc.h - the remote control group's input driver as a case sees it
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

#ifndef __support_fakerc_h__
#define __support_fakerc_h__

#include "coreapi/box/apply_keys.h"

#include <string>
#include <utility>
#include <vector>

/* Every call the remote control group makes, in order, so a case can say what
   one run sent and that it sent nothing else. */
struct FakeRcControl : public coreapi::RcControl
{
	std::vector<std::string> calls;
	std::vector<std::pair<int, int> > repeats;
	// What the repeat call answers, so a case can make one send fail.
	coreapi::Status repeat_answer;

	FakeRcControl() : repeat_answer(coreapi::Status::Ok) {}

	coreapi::Status setRepeat(int block_ms, int generic_ms)
	{
		calls.push_back("repeat");
		repeats.push_back(std::make_pair(block_ms, generic_ms));
		return repeat_answer;
	}

	coreapi::Status selectHardware()
	{
		calls.push_back("hardware");
		return coreapi::Status::Ok;
	}

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
		repeats.clear();
	}
};

#endif
