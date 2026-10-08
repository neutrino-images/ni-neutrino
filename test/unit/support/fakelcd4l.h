/*
 * fakelcd4l.h - the LCD4Linux group's service as a case sees it
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

#ifndef __support_fakelcd4l_h__
#define __support_fakelcd4l_h__

#include "coreapi/box/apply_lcd4l.h"

#include <string>
#include <vector>

// Every call the group makes, in order, and the modes it was asked to restart with.
struct FakeLcd4lControl : public coreapi::Lcd4lControl
{
	std::vector<std::string> calls;
	std::vector<int> modes;
	coreapi::Status restart_answer;

	FakeLcd4lControl() : restart_answer(coreapi::Status::Ok) {}

	coreapi::Status restartService(int mode)
	{
		calls.push_back("restart");
		modes.push_back(mode);
		return restart_answer;
	}

	coreapi::Status reinit()
	{
		calls.push_back("reinit");
		return coreapi::Status::Ok;
	}

	coreapi::Status forceRun()
	{
		calls.push_back("force");
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
		modes.clear();
	}
};

#endif
