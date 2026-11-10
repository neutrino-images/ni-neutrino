/*
 * python neutrino window
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

#include "py_neutrino.h"

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <system/settings.h>
#include <system/helpers.h>
#include <gui/color_custom.h>
#include <gui/components/cc.h>
#include <gui/movieplayer.h>
#include <driver/fontrenderer.h>
#include <driver/neutrinofonts.h>
#include <driver/pictureviewer/pictureviewer.h>
#include <driver/rcinput.h>
#include <neutrino.h>

#include "pyinstance.h"

/* a colour a script names as one of the box own */
#include "../scriptapi/scriptcolor.h"

extern CPictureViewer *g_PicViewer;

typedef struct
{
	PyObject_HEAD
	CPyWindowData *d;
} PyWindowObject;

static PyTypeObject *window_type = NULL;

CPythonInstNeutrino *CPythonInstNeutrino::getInstance()
{
	static CPythonInstNeutrino *PythonInstNeutrino = NULL;

	if (!PythonInstNeutrino)
		PythonInstNeutrino = new CPythonInstNeutrino();
	return PythonInstNeutrino;
}

static CPyWindowData *windowOf(PyObject *self)
{
	PyWindowObject *o = (PyWindowObject *)self;
	return o->d;
}

/* The font a call names: a number into the box's own fonts, or one of this
 * window's dynamic fonts where the call said so. */
Font *CPythonInstNeutrino::fontFromArgs(PyObject *self, PyObject *args, Py_ssize_t index,
		bool isDynFont, bool &found)
{
	found = true;
	CPyWindowData *W = windowOf(self);
	if (W == NULL || args == NULL || PyTuple_GET_SIZE(args) <= index)
	{
		found = false;
		return NULL;
	}

	PyObject *item = PyTuple_GET_ITEM(args, index);
	long value = PyLong_AsLong(item);
	if (value == -1 && PyErr_Occurred())
	{
		PyErr_Clear();
		found = false;
		return NULL;
	}

	if (!isDynFont)
	{
		if (value >= SNeutrinoSettings::FONT_TYPE_COUNT || value < 0)
			value = SNeutrinoSettings::FONT_TYPE_MENU;
		return g_Font[value];
	}

	for (pyfontmap_iterator_t it = W->fontmap.begin(); it != W->fontmap.end(); ++it)
		if (it->first == value)
			return it->second;

	printf("[CPythonInstNeutrino::%s] no font found\n", __func__);
	found = false;
	return NULL;
}

/* neutrino.new(x=SCREEN["OFF_X"], y=SCREEN["OFF_Y"],
 *	w=SCREEN["END_X"] - x, h=SCREEN["END_Y"] - y) */
PyObject *CPythonInstNeutrino::WindowNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	long x = g_settings.screen_StartX;
	long y = g_settings.screen_StartY;
	long w = g_settings.screen_EndX - x;
	long h = g_settings.screen_EndY - y;
	Py_ssize_t count = (args != NULL) ? PyTuple_GET_SIZE(args) : 0;

	if (count > 0 && !pyArgLong(PyTuple_GET_ITEM(args, 0), x))
		return NULL;
	if (count > 1 && !pyArgLong(PyTuple_GET_ITEM(args, 1), y))
		return NULL;
	if (count > 2 && !pyArgLong(PyTuple_GET_ITEM(args, 2), w))
		return NULL;
	if (count > 3 && !pyArgLong(PyTuple_GET_ITEM(args, 3), h))
		return NULL;

	pyLookupInt(kwds, "x", x);
	pyLookupInt(kwds, "y", y);
	pyLookupInt(kwds, "w", w);
	pyLookupInt(kwds, "h", h);

	PyWindowObject *self = (PyWindowObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		return NULL;
	}

	self->d = new CPyWindowData();
	self->d->fbwin = new CFBWindow((int)x, (int)y, (int)w, (int)h);
	self->d->rcinput = g_RCInput;
	return (PyObject *)self;
}

PyObject *CPythonInstNeutrino::WindowNewModule(PyObject *self, PyObject *args, PyObject *kwds)
{
	if (window_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "neutrino.window is not registered");
		return NULL;
	}
	return WindowNew(window_type, args, kwds);
}

