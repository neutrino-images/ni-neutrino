/*
 * apply_osd.h - what makes a changed on screen display setting take effect
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

#ifndef __coreapi_apply_osd_h__
#define __coreapi_apply_osd_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

namespace coreapi
{

/* How much of the fonts a change rebuilds. The face of the screen's font
   rebuilds everything, a new scaling the renderers and the dynamic fonts, and
   the monospace face only the renderer the shell reads. */
enum class FontSetup
{
	All,
	Scaling,
	Monospace
};

/* What the OSD groups tell the objects the box draws with. One call per thing
   the drawing objects take, so the groups decide what is sent and when and this
   only carries it. Nothing here asks anybody anything.

   A seam rather than the objects themselves, so the groups can run where no
   screen exists. An object that is not there yet is not an error: it builds
   itself from the settings when it comes. */
struct OsdOutput
{
	virtual ~OsdOutput() {}

	virtual Status setupFonts(FontSetup what) = 0;
	// The colours of the theme, put into the palette the screen is drawn with.
	virtual Status setPalette() = 0;
	/* The corners of the drawn area, taken from the slot the preset and the size of the
	   screen name, and the infobar's bars made again for them. */
	virtual Status setScreenGeometry() = 0;
	// The front display's skin text that shows the logo of the event, made again.
	virtual Status resetLcd4lParse() = 0;
	/* The infobar's modules, made again from the settings the next time it is shown. An
	   infobar that is not there yet builds them from the settings when it comes. */
	virtual Status resetInfoViewer() = 0;
	// The clock in the corner and the elapsed time of a played file, drawn again from the settings.
	virtual Status clearInfoClock() = 0;
	// The volume bar, laid out again for the sizes the settings and the clock now give it.
	virtual Status refreshVolumeBar() = 0;
	// The mute icon, drawn again where the volume bar left room for it.
	virtual Status refreshMuteIcon() = 0;
	// The radio text decoder, started or stopped for the radio programme on the screen.
	virtual Status resetRadioText() = 0;
	// The icons the screens drew, which the size the box draws at changes.
	virtual Status clearIconCache() = 0;
	/* The channel list's header, separator and mini TV, made again from the settings the next
	   time the list is drawn. A list that is not there yet builds them when it comes. */
	virtual Status resetChannelList() = 0;
	// The mode icons, started or stopped to match the setting; asking for the state they are in changes nothing.
	virtual Status resetInfoIcons() = 0;
};

/* NotSupported for every call while nothing is installed, so a group run
   before the drawing exists fails and says so instead of touching nothing. */
OsdOutput &osdOutput();
void setOsdOutput(OsdOutput *o);

// Binds the accessor above to the program's drawing objects.
void installRealOsdOutput();

/* The things the OSD groups put on the screen, each kept as what was last sent
   so a run rebuilds only what differs: a font rebuild replaces every font object
   the screens hold. */
enum class OsdSent : unsigned
{
	Fonts     = 1u << 0,
	InfoClock = 1u << 1
};

/* Something other than the groups changed that state, so the next run sends it
   again whatever was sent before. Any thread may call it. */
void forgetSentOsd(OsdSent what);

// Forgets what was sent, for a case that needs the first run again.
void resetSentOsd();

/* Defined by the application, because the fonts belong to an object whose header
   reaches the GUI and this layer must not. */
Status applicationSetupFonts(FontSetup what);
Status applicationSetPalette();
Status applicationSetScreenGeometry();
Status applicationResetLcd4lParse();
Status applicationResetInfoViewer();
Status applicationClearInfoClock();
Status applicationRefreshVolumeBar();
Status applicationRefreshMuteIcon();
Status applicationResetRadioText();
Status applicationClearIconCache();
Status applicationResetInfoIcons();
Status applicationResetChannelList();

// The face of both fonts and their scaling.
extern const ApplyGroup kFontsApplyGroup;

// The colours of the theme.
extern const ApplyGroup kPaletteApplyGroup;

// The preset of the drawn area and the corners of each preset.
extern const ApplyGroup kScreenGeometryApplyGroup;

// Whether the channel list shows the logo of the event, which the display skin shows too.
extern const ApplyGroup kEventLogoApplyGroup;

// What the infobar lays out its modules by.
extern const ApplyGroup kInfoViewerApplyGroup;

// Whether, how big and how the clock in the corner is drawn.
extern const ApplyGroup kInfoClockApplyGroup;

// What the channel list lays its header, columns and mini TV out by.
extern const ApplyGroup kChannelListApplyGroup;

// Whether the mode icons run.
extern const ApplyGroup kInfoIconsApplyGroup;

// How big the volume bar is and what it shows.
extern const ApplyGroup kVolumeBarApplyGroup;

// Whether the radio text of a radio programme is decoded.
extern const ApplyGroup kRadioTextApplyGroup;

// Which of the two sizes the box draws its own screen at.
extern const ApplyGroup kOsdResolutionApplyGroup;

} // namespace coreapi

#endif
