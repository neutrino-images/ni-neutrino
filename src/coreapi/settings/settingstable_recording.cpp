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

   The directories and the data pid rows reach the recorder through one apply
   group, which tells it everything at once rather than needing a restart.

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

// No pause before the timeshift starts by itself is no start at all.
constexpr EnumValue kTimeshiftAutoOff[] =
{
	option(0).label("options.off")
};

constexpr EnumValue kEndOfRecording[] =
{
	option(0).label("recordingmenu.end_of_recording_max"),
	option(1).label("recordingmenu.end_of_recording_epg")
};

constexpr EnumValue kFollowScreenings[] =
{
	option(FOLLOWSCREENINGS_OFF).label("options.off"),
	option(FOLLOWSCREENINGS_ON).label("options.on"),
	option(FOLLOWSCREENINGS_ALWAYS).label("options.always")
};

/* Recording off and recording to a file, under the two locales the program
   keeps for them and uses nowhere else. Nought is off and one is a file. */
constexpr EnumValue kRecordingType[] =
{
	option(0).label("recording_type.off"),
	option(1).label("recording_type.file")
};

// A no and a yes for the audio pid flags.
constexpr EnumValue kNoYes[] =
{
	option(0).label("messagebox.no"),
	option(1).label("messagebox.yes")
};

constexpr Descriptor kRecording[] =
{
	/* The screen greys this item and the timeshift directory below while a recording
	   runs; the row carries no such condition. */
	textRow("network_nfs_recordingdir")
		.section("recording")
		.label("recordingmenu.defdir")
		.hint("menu.hint_record_dir")
		.defaultValue(TARGET_ROOT "/media/sda1/movies")
		.text(kRuleDirectoryDurable)
		.field(COREAPI_TEXT_FIELD(network_nfs_recordingdir)),
	boolRow("recording_save_in_channeldir")
		.section("recording")
		.label("recordingmenu.save_in_channeldir")
		.hint("menu.hint_record_chandir")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(recording_save_in_channeldir)),
	// Hours.
	intRow("record_hours")
		.section("recording")
		.label("extra.record_time")
		.hint("menu.hint_record_time")
		.range(1, 24)
		.defaultValue(4)
		.unit("unit.short.hour")
		.field(COREAPI_NUMBER_FIELD(record_hours)),
	/* Loaded as a flag and offered as a choice, and the two words beside its
	   values are what it means rather than an on and an off, so a Bool row
	   would carry the value and lose the names. */
	enumRow("recording_epg_for_end")
		.section("recording")
		.label("recordingmenu.end_of_recording_name")
		.hint("menu.hint_record_end")
		.defaultValue(1)
		.values(kEndOfRecording)
		.field(COREAPI_NUMBER_FIELD(recording_epg_for_end)),
	boolRow("recording_already_found_check")
		.section("recording")
		.label("recordingmenu.already_found_check")
		.hint("menu.hint_record_already_found_check")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(recording_already_found_check)),
	boolRow("recording_slow_warning")
		.section("recording")
		.label("recordingmenu.slow_warn")
		.hint("menu.hint_record_slow_warn")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(recording_slow_warning)),
	// Per cent of the disc.
	intRow("recording_fill_warning")
		.section("recording")
		.label("recordingmenu.fill_warn")
		.hint("menu.hint_record_fill_warn")
		.range(75, 99)
		.defaultValue(95)
		.unit("unit.short.percent")
		.field(COREAPI_NUMBER_FIELD(recording_fill_warning)),
	boolRow("recording_startstop_msg")
		.section("recording")
		.label("recording.startstop_msg")
		.hint("menu.hint_record_startstop_msg")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(recording_startstop_msg)),
	// The key is not the field name here.
	textRow("recordingmenu.filename_template")
		.section("recording")
		.label("recordingmenu.filename_template")
		.hint("menu.hint_record_filename_template")
		.defaultValue("%C_%T_%d_%t")
		.field(COREAPI_TEXT_FIELD(recording_filename_template)),
	boolRow("auto_cover")
		.section("recording")
		.label("recordingmenu.auto_cover")
		.hint("menu.hint_record_auto_cover")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(auto_cover)),

	/* Megabytes, and the two below follow the settings struct into the
	   condition it puts the fields behind. The program loads and saves them
	   under the same condition. Neither carries a hint. */
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	intRow("recording_bufsize")
		.section("recording")
		.label("extra.record_bufsize")
		.range(1, 25)
		.defaultValue(4)
		.unit("unit.short.megabyte")
		.field(COREAPI_NUMBER_FIELD(recording_bufsize)),
	intRow("recording_bufsize_dmx")
		.section("recording")
		.label("extra.record_bufsize_dmx")
		.range(1, 25)
		.defaultValue(2)
		.unit("unit.short.megabyte")
		.field(COREAPI_NUMBER_FIELD(recording_bufsize_dmx)),
