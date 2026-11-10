/*
 * python curl
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

PyObject *CPythonInstCurl::CurlNewModule(PyObject * /*self*/, PyObject *args, PyObject *kwds)
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

std::string CPythonInstCurl::CurlUriInternal(std::string data, bool decode)
{
	return ScriptCurl::UriInternal(data, decode);
}

/* c.encodeUri(data) */
PyObject *CPythonInstCurl::CurlEncodeUri(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "data", NULL };
	const char *data = "";

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &data))
		return NULL;

	std::string retString = ScriptCurl::UriInternal(data, false);
	if (retString.empty())
		Py_RETURN_NONE;
	return PyUnicode_FromStringAndSize(retString.c_str(), (Py_ssize_t)retString.length());
}

/* c.decodeUri(data) */
PyObject *CPythonInstCurl::CurlDecodeUri(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "data", NULL };
	const char *data = "";

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &data))
		return NULL;

	std::string retString = ScriptCurl::UriInternal(data, true);
	if (retString.empty())
		Py_RETURN_NONE;
	return PyUnicode_FromStringAndSize(retString.c_str(), (Py_ssize_t)retString.length());
}

/* c.setUriData(key, data, key2, data2, ...): the query string they make. */
PyObject *CPythonInstCurl::CurlSetUriData(PyObject * /*self*/, PyObject *args, PyObject * /*kwds*/)
{
	Py_ssize_t numargs = (args != NULL) ? PyTuple_GET_SIZE(args) : 0;
	if (numargs < 2 || (numargs % 2) != 0)
	{
		printf("CPythonInstCurl::%s: not enough arguments (%d, expected at least 2, in pairs)\n", __func__, (int)numargs);
		Py_RETURN_NONE;
	}

	std::vector<std::string> namesAndValues;
	for (Py_ssize_t i = 0; i < numargs; i += 2)
	{
		std::string key = "", data = "";
		if (!pyArgString(PyTuple_GET_ITEM(args, i), key) ||
		    !pyArgString(PyTuple_GET_ITEM(args, i + 1), data))
		{
			PyErr_SetString(PyExc_TypeError, "setUriData() takes names and values in pairs");
			return NULL;
		}
		namesAndValues.push_back(key);
		namesAndValues.push_back(data);
	}

	std::string retString = ScriptCurl::QueryString(namesAndValues);

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
 * or, with redirectOnly, the url it ended at.
 *
 * The keyword arguments are read here; what they hold goes to
 * scriptapi/script_curl.cpp, where the download the Lua binding makes stands
 * too. */
PyObject *CPythonInstCurl::CurlDownload(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		PyErr_SetString(PyExc_TypeError, "download() takes keyword arguments only");
		return NULL;
	}

	ScriptCurlParams params;
	pyLookupString(kwds, "url", params.url);

	std::string outputfile = "";
	if (pyLookupString(kwds, "o", outputfile) || pyLookupString(kwds, "outputfile", outputfile))
		params.outputfile = outputfile;

	pyLookupString(kwds, "A", params.userAgent) || pyLookupString(kwds, "userAgent", params.userAgent);

	pyLookupString(kwds, "P", params.postfields) || pyLookupString(kwds, "postfields", params.postfields) ||
	pyLookupString(kwds, "data", params.postfields) || pyLookupString(kwds, "post-data", params.postfields);

	pyLookupBool(kwds, "v", params.verbose) || pyLookupBool(kwds, "verbose", params.verbose);

	if (!params.verbose)
		pyLookupBool(kwds, "s", params.silent) || pyLookupBool(kwds, "silent", params.silent);

	pyLookupBool(kwds, "header", params.pass_header);

	/* httpheader: the list of header lines */
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
				params.httpheader.push_back(val);
			else
				PyErr_Clear();
			Py_DECREF(item);
		}
	}

	long connectTimeout = 20;
	pyLookupInt(kwds, "connectTimeout", connectTimeout);
	params.connectTimeout = connectTimeout;

	pyLookupBool(kwds, "ipv4", params.ipv4);
	pyLookupBool(kwds, "ipv6", params.ipv6);
	pyLookupBool(kwds, "useProxy", params.useProxy);
	pyLookupBool(kwds, "followRedir", params.followRedir);

	long maxRedirs = 20;
	pyLookupInt(kwds, "maxRedirs", maxRedirs);
	params.maxRedirs = maxRedirs;

	pyLookupBool(kwds, "redirectOnly", params.redirectOnly);

	std::string answer = "";
	int ret = ScriptCurl::Download(params, answer);

	if (ret != ScriptCurl::OK)
		return Py_BuildValue("(is)", ret, answer.c_str());

	/* The body is handed over as it came: a document is bytes, and one that
	 * is not text is not a string. The Lua binding answers the same bytes. */
	return Py_BuildValue("(iy#)", ret, answer.c_str(), (Py_ssize_t)answer.length());
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
