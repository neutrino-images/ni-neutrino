/*
 * lua menue
 *
 * (C) 2014 by martii
 * (C) 2014-2015 M. Liebmann (micha-bbg)
 * (C) 2014 Sven Hoefer (svenhoefer)
 * (C) 2015 Jacek Jendrzej (SatBaby)
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

#ifndef _LUAMENUE_H
#define _LUAMENUE_H

#include <gui/scriptapi/scriptmenu.h>

class CLuaMenuChangeObserver : public CChangeObserver
{
	public:
		bool changeNotify(lua_State *, const std::string &, const std::string &, void *);
};

/* What the lua binding carries through with an item and no other binding has:
 * the state it calls the script with, the name of the function a row calls, and
 * the filters of a filebrowser. */
struct lua_item_params : ScriptMenuItemParams
{
	lua_State			*L;
	std::string			action;
	std::vector<std::string>	filters;
};

/* What a row of this menu hands on when it is picked, which is the one thing
 * about a menu that cannot be said without knowing the interpreter. The rest of
 * the menu, and the rows themselves, is the script API. The menu is its own
 * factory, so that what a row needs is made where the rows are kept and goes
 * with them. */
class CLuaMenu : public ScriptMenu, public ScriptMenu::factory
{
	public:
		CLuaMenuChangeObserver *observ;
		std::list<CMenuTarget *> targets;
		CLuaMenu(const params_t &pars);
		~CLuaMenu();

		CMenuTarget	*forwarder(const ScriptMenuItemParams &pars);
		CMenuTarget	*stringinput(const ScriptMenuItemParams &pars, ScriptMenuItem *item, const char *icon);
		CMenuTarget	*keyboardinput(const ScriptMenuItemParams &pars, ScriptMenuItem *item, const char *icon);
		CMenuTarget	*filebrowser(const ScriptMenuItemParams &pars, ScriptMenuItem *item);
		void		setMaskedRow(CMenuTarget *target, CMenuForwarder *row);
		CChangeObserver	*observer(const ScriptMenuItemParams &pars);
};

class CLuaMenuForwarder : public CMenuTarget
{
	private:
		void Init(lua_State *_L, std::string _luaAction, std::string _luaId);
	public:
		lua_State *L;
		std::string luaAction;
		std::string luaId;
		CLuaMenuForwarder(lua_State *L, std::string _luaAction, std::string _luaId);
		~CLuaMenuForwarder();
		int exec(CMenuTarget* parent=NULL, const std::string & actionKey="");
		// what the menu row shows while it has no text of its own
		void showValue(std::string *_value) { setValue(_value); }
};

class CLuaMenuFilebrowser : public CLuaMenuForwarder
{
	private:
		std::string *value;
		bool dirMode;
		std::vector<std::string> filter;
		void Init(std::string *_value, bool _dirMode);
	public:
		CLuaMenuFilebrowser(lua_State *_L, std::string _luaAction, std::string _luaId, std::string *_value, bool _dirMode);
		int exec(CMenuTarget* parent=NULL, const std::string & actionKey="");
		void addFilter(std::string s) { filter.push_back(s); };
};

class CLuaMenuStringinput : public CLuaMenuForwarder
{
	private:
		std::string *value;
		std::string valid_chars;
		const char *name;
		const char *icon;
		bool sms;
		int size;
		CChangeObserver *observ;
		void Init(const char *_name, std::string *_value, int _size, std::string _valid_chars, CChangeObserver *_observ, const char *_icon, bool _sms);
	public:
		CLuaMenuStringinput(lua_State *_L, std::string _luaAction, std::string _luaId, const char *_name, std::string *_value, int _size, std::string _valid_chars, CChangeObserver *_observ, const char *_icon, bool _sms);
		int exec(CMenuTarget* parent=NULL, const std::string & actionKey="");
};

class CLuaMenuKeyboardinput : public CLuaMenuForwarder
{
	private:
		std::string *value;
		const char *name;
		const char *icon;
		int size;
		bool masked;
		CMenuForwarder *row;
		CChangeObserver *observ;
		std::string help, help2;
		void Init(const char *_name, std::string *_value, int _size, CChangeObserver *_observ, const char *_icon, std::string _help, std::string _help2, bool _masked);
	public:
		CLuaMenuKeyboardinput(lua_State *_L, std::string _luaAction, std::string _luaId, const char *_name, std::string *_value, int _size, CChangeObserver *_observ, const char *_icon, std::string _help, std::string _help2, bool _masked = false);
		// the menu row of a masked entry, told after each input whether a value is set
		void setRow(CMenuForwarder *_row) { row = _row; }
		int exec(CMenuTarget* parent=NULL, const std::string & actionKey="");
};

class CLuaInstMenu
{
	public:
		CLuaInstMenu() {};
		~CLuaInstMenu() {};
		static CLuaInstMenu* getInstance();
		static void MenuRegister(lua_State *L);

	private:
		static CLuaMenu *MenuCheck(lua_State *L, int n);
		static int MenuNew(lua_State *L);
		static int MenuAddKey(lua_State *L);
		static int MenuAddItem(lua_State *L);
		static int MenuExec(lua_State *L);
		static int MenuHide(lua_State *L);
		static int MenuSetActive(lua_State *L);
		static int MenuSetName(lua_State *L);
		static int MenuDelete(lua_State *L);
		static int MenuSetSelected(lua_State *L);
		static int MenuGetSelected(lua_State *L);
		static int MenuSetValue(lua_State *L);
		static int MenuPaintItem(lua_State *L);
};

#endif //_LUAMENUE_H
