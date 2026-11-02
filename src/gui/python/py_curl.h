/*
 * python curl
 *
 * The Lua binding in lua_curl.cpp, read as
 *
 *	from neutrino import curl, CURL
 *	c = curl.new()
 *	ret, data = c.download(url="http://example.com")
 *	query = c.setUriData("name", "a value", "other", "another")
 *	text = c.encodeUri(data="a b/c")
 *
 * setUriData takes names and values in pairs and answers the query string
 * they make, with the values escaped; the Lua binding's does the same.
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

#ifndef _PYCURL_H
#define _PYCURL_H

#include "pyinstance_helpers.h"

#include <curl/curl.h>

class CPyCurl
{
	public:
		CPyCurl() {};
		~CPyCurl() {};
};

struct progressData {
	CURL *curl;
	curl_off_t last_dlnow;
};

class CPythonInstCurl
{
	public:
		CPythonInstCurl() {};
		~CPythonInstCurl() {};
		static CPythonInstCurl *getInstance();
		static bool CurlRegister(PyObject *module);
		static std::string CurlUriInternal(std::string data, bool decode);

	private:
		static PyObject *CurlNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static PyObject *CurlNewModule(PyObject *self, PyObject *args, PyObject *kwds);
		static void CurlDelete(PyObject *self);

		static size_t CurlWriteToString(void *ptr, size_t size, size_t nmemb, void *data);
#if !CURL_AT_LEAST_VERSION( 7,32,0 )
		static int CurlProgressFuncOld(void *p, double dltotal, double dlnow, double ultotal, double ulnow);
#endif
		static int CurlProgressFunc(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

		static PyObject *CurlDownload(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CurlEncodeUri(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CurlDecodeUri(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CurlSetUriData(PyObject *self, PyObject *args, PyObject *kwds);
};

#endif //_PYCURL_H
