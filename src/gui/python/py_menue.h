/*
 * python menu
 *
 * The Lua binding in lua_menue.cpp, read as
 *
 *	from neutrino import menu, MENU_RETURN
 *
 *	def on_ok(item_id):
 *		return MENU_RETURN["EXIT"]
 *
 *	m = menu.new(name="Title", btnRed="red")
 *	m.addItem(type="forwarder", name="Do it", id="ok", action=on_ok)
 *	m.addItem(type="chooser", name="Pick", options={1: "one", 2: "two"})
 *	m.exec()
 *
 * Where the Lua binding names a function of the script and looks it up in
 * its globals, this one takes the callable itself, and calls it with the
 * item's id alone where the row carries no value of its own - a forwarder,
 * and a key - and with the id and the value where it does: a chooser, a
 * numeric, a string, a stringinput, a keyboardinput and a filebrowser.
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

#ifndef _PYMENUE_H
#define _PYMENUE_H

#include "pyinstance_helpers.h"

#include <list>
#include <map>
#include <string>
#include <vector>

#include <gui/widget/menue.h>

/* One option of a chooser, as a script writes it: the value and the text. */
typedef std::pair<long, std::string> pymenu_option_t;

struct CPyMenuItem
{
	int int_val;
	std::string str_val;
	std::string name;
	std::string type;
	std::vector<pymenu_option_t> options;
	bool masked;
	CMenuItem *mi;
};

/* A row that calls back into the script, where the Lua binding has its
 * CLuaMenuForwarder. */
class CPyMenuTarget : public CMenuTarget
{
	public:
		PyObject *action;
		std::string id;
		CPyMenuTarget(PyObject *a, const std::string &i) { Py_XINCREF(a); action = a; id = i; }
		virtual ~CPyMenuTarget() { Py_XDECREF(action); }
		virtual int exec(CMenuTarget *parent = NULL, const std::string &actionKey = "");

		/* what the menu row shows while it has no text of its own */
		void showValue(std::string *_value) { valueString = _value; }

	protected:
		/* Calls the script's callable with the id, and with the value where
		 * one is handed on; answers the menu return code it gives. */
		int callScript(PyObject *value);
};

/* The rows that hand a value to the script after a dialog. */
class CPyMenuStringinput : public CPyMenuTarget
{
	private:
		std::string *value;
		std::string valid_chars;
		std::string name;
		std::string icon;
		bool sms;
		int size;
	public:
		CPyMenuStringinput(PyObject *a, const std::string &i, const std::string &_name,
				std::string *_value, int _size, std::string _valid_chars,
				const std::string &_icon, bool _sms)
			: CPyMenuTarget(a, i)
		{
			name = _name; value = _value; size = _size;
			valid_chars = _valid_chars; icon = _icon; sms = _sms;
			showValue(value);
		}
		int exec(CMenuTarget *parent = NULL, const std::string &actionKey = "");
};

class CPyMenuKeyboardinput : public CPyMenuTarget
{
	private:
		std::string *value;
		std::string name;
		std::string icon;
		int size;
		bool masked;
		CMenuForwarder *row;
		std::string help, help2;
	public:
		CPyMenuKeyboardinput(PyObject *a, const std::string &i, const std::string &_name,
				std::string *_value, int _size, const std::string &_icon,
				std::string _help, std::string _help2, bool _masked)
			: CPyMenuTarget(a, i)
		{
			name = _name; value = _value; size = _size; masked = _masked;
			row = NULL; icon = _icon; help = _help; help2 = _help2;
			// a masked row never falls back to the value, it is told what to show
			if (!masked)
				showValue(value);
		}
		void setRow(CMenuForwarder *_row) { row = _row; }
		int exec(CMenuTarget *parent = NULL, const std::string &actionKey = "");
};

class CPyMenuFilebrowser : public CPyMenuTarget
{
	private:
		std::string *value;
		bool dirMode;
		std::vector<std::string> filter;
	public:
		CPyMenuFilebrowser(PyObject *a, const std::string &i, std::string *_value, bool _dirMode)
			: CPyMenuTarget(a, i)
		{
			value = _value; dirMode = _dirMode;
			showValue(value);
		}
		void addFilter(const std::string &s) { filter.push_back(s); }
		int exec(CMenuTarget *parent = NULL, const std::string &actionKey = "");
};

/* What a script's callable is told when a chooser changes. */
class CPyMenuObserver : public CChangeObserver
{
	public:
		PyObject *action;
		std::string id;
		CPyMenuObserver(PyObject *a, const std::string &i) { Py_XINCREF(a); action = a; id = i; }
		virtual ~CPyMenuObserver() { Py_XDECREF(action); }
		virtual bool changeNotify(const std::string &optionName, void *data);
};

class CPyMenu
{
	public:
		CMenuWidget *m;
		std::list<CPyMenuItem> items;
		std::list<CMenuTarget *> targets;
		std::list<void *> tofree;
		std::list<CPyMenuObserver *> observers;
		std::map<long, CPyMenuItem *> itemmap;
		CPyMenu();
		~CPyMenu();
};

class CPythonInstMenu
{
	public:
		CPythonInstMenu() {};
		~CPythonInstMenu() {};
		static CPythonInstMenu *getInstance();
		static bool MenuRegister(PyObject *module);

	private:
		static PyObject *MenuNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static PyObject *MenuNewModule(PyObject *self, PyObject *args, PyObject *kwds);
		static void MenuDelete(PyObject *self);

		static PyObject *MenuAddKey(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *MenuAddItem(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *MenuExec(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *MenuHide(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *MenuSetActive(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *MenuSetName(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *MenuSetSelected(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *MenuGetSelected(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *MenuSetValue(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *MenuPaintItem(PyObject *self, PyObject *args, PyObject *kwds);
};

#endif //_PYMENUE_H
