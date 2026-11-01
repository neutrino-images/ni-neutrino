/*
 * python curl
 *
 * The Lua binding in lua_curl.cpp: new/download/encodeUri/decodeUri/setUriData.
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

#include "py_curl.h"

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <system/settings.h>
#include <neutrino.h>

#include "pyinstance.h"

typedef struct
{
	PyObject_HEAD
	CPyCurl *d;
} PyCurlObject;

static PyTypeObject *curl_type = NULL;

CPythonInstCurl *CPythonInstCurl::getInstance()
{
	static CPythonInstCurl *PythonInstCurl = NULL;

	if (!PythonInstCurl)
		PythonInstCurl = new CPythonInstCurl();
	return PythonInstCurl;
}

PyObject *CPythonInstCurl::CurlNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	if ((args != NULL && PyTuple_GET_SIZE(args) > 0) || (kwds != NULL && PyDict_Size(kwds) > 0))
	{
		PyErr_SetString(PyExc_TypeError, "curl.new() takes no arguments");
		return NULL;
	}

	PyCurlObject *self = (PyCurlObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		return NULL;
	}
	self->d = new CPyCurl();
	return (PyObject *)self;
}

PyObject *CPythonInstCurl::CurlNewModule(PyObject *self, PyObject *args, PyObject *kwds)
{
	if (curl_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "curl is not registered");
		return NULL;
	}
	return CurlNew(curl_type, args, kwds);
}

void CPythonInstCurl::CurlDelete(PyObject *self)
{
	PyCurlObject *o = (PyCurlObject *)self;
	delete o->d;
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

size_t CPythonInstCurl::CurlWriteToString(void *ptr, size_t size, size_t nmemb, void *data)
{
	if (size * nmemb > 0)
	{
		std::string *pStr = (std::string *)data;
		pStr->append((char *)ptr, nmemb);
	}
	return size * nmemb;
}

/* The bar the Lua binding draws while it downloads, with the same shape. */
int CPythonInstCurl::CurlProgressFunc(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t)
{
	if (dltotal == 0)
		return 0;

	struct progressData *_pgd = static_cast<struct progressData *>(p);
	if (_pgd->last_dlnow == dlnow)
		return 0;
	_pgd->last_dlnow = dlnow;

	double dlSpeed = 0;
	long responseCode = 0;
#if CURL_AT_LEAST_VERSION( 7,55,0 )
	curl_easy_getinfo(_pgd->curl, CURLINFO_SPEED_DOWNLOAD_T, &dlSpeed);
#else
	curl_easy_getinfo(_pgd->curl, CURLINFO_SPEED_DOWNLOAD, &dlSpeed);
#endif
	curl_easy_getinfo(_pgd->curl, CURLINFO_RESPONSE_CODE, &responseCode);

	uint32_t MUL = 0x7FFF;
	uint32_t dlFragment = (uint32_t)((dlnow * MUL) / dltotal);
	if (responseCode != 200)
	{
		dlFragment = 0;
		dlSpeed = 0;
	}

	int showDots = 50;
	int dots = (dlFragment * showDots) / MUL;
	printf(" %d%% [", (dlFragment * 100) / MUL);
	int i = 0;
	for (; i < dots - 1; i++)
		printf("=");
	if (i < showDots)
	{
		printf(">");
		i++;
	}
	for (; i < showDots; i++)
		printf(" ");
	printf("] speed: %.03f KB/sec     \r", dlSpeed / 1024);
	fflush(stdout);
	return 0;
}

#if !CURL_AT_LEAST_VERSION( 7,32,0 )
int CPythonInstCurl::CurlProgressFuncOld(void *p, double dltotal, double dlnow, double ultotal, double ulnow)
{
	return CurlProgressFunc(p, (curl_off_t)dltotal, (curl_off_t)dlnow, (curl_off_t)ultotal, (curl_off_t)ulnow);
}
#endif

std::string CPythonInstCurl::CurlUriInternal(std::string data, bool decode)
{
	std::string retString = "";
	CURL *curl_handle = curl_easy_init();
	if (curl_handle)
	{
		int outlength;
		char *pfTmp;
		if (decode)
			pfTmp = curl_easy_unescape(curl_handle, data.c_str(), data.length(), &outlength);
		else
			pfTmp = curl_easy_escape(curl_handle, data.c_str(), data.length());
		if (pfTmp)
		{
			retString = static_cast<std::string>(pfTmp);
			curl_free(pfTmp);
		}
		curl_easy_cleanup(curl_handle);
	}
	return retString;
}

/* c.encodeUri(data) */
PyObject *CPythonInstCurl::CurlEncodeUri(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "data", NULL };
	std::string data = "";

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &data))
		return NULL;

	std::string retString = CurlUriInternal(data, false);
	if (retString.empty())
		Py_RETURN_NONE;
	return PyUnicode_FromStringAndSize(retString.c_str(), (Py_ssize_t)retString.length());
}

/* c.decodeUri(data) */
PyObject *CPythonInstCurl::CurlDecodeUri(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "data", NULL };
	std::string data = "";

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &data))
		return NULL;

	std::string retString = CurlUriInternal(data, true);
	if (retString.empty())
		Py_RETURN_NONE;
	return PyUnicode_FromStringAndSize(retString.c_str(), (Py_ssize_t)retString.length());
}

/* c.setUriData(key, data, key2, data2, ...): the query string they make. */
PyObject *CPythonInstCurl::CurlSetUriData(PyObject *self, PyObject *args, PyObject *kwds)
{
	Py_ssize_t numargs = (args != NULL) ? PyTuple_GET_SIZE(args) : 0;
	if (numargs < 2 || (numargs % 2) != 0)
	{
		printf("CPythonInstCurl::%s: not enough arguments (%d, expected at least 2, in pairs)\n", __func__, (int)numargs);
		Py_RETURN_NONE;
	}

	std::string retString = "";
	for (Py_ssize_t i = 0; i < numargs; i += 2)
	{
		std::string key = "", data = "";
		if (!pyArgString(PyTuple_GET_ITEM(args, i), key) ||
		    !pyArgString(PyTuple_GET_ITEM(args, i + 1), data))
		{
			PyErr_SetString(PyExc_TypeError, "setUriData() takes names and values in pairs");
			return NULL;
		}
		if (key.empty())
			continue;
		if (i > 0)
			retString += "&";
		retString += key + "=";
		retString += CurlUriInternal(data, false);
	}

	if (retString.empty())
		Py_RETURN_NONE;
	return PyUnicode_FromStringAndSize(retString.c_str(), (Py_ssize_t)retString.length());
}

