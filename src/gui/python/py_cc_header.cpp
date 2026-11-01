/*
 * python components header
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

#include "py_cc_header.h"

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <gui/components/cc.h>
#include <neutrino.h>

#include "pyinstance.h"

typedef struct
{
	PyObject_HEAD
	CPyCCHeader *d;
} PyCCHeaderObject;

static PyTypeObject *ccheader_type = NULL;

CPythonInstCCHeader *CPythonInstCCHeader::getInstance()
{
	static CPythonInstCCHeader *PythonInstCCHeader = NULL;

	if (!PythonInstCCHeader)
		PythonInstCCHeader = new CPythonInstCCHeader();
	return PythonInstCCHeader;
}

static CPyCCHeader *ccHeaderOf(PyObject *self)
{
	PyCCHeaderObject *o = (PyCCHeaderObject *)self;
	return o->d;
}

/* header.new(parent=None, x=10, y=10, dx=100, dy=100, caption="", icon="",
 *	shadow_mode=CC["SHADOW_OFF"], color_frame=COL["FRAME_PLUS_0"],
 *	color_body=COL["MENUCONTENT_PLUS_0"], color_shadow=COL["SHADOW_PLUS_0"]) */
PyObject *CPythonInstCCHeader::CCHeaderNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		PyErr_SetString(PyExc_TypeError, "header.new() takes keyword arguments only");
		return NULL;
	}

	CComponentsForm *pw = pyLookupParentForm(kwds, "parent");

	long x = 10, y = 10, dx = 100, dy = 100, buttons = 0;
	long shadow_mode = CC_SHADOW_OFF;
	std::string caption, icon;
	unsigned long color_frame = (unsigned long)COL_FRAME_PLUS_0;
	unsigned long color_body = (unsigned long)COL_MENUCONTENT_PLUS_0;
	unsigned long color_shadow = (unsigned long)COL_SHADOW_PLUS_0;

	pyLookupInt(kwds, "x", x);
	pyLookupInt(kwds, "y", y);
	pyLookupInt(kwds, "dx", dx);
	pyLookupInt(kwds, "dy", dy);
	pyLookupString(kwds, "caption", caption);
	pyLookupString(kwds, "icon", icon);
	pyLookupInt(kwds, "shadow_mode", shadow_mode);
	pyLookupInt(kwds, "buttons", buttons);
	pyLookupUnsigned(kwds, "color_frame", color_frame);
	pyLookupUnsigned(kwds, "color_body", color_body);
	pyLookupUnsigned(kwds, "color_shadow", color_shadow);

	PyCCHeaderObject *self = (PyCCHeaderObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		return NULL;
	}

	self->d = new CPyCCHeader();
	self->d->ch = new CComponentsHeader((int)x, (int)y, (int)dx, (int)dy,
			caption, icon, (int)buttons,
			pw, (int)shadow_mode,
			(fb_pixel_t)pyCheckMagicMask((unsigned int)color_frame),
			(fb_pixel_t)pyCheckMagicMask((unsigned int)color_body),
			(fb_pixel_t)pyCheckMagicMask((unsigned int)color_shadow));
	self->d->parent = pw;
	return (PyObject *)self;
}

PyObject *CPythonInstCCHeader::CCHeaderNewModule(PyObject *self, PyObject *args, PyObject *kwds)
{
	if (ccheader_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "header is not registered");
		return NULL;
	}
	return CCHeaderNew(ccheader_type, args, kwds);
}

void CPythonInstCCHeader::CCHeaderDelete(PyObject *self)
{
	PyCCHeaderObject *o = (PyCCHeaderObject *)self;
	delete o->d;
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

PyObject *CPythonInstCCHeader::CCHeaderPaint(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyCCHeader *D = ccHeaderOf(self);
	if (D == NULL || D->ch == NULL)
		Py_RETURN_NONE;

	bool do_save_bg = true;
	pyLookupBoolCompat(kwds, "do_save_bg", do_save_bg);

	D->ch->paint(do_save_bg);
	Py_RETURN_NONE;
}

PyObject *CPythonInstCCHeader::CCHeaderHide(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyCCHeader *D = ccHeaderOf(self);
	if (D == NULL || D->ch == NULL)
		Py_RETURN_NONE;

	D->ch->hide();
	Py_RETURN_NONE;
}

bool CPythonInstCCHeader::CCHeaderRegister(PyObject *module)
{
	static PyMethodDef ccheader_methods[] =
	{
		PY_METHODDEF("paint", CPythonInstCCHeader::CCHeaderPaint),
		PY_METHODDEF("hide",  CPythonInstCCHeader::CCHeaderHide),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("new", CPythonInstCCHeader::CCHeaderNewModule),
		{ NULL, NULL, 0, NULL }
	};

	static PyType_Slot ccheader_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstCCHeader::CCHeaderNew },
		{ Py_tp_dealloc, (void *)CPythonInstCCHeader::CCHeaderDelete },
		{ Py_tp_methods, (void *)ccheader_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"components header" },
		{ 0, NULL }
	};

	static PyType_Spec ccheader_spec =
	{
		"neutrino.header.header",
		sizeof(PyCCHeaderObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		ccheader_slots
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.header",
		"components header",
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

	ccheader_type = (PyTypeObject *)PyType_FromSpec(&ccheader_spec);
	if (ccheader_type == NULL)
	{
		PyErr_Clear();
		Py_DECREF(sub);
		return false;
	}
	if (PyModule_AddObject(sub, "header", (PyObject *)ccheader_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)ccheader_type);
		ccheader_type = NULL;
		Py_DECREF(sub);
		return false;
	}

	return PythonInstAddSubmodule(module, "header", sub);
}
