/*
 * python components picture
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

#include "py_cc_picture.h"

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
	CPyCCPicture *d;
} PyCCPictureObject;

static PyTypeObject *ccpicture_type = NULL;

CPythonInstCCPicture *CPythonInstCCPicture::getInstance()
{
	static CPythonInstCCPicture *PythonInstCCPicture = NULL;

	if (!PythonInstCCPicture)
		PythonInstCCPicture = new CPythonInstCCPicture();
	return PythonInstCCPicture;
}

static CPyCCPicture *ccPictureOf(PyObject *self)
{
	PyCCPictureObject *o = (PyCCPictureObject *)self;
	return o->d;
}

/* cpicture.new(parent=None, x=10, y=10, dx=100, dy=100, image="",
 *	has_shadow=False, color_frame=COL["FRAME_PLUS_0"],
 *	color_background=COL["MENUCONTENT_PLUS_0"],
 *	color_shadow=COL["SHADOW_PLUS_0"],
 *	transparency=CFrameBuffer["TM_NONE"])
 *
 * dx and dy both nought mean the picture is not scaled, as the Lua binding
 * reads it. */
PyObject *CPythonInstCCPicture::CCPictureNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		PyErr_SetString(PyExc_TypeError, "cpicture.new() takes keyword arguments only");
		return NULL;
	}

	CComponentsForm *pw = pyLookupParentForm(kwds, "parent");

	long x = 10, y = 10, dx = 100, dy = 100;
	std::string image_name = "";
	long alignment = 0;
	unsigned long color_frame = (unsigned long)COL_FRAME_PLUS_0;
	unsigned long color_background = (unsigned long)COL_MENUCONTENT_PLUS_0;
	unsigned long color_shadow = (unsigned long)COL_SHADOW_PLUS_0;
	long transparency = CFrameBuffer::TM_NONE;

	pyLookupInt(kwds, "x", x);
	pyLookupInt(kwds, "y", y);
	pyLookupInt(kwds, "dx", dx);
	pyLookupInt(kwds, "dy", dy);
	pyLookupString(kwds, "image", image_name);
	pyLookupInt(kwds, "alignment", alignment);
	if (alignment)
		dprintf(DEBUG_NORMAL, "[CPythonInstance][%s - %d] invalid argument: 'alignment' has no effect!\n", __func__, __LINE__);

	bool has_shadow = false;
	pyLookupBoolCompat(kwds, "has_shadow", has_shadow);

	pyLookupUnsigned(kwds, "color_frame", color_frame);
	pyLookupUnsigned(kwds, "color_background", color_background);
	pyLookupUnsigned(kwds, "color_shadow", color_shadow);
	pyLookupInt(kwds, "transparency", transparency);

	/* The size, the colours and who deletes the picture are the script api's;
	 * this binding reads them and hands them over. */
	ScriptPicture::params_t pars;
	pars.parent           = pw;
	pars.x                = (int)x;
	pars.y                = (int)y;
	pars.dx               = (int)dx;
	pars.dy               = (int)dy;
	pars.image            = image_name;
	pars.has_shadow       = has_shadow;
	pars.transparency     = (int)transparency;
	pars.color_frame      = color_frame;
	pars.color_background = color_background;
	pars.color_shadow     = color_shadow;

	PyCCPictureObject *self = (PyCCPictureObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		return NULL;
	}

	self->d = new CPyCCPicture(pars);
	return (PyObject *)self;
}

PyObject *CPythonInstCCPicture::CCPictureNewModule(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
	if (ccpicture_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "cpicture is not registered");
		return NULL;
	}
	return CCPictureNew(ccpicture_type, args, kwds);
}

