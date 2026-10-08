/*
 * fakevideo.h - the video groups' decoders as a case sees them
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

#ifndef __support_fakevideo_h__
#define __support_fakevideo_h__

#include "coreapi/box/apply_video.h"

#include <string>
#include <utility>
#include <vector>

/* Every call the video groups make, in order, so a case can say what one run
   sent and that it sent nothing else. */
struct FakeVideoOutput : public coreapi::VideoOutput
{
	std::vector<std::string> calls;
	std::vector<int> systems;
	std::vector<std::pair<int, int> > aspects;

	coreapi::Status setVideoSystem(int system) { calls.push_back("system"); systems.push_back(system); return coreapi::Status::Ok; }
	coreapi::Status setAnalogMode(int) { calls.push_back("analog"); return coreapi::Status::Ok; }
	coreapi::Status setAspect(int format, int mode43)
	{
		calls.push_back("aspect");
		aspects.push_back(std::make_pair(format, mode43));
		return coreapi::Status::Ok;
	}
	// What the deblocking call answers, so a case can make one send fail.
	coreapi::Status dbdr_answer;
	FakeVideoOutput() : dbdr_answer(coreapi::Status::Ok), decoder_system(-1) {}
	coreapi::Status setDbdr(int) { calls.push_back("dbdr"); return dbdr_answer; }
	coreapi::Status setAutoModes(const std::vector<int> &) { calls.push_back("automodes"); return coreapi::Status::Ok; }
	coreapi::Status setControl(int, int) { calls.push_back("control"); return coreapi::Status::Ok; }
	coreapi::Status setZappingMode(int) { calls.push_back("zapping"); return coreapi::Status::Ok; }
	coreapi::Status scaleSdOsd(int) { calls.push_back("sdosd"); return coreapi::Status::Ok; }
	coreapi::Status setHdmiColorimetry(int) { calls.push_back("colorimetry"); return coreapi::Status::Ok; }

	// What the decoder is taken to run now; below nought it cannot say.
	int decoder_system;
	coreapi::Status currentVideoSystem(int &system) const
	{
		if (decoder_system < 0)
			return coreapi::Status::NotSupported;
		system = decoder_system;
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
		systems.clear();
		aspects.clear();
	}
};

#endif
