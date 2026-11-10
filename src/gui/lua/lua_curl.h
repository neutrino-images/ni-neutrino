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

#ifndef _LUACURL_H
#define _LUACURL_H

#include "../scriptapi/script_curl.h"

class CLuaCurl : public ScriptCurl
{
	public:
		CLuaCurl() {};
		~CLuaCurl() {};
};

class CLuaInstCurl
{
	public:
		/* the codes luainstance.cpp hands to the scripts, under the names they
		 * had before the download moved into the script API */
		enum {
			LUA_CURL_OK              = ScriptCurl::OK,
			LUA_CURL_ERR_HANDLE      = ScriptCurl::ERR_HANDLE,
			LUA_CURL_ERR_NO_URL      = ScriptCurl::ERR_NO_URL,
			LUA_CURL_ERR_CREATE_FILE = ScriptCurl::ERR_CREATE_FILE,
			LUA_CURL_ERR_CURL        = ScriptCurl::ERR_CURL
		};

		CLuaInstCurl() {};
		~CLuaInstCurl() {};
		static CLuaInstCurl* getInstance();
		static void LuaCurlRegister(lua_State *L);
		static std::string CurlUriInternal(std::string data, bool decode);

	private:
		static CLuaCurl *CurlCheckData(lua_State *L, int n);
		static int CurlNew(lua_State *L);
		static int CurlDownload(lua_State *L);
		static int CurlEncodeUri(lua_State *L);
		static int CurlDecodeUri(lua_State *L);
		static int CurlSetUriData(lua_State *L);
		static int CurlDelete(lua_State *L);
};

#endif //_LUACURL_H
