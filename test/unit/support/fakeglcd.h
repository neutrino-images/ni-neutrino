/*
 * fakeglcd.h - the graphic display group's service as a case sees it
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

#ifndef __support_fakeglcd_h__
#define __support_fakeglcd_h__

#include "coreapi/box/apply_glcd.h"

#include <string>
#include <vector>

// Every call the group makes, in order, with the switch values it carried.
struct FakeGlcdPanel : public coreapi::GlcdPanel
{
	std::vector<std::string> calls;
	std::vector<int> switched;
	coreapi::Status respawn_answer;
	coreapi::Status size_answer;
	int width, height;

	FakeGlcdPanel() : respawn_answer(coreapi::Status::Ok), size_answer(coreapi::Status::Ok), width(128), height(64), sizes(0) {}

	coreapi::Status setEnabled(bool on)
	{
		calls.push_back("enable");
		switched.push_back(on ? 1 : 0);
		return coreapi::Status::Ok;
	}

	coreapi::Status setMirrorOsd(bool on)
	{
		calls.push_back("mirror");
		switched.push_back(on ? 1 : 0);
		return coreapi::Status::Ok;
	}

	coreapi::Status respawn()
	{
		calls.push_back("respawn");
		return respawn_answer;
	}

	coreapi::Status reinitFont()
	{
		calls.push_back("font");
		return coreapi::Status::Ok;
	}

	coreapi::Status updateBrightness()
	{
		calls.push_back("brightness");
		return coreapi::Status::Ok;
	}

	coreapi::Status update()
	{
		calls.push_back("update");
		return coreapi::Status::Ok;
	}

	int sizes;

	coreapi::Status recordPanelSize()
	{
		++sizes;
		return coreapi::Status::Ok;
	}

	coreapi::Status panelSize(int &w, int &h)
	{
		w = width;
		h = height;
		return size_answer;
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
		switched.clear();
	}
};

#endif
