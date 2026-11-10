/*
 * python stringinput
 *
 * The Lua binding in lua_stringinput.cpp, read as
 *
 *	from neutrino import stringinput
 *	value = stringinput.exec(caption="Title", value="", size=4)
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

#ifndef _PYSTRINGINPUT_H
#define _PYSTRINGINPUT_H

#include "pyinstance_helpers.h"

class CPythonInstStringInput
{
	public:
		CPythonInstStringInput() {};
		~CPythonInstStringInput() {};
		static CPythonInstStringInput *getInstance();
		static bool StringInputRegister(PyObject *module);

	private:
		static PyObject *StringInputExec(PyObject *self, PyObject *args, PyObject *kwds);
};

#endif //_PYSTRINGINPUT_H
