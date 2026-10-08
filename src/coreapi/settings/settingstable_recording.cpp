/*
 * settingstable_recording.cpp - recording settings, one row per field
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

#include "settingstable.h"
#include "settingsfield.h"

#include "coreapi/base/deps.h"

#include <timerdclient/timerdtypes.h>

namespace coreapi
{

namespace
{

/* The recording section. The settings fall into groups (timeshift, timers,
   audio pids, data pids) and the descriptor carries no sub-section, so that
   structure is not in the table.

   The two data pid rows reach the recorder only through a recorder
   reconfiguration, which is a call the recording applier makes rather than a
   restart.

   The one row of this section declared elsewhere is
   recording_audio_pids_default, in settingstable.cpp. Three rows here are the
   bits of that mask, and the mask and its bits are one setting offered two
   ways rather than four settings. */

/* The two safety times, in minutes and not the seconds the daemon keeps them
   in. The box divides by sixty on the way in and multiplies back on the way
   out, so it cannot express ninety seconds at all. Seconds here would let a
   client set a value the box can neither show nor keep, and the next read
   would round it away without saying so.

   Both are told at once because the daemon takes them together, and the other
   one comes from the daemon (CTimerdClient::getRecordingSafety) rather than
   from the settings struct: nothing loads, saves or fills the members named
   after these, so folding one of those into the call would overwrite a value
   nobody asked about. */
const int kSecondsPerMinute = 60;

bool askSafetyBefore(long &out)
{
	int before = 0, after = 0;
	if (recordingSafetySource().read(before, after) != Status::Ok)
		return false;
	out = before / kSecondsPerMinute;
	return true;
}

bool askSafetyAfter(long &out)
{
	int before = 0, after = 0;
	if (recordingSafetySource().read(before, after) != Status::Ok)
		return false;
	out = after / kSecondsPerMinute;
	return true;
}

bool tellSafetyBefore(long value)
{
	int before = 0, after = 0;
	if (recordingSafetySource().read(before, after) != Status::Ok)
		return false;
	return recordingSafetySource().write((int) value * kSecondsPerMinute, after) == Status::Ok;
}

bool tellSafetyAfter(long value)
{
	int before = 0, after = 0;
	if (recordingSafetySource().read(before, after) != Status::Ok)
		return false;
	return recordingSafetySource().write(before, (int) value * kSecondsPerMinute) == Status::Ok;
}

const EnumValue kEndOfRecording[] =
{
	{ 0, "recordingmenu.end_of_recording_max", NULL, NULL },
	{ 1, "recordingmenu.end_of_recording_epg", NULL, NULL }
};

const EnumValue kFollowScreenings[] =
{
	{ FOLLOWSCREENINGS_OFF, "options.off", NULL, NULL },
	{ FOLLOWSCREENINGS_ON, "options.on", NULL, NULL },
	{ FOLLOWSCREENINGS_ALWAYS, "options.always", NULL, NULL }
};

/* Recording off and recording to a file, under the two locales the program
   keeps for them and uses nowhere else. Nought is off and one is a file. */
const EnumValue kRecordingType[] =
{
	{ 0, "recording_type.off", NULL, NULL },
	{ 1, "recording_type.file", NULL, NULL }
};

// A no and a yes for the audio pid flags.
const EnumValue kNoYes[] =
{
	{ 0, "messagebox.no", NULL, NULL },
	{ 1, "messagebox.yes", NULL, NULL }
};

