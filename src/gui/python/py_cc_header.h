/*
 * python components header
 *
 * The Lua binding in lua_cc_header.cpp, read as
 *
 *	from neutrino import cwindow, header
 *	w = cwindow.new(name="Title")
 *	h = header.new(parent=w, caption="Caption")
 *	h.paint()
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

#ifndef _PYCCHEADER_H
#define _PYCCHEADER_H

#include "pyinstance_helpers.h"

#include <gui/components/cc.h>

class CPyCCHeader
{
	public:
		CComponentsHeader *ch;
		CComponentsForm *parent;
		CPyCCHeader() { ch = NULL; parent = NULL; }
		~CPyCCHeader() { if (parent == NULL) delete ch; }
};

class CPythonInstCCHeader
{
	public:
		CPythonInstCCHeader() {};
		~CPythonInstCCHeader() {};
		static CPythonInstCCHeader *getInstance();
		static bool CCHeaderRegister(PyObject *module);

	private:
		static PyObject *CCHeaderNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static PyObject *CCHeaderNewModule(PyObject *self, PyObject *args, PyObject *kwds);
		static void CCHeaderDelete(PyObject *self);
		static PyObject *CCHeaderPaint(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCHeaderHide(PyObject *self, PyObject *args, PyObject *kwds);
};

#endif //_PYCCHEADER_H
