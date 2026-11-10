/*
 * python menu
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

#include "py_menue.h"

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <gui/widget/menue.h>
#include <gui/widget/keyboard_input.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
#include <gui/filebrowser.h>
#include <gui/components/cc.h>
#include <driver/rcinput.h>
#include <neutrino.h>

#include "pyinstance.h"
#include <scriptapi/script_stringinput.h>

typedef struct
{
	PyObject_HEAD
	CPyMenu *d;
} PyMenuObject;

static PyTypeObject *menu_type = NULL;

CPythonInstMenu *CPythonInstMenu::getInstance()
{
	static CPythonInstMenu *PythonInstMenu = NULL;

	if (!PythonInstMenu)
		PythonInstMenu = new CPythonInstMenu();
	return PythonInstMenu;
}

static CPyMenu *menuOf(PyObject *self)
{
	PyMenuObject *o = (PyMenuObject *)self;
	return o->d;
}

CPyMenu::CPyMenu(const params_t &pars) : ScriptMenu(pars)
{
}

CPyMenu::~CPyMenu()
{
	/* The widget and the items are the script API's and go with it. The rows
	 * that call the script, and what they are told of a chooser, are this
	 * binding's and are let go of here. */
	while (!targets.empty())
	{
		delete targets.back();
		targets.pop_back();
	}
	while (!observers.empty())
	{
		delete observers.back();
		observers.pop_back();
	}
}

/* What a row of this menu hands on when it is picked, and what a chooser tells
 * the script. The menu is its own factory, so a row is made where the rows are
 * kept, and is let go of with them. The callable a row calls is the script's,
 * and is told the value the dialog answered where it has one of its own. */
CMenuTarget *CPyMenu::forwarder(const ScriptMenuItemParams &pars)
{
	const py_item_params &pp = (const py_item_params &)pars;
	CPyMenuTarget *forwarder = new CPyMenuTarget(pp.action, pars.id);
	targets.push_back(forwarder);
	return forwarder;
}

CMenuTarget *CPyMenu::stringinput(const ScriptMenuItemParams &pars, ScriptMenuItem *item, const char * /*icon*/)
{
	const py_item_params &pp = (const py_item_params &)pars;
	/* "sms" asked for the multi-tap letters on the digit keys; the input
	 * dialog brings an on-screen keyboard for letters instead. */
	CPyMenuStringinput *stringinput = new CPyMenuStringinput(pp.action, pars.id, item->name,
			&item->str_val, pars.size, pars.valid_chars, pars.icon, pars.sms);
	targets.push_back(stringinput);
	return stringinput;
}

CMenuTarget *CPyMenu::keyboardinput(const ScriptMenuItemParams &pars, ScriptMenuItem *item, const char * /*icon*/)
{
	const py_item_params &pp = (const py_item_params &)pars;
	CPyMenuKeyboardinput *keyboardinput = new CPyMenuKeyboardinput(pp.action, pars.id, item->name,
			&item->str_val, pars.size, pars.icon, pars.help, pars.help2, pars.masked);
	targets.push_back(keyboardinput);
	return keyboardinput;
}

CMenuTarget *CPyMenu::filebrowser(const ScriptMenuItemParams &pars, ScriptMenuItem *item)
{
	const py_item_params &pp = (const py_item_params &)pars;
	CPyMenuFilebrowser *filebrowser = new CPyMenuFilebrowser(pp.action, pars.id, &item->str_val, pars.dir_mode);
	for (unsigned int i = 0; i < pp.filters.size(); i++)
		filebrowser->addFilter(pp.filters[i]);
	targets.push_back(filebrowser);
	return filebrowser;
}

/* A masked entry is told which row shows it, so that it can say after an input
 * whether a value is set. Only this binding's keyboardinput has that. */
void CPyMenu::setMaskedRow(CMenuTarget *target, CMenuForwarder *row)
{
	((CPyMenuKeyboardinput *)target)->setRow(row);
}

/* A chooser, a numeric and a string tell the script what they were changed to,
 * each through its own observer, since what the script is told of is the item's
 * and not the menu's. */
CChangeObserver *CPyMenu::observer(const ScriptMenuItemParams &pars)
{
	const py_item_params &pp = (const py_item_params &)pars;
	CPyMenuObserver *observ = new CPyMenuObserver(pp.action, pars.id);
	observers.push_back(observ);
	return observ;
}

