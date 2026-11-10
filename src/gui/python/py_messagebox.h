/*
 * python messagebox
 *
 * The Lua binding in lua_messagebox.cpp, read as
 *
 *	from neutrino import messagebox
 *	res = messagebox.exec(name="Title", text="text", buttons=["yes", "no"])
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

#ifndef _PYMESSAGEBOX_H
#define _PYMESSAGEBOX_H

#include "pyinstance_helpers.h"

class CPythonInstMessagebox
{
	public:
		CPythonInstMessagebox() {};
		~CPythonInstMessagebox() {};
		static CPythonInstMessagebox *getInstance();
		static bool MessageboxRegister(PyObject *module);

	private:
		static PyObject *MessageboxExec(PyObject *self, PyObject *args, PyObject *kwds);
};

#endif //_PYMESSAGEBOX_H
