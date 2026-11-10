/*
 * script hint box
 *
 * The message box a script shows, in the language the scripts are written in:
 * neither this nor anything it calls knows which interpreter asked for it. A
 * binding reads its own table, fills a ScriptHintbox::params and hands it over.
 *
 * What the box does while it is shown - waiting for a key and how long, what it
 * scrolls, and what it hands on to the program - is what the box does when a
 * menu of the program shows it, and is one implementation here.
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

#ifndef _SCRIPTHINTBOX_H
#define _SCRIPTHINTBOX_H

#include <string>

#include <gui/widget/hintbox.h>

/* The box a script shows a message in. */
class ScriptHintbox
{
	public:
		/* What a script asked for when it made the box. A binding fills
		 * this from its own table, so the field names are the ones the
		 * bindings' parameters are. */
		typedef struct params {
			std::string	name;
			std::string	text;
			std::string	icon;
			int		width;
		} params_t;

		explicit ScriptHintbox(const params_t &pars);
		virtual ~ScriptHintbox();

		/* the box itself, for a binding that has a call of its own on it */
		CHintBox	*b;

		void	paint(void);
		void	hide(void);
		/* Shows the box and waits for a key: what a menu does with a box
		 * it shows, and what a script does with one of its own. */
		void	exec(int timeout);

	private:
		/* the title, of which the box keeps the pointer it is given */
		char	*caption;
};

#endif /* _SCRIPTHINTBOX_H */
