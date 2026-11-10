/*
 * python neutrino window
 *
 * The class the Lua bindings in luainstance.cpp answer for, read as
 *
 *	from neutrino import new as window, FONT, COL, RC
 *	w = window()
 *	w.PaintBox(0, 0, 200, 100, COL["MENUCONTENT_PLUS_0"])
 *	w.RenderString(FONT["MENU"], "hello", 10, 10)
 *	msg, data = w.GetInput(500)
 *	del w
 *
 * The call shape is the one the Lua bindings take, including the boolean
 * in front of a font, which says the number that follows names a dynamic
 * font of this window instead of one of the box's own.
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

#ifndef _PYNEUTRINO_H
#define _PYNEUTRINO_H

#include "pyinstance_helpers.h"

#include <map>
#include <string>

#include <driver/fb_generic.h>
#include <driver/fb_window.h>

#include "../scriptapi/script_neutrino.h"

class CRCInput;
class Font;

typedef std::pair<long, Font *> pyfontmap_pair_t;
typedef std::map<long, Font *> pyfontmap_t;
typedef pyfontmap_t::iterator pyfontmap_iterator_t;

typedef std::pair<long, fb_pixel_t *> pyscreenmap_pair_t;
typedef std::map<long, fb_pixel_t *> pyscreenmap_t;
typedef pyscreenmap_t::iterator pyscreenmap_iterator_t;

/* What a window of the API holds: the framebuffer window it draws into,
 * the input it reads, and the dynamic fonts and saved screens it owns. */
class CPyWindowData
{
	public:
		CFBWindow *fbwin;
		CRCInput *rcinput;
		pyfontmap_t fontmap;
		pyscreenmap_t screenmap;

		CPyWindowData() { fbwin = NULL; rcinput = NULL; }
};

class CPythonInstNeutrino
{
	public:
		CPythonInstNeutrino() {};
		~CPythonInstNeutrino() {};
		static CPythonInstNeutrino *getInstance();
		/* Puts the class on the module a script imports, under the name
		 * 'window', and a function 'new' beside it, so that both
		 * neutrino.new() and neutrino.window() answer. */
		static bool NeutrinoRegister(PyObject *module);

	private:
		static PyObject *WindowNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static PyObject *WindowNewModule(PyObject *self, PyObject *args, PyObject *kwds);
		static void WindowDelete(PyObject *self);

		static PyObject *GetInput(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *Blit(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *GetLanguage(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *PaintBox(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *PaintHLineRel(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *PaintVLineRel(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *RenderString(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *GetRenderWidth(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *FontHeight(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *GetDynFont(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *PaintIcon(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *DisplayImage(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *GetSize(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *SaveScreen(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *RestoreScreen(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *DeleteSavedScreen(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *Scale2Res(PyObject *self, PyObject *args, PyObject *kwds);

		/* The font a call names: a number into the box's own fonts, or one
		 * of this window's dynamic fonts where the call said so. */
		static Font *fontFromArgs(PyObject *self, PyObject *args, Py_ssize_t index,
				bool isDynFont, bool &found);
};

#endif //_PYNEUTRINO_H
