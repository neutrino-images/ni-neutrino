/*
 * python hourglass/loader
 *
 * The Lua binding in lua_hourglass.cpp, read as
 *
 *	from neutrino import hourglass
 *	h = hourglass.new(x=20, y=20, dx=48, dy=48)
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

#ifndef _PYHOURGLASS_H_
#define _PYHOURGLASS_H_

#include "pyinstance_helpers.h"

#include <gui/widget/hourglass.h>

#include <gui/scriptapi/script_hourglass.h>

/* The hourglass a script made, held as the shared ScriptHourglass: what is done
 * with it is the same for every interpreter, and this binding only fills its
 * parameters out of a keyword table. */
class CPyHourGlass : public ScriptHourglass
{
	public:
		explicit CPyHourGlass(const params_t &pars) : ScriptHourglass(pars) {}
};

class CPythonInstHourGlass
{
	public:
		CPythonInstHourGlass() {};
		~CPythonInstHourGlass() {};
		static CPythonInstHourGlass *getInstance();
		static bool HourGlassRegister(PyObject *module);

	private:
		static PyObject *HourGlassNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static PyObject *HourGlassNewModule(PyObject *self, PyObject *args, PyObject *kwds);
		static void HourGlassDelete(PyObject *self);
		static PyObject *HourGlassPaint(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *HourGlassHide(PyObject *self, PyObject *args, PyObject *kwds);
};

#endif //_PYHOURGLASS_H_
