/*
 * fakeosd.h - the OSD groups' drawing objects as a case sees them
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

#ifndef __support_fakeosd_h__
#define __support_fakeosd_h__

#include "coreapi/box/apply_osd.h"

#include <string>
#include <vector>

/* Every call the OSD groups make, in order, so a case can say what one run
   sent and that it sent nothing else. */
struct FakeOsdOutput : public coreapi::OsdOutput
{
	std::vector<std::string> calls;
	std::vector<coreapi::FontSetup> fonts;

	// What the font rebuild answers, so a case can make one send fail.
	coreapi::Status fonts_answer;

	// What the clock redraw answers, so a case can make one fail.
	coreapi::Status clock_answer;

	FakeOsdOutput() : fonts_answer(coreapi::Status::Ok), clock_answer(coreapi::Status::Ok) {}

	coreapi::Status setPalette()
	{
		calls.push_back("palette");
		return coreapi::Status::Ok;
	}

	coreapi::Status setScreenGeometry()
	{
		calls.push_back("geometry");
		return coreapi::Status::Ok;
	}

	coreapi::Status resetLcd4lParse()
	{
		calls.push_back("lcd4lparse");
		return coreapi::Status::Ok;
	}

	coreapi::Status resetInfoViewer()
	{
		calls.push_back("infoviewer");
		return coreapi::Status::Ok;
	}

	coreapi::Status clearInfoClock()
	{
		calls.push_back("clock");
		return clock_answer;
	}

	coreapi::Status refreshVolumeBar()
	{
		calls.push_back("volume");
		return coreapi::Status::Ok;
	}

	coreapi::Status refreshMuteIcon()
	{
		calls.push_back("mute");
		return coreapi::Status::Ok;
	}

	coreapi::Status resetRadioText()
	{
		calls.push_back("radiotext");
		return coreapi::Status::Ok;
	}

	coreapi::Status resetChannelList()
	{
		calls.push_back("channellist");
		return coreapi::Status::Ok;
	}

	coreapi::Status resetInfoIcons()
	{
		calls.push_back("infoicons");
		return coreapi::Status::Ok;
	}

	coreapi::Status clearIconCache()
	{
		calls.push_back("iconcache");
		return coreapi::Status::Ok;
	}

	coreapi::Status setupFonts(coreapi::FontSetup what)
	{
		calls.push_back("fonts");
		fonts.push_back(what);
		return fonts_answer;
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
		fonts.clear();
	}
};

#endif
