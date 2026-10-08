/*
 * apply_cec.cpp - what makes a changed HDMI CEC setting take effect
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

#include "coreapi/box/apply_cec.h"
#include "coreapi/box/sentstate.h"

#include <system/settings.h>

#include <hardware/video.h>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

class NoCecLink : public CecLink
{
	public:
		Status setMode(int) { return Status::NotSupported; }
		Status setAutoStandby(int) { return Status::NotSupported; }
		Status setAutoView(int) { return Status::NotSupported; }
		bool takesAudioDestination() const { return false; }
		Status setAudioDestination(int) { return Status::NotSupported; }
};

NoCecLink g_no_cec_link;
CecLink *g_cec_link = 0;

/* What was last sent. Writing the mode again starts the link over and the
   television answers it with traffic on the bus, so a key of the group sends
   only what differs. */
struct CecState
{
	Sent<int> mode;
	Sent<int> standby;
	Sent<int> view_on;
	Sent<int> destination;
};

CecState g_state;
/* Only a standby that leaves the television alone holds a state, and nothing outside the
   group writes the link, so nothing is marked. */
SentFlags g_flags;
bool g_deferred = false;

const unsigned kStandbyHeld = 1;
const unsigned kViewHeld = 2;

template <class Call>
void send(Status &first, Sent<int> &sent, int v, Call call, unsigned what = 0)
{
	sendChanged(first, g_flags, what, sent, v, call);
}

/* The order the program set these at startup: what the television is told to do
   first, the mode last, since the mode is what turns the link on. */
Status runCec()
{
	if (g_deferred)
	{
		/* The television takes over the volume keys even while nothing is sent: the
		   box's own volume stays at full, as the load of the settings leaves it. */
		if (cecLink().takesAudioDestination() && g_settings.hdmi_cec_volume != 0 &&
		    g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF)
			g_settings.current_volume = 100;
		return Status::Ok;
	}

	CecLink &link = cecLink();
	Status first = Status::Ok;

	const int standby = g_settings.hdmi_cec_standby == 1 ? 1 : 0;
	send(first, g_state.standby, standby, [&]() { return link.setAutoStandby(standby); }, kStandbyHeld);
	const int view_on = g_settings.hdmi_cec_view_on == 1 ? 1 : 0;
	send(first, g_state.view_on, view_on, [&]() { return link.setAutoView(view_on); }, kViewHeld);

	/* With the link off a changed destination waits for the link to come on, as the
	   screen had it; the first run sends it whatever the mode, as startup did. */
	const bool link_on = g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF;
	if (link.takesAudioDestination() && (link_on || !g_state.destination.known))
	{
		const int destination = g_settings.hdmi_cec_volume;
		/* The box's own volume goes to full while the keys act elsewhere, and does
		   so when the destination is changed with the link on. Not at the first
		   run, which is the box starting with the volume it was saved at. */
		if (g_state.destination.known && g_state.destination.value != destination)
			g_settings.current_volume = 100;
		send(first, g_state.destination, destination, [&]() { return link.setAudioDestination(destination); });
	}

	const int mode = g_settings.hdmi_cec_mode;
	send(first, g_state.mode, mode, [&]() { return link.setMode(mode); });
	return first;
}

const char *const kCecKeys[] =
{
	"hdmi_cec_mode",
	"hdmi_cec_view_on",
	"hdmi_cec_standby",
	"hdmi_cec_volume"
};

} // namespace

CecLink &cecLink()
{
	if (!g_cec_link)
		return g_no_cec_link;
	return *g_cec_link;
}

void setCecLink(CecLink *l) { g_cec_link = l; }

void deferCec(bool deferred) { g_deferred = deferred; }

Status cecStandby(bool asleep)
{
	deferCec(asleep);
	if (asleep)
		return Status::Ok;
	return applyKey("hdmi_cec_mode");
}

void holdCecPower(bool asleep, bool held, bool leave_tv)
{
	const unsigned what = asleep ? kStandbyHeld : kViewHeld;
	Sent<int> &sent = asleep ? g_state.standby : g_state.view_on;
	CecLink &link = cecLink();
	if (held)
	{
		/* A setting turned off while asleep is not on the link yet, as the group is deferred,
		   so the change would still act on the 1 the link has. */
		const int setting = asleep ? g_settings.hdmi_cec_standby : g_settings.hdmi_cec_view_on;
		if (sent.known && sent.value == 0)
			return;
		if (!leave_tv && setting == 1)
			return;
		g_flags.hold(what, true);
		sent.known = false;
		if (asleep)
			link.setAutoStandby(0);
		else
			link.setAutoView(0);
		return;
	}
	if (!g_flags.isHeld(what))
		return;
	g_flags.hold(what, false);
	/* Sent here, as no run need come before the next change. The setting is read now, as a
	   write while held was not sent. A refused send stays unknown, so the next run retries. */
	const int v = (asleep ? g_settings.hdmi_cec_standby : g_settings.hdmi_cec_view_on) == 1 ? 1 : 0;
	const Status s = asleep ? link.setAutoStandby(v) : link.setAutoView(v);
	if (s == Status::Ok)
	{
		sent.known = true;
		sent.value = v;
	}
}

void resetSentCec()
{
	g_state = CecState();
	g_flags.reset();
	g_deferred = false;
}

/* After the channel daemon's client exists, which is where the program started
   the link before. */
const ApplyGroup kCecApplyGroup = { "cec", ApplyPhase::Zapit, COREAPI_KEYS(kCecKeys), &runCec };

} // namespace coreapi