/* What the Lua binding does when its window goes: the screens it saved and
 * the fonts it made are let go of, and a movie that was left playing behind
 * the script is taken up again. */
void CPythonInstNeutrino::WindowDelete(PyObject *self)
{
	PyWindowObject *o = (PyWindowObject *)self;
	CPyWindowData *W = o->d;

	if (W != NULL && W->fbwin != NULL)
	{
		for (pyscreenmap_iterator_t it = W->screenmap.begin(); it != W->screenmap.end(); ++it)
			if (it->second != NULL)
				delete[] it->second;
		W->screenmap.clear();
		W->fontmap.clear();
	}
	CNeutrinoFonts::getInstance()->deleteDynFontExtAll();

	bool block = CMoviePlayerGui::getInstance().getBlockedFromPlugin();
	bool play = CMoviePlayerGui::getInstance().Playing();
	if (block && play)
	{
		CMoviePlayerGui::getInstance().setBlockedFromPlugin(false);
		CMoviePlayerGui::getInstance().restoreNeutrino();
	}
	/* Where the Lua binding also rezaps the channel on a blank video, the
	 * video binding does it; that one is not here yet. */

	if (W != NULL)
	{
		delete W->fbwin;
		W->rcinput = NULL;
		delete W;
	}
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

/* w.GetInput(timeout=0): the key and its data, as the Lua binding answers
 * them, with RC["timeout"] -1 and RC["nokey"] -2. */
PyObject *CPythonInstNeutrino::GetInput(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyWindowData *W = windowOf(self);
	long timeout = 0;
	neutrino_msg_t msg;
	neutrino_msg_data_t data;

	if (W == NULL || W->fbwin == NULL || W->rcinput == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "window is gone");
		return NULL;
	}
	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		if (!pyArgLong(PyTuple_GET_ITEM(args, 0), timeout))
			return NULL;
	}
	pyLookupInt(kwds, "timeout", timeout);

	W->rcinput->getMsg_ms(&msg, &data, (int)timeout);

	/* TODO: I'm not sure if this works... */
	if (msg != CRCInput::RC_timeout && msg > CRCInput::RC_MaxRC)
	{
		PY_DEBUG("CPythonInstNeutrino::%s: msg 0x%08x data 0x%08x\n", __func__, msg, data);
		CNeutrinoApp::getInstance()->handleMsg(msg, data);
	}

	return Py_BuildValue("(iI)", (int)msg, (unsigned int)data);
}

/* w.Blit(): the Lua binding answers nothing here, and neither does this. */
PyObject *CPythonInstNeutrino::Blit(PyObject *self, PyObject *args, PyObject *kwds)
{
	Py_RETURN_NONE;
}

PyObject *CPythonInstNeutrino::GetLanguage(PyObject *self, PyObject *args, PyObject *kwds)
{
	// copied under the lock: a script can run on a thread of its own
	const std::string lang = settingsText(g_settings.language);
	return PyUnicode_FromStringAndSize(lang.c_str(), (Py_ssize_t)lang.length());
}

/* w.PaintBox(x, y, w, h, color, radius=0, corner=CORNER["ALL"]) */
PyObject *CPythonInstNeutrino::PaintBox(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "x", "y", "w", "h", "color", "radius", "corner", NULL };
	CPyWindowData *W = windowOf(self);
	long x = 0, y = 0, w = 0, h = 0, radius = 0, corner = CORNER_ALL;
	unsigned long c = 0;

	if (W == NULL || W->fbwin == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "window is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iiiik|ii", (char **)kwlist, &x, &y, &w, &h, &c, &radius, &corner))
		return NULL;

	/* those checks should be done in CFBWindow instead... */
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (w < 0 || x + w > W->fbwin->dx)
		w = W->fbwin->dx - x;
	if (h < 0 || y + h > W->fbwin->dy)
		h = W->fbwin->dy - y;
	c = ScriptColorResolve((unsigned int)c);
	W->fbwin->paintBoxRel((int)x, (int)y, (int)w, (int)h, (fb_pixel_t)c, (int)radius, (int)corner);

	Py_RETURN_NONE;
}

