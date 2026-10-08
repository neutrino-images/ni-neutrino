/*
 * couple_osd.cpp - what the infobar settings imply of each other
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

namespace coreapi
{
namespace settings
{

/* Whether the infobar shows the module line follows from where it puts it. Writing the
   position decides the flag, and a flag the same write names otherwise is refused with
   the position, so the two cannot be left out of step. The flag written alone stays a
   write of the flag: the infobar itself toggles it while the position stays. */
void coupleOsd(CoupledBatch &b)
{
	long position = 0;
	if (b.writtenNumber("show_ecm_pos", position))
		b.imply("show_ecm", position != 0 ? "1" : "0", "show_ecm_pos");
}

} // namespace settings
} // namespace coreapi
