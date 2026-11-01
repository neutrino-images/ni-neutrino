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
#include <system/helpers.h>
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

/* c.download(url=..., o=..., A=..., P=..., v=..., s=..., header=...,
 *	httpheader=[...], connectTimeout=20, ipv4=..., ipv6=...,
 *	useProxy=True, followRedir=True, maxRedirs=20, redirectOnly=False)
 *
 * Answers the status and what came back, as the Lua binding answers two
 * values: the text of the download, or the message that says what failed,
 * or, with redirectOnly, the url it ended at. */
PyObject *CPythonInstCurl::CurlDownload(PyObject *self, PyObject *args, PyObject *kwds)
{
#define CURL_MSG_ERROR "[curl:download \33[1;31mERROR!\33[0m]"

	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		PyErr_SetString(PyExc_TypeError, "download() takes keyword arguments only");
		return NULL;
	}

	char errMsg[1024] = {0};
	CURL *curl_handle = curl_easy_init();
	if (!curl_handle)
	{
		snprintf(errMsg, sizeof(errMsg) - 1, "error creating cUrl handle.");
		printf("%s %s\n", CURL_MSG_ERROR, errMsg);
		return Py_BuildValue("(is)", PY_CURL_ERR_HANDLE, errMsg);
	}

	std::string url = "";
	pyLookupString(kwds, "url", url);
	if (url.empty())
	{
		curl_easy_cleanup(curl_handle);
		snprintf(errMsg, sizeof(errMsg) - 1, "no url given.");
		printf("%s %s\n", CURL_MSG_ERROR, errMsg);
		return Py_BuildValue("(is)", PY_CURL_ERR_NO_URL, errMsg);
	}

	std::string outputfile = "";
	bool toFile = pyLookupString(kwds, "o", outputfile) || pyLookupString(kwds, "outputfile", outputfile);
	std::string retString = "";
	FILE *fp = NULL;
	if (toFile)
	{
		fp = fopen(outputfile.c_str(), "wb");
		if (fp == NULL)
		{
			snprintf(errMsg, sizeof(errMsg) - 1, "Can't create %s", outputfile.c_str());
			printf("%s %s\n", CURL_MSG_ERROR, errMsg);
			curl_easy_cleanup(curl_handle);
			return Py_BuildValue("(is)", PY_CURL_ERR_CREATE_FILE, errMsg);
		}
	}

	std::string userAgent = "";
	pyLookupString(kwds, "A", userAgent) || pyLookupString(kwds, "userAgent", userAgent);

	std::string postfields = "";
	pyLookupString(kwds, "P", postfields) || pyLookupString(kwds, "postfields", postfields) ||
	pyLookupString(kwds, "data", postfields) || pyLookupString(kwds, "post-data", postfields);

	bool verbose = false;
	pyLookupBool(kwds, "v", verbose) || pyLookupBool(kwds, "verbose", verbose);

	bool silent = false;
	if (!verbose)
		pyLookupBool(kwds, "s", silent) || pyLookupBool(kwds, "silent", silent);

	bool pass_header = false;
	pyLookupBool(kwds, "header", pass_header);

	/* httpheader: the list of header lines */
	curl_slist *hlist = NULL;
	PyObject *headers = NULL;
	if (pyLookupList(kwds, "httpheader", &headers))
	{
		Py_ssize_t count = PySequence_Size(headers);
		for (Py_ssize_t i = 0; i < count; i++)
		{
			PyObject *item = PySequence_GetItem(headers, i);
			if (item == NULL)
			{
				PyErr_Clear();
				continue;
			}
			const char *val = PyUnicode_Check(item) ? PyUnicode_AsUTF8(item) : NULL;
			if (val != NULL)
				hlist = curl_slist_append(hlist, val);
			else
				PyErr_Clear();
			Py_DECREF(item);
		}
	}

	long connectTimeout = 20;
	pyLookupInt(kwds, "connectTimeout", connectTimeout);

	bool ipv4 = false, ipv6 = false;
	pyLookupBool(kwds, "ipv4", ipv4);
	pyLookupBool(kwds, "ipv6", ipv6);

	bool useProxy = true;
	pyLookupBool(kwds, "useProxy", useProxy);

	bool followRedir = true;
	pyLookupBool(kwds, "followRedir", followRedir);

	long maxRedirs = 20;
	pyLookupInt(kwds, "maxRedirs", maxRedirs);

	bool redirectOnly = false;
	pyLookupBool(kwds, "redirectOnly", redirectOnly);

	curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
	if (toFile)
	{
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, NULL);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, fp);
	}
	else
	{
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, &CPythonInstCurl::CurlWriteToString);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&retString);
	}

	curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, (long)(4 * connectTimeout));
	curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, (long)connectTimeout);
	curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
	/* enable all supported built-in compressions */
	curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING, "");
	curl_easy_setopt(curl_handle, CURLOPT_COOKIE, "");
	if (!userAgent.empty())
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, userAgent.c_str());

	if (!postfields.empty())
	{
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(postfields.length()));
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, postfields.c_str());
	}

	if (ipv4 && ipv6)
		curl_easy_setopt(curl_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
	else if (ipv4)
		curl_easy_setopt(curl_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	else if (ipv6)
		curl_easy_setopt(curl_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);

	/* Copied under the lock: a web channel's script runs this on the stream
	   server's thread as well, while the box's loop may assign to these. */
	const std::string proxy_server = settingsText(g_settings.softupdate_proxyserver);
	if (!proxy_server.empty() && useProxy)
	{
		curl_easy_setopt(curl_handle, CURLOPT_PROXY, proxy_server.c_str());
		const std::string proxy_user = settingsText(g_settings.softupdate_proxyusername);
		if (!proxy_user.empty())
		{
			std::string tmp = proxy_user + ":" + settingsText(g_settings.softupdate_proxypassword);
			curl_easy_setopt(curl_handle, CURLOPT_PROXYUSERPWD, tmp.c_str());
		}
	}

	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, (followRedir) ? 1L : 0L);
	curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, (long)maxRedirs);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, (silent) ? 1L : 0L);
	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, (verbose) ? 1L : 0L);
	curl_easy_setopt(curl_handle, CURLOPT_HEADER, (pass_header) ? 1L : 0L);
	if (hlist)
		curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, hlist);

	/* the bar, unless the script asked for quiet */
	if (!silent)
	{
		progressData pgd;
		pgd.curl = curl_handle;
		pgd.last_dlnow = 0;
#if CURL_AT_LEAST_VERSION( 7,32,0 )
		curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, CPythonInstCurl::CurlProgressFunc);
		curl_easy_setopt(curl_handle, CURLOPT_XFERINFODATA, &pgd);