const Descriptor kRecording[] =
{
	/* Refused while a recording is running, as is the timeshift directory below,
	   which is a state of the box and not a setting. */
	{
		"network_nfs_recordingdir", ValueType::String, "recording",
		"recordingmenu.defdir", "menu.hint_record_dir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/movies", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(network_nfs_recordingdir)
	},
	{
		"recording_save_in_channeldir", ValueType::Bool, "recording",
		"recordingmenu.save_in_channeldir", "menu.hint_record_chandir",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_save_in_channeldir)
	},
	// Hours.
	{
		"record_hours", ValueType::Int, "recording",
		"extra.record_time", "menu.hint_record_time",
		1, 24, NULL, 0, 4, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(record_hours)
	},
	/* Loaded as a flag and offered as a choice, and the two words beside its
	   values are what it means rather than an on and an off, so a Bool row
	   would carry the value and lose the names. */
	{
		"recording_epg_for_end", ValueType::Enum, "recording",
		"recordingmenu.end_of_recording_name", "menu.hint_record_end",
		0, 0, COREAPI_ENUM(kEndOfRecording), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_epg_for_end)
	},
	{
		"recording_already_found_check", ValueType::Bool, "recording",
		"recordingmenu.already_found_check", "menu.hint_record_already_found_check",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_already_found_check)
	},
	{
		"recording_slow_warning", ValueType::Bool, "recording",
		"recordingmenu.slow_warn", "menu.hint_record_slow_warn",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_slow_warning)
	},
	// Per cent of the disc.
	{
		"recording_fill_warning", ValueType::Int, "recording",
		"recordingmenu.fill_warn", "menu.hint_record_fill_warn",
		75, 99, NULL, 0, 95, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_fill_warning)
	},
	{
		"recording_startstop_msg", ValueType::Bool, "recording",
		"recording.startstop_msg", "menu.hint_record_startstop_msg",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_startstop_msg)
	},
	// The key is not the field name here.
	{
		"recordingmenu.filename_template", ValueType::String, "recording",
		"recordingmenu.filename_template", "menu.hint_record_filename_template",
		0, 0, NULL, 0, 0, "%C_%T_%d_%t", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(recording_filename_template)
	},
	{
		"auto_cover", ValueType::Bool, "recording",
		"recordingmenu.auto_cover", "menu.hint_record_auto_cover",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(auto_cover)
	},

	/* Megabytes, and the two below follow the settings struct into the
	   condition it puts the fields behind. The program loads and saves them
	   under the same condition. Neither carries a hint. */
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	{
		"recording_bufsize", ValueType::Int, "recording",
		"extra.record_bufsize", NULL,
		1, 25, NULL, 0, 4, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_bufsize)
	},
	{
		"recording_bufsize_dmx", ValueType::Int, "recording",
		"extra.record_bufsize_dmx", NULL,
		1, 25, NULL, 0, 2, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_bufsize_dmx)
	},
