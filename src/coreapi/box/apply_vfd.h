/*
 * apply_vfd.h - what makes a changed front panel setting take effect
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

#ifndef __coreapi_apply_vfd_h__
#define __coreapi_apply_vfd_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

namespace coreapi
{

/* The front panel of the box as its driver takes it, one call per thing the
   driver is told, in the driver's numbers. A seam so the group runs where no
   panel exists. */
struct VfdPanel
{
	virtual ~VfdPanel() {}

	enum Brightness { Normal = 0, Standby = 1, DeepStandby = 2 };

	virtual Status setBrightness(Brightness which, int value) = 0;
	// How many times a line too long for the panel is scrolled past; the flag where the panel takes no count.
	virtual Status setScrollMode(int repeats) = 0;
	// The LEDs of the mode the box is in now, from the settings.
	virtual Status refreshLeds() = 0;
	// Contrast, power and inverse as the panel takes them, from the settings, with the brightness of the mode the box is in.
	virtual Status refreshParameters() = 0;
	virtual Status setBacklight(int on) = 0;
	// What the second line shows (play time, volume, nothing), with the volume it would show.
	virtual Status showStatusline(int mode, int volume) = 0;
};

// NotSupported while nothing is installed.
VfdPanel &vfdPanel();
void setVfdPanel(VfdPanel *p);

// Binds the accessor above to the program's front panel.
void installRealVfdPanel();

/* Defined by the application, in the screen that owns the panel: the driver's
   header reaches libraries this layer does not link. */
Status applicationVfdBrightness(int which, int value);
Status applicationVfdScroll(int repeats);
Status applicationVfdLeds();
Status applicationVfdParameters();
Status applicationVfdBacklight(int on);
Status applicationVfdStatusline(int mode, int volume);

/* Keeps the backlight for whoever holds it: the box in standby has its own, and
   while held the group sends none; once let go the next run sends the TV one again.
   On the loop only. */
void holdVfdBacklight(bool held);

// Forgets what was sent and every hold, for a case that needs the first run again.
void resetSentVfd();

/* The three brightnesses, contrast, power and inverse (one send between them), the
   scrolling, the LEDs of the mode the box runs in, the backlight and the second line. The panel is brought up with these settings
   already, so the first run sends the scrolling and the backlight, which the
   startup always sent, and only learns the rest; every later run sends what
   differs from what the last one saw. The dim brightness is not here: the driver
   reads it when it dims. */
extern const ApplyGroup kVfdApplyGroup;

} // namespace coreapi

#endif