/* w.paintHLineRel(x, dx, y, color) */
PyObject *CPythonInstNeutrino::PaintHLineRel(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "x", "dx", "y", "color", NULL };
	CPyWindowData *W = windowOf(self);
	long x = 0, dx = 0, y = 0;
	unsigned long c = 0;

	if (W == NULL || W->fbwin == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "window is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iiik", (char **)kwlist, &x, &dx, &y, &c))
		return NULL;

	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (dx < 0 || x + dx > W->fbwin->dx)
		dx = W->fbwin->dx - x;
	c = ScriptColorResolve((unsigned int)c);
	W->fbwin->paintHLineRel((int)x, (int)dx, (int)y, (fb_pixel_t)c);

	Py_RETURN_NONE;
}

/* w.paintVLineRel(x, y, dy, color) */
PyObject *CPythonInstNeutrino::PaintVLineRel(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "x", "y", "dy", "color", NULL };
	CPyWindowData *W = windowOf(self);
	long x = 0, y = 0, dy = 0;
	unsigned long c = 0;

	if (W == NULL || W->fbwin == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "window is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iiik", (char **)kwlist, &x, &y, &dy, &c))
		return NULL;

	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (dy < 0 || y + dy > W->fbwin->dy)
		dy = W->fbwin->dy - y;
	c = ScriptColorResolve((unsigned int)c);
	W->fbwin->paintVLineRel((int)x, (int)y, (int)dy, (fb_pixel_t)c);

	Py_RETURN_NONE;
}


/* w.RenderString(font, text, x, y[, color, w, boxh, center]), and with a
 * True in front of the font number where it names a dynamic font of this
 * window. Answers the width the text takes, as the Lua binding answers. */
PyObject *CPythonInstNeutrino::RenderString(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyWindowData *W = windowOf(self);
	if (W == NULL || W->fbwin == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "window is gone");
		return NULL;
	}

	Py_ssize_t numargs = (args != NULL) ? PyTuple_GET_SIZE(args) : 0;
	Py_ssize_t step = 0;
	bool isDynFont = false;

	if (numargs < 4)
	{
		PyErr_SetString(PyExc_TypeError, "RenderString() takes a font, a text, an x and a y");
		return NULL;
	}
	if (PyBool_Check(PyTuple_GET_ITEM(args, 0)))
	{
		isDynFont = (PyTuple_GET_ITEM(args, 0) == Py_True);
		step = 1;
	}

	bool found = true;
	Font *font = fontFromArgs(self, args, step, isDynFont, found);
	if (!found)
		return PyLong_FromLong(0); /* no font found */

	std::string text = "";
	long x = 0, y = 0, w = 0, boxh = 0, center = 0;
	unsigned long c = COL_MENUCONTENT_TEXT;

	/* The Lua binding counts from its own first argument, which is the
	 * window; the window is the method here, so every index it names after
	 * the font is one further along than the argument tuple goes. */
	if (!pyArgString(PyTuple_GET_ITEM(args, 1 + step), text) ||
	    !pyArgLong(PyTuple_GET_ITEM(args, 2 + step), x) ||
	    !pyArgLong(PyTuple_GET_ITEM(args, 3 + step), y))
	{
		PyErr_SetString(PyExc_TypeError, "RenderString() takes a text, an x and a y");
		return NULL;
	}
	if (numargs > 4 + step)
		pyArgUnsigned(PyTuple_GET_ITEM(args, 4 + step), c);
	if (numargs > 5 + step)
		pyArgLong(PyTuple_GET_ITEM(args, 5 + step), w);
	else
		w = W->fbwin->dx - x;
	if (numargs > 6 + step)
		pyArgLong(PyTuple_GET_ITEM(args, 6 + step), boxh);
	if (numargs > 7 + step)
		pyArgLong(PyTuple_GET_ITEM(args, 7 + step), center);

	int rwidth = font->getRenderWidth(text);

	if (center) /* center the text inside the box */
	{
		if (rwidth < w)
			x += (w - rwidth) / 2;
	}
	c = ScriptColorResolve((unsigned int)c);
	if (boxh > -1) /* if boxh < 0, don't paint string */
		W->fbwin->RenderString(font, (int)x, (int)y, (int)w, text.c_str(), (fb_pixel_t)c, (int)boxh);

	return PyLong_FromLong(rwidth);
}

