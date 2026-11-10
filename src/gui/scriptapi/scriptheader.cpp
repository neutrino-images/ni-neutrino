/*
 * script header
 *
 * The header a script shows, made here for both bindings: scriptheader.h says
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

#include "scriptheader.h"

#include <config.h>

#include "scriptcolor.h"

ScriptHeader::ScriptHeader(const params_t &pars)
{
	parent = pars.parent;

	ch = new CComponentsHeader(pars.x, pars.y, pars.dx, pars.dy,
		pars.caption, pars.icon, pars.buttons,
		parent, pars.shadow_mode,
		(fb_pixel_t)ScriptColorResolve(pars.color_frame),
		(fb_pixel_t)ScriptColorResolve(pars.color_body),
		(fb_pixel_t)ScriptColorResolve(pars.color_shadow));
}

ScriptHeader::~ScriptHeader()
{
	/* A header that went into a form belongs to that form, which deletes
	 * what it holds; only one without a parent is ours to delete. */
	if (parent == NULL)
		delete ch;
}
