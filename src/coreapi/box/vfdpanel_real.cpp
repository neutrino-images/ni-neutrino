/*
 * vfdpanel_real.cpp - the front panel group's seam bound to the program's driver
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

#include "coreapi/box/apply_vfd.h"

namespace coreapi
{

namespace
{

class RealVfdPanel : public VfdPanel
{
	public:
		Status setBrightness(Brightness which, int value)
		{
			return applicationVfdBrightness((int) which, value);
		}

		Status setScrollMode(int repeats)
		{
			return applicationVfdScroll(repeats);
		}

		Status refreshLeds()
		{
			return applicationVfdLeds();
		}

		Status refreshParameters()
		{
			return applicationVfdParameters();
		}

		Status setBacklight(int on)
		{
			return applicationVfdBacklight(on);
		}

		Status showStatusline(int mode, int volume)
		{
			return applicationVfdStatusline(mode, volume);
		}
};

RealVfdPanel g_real_vfd_panel;

} // namespace

void installRealVfdPanel()
{
	setVfdPanel(&g_real_vfd_panel);
}

} // namespace coreapi