#endif

	// timers
	{
		"recording_zap_on_announce", ValueType::Bool, "recording",
		"recordingmenu.zap_on_announce", "menu.hint_record_zap",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_zap_on_announce)
	},
	// Minutes.
	{
		"zapto_pre_time", ValueType::Int, "recording",
		"miscsettings.zapto_pre_time", "menu.hint_record_zap_pre_time",
		0, 10, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(zapto_pre_time)
	},
	{
		"timer_followscreenings", ValueType::Enum, "recording",
		"timersettings.followscreenings", "menu.hint_timer_followscreenings",
		0, 0, COREAPI_ENUM(kFollowScreenings), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(timer_followscreenings)
	},

	// data pids, and neither key is its field name
	{
		"recordingmenu.stream_vtxt_pid", ValueType::Bool, "recording",
		"recordingmenu.vtxt_pid", "menu.hint_record_data_vtxt",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_stream_vtxt_pid)
	},
	{
		"recordingmenu.stream_subtitle_pids", ValueType::Bool, "recording",
		"recordingmenu.dvbsub_pids", "menu.hint_record_data_dvbsub",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_stream_subtitle_pids)
	},

	/* Timeshift. An empty directory is a default and means the box puts the
	   timeshift under the recording directory. */
	{
		"timeshiftdir", ValueType::String, "recording",
		"recordingmenu.tsdir", "menu.hint_record_tdir",
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(timeshiftdir)
	},
	{
		"timeshift_pause", ValueType::Bool, "recording",
		"extra.timeshift_pause", "menu.hint_record_timeshift_pause",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(timeshift_pause)
	},
	// Seconds; nought means off.
	{
		"timeshift_auto", ValueType::Int, "recording",
		"extra.timeshift_auto", "menu.hint_record_timeshift_auto",
		0, 300, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(timeshift_auto)
	},
	{
		"timeshift_delete", ValueType::Bool, "recording",
		"extra.timeshift_delete", "menu.hint_record_timeshift_delete",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(timeshift_delete)
	},
	{
		"timeshift_temp", ValueType::Bool, "recording",
		"extra.timeshift_temp", "menu.hint_record_timeshift_temp",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(timeshift_temp)
	},
	// Hours.
	{
		"timeshift_hours", ValueType::Int, "recording",
		"extra.record_time_ts", "menu.hint_record_time_ts",
		1, 24, NULL, 0, 4, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(timeshift_hours)
	},

	/* Where the movie browser looks, which is not where recordings are
	   written: the browser shows it and does not offer it for editing. */
	{
		"network_nfs_moviedir", ValueType::String, "recording",
		"moviebrowser.dir", NULL,
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/movies", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(network_nfs_moviedir)
	},
	/* Whether a direct recording asks where to put itself, and how far it
	   asks: the recorder reads one and two apart, which is the only statement
	   of the range there is. No item names it. */
	{
		"recording_choose_direct_rec_dir", ValueType::Int, "recording",
		NULL, NULL,
		0, 2, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_choose_direct_rec_dir)
	},
	/* Whether the box records at all. No item offers it any more, and the words
	   below are the only statement of its two values; other code still reads
	   the value. */
	{
		"recording_type", ValueType::Enum, "recording",
		NULL, NULL,
		0, 0, COREAPI_VALUES(kRecordingType), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_type)
	},
	/* Whether a timer that woke the box was a recording one, which the box
	   writes itself as it goes to sleep, and reads once as it wakes before
	   clearing it. No item names it, and a written value lives until the next
	   start. */
	{
		"shutdown_timer_record_type", ValueType::Bool, "recording",
		NULL, NULL,
		0, 1, NULL, 0, 0, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(shutdown_timer_record_type)
	},
	/* The two below reach the recorder through the same call as the data pid
	   flags beside them, and neither has an item or a name: the program has no
	   locale for either. */
	{
		"recording_stopsectionsd", ValueType::Bool, "recording",
		NULL, NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_stopsectionsd)
	},
	{
		"recordingmenu.stream_pmt_pid", ValueType::Bool, "recording",
		NULL, NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_stream_pmt_pid)
	},
	/* The two that decide when a recording begins and ends. The settings file
	   names neither: the value is the timer daemon's, which keeps it in a file
	   of its own and falls back to nought before and ten minutes after on a box
	   that has none. */
	{
		"record_safety_time_before", ValueType::Int, "recording",
		"timersettings.record_safety_time_before", "menu.hint_record_timebefore",
		0, 99, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_SERVICE_FIELD(record_safety_time_before, askSafetyBefore, tellSafetyBefore)
	},
	{
		"record_safety_time_after", ValueType::Int, "recording",
		"timersettings.record_safety_time_after", "menu.hint_record_timeafter",
		0, 99, NULL, 0, 10, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_SERVICE_FIELD(record_safety_time_after, askSafetyAfter, tellSafetyAfter)
	},
	/* The three bits of recording_audio_pids_default, which are offered as
	   three questions and folded back into the mask. Both the bits and the mask
	   are declared, so a caller may write either, and they cannot disagree
	   because there is one field under them.

	   Each row is named after the settings member that used to carry its bit,
	   and that member is not where the value is: it holds whatever was last
	   left in it.

	   The defaults are the bits of the mask's own default, TIMERD_APIDS_STD
	   together with TIMERD_APIDS_AC3. */
	{
		"recording_audio_pids_std", ValueType::Bool, "recording",
		"recordingmenu.apids_std", "menu.hint_record_apid_std",
		0, 1, COREAPI_ENUM(kNoYes), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_MASK_BIT_FIELD(recording_audio_pids_std, recording_audio_pids_default,
		                       TIMERD_APIDS_STD)
	},
	{
		"recording_audio_pids_alt", ValueType::Bool, "recording",
		"recordingmenu.apids_alt", "menu.hint_record_apid_alt",
		0, 1, COREAPI_ENUM(kNoYes), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_MASK_BIT_FIELD(recording_audio_pids_alt, recording_audio_pids_default,
		                       TIMERD_APIDS_ALT)
	},
	{
		"recording_audio_pids_ac3", ValueType::Bool, "recording",
		"recordingmenu.apids_ac3", "menu.hint_record_apid_ac3",
		0, 1, COREAPI_ENUM(kNoYes), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_MASK_BIT_FIELD(recording_audio_pids_ac3, recording_audio_pids_default,
		                       TIMERD_APIDS_AC3)
	},
};

} // anonymous namespace

const Descriptor *settingsTableRecording(size_t &count)
{
	count = sizeof(kRecording) / sizeof(kRecording[0]);
	return kRecording;
}

} // namespace coreapi
