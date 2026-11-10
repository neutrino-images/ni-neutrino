/*
 * simple lua curl functions
 *
 * (C) 2015 M. Liebmann (micha-bbg)
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

#include <global.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_curl.h"

CLuaInstCurl* CLuaInstCurl::getInstance()
{
	static CLuaInstCurl* LuaInstCurl = NULL;

	if (!LuaInstCurl)
		LuaInstCurl = new CLuaInstCurl();
	return LuaInstCurl;
}

CLuaCurl *CLuaInstCurl::CurlCheckData(lua_State *L, int n)
{
	return *(CLuaCurl **) luaL_checkudata(L, n, LUA_CURL_CLASSNAME);
}

void CLuaInstCurl::LuaCurlRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",	CLuaInstCurl::CurlNew },
		{ "download",	CLuaInstCurl::CurlDownload },
		{ "encodeUri",	CLuaInstCurl::CurlEncodeUri },
		{ "decodeUri",	CLuaInstCurl::CurlDecodeUri },
		{ "setUriData",	CLuaInstCurl::CurlSetUriData },
		{ "__gc",	CLuaInstCurl::CurlDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, LUA_CURL_CLASSNAME);
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, LUA_CURL_CLASSNAME);
}

int CLuaInstCurl::CurlNew(lua_State *L)
{
	CLuaCurl **udata = (CLuaCurl **) lua_newuserdata(L, sizeof(CLuaCurl *));
	*udata = new CLuaCurl();
	luaL_getmetatable(L, LUA_CURL_CLASSNAME);
	lua_setmetatable(L, -2);
	return 1;
}

std::string CLuaInstCurl::CurlUriInternal(std::string data, bool decode)
{
	return ScriptCurl::UriInternal(data, decode);
}

/* lua_curl.h said it years ago:
 *
 *	parameter     type          default
 *	----------------------------------------
 *	url           string        required
 *	o, outputfile string        when empty the body comes back as the
 *	                            second answer
 *	A, userAgent  string        empty
 *	P, postfields string        empty
 *	data, post-data string      empty
 *	v, verbose    bool          false
 *	s, silent     bool          false
 *	h, header     bool          false
 *	connectTimeout number       20
 *	ipv4, ipv6    bool          false
 *	useProxy      bool          true
 *	followRedir   bool          true
 *	maxRedirs     number        20
 *	httpheader    table         empty
 *	redirectOnly  bool          false
 *
 * The table is read here; what it holds goes to scriptapi/script_curl.cpp,
 * where the download the Python binding makes stands too.
 *
 * Example:
 *	local curl = curl.new()
 *	local ret, data = curl:download{url="http://example.com", o="/tmp/test.txt"}
 *	if ret ~= CURL.OK then
 *		print("Error: " .. data)
 *	end
 */
int CLuaInstCurl::CurlDownload(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCurl *D = CurlCheckData(L, 1);
	if (!D) return 0;

	ScriptCurlParams params;
	tableLookup(L, "url", params.url);

	std::string outputfile = "";
	if (tableLookup(L, "o", outputfile) || tableLookup(L, "outputfile", outputfile))
		params.outputfile = outputfile;

	tableLookup(L, "A", params.userAgent) || tableLookup(L, "userAgent", params.userAgent);

	tableLookup(L, "P", params.postfields) || tableLookup(L, "postfields", params.postfields) ||
	tableLookup(L, "data", params.postfields) || tableLookup(L, "post-data", params.postfields);

	tableLookup(L, "v", params.verbose) || tableLookup(L, "verbose", params.verbose);

	if (!params.verbose)
		tableLookup(L, "s", params.silent) || tableLookup(L, "silent", params.silent);

	tableLookup(L, "header", params.pass_header);

	/* httpheader */
	lua_pushstring(L, "httpheader");
	lua_gettable(L, -2);
	if (lua_istable(L, -1)) {
		for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 2)) {
			lua_pushvalue(L, -2);
			const char *val = lua_tostring(L, -2);
			if (val) {
				params.httpheader.push_back(val);
			}
		}
	}
	lua_pop(L, 1);
	/* httpheader end */

	lua_Integer connectTimeout = 20;
	tableLookup(L, "connectTimeout", connectTimeout);
	params.connectTimeout = (long)connectTimeout;

	tableLookup(L, "ipv4", params.ipv4);
	tableLookup(L, "ipv6", params.ipv6);
	tableLookup(L, "useProxy", params.useProxy);
	tableLookup(L, "followRedir", params.followRedir);

	lua_Integer maxRedirs = 20;
	tableLookup(L, "maxRedirs", maxRedirs);
	params.maxRedirs = (long)maxRedirs;

	tableLookup(L, "redirectOnly", params.redirectOnly);

	std::string answer = "";
	int ret = ScriptCurl::Download(params, answer);

	lua_pushinteger(L, ret);
	lua_pushstring(L, answer.c_str());
	return 2;
}

int CLuaInstCurl::CurlEncodeUri(lua_State *L)
{
	CLuaCurl *D = CurlCheckData(L, 1);
	if (!D) {
		lua_pushnil(L);
		return 1;
	}
	const char *data = luaL_checkstring(L, 2);
	if (!data) {
		lua_pushnil(L);
		return 1;
	}

	std::string retString = ScriptCurl::UriInternal(static_cast<std::string>(data), false);

	if (retString.empty())
		lua_pushnil(L);
	else
		lua_pushstring(L, retString.c_str());

	return 1;
}

int CLuaInstCurl::CurlDecodeUri(lua_State *L)
{
	CLuaCurl *D = CurlCheckData(L, 1);
	if (!D) {
		lua_pushnil(L);
		return 1;
	}
	const char *data = luaL_checkstring(L, 2);
	if (!data) {
		lua_pushnil(L);
		return 1;
	}

	std::string retString = ScriptCurl::UriInternal(static_cast<std::string>(data), true);

	if (retString.empty())
		lua_pushnil(L);
	else
		lua_pushstring(L, retString.c_str());

	return 1;
}

int CLuaInstCurl::CurlSetUriData(lua_State *L)
{
	CLuaCurl *D = CurlCheckData(L, 1);
	if (!D) {
		lua_pushnil(L);
		return 1;
	}
	int numargs = lua_gettop(L);
	if (numargs < 3) {
		printf("CLuaInstCurl::%s: not enough arguments (%d, expected at least 2)\n", __func__, numargs-1);
		lua_pushnil(L);
		return 1;
	}

	std::vector<std::string> namesAndValues;
	for (int i = 2; i < numargs; i += 2) {
		const char *key  = luaL_checkstring(L, i);
		const char *data = luaL_checkstring(L, i+1);
		namesAndValues.push_back(key  ? key  : "");
		namesAndValues.push_back(data ? data : "");
	}

	std::string retString = ScriptCurl::QueryString(namesAndValues);

	if (retString.empty())
		lua_pushnil(L);
	else
		lua_pushstring(L, retString.c_str());

	return 1;
}

int CLuaInstCurl::CurlDelete(lua_State *L)
{
	CLuaCurl *D = CurlCheckData(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}
