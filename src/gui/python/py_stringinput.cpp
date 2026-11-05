/*
 * python stringinput
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

#include "py_stringinput.h"

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <gui/widget/stringinput.h>
#include <neutrino.h>

#include "pyinstance.h"

CPythonInstStringInput *CPythonInstStringInput::getInstance()
{
	static CPythonInstStringInput *PythonInstStringInput = NULL;

	if (!PythonInstStringInput)
		PythonInstStringInput = new CPythonInstStringInput();
	return PythonInstStringInput;
}

/* stringinput.exec(caption="Title", value="value", size=30,
 *	valid_chars="...", icon="info", sms=False, pin=False)
 *
 * The keys are the ones the Lua binding reads out of its table. */
PyObject *CPythonInstStringInput::StringInputExec(PyObject *self, PyObject *args, PyObject *kwds)
{
	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		PyErr_SetString(PyExc_TypeError, "stringinput.exec() takes keyword arguments only");
		return NULL;
	}

	std::string name;
	pyLookupString(kwds, "name", name) || pyLookupString(kwds, "title", name) || pyLookupString(kwds, "caption", name);

	std::string value;
	pyLookupString(kwds, "value", value);

	long size = 30;
	pyLookupInt(kwds, "size", size);

	std::string valid_chars = "abcdefghijklmnopqrstuvwxyz0123456789!\"$%&/()=?-.@,_: ";
	pyLookupString(kwds, "valid_chars", valid_chars);

	std::string icon = std::string(NEUTRINO_ICON_INFO);
	pyLookupString(kwds, "icon", icon);

	bool sms = false, pin = false;
	pyLookupBool(kwds, "sms", sms);
	pyLookupBool(kwds, "pin", pin);

	if (sms && pin)
		dprintf(DEBUG_NORMAL, "[CPythonInstance][%s - %d]: 'sms' AND 'pin' is defined! 'pin' will be prefered.\n", __func__, __LINE__);

	/* The PIN input is a dialog of its own now, no string input, and the
	 * sms multi-tap input is gone with it: the dialog brings an on-screen
	 * keyboard for letters instead, so "sms" is read and does nothing. */
	CMenuTarget *i;
	if (pin)
		i = new CPINInput(name, &value, (int)size,
				NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, valid_chars.c_str(), NULL);
	else
		i = new CStringInput(name, &value, (int)size,
				NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, valid_chars.c_str(), NULL, icon.c_str());
	i->exec(NULL, "");
	delete i;

	return PyUnicode_FromStringAndSize(value.c_str(), (Py_ssize_t)value.length());
}

bool CPythonInstStringInput::StringInputRegister(PyObject *module)
{
	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("exec", CPythonInstStringInput::StringInputExec),
		{ NULL, NULL, 0, NULL }
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.stringinput",
		"stringinput",
		-1,
		module_methods,
		NULL, NULL, NULL, NULL
	};

	PyObject *sub = PyModule_Create(&module_def);
	if (sub == NULL)
	{
		PyErr_Clear();
		return false;
	}

	return PythonInstAddSubmodule(module, "stringinput", sub);
}
