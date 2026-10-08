/*
 * fakeservices.h - the services group's seam, recording what it was told
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

#ifndef __support_fakeservices_h__
#define __support_fakeservices_h__

#include "coreapi/box/apply_services.h"

#include <functional>
#include <string>
#include <vector>

// Every start and stop in order, as "start:name" and "stop:name", a softcam as "start:camd:name".
struct FakeServiceControl : public coreapi::ServiceControl
{
	std::vector<std::string> calls;
	coreapi::Status answer;
	// The flag files that are there, by name.
	std::vector<std::string> flags;
	// The programs that are installed, by the name the group looks them up by.
	std::vector<std::string> present;
	// Called first in every start and stop, on the thread that makes it.
	std::function<void()> before;

	FakeServiceControl() : answer(coreapi::Status::Ok) {}

	bool flagIsSet(const char *name) const
	{
		for (size_t i = 0; i < flags.size(); ++i)
			if (flags[i] == name)
				return true;
		return false;
	}

	bool installed(const char *program, bool) const
	{
		for (size_t i = 0; i < present.size(); ++i)
			if (present[i] == program)
				return true;
		return false;
	}

	void set(const char *name, bool on)
	{
		for (size_t i = 0; i < flags.size(); ++i)
			if (flags[i] == name)
				flags.erase(flags.begin() + i--);
		if (on)
			flags.push_back(name);
	}

	coreapi::Status start(const char *name, bool softcam)
	{
		if (before)
			before();
		calls.push_back(std::string("start:") + (softcam ? "camd:" : "") + name);
		return answer;
	}
	coreapi::Status stop(const char *name, bool softcam)
	{
		if (before)
			before();
		calls.push_back(std::string("stop:") + (softcam ? "camd:" : "") + name);
		return answer;
	}
};

#endif
