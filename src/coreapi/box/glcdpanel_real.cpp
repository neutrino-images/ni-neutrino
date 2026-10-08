/*
 * glcdpanel_real.cpp - the graphic display group's seam bound to the program's service
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

#include "coreapi/box/apply_glcd.h"

namespace coreapi
{

namespace
{

/* Where the build has no graphic display there is no service, and the settings of
   it are never written, so a run finds nothing to do. */
class RealGlcdPanel : public GlcdPanel
{
	public:
		Status setEnabled(bool on) { return act(GlcdEnable, on ? 1 : 0); }
		Status setMirrorOsd(bool on) { return act(GlcdMirrorOsd, on ? 1 : 0); }
		Status respawn() { return act(GlcdRespawn, 0); }
		Status reinitFont() { return act(GlcdReinitFont, 0); }
		Status updateBrightness() { return act(GlcdBrightness, 0); }
		Status update() { return act(GlcdUpdate, 0); }

		Status panelSize(int &width, int &height)
		{
			return recordedGlcdPanelSize(width, height);
		}

		Status recordPanelSize() { return act(GlcdRecordSize, 0); }

	private:
		Status act(int what, int value)
		{
#ifdef ENABLE_GRAPHLCD
			return applicationGlcd(what, value);
#else
			(void) what;
			(void) value;
			return Status::Ok;
#endif
		}
};

RealGlcdPanel g_real_glcd_panel;

} // namespace

void installRealGlcdPanel()
{
	setGlcdPanel(&g_real_glcd_panel);
}

} // namespace coreapi
