/*
 * python config file
 *
 * The Lua binding in lua_configfile.cpp, read as
 *
 *	from neutrino import configfile
 *	c = configfile.new()
 *	c.loadConfig("/tmp/test.conf")
 *	value = c.getString("key", "default")
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

#ifndef _PYCONFIGFILE_H
#define _PYCONFIGFILE_H

#include "pyinstance_helpers.h"

#include <configfile.h>

class CPyConfigFile
{
	public:
		CConfigFile *c;
		CPyConfigFile() { c = NULL; }
		~CPyConfigFile() { delete c; }
};

class CPythonInstConfigFile
{
	public:
		CPythonInstConfigFile() {};
		~CPythonInstConfigFile() {};
		static CPythonInstConfigFile *getInstance();
		static bool ConfigFileRegister(PyObject *module);

	private:
		static PyObject *ConfigFileNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static PyObject *ConfigFileNewModule(PyObject *self, PyObject *args, PyObject *kwds);
		static void ConfigFileDelete(PyObject *self);

		static PyObject *LoadConfig(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *SaveConfig(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *Clear(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *GetString(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *SetString(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *GetInt32(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *SetInt32(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *GetBool(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *SetBool(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *DeleteKey(PyObject *self, PyObject *args, PyObject *kwds);
};

#endif //_PYCONFIGFILE_H
