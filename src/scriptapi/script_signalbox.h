/*
 * script signal box
 *
 * The signal box a script shows, in the language the scripts are written in:
 * neither this nor anything it calls knows which interpreter asked for it. A
 * binding reads its own table, fills a ScriptSignalbox::params and hands it
 * over.
 *
 * What both bindings wrote down twice is here once: the frontend the box shows
 * - the live one of the box, not one a script names - the way the bars stand,
 * and who deletes the box again.
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

#ifndef _SCRIPTSIGNALBOX_H
#define _SCRIPTSIGNALBOX_H

#include <gui/components/cc.h>

/* The signal box a script shows. */
class ScriptSignalbox
{
	public:
		/* What a script asked for when it made the box. A binding fills this
		 * from its own table, so the field names are the ones the bindings'
		 * parameters are. */
		typedef struct params {
			CComponentsForm	*parent;	/* the form it goes into, if any */
			int		x, y, dx, dy;
			/* the bars of the box stand upright, or lie flat */
			bool		vertical;
		} params_t;

		explicit ScriptSignalbox(const params_t &pars);
		virtual ~ScriptSignalbox();

		/* the box itself, for a binding that has a call of its own on it */
		CSignalBox		*s;
		/* the form it went into: a form deletes what it holds, so a box with
		 * a parent is not ours to delete */
		CComponentsForm		*parent;
};

#endif /* _SCRIPTSIGNALBOX_H */
