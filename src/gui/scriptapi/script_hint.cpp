/*
 * script hint
 *
 * The hint a script shows, made here for both bindings: script_hint.h says what
 * of it is shared and what a binding still reads itself.
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

#include "script_hint.h"

#include <config.h>

ScriptHint::ScriptHint(const params_t &pars)
{
	h = new CHint(pars.text.c_str(), pars.show_background);
}

ScriptHint::~ScriptHint()
{
	/* Nothing holds a hint: the scripts' own are the only ones there are, so
	 * this one is always ours to delete. */
	delete h;
}