#endif

	// timers
	boolRow("recording_zap_on_announce")
		.section("recording")
		.label("recordingmenu.zap_on_announce")
		.hint("menu.hint_record_zap")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(recording_zap_on_announce)),
	// Minutes.
	intRow("zapto_pre_time")
		.section("recording")
		.label("miscsettings.zapto_pre_time")
		.hint("menu.hint_record_zap_pre_time")
		.range(0, 10)
		.defaultValue(0)
		.unit("unit.short.minute")
		.field(COREAPI_NUMBER_FIELD(zapto_pre_time)),
	enumRow("timer_followscreenings")
		.section("recording")
		.label("timersettings.followscreenings")
		.hint("menu.hint_timer_followscreenings")
		.defaultValue(1)
		.values(kFollowScreenings)
		.field(COREAPI_NUMBER_FIELD(timer_followscreenings)),

	// data pids, and neither key is its field name
	boolRow("recordingmenu.stream_vtxt_pid")
		.section("recording")
		.label("recordingmenu.vtxt_pid")
		.hint("menu.hint_record_data_vtxt")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(recording_stream_vtxt_pid)),
	boolRow("recordingmenu.stream_subtitle_pids")
		.section("recording")
		.label("recordingmenu.dvbsub_pids")
		.hint("menu.hint_record_data_dvbsub")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(recording_stream_subtitle_pids)),

	/* Timeshift. An empty directory is a default and means the box puts the
	   timeshift under the recording directory. */
	textRow("timeshiftdir")
		.section("recording")
		.label("recordingmenu.tsdir")
		.hint("menu.hint_record_tdir")
		.defaultValue("")
		.text(kRuleDirectoryDurableOrEmpty)
		.field(COREAPI_TEXT_FIELD(timeshiftdir)),
	boolRow("timeshift_pause")
		.section("recording")
		.label("extra.timeshift_pause")
		.hint("menu.hint_record_timeshift_pause")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(timeshift_pause)),
	// Seconds after a channel starts.
	intRow("timeshift_auto")
		.section("recording")
		.label("extra.timeshift_auto")
		.hint("menu.hint_record_timeshift_auto")
		.range(0, 300)
		.defaultValue(0)
		.unit("unit.short.second")
		.values(kTimeshiftAutoOff)
		.field(COREAPI_NUMBER_FIELD(timeshift_auto)),
	boolRow("timeshift_delete")
		.section("recording")
		.label("extra.timeshift_delete")
		.hint("menu.hint_record_timeshift_delete")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(timeshift_delete)),
	boolRow("timeshift_temp")
		.section("recording")
		.label("extra.timeshift_temp")
		.hint("menu.hint_record_timeshift_temp")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(timeshift_temp)),
	// Hours.
	intRow("timeshift_hours")
		.section("recording")
		.label("extra.record_time_ts")
		.hint("menu.hint_record_time_ts")
		.range(1, 24)
		.defaultValue(4)
		.unit("unit.short.hour")
		.field(COREAPI_NUMBER_FIELD(timeshift_hours)),

	/* Where the movie browser looks, which is not where recordings are
	   written: the browser shows it and does not offer it for editing. */
	textRow("network_nfs_moviedir")
		.section("recording")
		.label("moviebrowser.dir")
		.defaultValue(TARGET_ROOT "/media/sda1/movies")
		.text(kRuleDirectory)
		.field(COREAPI_TEXT_FIELD(network_nfs_moviedir)),
	/* Whether a direct recording asks where to put itself, and how far it
	   asks: the recorder reads one and two apart, which is the only statement
	   of the range there is. No item names it. */
	intRow("recording_choose_direct_rec_dir")
		.section("recording")
		.range(0, 2)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(recording_choose_direct_rec_dir)),
	/* Whether the box records at all. No item offers it any more, and the words
	   below are the only statement of its two values; other code still reads
	   the value. */
	enumRow("recording_type")
		.section("recording")
		.defaultValue(1)
		.values(kRecordingType)
		.field(COREAPI_NUMBER_FIELD(recording_type)),
	/* Whether a timer that woke the box was a recording one, which the box
	   writes itself as it goes to sleep, and reads once as it wakes before
	   clearing it. No item names it, and a written value lives until the next
	   start. */
	boolRow("shutdown_timer_record_type")
		.section("recording")
		.defaultValue(0)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(shutdown_timer_record_type)),
	/* The two below reach the recorder through the same call as the data pid
	   flags beside them, and neither has an item or a name: the program has no
	   locale for either. */
	boolRow("recording_stopsectionsd")
		.section("recording")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(recording_stopsectionsd)),
	boolRow("recordingmenu.stream_pmt_pid")
		.section("recording")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(recording_stream_pmt_pid)),
	/* The two that decide when a recording begins and ends. The settings file
	   names neither: the value is the timer daemon's, which keeps it in a file
	   of its own and falls back to nought before and ten minutes after on a box
	   that has none. */
	intRow("record_safety_time_before")
		.section("recording")
		.label("timersettings.record_safety_time_before")
		.hint("menu.hint_record_timebefore")
		.range(0, 99)
		.defaultValue(0)
		.unit("unit.short.minute")
		.field(COREAPI_SERVICE_FIELD(record_safety_time_before, askSafetyBefore, tellSafetyBefore)),
	intRow("record_safety_time_after")
		.section("recording")
		.label("timersettings.record_safety_time_after")
		.hint("menu.hint_record_timeafter")
		.range(0, 99)
		.defaultValue(10)
		.unit("unit.short.minute")
		.field(COREAPI_SERVICE_FIELD(record_safety_time_after, askSafetyAfter, tellSafetyAfter)),
	/* The three bits of recording_audio_pids_default, which are offered as
	   three questions and folded back into the mask. Both the bits and the mask
	   are declared, so a caller may write either, and they cannot disagree
	   because there is one field under them.

	   Each row is named after the settings member that used to carry its bit,
	   and that member is not where the value is: it holds whatever was last
	   left in it.

	   The defaults are the bits of the mask's own default, TIMERD_APIDS_STD
	   together with TIMERD_APIDS_AC3. */
	boolRow("recording_audio_pids_std")
		.section("recording")
		.label("recordingmenu.apids_std")
		.hint("menu.hint_record_apid_std")
		.defaultValue(1)
		.values(kNoYes)
		.field(COREAPI_MASK_BIT_FIELD(recording_audio_pids_std, recording_audio_pids_default,
			TIMERD_APIDS_STD)),
	boolRow("recording_audio_pids_alt")
		.section("recording")
		.label("recordingmenu.apids_alt")
		.hint("menu.hint_record_apid_alt")
		.defaultValue(0)
		.values(kNoYes)
		.field(COREAPI_MASK_BIT_FIELD(recording_audio_pids_alt, recording_audio_pids_default,
			TIMERD_APIDS_ALT)),
	boolRow("recording_audio_pids_ac3")
		.section("recording")
		.label("recordingmenu.apids_ac3")
		.hint("menu.hint_record_apid_ac3")
		.defaultValue(1)
		.values(kNoYes)
		.field(COREAPI_MASK_BIT_FIELD(recording_audio_pids_ac3, recording_audio_pids_default,
			TIMERD_APIDS_AC3)),
};

} // anonymous namespace

const Descriptor *settingsTableRecording(size_t &count)
{
	count = sizeof(kRecording) / sizeof(kRecording[0]);
	return kRecording;
}

} // namespace coreapi
