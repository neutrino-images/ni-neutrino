/*
 * lua file helpers functions
 *
 * (C) 2016 M. Liebmann (micha-bbg)
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
#include <math.h>
#include <errno.h>
#include <utime.h>

#include <global.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <neutrino.h>
#include <gui/widget/msgbox.h>

#include <scriptapi/script_filehelpers.h>

#include "luainstance.h"
#include "lua_filehelpers.h"

#if LUA_VERSION_NUM >= 503 /* Lua 5.3 */

#ifndef luaL_checkint
#define luaL_checkint luaL_checkinteger
#endif

#endif /* Lua 5.3 */

CLuaInstFileHelpers* CLuaInstFileHelpers::getInstance()
{
	static CLuaInstFileHelpers* LuaInstFileHelpers = NULL;

	if (!LuaInstFileHelpers)
		LuaInstFileHelpers = new CLuaInstFileHelpers();
	return LuaInstFileHelpers;
}

CLuaFileHelpers *CLuaInstFileHelpers::FileHelpersCheckData(lua_State *L, int n)
{
	return *(CLuaFileHelpers **) luaL_checkudata(L, n, LUA_FILEHELPER_CLASSNAME);
}

/* What a failed call prints: where the script stands, what went wrong, and
 * where in neutrino it was noticed. The work itself stands in the script API
 * (scriptapi/script_filehelpers.h), where the Python binding reads it too. */
static void fileHelpersError(lua_State *L, const ScriptFileHelpers::Error &error)
{
	lua_Debug ar;
	lua_getstack(L, 1, &ar);
	lua_getinfo(L, "Sl", &ar);
	printf(">>> Lua script error [%s:%d] %s\n    (error from neutrino: [%s:%d])\n",
	       ar.short_src, ar.currentline, error.msg.c_str(), error.file.c_str(), error.line);
}

void CLuaInstFileHelpers::LuaFileHelpersRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",        CLuaInstFileHelpers::FileHelpersNew      },
		{ "cp",         CLuaInstFileHelpers::FileHelpersCp       },
		{ "chmod",      CLuaInstFileHelpers::FileHelpersChmod    },
		{ "touch",      CLuaInstFileHelpers::FileHelpersTouch    },
		{ "rmdir",      CLuaInstFileHelpers::FileHelpersRmdir    },
		{ "mkdir",      CLuaInstFileHelpers::FileHelpersMkdir    },
		{ "readlink",   CLuaInstFileHelpers::FileHelpersReadlink },
		{ "ln",         CLuaInstFileHelpers::FileHelpersLn       },
		{ "exist",      CLuaInstFileHelpers::FileHelpersExist    },
		{ "empty",      CLuaInstFileHelpers::FileHelpersIsEmpty  },
		{ "__gc",       CLuaInstFileHelpers::FileHelpersDelete   },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, LUA_FILEHELPER_CLASSNAME);
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, LUA_FILEHELPER_CLASSNAME);
}

