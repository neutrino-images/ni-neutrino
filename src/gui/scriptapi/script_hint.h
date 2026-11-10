/*
 * script hint
 *
 * The simple hint a script shows, in the language the scripts are written in:
 * neither this nor anything it calls knows which interpreter asked for it. A
 * binding reads its own table, fills a ScriptHint::params and hands it over.
 *
 * What both bindings wrote down twice is here once: that a hint lives alone -
 * no form holds it, so it is always ours to delete - and that its background is
 * shown when a script says nothing about it.
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

#ifndef _SCRIPTHINT_H
#define _SCRIPTHINT_H

#include <string>

#include <gui/widget/hintbox.h>

/* The hint a script shows. */
class ScriptHint
{
	public:
		/* What a script asked for when it made the hint. A binding fills
		 * this from its own table, so the field names are the ones the
		 * bindings' parameters are. */
		typedef struct params {
			std::string	text;
			bool		show_background;
		} params_t;

		explicit ScriptHint(const params_t &pars);
		virtual ~ScriptHint();

		/* the hint itself, for a binding that has a call of its own on it */
		CHint	*h;
};

#endif /* _SCRIPTHINT_H */
