/*
 * python config file
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

#include "py_configfile.h"

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <neutrino.h>

#include "pyinstance.h"

typedef struct
{
	PyObject_HEAD
	CPyConfigFile *d;
} PyConfigFileObject;

static PyTypeObject *configfile_type = NULL;

CPythonInstConfigFile *CPythonInstConfigFile::getInstance()
{
	static CPythonInstConfigFile *PythonInstConfigFile = NULL;

	if (!PythonInstConfigFile)
		PythonInstConfigFile = new CPythonInstConfigFile();
	return PythonInstConfigFile;
}

static CConfigFile *configFileOf(PyObject *self)
{
	PyConfigFileObject *o = (PyConfigFileObject *)self;
	return (o->d != NULL) ? o->d->c : NULL;
}

/* configfile.new(delimiter="\t", saveDefaults=True) */
PyObject *CPythonInstConfigFile::ConfigFileNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "delimiter", "saveDefaults", NULL };
	const char *delimiter = "\t";
	int saveDefaults = 1;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|sp", (char **)kwlist, &delimiter, &saveDefaults))
		return NULL;

	PyConfigFileObject *self = (PyConfigFileObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		return NULL;
	}

	self->d = new CPyConfigFile();
	self->d->c = new CConfigFile(delimiter[0], saveDefaults ? true : false);
	return (PyObject *)self;
}

PyObject *CPythonInstConfigFile::ConfigFileNewModule(PyObject *self, PyObject *args, PyObject *kwds)
{
	if (configfile_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "configfile is not registered");
		return NULL;
	}
	return ConfigFileNew(configfile_type, args, kwds);
}

void CPythonInstConfigFile::ConfigFileDelete(PyObject *self)
{
	PyConfigFileObject *o = (PyConfigFileObject *)self;
	delete o->d;
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

PyObject *CPythonInstConfigFile::LoadConfig(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "file", NULL };
	const char *fname = NULL;
	CConfigFile *c = configFileOf(self);

	if (c == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "configfile is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &fname))
		return NULL;

	return PyBool_FromLong(c->loadConfig(fname) ? 1 : 0);
}

PyObject *CPythonInstConfigFile::SaveConfig(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "file", NULL };
	const char *fname = NULL;
	CConfigFile *c = configFileOf(self);

	if (c == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "configfile is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &fname))
		return NULL;

	return PyBool_FromLong(c->saveConfig(fname) ? 1 : 0);
}

PyObject *CPythonInstConfigFile::Clear(PyObject *self, PyObject *args, PyObject *kwds)
{
	CConfigFile *c = configFileOf(self);
	if (c == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "configfile is gone");
		return NULL;
	}

	c->clear();
	Py_RETURN_NONE;
}

PyObject *CPythonInstConfigFile::GetString(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "key", "default", NULL };
	const char *key = NULL;
	const char *defaultVal = "";
	CConfigFile *c = configFileOf(self);

	if (c == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "configfile is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|s", (char **)kwlist, &key, &defaultVal))
		return NULL;

	std::string ret = c->getString(key, defaultVal);
	return PyUnicode_FromStringAndSize(ret.c_str(), (Py_ssize_t)ret.length());
}

PyObject *CPythonInstConfigFile::SetString(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "key", "value", NULL };
	const char *key = NULL;
	const char *val = NULL;
	CConfigFile *c = configFileOf(self);

	if (c == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "configfile is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ss", (char **)kwlist, &key, &val))
		return NULL;

	c->setString(key, val);
	Py_RETURN_NONE;
}

PyObject *CPythonInstConfigFile::GetInt32(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "key", "default", NULL };
	const char *key = NULL;
	int defaultVal = 0;
	CConfigFile *c = configFileOf(self);

	if (c == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "configfile is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|i", (char **)kwlist, &key, &defaultVal))
		return NULL;

	return PyLong_FromLong(c->getInt32(key, defaultVal));
}

PyObject *CPythonInstConfigFile::SetInt32(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "key", "value", NULL };
	const char *key = NULL;
	int val = 0;
	CConfigFile *c = configFileOf(self);

	if (c == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "configfile is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "si", (char **)kwlist, &key, &val))
		return NULL;

	c->setInt32(key, val);
	Py_RETURN_NONE;
}

PyObject *CPythonInstConfigFile::GetBool(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "key", "default", NULL };
	const char *key = NULL;
	int defaultVal = 0;
	CConfigFile *c = configFileOf(self);

	if (c == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "configfile is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|p", (char **)kwlist, &key, &defaultVal))
		return NULL;

	return PyBool_FromLong(c->getBool(key, defaultVal ? true : false) ? 1 : 0);
}

PyObject *CPythonInstConfigFile::SetBool(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "key", "value", NULL };
	const char *key = NULL;
	int val = 0;
	CConfigFile *c = configFileOf(self);

	if (c == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "configfile is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "sp", (char **)kwlist, &key, &val))
		return NULL;

	c->setBool(key, val ? true : false);
	Py_RETURN_NONE;
}

PyObject *CPythonInstConfigFile::DeleteKey(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "key", NULL };
	const char *key = NULL;
	CConfigFile *c = configFileOf(self);

	if (c == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "configfile is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &key))
		return NULL;

	std::string k(key);
	c->deleteKey(k);
	Py_RETURN_NONE;
}


bool CPythonInstConfigFile::ConfigFileRegister(PyObject *module)
{
	static PyMethodDef configfile_methods[] =
	{
		PY_METHODDEF("loadConfig", CPythonInstConfigFile::LoadConfig),
		PY_METHODDEF("saveConfig", CPythonInstConfigFile::SaveConfig),
		PY_METHODDEF("clear",      CPythonInstConfigFile::Clear),
		PY_METHODDEF("getString",  CPythonInstConfigFile::GetString),
		PY_METHODDEF("setString",  CPythonInstConfigFile::SetString),
		PY_METHODDEF("getInt32",   CPythonInstConfigFile::GetInt32),
		PY_METHODDEF("setInt32",   CPythonInstConfigFile::SetInt32),
		PY_METHODDEF("getBool",    CPythonInstConfigFile::GetBool),
		PY_METHODDEF("setBool",    CPythonInstConfigFile::SetBool),
		PY_METHODDEF("deleteKey",  CPythonInstConfigFile::DeleteKey),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("new", CPythonInstConfigFile::ConfigFileNewModule),
		{ NULL, NULL, 0, NULL }
	};

	static PyType_Slot configfile_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstConfigFile::ConfigFileNew },
		{ Py_tp_dealloc, (void *)CPythonInstConfigFile::ConfigFileDelete },
		{ Py_tp_methods, (void *)configfile_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"config file" },
		{ 0, NULL }
	};

	static PyType_Spec configfile_spec =
	{
		"neutrino.configfile.configfile",
		sizeof(PyConfigFileObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		configfile_slots
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.configfile",
		"config file",
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

	configfile_type = (PyTypeObject *)PyType_FromSpec(&configfile_spec);
	if (configfile_type == NULL)
	{
		PyErr_Clear();
		Py_DECREF(sub);
		return false;
	}
	if (PyModule_AddObject(sub, "configfile", (PyObject *)configfile_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)configfile_type);
		configfile_type = NULL;
		Py_DECREF(sub);
		return false;
	}

	return PythonInstAddSubmodule(module, "configfile", sub);
}

