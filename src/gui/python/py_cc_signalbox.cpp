/*
 * python components signalbox
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

#include "py_cc_signalbox.h"

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <gui/components/cc.h>
#include <neutrino.h>

#include <zapit/femanager.h>

#include "pyinstance.h"

typedef struct
{
	PyObject_HEAD
	CPyCCSignalBox *d;
} PyCCSignalBoxObject;

static PyTypeObject *ccsignalbox_type = NULL;

CPythonInstCCSignalbox *CPythonInstCCSignalbox::getInstance()
{
	static CPythonInstCCSignalbox *PythonInstCCSignalbox = NULL;

	if (!PythonInstCCSignalbox)
		PythonInstCCSignalbox = new CPythonInstCCSignalbox();
	return PythonInstCCSignalbox;
}

static CPyCCSignalBox *ccSignalBoxOf(PyObject *self)
{
	PyCCSignalBoxObject *o = (PyCCSignalBoxObject *)self;
	return o->d;
}

/* signalbox.new(parent=None, x=110, y=150, dx=430, dy=150, vertical=True) */
PyObject *CPythonInstCCSignalbox::CCSignalBoxNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		PyErr_SetString(PyExc_TypeError, "signalbox.new() takes keyword arguments only");
		return NULL;
	}

	CComponentsForm *pw = pyLookupParentForm(kwds, "parent");

	long x = 110, y = 150, dx = 430, dy = 150;
	bool vertical = true;
	pyLookupInt(kwds, "x", x);
	pyLookupInt(kwds, "y", y);
	pyLookupInt(kwds, "dx", dx);
	pyLookupInt(kwds, "dy", dy);
	pyLookupBoolCompat(kwds, "vertical", vertical);

	/* The form it goes into and who deletes the box are the script api's;
	 * this binding reads them and hands them over. */
	ScriptSignalbox::params_t pars;
	pars.parent   = pw;
	pars.x        = (int)x;
	pars.y        = (int)y;
	pars.dx       = (int)dx;
	pars.dy       = (int)dy;
	pars.vertical = vertical;

	PyCCSignalBoxObject *self = (PyCCSignalBoxObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		return NULL;
	}

	self->d = new CPyCCSignalBox(pars);
	return (PyObject *)self;
}

PyObject *CPythonInstCCSignalbox::CCSignalBoxNewModule(PyObject *self, PyObject *args, PyObject *kwds)
{
	if (ccsignalbox_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "signalbox is not registered");
		return NULL;
	}
	return CCSignalBoxNew(ccsignalbox_type, args, kwds);
}

void CPythonInstCCSignalbox::CCSignalBoxDelete(PyObject *self)
{
	PyCCSignalBoxObject *o = (PyCCSignalBoxObject *)self;
	delete o->d;
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

PyObject *CPythonInstCCSignalbox::CCSignalBoxPaint(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyCCSignalBox *D = ccSignalBoxOf(self);
	if (D == NULL || D->s == NULL)
		Py_RETURN_NONE;

	bool do_save_bg = true;
	pyLookupBoolCompat(kwds, "do_save_bg", do_save_bg);

	D->s->paint(do_save_bg);
	Py_RETURN_NONE;
}

PyObject *CPythonInstCCSignalbox::CCSignalBoxSetCenterPos(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyCCSignalBox *D = ccSignalBoxOf(self);
	if (D == NULL || D->s == NULL)
		Py_RETURN_NONE;

	long tmp_along_mode = 0, along_mode = CC_ALONG_X | CC_ALONG_Y;
	if (pyLookupInt(kwds, "along_mode", tmp_along_mode))
	{
		if ((tmp_along_mode & CC_ALONG_X) || (tmp_along_mode & CC_ALONG_Y))
			along_mode = tmp_along_mode;
	}

	D->s->setCenterPos((int)along_mode);
	Py_RETURN_NONE;
}

bool CPythonInstCCSignalbox::CCSignalBoxRegister(PyObject *module)
{
	static PyMethodDef ccsignalbox_methods[] =
	{
		PY_METHODDEF("paint",        CPythonInstCCSignalbox::CCSignalBoxPaint),
		PY_METHODDEF("setCenterPos", CPythonInstCCSignalbox::CCSignalBoxSetCenterPos),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("new", CPythonInstCCSignalbox::CCSignalBoxNewModule),
		{ NULL, NULL, 0, NULL }
	};

	static PyType_Slot ccsignalbox_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstCCSignalbox::CCSignalBoxNew },
		{ Py_tp_dealloc, (void *)CPythonInstCCSignalbox::CCSignalBoxDelete },
		{ Py_tp_methods, (void *)ccsignalbox_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"components signalbox" },
		{ 0, NULL }
	};

	static PyType_Spec ccsignalbox_spec =
	{
		"neutrino.signalbox.signalbox",
		sizeof(PyCCSignalBoxObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		ccsignalbox_slots
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.signalbox",
		"components signalbox",
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

	ccsignalbox_type = (PyTypeObject *)PyType_FromSpec(&ccsignalbox_spec);
	if (ccsignalbox_type == NULL)
	{
		PyErr_Clear();
		Py_DECREF(sub);
		return false;
	}
	if (PyModule_AddObject(sub, "signalbox", (PyObject *)ccsignalbox_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)ccsignalbox_type);
		ccsignalbox_type = NULL;
		Py_DECREF(sub);
		return false;
	}

	return PythonInstAddSubmodule(module, "signalbox", sub);
}
