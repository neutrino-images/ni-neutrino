/*
 * the clamped drawing the neutrino surface shares
 *
 * A script gets a window of the box's own and draws into it: a box, a
 * horizontal line, a vertical line. Both bindings read that call in their own
 * way - positional arguments there, keyword arguments here - and then did the
 * same three things with it, twice: clamp the numbers to the framebuffer
 * window, resolve a colour a script names as one of the box's own, and call
 * into the window. Here it stands once.
 *
 * The clamping is guarded by the same note both bindings carried:
 *	"those checks should be done in CFBWindow instead..."
 * - left where it was, and now written down in one place instead of two.
 *
 * The corners of a box are named by the script that draws it; CORNER_ALL and
 * the rest live where they always did, and are handed in here, so that this
 * file does not carry a second set of them.
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

#ifndef _SCRIPT_NEUTRINO_H
#define _SCRIPT_NEUTRINO_H

#include <cstdio>
#include <map>

#include <global.h>
#include <system/settings.h>

#include <driver/fb_generic.h>
#include <driver/fb_window.h>

class Font;

/* The dynamic fonts one window of the API owns, under the numbers the script
 * made them with. Both bindings hold one of these in the handle of their
 * window, and hand it over here. */
typedef std::map<long, Font *> ScriptFontMap;

class ScriptNeutrino
{
	public:
		/* The font a call names: a number into the box's own fonts, or, where
		 * the call said so, one of this window's dynamic fonts. Where the
		 * window has no such font, found comes back false and the call is
		 * answered as the bindings answered it.
		 *
		 * A template, because the two bindings keep that map under different
		 * keys - int in the Lua one, long in the Python one - and the question
		 * asked of it is the same either way. */
		template <typename MapT>
		static Font *FontOf(const MapT &fontmap, long value, bool isDynFont, bool &found)
		{
			found = true;

			if (!isDynFont)
			{
				/* font number, use FONT_TYPE_XXX in the script */
				if (value >= SNeutrinoSettings::FONT_TYPE_COUNT || value < 0)
					value = SNeutrinoSettings::FONT_TYPE_MENU;
				return g_Font[value];
			}

			for (typename MapT::const_iterator it = fontmap.begin(); it != fontmap.end(); ++it)
				if (it->first == value)
					return it->second;

			printf("[ScriptNeutrino::%s] no font found\n", __func__);
			found = false;
			return NULL;
		}

		/* w.PaintBox(x, y, w, h, color[, radius[, corner]]) */
		static void PaintBox(CFBWindow *fbwin, int x, int y, int w, int h,
				unsigned long color, int radius, int corner);

		/* w.paintHLineRel(x, dx, y, color) */
		static void PaintHLineRel(CFBWindow *fbwin, int x, int dx, int y, unsigned long color);

		/* w.paintVLineRel(x, y, dy, color) */
		static void PaintVLineRel(CFBWindow *fbwin, int x, int y, int dy, unsigned long color);
};

#endif //_SCRIPT_NEUTRINO_H
