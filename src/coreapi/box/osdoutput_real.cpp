/*
 * osdoutput_real.cpp - the OSD groups' drawing objects
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

#include "coreapi/box/apply_osd.h"

namespace coreapi
{

namespace
{

/* The objects are the screens' own and their headers reach the GUI, so each
   call is one the application defines. */
class RealOsdOutput : public OsdOutput
{
	public:
		Status setupFonts(FontSetup what) { return applicationSetupFonts(what); }
		Status setPalette() { return applicationSetPalette(); }
		Status setScreenGeometry() { return applicationSetScreenGeometry(); }
		Status resetLcd4lParse() { return applicationResetLcd4lParse(); }
		Status resetInfoViewer() { return applicationResetInfoViewer(); }
		Status clearInfoClock() { return applicationClearInfoClock(); }
		Status refreshVolumeBar() { return applicationRefreshVolumeBar(); }
		Status refreshMuteIcon() { return applicationRefreshMuteIcon(); }
		Status resetRadioText() { return applicationResetRadioText(); }
		Status clearIconCache() { return applicationClearIconCache(); }
		Status resetInfoIcons() { return applicationResetInfoIcons(); }
		Status resetChannelList() { return applicationResetChannelList(); }
};

RealOsdOutput g_real_osd_output;

} // namespace

void installRealOsdOutput()
{
	setOsdOutput(&g_real_osd_output);
}

} // namespace coreapi
