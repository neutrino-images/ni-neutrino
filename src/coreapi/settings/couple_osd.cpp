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

#include <system/settings.h>

namespace coreapi
{
namespace settings
{

/* The picture fix of the SCART output moves the drawn area and the fonts with it: switched on
   it takes the second preset with the corners the fix needs and the unscaled fonts, switched
   off it puts the corners and the font scaling back to the program's own. The preset stays
   where it is on the way back, since the user may have chosen it for itself meanwhile. */
static void coupleScartFix(CoupledBatch &b)
{
	long on = 0;
	if (!b.writtenNumber("flag_scart_osd_fix", on))
		return;

	const char *const why = "flag_scart_osd_fix";
	if (on != 0)
	{
		b.imply("screen_StartX_b_0", "30", why);
		b.imply("screen_StartY_b_0", "45", why);
		b.imply("screen_EndX_b_0", "690", why);
		b.imply("screen_EndY_b_0", "535", why);
		b.imply("screen_preset", "1", why);
		b.imply("font_scaling_x", "100", why);
		b.imply("font_scaling_y", "100", why);
	}
	else
	{
		b.imply("screen_StartX_b_0", "22", why);
		b.imply("screen_StartY_b_0", "12", why);
		b.imply("screen_EndX_b_0", "1236", why);
		b.imply("screen_EndY_b_0", "695", why);
		b.imply("font_scaling_x", "105", why);
		b.imply("font_scaling_y", "105", why);
	}
}

/* The two infobar icon settings hold one rule: the icons are not drawn in the skin the infobar
   draws itself. Their rows' conditions only grey the menu; a write is judged here on the pair it
   leaves, the partner from the batch or else the store, so either can be written alone and both
   together in any order. */
const KeyPair kJudgedPairs[] =
{
	{ "mode_icons", "mode_icons_skin" }
};

const size_t kJudgedPairCount = sizeof(kJudgedPairs) / sizeof(kJudgedPairs[0]);

static void coupleInfoIcons(CoupledBatch &b)
{
	const char *const mode_key = kJudgedPairs[0].first;
	const char *const skin_key = kJudgedPairs[0].second;
	if (b.written(mode_key) == NULL && b.written(skin_key) == NULL)
		return;
	long mode = 0;
	long skin = 0;
	if (!b.currentNumber(mode_key, mode) || !b.currentNumber(skin_key, skin))
		return;
	if (mode == 0 || skin != INFOICONS_INFOVIEWER)
		return;
	/* A pair already stored so, from a loaded file, is not this write's doing: one that
	   names either unchanged is left to the rule that an unchanged member is not judged. */
	if (!b.changes(mode_key) && !b.changes(skin_key))
		return;
	static const char *const kSaid = "the icons are not drawn in the skin the infobar draws itself";
	b.refuse(mode_key, kSaid, skin_key);
	b.refuse(skin_key, kSaid, mode_key);
}

/* Whether the infobar shows the module line follows from where it puts it. Writing the
   position decides the flag, and a flag the same write names otherwise is refused with
   the position, so the two cannot be left out of step. The flag written alone stays a
   write of the flag: the infobar itself toggles it while the position stays. */
void coupleOsd(CoupledBatch &b)
{
	long position = 0;
	if (b.writtenNumber("show_ecm_pos", position))
		b.imply("show_ecm", position != 0 ? "1" : "0", "show_ecm_pos");

	coupleScartFix(b);
	coupleInfoIcons(b);
}

} // namespace settings
} // namespace coreapi
