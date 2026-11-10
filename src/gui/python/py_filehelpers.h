/*
 * python file helpers functions
 *
 * The Lua binding in lua_filehelpers.cpp, read as
 *
 *	from neutrino import filehelpers
 *	f = filehelpers.new()
 *	if f.exist("/tmp/x", "f"):
 *		f.rmdir("/tmp/x")
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

#ifndef _PYFILEHELPERS_H
#define _PYFILEHELPERS_H

#include "pyinstance_helpers.h"

class CPyFileHelpers
{
	public:
		CPyFileHelpers() {};
		~CPyFileHelpers() {};
};

class CPythonInstFileHelpers
{
	public:
		CPythonInstFileHelpers() {};
		~CPythonInstFileHelpers() {};
		static CPythonInstFileHelpers *getInstance();
		static bool FileHelpersRegister(PyObject *module);

	private:
		static PyObject *FileHelpersNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static PyObject *FileHelpersNewModule(PyObject *self, PyObject *args, PyObject *kwds);
		static void FileHelpersDelete(PyObject *self);

		static PyObject *FileHelpersCp(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *FileHelpersChmod(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *FileHelpersTouch(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *FileHelpersRmdir(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *FileHelpersMkdir(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *FileHelpersReadlink(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *FileHelpersLn(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *FileHelpersExist(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *FileHelpersIsEmpty(PyObject *self, PyObject *args, PyObject *kwds);
};

#endif //_PYFILEHELPERS_H
