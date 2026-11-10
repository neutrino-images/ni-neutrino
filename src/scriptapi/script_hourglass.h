/*
 * script hourglass
 *
 * The hourglass a script shows, in the language the scripts are written in:
 * neither this nor anything it calls knows which interpreter asked for it. A
 * binding reads its own table, fills a ScriptHourglass::params and hands it
 * over.
 *
 * What both bindings wrote down twice is here once: the image it turns, the
 * interval it turns in, the colours of the frame, the background and the
 * shadow it names as ones of the box's own - and that the agent behind its
 * painting is stopped before it goes.
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

#ifndef _SCRIPTHOURGLASS_H
#define _SCRIPTHOURGLASS_H

#include <stdint.h>
#include <string>

#include <gui/widget/hourglass.h>

/* The hourglass a script shows. */
class ScriptHourglass
{
	public:
		/* What a script asked for when it made the hourglass. A binding
		 * fills this from its own table, so the field names are the ones the
		 * bindings' parameters are. */
		typedef struct params {
			CComponentsForm	*parent;	/* the form it goes into, if any */
			int		x, y, dx, dy;
			std::string	image_basename;
			int64_t		interval;
			int		shadow_mode;
			unsigned long	color_frame, color_background, color_shadow;
		} params_t;

		explicit ScriptHourglass(const params_t &pars);
		virtual ~ScriptHourglass();

		/* the hourglass itself, for a binding that has a call of its own */
		CHourGlass		*h;
		/* the form it went into, if any: a form deletes what it holds */
		CComponentsForm		*parent;
};

#endif /* _SCRIPTHOURGLASS_H */