/* menu.new(name="", title="", icon="", mwidth=..., btnRed=..., btnGreen=...,
 *	btnYellow=..., btnBlue=...) */
PyObject *CPythonInstMenu::MenuNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		PyErr_SetString(PyExc_TypeError, "menu.new() takes keyword arguments only");
		return NULL;
	}

	/* The parameters a script names in its call. What is done with them is
	 * done by the script API, so that both bindings ask for one menu and
	 * cannot come to differ. */
	ScriptMenu::params_t pars;
	pars.width_given = false;
	pars.width = 0;

	pyLookupString(kwds, "name", pars.name) || pyLookupString(kwds, "title", pars.name);
	pyLookupString(kwds, "icon", pars.icon);

	long mwidth = 0;
	if (pyLookupInt(kwds, "mwidth", mwidth))
	{
		pars.width_given = true;
		pars.width = (int)mwidth;
	}

	/* The footer's buttons, each with the icon of the key it belongs to, in
	 * the order the footer shows them. */
	static const struct { const char *key; const char *icon; } btn_keys[] = {
		{ "btnRed",    NEUTRINO_ICON_BUTTON_RED },
		{ "btnGreen",  NEUTRINO_ICON_BUTTON_GREEN },
		{ "btnYellow", NEUTRINO_ICON_BUTTON_YELLOW },
		{ "btnBlue",   NEUTRINO_ICON_BUTTON_BLUE },
		{ NULL, NULL }
	};

	for (int i = 0; btn_keys[i].key != NULL; i++)
	{
		std::string label = "";
		if (!pyLookupString(kwds, btn_keys[i].key, label) || label.empty())
			continue;
		button_label_cc b;
		b.button = btn_keys[i].icon;
		b.text = label;
		pars.buttons.push_back(b);
	}

	PyMenuObject *self = (PyMenuObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		return NULL;
	}
	self->d = new CPyMenu(pars);
	return (PyObject *)self;
}

PyObject *CPythonInstMenu::MenuNewModule(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
	if (menu_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "menu is not registered");
		return NULL;
	}
	return MenuNew(menu_type, args, kwds);
}

