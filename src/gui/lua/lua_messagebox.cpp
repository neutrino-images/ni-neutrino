/*
 * lua messagebox
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
#include <system/helpers.h>
#include <gui/widget/msgbox.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_messagebox.h"
#include <gui/scriptapi/script_messagebox.h>

CLuaInstMessagebox* CLuaInstMessagebox::getInstance()
{
	static CLuaInstMessagebox* LuaInstMessagebox = NULL;

	if(!LuaInstMessagebox)
		LuaInstMessagebox = new CLuaInstMessagebox();
	return LuaInstMessagebox;
}

void CLuaInstMessagebox::MessageboxRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "exec", CLuaInstMessagebox::MessageboxExec },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "messagebox");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "messagebox");
}

// messagebox.exec{caption="Title", text="text", icon="settings", width=500,timeout=-1,return_default_on_timeout=0,
//	default = "yes", buttons = { "yes", "no", "cancel", "all", "back", "ok" }, align="center1|center2|left|right" }
int CLuaInstMessagebox::MessageboxExec(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	ScriptMessagebox::params_t pars;
	pars.icon = std::string(NEUTRINO_ICON_INFO);
	pars.width = MSGBOX_MIN_WIDTH;
	pars.timeout = -1;
	pars.return_default_on_timeout = 0;

	lua_Integer timeout = pars.timeout, width = pars.width, return_default_on_timeout = pars.return_default_on_timeout;
	tableLookup(L, "name", pars.name) || tableLookup(L, "title", pars.name) || tableLookup(L, "caption", pars.name);
	tableLookup(L, "text", pars.text);
	tableLookup(L, "icon", pars.icon);
	tableLookup(L, "timeout", timeout);
	tableLookup(L, "width", width);
	tableLookup(L, "return_default_on_timeout", return_default_on_timeout);
	tableLookup(L, "text_mode", pars.text_mode);
	tableLookup(L, "align", pars.align);
	tableLookup(L, "default", pars.default_button);
	pars.timeout = (int)timeout;
	pars.width = (int)width;
	pars.return_default_on_timeout = (int)return_default_on_timeout;

	lua_pushstring(L, "buttons");
	lua_gettable(L, -2);
	// "buttons" may be left out; walking anything but a table crashes
	if (lua_istable(L, -1))
		for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 2)) {
			lua_pushvalue(L, -2);
			const char *val = lua_tostring(L, -2);
			if (val != NULL)
				pars.buttons.push_back(val);
		}
	lua_pop(L, 1);

	/* the words of the buttons, of the alignment and of the text mode are
	 * the script api's; this binding reads them and hands them over */
	CLuaInstance::scriptUiOpen();
	std::string answer = ScriptMessagebox::exec(pars);
	CLuaInstance::scriptUiClose();

	lua_pushstring(L, answer.c_str());

	return 1;
}
