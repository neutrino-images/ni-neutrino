/*
 * lua instance helper functions
 *
 * (C) 2013 Stefan Seyfried (seife)
 * (C) 2014-2015 M. Liebmann (micha-bbg)
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
#include <neutrino.h>

#include "luainstance.h"
#include <scriptapi/script_color.h>
#include <scriptapi/script_deprecated.h>

/* Ported by Benny Chen, http://www.bennychen.cn/tag/lual_checkbool/ */
bool _luaL_checkbool(lua_State *L, int numArg)
{
	bool b = false;
	if (lua_isboolean(L, numArg))
		b = lua_toboolean(L, numArg);
	else {
		lua_Debug ar;
		lua_getstack(L, 0, &ar);
		lua_getinfo(L, "n", &ar);
		luaL_error(L, "bad argument #%d to '%s' (%s expected, got %s)\n",
				numArg-1, ar.name,
				lua_typename(L, LUA_TBOOLEAN),
				lua_typename(L, lua_type(L, numArg)));
	}
	return b;
}

/* Where the script stands, as the notices below name it. The sentences
 * themselves stand in the script API, where the Python binding reads the same
 * ones: scriptapi/script_deprecated.h. */
static std::string luaScriptWhere(lua_State *L)
{
	lua_Debug ar;
	lua_getstack(L, 1, &ar);
	lua_getinfo(L, "Sl", &ar);
	return std::string(ar.short_src) + ":" + to_string(ar.currentline);
}

void paramBoolDeprecated(lua_State *L, const char* val)
{
	ScriptDeprecated::paramBool("Lua", luaScriptWhere(L), val, "true");
}

void paramDeprecated(lua_State *L, const char* oldParam, const char* newParam)
{
	ScriptDeprecated::param("Lua", luaScriptWhere(L), oldParam, newParam);
}

void functionDeprecated(lua_State *L, const char* oldFunc, const char* newFunc)
{
	ScriptDeprecated::function("Lua", luaScriptWhere(L), oldFunc, newFunc);
}

void obsoleteHideParameter(lua_State *L)
{
	ScriptDeprecated::hideParameter("Lua", luaScriptWhere(L));
}


/* A colour a script named as one of the box's own is resolved where the window
 * of a script resolves it, so that a window and a widget of a script cannot
 * come to resolve one differently. */
bool tableLookup(lua_State *L, const char *what, std::string &value)
{
	lua_pushstring(L, what);
	lua_gettable(L, -2);
	bool res = lua_isstring(L, -1);
	if (res)
		value = lua_tostring(L, -1);
	lua_pop(L, 1);
	return res;
}

bool tableLookup(lua_State *L, const char *what, lua_Integer &value)
{
	lua_pushstring(L, what);
	lua_gettable(L, -2);
	bool res = lua_isnumber(L, -1);
	if (res)
		value = (lua_Integer) lua_tonumber(L, -1);
	lua_pop(L, 1);
	return res;
}

bool tableLookup(lua_State *L, const char *what, lua_Unsigned &value)
{
	lua_pushstring(L, what);
	lua_gettable(L, -2);
	bool res = lua_isnumber(L, -1);
	if (res)
		value = (lua_Unsigned) lua_tonumber(L, -1);
	lua_pop(L, 1);
	return res;
}

bool tableLookup(lua_State *L, const char *what, void** value)
{
	lua_pushstring(L, what);
	lua_gettable(L, -2);
	bool res = lua_isuserdata(L, -1);
	if (res)
		*value = lua_unboxpointer(L, -1);
	lua_pop(L, 1);
	return res;
}

bool tableLookup(lua_State *L, const char *what, bool &value)
{
	lua_pushstring(L, what);
	lua_gettable(L, -2);
	bool res = lua_isboolean(L, -1);
	if (res)
		value = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return res;
}
