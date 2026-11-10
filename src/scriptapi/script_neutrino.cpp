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
#include <cstdio>

#include <global.h>
#include <system/settings.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>

#include "script_color.h"
#include "script_neutrino.h"

/* The text of RenderString: where it goes, how wide the box is that it is
 * centred in, and the note that a negative boxh means no string at all. */
ScriptNeutrino::RenderText::RenderText()
	: x(0), y(0), w(0), boxh(0), center(0), color(0), sized(false)
{
}

int ScriptNeutrino::RenderString(CFBWindow *fbwin, Font *font, const std::string &text,
		const RenderText &params)
{
	if (!fbwin || !font)
		return 0;

	int x = params.x;
	int w = params.sized ? params.w : fbwin->dx - params.x;
	const int rwidth = font->getRenderWidth(text.c_str());

	if (params.center) /* center the text inside the box */
	{
		if (rwidth < w)
			x += (w - rwidth) / 2;
	}
	unsigned long c = ScriptColorResolve(params.color);
	if (params.boxh > -1) /* if boxh < 0, don't paint string */
		fbwin->RenderString(font, x, params.y, w, text.c_str(), (fb_pixel_t)c, params.boxh);

	return rwidth;
}

/* One message out of the window's input. The hand-off below is the TODO both
 * bindings carried beside it, word for word: a message bigger than a key is
 * given to the box, and neither side was sure that it belongs here. It is
 * carried over as it stood. */
void ScriptNeutrino::GetInput(CRCInput *rcinput, int timeout, neutrino_msg_t &msg, neutrino_msg_data_t &data)
{
	if (!rcinput)
		return;

	rcinput->getMsg_ms(&msg, &data, timeout);

	if (msg != CRCInput::RC_timeout && msg > CRCInput::RC_MaxRC)
		CNeutrinoApp::getInstance()->handleMsg(msg, data);
}

/* The dynamic font a script asks this window for. Both bindings asked the same
 * three questions and called the same one; only reading the numbers of their
 * own syntax was theirs. */
Font *ScriptNeutrino::GetDynFont(int dx, int dy, int fontID, const std::string &text,
		int style, DynFontStatus &status)
{
	status = DynFontOk;

	if (fontID >= CNeutrinoFonts::DYNFONTEXT_MAX)
	{
		status = DynFontNoRoom;
		return NULL;
	}
	if (dx > (int)CFrameBuffer::getInstance()->getScreenWidth(true))
	{
		status = DynFontTooWide;
		return NULL;
	}
	if (dy > 100)
	{
		status = DynFontTooHigh;
		return NULL;
	}
	if (style > CNeutrinoFonts::FONT_STYLE_ITALIC)
		style = CNeutrinoFonts::FONT_STYLE_REGULAR;

	return CNeutrinoFonts::getInstance()->getDynFontExt(dx, dy, fontID, text, style);
}

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