void CPythonInstCCPicture::CCPictureDelete(PyObject *self)
{
	PyCCPictureObject *o = (PyCCPictureObject *)self;
	delete o->d;
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

PyObject *CPythonInstCCPicture::CCPicturePaint(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyCCPicture *D = ccPictureOf(self);
	if (D == NULL || D->cp == NULL)
		Py_RETURN_NONE;

	bool do_save_bg = true;
	pyLookupBoolCompat(kwds, "do_save_bg", do_save_bg);

	D->cp->paint(do_save_bg);
	Py_RETURN_NONE;
}

PyObject *CPythonInstCCPicture::CCPictureHide(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyCCPicture *D = ccPictureOf(self);
	if (D == NULL || D->cp == NULL)
		Py_RETURN_NONE;

	bool tmp1 = false;
	std::string tmp2 = "false";
	if (pyLookupBool(kwds, "no_restore", tmp1) || pyLookupString(kwds, "no_restore", tmp2))
		pyObsoleteHideParameter();

	if (D->parent)
	{
		D->cp->setPicture("");
		D->cp->paint();
	}
	else
		D->cp->hide();

	Py_RETURN_NONE;
}

PyObject *CPythonInstCCPicture::CCPictureSetPicture(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyCCPicture *D = ccPictureOf(self);
	if (D == NULL || D->cp == NULL)
		Py_RETURN_NONE;

	std::string image_name = "";
	pyLookupString(kwds, "image", image_name);

	D->cp->setPicture(image_name);
	Py_RETURN_NONE;
}

PyObject *CPythonInstCCPicture::CCPictureSetCenterPos(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyCCPicture *D = ccPictureOf(self);
	if (D == NULL || D->cp == NULL)
		Py_RETURN_NONE;

	long tmp_along_mode = 0, along_mode = CC_ALONG_X | CC_ALONG_Y;
	if (pyLookupInt(kwds, "along_mode", tmp_along_mode))
	{
		if ((tmp_along_mode & CC_ALONG_X) || (tmp_along_mode & CC_ALONG_Y))
			along_mode = tmp_along_mode;
	}

	D->cp->setCenterPos((int)along_mode);
	Py_RETURN_NONE;
}

PyObject *CPythonInstCCPicture::CCPictureGetHeight(PyObject *self, PyObject * /*args*/, PyObject * /*kwds*/)
{
	CPyCCPicture *D = ccPictureOf(self);
	if (D == NULL || D->cp == NULL)
		Py_RETURN_NONE;

	return PyLong_FromLong(D->cp->getHeight());
}

PyObject *CPythonInstCCPicture::CCPictureGetWidth(PyObject *self, PyObject * /*args*/, PyObject * /*kwds*/)
{
	CPyCCPicture *D = ccPictureOf(self);
	if (D == NULL || D->cp == NULL)
		Py_RETURN_NONE;

	return PyLong_FromLong(D->cp->getWidth());
}

/* p.doPaintBg(paint_bg=False) */
PyObject *CPythonInstCCPicture::CCPictureSetDoPaintBG(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyCCPicture *D = ccPictureOf(self);
	if (D == NULL || D->cp == NULL)
		Py_RETURN_NONE;

	bool paint_bg = false;
	pyLookupBoolCompat(kwds, "paint_bg", paint_bg);

	D->cp->doPaintBg(paint_bg);
	Py_RETURN_NONE;
}

bool CPythonInstCCPicture::CCPictureRegister(PyObject *module)
{
	static PyMethodDef ccpicture_methods[] =
	{
		PY_METHODDEF("paint",        CPythonInstCCPicture::CCPicturePaint),
		PY_METHODDEF("hide",         CPythonInstCCPicture::CCPictureHide),
		PY_METHODDEF("setPicture",   CPythonInstCCPicture::CCPictureSetPicture),
		PY_METHODDEF("setCenterPos", CPythonInstCCPicture::CCPictureSetCenterPos),
		PY_METHODDEF("getHeight",    CPythonInstCCPicture::CCPictureGetHeight),
		PY_METHODDEF("getWidth",     CPythonInstCCPicture::CCPictureGetWidth),
		PY_METHODDEF("doPaintBg",    CPythonInstCCPicture::CCPictureSetDoPaintBG),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("new", CPythonInstCCPicture::CCPictureNewModule),
		{ NULL, NULL, 0, NULL }
	};

	static PyType_Slot ccpicture_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstCCPicture::CCPictureNew },
		{ Py_tp_dealloc, (void *)CPythonInstCCPicture::CCPictureDelete },
		{ Py_tp_methods, (void *)ccpicture_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"components picture" },
		{ 0, NULL }
	};

	static PyType_Spec ccpicture_spec =
	{
		"neutrino.cpicture.cpicture",
		sizeof(PyCCPictureObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		ccpicture_slots
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.cpicture",
		"components picture",
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

	ccpicture_type = (PyTypeObject *)PyType_FromSpec(&ccpicture_spec);
	if (ccpicture_type == NULL)
	{
		PyErr_Clear();
		Py_DECREF(sub);
		return false;
	}
	if (PyModule_AddObject(sub, "cpicture", (PyObject *)ccpicture_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)ccpicture_type);
		ccpicture_type = NULL;
		Py_DECREF(sub);
		return false;
	}

	return PythonInstAddSubmodule(module, "cpicture", sub);
}
