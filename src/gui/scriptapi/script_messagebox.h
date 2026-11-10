/*
 * script message box
 *
 * The message box a script shows, in the language the scripts are written in:
 * neither this nor anything it calls knows which interpreter asked for it. A
 * binding reads its own table or its keywords, fills a
 * ScriptMessagebox::params and calls.
 *
 * There is no widget to hold: the box comes and goes within the call, so this
 * is a call and not a class a binding keeps.
 *
 * What both bindings wrote down twice is here once, and there was much of it:
 * the words a script names its buttons by and the codes behind them, the words
 * for the button alignment, the words for the button that is preselected, the
 * word each answer is turned back into, and the words of the text mode - the
 * last of which are the ones a text of a script uses, so they go through
 * ScriptText::readMode and stand in one place for both.
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

#ifndef _SCRIPTMESSAGEBOX_H
#define _SCRIPTMESSAGEBOX_H

#include <string>
#include <vector>

/* The message box a script shows. */
class ScriptMessagebox
{
	public:
		/* What a script asked for when it showed the box, in the words the
		 * script named: a binding fills this from its own table or its
		 * keywords and knows nothing of the codes behind them. The name of
		 * the caption, which a script may call name, title or caption, is
		 * the binding's business and arrives here as the one caption. */
		typedef struct params {
			std::string		name;
			std::string		text;
			std::string		icon;
			int			width;
			int			timeout;
			int			return_default_on_timeout;
			std::string		align;
			std::vector<std::string>	buttons;
			std::string		default_button;
			std::string		text_mode;
		} params_t;

		/* Shows the box and answers the word of the button that was taken. */
		static std::string	exec(const params_t &pars);
};

#endif /* _SCRIPTMESSAGEBOX_H */
