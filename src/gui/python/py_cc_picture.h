/*
 * python components picture
 *
 * The Lua binding in lua_cc_picture.cpp, read as
 *
 *	from neutrino import cwindow, cpicture
 *	w = cwindow.new(name="Title")
 *	p = cpicture.new(parent=w, image="/tmp/pic.png", dx=100, dy=80)
 *	p.paint()
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

#ifndef _PYCCPICTURE_H
#define _PYCCPICTURE_H

#include "pyinstance_helpers.h"

#include <gui/components/cc.h>

#include <gui/scriptapi/scriptpicture.h>

/* The picture a script made, held as the shared ScriptPicture: what is done
 * with it is the same for every interpreter, and this binding only fills its
 * parameters out of a keyword table. */
class CPyCCPicture : public ScriptPicture
{
	public:
		explicit CPyCCPicture(const params_t &pars) : ScriptPicture(pars) {}
};

class CPythonInstCCPicture
{
	public:
		CPythonInstCCPicture() {};
		~CPythonInstCCPicture() {};
		static CPythonInstCCPicture *getInstance();
		static bool CCPictureRegister(PyObject *module);

	private:
		static PyObject *CCPictureNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static PyObject *CCPictureNewModule(PyObject *self, PyObject *args, PyObject *kwds);
		static void CCPictureDelete(PyObject *self);

		static PyObject *CCPicturePaint(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCPictureHide(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCPictureSetPicture(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCPictureSetCenterPos(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCPictureGetHeight(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCPictureGetWidth(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CCPictureSetDoPaintBG(PyObject *self, PyObject *args, PyObject *kwds);
};

#endif //_PYCCPICTURE_H
