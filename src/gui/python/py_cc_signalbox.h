/*
 * python components signalbox
 *
 * The Lua binding in lua_cc_signalbox.cpp, read as
 *
 *	from neutrino import cwindow, signalbox
 *	w = cwindow.new(name="Title")
 *	s = signalbox.new(parent=w)
 *	s.paint()
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

#ifndef _PYCCSIGNALBOX_H
#define _PYCCSIGNALBOX_H

#include "pyinstance_helpers.h"

#include <gui/components/cc.h>

#include <gui/scriptapi/script_signalbox.h>

/* The box a script made, held as the shared ScriptSignalbox: what is done with
 * it is the same for every interpreter, and this binding only fills its
 * parameters out of a keyword table. */
class CPyCCSignalBox : public ScriptSignalbox
{
	public:
		explicit CPyCCSignalBox(const params_t &pars) : ScriptSignalbox(pars) {}
};

class CPythonInstCCSignalbox
{
	public:
		CPythonInstCCSignalbox() {};
		~CPythonInstCCSignalbox() {};
		static CPythonInstCCSignalbox *getInstance();
		static bool CCSignalBoxRegister(PyObject *module);

	private:
		static PyObject *CCSignalBoxNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static PyObject *CCSignalBoxNewModule(PyObject *self, PyObject *args, PyObject *kwds);
		static void CCSignalBoxDelete(PyObject *self);
		static PyObject *CCSignalBoxPaint(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCSignalBoxSetCenterPos(PyObject *self, PyObject *args, PyObject *kwds);
};

#endif //_PYCCSIGNALBOX_H
