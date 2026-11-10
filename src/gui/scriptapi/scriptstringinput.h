/*
 * script string input
 *
 * The dialog a script asks a value in, in the language the scripts are written
 * in: neither this nor anything it calls knows which interpreter asked for it.
 * A binding reads its own table, fills a ScriptStringinput::params and calls.
 *
 * There is no widget to hold here: the dialog comes and goes within the call,
 * so this is a call and not a class a binding keeps.
 *
 * What both bindings wrote down twice is here once: the size a value has when a
 * script names none, the characters it may hold, the icon, the pin input that
 * is a dialog of its own, and the word when a script asks for both the sms and
 * the pin input. The characters were even written in two versions - the lua
 * binding allowed a paragraph sign the python one did not - and the one that
 * allows more is kept, so that no script loses a character it had.
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

#ifndef _SCRIPTSTRINGINPUT_H
#define _SCRIPTSTRINGINPUT_H

#include <string>

/* The dialog a script asks a value in. */
class ScriptStringinput
{
	public:
		/* What a script asked for. A binding fills this from its own table,
		 * so the field names are the ones the bindings' parameters are; what
		 * a script did not name comes from the defaults, which both bindings
		 * carried before. */
		typedef struct params {
			std::string	name;		/* the caption: name, title or caption */
			std::string	value;
			int		size;
			std::string	valid_chars;
			std::string	icon;
			bool		sms;		/* read, and of no use: the dialog brings its own keyboard */
			bool		pin;		/* the pin input is a dialog of its own */
			params(void);
		} params_t;

		/* Shows the dialog and answers what stands in it when it is done. */
		static std::string	exec(const params_t &pars);
};

#endif /* _SCRIPTSTRINGINPUT_H */
