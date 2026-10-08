/*
 * toolhint.cpp - what the model can do about a refusal
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

#include "httpd/mcp/toolhint.h"

#include <cstring>

namespace httpd
{
namespace mcp
{

namespace
{

struct Hint
{
	coreapi::ErrorCode code;
	const char        *text;
};

const Hint kHints[] = {
	{ coreapi::ErrorCode::RecordingHoldsTuner,
	  "A recording holds the tuner: stop it with stop_recording, or pick a channel it does not need." },
	{ coreapi::ErrorCode::TimerInThePast, "Pick a programme or a time that has not passed." },
	{ coreapi::ErrorCode::TimerExists, "list_timers shows the timer that is already there." },
	{ coreapi::ErrorCode::AmbiguousChannel, "Retry with one of the names or ids the refusal lists." },
	{ coreapi::ErrorCode::NoSuchChannel,
	  "Look the channel up with list_channels, or use its name as the box lists it." },
	{ coreapi::ErrorCode::NoSuchBouquet, "list_bouquets names the bouquets there are." },
	{ coreapi::ErrorCode::NoSuchEvent,
	  "Look the programme up again with find_programme or channel_schedule; the guide changes." },
	{ coreapi::ErrorCode::NoSuchTimer, "list_timers shows the timers there are." },
	{ coreapi::ErrorCode::NoRunningChannel,
	  "No channel plays live: the box is in standby or plays a recording or a file, "
	  "now_playing says which. A caller with write access can start a channel with "
	  "switch_channel and wake true." },
	{ coreapi::ErrorCode::QueryTooShort, "Search with at least two characters." },
	{ coreapi::ErrorCode::RecordingRunning, "A running recording keeps its start; only its end can move." },
	{ coreapi::ErrorCode::EmptyWindow, "Make to later than from." },
	{ coreapi::ErrorCode::NotPermitted,
	  "This needs a scope the connection was not granted; ask the user to reconnect with it." },
	{ coreapi::ErrorCode::PluginNotAllowed,
	  "Tell the user the owner has to allow this in the Freigaben screen of the KI tab in ni-web; "
	  "do not try another way." },
	{ coreapi::ErrorCode::SettingsSectionNotAllowed,
	  "Tell the user the owner has to allow this in the Freigaben screen of the KI tab in ni-web; "
	  "do not try another way." },
	{ coreapi::ErrorCode::SettingLocked,
	  "The box's image fixes its parental lock; tell the user this setting cannot be changed on "
	  "this box. Do not try another way." },
	{ coreapi::ErrorCode::SettingNotOnThisBox,
	  "This box does not have what the setting controls; tell the user it does not apply to this "
	  "box. Do not try another way." },
	{ coreapi::ErrorCode::SettingsSectionDenied,
	  "No AI client may ever change this section or credential; tell the user to change it in "
	  "ni-web directly. Do not try another way." },
};

bool takes(const Endpoint &ep, const char *name)
{
	for (size_t i = 0; i < ep.param_count && ep.params != NULL; ++i)
		if (ep.params[i].name != NULL && std::strcmp(ep.params[i].name, name) == 0)
			return true;
	return false;
}

bool declares(const Endpoint &ep, coreapi::ErrorCode code)
{
	for (size_t i = 0; i < ep.refusal_count && ep.refusals != NULL; ++i)
		if (ep.refusals[i].code == code)
			return true;
	return false;
}

const char kArchivePrefix[] = "/api/v1/recordings/archive";

bool isArchiveRoute(const Endpoint &ep)
{
	return ep.path != NULL && std::strncmp(ep.path, kArchivePrefix, sizeof(kArchivePrefix) - 1) == 0;
}

} // namespace

const char *retryHint(coreapi::ErrorCode code, const Endpoint &ep)
{
	if (code == coreapi::ErrorCode::BoxInStandby)
		return takes(ep, "wake") ? "Retry with wake true."
		                         : "Wake the box with switch_channel and wake true, "
		                           "or with set_standby (on false), which needs the system scope.";
	if (code == coreapi::ErrorCode::PlaybackRunning)
		return takes(ep, "stop_playback") ? "Retry with stop_playback true." : NULL;
	// The archive and a running recording are looked up through different tools.
	if (code == coreapi::ErrorCode::NoSuchRecording)
		return isArchiveRoute(ep) ? "list_archive shows the finished recordings and their ids."
		                         : "list_recordings shows what is recording.";
	// A route's own not-permitted is about what was asked, and its words say what to do.
	if (code == coreapi::ErrorCode::NotPermitted && declares(ep, code))
		return NULL;
	for (size_t i = 0; i < sizeof(kHints) / sizeof(kHints[0]); ++i)
		if (kHints[i].code == code)
			return kHints[i].text;
	return NULL;
}

} // namespace mcp
} // namespace httpd
