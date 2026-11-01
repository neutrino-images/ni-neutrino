/*
 * python progress window
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

#include "py_progresswindow.h"

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <gui/widget/progresswindow.h>
#include <neutrino.h>

#include "pyinstance.h"

typedef struct
{
	PyObject_HEAD
	CPyCProgressWindow *d;
} PyProgressWindowObject;

static PyTypeObject *progresswindow_type = NULL;

CPythonInstProgressWindow *CPythonInstProgressWindow::getInstance()
{
	static CPythonInstProgressWindow *PythonInstProgressWindow = NULL;

	if (!PythonInstProgressWindow)
		PythonInstProgressWindow = new CPythonInstProgressWindow();
	return PythonInstProgressWindow;
}

static CProgressWindow *progressWindowOf(PyObject *self)
{
	PyProgressWindowObject *o = (PyProgressWindowObject *)self;
	return (o->d != NULL) ? o->d->w : NULL;
}

/* cprogresswindow.new(name="") */
PyObject *CPythonInstProgressWindow::ProgressWindowNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	std::string name = "";
	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		PyObject *first = PyTuple_GET_ITEM(args, 0);
		if (PyUnicode_Check(first))
		{
			const char *s = PyUnicode_AsUTF8(first);
			if (s != NULL)
				name = s;
			else
				PyErr_Clear();
		}
	}
	pyLookupString(kwds, "name", name) || pyLookupString(kwds, "title", name) || pyLookupString(kwds, "caption", name);

	PyProgressWindowObject *self = (PyProgressWindowObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		return NULL;
	}

	self->d = new CPyCProgressWindow();
	self->d->w = new CProgressWindow();
	self->d->w->setTitle(name);
	return (PyObject *)self;
}

PyObject *CPythonInstProgressWindow::ProgressWindowNewModule(PyObject *self, PyObject *args, PyObject *kwds)
{
	if (progresswindow_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "cprogresswindow is not registered");
		return NULL;
	}
	return ProgressWindowNew(progresswindow_type, args, kwds);
}

void CPythonInstProgressWindow::ProgressWindowDelete(PyObject *self)
{
	PyProgressWindowObject *o = (PyProgressWindowObject *)self;
	delete o->d;
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

PyObject *CPythonInstProgressWindow::ProgressWindowPaint(PyObject *self, PyObject *args, PyObject *kwds)
{
	CProgressWindow *w = progressWindowOf(self);
	if (w == NULL)
		Py_RETURN_NONE;

	bool do_save_bg = true;
	pyLookupBoolCompat(kwds, "do_save_bg", do_save_bg);

	w->paint(do_save_bg);
	Py_RETURN_NONE;
}

PyObject *CPythonInstProgressWindow::ProgressWindowHide(PyObject *self, PyObject *args, PyObject *kwds)
{
	CProgressWindow *w = progressWindowOf(self);
	if (w == NULL)
		Py_RETURN_NONE;

	bool tmp1 = false;
	std::string tmp2 = "false";
	if (pyLookupBool(kwds, "no_restore", tmp1) || pyLookupString(kwds, "no_restore", tmp2))
		pyObsoleteHideParameter();

	w->hide();
	Py_RETURN_NONE;
}

PyObject *CPythonInstProgressWindow::ProgressWindowSetTitle(PyObject *self, PyObject *args, PyObject *kwds)
{
	CProgressWindow *w = progressWindowOf(self);
	if (w == NULL)
		Py_RETURN_NONE;

	std::string name = "";
	pyLookupString(kwds, "name", name) || pyLookupString(kwds, "title", name) || pyLookupString(kwds, "caption", name);

	w->setTitle(name);
	Py_RETURN_NONE;
}

/* w.showStatus(prog=0, max=100, statusText=""), the same for the local and
 * the global status bar, as the Lua binding answers both. */
PyObject *CPythonInstProgressWindow::ProgressWindowShowStatus(PyObject *self, PyObject *args, PyObject *kwds, bool local)
{
	CProgressWindow *w = progressWindowOf(self);
	if (w == NULL)
		Py_RETURN_NONE;

	unsigned long prog = 0;
	std::string statusText = std::string();
	long max = 100;

	pyLookupUnsigned(kwds, "prog", prog);
	pyLookupString(kwds, "statusText", statusText);
	pyLookupInt(kwds, "max", max);

	if (local)
		w->showLocalStatus((unsigned int)prog, (unsigned int)max, statusText);
	else
		w->showGlobalStatus((unsigned int)prog, (unsigned int)max, statusText);

	Py_RETURN_NONE;
}

PyObject *CPythonInstProgressWindow::ProgressWindowShowLocalStatus(PyObject *self, PyObject *args, PyObject *kwds)
{
	return ProgressWindowShowStatus(self, args, kwds, true);
}

PyObject *CPythonInstProgressWindow::ProgressWindowShowGlobalStatus(PyObject *self, PyObject *args, PyObject *kwds)
{
	return ProgressWindowShowStatus(self, args, kwds, false);
}

bool CPythonInstProgressWindow::ProgressWindowRegister(PyObject *module)
{
	static PyMethodDef progresswindow_methods[] =
	{
		PY_METHODDEF("paint",            CPythonInstProgressWindow::ProgressWindowPaint),
		PY_METHODDEF("hide",             CPythonInstProgressWindow::ProgressWindowHide),
		PY_METHODDEF("showStatus",       CPythonInstProgressWindow::ProgressWindowShowLocalStatus),
		PY_METHODDEF("showLocalStatus",  CPythonInstProgressWindow::ProgressWindowShowLocalStatus),
		PY_METHODDEF("showGlobalStatus", CPythonInstProgressWindow::ProgressWindowShowGlobalStatus),
		PY_METHODDEF("setTitle",         CPythonInstProgressWindow::ProgressWindowSetTitle),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("new", CPythonInstProgressWindow::ProgressWindowNewModule),
		{ NULL, NULL, 0, NULL }
	};

	static PyType_Slot progresswindow_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstProgressWindow::ProgressWindowNew },
		{ Py_tp_dealloc, (void *)CPythonInstProgressWindow::ProgressWindowDelete },
		{ Py_tp_methods, (void *)progresswindow_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"progress window" },
		{ 0, NULL }
	};

	static PyType_Spec progresswindow_spec =
	{
		"neutrino.cprogresswindow.cprogresswindow",
		sizeof(PyProgressWindowObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		progresswindow_slots
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.cprogresswindow",
		"progress window",
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

	progresswindow_type = (PyTypeObject *)PyType_FromSpec(&progresswindow_spec);
	if (progresswindow_type == NULL)
	{
		PyErr_Clear();
		Py_DECREF(sub);
		return false;
	}
	if (PyModule_AddObject(sub, "cprogresswindow", (PyObject *)progresswindow_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)progresswindow_type);
		progresswindow_type = NULL;
		Py_DECREF(sub);
		return false;
	}

	return PythonInstAddSubmodule(module, "cprogresswindow", sub);
}

