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
PyObject *CPythonInstMessagebox::MessageboxExec(PyObject *self, PyObject *args, PyObject *kwds)
{
	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		PyErr_SetString(PyExc_TypeError, "messagebox.exec() takes keyword arguments only");
		return NULL;
	}

	std::string name, text, icon = std::string(NEUTRINO_ICON_INFO), txtMode = "";
	pyLookupString(kwds, "name", name) || pyLookupString(kwds, "title", name) || pyLookupString(kwds, "caption", name);
	pyLookupString(kwds, "text", text);
	pyLookupString(kwds, "icon", icon);

	long timeout = -1, width = MSGBOX_MIN_WIDTH, return_default_on_timeout = 0;
	long show_buttons = 0, default_button = 0, text_mode = 0;
	pyLookupInt(kwds, "timeout", timeout);
	pyLookupInt(kwds, "width", width);
	pyLookupInt(kwds, "return_default_on_timeout", return_default_on_timeout);
	pyLookupString(kwds, "text_mode", txtMode);

	std::string tmp;
	if (pyLookupString(kwds, "align", tmp))
	{
		static const struct { const char *name; long code; } mb[] = {
			{ "center1", CMsgBox::mbBtnAlignCenter1 },
			{ "center2", CMsgBox::mbBtnAlignCenter2 },
			{ "left",    CMsgBox::mbBtnAlignLeft },
			{ "right",   CMsgBox::mbBtnAlignRight },
			{ NULL, 0 }
		};
		for (int i = 0; mb[i].name; i++)
			if (!strcmp(mb[i].name, tmp.c_str()))
			{
				show_buttons |= mb[i].code;
				break;
			}
	}

	PyObject *buttons = NULL;
	if (pyLookupList(kwds, "buttons", &buttons))
	{
		static const struct { const char *name; long code; } mb[] = {
			{ "yes",    CMsgBox::mbYes },
			{ "no",     CMsgBox::mbNo },
			{ "cancel", CMsgBox::mbCancel },
			{ "all",    CMsgBox::mbAll },
			{ "back",   CMsgBox::mbBack },
			{ "ok",     CMsgBox::mbOk },
			{ NULL, 0 }
		};

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
				for (int j = 0; mb[j].name; j++)
					if (!strcmp(mb[j].name, val))
					{
						show_buttons |= mb[j].code;
						break;
					}
			Py_DECREF(item);
		}
	}
	if ((show_buttons & 0xFF) == 0)
		show_buttons |= CMsgBox::mbAll;

	static const struct { const char *name; long code; } mbr[] = {
		{ "yes",    CMsgBox::mbrYes },
		{ "no",     CMsgBox::mbrNo },
		{ "cancel", CMsgBox::mbrCancel },
		{ "back",   CMsgBox::mbrBack },
		{ "ok",     CMsgBox::mbrOk },
		{ NULL, 0 }
	};
	if (pyLookupString(kwds, "default", tmp))
	{
		for (int i = 0; mbr[i].name; i++)
			if (!strcmp(mbr[i].name, tmp.c_str()))
			{
				default_button = mbr[i].code;
				break;
			}
	}

	if (!txtMode.empty())
	{
		static const struct { const char *name; long code; } txt_align[] = {
			{ "ALIGN_AUTO_WIDTH",        CTextBox::AUTO_WIDTH },
			{ "ALIGN_AUTO_HIGH",         CTextBox::AUTO_HIGH },
			{ "ALIGN_SCROLL",            CTextBox::SCROLL },
			{ "ALIGN_CENTER",            CTextBox::CENTER },
			{ "ALIGN_RIGHT",             CTextBox::RIGHT },
			{ "ALIGN_TOP",               CTextBox::TOP },
			{ "ALIGN_BOTTOM",            CTextBox::BOTTOM },
			{ "ALIGN_NO_AUTO_LINEBREAK", CTextBox::NO_AUTO_LINEBREAK },
			{ "DECODE_HTML",             0 },
			{ NULL, 0 }
		};
		for (int i = 0; txt_align[i].name; i++)
			if (txtMode.find(txt_align[i].name) != std::string::npos)
				text_mode |= txt_align[i].code;
		if (txtMode.find("DECODE_HTML") != std::string::npos)
			htmlEntityDecode(text);
	}
	else
		text_mode = DEFAULT_MSGBOX_TEXT_MODE;

	CPythonInstance::scriptUiOpen();
	int res = ShowMsg(name, text, (CMsgBox::msg_result_t)default_button,
			(CMsgBox::button_define_t)show_buttons,
			icon.empty() ? NULL : icon.c_str(),
			(int)width, (int)timeout, (int)return_default_on_timeout, (int)text_mode);
	CPythonInstance::scriptUiClose();

	tmp = "cancel";
	for (int i = 0; mbr[i].name; i++)
		if (res == mbr[i].code)
		{
			tmp = mbr[i].name;
			break;
		}

	return PyUnicode_FromString(tmp.c_str());
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

