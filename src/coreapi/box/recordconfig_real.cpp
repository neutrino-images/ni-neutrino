/*
 * recordconfig_real.cpp - the recording group's seam on the running box
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

#include <config.h>

#include "coreapi/box/apply_record.h"

#include <system/helpers.h>

namespace coreapi
{

namespace
{

class RealRecordConfigOutput : public RecordConfigOutput
{
	public:
		Status setDirectory(const std::string &dir) { return applicationSetRecordDirectory(dir); }

		Status setTimeshiftDirectory(const std::string &dir) { return applicationSetTimeshiftDirectory(dir); }

		Status configure(bool stop_sectionsd, bool stream_vtxt_pid, bool stream_pmt_pid, bool stream_subtitle_pids)
		{
			return applicationConfigureRecorder(stop_sectionsd, stream_vtxt_pid, stream_pmt_pid,
							    stream_subtitle_pids);
		}

		Status setUsageDirectory(const std::string &dir) { return applicationSetUsageDirectory(dir); }

		bool recording() { return applicationRecordingRunning(); }

		Status makeDirectory(const std::string &dir)
		{
			safe_mkdir(dir.c_str());
			return Status::Ok;
		}
};

RealRecordConfigOutput g_real_record_config_output;

} // namespace

void installRealRecordConfigOutput()
{
	setRecordConfigOutput(&g_real_record_config_output);
}

} // namespace coreapi
