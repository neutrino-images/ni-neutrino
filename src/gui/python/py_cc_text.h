/*
 * python components text
 *
 * The Lua binding in lua_cc_text.cpp, read as
 *
 *	from neutrino import cwindow, ctext
 *	w = cwindow.new(name="Title")
 *	t = ctext.new(parent=w, text="hello", x=20, y=20, dx=100, dy=40)
 *	t.paint()
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

#ifndef _PYCCTEXT_H
#define _PYCCTEXT_H

#include "pyinstance_helpers.h"

#include <gui/components/cc.h>
#include <gui/scriptapi/scripttext.h>

/* The text a script made, held as the shared ScriptText: what is done with it
 * is the same for every interpreter, and this binding only fills its
 * parameters out of a keyword table. */
class CPyCCText : public ScriptText
{
	public:
		explicit CPyCCText(const params_t &pars) : ScriptText(pars) {}
};

class CPythonInstCCText
{
	public:
		CPythonInstCCText() {};
		~CPythonInstCCText() {};
		static CPythonInstCCText *getInstance();
		static bool CCTextRegister(PyObject *module);

	private:
		static PyObject *CCTextNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static PyObject *CCTextNewModule(PyObject *self, PyObject *args, PyObject *kwds);
		static void CCTextDelete(PyObject *self);

		static PyObject *CCTextPaint(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCTextHide(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCTextKill(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCTextSetText(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCTextGetLines(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCTextScroll(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCTextSetCenterPos(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCTextEnableUTF8(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCTextSetDimensionsAll(PyObject *self, PyObject *args, PyObject *kwds);
};

#endif //_PYCCTEXT_H
