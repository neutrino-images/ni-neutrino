/*
 * script picture
 *
 * The picture a script shows, in the language the scripts are written in:
 * neither this nor anything it calls knows which interpreter asked for it. A
 * binding reads its own table, fills a ScriptPicture::params and hands it over.
 *
 * What both bindings wrote down twice is here once: the two ways a picture is
 * made - with a size of its own or drawn as it is - the colours of the frame,
 * the background and the shadow it names as ones of the box's own, and who
 * deletes it again.
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

#ifndef _SCRIPTPICTURE_H
#define _SCRIPTPICTURE_H

#include <string>

#include <gui/components/cc.h>

/* The picture a script shows. */
class ScriptPicture
{
	public:
		/* What a script asked for when it made the picture. A binding fills
		 * this from its own table, so the field names are the ones the
		 * bindings' parameters are. */
		typedef struct params {
			CComponentsForm	*parent;	/* the form it goes into, if any */
			int		x, y, dx, dy;
			/* A picture of no size (dx and dy both zero) is drawn as it
			 * is and gets no size of its own. */
			std::string	image;
			bool		has_shadow;
			int		transparency;
			unsigned long	color_frame, color_background, color_shadow;
		} params_t;

		explicit ScriptPicture(const params_t &pars);
		virtual ~ScriptPicture();

		/* the picture itself, for a binding that has a call of its own on it */
		CComponentsPicture	*cp;
		/* the form it went into: a form deletes what it holds, so a picture
		 * with a parent is not ours to delete */
		CComponentsForm		*parent;
};

#endif /* _SCRIPTPICTURE_H */