int CLuaInstFileHelpers::FileHelpersNew(lua_State *L)
{
	CLuaFileHelpers **udata = (CLuaFileHelpers **) lua_newuserdata(L, sizeof(CLuaFileHelpers *));
	*udata = new CLuaFileHelpers();
	luaL_getmetatable(L, LUA_FILEHELPER_CLASSNAME);
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersCp(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 2;
	if (numargs < min_numargs) {
		printf("luascript cp: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushboolean(L, false);
		return 1;
	}

	if (!lua_isstring(L, 2) || !lua_isstring(L, 3)) {
		printf("%s: argument 1 or 2 is not a string.\n",__func__);
		lua_pushboolean(L, false);
		return 1;
	}
	const char *from = luaL_checkstring(L, 2);
	const char *to = luaL_checkstring(L, 3);

	const char *flags = "";
	if (numargs > min_numargs){
		if (!lua_isstring(L, 4)) {
			printf("%s: argument 3 is not a string.\n",__func__);
			lua_pushboolean(L, false);
			return 1;
		}
		flags = luaL_checkstring(L, 4);
	}
	ScriptFileHelpers::Error error;
	bool ret = ScriptFileHelpers::Cp(from, to, flags, error);
	if (!ret)
		fileHelpersError(L, error);

	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersChmod(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 2;
	if (numargs < min_numargs) {
		printf("luascript chmod: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushboolean(L, false);
		return 1;
	}
	if (!lua_isstring(L, 2)) {
		printf("%s: argument 1 is not a string.\n",__func__);
		lua_pushboolean(L, false);
		return 1;
	}
	const char *file = luaL_checkstring(L, 2);

	int mode_i = luaL_checkint(L, 3);

	std::string error = "";
	bool ret = ScriptFileHelpers::Chmod(file, mode_i, error);
	if (!ret) {
		lua_Debug ar;
		lua_getstack(L, 1, &ar);
		lua_getinfo(L, "Sl", &ar);
		printf(">>> Lua script error [%s:%d] %s\n    (error from neutrino: [%s:%d])\n",
		       ar.short_src, ar.currentline, error.c_str(), __path_file__, __LINE__);
	}

	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersTouch(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 1;
	if (numargs < min_numargs) {
		printf("luascript touch: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushboolean(L, false);
		return 1;
	}
	if (!lua_isstring(L, 2)) {
		printf("%s: argument 1 is not a string.\n",__func__);
		lua_pushboolean(L, false);
		return 1;
	}
	const char *file = luaL_checkstring(L, 2);

	bool haveTime = (numargs > min_numargs);
	time_t newTime = haveTime ? (time_t)luaL_checkint(L, 3) : 0;

	ScriptFileHelpers::Error error;
	bool ret = ScriptFileHelpers::Touch(file, haveTime, newTime, error);
	if (!ret)
		fileHelpersError(L, error);

	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersRmdir(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 1;
	if (numargs < min_numargs) {
		printf("luascript rmdir: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushboolean(L, false);
		return 1;
	}
	if (!lua_isstring(L, 2)) {
		printf("%s: argument 1 is not a string.\n",__func__);
		lua_pushboolean(L, false);
		return 1;
	}
	const char *dir = luaL_checkstring(L, 2);

	ScriptFileHelpers::Error error;
	bool ret = ScriptFileHelpers::Rmdir(dir, error);
	if (!ret)
		fileHelpersError(L, error);

	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersMkdir(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 1;
	if (numargs < min_numargs) {
		printf("luascript mkdir: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushboolean(L, false);
		return 1;
	}
	if (!lua_isstring(L, 2)) {
		printf("%s: argument 1 is not a string.\n",__func__);
		lua_pushboolean(L, false);
		return 1;
	}
	const char *dir = luaL_checkstring(L, 2);

	bool haveMode = (numargs > min_numargs);
	int mode_i = haveMode ? luaL_checkint(L, 3) : 0;

	ScriptFileHelpers::Error error;
	bool ret = ScriptFileHelpers::Mkdir(dir, haveMode, mode_i, error);
	if (!ret)
		fileHelpersError(L, error);

	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersReadlink(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 1;
	if (numargs < min_numargs) {
		printf("luascript readlink: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushnil(L);
		return 1;
	}
	if (!lua_isstring(L, 2)) {
		printf("%s: argument 1 is not a string.\n",__func__);
		lua_pushboolean(L, false);
		return 1;
	}
	const char *link = luaL_checkstring(L, 2);

	std::string target;
	ScriptFileHelpers::Error error;
	if (!ScriptFileHelpers::Readlink(link, target, error)) {
		fileHelpersError(L, error);
		lua_pushnil(L);
		return 1;
	}

	lua_pushstring(L, target.c_str());
	return 1;
}

int CLuaInstFileHelpers::FileHelpersLn(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 2;
	if (numargs < min_numargs) {
		printf("luascript ln: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushboolean(L, false);
		return 1;
	}
	if (!lua_isstring(L, 2) || !lua_isstring(L, 3)) {
		printf("%s: argument 1 or 2 is not a string.\n",__func__);
		lua_pushboolean(L, false);
		return 1;
	}
	const char *src = luaL_checkstring(L, 2);
	const char *link = luaL_checkstring(L, 3);

	const char *flags = "";
	if (numargs > min_numargs){
		if (!lua_isstring(L, 4)) {
		printf("%s: argument 3 is not a string.\n",__func__);
			lua_pushboolean(L, false);
			return 1;
		}
		flags = luaL_checkstring(L, 4);
	}
	ScriptFileHelpers::Error error;
	bool ret = ScriptFileHelpers::Ln(src, link, flags, error);
	if (!ret)
		fileHelpersError(L, error);

	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersExist(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 2;
	if (numargs < min_numargs) {
		printf("luascript exist: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushnil(L);
		return 1;
	}

	if (!lua_isstring(L, 2) || !lua_isstring(L, 3)) {
		printf("%s: argument 1 or 2 is not a string.\n",__func__);
		lua_pushboolean(L, false);
		return 1;
	}
	const char *file = luaL_checkstring(L, 2);
	const char *flag = luaL_checkstring(L, 3);

	bool ret = false;
	ScriptFileHelpers::Error error;
	if (!ScriptFileHelpers::Exist(file, flag, ret, error)) {
		fileHelpersError(L, error);
		lua_pushnil(L);
		return 1;
	}

	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersIsEmpty(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 1;
	if (numargs < min_numargs) {
		printf("luascript isempty: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushnil(L);
		return 1;
	}

	if (!lua_isstring(L, 2)) {
		printf("%s: argument 1 is not a string.\n", __func__);
		lua_pushboolean(L, false);
		return 1;
	}

	const char *file = luaL_checkstring(L, 2);

	bool ret = false;
	ScriptFileHelpers::Error error;
	if (!ScriptFileHelpers::IsEmpty(file, ret, error)) {
		fileHelpersError(L, error);
		DisplayErrorMessage(error.msg.c_str(), "Lua Script Error:");
		lua_pushnil(L);
		return 1;
	}

	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersDelete(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}
