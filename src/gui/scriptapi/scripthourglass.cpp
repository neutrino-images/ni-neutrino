/*
 * script hourglass
 *
 * The hourglass a script shows, made here for both bindings: scripthourglass.h
 * says what of it is shared and what a binding still reads itself.
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

#include "scripthourglass.h"

#include <config.h>

#include "scriptcolor.h"

ScriptHourglass::ScriptHourglass(const params_t &pars)
{
	parent = pars.parent;

	h = new CHourGlass(pars.x, pars.y, pars.dx, pars.dy, pars.image_basename,
		(int64_t)pars.interval, parent, pars.shadow_mode,
		(fb_pixel_t)ScriptColorResolve(pars.color_frame),
		(fb_pixel_t)ScriptColorResolve(pars.color_background),
		(fb_pixel_t)ScriptColorResolve(pars.color_shadow));
}

ScriptHourglass::~ScriptHourglass()
{
	/* The hourglass paints itself, and the agent behind that has to be
	 * stopped before it goes: the python binding did so, the lua one left it
	 * to the widget. A form deletes what it holds, so only an hourglass
	 * without a parent is ours to delete. */
	if (parent == NULL)
	{
		h->kill();
		delete h;
	}
}
