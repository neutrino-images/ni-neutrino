/*
 * python components text
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

#include "py_cc_text.h"

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <system/settings.h>
#include <gui/components/cc.h>
#include <driver/fontrenderer.h>
#include <driver/neutrinofonts.h>
#include <neutrino.h>

#include "pyinstance.h"

typedef struct
{
	PyObject_HEAD
	CPyCCText *d;
} PyCCTextObject;

static PyTypeObject *cctext_type = NULL;

CPythonInstCCText *CPythonInstCCText::getInstance()
{
	static CPythonInstCCText *PythonInstCCText = NULL;

	if (!PythonInstCCText)
		PythonInstCCText = new CPythonInstCCText();
	return PythonInstCCText;
}

static CPyCCText *ccTextOf(PyObject *self)
{
	PyCCTextObject *o = (PyCCTextObject *)self;
	return o->d;
}

/* ctext.new(parent=None, x=OFFSET["INNER_MID"], y=OFFSET["INNER_SMALL"],
 *	dx=-1, dy=-1, text="", text_mode="", font_text=FONT["MENU"],
 *	has_shadow=False, color_text=COL["MENUCONTENT_TEXT"],
 *	color_frame=COL["FRAME_PLUS_0"], color_body=COL["MENUCONTENT_PLUS_0"],
 *	color_shadow=COL["SHADOW_PLUS_0"]) */
PyObject *CPythonInstCCText::CCTextNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		PyErr_SetString(PyExc_TypeError, "ctext.new() takes keyword arguments only");
		return NULL;
	}

	CComponentsForm *pw = pyLookupParentForm(kwds, "parent");

	long x = OFFSET_INNER_MID, y = OFFSET_INNER_SMALL, dx = -1, dy = -1;
	std::string text = "";
	std::string tmpMode = "";
	long font_text = SNeutrinoSettings::FONT_TYPE_MENU;
	unsigned long color_text = (unsigned long)COL_MENUCONTENT_TEXT;
	unsigned long color_frame = (unsigned long)COL_FRAME_PLUS_0;
	unsigned long color_body = (unsigned long)COL_MENUCONTENT_PLUS_0;
	unsigned long color_shadow = (unsigned long)COL_SHADOW_PLUS_0;

	pyLookupInt(kwds, "x", x);
	pyLookupInt(kwds, "y", y);
	pyLookupInt(kwds, "dx", dx);
	pyLookupInt(kwds, "dy", dy);
	pyLookupString(kwds, "text", text);
	/* to prevent possible ambiguity of text mode, use 'text_mode' instead
	 * 'mode' in api; 'mode' is deprecated, but still available */
	pyLookupString(kwds, "text_mode", tmpMode);
	pyLookupString(kwds, "mode", tmpMode);
	pyLookupInt(kwds, "font_text", font_text);

	long shadow_mode = CC_SHADOW_OFF;
	bool shadow = false;
	if (pyLookupBoolCompat(kwds, "has_shadow", shadow))
		shadow_mode = shadow ? CC_SHADOW_ON : CC_SHADOW_OFF;

	pyLookupUnsigned(kwds, "color_text", color_text);
	pyLookupUnsigned(kwds, "color_frame", color_frame);
	pyLookupUnsigned(kwds, "color_body", color_body);
	pyLookupUnsigned(kwds, "color_shadow", color_shadow);

	/* The mode, the font, the size, the colours and who deletes the text are
	 * the script api's; this binding reads them and hands them over. */
	ScriptText::params_t pars;
	pars.parent       = pw;
	pars.x            = (int)x;
	pars.y            = (int)y;
	pars.dx           = (int)dx;
	pars.dy           = (int)dy;
	pars.text         = text;
	pars.mode         = tmpMode;
	pars.font_text    = (int)font_text;
	pars.shadow_mode  = (int)shadow_mode;
	pars.color_text   = color_text;
	pars.color_frame  = color_frame;
	pars.color_body   = color_body;
	pars.color_shadow = color_shadow;

	PyCCTextObject *self = (PyCCTextObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		return NULL;
	}

	self->d = new CPyCCText(pars);
	return (PyObject *)self;
}

