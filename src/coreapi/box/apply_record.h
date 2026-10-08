/*
 * apply_record.h - what makes a changed recording setting take effect
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

#ifndef __coreapi_apply_record_h__
#define __coreapi_apply_record_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

#include <string>

namespace coreapi
{

/* What the recording group tells the recorder and the disk usage watcher: where
   recordings and timeshift go, which data pids a recording carries, and which
   directory the usage figure is taken from. One call per thing they take.

   A seam rather than the objects, so the group can run where no recorder
   exists. */
struct RecordConfigOutput
{
	virtual ~RecordConfigOutput() {}

	virtual Status setDirectory(const std::string &dir) = 0;
	virtual Status setTimeshiftDirectory(const std::string &dir) = 0;
	virtual Status configure(bool stop_sectionsd, bool stream_vtxt_pid, bool stream_pmt_pid,
				 bool stream_subtitle_pids) = 0;
	virtual Status setUsageDirectory(const std::string &dir) = 0;
	// Makes the folder if it is not there; one that exists is left as it is.
	virtual Status makeDirectory(const std::string &dir) = 0;
	// Whether a recording runs now.
	virtual bool recording() = 0;
};

/* NotSupported for every call while nothing is installed, so a group run
   before the seam exists fails and says so instead of touching nothing. */
RecordConfigOutput &recordConfigOutput();
void setRecordConfigOutput(RecordConfigOutput *o);

// Binds the seam to the recorder and the usage watcher of the running box.
void installRealRecordConfigOutput();

/* The real side is the application's, which owns the recorder and the usage watcher. */
Status applicationSetRecordDirectory(const std::string &dir);
Status applicationSetTimeshiftDirectory(const std::string &dir);
Status applicationConfigureRecorder(bool stop_sectionsd, bool stream_vtxt_pid, bool stream_pmt_pid,
				    bool stream_subtitle_pids);
Status applicationSetUsageDirectory(const std::string &dir);
bool applicationRecordingRunning();

/* The folder timeshift files go to: the one set, unless it is empty or the
   recording folder itself, which keeps them in a folder of their own below it. */
std::string timeshiftDirectoryFor(const std::string &recording_dir, const std::string &timeshift_dir);

// Forgets what was sent, for a case that needs the first run again.
void resetSentRecord();

extern const ApplyGroup kRecordConfigApplyGroup;

} // namespace coreapi

#endif
