/*
 * script picture
 *
 * The picture a script shows, made here for both bindings: scriptpicture.h says
 * what of it is shared and what a binding still reads itself.
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

#include "scriptpicture.h"

#include <config.h>

#include "scriptcolor.h"

ScriptPicture::ScriptPicture(const params_t &pars)
{
	parent = pars.parent;

	unsigned long color_frame      = ScriptColorResolve(pars.color_frame);
	unsigned long color_background = ScriptColorResolve(pars.color_background);
	unsigned long color_shadow     = ScriptColorResolve(pars.color_shadow);

	/* A picture that names no size is drawn as it is; the widget has a
	 * constructor of its own for that, and the other one would give it the
	 * size the script named - which are both zero here. */
	if (pars.dx == 0 && pars.dy == 0) /* NO_SCALE */
		cp = new CComponentsPicture(pars.x, pars.y, pars.image, parent, pars.has_shadow,
			(fb_pixel_t)color_frame, (fb_pixel_t)color_background,
			(fb_pixel_t)color_shadow, pars.transparency);
	else
		cp = new CComponentsPicture(pars.x, pars.y, pars.dx, pars.dy, pars.image, parent, pars.has_shadow,
			(fb_pixel_t)color_frame, (fb_pixel_t)color_background,
			(fb_pixel_t)color_shadow, pars.transparency);

	/* as both bindings set it after making the picture */
	cp->doPaintBg(true);
	cp->setCornerType(CORNER_NONE);
}

ScriptPicture::~ScriptPicture()
{
	/* A picture that went into a form belongs to that form, which deletes
	 * what it holds; only one without a parent is ours to delete. */
	if (parent == NULL)
		delete cp;
}
