/*
 * fakeaudio.h - an audio seam that records what the groups send
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

#ifndef __support_fakeaudio_h__
#define __support_fakeaudio_h__

#include "coreapi/box/apply_audio.h"

#include <string>
#include <utility>
#include <vector>

/* Every call the audio groups make, in order, with the number each carried, so
   a case can say what one run sent and that it sent nothing else. */
struct FakeAudioOutput : public coreapi::AudioOutput
{
	std::vector<std::string> calls;
	std::vector<int> values;
	std::vector<std::pair<int, int> > percents;

	// What the sync call answers, so a case can make one send fail.
	coreapi::Status sync_answer;
	FakeAudioOutput() : sync_answer(coreapi::Status::Ok) {}

	coreapi::Status setSrs(int enable, int nmgr, int algo, int ref)
	{
		calls.push_back("srs");
		values.push_back(enable);
		values.push_back(nmgr);
		values.push_back(algo);
		values.push_back(ref);
		return coreapi::Status::Ok;
	}
	coreapi::Status enableAnalogOut(int on) { calls.push_back("analog"); values.push_back(on); return coreapi::Status::Ok; }
	coreapi::Status setHdmiDolby(int v) { calls.push_back("hdmi"); values.push_back(v); return coreapi::Status::Ok; }
	coreapi::Status setSpdifDolby(int v) { calls.push_back("spdif"); values.push_back(v); return coreapi::Status::Ok; }
	coreapi::Status setSyncMode(int mode) { calls.push_back("sync"); values.push_back(mode); return sync_answer; }
	coreapi::Status setAudioMode(int mode) { calls.push_back("mode"); values.push_back(mode); return coreapi::Status::Ok; }
	coreapi::Status setVolumePercent(int ac3, int pcm)
	{
		calls.push_back("percent");
		percents.push_back(std::make_pair(ac3, pcm));
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
		values.clear();
		percents.clear();
	}
};

#endif
