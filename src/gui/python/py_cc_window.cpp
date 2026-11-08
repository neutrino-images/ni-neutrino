/*
 * python components window
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

#include "py_cc_window.h"

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
	CPyCCWindow *d;
} PyCCWindowObject;

static PyTypeObject *ccwindow_type = NULL;

CPythonInstCCWindow *CPythonInstCCWindow::getInstance()
{
	static CPythonInstCCWindow *PythonInstCCWindow = NULL;

	if (!PythonInstCCWindow)
		PythonInstCCWindow = new CPythonInstCCWindow();
	return PythonInstCCWindow;
}

static ScriptWindow *scriptWindowOf(PyObject *self)
{
	PyCCWindowObject *o = (PyCCWindowObject *)self;
	return (o->d != NULL) ? o->d->sw : NULL;
}

/* cwindow.new(name="", x=100, y=100, dx=450, dy=250, icon="info",
 *	has_shadow=False, show_header=True, show_footer=True,
 *	color_frame=COL["FRAME_PLUS_0"], color_body=COL["MENUCONTENT_PLUS_0"],
 *	color_shadow=COL["SHADOW_PLUS_0"], btnRed=..., btn0=..., btnPlay=...)
 *
 * The keys are the ones the Lua binding reads out of its table. */
PyObject *CPythonInstCCWindow::CCWindowNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		PyErr_SetString(PyExc_TypeError, "cwindow.new() takes keyword arguments only");
		return NULL;
	}

	/* The parameters the Lua binding reads, in the same names and with the
	 * same defaults. What is done with them is done by the script API, so
	 * that both bindings ask for one window and cannot come to differ. */
	ScriptWindow::params_t pars;
	pars.icon = std::string(NEUTRINO_ICON_INFO);
	pars.x = 100;
	pars.y = 100;
	pars.dx = 450;
	pars.dy = 250;
	pars.shadow = CC_SHADOW_OFF;
	pars.color_frame = (unsigned long)COL_FRAME_PLUS_0;
	pars.color_body = (unsigned long)COL_MENUCONTENT_PLUS_0;
	pars.color_shadow = (unsigned long)COL_SHADOW_PLUS_0;
	pars.show_header = true;
	pars.show_footer = true;

	/* The four dimensions are handed over as a long, while the script API
	 * keeps them as the int the components want. */
	long x = (long)pars.x, y = (long)pars.y, dx = (long)pars.dx, dy = (long)pars.dy;
	pyLookupInt(kwds, "x", x);
	pyLookupInt(kwds, "y", y);
	pyLookupInt(kwds, "dx", dx);
	pyLookupInt(kwds, "dy", dy);
	pars.x = (int)x;
	pars.y = (int)y;
	pars.dx = (int)dx;
	pars.dy = (int)dy;
	pyLookupString(kwds, "name", pars.name) || pyLookupString(kwds, "title", pars.name) || pyLookupString(kwds, "caption", pars.name);
	pyLookupString(kwds, "icon", pars.icon);

	bool shadow = false;
	if (pyLookupBoolCompat(kwds, "has_shadow", shadow))
		pars.shadow = shadow ? CC_SHADOW_ON : CC_SHADOW_OFF;

	pyLookupUnsigned(kwds, "color_frame", pars.color_frame);
	pyLookupUnsigned(kwds, "color_body", pars.color_body);
	pyLookupUnsigned(kwds, "color_shadow", pars.color_shadow);
	pyLookupBoolCompat(kwds, "show_header", pars.show_header);
	pyLookupBoolCompat(kwds, "show_footer", pars.show_footer);

	/* The labels of the footer's buttons, each with the icon of the key it
	 * belongs to, in the order the Lua binding writes them. */
	static const struct { const char *key; const char *icon; } btn_keys[] = {
		{ "btnRed",       NEUTRINO_ICON_BUTTON_RED },
		{ "btnGreen",     NEUTRINO_ICON_BUTTON_GREEN },
		{ "btnYellow",    NEUTRINO_ICON_BUTTON_YELLOW },
		{ "btnBlue",      NEUTRINO_ICON_BUTTON_BLUE },
		{ "btn0",         NEUTRINO_ICON_BUTTON_0 },
		{ "btn1",         NEUTRINO_ICON_BUTTON_1 },
		{ "btn2",         NEUTRINO_ICON_BUTTON_2 },
		{ "btn3",         NEUTRINO_ICON_BUTTON_3 },
		{ "btn4",         NEUTRINO_ICON_BUTTON_4 },
		{ "btn5",         NEUTRINO_ICON_BUTTON_5 },
		{ "btn6",         NEUTRINO_ICON_BUTTON_6 },
		{ "btn7",         NEUTRINO_ICON_BUTTON_7 },
		{ "btn8",         NEUTRINO_ICON_BUTTON_8 },
		{ "btn9",         NEUTRINO_ICON_BUTTON_9 },
		{ "btnPlay",      NEUTRINO_ICON_BUTTON_PLAY },
		{ "btnPlayPause", NEUTRINO_ICON_BUTTON_PLAY },
		{ "btnOk",        NEUTRINO_ICON_BUTTON_OKAY },
		{ "btnSetup",     NEUTRINO_ICON_BUTTON_MENU },
		{ NULL, NULL }
	};

	for (int i = 0; btn_keys[i].key != NULL; i++)
	{
		std::string label = "";
		if (!pyLookupString(kwds, btn_keys[i].key, label) || label.empty())
			continue;
		ScriptWindow::button_t btn;
		btn.button = btn_keys[i].icon;
		btn.text = label;
		pars.buttons.push_back(btn);
	}

	PyCCWindowObject *self = (PyCCWindowObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		return NULL;
	}

	self->d = new CPyCCWindow();
	self->d->sw = new ScriptWindow(pars);

	/* So that another binding can take this window as its parent, where a
	 * Lua binding hands on its userdata. */
	PyObject *capsule = PyCapsule_New(self->d->sw->body(), PY_FORM_CAPSULE, NULL);
	if (capsule != NULL)
	{
		if (PyObject_SetAttrString((PyObject *)self, PY_FORM_ATTRIBUTE, capsule) < 0)
			PyErr_Clear();
		Py_DECREF(capsule);
	}
	else
		PyErr_Clear();

	return (PyObject *)self;
}