PyObject *CPythonInstCCText::CCTextNewModule(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
	if (cctext_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "ctext is not registered");
		return NULL;
	}
	return CCTextNew(cctext_type, args, kwds);
}

void CPythonInstCCText::CCTextDelete(PyObject *self)
{
	PyCCTextObject *o = (PyCCTextObject *)self;
	delete o->d;
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

PyObject *CPythonInstCCText::CCTextPaint(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyCCText *D = ccTextOf(self);
	if (D == NULL || D->ct == NULL)
		Py_RETURN_NONE;

	bool do_save_bg = true;
	pyLookupBoolCompat(kwds, "do_save_bg", do_save_bg);

	D->ct->paint(do_save_bg);
	Py_RETURN_NONE;
}

PyObject *CPythonInstCCText::CCTextHide(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyCCText *D = ccTextOf(self);
	if (D == NULL || D->ct == NULL)
		Py_RETURN_NONE;

	bool tmp1 = false;
	std::string tmp2 = "false";
	if (pyLookupBool(kwds, "no_restore", tmp1) || pyLookupString(kwds, "no_restore", tmp2))
		pyObsoleteHideParameter();

	if (D->parent)
	{
		D->ct->setText("", D->mode, g_Font[D->font_text]);
		D->ct->paint();
	}
	else
		D->ct->hide();

	Py_RETURN_NONE;
}

PyObject *CPythonInstCCText::CCTextKill(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyCCText *D = ccTextOf(self);
	if (D == NULL || D->ct == NULL)
		Py_RETURN_NONE;

	unsigned long color_bg = (unsigned long)COL_BACKGROUND_PLUS_0;
	long corner_radius = -1;
	pyLookupUnsigned(kwds, "color_bg", color_bg);
	pyLookupInt(kwds, "corner_radius", corner_radius);

	D->ct->kill((fb_pixel_t)color_bg, (int)corner_radius, ~CC_FBDATA_TYPES);
	Py_RETURN_NONE;
}

PyObject *CPythonInstCCText::CCTextSetText(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyCCText *D = ccTextOf(self);
	if (D == NULL || D->ct == NULL)
		Py_RETURN_NONE;

	std::string text = "";
	long mode = D->mode;
	long font_text = D->font_text;
	pyLookupString(kwds, "text", text);
	pyLookupInt(kwds, "mode", mode);
	pyLookupInt(kwds, "font_text", font_text);

	D->ct->setText(text, (int)mode, g_Font[font_text]);
	Py_RETURN_NONE;
}

/* t.getLines(text=None): the lines of the text handed in, or of the text
 * the component holds. */
PyObject *CPythonInstCCText::CCTextGetLines(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "text", NULL };
	const char *text = NULL;
	CPyCCText *D = ccTextOf(self);

	if (D == NULL || D->ct == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "ctext is gone");
		return NULL;
	}

	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &text))
			return NULL;
		return PyLong_FromLong((long)CTextBox::getLines(text));
	}
	else
	{
		std::string t = "";
		if (pyLookupString(kwds, "text", t))
			return PyLong_FromLong((long)CTextBox::getLines(t.c_str()));
	}

	long lines = 0;
	CTextBox *ctb = D->ct->getCTextBoxObject();
	D->ct->initCCText();
	if (ctb)
		lines = (long)ctb->getLines();

	return PyLong_FromLong(lines);
}


/* t.scroll(dir="down", pages=1), pages=-1 for all of them */
PyObject *CPythonInstCCText::CCTextScroll(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyCCText *D = ccTextOf(self);
	if (D == NULL || D->ct == NULL)
		Py_RETURN_NONE;

	std::string tmp = "true";
	pyLookupString(kwds, "dir", tmp);
	bool scrollDown = (tmp == "down" || tmp == "1");
	long pages = 1;
	pyLookupInt(kwds, "pages", pages);

	CTextBox *ctb = D->ct->getCTextBoxObject();
	if (ctb)
	{
		if (pages == -1)
			pages = ctb->getPages();
		ctb->enableBackgroundPaint(true);
		if (scrollDown)
			ctb->scrollPageDown((int)pages);
		else
			ctb->scrollPageUp((int)pages);
		ctb->enableBackgroundPaint(false);
	}

	Py_RETURN_NONE;
}

