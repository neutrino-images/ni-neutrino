/*
 * python hintbox
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

#include "py_hintbox.h"

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
	CPyHintbox *d;
} PyHintboxObject;

static PyTypeObject *hintbox_type = NULL;

CPythonInstHintbox *CPythonInstHintbox::getInstance()
{
	static CPythonInstHintbox *PythonInstHintbox = NULL;

	if (!PythonInstHintbox)
		PythonInstHintbox = new CPythonInstHintbox();
	return PythonInstHintbox;
}

static CPyHintbox *hintboxOf(PyObject *self)
{
	PyHintboxObject *o = (PyHintboxObject *)self;
	return o->d;
}

/* hintbox.new(name="Title", text="text", icon="info", width=HINTBOX_MIN_WIDTH)
 *
 * The name is also read under the names the Lua binding takes it under,
 * title and caption, so a script carried over needs nothing changed here. */
PyObject *CPythonInstHintbox::HintboxNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		PyErr_SetString(PyExc_TypeError, "hintbox.new() takes keyword arguments only");
		return NULL;
	}

	/* The parameters a script names in its call. What is done with them is
	 * done by the script API, so that both bindings ask for one box and
	 * cannot come to differ. */
	ScriptHintbox::params_t pars;
	pars.icon = std::string(NEUTRINO_ICON_INFO);
	pars.width = HINTBOX_MIN_WIDTH;

	pyLookupString(kwds, "name", pars.name) || pyLookupString(kwds, "title", pars.name) || pyLookupString(kwds, "caption", pars.name);
	pyLookupString(kwds, "text", pars.text);
	pyLookupString(kwds, "icon", pars.icon);

	long width = (long)pars.width;
	pyLookupInt(kwds, "width", width);
	pars.width = (int)width;

	PyHintboxObject *self = (PyHintboxObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		return NULL;
	}

	self->d = new CPyHintbox(pars);
	return (PyObject *)self;
}

PyObject *CPythonInstHintbox::HintboxNewModule(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
	if (hintbox_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "hintbox is not registered");
		return NULL;
	}
	return HintboxNew(hintbox_type, args, kwds);
}

void CPythonInstHintbox::HintboxDelete(PyObject *self)
{
	PyHintboxObject *o = (PyHintboxObject *)self;
	delete o->d;
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

PyObject *CPythonInstHintbox::HintboxPaint(PyObject *self, PyObject * /*args*/, PyObject * /*kwds*/)
{
	CPyHintbox *D = hintboxOf(self);
	if (D != NULL)
		D->paint();
	Py_RETURN_NONE;
}

PyObject *CPythonInstHintbox::HintboxHide(PyObject *self, PyObject * /*args*/, PyObject * /*kwds*/)
{
	CPyHintbox *D = hintboxOf(self);
	if (D != NULL)
		D->hide();
	Py_RETURN_NONE;
}

/* b.exec(timeout=5): shows the box and waits for a key, with the loop the box
 * runs when a menu of the program shows it, which is the script API's. */
PyObject *CPythonInstHintbox::HintboxExec(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "timeout", NULL };
	int timeout = -1;
	CPyHintbox *D = hintboxOf(self);

	if (D == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "hintbox is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|i", (char **)kwlist, &timeout))
		return NULL;

	CPythonInstance::scriptUiOpen();
	D->exec(timeout);
	CPythonInstance::scriptUiClose();
	Py_RETURN_NONE;
}


bool CPythonInstHintbox::HintboxRegister(PyObject *module)
{
	static PyMethodDef hintbox_methods[] =
	{
		PY_METHODDEF("exec",  CPythonInstHintbox::HintboxExec),
		PY_METHODDEF("paint", CPythonInstHintbox::HintboxPaint),
		PY_METHODDEF("hide",  CPythonInstHintbox::HintboxHide),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("new", CPythonInstHintbox::HintboxNewModule),
		{ NULL, NULL, 0, NULL }
	};

	static PyType_Slot hintbox_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstHintbox::HintboxNew },
		{ Py_tp_dealloc, (void *)CPythonInstHintbox::HintboxDelete },
		{ Py_tp_methods, (void *)hintbox_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"hintbox" },
		{ 0, NULL }
	};

	static PyType_Spec hintbox_spec =
	{
		"neutrino.hintbox.hintbox",
		sizeof(PyHintboxObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		hintbox_slots
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.hintbox",
		"hintbox",
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

	hintbox_type = (PyTypeObject *)PyType_FromSpec(&hintbox_spec);
	if (hintbox_type == NULL)
	{
		PyErr_Clear();
		Py_DECREF(sub);
		return false;
	}
	if (PyModule_AddObject(sub, "hintbox", (PyObject *)hintbox_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)hintbox_type);
		hintbox_type = NULL;
		Py_DECREF(sub);
		return false;
	}

	return PythonInstAddSubmodule(module, "hintbox", sub);
}

