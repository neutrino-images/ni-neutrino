/*
 * python misc functions
 *
 * The functions the Lua bindings in lua_misc.cpp answer, under the same
 * names and with the same arguments, reached as
 *
 *	from neutrino import misc
 *	m = misc.new()
 *	m.enableInfoClock(True)
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

#ifndef _PYMISCFUNCS_H
#define _PYMISCFUNCS_H

#include "pyinstance_helpers.h"

class CPyMisc
{
	public:
		CPyMisc() {};
		~CPyMisc() {};
};

class CPythonInstMisc
{
	public:
		enum {
			POSTMSG_STANDBY_ON = 1
		};

		CPythonInstMisc() {};
		~CPythonInstMisc() {};
		static CPythonInstMisc *getInstance();
		static bool MiscRegister(PyObject *module);

	private:
		static PyObject *MiscNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static void MiscDelete(PyObject *self);

		static PyObject *strFind(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *strSub(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *enableInfoClock(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *enableMuteIcon(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *setVolume(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *getVolume(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *AudioMute(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *isMuted(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *runScript(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *GetRevision(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *checkVersion(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *postMsg(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *getTimeOfDay(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *FlashUpdate(PyObject *self, PyObject *args, PyObject *kwds); //NI
};

#endif //_PYMISCFUNCS_H
