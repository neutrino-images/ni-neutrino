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

/* Items are keyed by the id addItem handed out to the script. */
static CPyMenuItem *getMenuItem(CPyMenu *D, long id)
{
	std::map<long, CPyMenuItem *>::iterator it = D->itemmap.find(id);
	return (it == D->itemmap.end()) ? NULL : it->second;
}

/* What the row of a masked entry shows: whether a value is set, not the value. */
static std::string maskedText(const std::string &value)
{
	return value.empty() ? "" : "********";
}

CPyMenu::CPyMenu()
{
	m = NULL;
}

CPyMenu::~CPyMenu()
{
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
	while (!tofree.empty())
	{
		free(tofree.back());
		tofree.pop_back();
	}
	itemmap.clear();
	delete m;
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

	std::string name, icon;
	pyLookupString(kwds, "name", name) || pyLookupString(kwds, "title", name);
	pyLookupString(kwds, "icon", icon);

	long mwidth = 0;
	bool haveWidth = pyLookupInt(kwds, "mwidth", mwidth);

	CMenuWidget *m = haveWidth ? new CMenuWidget(name.c_str(), icon.c_str(), (int)mwidth)
				   : new CMenuWidget(name.c_str(), icon.c_str());

	/* the footer's buttons, each with the icon of the key it belongs to */
	static const struct { const char *key; const char *icon; } btn_keys[] = {
		{ "btnRed",    NEUTRINO_ICON_BUTTON_RED },
		{ "btnGreen",  NEUTRINO_ICON_BUTTON_GREEN },
		{ "btnYellow", NEUTRINO_ICON_BUTTON_YELLOW },
		{ "btnBlue",   NEUTRINO_ICON_BUTTON_BLUE },
		{ NULL, NULL }
	};

	std::vector<button_label_cc> buttons;
	for (int i = 0; btn_keys[i].key != NULL; i++)
	{
		std::string label = "";
		if (!pyLookupString(kwds, btn_keys[i].key, label) || label.empty())
			continue;
		button_label_cc b;
		b.button = btn_keys[i].icon;
		b.text = label;
		buttons.push_back(b);
	}
	if (!buttons.empty())
		m->setFooterButtons(buttons);

	PyMenuObject *self = (PyMenuObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		delete m;
		return NULL;
	}
	self->d = new CPyMenu();
	self->d->m = m;
	return (PyObject *)self;
}

