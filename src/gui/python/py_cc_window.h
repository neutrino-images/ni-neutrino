/*
 * python components window
 *
 * The Lua binding in lua_cc_window.cpp, read as
 *
 *	from neutrino import cwindow, ctext
 *	w = cwindow.new(name="Title", x=100, y=100, dx=450, dy=250)
 *	t = ctext.new(parent=w, text="hello")
 *	w.setDimensionsAll(x=100, y=100, w=450, h=250)
 *	w.paint()
 *
 * The dimensions a window is made with are named dx and dy and the ones it
 * is moved to are named w and h, which is what the Lua binding calls them.
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

#ifndef _PYCCWINDOW_H
#define _PYCCWINDOW_H

#include "pyinstance_helpers.h"

#include <gui/components/cc.h>
#include <gui/components/cc_frm_header.h>
#include <gui/scriptapi/script_window.h>

/* The window a script made, held as the shared ScriptWindow: what is done
 * with it is the same for every interpreter, and this binding only fills its
 * parameters out of a python call. */
class CPyCCWindow
{
	public:
		ScriptWindow *sw;
		CPyCCWindow() { sw = NULL; }
		~CPyCCWindow() { delete sw; }
};

class CPythonInstCCWindow
{
	public:
		CPythonInstCCWindow() {};
		~CPythonInstCCWindow() {};
		static CPythonInstCCWindow *getInstance();
		static bool CCWindowRegister(PyObject *module);

	private:
		static PyObject *CCWindowNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static PyObject *CCWindowNewModule(PyObject *self, PyObject *args, PyObject *kwds);
		static void CCWindowDelete(PyObject *self);

		static PyObject *CCWindowPaint(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCWindowHide(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCWindowSetCaption(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCWindowSetWindowColor(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCWindowPaintHeader(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCWindowGetHeaderHeight(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCWindowGetFooterHeight(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCWindowHeaderHeightDep(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCWindowFooterHeightDep(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCWindowSetCenterPos(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCWindowSetDimensionsAll(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCWindowSetBodyImage(PyObject *self, PyObject *args, PyObject *kwds);
};

#endif //_PYCCWINDOW_H
