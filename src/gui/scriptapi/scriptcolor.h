/*
 * script colour
 *
 * A colour a script names as one of the box's own carries a mark in its high
 * bits, and the number in the low byte is the index into the palette the
 * display was given. Both bindings carried these two constants and their own
 * copy of the resolution, which is what this holds once instead.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SCRIPTCOLOR_H
#define _SCRIPTCOLOR_H

#include <driver/framebuffer.h>

#define MAGIC_COLOR	0x42424200
#define MAGIC_MASK	0xFFFFFF00

/* A colour a script names as one of the box's own carries the magic the colour
 * tables are built with, and is resolved here to the pixel the display holds
 * for it.
 *
 * Each binding carried a function of its own for this and the window of a
 * script a third; the rule is the same in all three and is written once, so
 * that neither can come to resolve a colour differently. */
static inline unsigned long ScriptColorResolve(unsigned long color)
{
	if ((color & MAGIC_MASK) == MAGIC_COLOR)
		color = CFrameBuffer::getInstance()->realcolor[color & 0x000000ff];
	return color;
}

#endif /* _SCRIPTCOLOR_H */