void CPythonInstMenu::MenuDelete(PyObject *self)
{
	PyMenuObject *o = (PyMenuObject *)self;
	delete o->d;
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

/* Calls the script's callable with the id, and with the value where one is
 * handed on, and answers the menu return code it gives. */
int CPyMenuTarget::callScript(PyObject *value)
{
	int res = menu_return::RETURN_REPAINT;
	if (action == NULL || action == Py_None)
		return res;

	PyGILState_STATE gstate = PyGILState_Ensure();

	PyObject *id_obj = PyUnicode_FromString(id.c_str());
	PyObject *result = NULL;
	if (id_obj != NULL)
	{
		if (value != NULL)
			result = PyObject_CallFunctionObjArgs(action, id_obj, value, NULL);
		else
			result = PyObject_CallFunctionObjArgs(action, id_obj, NULL);
		Py_DECREF(id_obj);
	}

	if (result == NULL)
	{
		PyObject *type = NULL, *val = NULL, *tb = NULL;
		PyErr_Fetch(&type, &val, &tb);
		PyErr_NormalizeException(&type, &val, &tb);

		std::string what = "NULL";
		if (val != NULL)
		{
			PyObject *str = PyObject_Str(val);
			if (str != NULL)
			{
				const char *s = PyUnicode_AsUTF8(str);
				if (s != NULL)
					what = s;
				else
					PyErr_Clear();
				Py_DECREF(str);
			}
			else
				PyErr_Clear();
		}

		fprintf(stderr, "[CPyMenuTarget::%s] error in script: %s\n", __func__, what.c_str());
		DisplayErrorMessage(what.c_str(), "Python Script Error:");

		Py_XDECREF(type);
		Py_XDECREF(val);
		Py_XDECREF(tb);
		PyGILState_Release(gstate);
		return res;
	}

	long code = PyLong_AsLong(result);
	if (code == -1 && PyErr_Occurred())
		PyErr_Clear();
	else
		res = (int)code;
	Py_DECREF(result);

	PyGILState_Release(gstate);
	return res;
}

int CPyMenuTarget::exec(CMenuTarget * /*parent*/, const std::string & /*actionKey*/)
{
	return callScript(NULL);
}

/* The rows that open a dialog and hand what it answered to the script. */
int CPyMenuStringinput::exec(CMenuTarget * /*parent*/, const std::string & /*actionKey*/)
{
	/* "sms" asked for the multi-tap letters on the digit keys; the input
	 * dialog brings an on-screen keyboard for letters instead. */
	ScriptStringinput::params_t pars;
	pars.name        = name;
	pars.value       = *value;
	pars.size        = size;
	pars.valid_chars = valid_chars;
	pars.icon        = icon;
	*value = ScriptStringinput::exec(pars);

	PyGILState_STATE gstate = PyGILState_Ensure();
	PyObject *val = PyUnicode_FromString(value->c_str());
	int res = callScript(val);
	Py_XDECREF(val);
	PyGILState_Release(gstate);
	return res;
}

int CPyMenuKeyboardinput::exec(CMenuTarget * /*parent*/, const std::string & /*actionKey*/)
{
	CKeyboardInput *i;
	i = new CKeyboardInput((char *)name.c_str(), value, size, NULL, icon.c_str(), help, help2);
	i->setMasked(masked);
	i->exec(NULL, "");
	delete i;

	/* a masked row shows whether a value is set, never the value */
	if (masked && row)
		row->setOption(ScriptMenu::maskedText(*value));

	PyGILState_STATE gstate = PyGILState_Ensure();
	PyObject *val = PyUnicode_FromString(value->c_str());
	int res = callScript(val);
	Py_XDECREF(val);
	PyGILState_Release(gstate);
	return res;
}

int CPyMenuFilebrowser::exec(CMenuTarget * /*parent*/, const std::string & /*actionKey*/)
{
	CFileBrowser fileBrowser;
	fileBrowser.Dir_Mode = dirMode;

	CFileFilter fileFilter;
	for (std::vector<std::string>::iterator it = filter.begin(); it != filter.end(); ++it)
		fileFilter.addFilter(*it);
	if (!filter.empty())
		fileBrowser.Filter = &fileFilter;

	std::string tmpValue = (dirMode) ? *value : getPathName(*value);
	if (fileBrowser.exec(tmpValue.c_str()) == true)
		*value = fileBrowser.getSelectedFile()->Name;

	PyGILState_STATE gstate = PyGILState_Ensure();
	PyObject *val = PyUnicode_FromString(value->c_str());
	int res = callScript(val);
	Py_XDECREF(val);
	PyGILState_Release(gstate);
	return res;
}

/* What a script's callable is told when a chooser changes. */
bool CPyMenuObserver::changeNotify(const std::string &optionName, void *data)
{
	if (action == NULL || action == Py_None)
		return false;

	const char *optionValue = (const char *)data;

	PyGILState_STATE gstate = PyGILState_Ensure();
	PyObject *id_obj = PyUnicode_FromString(id.c_str());
	PyObject *val_obj = PyUnicode_FromString(optionValue != NULL ? optionValue : "");
	PyObject *result = NULL;
	if (id_obj != NULL && val_obj != NULL)
		result = PyObject_CallFunctionObjArgs(action, id_obj, val_obj, NULL);

	bool repaint = false;
	if (result != NULL)
	{
		long code = PyLong_AsLong(result);
		if (code == -1 && PyErr_Occurred())
			PyErr_Clear();
		else
			repaint = ((int)code == menu_return::RETURN_REPAINT) || ((int)code == menu_return::RETURN_EXIT_REPAINT);
		Py_DECREF(result);
	}
	else if (PyErr_Occurred())
	{
		PyObject *type = NULL, *val = NULL, *tb = NULL;
		PyErr_Fetch(&type, &val, &tb);
		PyErr_NormalizeException(&type, &val, &tb);
		std::string what = "NULL";
		if (val != NULL)
		{
			PyObject *str = PyObject_Str(val);
			if (str != NULL)
			{
				const char *s = PyUnicode_AsUTF8(str);
				if (s != NULL)
					what = s;
				else
					PyErr_Clear();
				Py_DECREF(str);
			}
			else
				PyErr_Clear();
		}
		fprintf(stderr, "[CPyMenuObserver::%s] error in script: %s\n", __func__, what.c_str());
		DisplayErrorMessage(what.c_str(), "Python Script Error:");
		Py_XDECREF(type);
		Py_XDECREF(val);
		Py_XDECREF(tb);
	}
	Py_XDECREF(id_obj);
	Py_XDECREF(val_obj);
	PyGILState_Release(gstate);

	(void)optionName;
	return repaint;
}

/* m.addKey(action=callable, id="", directkey=RC[...]), where a direct key is
 * one of the keys the RC table names; the row is made without one, which is
 * what the Lua binding's RC_nokey default is, where none is given. */
PyObject *CPythonInstMenu::MenuAddKey(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyMenu *D = menuOf(self);
	if (D == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "menu is gone");
		return NULL;
	}

	std::string id = "";
	pyLookupString(kwds, "id", id);

	unsigned long directkey = (unsigned long)CRCInput::RC_nokey;
	pyLookupUnsigned(kwds, "directkey", directkey);

	PyObject *action = NULL;
	if (!pyLookupObject(kwds, "action", &action))
		Py_RETURN_NONE;
	if (directkey == (unsigned long)CRCInput::RC_nokey)
		Py_RETURN_NONE;

	CPyMenuTarget *forwarder = new CPyMenuTarget(action, id);
	D->m->addKey(directkey, forwarder, id);
	D->targets.push_back(forwarder);

	Py_RETURN_NONE;
}

