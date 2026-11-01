/*
 * python hintbox
 *
 * The Lua binding in lua_hintbox.cpp, read as
 *
 *	from neutrino import hintbox
 *	b = hintbox.new(name="Title", text="text")
 *	b.exec(5)
 *	b.hide()
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

#ifndef _PYHINTBOX_H
#define _PYHINTBOX_H

#include "pyinstance_helpers.h"

#include <gui/widget/hintbox.h>

class CPyHintbox
{
	public:
		CHintBox *b;
		char *caption;
		CPyHintbox() { b = NULL; caption = NULL; }
		~CPyHintbox() { if (caption) free(caption); delete b; }
};

class CPythonInstHintbox
{
	public:
		CPythonInstHintbox() {};
		~CPythonInstHintbox() {};
		static CPythonInstHintbox *getInstance();
		static bool HintboxRegister(PyObject *module);

	private:
		static PyObject *HintboxNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static PyObject *HintboxNewModule(PyObject *self, PyObject *args, PyObject *kwds);
		static void HintboxDelete(PyObject *self);
		static PyObject *HintboxExec(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *HintboxPaint(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *HintboxHide(PyObject *self, PyObject *args, PyObject *kwds);
};

#endif //_PYHINTBOX_H
