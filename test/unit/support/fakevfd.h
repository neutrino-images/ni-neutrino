/*
 * fakevfd.h - the front panel group's driver as a case sees it
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

#ifndef __support_fakevfd_h__
#define __support_fakevfd_h__

#include "coreapi/box/apply_vfd.h"

#include <string>
#include <utility>
#include <vector>

// Every call the group makes, in order, with what it carried.
struct FakeVfdPanel : public coreapi::VfdPanel
{
	std::vector<std::string> calls;
	std::vector<std::pair<int, int> > brightness;
	std::vector<int> scrolls;
	std::vector<int> backlights;
	std::vector<std::pair<int, int> > statuslines;
	coreapi::Status brightness_answer;

	FakeVfdPanel() : brightness_answer(coreapi::Status::Ok) {}

	coreapi::Status setBrightness(Brightness which, int value)
	{
		calls.push_back("brightness");
		brightness.push_back(std::make_pair((int) which, value));
		return brightness_answer;
	}

	coreapi::Status setScrollMode(int repeats)
	{
		calls.push_back("scroll");
		scrolls.push_back(repeats);
		return coreapi::Status::Ok;
	}

	coreapi::Status refreshLeds()
	{
		calls.push_back("leds");
		return coreapi::Status::Ok;
	}

	coreapi::Status refreshParameters()
	{
		calls.push_back("parameters");
		return coreapi::Status::Ok;
	}

	coreapi::Status setBacklight(int on)
	{
		calls.push_back("backlight");
		backlights.push_back(on);
		return coreapi::Status::Ok;
	}

	coreapi::Status showStatusline(int mode, int volume)
	{
		calls.push_back("statusline");
		statuslines.push_back(std::make_pair(mode, volume));
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
		brightness.clear();
		scrolls.clear();
		backlights.clear();
		statuslines.clear();
	}
};

#endif
