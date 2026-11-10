/*
 * python messagebox
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

#include "py_messagebox.h"

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <gui/widget/msgbox.h>
#include <neutrino.h>

#include "pyinstance.h"
#include <scriptapi/script_messagebox.h>

CPythonInstMessagebox *CPythonInstMessagebox::getInstance()
{
	static CPythonInstMessagebox *PythonInstMessagebox = NULL;

	if (!PythonInstMessagebox)
		PythonInstMessagebox = new CPythonInstMessagebox();
	return PythonInstMessagebox;
}

/* messagebox.exec(name="Title", text="text", icon="settings", width=500,
 *	timeout=-1, return_default_on_timeout=False, default="yes",
 *	buttons=["yes", "no", "cancel", "all", "back", "ok"],
 *	align="center1|center2|left|right")
 *
 * The keys are the ones the Lua binding reads out of its table, so a
 * script that is carried over needs the quotes around the names changed
 * and nothing else. */
PyObject *CPythonInstMessagebox::MessageboxExec(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		PyErr_SetString(PyExc_TypeError, "messagebox.exec() takes keyword arguments only");
		return NULL;
	}

	ScriptMessagebox::params_t pars;
	pars.icon = std::string(NEUTRINO_ICON_INFO);
	pars.width = MSGBOX_MIN_WIDTH;
	pars.timeout = -1;
	pars.return_default_on_timeout = 0;

	long timeout = pars.timeout, width = pars.width, return_default_on_timeout = pars.return_default_on_timeout;
	pyLookupString(kwds, "name", pars.name) || pyLookupString(kwds, "title", pars.name) || pyLookupString(kwds, "caption", pars.name);
	pyLookupString(kwds, "text", pars.text);
	pyLookupString(kwds, "icon", pars.icon);
	pyLookupInt(kwds, "timeout", timeout);
	pyLookupInt(kwds, "width", width);
	pyLookupInt(kwds, "return_default_on_timeout", return_default_on_timeout);
	pyLookupString(kwds, "text_mode", pars.text_mode);
	pyLookupString(kwds, "align", pars.align);
	pyLookupString(kwds, "default", pars.default_button);
	pars.timeout = (int)timeout;
	pars.width = (int)width;
	pars.return_default_on_timeout = (int)return_default_on_timeout;

	PyObject *buttons = NULL;
	if (pyLookupList(kwds, "buttons", &buttons))
	{
		Py_ssize_t count = PySequence_Size(buttons);
		for (Py_ssize_t i = 0; i < count; i++)
		{
			PyObject *item = PySequence_GetItem(buttons, i);
			if (item == NULL)
			{
				PyErr_Clear();
				continue;
			}
			const char *val = PyUnicode_Check(item) ? PyUnicode_AsUTF8(item) : NULL;
			if (val == NULL)
				PyErr_Clear();
			else
				pars.buttons.push_back(val);
			Py_DECREF(item);
		}
	}

	/* the words of the buttons, of the alignment and of the text mode are
	 * the script api's; this binding reads them and hands them over */
	CPythonInstance::scriptUiOpen();
	std::string answer = ScriptMessagebox::exec(pars);
	CPythonInstance::scriptUiClose();

	return PyUnicode_FromString(answer.c_str());
}

bool CPythonInstMessagebox::MessageboxRegister(PyObject *module)
{
	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("exec", CPythonInstMessagebox::MessageboxExec),
		{ NULL, NULL, 0, NULL }
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.messagebox",
		"messagebox",
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

	return PythonInstAddSubmodule(module, "messagebox", sub);
}