PyObject *CPythonInstMenu::MenuNewModule(PyObject *self, PyObject *args, PyObject *kwds)
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
	CStringInput *i;
	if (sms)
		i = new CStringInputSMS((char *)name.c_str(), value, size,
				NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, valid_chars.c_str(), NULL, icon.c_str());
	else
		i = new CStringInput((char *)name.c_str(), value, size,
				NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, valid_chars.c_str(), NULL, icon.c_str());
	i->exec(NULL, "");
	delete i;

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
		row->setOption(maskedText(*value));

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
PyObject *CPythonInstMenu::MenuAddKey(PyObject *self, PyObject *args, PyObject *kwds)
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

	CPyMenuItem i;
	i.int_val = 0;
	i.masked = false;
	i.mi = NULL;
	D->items.push_back(i);
	CPyMenuItem *b = &D->items.back();

	pyLookupString(kwds, "name", b->name);
	std::string type = "";
	pyLookupString(kwds, "type", type);
	b->type = type;

	CMenuItem *mi = NULL;

	if (type == "back")
	{
		D->m->addItem(GenericMenuBack);
	}
	else if (type == "next")
	{
		D->m->addItem(GenericMenuNext);
	}
	else if (type == "cancel")
	{
		D->m->addItem(GenericMenuCancel);
	}
	else if (type == "separator")
	{
		D->m->addItem(GenericMenuSeparator);
	}
	else if ((type == "separatorline") || (type == "subhead"))
	{
		if (!b->name.empty())
		{
			int flag = (type == "separatorline") ? CMenuSeparator::LINE : CMenuSeparator::SUB_HEAD;
			D->m->addItem(new CMenuSeparator(CMenuSeparator::STRING | flag, b->name.c_str(), NONEXISTANT_LOCALE));
		}
		else
			D->m->addItem(GenericMenuSeparatorLine);
	}
	else
	{
		std::string right_icon_str = "", action_str = "", value = "", hint = "", hint_icon_str = "", icon_str = "", id = "";
		pyLookupString(kwds, "right_icon", right_icon_str);
		pyLookupString(kwds, "value", value);
		pyLookupString(kwds, "hint", hint);
		pyLookupString(kwds, "hint_icon", hint_icon_str);
		pyLookupString(kwds, "icon", icon_str);
		pyLookupString(kwds, "id", id);

		PyObject *action = NULL;
		pyLookupObject(kwds, "action", &action);

		char *right_icon = NULL;
		if (!right_icon_str.empty())
		{
			right_icon = strdup(right_icon_str.c_str());
			D->tofree.push_back(right_icon);
		}
		char *hint_icon = NULL;
		if (!hint_icon_str.empty())
		{
			hint_icon = strdup(hint_icon_str.c_str());
			D->tofree.push_back(hint_icon);
		}
		char *icon = NULL;
		if (!icon_str.empty())
		{
			icon = strdup(icon_str.c_str());
			D->tofree.push_back(icon);
		}

		unsigned long directkey = (unsigned long)CRCInput::RC_nokey;
		pyLookupUnsigned(kwds, "directkey", directkey);

		long pulldown = 0;
		pyLookupInt(kwds, "pulldown", pulldown);

		bool enabled = true;
		pyLookupBoolCompat(kwds, "enabled", enabled);

		std::string range = "";
		pyLookupString(kwds, "range", range);
		int range_from = 0, range_to = 99;
		sscanf(range.c_str(), "%d,%d", &range_from, &range_to);

		if (type == "forwarder")
		{
			b->str_val = value;
			CPyMenuTarget *forwarder = new CPyMenuTarget(action, id);
			forwarder->showValue(&b->str_val);
			mi = new CMenuForwarder(b->name, enabled, b->str_val, forwarder, NULL, directkey, icon, right_icon);
			if (!hint.empty() || hint_icon)
				mi->setHint(hint_icon, hint);
			D->targets.push_back(forwarder);
		}
		else if (type == "chooser")
		{
			/* the options the script wrote: a dict of value to text */
			PyObject *options = NULL;
			Py_ssize_t options_count = 0;
			if (pyLookupObject(kwds, "options", &options) && PyDict_Check(options))
				options_count = PyDict_Size(options);
			if (options_count == 0)
			{
				D->m->addItem(new CMenuSeparator(CMenuSeparator::STRING | CMenuSeparator::LINE, "ERROR! (options_count)", NONEXISTANT_LOCALE));
				Py_RETURN_NONE;
			}

			CMenuOptionChooser::keyval_ext *kext = (CMenuOptionChooser::keyval_ext *)calloc(options_count, sizeof(CMenuOptionChooser::keyval_ext));
			D->tofree.push_back(kext);
			b->int_val = 0;

			PyObject *key = NULL, *val = NULL;
			Py_ssize_t pos = 0;
			int j = 0;
			while (PyDict_Next(options, &pos, &key, &val) && j < options_count)
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

				kext[j].key = (int)k;
				kext[j].value = NONEXISTANT_LOCALE;
				kext[j].valname = strdup(v.c_str());
				D->tofree.push_back((void *)kext[j].valname);
				b->options.push_back(pymenu_option_t((long)kext[j].key, kext[j].valname));
				if (value == kext[j].valname)
					b->int_val = kext[j].key;
				j++;
			}

			CPyMenuObserver *observ = new CPyMenuObserver(action, id);
			D->observers.push_back(observ);
			mi = new CMenuOptionChooser(b->name.c_str(), &b->int_val, kext, (int)options_count, enabled, observ, directkey, icon, pulldown);
		}
		else if (type == "numeric")
		{
			b->int_val = range_from;
			sscanf(value.c_str(), "%d", &b->int_val);
			CPyMenuObserver *observ = new CPyMenuObserver(action, id);
			D->observers.push_back(observ);
			mi = new CMenuOptionNumberChooser(b->name, &b->int_val, enabled, range_from, range_to, observ, 0, 0, NONEXISTANT_LOCALE, pulldown);
		}
		else if (type == "string")
		{
			b->str_val = value;
			CPyMenuObserver *observ = new CPyMenuObserver(action, id);
			D->observers.push_back(observ);
			mi = new CMenuOptionStringChooser(b->name.c_str(), &b->str_val, enabled, observ, directkey, icon, pulldown);
		}
		else if (type == "stringinput")
		{
			b->str_val = value;
			std::string valid_chars = "abcdefghijklmnopqrstuvwxyz0123456789!\"$%&/()=?-.@,_: ";
			pyLookupString(kwds, "valid_chars", valid_chars);
			bool sms = false;
			pyLookupBoolCompat(kwds, "sms", sms);
			long size = 30;
			pyLookupInt(kwds, "size", size);

			CPyMenuStringinput *stringinput = new CPyMenuStringinput(action, id, b->name, &b->str_val,
					(int)size, valid_chars, icon_str, sms);
			mi = new CMenuForwarder(b->name, enabled, b->str_val, stringinput, NULL, directkey, icon, right_icon);
			D->targets.push_back(stringinput);
		}
		else if (type == "keyboardinput")
		{
			b->str_val = value;
			long size = 0;
			pyLookupInt(kwds, "size", size);
			std::string help = "", help2 = "";
			pyLookupString(kwds, "help", help);
			pyLookupString(kwds, "help2", help2);

			/* masked=True, or masked=1 */
			bool masked = false;
			pyLookupBoolCompat(kwds, "masked", masked);
			b->masked = masked;

			CPyMenuKeyboardinput *keyboardinput = new CPyMenuKeyboardinput(action, id, b->name, &b->str_val,
					(int)size, icon_str, help, help2, masked);
			CMenuForwarder *row = new CMenuForwarder(b->name, enabled, b->str_val, keyboardinput, NULL, directkey, icon, right_icon);
			if (b->masked)
			{
				row->setOption(maskedText(b->str_val));
				keyboardinput->setRow(row);
			}
			mi = row;
			D->targets.push_back(keyboardinput);
		}
		else if (type == "filebrowser")
		{
			b->str_val = value;
			bool dirMode = false;
			pyLookupBoolCompat(kwds, "dir_mode", dirMode);

			CPyMenuFilebrowser *filebrowser = new CPyMenuFilebrowser(action, id, &b->str_val, dirMode);
			PyObject *filters = NULL;
			if (pyLookupList(kwds, "filter", &filters))
			{
				Py_ssize_t count = PySequence_Size(filters);
				for (Py_ssize_t k = 0; k < count; k++)
				{
					PyObject *item = PySequence_GetItem(filters, k);
					if (item == NULL)
					{
						PyErr_Clear();
						continue;
					}
					const char *val = PyUnicode_Check(item) ? PyUnicode_AsUTF8(item) : NULL;
					if (val != NULL)
						filebrowser->addFilter(val);
					else
						PyErr_Clear();
					Py_DECREF(item);
				}
			}

			mi = new CMenuForwarder(b->name, enabled, b->str_val, filebrowser, NULL, directkey, icon, right_icon);
			D->targets.push_back(filebrowser);
		}

		if (mi)
		{
			if (!hint.empty() || hint_icon)
				mi->setHint(hint_icon, hint);
			D->m->addItem(mi);
		}
	}

	if (mi)
	{
		long id = (long)D->itemmap.size() + 1;
		b->mi = mi;
		D->itemmap.insert(std::pair<long, CPyMenuItem *>(id, b));
		return PyLong_FromLong(id);
	}

	Py_RETURN_NONE;
}

