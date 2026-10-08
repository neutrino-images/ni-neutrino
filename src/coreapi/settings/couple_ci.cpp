/*
 * couple_ci.cpp - the pin a module keeps follows its switch
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

#include "couple.h"

#include <cstdio>

namespace coreapi
{
namespace settings
{

/* A pin is kept only for a slot that has the keeping switched on. Switching it off drops
   what was kept in the same write, so the file the write is saved to does not go on
   holding a pin nothing will use. A pin the same write names with text is refused with
   the switch. */
void coupleCi(CoupledBatch &b)
{
	for (int slot = 0; slot < 4; ++slot)
	{
		char keep[24];
		char pin[24];
		snprintf(keep, sizeof(keep), "ci_save_pincode_%d", slot);
		snprintf(pin, sizeof(pin), "ci_pincode_%d", slot);

		long on = 1;
		if (b.writtenNumber(keep, on) && on == 0)
			b.clear(pin, keep);
	}
}

} // namespace settings
} // namespace coreapi
