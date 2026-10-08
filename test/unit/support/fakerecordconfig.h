/*
 * fakerecordconfig.h - a recorder seam that records what the group sends
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

#ifndef __support_fakerecordconfig_h__
#define __support_fakerecordconfig_h__

#include "coreapi/box/apply_record.h"

#include <string>
#include <vector>

/* Every call the recording group makes, in order, with the text or flags it carried. */
struct FakeRecordConfig : public coreapi::RecordConfigOutput
{
	std::vector<std::string> calls;
	std::vector<std::string> values;
	// Whether a recording runs.
	bool running;

	FakeRecordConfig() : running(false) {}

	coreapi::Status setDirectory(const std::string &d) { note("directory", d); return coreapi::Status::Ok; }
	coreapi::Status setTimeshiftDirectory(const std::string &d) { note("timeshift", d); return coreapi::Status::Ok; }
	coreapi::Status configure(bool stop, bool vtxt, bool pmt, bool sub)
	{
		note("config", std::string(stop ? "1" : "0") + (vtxt ? "1" : "0") + (pmt ? "1" : "0") + (sub ? "1" : "0"));
		return coreapi::Status::Ok;
	}
	coreapi::Status setUsageDirectory(const std::string &d) { note("usage", d); return coreapi::Status::Ok; }
	coreapi::Status makeDirectory(const std::string &d) { note("make", d); return coreapi::Status::Ok; }
	bool recording() { return running; }

	size_t count(const std::string &what) const
	{
		size_t n = 0;
		for (size_t i = 0; i < calls.size(); ++i)
			n += (calls[i] == what) ? 1 : 0;
		return n;
	}

	// The value the last call of that name carried.
	std::string last(const std::string &what) const
	{
		for (size_t i = calls.size(); i > 0; --i)
			if (calls[i - 1] == what)
				return values[i - 1];
		return std::string();
	}

	void forget()
	{
		calls.clear();
		values.clear();
	}

	private:
		void note(const std::string &what, const std::string &v)
		{
			calls.push_back(what);
			values.push_back(v);
		}
};

#endif
