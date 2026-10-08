/*
 * apply_glcd.h - what makes a changed graphic display setting take effect
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

#ifndef __coreapi_apply_glcd_h__
#define __coreapi_apply_glcd_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

#include <cstddef>
#include <string>

namespace coreapi
{

struct ValueLookup;

/* The graphic display's service: a thread drawing the layout the theme states.
   One call per thing it is told. A seam so the group runs where no display
   exists, which is every build but the one with the display library. */
struct GlcdPanel
{
	virtual ~GlcdPanel() {}

	virtual Status setEnabled(bool on) = 0;
	virtual Status setMirrorOsd(bool on) = 0;
	// The display driver chosen from the configuration file: the service is made again.
	virtual Status respawn() = 0;
	virtual Status reinitFont() = 0;
	virtual Status updateBrightness() = 0;
	// Draws the layout again from the settings.
	virtual Status update() = 0;
	/* The size of the connected panel, in pixels, as it was last recorded. Answered from
	   what the loop noted and from nothing else, since a write is checked against it on
	   the web server's thread, which must not reach the display service. */
	virtual Status panelSize(int &width, int &height) = 0;
	// Notes the panel's size from the service, if it has one. On the loop only.
	virtual Status recordPanelSize() = 0;
};

// NotSupported while nothing is installed.
GlcdPanel &glcdPanel();
void setGlcdPanel(GlcdPanel *p);

// Binds the accessor above to the program's display service.
void installRealGlcdPanel();

/* The largest panel the display drivers are written for: what a position is bounded
   by while no panel answers, and what the rows state as their constant. */
constexpr long kGlcdPanelWidthMax = 1920;
constexpr long kGlcdPanelHeightMax = 1080;

/* The size the loop saw the panel have, kept for any thread to read. NotSupported
   until the loop has noted one. */
void noteGlcdPanelSize(int width, int height);
Status recordedGlcdPanelSize(int &width, int &height);

/* The width and height of the connected panel, for the rows of the theme that bound a
   position by it; the constants above while the panel cannot say. */
long glcdPanelWidth(const ValueLookup * = NULL);
long glcdPanelHeight(const ValueLookup * = NULL);

/* Defined by the application, in the screen that owns the service: the display
   library's headers are not this layer's. Only where the build has the display. */
Status applicationGlcd(int what, int value);

// The things applicationGlcd is asked for.
enum GlcdAction
{
	GlcdEnable = 0,
	GlcdMirrorOsd = 1,
	GlcdRespawn = 2,
	GlcdReinitFont = 3,
	GlcdBrightness = 4,
	GlcdUpdate = 5,
	GlcdRecordSize = 6
};

/* What the group looks at in the settings. They exist only in a build with the display,
   so the group reads them into this, and a build without it hands the group zeros. */
struct GlcdSnapshot
{
	int enable;
	int mirror;
	int config;
	std::string font;
	GlcdSnapshot() : enable(0), mirror(0), config(0) {}
};

// What one run does with a reading of the settings, apart from where it came from.
Status applyGlcdSnapshot(const GlcdSnapshot &now);

// Forgets what was sent, for a case that needs the first run again.
void resetSentGlcd();

/* The switch, the mirroring, the driver, the font, the brightnesses, the scrolling
   and every number, text and colour of the theme. The service is made with these
   settings already, so the first run only learns the switch, the mirroring, the
   driver and the font; every later run sends what differs from what the last one
   saw, and has the layout drawn and the brightness set again. The mirroring is
   also set by the modes that show their own picture, which put it back when they
   end; this group sets it only when the setting changes. */
extern const ApplyGroup kGlcdApplyGroup;

} // namespace coreapi

#endif
