/*
 * python hintbox
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

#include "py_hintbox.h"

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <neutrino.h>

#include "pyinstance.h"

typedef struct
{
	PyObject_HEAD
	CPyHintbox *d;
} PyHintboxObject;

static PyTypeObject *hintbox_type = NULL;

CPythonInstHintbox *CPythonInstHintbox::getInstance()
{
	static CPythonInstHintbox *PythonInstHintbox = NULL;

	if (!PythonInstHintbox)
		PythonInstHintbox = new CPythonInstHintbox();
	return PythonInstHintbox;
}

static CHintBox *hintBoxOf(PyObject *self)
{
	PyHintboxObject *o = (PyHintboxObject *)self;
	return (o->d != NULL) ? o->d->b : NULL;
}

/* hintbox.new(name="Title", text="text", icon="info", width=HINTBOX_MIN_WIDTH)
 *
 * The name is also read under the names the Lua binding takes it under,
 * title and caption, so a script carried over needs nothing changed here. */
PyObject *CPythonInstHintbox::HintboxNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		PyErr_SetString(PyExc_TypeError, "hintbox.new() takes keyword arguments only");
		return NULL;
	}

	std::string name, text, icon = std::string(NEUTRINO_ICON_INFO);
	pyLookupString(kwds, "name", name) || pyLookupString(kwds, "title", name) || pyLookupString(kwds, "caption", name);
	pyLookupString(kwds, "text", text);
	pyLookupString(kwds, "icon", icon);

	long width = HINTBOX_MIN_WIDTH;
	pyLookupInt(kwds, "width", width);

	PyHintboxObject *self = (PyHintboxObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		return NULL;
	}

	self->d = new CPyHintbox();
	self->d->caption = strdup(name.c_str());
	self->d->b = new CHintBox(self->d->caption, text.c_str(), (int)width, icon.c_str());
	return (PyObject *)self;
}

PyObject *CPythonInstHintbox::HintboxNewModule(PyObject *self, PyObject *args, PyObject *kwds)
{
	if (hintbox_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "hintbox is not registered");
		return NULL;
	}
	return HintboxNew(hintbox_type, args, kwds);
}

void CPythonInstHintbox::HintboxDelete(PyObject *self)
{
	PyHintboxObject *o = (PyHintboxObject *)self;
	delete o->d;
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

PyObject *CPythonInstHintbox::HintboxPaint(PyObject *self, PyObject *args, PyObject *kwds)
{
	CHintBox *b = hintBoxOf(self);
	if (b != NULL)
		b->paint();
	Py_RETURN_NONE;
}

PyObject *CPythonInstHintbox::HintboxHide(PyObject *self, PyObject *args, PyObject *kwds)
{
	CHintBox *b = hintBoxOf(self);
	if (b != NULL)
		b->hide();
	Py_RETURN_NONE;
}

/* b.exec(timeout=5): shows the box and waits for a key, with the loop the
 * Lua binding carries, which is the one gui/widget/hintbox.cpp runs. */
PyObject *CPythonInstHintbox::HintboxExec(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "timeout", NULL };
	int timeout = -1;
	CHintBox *b = hintBoxOf(self);

	if (b == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "hintbox is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|i", (char **)kwlist, &timeout))
		return NULL;

	CPythonInstance::scriptUiOpen();
	b->paint();

	neutrino_msg_t msg;
	neutrino_msg_data_t data;
	if (timeout == -1)
		timeout = 5; /* default timeout 5 sec */

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(timeout);
	int res = messages_return::none;

	while (!(res & (messages_return::cancel_info | messages_return::cancel_all)))
	{
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		if ((msg == CRCInput::RC_timeout) || (msg == CRCInput::RC_ok))
			res = messages_return::cancel_info;
		else if (CNeutrinoApp::getInstance()->backKey(msg))
			res = messages_return::cancel_all;
		else if ((msg == CRCInput::RC_up) || (msg == CRCInput::RC_down))
		{
			if (msg == CRCInput::RC_up)
				b->scroll_up();
			else
				b->scroll_down();
		}
		else if (CNeutrinoApp::getInstance()->listModeKey(msg))
		{
			// do nothing
		}
		else if (msg == CRCInput::RC_mode)
		{
			break;
		}
		else if ((msg == CRCInput::RC_next) || (msg == CRCInput::RC_prev))
		{
			res = messages_return::cancel_all;
			g_RCInput->postMsg(msg, data);
		}
		else
		{
			res = CNeutrinoApp::getInstance()->handleMsg(msg, data);
			if (res & messages_return::unhandled)
			{
				// leave here and handle above...
				g_RCInput->postMsg(msg, data);
				res = messages_return::cancel_all;
			}
		}
	}

	b->hide();
	CPythonInstance::scriptUiClose();
	Py_RETURN_NONE;
}


bool CPythonInstHintbox::HintboxRegister(PyObject *module)
{
	static PyMethodDef hintbox_methods[] =
	{
		PY_METHODDEF("exec",  CPythonInstHintbox::HintboxExec),
		PY_METHODDEF("paint", CPythonInstHintbox::HintboxPaint),
		PY_METHODDEF("hide",  CPythonInstHintbox::HintboxHide),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("new", CPythonInstHintbox::HintboxNewModule),
		{ NULL, NULL, 0, NULL }
	};

	static PyType_Slot hintbox_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstHintbox::HintboxNew },
		{ Py_tp_dealloc, (void *)CPythonInstHintbox::HintboxDelete },
		{ Py_tp_methods, (void *)hintbox_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"hintbox" },
		{ 0, NULL }
	};

	static PyType_Spec hintbox_spec =
	{
		"neutrino.hintbox.hintbox",
		sizeof(PyHintboxObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		hintbox_slots
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.hintbox",
		"hintbox",
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

	hintbox_type = (PyTypeObject *)PyType_FromSpec(&hintbox_spec);
	if (hintbox_type == NULL)
	{
		PyErr_Clear();
		Py_DECREF(sub);
		return false;
	}
	if (PyModule_AddObject(sub, "hintbox", (PyObject *)hintbox_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)hintbox_type);
		hintbox_type = NULL;
		Py_DECREF(sub);
		return false;
	}

	return PythonInstAddSubmodule(module, "hintbox", sub);
}

