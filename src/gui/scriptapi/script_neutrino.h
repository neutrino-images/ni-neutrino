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
 * What is not here, and why: PaintIcon, DisplayImage, GetSize and scale2Res
 * are one call each into the box - fbwin->paintIcon, g_PicViewer->getSize,
 * CFrameBuffer::scale2Res - and the ten to fifteen lines around them are the
 * reading of the binding's own syntax. A wrapper over one call would be
 * forwarding and nothing else, so both bindings keep them as they are.
 *
 * What is left of this side is the map of saved screens: saveScreen,
 * restoreScreen and deleteSavedScreen walk it in both bindings - look a screen
 * up by the number it was given, hand it out, forget it, or free it.
 *
 * The making of a window itself stays with the bindings as well, and for the
 * same reason: new CFBWindow over the box's own screen, and rcinput = g_RCInput
 * - two lines, with the defaults out of g_settings and the reading of what the
 * script named around them, which belongs to the call of each language.
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
#include <string>

#include <global.h>
#include <neutrino.h>
#include <system/settings.h>

#include <driver/fb_generic.h>
#include <driver/fb_window.h>
#include <driver/neutrinofonts.h>

#include <gui/movieplayer.h>

class CRCInput;
class Font;

/* The dynamic fonts one window of the API owns, under the numbers the script
 * made them with. Both bindings hold one of these in the handle of their
 * window, and hand it over here. */
typedef std::map<long, Font *> ScriptFontMap;

class ScriptNeutrino
{
	public:
		/* The end of a window of the API, the same in both bindings down to the
		 * order: the screens it saved are freed, its two maps are let go of, the
		 * box's dynamic fonts are dropped, and a movie the script left playing
		 * behind it is taken up again. The window and its input go last.
		 *
		 * It answers whether that movie was taken up, because one binding has
		 * an older way of its own beside it - a channel that is rezapped where
		 * the video had gone blank - and only that binding can follow it.
		 *
		 * A template for the same reason the others are: the two maps belong to
		 * the binding and keep their keys under types of its own. */
		template <typename ScreenMapT, typename FontMapT>
		static bool WindowDrop(CFBWindow *&fbwin, CRCInput *&rcinput, ScreenMapT &screenmap, FontMapT &fontmap)
		{
			for (typename ScreenMapT::iterator it = screenmap.begin(); it != screenmap.end(); ++it)
				if (it->second != NULL)
					delete[] it->second;
			screenmap.clear();
			fontmap.clear();

			CNeutrinoFonts::getInstance()->deleteDynFontExtAll();

			/* restoreNeutrino at plugin closing, when blocked from plugin */
			bool restored = false;
			if (CMoviePlayerGui::getInstance().getBlockedFromPlugin() &&
			    CMoviePlayerGui::getInstance().Playing())
			{
				CMoviePlayerGui::getInstance().setBlockedFromPlugin(false);
				CMoviePlayerGui::getInstance().restoreNeutrino();
				restored = true;
			}

			delete fbwin;
			fbwin = NULL;
			rcinput = NULL;

			return restored;
		}

		/* w.GetInput(timeout): one message out of the window's input, and where
		 * that message is one of the bigger ones, handed on to the box - both
		 * bindings did that, with the same doubt written beside it. Answers
		 * the message and the data that came with it. */
		static void GetInput(CRCInput *rcinput, int timeout,
				neutrino_msg_t &msg, neutrino_msg_data_t &data);

		/* The map of screens a window has saved, walked the same way in both
		 * bindings: a screen is looked up by the number it was given, and then
		 * either handed out as it is, handed out and forgotten, or freed. The
		 * buffer came from the framebuffer window, so it is freed where the
		 * same shape is written down, not in two places.
		 *
		 * A template for the same reason FontOf is one: the Lua map and the
		 * Python one keep their numbers under keys of different types. */
		template <typename MapT>
		static fb_pixel_t *ScreenTake(MapT &screenmap, long id, bool forget)
		{
			for (typename MapT::iterator it = screenmap.begin(); it != screenmap.end(); ++it)
			{
				if (it->first == id)
				{
					fb_pixel_t *buf = it->second;
					if (forget)
						it->second = NULL;
					return buf;
				}
			}
			return NULL;
		}

		template <typename MapT>
		static void ScreenDrop(MapT &screenmap, long id)
		{
			for (typename MapT::iterator it = screenmap.begin(); it != screenmap.end(); ++it)
			{
				if (it->first == id)
				{
					if (it->second != NULL)
					{
						delete[] it->second;
						it->second = NULL;
					}
					return;
				}
			}
		}

		/* The four answers a dynamic font can come back with: the numbers the
		 * DYNFONT table of both bindings hands to a script. */
		enum DynFontStatus {
			DynFontOk      = 0,	/* DYNFONT_NO_ERROR */
			DynFontNoRoom  = 1,	/* DYNFONT_MAXIMUM_FONTS */
			DynFontTooWide = 2,	/* DYNFONT_TOO_WIDE */
			DynFontTooHigh = 3	/* DYNFONT_TOO_HIGH */
		};

		/* w.getDynFont(dx, dy[, text, style]): the font this window makes, or
		 * NULL with the reason why not. fontID is the place in the window's own
		 * map the font gets - the binding hands it in, because that map is the
		 * one thing here whose shape belongs to the binding. */
		static Font *GetDynFont(int dx, int dy, int fontID, const std::string &text,
				int style, DynFontStatus &status);

		/* w.RenderString(font, text, x, y[, color, w, boxh, center]): the width
		 * the text takes, the centring in the box the script named, and the
		 * drawing itself. Reading the numbers stays with the binding - and with
		 * them whether a width was named at all, which the two calls tell apart
		 * in different ways. */
		struct RenderText {
			int x;
			int y;
			int w;
			int boxh;
			int center;
			unsigned long color;
			bool sized;	/* a width was named; else the rest of the window */
			RenderText();
		};

		/* Answers the width the text takes. */
		static int RenderString(CFBWindow *fbwin, Font *font, const std::string &text,
				const RenderText &params);

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
