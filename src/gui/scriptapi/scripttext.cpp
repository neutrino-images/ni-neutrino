/*
 * script text
 *
 * The text a script shows, made here for both bindings: scripttext.h says what
 * of it is shared and what a binding still reads itself.
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

#include "scripttext.h"

#include <config.h>
#include <global.h>

#include <system/settings.h>
#include <system/helpers.h>
#include <neutrino.h>

#include "scriptcolor.h"

/* A text that names no size and stands in no form gets one, as both bindings
 * gave it one. */
#define SCRIPT_TEXT_WIDTH	100
#define SCRIPT_TEXT_HEIGHT	100

ScriptText::ScriptText(const params_t &pars)
{
	parent = pars.parent;

	/* a font the settings do not hold is the menu's */
	font_text = pars.font_text;
	if (font_text >= SNeutrinoSettings::FONT_TYPE_COUNT || font_text < 0)
		font_text = SNeutrinoSettings::FONT_TYPE_MENU;

	/* the mode as words, or the width the text asks for by itself */
	std::string text = pars.text;
	mode = pars.mode.empty() ? CTextBox::AUTO_WIDTH : readMode(pars.mode, text);

	/* a text that names no size takes the one of the form it goes into */
	int dx = pars.dx, dy = pars.dy;
	if (parent)
	{
		if (dy < 1)
			dy = parent->getHeight();
		if (dx < 1)
			dx = parent->getWidth();
	}
	if (dx < 1)
		dx = SCRIPT_TEXT_WIDTH;
	if (dy < 1)
		dy = SCRIPT_TEXT_HEIGHT;

	ct = new CComponentsText(pars.x, pars.y, dx, dy, text, mode,
		g_Font[font_text], 0 /* style not passed */, parent, pars.shadow_mode,
		(fb_pixel_t)ScriptColorResolve(pars.color_text),
		(fb_pixel_t)ScriptColorResolve(pars.color_frame),
		(fb_pixel_t)ScriptColorResolve(pars.color_body),
		(fb_pixel_t)ScriptColorResolve(pars.color_shadow));
}

ScriptText::~ScriptText()
{
	/* A text that went into a form belongs to that form, which deletes what
	 * it holds; only one without a parent is ours to delete. */
	if (parent == NULL)
		delete ct;
}

int ScriptText::readMode(const std::string &words, std::string &text)
{
	static const struct { const char *name; int code; } align[] = {
		{ "ALIGN_AUTO_WIDTH",		CTextBox::AUTO_WIDTH },
		{ "ALIGN_AUTO_HIGH",		CTextBox::AUTO_HIGH },
		{ "ALIGN_SCROLL",		CTextBox::SCROLL },
		{ "ALIGN_CENTER",		CTextBox::CENTER },
		{ "ALIGN_RIGHT",		CTextBox::RIGHT },
		{ "ALIGN_TOP",			CTextBox::TOP },
		{ "ALIGN_BOTTOM",		CTextBox::BOTTOM },
		{ "ALIGN_NO_AUTO_LINEBREAK",	CTextBox::NO_AUTO_LINEBREAK },
		{ "DECODE_HTML",		0 },
		{ NULL,				0 }
	};

	int mode = 0;
	for (int i = 0; align[i].name; i++)
		if (words.find(align[i].name) != std::string::npos)
			mode |= align[i].code;
	if (words.find("DECODE_HTML") != std::string::npos)
		htmlEntityDecode(text);
	return mode;
}
