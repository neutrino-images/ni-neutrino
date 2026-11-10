/*
 * lua hintbox
 *
 * (C) 2014 by martii
 * (C) 2014-2015 M. Liebmann (micha-bbg)
 * (C) 2014 Sven Hoefer (svenhoefer)
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

#ifndef _LUAHINTBOX_H
#define _LUAHINTBOX_H

#include <gui/scriptapi/script_hintbox.h>

/* The box a script made, held as the shared ScriptHintbox: what is done with it
 * is the same for every interpreter, and this binding only fills its parameters
 * out of a lua table. */
class CLuaHintbox : public ScriptHintbox
{
	public:
		CLuaHintbox(const params_t &pars) : ScriptHintbox(pars) {}
};

class CLuaInstHintbox
{
	public:
		CLuaInstHintbox() {};
		~CLuaInstHintbox() {};
		static CLuaInstHintbox* getInstance();
		static void HintboxRegister(lua_State *L);

	private:
		static CLuaHintbox *HintboxCheck(lua_State *L, int n);
		static int HintboxNew(lua_State *L);
		static int HintboxDelete(lua_State *L);
		static int HintboxExec(lua_State *L);
		static int HintboxPaint(lua_State *L);
		static int HintboxHide(lua_State *L);
};

#endif //_LUAHINTBOX_H
