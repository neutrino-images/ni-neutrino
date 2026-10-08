/*
 * couple_lcd4l.cpp - what the LCD4Linux panel limits
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
#include <cstdlib>

namespace coreapi
{
namespace settings
{

long lcd4lBrightnessCeiling(long display_type)
{
	// The Samsung panels take ten; the Pearl one, and any type not known, seven.
	return (display_type >= 1 && display_type <= 3) ? 10 : 7;
}

bool lcd4lSkinOffered(long display_type, long skin)
{
	// Only the Pearl panel has the skins 1 to 3.
	return display_type == 0 || skin < 1 || skin > 3;
}

namespace
{

bool numberOfText(const std::string &text, long &out)
{
	char *end = NULL;
	out = std::strtol(text.c_str(), &end, 10);
	return end != text.c_str() && *end == '\0';
}

/* The value the batch leaves under key: the one it names, else the one stored. */
bool valueAfter(CoupledBatch &b, const char *key, long &out)
{
	if (b.writtenNumber(key, out))
		return true;
	std::string text;
	return b.current(key, text) && numberOfText(text, out);
}

void keepBrightness(CoupledBatch &b, const char *key, long type, bool moved)
{
	long value = 0;
	if (!valueAfter(b, key, value) || value <= lcd4lBrightnessCeiling(type))
		return;
	if (moved)
	{
		char fix[16];
		std::snprintf(fix, sizeof(fix), "%ld", lcd4lBrightnessCeiling(type));
		b.imply(key, fix, "lcd4l_display_type");
		return;
	}
	b.refuse(key, "the panel takes a lower brightness", "lcd4l_display_type");
}

}

/* The panel decides what the brightness and the skin may be. A write of the type brings
   the values it leaves behind back inside, as the screen did when it opened, and a value
   the batch names that the panel does not take is refused, whichever way it was written. */
void coupleLcd4l(CoupledBatch &b)
{
	long type = 0;
	long named = 0;
	const bool moved = b.writtenNumber("lcd4l_display_type", named);
	if (!valueAfter(b, "lcd4l_display_type", type))
		return;

	keepBrightness(b, "lcd4l_brightness", type, moved);
	keepBrightness(b, "lcd4l_brightness_standby", type, moved);

	long skin = 0;
	if (!valueAfter(b, "lcd4l_skin", skin) || lcd4lSkinOffered(type, skin))
		return;
	if (moved)
		b.imply("lcd4l_skin", "0", "lcd4l_display_type");
	else
		b.refuse("lcd4l_skin", "the panel has no such skin", "lcd4l_display_type");
}

} // namespace settings
} // namespace coreapi
