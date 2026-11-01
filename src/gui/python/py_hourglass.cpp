/*
 * python hourglass/loader
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

#include "py_hourglass.h"

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
	CPyHourGlass *d;
} PyHourGlassObject;

static PyTypeObject *hourglass_type = NULL;

CPythonInstHourGlass *CPythonInstHourGlass::getInstance()
{
	static CPythonInstHourGlass *PythonInstHourGlass = NULL;

	if (!PythonInstHourGlass)
		PythonInstHourGlass = new CPythonInstHourGlass();
	return PythonInstHourGlass;
}

/* hourglass.new(x=20, y=20, dx=48, dy=48, image_basename="hourglass",
 *	interval=HG_AUTO_PAINT_INTERVAL, parent=None,
 *	shadow_mode=CC["SHADOW_OFF"], color_frame=COL["FRAME_PLUS_0"],
 *	color_background=COL["MENUCONTENT_PLUS_0"],
 *	color_shadow=COL["SHADOW_PLUS_0"]) */
PyObject *CPythonInstHourGlass::HourGlassNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		PyErr_SetString(PyExc_TypeError, "hourglass.new() takes keyword arguments only");
		return NULL;
	}

	long x = 20, y = 20, dx = 48, dy = 48;
	pyLookupInt(kwds, "x", x);
	pyLookupInt(kwds, "y", y);
	pyLookupInt(kwds, "dx", dx);
	pyLookupInt(kwds, "dy", dy);

	std::string image_basename = "hourglass";
	pyLookupString(kwds, "image_basename", image_basename);

	long interval = HG_AUTO_PAINT_INTERVAL;
	pyLookupInt(kwds, "interval", interval);

	CComponentsForm *parent_container = pyLookupParentForm(kwds, "parent");

	long shadow_mode = CC_SHADOW_OFF;
	pyLookupInt(kwds, "shadow_mode", shadow_mode);

	unsigned long color_frame = (unsigned long)COL_FRAME_PLUS_0;
	unsigned long color_background = (unsigned long)COL_MENUCONTENT_PLUS_0;
	unsigned long color_shadow = (unsigned long)COL_SHADOW_PLUS_0;
	pyLookupUnsigned(kwds, "color_frame", color_frame);
	pyLookupUnsigned(kwds, "color_background", color_background);
	pyLookupUnsigned(kwds, "color_shadow", color_shadow);

	PyHourGlassObject *self = (PyHourGlassObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		return NULL;
	}

	self->d = new CPyHourGlass();
	self->d->h = new CHourGlass((int)x, (int)y, (int)dx, (int)dy, image_basename,
			(int64_t)interval, parent_container, (int)shadow_mode,
			(fb_pixel_t)pyCheckMagicMask((unsigned int)color_frame),
			(fb_pixel_t)pyCheckMagicMask((unsigned int)color_background),
			(fb_pixel_t)pyCheckMagicMask((unsigned int)color_shadow));

	return (PyObject *)self;
}

PyObject *CPythonInstHourGlass::HourGlassNewModule(PyObject *self, PyObject *args, PyObject *kwds)
{
	if (hourglass_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "hourglass is not registered");
		return NULL;
	}
	return HourGlassNew(hourglass_type, args, kwds);
}

void CPythonInstHourGlass::HourGlassDelete(PyObject *self)
{
	PyHourGlassObject *o = (PyHourGlassObject *)self;
	if (o->d != NULL && o->d->h != NULL)
		o->d->h->kill();
	delete o->d;
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

PyObject *CPythonInstHourGlass::HourGlassPaint(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyHourGlassObject *o = (PyHourGlassObject *)self;
	if (o->d == NULL || o->d->h == NULL)
		Py_RETURN_NONE;

	bool do_save_bg = true;
	pyLookupBoolCompat(kwds, "do_save_bg", do_save_bg);

	o->d->h->paint(do_save_bg);
	Py_RETURN_NONE;
}

PyObject *CPythonInstHourGlass::HourGlassHide(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyHourGlassObject *o = (PyHourGlassObject *)self;
	if (o->d == NULL || o->d->h == NULL)
		Py_RETURN_NONE;

	bool tmp1 = false;
	std::string tmp2 = "false";
	if (pyLookupBool(kwds, "no_restore", tmp1) || pyLookupString(kwds, "no_restore", tmp2))
		pyObsoleteHideParameter();

	o->d->h->hide();
	Py_RETURN_NONE;
}

bool CPythonInstHourGlass::HourGlassRegister(PyObject *module)
{
	static PyMethodDef hourglass_methods[] =
	{
		PY_METHODDEF("paint", CPythonInstHourGlass::HourGlassPaint),
		PY_METHODDEF("hide",  CPythonInstHourGlass::HourGlassHide),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("new", CPythonInstHourGlass::HourGlassNewModule),
		{ NULL, NULL, 0, NULL }
	};

	static PyType_Slot hourglass_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstHourGlass::HourGlassNew },
		{ Py_tp_dealloc, (void *)CPythonInstHourGlass::HourGlassDelete },
		{ Py_tp_methods, (void *)hourglass_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"hourglass/loader" },
		{ 0, NULL }
	};

	static PyType_Spec hourglass_spec =
	{
		"neutrino.hourglass.hourglass",
		sizeof(PyHourGlassObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		hourglass_slots
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.hourglass",
		"hourglass/loader",
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

	hourglass_type = (PyTypeObject *)PyType_FromSpec(&hourglass_spec);
	if (hourglass_type == NULL)
	{
		PyErr_Clear();
		Py_DECREF(sub);
		return false;
	}
	if (PyModule_AddObject(sub, "hourglass", (PyObject *)hourglass_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)hourglass_type);
		hourglass_type = NULL;
		Py_DECREF(sub);
		return false;
	}

	return PythonInstAddSubmodule(module, "hourglass", sub);
}