PyObject *CPythonInstCCWindow::CCWindowNewModule(PyObject *self, PyObject *args, PyObject *kwds)
{
	if (ccwindow_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "cwindow is not registered");
		return NULL;
	}
	return CCWindowNew(ccwindow_type, args, kwds);
}

void CPythonInstCCWindow::CCWindowDelete(PyObject *self)
{
	PyCCWindowObject *o = (PyCCWindowObject *)self;
	delete o->d;
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

PyObject *CPythonInstCCWindow::CCWindowPaint(PyObject *self, PyObject *args, PyObject *kwds)
{
	ScriptWindow *sw = scriptWindowOf(self);
	if (sw == NULL)
		Py_RETURN_NONE;

	bool do_save_bg = true;
	pyLookupBoolCompat(kwds, "do_save_bg", do_save_bg);

	sw->paint(do_save_bg);
	Py_RETURN_NONE;
}

PyObject *CPythonInstCCWindow::CCWindowHide(PyObject *self, PyObject *args, PyObject *kwds)
{
	ScriptWindow *sw = scriptWindowOf(self);
	if (sw == NULL)
		Py_RETURN_NONE;

	bool tmp1 = false;
	std::string tmp2 = "false";
	if (pyLookupBool(kwds, "no_restore", tmp1) || pyLookupString(kwds, "no_restore", tmp2))
		pyObsoleteHideParameter();

	sw->hide();
	Py_RETURN_NONE;
}


PyObject *CPythonInstCCWindow::CCWindowSetCaption(PyObject *self, PyObject *args, PyObject *kwds)
{
	ScriptWindow *sw = scriptWindowOf(self);
	if (sw == NULL)
		Py_RETURN_NONE;

	std::string name = "";
	pyLookupString(kwds, "name", name) || pyLookupString(kwds, "title", name) || pyLookupString(kwds, "caption", name);

	long alignment = (long)DEFAULT_TITLE_ALIGN;
	pyLookupInt(kwds, "alignment", alignment);

	sw->setCaption(name, (int)alignment);
	Py_RETURN_NONE;
}

PyObject *CPythonInstCCWindow::CCWindowSetWindowColor(PyObject *self, PyObject *args, PyObject *kwds)
{
	ScriptWindow *sw = scriptWindowOf(self);
	if (sw == NULL)
		Py_RETURN_NONE;

	unsigned long color = 0;
	if (pyLookupUnsigned(kwds, "color_frame", color))
		sw->setColorFrame(color);
	if (pyLookupUnsigned(kwds, "color_body", color))
		sw->setColorBody(color);
	if (pyLookupUnsigned(kwds, "color_shadow", color))
		sw->setColorShadow(color);

	Py_RETURN_NONE;
}

PyObject *CPythonInstCCWindow::CCWindowPaintHeader(PyObject *self, PyObject *args, PyObject *kwds)
{
	ScriptWindow *sw = scriptWindowOf(self);
	if (sw == NULL)
		Py_RETURN_NONE;

	sw->paintHeader();
	Py_RETURN_NONE;
}

PyObject *CPythonInstCCWindow::CCWindowGetHeaderHeight(PyObject *self, PyObject *args, PyObject *kwds)
{
	ScriptWindow *sw = scriptWindowOf(self);
	if (sw == NULL)
		Py_RETURN_NONE;

	return PyLong_FromLong(sw->headerHeight());
}

PyObject *CPythonInstCCWindow::CCWindowGetFooterHeight(PyObject *self, PyObject *args, PyObject *kwds)
{
	ScriptWindow *sw = scriptWindowOf(self);
	if (sw == NULL)
		Py_RETURN_NONE;

	return PyLong_FromLong(sw->footerHeight());
}

/* the names the Lua binding keeps for a script that still writes them */
PyObject *CPythonInstCCWindow::CCWindowHeaderHeightDep(PyObject *self, PyObject *args, PyObject *kwds)
{
	pyFunctionDeprecated("header_height", "headerHeight");
	return CCWindowGetHeaderHeight(self, args, kwds);
}

PyObject *CPythonInstCCWindow::CCWindowFooterHeightDep(PyObject *self, PyObject *args, PyObject *kwds)
{
	pyFunctionDeprecated("footer_height", "footerHeight");
	return CCWindowGetFooterHeight(self, args, kwds);
}

PyObject *CPythonInstCCWindow::CCWindowSetCenterPos(PyObject *self, PyObject *args, PyObject *kwds)
{
	ScriptWindow *sw = scriptWindowOf(self);
	if (sw == NULL)
		Py_RETURN_NONE;

	long tmp_along_mode = 0, along_mode = CC_ALONG_X | CC_ALONG_Y;
	if (pyLookupInt(kwds, "along_mode", tmp_along_mode))
	{
		if ((tmp_along_mode & CC_ALONG_X) || (tmp_along_mode & CC_ALONG_Y))
			along_mode = tmp_along_mode;
	}

	sw->setCenterPos((int)along_mode);
	Py_RETURN_NONE;
}

PyObject *CPythonInstCCWindow::CCWindowSetDimensionsAll(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "x", "y", "w", "h", NULL };
	ScriptWindow *sw = scriptWindowOf(self);
	int x = 0, y = 0, dw = 0, dh = 0;

	if (sw == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "cwindow is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iiii", (char **)kwlist, &x, &y, &dw, &dh))
		return NULL;

	/* Keeping the window on the screen is not something a binding should
	 * decide for itself, so it is the script API that decides it. */
	sw->setDimensionsAll(x, y, dw, dh);

	Py_RETURN_NONE;
}

