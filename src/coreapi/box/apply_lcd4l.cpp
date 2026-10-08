/*
 * apply_lcd4l.cpp - what makes a changed LCD4Linux setting take effect
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

#include "coreapi/box/apply_lcd4l.h"
#include "coreapi/box/applyworker.h"
#include "coreapi/box/sentstate.h"

#include <system/settings.h>

#include <vector>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

class NoLcd4lControl : public Lcd4lControl
{
	public:
		Status restartService(int) { return Status::NotSupported; }
		Status reinit() { return Status::NotSupported; }
		Status forceRun() { return Status::NotSupported; }
};

NoLcd4lControl g_no_lcd4l_control;
Lcd4lControl *g_lcd4l_control = 0;

/* What the service was last told. Stopping and starting it is a script and a
   thread, and rewriting its files is a pass over all of them, so a sibling key
   must not repeat either. */
struct Lcd4lState
{
	bool started;
	Sent<int> mode;
	Sent<std::vector<int> > look;
	Lcd4lState() : started(false) {}
};

Lcd4lState g_state;
// What the worker could not send.
SentFlags g_flags;

const unsigned kModeSent = 1;
const unsigned kLookSent = 2;

Status runLcd4l()
{
	Lcd4lControl &c = lcd4lControl();
	Status first = Status::Ok;

	// The settings are in the struct only where the build has LCD4Linux; without it there is nothing to tell.
	int mode = 0;
	std::vector<int> look(4, 0);
#ifdef ENABLE_LCD4LINUX
	mode = g_settings.lcd4l_support;
	look[0] = g_settings.lcd4l_display_type;
	look[1] = g_settings.lcd4l_skin;
	look[2] = g_settings.lcd4l_brightness;
	look[3] = g_settings.lcd4l_screenshots;
#endif

	if (!g_state.started)
	{
		// Startup: the application starts the service later, with the mode read here.
		g_state.started = true;
		g_state.mode.known = true;
		g_state.mode.value = mode;
		g_state.look.known = true;
		g_state.look.value = look;
	}
	else
	{
		// A restart that failed has written nothing either.
		const unsigned failed = g_flags.take();
		if (failed & kModeSent)
			g_state.mode.known = g_state.look.known = false;
		if (failed & kLookSent)
			g_state.look.known = false;

		/* Stopping and starting joins the service's thread and runs its script, and a
		   rewrite is a pass over every file, so both run on the apply worker. */
		Lcd4lControl *control = &c;
		const bool restart = g_state.mode.differs(mode);
		postChanged(first, g_flags, kModeSent, g_state.mode, mode, "lcd4l.mode", "lcd4l_support", [control, mode]() { return control->restartService(mode); });
		/* A service started now writes everything when its thread begins, so
		   what it would be rewritten for is written already. */
		if (restart && !g_state.mode.differs(mode))
		{
			g_state.look.known = true;
			g_state.look.value = look;
		}
		else
			postChanged(first, g_flags, kLookSent, g_state.look, look, "lcd4l.look",
				    "lcd4l_display_type lcd4l_skin lcd4l_brightness lcd4l_screenshots", [control]() { return control->reinit(); });
	}

	// A flag the thread reads: nothing is repeated by setting it again. Queued behind a restart this run asked for.
	if (mode == 1)
	{
		Lcd4lControl *control = &c;
		if (!applyWorker().post("lcd4l.force", [control]() { control->forceRun(); }))
			noteFirst(first, Status::Internal);
	}
	return first;
}

const char *const kLcd4lKeys[] =
{
	"lcd4l_support",
	"lcd4l_display_type",
	"lcd4l_skin",
	"lcd4l_brightness",
	"lcd4l_screenshots"
};

} // namespace

Lcd4lControl &lcd4lControl()
{
	if (!g_lcd4l_control)
		return g_no_lcd4l_control;
	return *g_lcd4l_control;
}

void setLcd4lControl(Lcd4lControl *c) { g_lcd4l_control = c; }

void resetSentLcd4l()
{
	g_state = Lcd4lState();
	g_flags.reset();
}

/* After the network: the application's own start of the service follows the
   phases, and a change before it has nothing to rewrite. */
const ApplyGroup kLcd4lApplyGroup = { "lcd4l", ApplyPhase::Network, COREAPI_KEYS(kLcd4lKeys), &runLcd4l };

} // namespace coreapi
