/*
 * script header
 *
 * The header a script shows, in the language the scripts are written in:
 * neither this nor anything it calls knows which interpreter asked for it. A
 * binding reads its own table, fills a ScriptHeader::params and hands it over.
 *
 * What both bindings did differently is here once. The lua binding hung the
 * header on the window handle it had read, which is not a form, and handed on
 * an empty icon although it had read one, and it never read the buttons at
 * all; the python binding hung it on the form, handed the icon on and read the
 * buttons. The form is the one the widget takes, and the two parameters the
 * lua binding dropped are read there now.
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

#ifndef _SCRIPTHEADER_H
#define _SCRIPTHEADER_H

#include <string>

#include <gui/components/cc.h>

/* The header a script shows. */
class ScriptHeader
{
	public:
		/* What a script asked for when it made the header. A binding fills
		 * this from its own table, so the field names are the ones the
		 * bindings' parameters are. */
		typedef struct params {
			CComponentsForm	*parent;	/* the form it goes into, if any */
			int		x, y, dx, dy;
			std::string	caption;
			std::string	icon;
			int		buttons;
			int		shadow_mode;
			unsigned long	color_frame, color_body, color_shadow;
		} params_t;

		explicit ScriptHeader(const params_t &pars);
		virtual ~ScriptHeader();

		/* the header itself, for a binding that has a call of its own on it */
		CComponentsHeader	*ch;
		/* the form it went into: a form deletes what it holds, so a header
		 * with a parent is not ours to delete */
		CComponentsForm		*parent;
};

#endif /* _SCRIPTHEADER_H */
