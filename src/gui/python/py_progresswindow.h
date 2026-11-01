/*
 * python progress window
 *
 * The Lua binding in lua_progresswindow.cpp, read as
 *
 *	from neutrino import cprogresswindow
 *	w = cprogresswindow.new(name="Title")
 *	w.paint()
 *	w.showStatus(prog=50, max=100, statusText="half")
 *	w.hide()
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

#ifndef _PYPROGRESSWINDOW_H_
#define _PYPROGRESSWINDOW_H_

#include "pyinstance_helpers.h"

#include <gui/widget/progresswindow.h>

class CPyCProgressWindow
{
	public:
		CProgressWindow *w;
		CPyCProgressWindow() { w = NULL; }
		~CPyCProgressWindow() { delete w; }
};

class CPythonInstProgressWindow
{
	public:
		CPythonInstProgressWindow() {};
		~CPythonInstProgressWindow() {};
		static CPythonInstProgressWindow *getInstance();
		static bool ProgressWindowRegister(PyObject *module);

	private:
		static PyObject *ProgressWindowNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static PyObject *ProgressWindowNewModule(PyObject *self, PyObject *args, PyObject *kwds);
		static void ProgressWindowDelete(PyObject *self);
		static PyObject *ProgressWindowPaint(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ProgressWindowHide(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ProgressWindowSetTitle(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ProgressWindowShowLocalStatus(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ProgressWindowShowGlobalStatus(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ProgressWindowShowStatus(PyObject *self, PyObject *args, PyObject *kwds, bool local);
};

#endif //_PYPROGRESSWINDOW_H_