/* m.addItem(type=..., name=..., ...), answering the id the item is known by,
 * as the Lua binding answers, or None where it could not be made. */
PyObject *CPythonInstMenu::MenuAddItem(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyMenu *D = menuOf(self);
	if (D == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "menu is gone");
		return NULL;
	}
	if (args != NULL && PyTuple_GET_SIZE(args) > 0)
	{
		PyErr_SetString(PyExc_TypeError, "addItem() takes keyword arguments only");
		return NULL;
	}

	ScriptMenuItem *b = D->add();

	pyLookupString(kwds, "name", b->name);
	std::string type = "";
	pyLookupString(kwds, "type", type);
	b->type = type;

	CMenuItem *mi = NULL;

	/* The types that are the menu's own are the same for both bindings and are
	 * built where the menu is; what is left here is the item a script names of
	 * its own. */
	int row_id = 0;
	if (!D->addIfPlain(type, b->name))
	{
		/* What a script names of its own, read out of its call. The list a
		 * chooser is built from, and the names a filebrowser takes, are read
		 * into the parameters; the row itself is built where the menu is. */
		py_item_params pars;
		pyLookupString(kwds, "id", pars.id);
		pyLookupString(kwds, "value", pars.value);
		pyLookupString(kwds, "icon", pars.icon);
		pyLookupString(kwds, "right_icon", pars.right_icon);
		pyLookupString(kwds, "hint", pars.hint);
		pyLookupString(kwds, "hint_icon", pars.hint_icon);
		pyLookupString(kwds, "range", pars.range);
		pyLookupString(kwds, "help", pars.help);
		pyLookupString(kwds, "help2", pars.help2);

		/* the characters a stringinput takes, unless a script narrows them */
		std::string valid_chars = "abcdefghijklmnopqrstuvwxyz0123456789!\"$%&/()=?-.@,_: ";
		pyLookupString(kwds, "valid_chars", valid_chars);
		pars.valid_chars = valid_chars;

		pyLookupObject(kwds, "action", &pars.action);

		long size = (type == "stringinput") ? 30 : 0;
		pyLookupInt(kwds, "size", size);
		pars.size = (int)size;

		unsigned long directkey = (unsigned long)CRCInput::RC_nokey;
		pyLookupUnsigned(kwds, "directkey", directkey);
		pars.directkey = directkey;

		long pulldown = 0;
		pyLookupInt(kwds, "pulldown", pulldown);
		pars.pulldown = (pulldown != 0);

		pars.enabled = true;
		pyLookupBoolCompat(kwds, "enabled", pars.enabled);

		pars.sms = false;
		pyLookupBoolCompat(kwds, "sms", pars.sms);

		pars.masked = false;
		pyLookupBoolCompat(kwds, "masked", pars.masked);

		pars.dir_mode = false;
		pyLookupBoolCompat(kwds, "dir_mode", pars.dir_mode);

		if (type == "chooser")
		{
			/* the options the script wrote: a dict of value to text */
			PyObject *options = NULL;
			Py_ssize_t options_count = 0;
			if (pyLookupObject(kwds, "options", &options) && PyDict_Check(options))
				options_count = PyDict_Size(options);
			if (options_count == 0)
			{
				D->optionError();
				Py_RETURN_NONE;
			}

			PyObject *key = NULL, *val = NULL;
			Py_ssize_t pos = 0;
			while (PyDict_Next(options, &pos, &key, &val))
			{
				long k = PyLong_AsLong(key);
				if (k == -1 && PyErr_Occurred())
				{
					PyErr_Clear();
					continue;
				}
				std::string v = "";
				if (!pyArgString(val, v))
					v = "ERROR";

				b->options.push_back(std::pair<int, std::string>((int)k, v));
			}
		}
		else if (type == "filebrowser")
		{
			/* and the names it takes, where it named any */
			PyObject *filters = NULL;
			if (pyLookupList(kwds, "filter", &filters))
			{
				Py_ssize_t count = PySequence_Size(filters);
				for (Py_ssize_t k = 0; k < count; k++)
				{
					PyObject *filter = PySequence_GetItem(filters, k);
					if (filter == NULL)
					{
						PyErr_Clear();
						continue;
					}
					const char *val = PyUnicode_Check(filter) ? PyUnicode_AsUTF8(filter) : NULL;
					if (val != NULL)
						pars.filters.push_back(val);
					else
						PyErr_Clear();
					Py_DECREF(filter);
				}
			}
		}

		mi = D->makeRow(b, pars, D);
		row_id = D->addRow(b, mi, pars);
	}

	if (row_id > 0)
		return PyLong_FromLong(row_id);

	Py_RETURN_NONE;
}

