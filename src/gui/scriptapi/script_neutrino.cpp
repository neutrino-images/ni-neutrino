/*
 * the clamped drawing the neutrino surface shares
 *
 * Both bindings read the numbers of their own syntax and hand them over here;
 * what happens with them stood twice, word for word.
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

#include <config.h>

#include "script_color.h"
#include "script_neutrino.h"

/* The three notes below are the ones both bindings carried: the numbers a
 * script gives are clamped to the framebuffer window, and the comment that
 * says this belongs into CFBWindow is left where it stood. */
void ScriptNeutrino::PaintBox(CFBWindow *fbwin, int x, int y, int w, int h,
		unsigned long color, int radius, int corner)
{
	if (!fbwin)
		return;

	/* those checks should be done in CFBWindow instead... */
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (w < 0 || x + w > fbwin->dx)
		w = fbwin->dx - x;
	if (h < 0 || y + h > fbwin->dy)
		h = fbwin->dy - y;

	fbwin->paintBoxRel(x, y, w, h, (fb_pixel_t)ScriptColorResolve(color), radius, corner);
}

void ScriptNeutrino::PaintHLineRel(CFBWindow *fbwin, int x, int dx, int y, unsigned long color)
{
	if (!fbwin)
		return;

	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (dx < 0 || x + dx > fbwin->dx)
		dx = fbwin->dx - x;

	fbwin->paintHLineRel(x, dx, y, (fb_pixel_t)ScriptColorResolve(color));
}

void ScriptNeutrino::PaintVLineRel(CFBWindow *fbwin, int x, int y, int dy, unsigned long color)
{
	if (!fbwin)
		return;

	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (dy < 0 || y + dy > fbwin->dy)
		dy = fbwin->dy - y;

	fbwin->paintVLineRel(x, y, dy, (fb_pixel_t)ScriptColorResolve(color));
}
