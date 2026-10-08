/*
 * apply_record.cpp - what makes a changed recording setting take effect
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
#include "coreapi/box/sentstate.h"

#include <system/settings.h>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

class NoRecordConfigOutput : public RecordConfigOutput
{
	public:
		Status setDirectory(const std::string &) { return Status::NotSupported; }
		Status setTimeshiftDirectory(const std::string &) { return Status::NotSupported; }
		Status configure(bool, bool, bool, bool) { return Status::NotSupported; }
		Status setUsageDirectory(const std::string &) { return Status::NotSupported; }
		Status makeDirectory(const std::string &) { return Status::NotSupported; }
		bool recording() { return false; }
};

NoRecordConfigOutput g_no_record_config_output;
RecordConfigOutput *g_record_config_output = 0;

/* What was last sent. The recorder sets its stop-sectionsd flag itself when a second
   recording starts and Config overwrites it, so the flags are sent only when one of
   them differs and never while a recording runs: the run that follows the end of the
   recording (the application applies the group then) sends them. */
Sent<std::string> g_directory;
Sent<std::string> g_timeshift;
Sent<std::string> g_usage;
Sent<int> g_flags;
// Nothing else writes these from outside the group, so nothing is marked or held.
SentFlags g_marks;

Status runRecordConfig()
{
	RecordConfigOutput &out = recordConfigOutput();
	Status first = Status::Ok;

	const std::string recording_dir = g_settings.network_nfs_recordingdir;
	const std::string timeshift_dir = timeshiftDirectoryFor(recording_dir, g_settings.timeshiftdir);
	const int flags = (g_settings.recording_stopsectionsd != 0 ? 1 : 0) | (g_settings.recording_stream_vtxt_pid != 0 ? 2 : 0) |
			  (g_settings.recording_stream_pmt_pid != 0 ? 4 : 0) | (g_settings.recording_stream_subtitle_pids != 0 ? 8 : 0);

	sendChanged(first, g_marks, 0, g_directory, recording_dir, [&]() { return out.setDirectory(recording_dir); });
	sendChanged(first, g_marks, 0, g_timeshift, timeshift_dir, [&]()
	{
		Status s = Status::Ok;
		// A folder of its own below the recording folder is the box's to make.
		if (timeshift_dir != g_settings.timeshiftdir)
			noteFirst(s, out.makeDirectory(timeshift_dir));
		noteFirst(s, out.setTimeshiftDirectory(timeshift_dir));
		return s;
	});
	if (!out.recording())
		sendChanged(first, g_marks, 0, g_flags, flags, [&]()
		{
			return out.configure((flags & 1) != 0, (flags & 2) != 0, (flags & 4) != 0, (flags & 8) != 0);
		});
	sendChanged(first, g_marks, 0, g_usage, recording_dir, [&]() { return out.setUsageDirectory(recording_dir); });
	return first;
}

const char *const kRecordConfigKeys[] =
{
	"network_nfs_recordingdir",
	"timeshiftdir",
	"recording_stopsectionsd",
	"recordingmenu.stream_vtxt_pid",
	"recordingmenu.stream_pmt_pid",
	"recordingmenu.stream_subtitle_pids"
};

} // namespace

RecordConfigOutput &recordConfigOutput()
{
	if (!g_record_config_output)
		return g_no_record_config_output;
	return *g_record_config_output;
}

void setRecordConfigOutput(RecordConfigOutput *o) { g_record_config_output = o; }

void resetSentRecord()
{
	g_directory = Sent<std::string>();
	g_timeshift = Sent<std::string>();
	g_usage = Sent<std::string>();
	g_flags = Sent<int>();
	g_marks.reset();
}

std::string timeshiftDirectoryFor(const std::string &recording_dir, const std::string &timeshift_dir)
{
	if (timeshift_dir.empty() || timeshift_dir == recording_dir)
		return recording_dir + "/.timeshift";
	return timeshift_dir;
}

/* After the mounts, which a recording folder on a disk needs. */
const ApplyGroup kRecordConfigApplyGroup = { "recordConfig", ApplyPhase::Network, COREAPI_KEYS(kRecordConfigKeys), &runRecordConfig };

} // namespace coreapi
