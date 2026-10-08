/*
 * fakemisc.h - the miscellaneous groups' seam, recording
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

#ifndef __support_fakemisc_h__
#define __support_fakemisc_h__

#include "coreapi/box/apply_misc.h"

#include <string>
#include <vector>

/* Every call the miscellaneous groups make, in order, with what they were given,
   so a case can say what one run sent and that it sent nothing else. */
struct FakeMiscOutput : public coreapi::MiscOutput
{
	std::vector<std::string> calls;
	std::vector<int> sdt;
	std::vector<int> ports;
	std::vector<bool> teletext;
	std::vector<int> cpu;
	std::vector<int> fan;
	// What every call answers, so a case can make a send fail.
	coreapi::Status answer;

	FakeMiscOutput() : answer(coreapi::Status::Ok) {}

	coreapi::Status setScanSdt(int mode) { calls.push_back("sdt"); sdt.push_back(mode); return answer; }
	coreapi::Status setStreamPort(int port) { calls.push_back("port"); ports.push_back(port); return answer; }
	coreapi::Status setTeletextCache(bool on) { calls.push_back(on ? "txt-on" : "txt-off"); teletext.push_back(on); return answer; }
	coreapi::Status configureEpgFilter() { calls.push_back("filter"); return answer; }
	coreapi::Status startEpgScan() { calls.push_back("scan-start"); return answer; }
	coreapi::Status clearEpgScan() { calls.push_back("scan-clear"); return answer; }

	coreapi::Status setCpuFreq(int mhz) { calls.push_back("cpu"); cpu.push_back(mhz); return answer; }
	coreapi::Status setFanSpeed(int speed) { calls.push_back("fan"); fan.push_back(speed); return answer; }

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
		sdt.clear();
		ports.clear();
		teletext.clear();
		cpu.clear();
		fan.clear();
	}
};

#endif