/* w.getRenderWidth(font, text), with the same True in front of a dynamic
 * font number. */
PyObject *CPythonInstNeutrino::GetRenderWidth(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyWindowData *W = windowOf(self);
	if (W == NULL || W->fbwin == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "window is gone");
		return NULL;
	}

	Py_ssize_t numargs = (args != NULL) ? PyTuple_GET_SIZE(args) : 0;
	Py_ssize_t step = 0;
	bool isDynFont = false;

	if (numargs < 2)
	{
		PyErr_SetString(PyExc_TypeError, "getRenderWidth() takes a font and a text");
		return NULL;
	}
	if (PyBool_Check(PyTuple_GET_ITEM(args, 0)))
	{
		isDynFont = (PyTuple_GET_ITEM(args, 0) == Py_True);
		step = 1;
	}

	bool found = true;
	Font *font = fontFromArgs(self, args, step, isDynFont, found);
	if (!found)
		return PyLong_FromLong(0); /* no font found */

	std::string text = "";
	if (!pyArgString(PyTuple_GET_ITEM(args, 1 + step), text))
	{
		PyErr_SetString(PyExc_TypeError, "getRenderWidth() takes a text");
		return NULL;
	}

	return PyLong_FromLong((long)font->getRenderWidth(text));
}

/* w.FontHeight(font), with the same True in front of a dynamic font. */
PyObject *CPythonInstNeutrino::FontHeight(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyWindowData *W = windowOf(self);
	if (W == NULL || W->fbwin == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "window is gone");
		return NULL;
	}

	Py_ssize_t numargs = (args != NULL) ? PyTuple_GET_SIZE(args) : 0;
	Py_ssize_t step = 0;
	bool isDynFont = false;

	if (numargs < 1)
	{
		PyErr_SetString(PyExc_TypeError, "FontHeight() takes a font");
		return NULL;
	}
	if (PyBool_Check(PyTuple_GET_ITEM(args, 0)))
	{
		isDynFont = (PyTuple_GET_ITEM(args, 0) == Py_True);
		step = 1;
	}

	bool found = true;
	Font *font = fontFromArgs(self, args, step, isDynFont, found);
	if (!found)
		return PyLong_FromLong(0); /* no font found */

	return PyLong_FromLong((long)font->getHeight());
}

/* w.getDynFont(dx, dy[, text, style]): the number of a font this window
 * makes, and whether it could be made, as the Lua binding answers two
 * values. */
PyObject *CPythonInstNeutrino::GetDynFont(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyWindowData *W = windowOf(self);
	if (W == NULL || W->fbwin == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "window is gone");
		return NULL;
	}

	Py_ssize_t numargs = (args != NULL) ? PyTuple_GET_SIZE(args) : 0;
	if (numargs < 2)
	{
		printf("CPythonInstNeutrino::%s: not enough arguments (%d, expected 2)\n", __func__, (int)numargs);
		PyErr_SetString(PyExc_TypeError, "getDynFont() takes a width and a height");
		return NULL;
	}

	long fontID = (long)W->fontmap.size();
	if (fontID >= CNeutrinoFonts::DYNFONTEXT_MAX)
		return Py_BuildValue("(Oi)", Py_None, CPythonInstance::DYNFONT_MAXIMUM_FONTS);

	long dx = 0, dy = 0, style = CNeutrinoFonts::FONT_STYLE_REGULAR;
	std::string text = "";

	if (!pyArgLong(PyTuple_GET_ITEM(args, 0), dx))
	{
		PyErr_SetString(PyExc_TypeError, "getDynFont() takes a width");
		return NULL;
	}
	if (dx > (long)CFrameBuffer::getInstance()->getScreenWidth(true))
		return Py_BuildValue("(Oi)", Py_None, CPythonInstance::DYNFONT_TOO_WIDE);

	if (!pyArgLong(PyTuple_GET_ITEM(args, 1), dy))
	{
		PyErr_SetString(PyExc_TypeError, "getDynFont() takes a height");
		return NULL;
	}
	if (dy > 100)
		return Py_BuildValue("(Oi)", Py_None, CPythonInstance::DYNFONT_TOO_HIGH);

	if (numargs > 2)
		pyArgString(PyTuple_GET_ITEM(args, 2), text);
	if (numargs > 3)
	{
		pyArgLong(PyTuple_GET_ITEM(args, 3), style);
		if (style > CNeutrinoFonts::FONT_STYLE_ITALIC)
			style = CNeutrinoFonts::FONT_STYLE_REGULAR;
	}

	int dx_i = (int)dx, dy_i = (int)dy, font_i = (int)fontID, style_i = (int)style;
	Font *f = CNeutrinoFonts::getInstance()->getDynFontExt(dx_i, dy_i, font_i, text, style_i);

	long id = fontID + 1;
	W->fontmap.insert(pyfontmap_pair_t(id, f));

	return Py_BuildValue("(ii)", (int)id, CPythonInstance::DYNFONT_NO_ERROR);
}