PyObject *CPythonInstCCWindow::CCWindowSetBodyImage(PyObject *self, PyObject *args, PyObject *kwds)
{
	ScriptWindow *sw = scriptWindowOf(self);
	if (sw == NULL)
		Py_RETURN_NONE;

	std::string image = "";
	pyLookupString(kwds, "image_path", image);

	sw->setBodyImage(image);
	Py_RETURN_NONE;
}


bool CPythonInstCCWindow::CCWindowRegister(PyObject *module)
{
	static PyMethodDef ccwindow_methods[] =
	{
		PY_METHODDEF("paint",           CPythonInstCCWindow::CCWindowPaint),
		PY_METHODDEF("hide",            CPythonInstCCWindow::CCWindowHide),
		PY_METHODDEF("setCaption",      CPythonInstCCWindow::CCWindowSetCaption),
		PY_METHODDEF("setWindowColor",  CPythonInstCCWindow::CCWindowSetWindowColor),
		PY_METHODDEF("paintHeader",     CPythonInstCCWindow::CCWindowPaintHeader),
		PY_METHODDEF("headerHeight",    CPythonInstCCWindow::CCWindowGetHeaderHeight),
		PY_METHODDEF("footerHeight",    CPythonInstCCWindow::CCWindowGetFooterHeight),
		PY_METHODDEF("header_height",   CPythonInstCCWindow::CCWindowHeaderHeightDep),
		PY_METHODDEF("footer_height",   CPythonInstCCWindow::CCWindowFooterHeightDep),
		PY_METHODDEF("setCenterPos",    CPythonInstCCWindow::CCWindowSetCenterPos),
		PY_METHODDEF("setDimensionsAll", CPythonInstCCWindow::CCWindowSetDimensionsAll),
		PY_METHODDEF("setBodyImage",    CPythonInstCCWindow::CCWindowSetBodyImage),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("new", CPythonInstCCWindow::CCWindowNewModule),
		{ NULL, NULL, 0, NULL }
	};

	static PyType_Slot ccwindow_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstCCWindow::CCWindowNew },
		{ Py_tp_dealloc, (void *)CPythonInstCCWindow::CCWindowDelete },
		{ Py_tp_methods, (void *)ccwindow_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"components window" },
		{ 0, NULL }
	};

	static PyType_Spec ccwindow_spec =
	{
		"neutrino.cwindow.cwindow",
		sizeof(PyCCWindowObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		ccwindow_slots
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.cwindow",
		"components window",
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

	ccwindow_type = (PyTypeObject *)PyType_FromSpec(&ccwindow_spec);
	if (ccwindow_type == NULL)
	{
		PyErr_Clear();
		Py_DECREF(sub);
		return false;
	}
	if (PyModule_AddObject(sub, "cwindow", (PyObject *)ccwindow_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)ccwindow_type);
		ccwindow_type = NULL;
		Py_DECREF(sub);
		return false;
	}

	return PythonInstAddSubmodule(module, "cwindow", sub);
}