/* m.exec(): shows the menu and waits, as the Lua binding runs it. */
PyObject *CPythonInstMenu::MenuExec(PyObject *self, PyObject * /*args*/, PyObject * /*kwds*/)
{
	CPyMenu *D = menuOf(self);
	if (D == NULL || D->m == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "menu is gone");
		return NULL;
	}

	CPythonInstance::scriptUiOpen();
	D->m->exec(NULL, "");
	D->m->hide();
	CPythonInstance::scriptUiClose();

	Py_RETURN_NONE;
}

PyObject *CPythonInstMenu::MenuHide(PyObject *self, PyObject * /*args*/, PyObject * /*kwds*/)
{
	CPyMenu *D = menuOf(self);
	if (D == NULL || D->m == NULL)
		Py_RETURN_NONE;

	D->m->hide();
	Py_RETURN_NONE;
}

/* m.setActive(item=id, activ=True) */
PyObject *CPythonInstMenu::MenuSetActive(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyMenu *D = menuOf(self);
	if (D == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "menu is gone");
		return NULL;
	}

	long item = 0;
	bool activ = false;
	pyLookupInt(kwds, "item", item);
	pyLookupBool(kwds, "activ", activ);

	ScriptMenuItem *it = D->item((int)item);
	if (it != NULL && it->mi != NULL)
		it->mi->setActive(activ);

	Py_RETURN_NONE;
}

/* m.setName(item=id, name="...") */
PyObject *CPythonInstMenu::MenuSetName(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyMenu *D = menuOf(self);
	if (D == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "menu is gone");
		return NULL;
	}

	long item = 0;
	std::string name = "";
	pyLookupInt(kwds, "item", item);
	pyLookupString(kwds, "name", name);

	ScriptMenuItem *it = D->item((int)item);
	if (it != NULL && it->mi != NULL)
		it->mi->setName(name);

	Py_RETURN_NONE;
}

/* m.setSelected(preselected=...) */
PyObject *CPythonInstMenu::MenuSetSelected(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyMenu *D = menuOf(self);
	if (D == NULL || D->m == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "menu is gone");
		return NULL;
	}

	long preselected = 0;
	pyLookupInt(kwds, "preselected", preselected);

	D->m->setSelected((int)preselected);
	Py_RETURN_NONE;
}

PyObject *CPythonInstMenu::MenuGetSelected(PyObject *self, PyObject * /*args*/, PyObject * /*kwds*/)
{
	CPyMenu *D = menuOf(self);
	if (D == NULL || D->m == NULL)
		Py_RETURN_NONE;

	return PyLong_FromLong((long)D->m->getSelected());
}

/* m.setValue(item=id, value="..."): what the row shows, without telling the
 * script, which is what the Lua binding does with it. */
