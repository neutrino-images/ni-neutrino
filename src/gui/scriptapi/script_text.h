/*
 * script text
 *
 * A line of text a script shows, in the language the scripts are written in:
 * neither this nor anything it calls knows which interpreter asked for it. A
 * binding reads its own table, fills a ScriptText::params and hands it over.
 *
 * What both bindings wrote down twice is here once: the words of the mode as a
 * script names them, the font a text falls back to, the size it takes from the
 * form it goes into, the resolution of a colour it names as one of the box's
 * own - and who deletes it again.
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

#ifndef _SCRIPTTEXT_H
#define _SCRIPTTEXT_H

#include <string>

#include <gui/components/cc.h>

/* The text a script shows. */
class ScriptText
{
	public:
		/* What a script asked for when it made the text. A binding fills
		 * this from its own table, so the field names are the ones the
		 * bindings' parameters are. */
		typedef struct params {
			CComponentsForm	*parent;	/* the form it goes into, if any */
			int		x, y, dx, dy;
			std::string	text;
			/* the mode in words, as the script wrote it, or empty;
			 * the older parameter 'mode' is the same thing */
			std::string	mode;
			int		font_text;
			int		shadow_mode;
			unsigned long	color_text, color_frame, color_body, color_shadow;
		} params_t;

		explicit ScriptText(const params_t &pars);
		virtual ~ScriptText();

		/* The mode a script names in words, as the lua binding read it out
		 * of 'text_mode' (or the older 'mode'), and with DECODE_HTML in it,
		 * which is not a mode but a request to decode the text. Public,
		 * because a call that puts a new text in keeps the mode of the one
		 * it had. */
		static int	readMode(const std::string &words, std::string &text);

		/* the text itself, for a binding that has a call of its own on it */
		CComponentsText	*ct;
		/* the form it went into: a form deletes what it holds, so a text
		 * with a parent is not ours to delete */
		CComponentsForm	*parent;
		/* what it was made with, for the calls that ask again */
		int		mode, font_text;
};

#endif /* _SCRIPTTEXT_H */
