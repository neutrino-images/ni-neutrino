/*
 * python simple hint window
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

#include "py_hint.h"

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
	CPyHint *d;
} PyHintObject;

static PyTypeObject *hint_type = NULL;

CPythonInstHint *CPythonInstHint::getInstance()
{
	static CPythonInstHint *PythonInstHint = NULL;

	if (!PythonInstHint)
		PythonInstHint = new CPythonInstHint();
	return PythonInstHint;
}

static CHint *hintOf(PyObject *self)
{
	PyHintObject *o = (PyHintObject *)self;
	return (o->d != NULL) ? o->d->h : NULL;
}

/* hint.new(text="text", show_background=True) */
/* hint.new(text="text", show_background=True) */
PyObject *CPythonInstHint::HintNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	std::string text = "";
	bool show_background = true;

	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		/* the Lua binding reads the text out of its table; a script that
		 * passes it as the one argument is taken as meaning it */
		PyObject *first = PyTuple_GET_ITEM(args, 0);
		if (PyUnicode_Check(first))
		{
			const char *s = PyUnicode_AsUTF8(first);
			if (s != NULL)
				text = s;
			else
				PyErr_Clear();
		}
		if (PyTuple_GET_SIZE(args) > 1)
		{
			int flag = PyObject_IsTrue(PyTuple_GET_ITEM(args, 1));
			if (flag < 0)
				PyErr_Clear();
			else
				show_background = (flag != 0);
		}
	}

	pyLookupString(kwds, "text", text);
	pyLookupBoolCompat(kwds, "show_background", show_background);

	/* the hint itself is the script api's; this binding reads its two
	 * parameters and hands them over */
	ScriptHint::params_t pars;
	pars.text            = text;
	pars.show_background = show_background;

	PyHintObject *self = (PyHintObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		return NULL;
	}

	self->d = new CPyHint(pars);
	return (PyObject *)self;
}

PyObject *CPythonInstHint::HintNewModule(PyObject *self, PyObject *args, PyObject *kwds)
{
	if (hint_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "hint is not registered");
		return NULL;
	}
	return HintNew(hint_type, args, kwds);
}

void CPythonInstHint::HintDelete(PyObject *self)
{
	PyHintObject *o = (PyHintObject *)self;
	delete o->d;
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

PyObject *CPythonInstHint::HintPaint(PyObject *self, PyObject *args, PyObject *kwds)
{
	CHint *h = hintOf(self);
	if (h == NULL)
		Py_RETURN_NONE;

	bool do_save_bg = true;
	pyLookupBoolCompat(kwds, "do_save_bg", do_save_bg);

	h->paint(do_save_bg);
	Py_RETURN_NONE;
}

PyObject *CPythonInstHint::HintHide(PyObject *self, PyObject *args, PyObject *kwds)
{
	CHint *h = hintOf(self);
	if (h == NULL)
		Py_RETURN_NONE;

	bool tmp1 = false;
	std::string tmp2 = "false";
	if (pyLookupBool(kwds, "no_restore", tmp1) || pyLookupString(kwds, "no_restore", tmp2))
		pyObsoleteHideParameter();

	h->hide();
	Py_RETURN_NONE;
}

bool CPythonInstHint::HintRegister(PyObject *module)
{
	static PyMethodDef hint_methods[] =
	{
		PY_METHODDEF("paint", CPythonInstHint::HintPaint),
		PY_METHODDEF("hide",  CPythonInstHint::HintHide),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("new", CPythonInstHint::HintNewModule),
		{ NULL, NULL, 0, NULL }
	};

	static PyType_Slot hint_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstHint::HintNew },
		{ Py_tp_dealloc, (void *)CPythonInstHint::HintDelete },
		{ Py_tp_methods, (void *)hint_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"simple hint window" },
		{ 0, NULL }
	};

	static PyType_Spec hint_spec =
	{
		"neutrino.hint.hint",
		sizeof(PyHintObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		hint_slots
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.hint",
		"simple hint window",
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

	hint_type = (PyTypeObject *)PyType_FromSpec(&hint_spec);
	if (hint_type == NULL)
	{
		PyErr_Clear();
		Py_DECREF(sub);
		return false;
	}
	if (PyModule_AddObject(sub, "hint", (PyObject *)hint_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)hint_type);
		hint_type = NULL;
		Py_DECREF(sub);
		return false;
	}

	return PythonInstAddSubmodule(module, "hint", sub);
}