/* m.exec(): shows the menu and waits, as the Lua binding runs it. */
PyObject *CPythonInstMenu::MenuExec(PyObject *self, PyObject *args, PyObject *kwds)
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

PyObject *CPythonInstMenu::MenuHide(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyMenu *D = menuOf(self);
	if (D == NULL || D->m == NULL)
		Py_RETURN_NONE;

	D->m->hide();
	Py_RETURN_NONE;
}

/* m.setActive(item=id, activ=True) */
PyObject *CPythonInstMenu::MenuSetActive(PyObject *self, PyObject *args, PyObject *kwds)
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

	CPyMenuItem *it = getMenuItem(D, item);
	if (it != NULL && it->mi != NULL)
		it->mi->setActive(activ);

	Py_RETURN_NONE;
}

/* m.setName(item=id, name="...") */
PyObject *CPythonInstMenu::MenuSetName(PyObject *self, PyObject *args, PyObject *kwds)
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

	CPyMenuItem *it = getMenuItem(D, item);
	if (it != NULL && it->mi != NULL)
		it->mi->setName(name);

	Py_RETURN_NONE;
}

/* m.setSelected(preselected=...) */
PyObject *CPythonInstMenu::MenuSetSelected(PyObject *self, PyObject *args, PyObject *kwds)
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

PyObject *CPythonInstMenu::MenuGetSelected(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyMenu *D = menuOf(self);
	if (D == NULL || D->m == NULL)
		Py_RETURN_NONE;

	return PyLong_FromLong((long)D->m->getSelected());
}

/* m.setValue(item=id, value="..."): what the row shows, without telling the
 * script, which is what the Lua binding does with it. */
PyObject *CPythonInstMenu::MenuSetValue(PyObject *self, PyObject *args, PyObject *kwds)
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

	CPyMenuItem *it = getMenuItem(D, item);
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
		static_cast<CMenuForwarder *>(it->mi)->setOption(it->masked ? maskedText(value) : "");
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
PyObject *CPythonInstMenu::MenuPaintItem(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyMenu *D = menuOf(self);
	if (D == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "menu is gone");
		return NULL;
	}

	long item = 0;
	pyLookupInt(kwds, "item", item);

	CPyMenuItem *it = getMenuItem(D, item);
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