#else
		curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, CPythonInstCurl::CurlProgressFuncOld);
		curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, &pgd);
#endif
	}

	char cerror[CURL_ERROR_SIZE] = {0};
	curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, cerror);

	printf("\n[curl:download] download %s => %s\n", url.c_str(), (toFile) ? outputfile.c_str() : "return string");
	if (!silent)
		printf("\e[?25l"); /* cursor off */
	printf("\n");
	CURLcode ret = curl_easy_perform(curl_handle);
	if (!silent)
		printf("\e[?25h"); /* cursor on */
	printf("\n");

	std::string redirectUrl;
	std::string msg;
	if (!silent)
	{
		double dsize = 0, dtime = 0;
		char *dredirect = NULL, *deffektive = NULL;
#if CURL_AT_LEAST_VERSION( 7,55,0 )
		curl_easy_getinfo(curl_handle, CURLINFO_SIZE_DOWNLOAD_T, &dsize);
#else
		curl_easy_getinfo(curl_handle, CURLINFO_SIZE_DOWNLOAD, &dsize);
#endif
		curl_easy_getinfo(curl_handle, CURLINFO_TOTAL_TIME, &dtime);
		CURLcode res1 = curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &deffektive);
		CURLcode res2 = curl_easy_getinfo(curl_handle, CURLINFO_REDIRECT_URL, &dredirect);

		char msg1[1024] = {0};
		snprintf(msg1, sizeof(msg1) - 1, "\n[curl:download] O.K. size: %.0f bytes, time: %.02f sec.", dsize, dtime);
		msg = msg1;
		if (toFile)
			msg += std::string("\n\t\t     file: ") + outputfile;
		else
			msg += std::string("\n\t\t   output: return string");
		msg += std::string("\n\t\t      url: ") + url;
		if ((res1 == CURLE_OK) && deffektive)
		{
			std::string tmp1 = std::string(deffektive);
			std::string tmp2 = url;
			if (trim(tmp1, " /") != trim(tmp2, " /"))
				msg += std::string("\n\t    effektive url: ") + deffektive;

			if (redirectOnly)
				redirectUrl = deffektive;
		}
		if ((res2 == CURLE_OK) && dredirect)
			msg += std::string("\n\t      redirect to: ") + dredirect;
	}
	if (hlist)
		curl_slist_free_all(hlist);
	curl_easy_cleanup(curl_handle);
	if (toFile)
		fclose(fp);

	if (ret != 0)
	{
		snprintf(errMsg, sizeof(errMsg) - 1, "%s", cerror);
		printf("%s curl error: %s\n", CURL_MSG_ERROR, errMsg);
		if (toFile)
			unlink(outputfile.c_str());
		return Py_BuildValue("(is)", PY_CURL_ERR_CURL, errMsg);
	}

	if (redirectOnly && !redirectUrl.empty())
		return Py_BuildValue("(is)", PY_CURL_OK, redirectUrl.c_str());

	if (verbose == true)
		printf("%s\n \n", msg.c_str());

	return Py_BuildValue("(is)", PY_CURL_OK, retString.c_str());
#undef CURL_MSG_ERROR
}

bool CPythonInstCurl::CurlRegister(PyObject *module)
{
	static PyMethodDef curl_methods[] =
	{
		PY_METHODDEF("download",   CPythonInstCurl::CurlDownload),
		PY_METHODDEF("encodeUri",  CPythonInstCurl::CurlEncodeUri),
		PY_METHODDEF("decodeUri",  CPythonInstCurl::CurlDecodeUri),
		PY_METHODDEF("setUriData", CPythonInstCurl::CurlSetUriData),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("new", CPythonInstCurl::CurlNewModule),
		{ NULL, NULL, 0, NULL }
	};

	static PyType_Slot curl_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstCurl::CurlNew },
		{ Py_tp_dealloc, (void *)CPythonInstCurl::CurlDelete },
		{ Py_tp_methods, (void *)curl_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"curl" },
		{ 0, NULL }
	};

	static PyType_Spec curl_spec =
	{
		"neutrino.curl.curl",
		sizeof(PyCurlObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		curl_slots
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.curl",
		"curl",
		-1,
		module_methods,
		NULL, NULL, NULL, NULL
	};

	PyObject *sub = PyModule_Create(&module_def);
	if (sub == NULL)
	{
		PyErr_Clear();
		return false;
	}

	curl_type = (PyTypeObject *)PyType_FromSpec(&curl_spec);
	if (curl_type == NULL)
	{
		PyErr_Clear();
		Py_DECREF(sub);
		return false;
	}
	if (PyModule_AddObject(sub, "curl", (PyObject *)curl_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)curl_type);
		curl_type = NULL;
		Py_DECREF(sub);
		return false;
	}

	return PythonInstAddSubmodule(module, "curl", sub);
}