PyObject *CPythonInstCCText::CCTextSetCenterPos(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyCCText *D = ccTextOf(self);
	if (D == NULL || D->ct == NULL)
		Py_RETURN_NONE;

	long tmp_along_mode = 0, along_mode = CC_ALONG_X | CC_ALONG_Y;
	if (pyLookupInt(kwds, "along_mode", tmp_along_mode))
	{
		if ((tmp_along_mode & CC_ALONG_X) || (tmp_along_mode & CC_ALONG_Y))
			along_mode = tmp_along_mode;
	}

	D->ct->setCenterPos((int)along_mode);
	Py_RETURN_NONE;
}

PyObject *CPythonInstCCText::CCTextEnableUTF8(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyCCText *D = ccTextOf(self);
	if (D == NULL || D->ct == NULL)
		Py_RETURN_NONE;

	bool utf8_encoded = true;
	pyLookupBoolCompat(kwds, "utf8_encoded", utf8_encoded);

	D->ct->enableUTF8(utf8_encoded);
	Py_RETURN_NONE;
}

PyObject *CPythonInstCCText::CCTextSetDimensionsAll(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "x", "y", "w", "h", NULL };
	CPyCCText *D = ccTextOf(self);
	int x = 0, y = 0, dw = 0, dh = 0;

	if (D == NULL || D->ct == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "ctext is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iiii", (char **)kwlist, &x, &y, &dw, &dh))
		return NULL;

	if (x > -1 && y > -1 && dw > 1 && dh > 1)
	{
		if (dh > (int)CFrameBuffer::getInstance()->getScreenHeight())
			dh = (int)CFrameBuffer::getInstance()->getScreenHeight();
		if (dw > (int)CFrameBuffer::getInstance()->getScreenWidth())
			dw = (int)CFrameBuffer::getInstance()->getScreenWidth();
		if (x > dw)
			x = 0;
		if (y > dh)
			y = 0;
		D->ct->setDimensionsAll(x, y, dw, dh);
	}

	Py_RETURN_NONE;
}

bool CPythonInstCCText::CCTextRegister(PyObject *module)
{
	static PyMethodDef cctext_methods[] =
	{
		PY_METHODDEF("paint",            CPythonInstCCText::CCTextPaint),
		PY_METHODDEF("hide",             CPythonInstCCText::CCTextHide),
		PY_METHODDEF("kill",             CPythonInstCCText::CCTextKill),
		PY_METHODDEF("setText",          CPythonInstCCText::CCTextSetText),
		PY_METHODDEF("getLines",         CPythonInstCCText::CCTextGetLines),
		PY_METHODDEF("scroll",           CPythonInstCCText::CCTextScroll),
		PY_METHODDEF("setCenterPos",     CPythonInstCCText::CCTextSetCenterPos),
		PY_METHODDEF("enableUTF8",       CPythonInstCCText::CCTextEnableUTF8),
		PY_METHODDEF("setDimensionsAll", CPythonInstCCText::CCTextSetDimensionsAll),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("new", CPythonInstCCText::CCTextNewModule),
		{ NULL, NULL, 0, NULL }
	};

	static PyType_Slot cctext_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstCCText::CCTextNew },
		{ Py_tp_dealloc, (void *)CPythonInstCCText::CCTextDelete },
		{ Py_tp_methods, (void *)cctext_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"components text" },
		{ 0, NULL }
	};

	static PyType_Spec cctext_spec =
	{
		"neutrino.ctext.ctext",
		sizeof(PyCCTextObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		cctext_slots
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.ctext",
		"components text",
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

	cctext_type = (PyTypeObject *)PyType_FromSpec(&cctext_spec);
	if (cctext_type == NULL)
	{
		PyErr_Clear();
		Py_DECREF(sub);
		return false;
	}
	if (PyModule_AddObject(sub, "ctext", (PyObject *)cctext_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)cctext_type);
		cctext_type = NULL;
		Py_DECREF(sub);
		return false;
	}

	return PythonInstAddSubmodule(module, "ctext", sub);
}