/* w.PaintIcon(fname, x, y, h, o) */
PyObject *CPythonInstNeutrino::PaintIcon(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "fname", "x", "y", "h", "o", NULL };
	CPyWindowData *W = windowOf(self);
	const char *fname = "";
	long x = 0, y = 0, h = 0, o = 0;

	if (W == NULL || W->fbwin == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "window is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "siiii", (char **)kwlist, &fname, &x, &y, &h, &o))
		return NULL;

	W->fbwin->paintIcon(fname, (int)x, (int)y, (int)h, (unsigned int)o);
	Py_RETURN_NONE;
}

/* w.DisplayImage(fname, x, y, w, h, trans=0) */
PyObject *CPythonInstNeutrino::DisplayImage(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "fname", "x", "y", "w", "h", "trans", NULL };
	CPyWindowData *W = windowOf(self);
	const char *fname = "";
	long x = 0, y = 0, w = 0, h = 0, trans = 0;

	if (W == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "window is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "siiii|i", (char **)kwlist, &fname, &x, &y, &w, &h, &trans))
		return NULL;

	g_PicViewer->DisplayImage(fname, (int)x, (int)y, (int)w, (int)h, (int)trans);
	Py_RETURN_NONE;
}

/* w.GetSize(fname): the width and the height of the picture, as the Lua
 * binding answers two values. */
PyObject *CPythonInstNeutrino::GetSize(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "fname", NULL };
	CPyWindowData *W = windowOf(self);
	const char *fname = "";
	int w = 0, h = 0;

	if (W == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "window is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &fname))
		return NULL;

	g_PicViewer->getSize(fname, &w, &h);
	return Py_BuildValue("(ii)", w, h);
}

/* w.saveScreen(x, y, w, h): the number the screen is kept under. */
PyObject *CPythonInstNeutrino::SaveScreen(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "x", "y", "w", "h", NULL };
	CPyWindowData *W = windowOf(self);
	long x = 0, y = 0, w = 0, h = 0;

	if (W == NULL || W->fbwin == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "window is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iiii", (char **)kwlist, &x, &y, &w, &h))
		return NULL;

	fb_pixel_t *buf = W->fbwin->saveScreen((int)x, (int)y, (int)w, (int)h);
	long id = (long)W->screenmap.size() + 1;
	W->screenmap.insert(pyscreenmap_pair_t(id, buf));

	return PyLong_FromLong(id);
}

/* w.restoreScreen(x, y, w, h, id, del) */
PyObject *CPythonInstNeutrino::RestoreScreen(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "x", "y", "w", "h", "id", "del", NULL };
	CPyWindowData *W = windowOf(self);
	long x = 0, y = 0, w = 0, h = 0, id = 0;
	int del = 0;
	fb_pixel_t *buf = NULL;

	if (W == NULL || W->fbwin == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "window is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iiiiip", (char **)kwlist, &x, &y, &w, &h, &id, &del))
		return NULL;

	for (pyscreenmap_iterator_t it = W->screenmap.begin(); it != W->screenmap.end(); ++it)
	{
		if (it->first == id)
		{
			buf = it->second;
			if (del)
				it->second = NULL;
			break;
		}
	}
	if (buf != NULL)
		W->fbwin->restoreScreen((int)x, (int)y, (int)w, (int)h, buf, del ? true : false);

	Py_RETURN_NONE;
}

