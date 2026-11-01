/*
 * python to c++ bridge
 *
 * The bindings in this directory are the ones in src/gui/lua written for
 * Python: the same objects under the same names, reached as attributes of
 * the module a script imports, where a Lua script reached them as globals.
 * What a Lua script writes as
 *
 *	misc = misc.new()
 *	r = misc:getVolume()
 *
 * a Python script writes as
 *
 *	from neutrino import misc
 *	m = misc.new()
 *	r = m.getVolume()
 *
 * and the constants a Lua script reads as COL['WHITE'] or DIR['CONFIGDIR']
 * are dictionaries of the same names, so the same spelling works.
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
#ifndef _PYINSTANCE_H
#define _PYINSTANCE_H

/* Python.h has to be read before the standard headers: it sets the feature
 * macros those are then read under. */
#include "pyinstance_helpers.h"

#include <string>
#include <vector>

/* Fills in the module a script imports, with the constants and with every
 * binding this directory holds. */
void PythonInstRegisterFunctions(PyObject *module);

/* The module a script imports, started on the first script that is run.
 * Borrowed: it lives as long as the program does. */
PyObject *PythonInstModule(void);

/* Whether the interpreter is up. A binding that needs to raise a Python
 * error has to ask before the first script has run, and answer in C
 * instead while there is nobody to raise it to. */
bool PythonInstReady(void);

/* Puts a submodule on the module a script imports, under its own name and
 * under the dotted name the import machinery looks up, so that both
 * "from neutrino import misc" and "import neutrino.misc" find it. */
bool PythonInstAddSubmodule(PyObject *parent, const char *name, PyObject *sub);

class CPythonInstance
{
		static const char className[];
	public:
		CPythonInstance();
		~CPythonInstance();
		void runScript(const char *fileName, std::vector<std::string> *argv = NULL, std::string *result_code = NULL, std::string *result_string = NULL, std::string *error_string = NULL);
		void abortScript();
		bool DisableAbort;
		enum
		{
			DYNFONT_NO_ERROR      = 0,
			DYNFONT_MAXIMUM_FONTS = 1,
			DYNFONT_TOO_WIDE      = 2,
			DYNFONT_TOO_HIGH      = 3
		};

		// Example: runScript(fileName, "Arg1", "Arg2", "Arg3", ..., NULL);
		//	Type of all parameters: const char*
		//	The last parameter to NULL is imperative.
		void runScript(const char *fileName, const char *arg0, ...);

		/* A script may open its own OSD (menu, messagebox, hintbox) while
		 * the caller of runScript() still shows a hint of its own, such as
		 * the "reading data" box of a WebTV resolve. The caller registers a
		 * listener on its own thread; the bindings report the outermost
		 * open/close transition (nested dialogs are counted), and the
		 * callback runs only on the registering thread, so a script started
		 * elsewhere in the meantime cannot touch the caller's objects. */
		static void setScriptUiListener(void (*cb)(bool open, void *data), void *data);
		static void scriptUiOpen();
		static void scriptUiClose();

	private:
		/* The namespace the script of this instance runs in, so that two
		 * scripts run one after the other do not see each other's names,
		 * as two Lua states do not. */
		PyObject *globals;

		void reportError(const char *fileName, std::string *error_string);
};

#endif /* _PYINSTANCE_H */
