/*
 * python simple hint window
 *
 * The Lua binding in lua_hint.cpp, read as
 *
 *	from neutrino import hint
 *	h = hint.new(text="text")
 *	h.paint()
 *	h.hide()
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

#ifndef _PYHINT_H_
#define _PYHINT_H_

#include "pyinstance_helpers.h"

#include <gui/widget/hintbox.h>
#include <gui/scriptapi/scripthint.h>

/* The hint a script made, held as the shared ScriptHint: what is done with it
 * is the same for every interpreter, and this binding only fills its
 * parameters out of a keyword table. */
class CPyHint : public ScriptHint
{
	public:
		explicit CPyHint(const params_t &pars) : ScriptHint(pars) {}
};

class CPythonInstHint
{
	public:
		CPythonInstHint() {};
		~CPythonInstHint() {};
		static CPythonInstHint *getInstance();
		static bool HintRegister(PyObject *module);

	private:
		static PyObject *HintNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static PyObject *HintNewModule(PyObject *self, PyObject *args, PyObject *kwds);
		static void HintDelete(PyObject *self);
		static PyObject *HintPaint(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *HintHide(PyObject *self, PyObject *args, PyObject *kwds);
};

#endif //_PYHINT_H_
