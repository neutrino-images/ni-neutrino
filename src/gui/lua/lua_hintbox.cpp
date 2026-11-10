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

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <gui/widget/hintbox.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_hintbox.h"

CLuaInstHintbox* CLuaInstHintbox::getInstance()
{
	static CLuaInstHintbox* LuaInstHintbox = NULL;

	if(!LuaInstHintbox)
		LuaInstHintbox = new CLuaInstHintbox();
	return LuaInstHintbox;
}

void CLuaInstHintbox::HintboxRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new", CLuaInstHintbox::HintboxNew },
		{ "exec", CLuaInstHintbox::HintboxExec },
		{ "paint", CLuaInstHintbox::HintboxPaint },
		{ "hide", CLuaInstHintbox::HintboxHide },
		{ "__gc", CLuaInstHintbox::HintboxDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "hintbox");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "hintbox");
}

CLuaHintbox *CLuaInstHintbox::HintboxCheck(lua_State *L, int n)
{
	return *(CLuaHintbox **) luaL_checkudata(L, n, "hintbox");
}

int CLuaInstHintbox::HintboxNew(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	/* The parameters a script names in its table. What is done with them is
	 * done by the script API, so that both bindings ask for one box and
	 * cannot come to differ. */
	ScriptHintbox::params_t pars;
	pars.icon = std::string(NEUTRINO_ICON_INFO);
	pars.width = HINTBOX_MIN_WIDTH;

	tableLookup(L, "name", pars.name) || tableLookup(L, "title", pars.name) || tableLookup(L, "caption", pars.name);
	tableLookup(L, "text", pars.text);
	tableLookup(L, "icon", pars.icon);

	lua_Integer width = (lua_Integer)pars.width;
	tableLookup(L, "width", width);
	pars.width = (int)width;

	CLuaHintbox **udata = (CLuaHintbox **) lua_newuserdata(L, sizeof(CLuaHintbox *));
	*udata = new CLuaHintbox(pars);
	luaL_getmetatable(L, "hintbox");
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstHintbox::HintboxDelete(lua_State *L)
{
	CLuaHintbox *D = HintboxCheck(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}

int CLuaInstHintbox::HintboxPaint(lua_State *L)
{
	CLuaHintbox *D = HintboxCheck(L, 1);
	if (!D) return 0;
	D->paint();
	return 0;
}

int CLuaInstHintbox::HintboxHide(lua_State *L)
{
	CLuaHintbox *D = HintboxCheck(L, 1);
	if (!D) return 0;
	D->hide();
	return 0;
}

int CLuaInstHintbox::HintboxExec(lua_State *L)
{
	CLuaHintbox *D = HintboxCheck(L, 1);
	if (!D) return 0;

	int timeout = -1;
	if (lua_isnumber(L, -1))
		timeout = (int) lua_tonumber(L, -1);

	CLuaInstance::scriptUiOpen();
	D->exec(timeout);
	CLuaInstance::scriptUiClose();
	return 0;
}