/* w.deleteSavedScreen(id) */
PyObject *CPythonInstNeutrino::DeleteSavedScreen(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "id", NULL };
	CPyWindowData *W = windowOf(self);
	long id = 0;

	if (W == NULL || W->fbwin == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "window is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", (char **)kwlist, &id))
		return NULL;

	for (pyscreenmap_iterator_t it = W->screenmap.begin(); it != W->screenmap.end(); ++it)
	{
		if (it->first == id)
		{
			if (it->second != NULL)
			{
				delete[] it->second;
				it->second = NULL;
			}
			break;
		}
	}

	Py_RETURN_NONE;
}

/* w.scale2Res(value) */
PyObject *CPythonInstNeutrino::Scale2Res(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "value", NULL };
	CPyWindowData *W = windowOf(self);
	long value = 0;

	if (W == NULL || W->fbwin == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "window is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", (char **)kwlist, &value))
		return NULL;

	return PyLong_FromLong(CFrameBuffer::getInstance()->scale2Res((int)value));
}


/* The class goes on the module a script imports itself, under the name
 * 'window', with a function 'new' beside it, so that both
 *
 *	from neutrino import window, new
 *	w = window()
 *	w = new()
 *
 * answer, the second being the shape the Lua binding has. */
bool CPythonInstNeutrino::NeutrinoRegister(PyObject *module)
{
	static PyMethodDef window_methods[] =
	{
		PY_METHODDEF("GetInput",           CPythonInstNeutrino::GetInput),
		PY_METHODDEF("Blit",               CPythonInstNeutrino::Blit),
		PY_METHODDEF("GetLanguage",        CPythonInstNeutrino::GetLanguage),
		PY_METHODDEF("PaintBox",           CPythonInstNeutrino::PaintBox),
		PY_METHODDEF("paintHLineRel",      CPythonInstNeutrino::PaintHLineRel),
		PY_METHODDEF("paintVLineRel",      CPythonInstNeutrino::PaintVLineRel),
		PY_METHODDEF("RenderString",       CPythonInstNeutrino::RenderString),
		PY_METHODDEF("getRenderWidth",     CPythonInstNeutrino::GetRenderWidth),
		PY_METHODDEF("FontHeight",         CPythonInstNeutrino::FontHeight),
		PY_METHODDEF("getDynFont",         CPythonInstNeutrino::GetDynFont),
		PY_METHODDEF("PaintIcon",          CPythonInstNeutrino::PaintIcon),
		PY_METHODDEF("DisplayImage",       CPythonInstNeutrino::DisplayImage),
		PY_METHODDEF("GetSize",            CPythonInstNeutrino::GetSize),
		PY_METHODDEF("saveScreen",         CPythonInstNeutrino::SaveScreen),
		PY_METHODDEF("restoreScreen",      CPythonInstNeutrino::RestoreScreen),
		PY_METHODDEF("deleteSavedScreen",  CPythonInstNeutrino::DeleteSavedScreen),
		PY_METHODDEF("scale2Res",          CPythonInstNeutrino::Scale2Res),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef new_def =
	{
		"new",
		(PyCFunction)(void (*)(void))CPythonInstNeutrino::WindowNewModule,
		METH_VARARGS | METH_KEYWORDS,
		"a window of the API, drawn into and read from"
	};

	static PyType_Slot window_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstNeutrino::WindowNew },
		{ Py_tp_dealloc, (void *)CPythonInstNeutrino::WindowDelete },
		{ Py_tp_methods, (void *)window_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"a window of the API, drawn into and read from" },
		{ 0, NULL }
	};

	static PyType_Spec window_spec =
	{
		"neutrino.window",
		sizeof(PyWindowObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		window_slots
	};

	if (module == NULL)
		return false;

	window_type = (PyTypeObject *)PyType_FromSpec(&window_spec);
	if (window_type == NULL)
	{
		PyErr_Clear();
		return false;
	}
	if (PyModule_AddObject(module, "window", (PyObject *)window_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)window_type);
		window_type = NULL;
		return false;
	}

	PyObject *newfunc = PyCFunction_New(&new_def, NULL);
	if (newfunc == NULL)
	{
		PyErr_Clear();
		return false;
	}
	if (PyModule_AddObject(module, "new", newfunc) < 0)
	{
		PyErr_Clear();
		Py_DECREF(newfunc);
		return false;
	}

	return true;
}