PyObject *CPythonInstMenu::MenuSetValue(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyMenu *D = menuOf(self);
	if (D == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "menu is gone");
		return NULL;
	}

	long item = 0;
	std::string value = "";
	pyLookupInt(kwds, "item", item);
	pyLookupString(kwds, "value", value);

	ScriptMenuItem *it = D->item((int)item);
	if (it == NULL || it->mi == NULL)
		Py_RETURN_NONE;

	/*
	 * setOption() belongs to CMenuForwarder and the cast to it cannot be
	 * checked at runtime, this directory is built with -fno-rtti. The
	 * chooser family are siblings of CMenuForwarder, not subclasses, so
	 * casting one of them would write through a pointer to an unrelated
	 * type. Ask the item itself which family it belongs to.
	 */
	if (!it->mi->isMenueOptionChooser())
	{
		/*
		 * Set the value the entry edits, so that its input opens with it.
		 * Without a text of its own the row shows that value through the
		 * entry's target and follows every later input; a masked row is
		 * told whether a value is set, never the value.
		 */
		it->str_val = value;
		static_cast<CMenuForwarder *>(it->mi)->setOption(it->masked ? ScriptMenu::maskedText(value) : "");
		Py_RETURN_NONE;
	}

	if (it->type == "chooser")
	{
		for (unsigned int i = 0; i < it->options.size(); i++)
		{
			if (value == it->options[i].second)
			{
				it->int_val = (int)it->options[i].first;
				break;
			}
		}
	}
	else if (it->type == "numeric")
	{
		sscanf(value.c_str(), "%d", &it->int_val);
	}
	else if (it->type == "string")
	{
		it->str_val = value;
	}

	Py_RETURN_NONE;
}

/* m.paintItem(item=id): the row is painted again where it stands. */
PyObject *CPythonInstMenu::MenuPaintItem(PyObject *self, PyObject * /*args*/, PyObject *kwds)
{
	CPyMenu *D = menuOf(self);
	if (D == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "menu is gone");
		return NULL;
	}

	long item = 0;
	pyLookupInt(kwds, "item", item);

	ScriptMenuItem *it = D->item((int)item);
	if (it != NULL && it->mi != NULL)
		it->mi->paint();

	Py_RETURN_NONE;
}

bool CPythonInstMenu::MenuRegister(PyObject *module)
{
	static PyMethodDef menu_methods[] =
	{
		PY_METHODDEF("addKey",      CPythonInstMenu::MenuAddKey),
		PY_METHODDEF("addItem",     CPythonInstMenu::MenuAddItem),
		PY_METHODDEF("exec",        CPythonInstMenu::MenuExec),
		PY_METHODDEF("hide",        CPythonInstMenu::MenuHide),
		PY_METHODDEF("setActive",   CPythonInstMenu::MenuSetActive),
		PY_METHODDEF("setName",     CPythonInstMenu::MenuSetName),
		PY_METHODDEF("setSelected", CPythonInstMenu::MenuSetSelected),
		PY_METHODDEF("getSelected", CPythonInstMenu::MenuGetSelected),
		PY_METHODDEF("setValue",    CPythonInstMenu::MenuSetValue),
		PY_METHODDEF("paintItem",   CPythonInstMenu::MenuPaintItem),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("new", CPythonInstMenu::MenuNewModule),
		{ NULL, NULL, 0, NULL }
	};

	static PyType_Slot menu_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstMenu::MenuNew },
		{ Py_tp_dealloc, (void *)CPythonInstMenu::MenuDelete },
		{ Py_tp_methods, (void *)menu_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"menu" },
		{ 0, NULL }
	};

	static PyType_Spec menu_spec =
	{
		"neutrino.menu.menu",
		sizeof(PyMenuObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		menu_slots
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.menu",
		"menu",
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

	menu_type = (PyTypeObject *)PyType_FromSpec(&menu_spec);
	if (menu_type == NULL)
	{
		PyErr_Clear();
		Py_DECREF(sub);
		return false;
	}
	if (PyModule_AddObject(sub, "menu", (PyObject *)menu_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)menu_type);
		menu_type = NULL;
		Py_DECREF(sub);
		return false;
	}

	if (!PythonInstAddSubmodule(module, "menu", sub))
		return false;

	/* The misspelling the Lua binding also keeps, so that a script carried
	 * over can be left as it is. */
	Py_INCREF(sub);
	if (PyModule_AddObject(module, "menue", sub) < 0)
	{
		PyErr_Clear();
		Py_DECREF(sub);
	}
	PyObject *modules = PyImport_GetModuleDict();
	if (PyDict_SetItemString(modules, PY_CLASSNAME ".menue", sub) < 0)
		PyErr_Clear();

	return true;
}




