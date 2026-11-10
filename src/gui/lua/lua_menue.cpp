/*
 * lua menue
 *
 * (C) 2014 by martii
 * (C) 2014-2015 M. Liebmann (micha-bbg)
 * (C) 2016 Sven Hoefer (svenhoefer)
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

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <gui/widget/keyboard_input.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/msgbox.h>
#include <gui/filebrowser.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_menue.h"
#include <gui/scriptapi/scriptstringinput.h>

CLuaInstMenu* CLuaInstMenu::getInstance()
{
	static CLuaInstMenu* LuaInstMenu = NULL;

	if(!LuaInstMenu)
		LuaInstMenu = new CLuaInstMenu();
	return LuaInstMenu;
}

bool CLuaMenuChangeObserver::changeNotify(lua_State *L, const std::string &luaAction, const std::string &luaId, void *Data)
{
	const char *optionValue = (const char *) Data;
	lua_pushglobaltable(L);
	lua_getfield(L, -1, luaAction.c_str());
	lua_remove(L, -2);
	lua_pushstring(L, luaId.c_str());
	lua_pushstring(L, optionValue);
	int status = lua_pcall(L, 2 /* two args */, 1 /* one result */, 0);
	if (status) {
		bool isString = lua_isstring(L,-1);
		const char *null = "NULL";
		fprintf(stderr, "[CLuaMenuChangeObserver::%s:%d] error in script: %s\n", __func__, __LINE__, isString ? lua_tostring(L, -1): null);
		DisplayErrorMessage(isString ? lua_tostring(L, -1):null, "Lua Script Error:");
	}
	double res = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0;
	lua_pop(L, 1);
	return (((int)res == menu_return::RETURN_REPAINT) || ((int)res == menu_return::RETURN_EXIT_REPAINT));
}

void CLuaInstMenu::MenuRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",       CLuaInstMenu::MenuNew },
		{ "addKey",    CLuaInstMenu::MenuAddKey },
		{ "addItem",   CLuaInstMenu::MenuAddItem },
		{ "exec",      CLuaInstMenu::MenuExec },
		{ "hide",      CLuaInstMenu::MenuHide },
		{ "setActive", CLuaInstMenu::MenuSetActive },
		{ "setName",   CLuaInstMenu::MenuSetName },
		{ "setSelected",   CLuaInstMenu::MenuSetSelected },
		{ "getSelected",   CLuaInstMenu::MenuGetSelected },
		{ "setValue",   CLuaInstMenu::MenuSetValue },
		{ "paintItem",   CLuaInstMenu::MenuPaintItem },
		{ "__gc",      CLuaInstMenu::MenuDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "menu");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "menu");

	// keep misspelled "menue" for backwards-compatibility
	luaL_newmetatable(L, "menu");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "menue");
}

CLuaMenu *CLuaInstMenu::MenuCheck(lua_State *L, int n)
{
	return *(CLuaMenu **) luaL_checkudata(L, n, "menu");
}

CLuaMenu::CLuaMenu(const params_t &pars) : ScriptMenu(pars)
{
	observ = new CLuaMenuChangeObserver();
}

CLuaMenu::~CLuaMenu()
{
	/* The widget and the items are the script API's and go with it. The rows
	 * that call the script are this binding's; they are let go of by
	 * MenuDelete, which is where a menu of a script ends. */
	delete observ;
}

/* What a row of this menu hands on when it is picked, and what a chooser tells
 * the script. The menu is its own factory, so a row is made where the rows are
 * kept, and is let go of with them. */
CMenuTarget *CLuaMenu::forwarder(const ScriptMenuItemParams &pars)
{
	const lua_item_params &lp = (const lua_item_params &)pars;
	CLuaMenuForwarder *forwarder = new CLuaMenuForwarder(lp.L, lp.action, pars.id);
	targets.push_back(forwarder);
	return forwarder;
}

CMenuTarget *CLuaMenu::stringinput(const ScriptMenuItemParams &pars, ScriptMenuItem *item, const char *icon)
{
	const lua_item_params &lp = (const lua_item_params &)pars;
	/* "sms" asked for the multi-tap letters on the digit keys; the input
	 * dialog brings an on-screen keyboard for letters instead. */
	CLuaMenuStringinput *stringinput = new CLuaMenuStringinput(lp.L, lp.action, pars.id,
			item->name.c_str(), &item->str_val, pars.size, pars.valid_chars, observ,
			icon, pars.sms);
	targets.push_back(stringinput);
	return stringinput;
}

CMenuTarget *CLuaMenu::keyboardinput(const ScriptMenuItemParams &pars, ScriptMenuItem *item, const char *icon)
{
	const lua_item_params &lp = (const lua_item_params &)pars;
	CLuaMenuKeyboardinput *keyboardinput = new CLuaMenuKeyboardinput(lp.L, lp.action, pars.id,
			item->name.c_str(), &item->str_val, pars.size, observ, icon,
			pars.help, pars.help2, pars.masked);
	targets.push_back(keyboardinput);
	return keyboardinput;
}

CMenuTarget *CLuaMenu::filebrowser(const ScriptMenuItemParams &pars, ScriptMenuItem *item)
{
	const lua_item_params &lp = (const lua_item_params &)pars;
	CLuaMenuFilebrowser *filebrowser = new CLuaMenuFilebrowser(lp.L, lp.action, pars.id,
			&item->str_val, pars.dir_mode);
	for (unsigned int i = 0; i < lp.filters.size(); i++)
		filebrowser->addFilter(lp.filters[i]);
	targets.push_back(filebrowser);
	return filebrowser;
}

/* A masked entry is told which row shows it, so that it can say after an input
 * whether a value is set. Only this binding's keyboardinput has that. */
void CLuaMenu::setMaskedRow(CMenuTarget *target, CMenuForwarder *row)
{
	((CLuaMenuKeyboardinput *)target)->setRow(row);
}

/* One observer for the whole menu: the state a chooser is told in is the
 * item's, and is put on the row when it is made. */
CChangeObserver *CLuaMenu::observer(const ScriptMenuItemParams & /*pars*/)
{
	return observ;
}

CLuaMenuForwarder::CLuaMenuForwarder(lua_State *_L, std::string _luaAction, std::string _luaId)
{
	Init(_L, _luaAction, _luaId);
}

void CLuaMenuForwarder::Init(lua_State *_L, std::string _luaAction, std::string _luaId)
{
	L         = _L;
	luaAction = _luaAction;
	luaId     = _luaId;
}

CLuaMenuForwarder::~CLuaMenuForwarder()
{
}

int CLuaMenuForwarder::exec(CMenuTarget* /*parent*/, const std::string & /*actionKey*/)
{
	int res = menu_return::RETURN_REPAINT;
	if (!luaAction.empty()) {
		lua_pushglobaltable(L);
		lua_getfield(L, -1, luaAction.c_str());
		lua_remove(L, -2);
		lua_pushstring(L, luaId.c_str());
		int status = lua_pcall(L, 1 /* one arg */, 1 /* one result */, 0);
		if (status) {
			bool isString = lua_isstring(L,-1);
			const char *null = "NULL";
			fprintf(stderr, "[CLuaMenuForwarder::%s:%d] error in script: %s\n", __func__, __LINE__, isString ? lua_tostring(L, -1):null);
			DisplayErrorMessage(isString ? lua_tostring(L, -1):null, "Lua Script Error:");
		}
		if (lua_isnumber(L, -1))
			res = (int) lua_tonumber(L, -1);
		lua_pop(L, 1);
	}
	return res;
}

CLuaMenuFilebrowser::CLuaMenuFilebrowser(lua_State *_L, std::string _luaAction, std::string _luaId, std::string *_value, bool _dirMode) : CLuaMenuForwarder(_L, _luaAction, _luaId)
{
	Init(_value, _dirMode);
}

void CLuaMenuFilebrowser::Init(std::string *_value, bool _dirMode)
{
	value   = _value;
	dirMode = _dirMode;
	setValue(value);
}

int CLuaMenuFilebrowser::exec(CMenuTarget* /*parent*/, const std::string& /*actionKey*/)
{
	int res = menu_return::RETURN_REPAINT;
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

	if (!luaAction.empty()) {
		lua_pushglobaltable(L);
		lua_getfield(L, -1, luaAction.c_str());
		lua_remove(L, -2);
		lua_pushstring(L, luaId.c_str());
		lua_pushstring(L, value->c_str());
		int status = lua_pcall(L, 2 /* two arg */, 1 /* one result */, 0);
		if (status) {
			bool isString = lua_isstring(L,-1);
			const char *null = "NULL";
			fprintf(stderr, "[CLuaMenuFilebrowser::%s:%d] error in script: %s\n", __func__, __LINE__, isString ? lua_tostring(L, -1):null);
			DisplayErrorMessage(isString ? lua_tostring(L, -1):null, "Lua Script Error:");
		}
		if (lua_isnumber(L, -1))
			res = (int) lua_tonumber(L, -1);
		lua_pop(L, 1);
	}
	return res;
}

CLuaMenuStringinput::CLuaMenuStringinput(lua_State *_L, std::string _luaAction, std::string _luaId, const char *_name, std::string *_value, int _size, std::string _valid_chars, CChangeObserver *_observ, const char *_icon, bool _sms) : CLuaMenuForwarder(_L, _luaAction, _luaId)
{
	Init(_name, _value, _size, _valid_chars, _observ, _icon, _sms);
}

void CLuaMenuStringinput::Init(const char *_name, std::string *_value, int _size, std::string _valid_chars, CChangeObserver *_observ, const char *_icon, bool _sms)
{
	name        = _name;
	value       = _value;
	size        = _size;
	valid_chars = _valid_chars;
	icon        = _icon;
	observ      = _observ;
	sms         = _sms;
	setValue(value);
}

int CLuaMenuStringinput::exec(CMenuTarget* /*parent*/, const std::string & /*actionKey*/)
{
	int res = menu_return::RETURN_REPAINT;
	/* "sms" asked for the multi-tap letters on the digit keys; the input
	   dialog brings an on-screen keyboard for letters instead. */
	ScriptStringinput::params_t pars;
	pars.name        = name;
	pars.value       = *value;
	pars.size        = size;
	pars.valid_chars = valid_chars;
	pars.icon        = icon;
	pars.observ      = observ;
	*value = ScriptStringinput::exec(pars);
	if (!luaAction.empty()) {
		lua_pushglobaltable(L);
		lua_getfield(L, -1, luaAction.c_str());
		lua_remove(L, -2);
		lua_pushstring(L, luaId.c_str());
		lua_pushstring(L, value->c_str());
		int status = lua_pcall(L, 2 /* two arg */, 1 /* one result */, 0);
		if (status) {
			bool isString = lua_isstring(L,-1);
			const char *null = "NULL";
			fprintf(stderr, "[CLuaMenuStringinput::%s:%d] error in script: %s\n", __func__, __LINE__, isString ? lua_tostring(L, -1):null);
			DisplayErrorMessage(isString ? lua_tostring(L, -1):null, "Lua Script Error:");
		}
		if (lua_isnumber(L, -1))
			res = (int) lua_tonumber(L, -1);
		lua_pop(L, 1);
	}
	return res;
}

CLuaMenuKeyboardinput::CLuaMenuKeyboardinput(lua_State *_L, std::string _luaAction, std::string _luaId, const char *_name, std::string *_value, int _size, CChangeObserver *_observ, const char *_icon, std::string _help, std::string _help2, bool _masked) : CLuaMenuForwarder(_L, _luaAction, _luaId)
{
	Init(_name, _value, _size, _observ, _icon, _help, _help2, _masked);
}

void CLuaMenuKeyboardinput::Init(const char *_name, std::string *_value, int _size, CChangeObserver *_observ, const char *_icon, std::string _help, std::string _help2, bool _masked)
{
	name   = _name;
	value  = _value;
	size   = _size;
	masked = _masked;
	row    = NULL;
	icon   = _icon;
	observ = _observ;
	help   = _help;
	help2  = _help2;
	// a masked row never falls back to the value, it is told what to show
	if (!masked)
		setValue(value);
}

int CLuaMenuKeyboardinput::exec(CMenuTarget* /*parent*/, const std::string & /*actionKey*/)
{
	int res = menu_return::RETURN_REPAINT;
	CKeyboardInput *i;
	i = new CKeyboardInput((char *)name, value, size, observ, icon, help, help2);
	i->setMasked(masked);
	i->exec(NULL, "");
	delete i;
	if (masked && row)
		row->setOption(ScriptMenu::maskedText(*value));
	if (!luaAction.empty()) {
		lua_pushglobaltable(L);
		lua_getfield(L, -1, luaAction.c_str());
		lua_remove(L, -2);
		lua_pushstring(L, luaId.c_str());
		lua_pushstring(L, value->c_str());
		int status = lua_pcall(L, 2 /* two arg */, 1 /* one result */, 0);
		if (status) {
			bool isString = lua_isstring(L,-1);
			const char *null = "NULL";
			fprintf(stderr, "[CLuaMenuKeyboardinput::%s:%d] error in script: %s\n", __func__, __LINE__, isString ? lua_tostring(L, -1):null);
			DisplayErrorMessage(isString ? lua_tostring(L, -1):null, "Lua Script Error:");
		}
		if (lua_isnumber(L, -1))
			res = (int) lua_tonumber(L, -1);
		lua_pop(L, 1);
	}
	return res;
}

int CLuaInstMenu::MenuNew(lua_State *L)
{
	/* The parameters a script names in its table. What is done with them is
	 * done by the script API, so that both bindings ask for one menu and
	 * cannot come to differ. */
	ScriptMenu::params_t pars;
	pars.width_given = false;
	pars.width = 0;

	if (lua_istable(L, 1)) {
		tableLookup(L, "name", pars.name) || tableLookup(L, "title", pars.name);
		tableLookup(L, "icon", pars.icon);

		lua_Integer mwidth;
		if (tableLookup(L, "mwidth", mwidth)) {
			pars.width_given = true;
			pars.width = (int)mwidth;
		}

		/* The footer's buttons, each with the icon of the key it belongs
		 * to, in the order the footer shows them. */
		static const struct { const char *key; const char *icon; } btn_keys[] = {
			{ "btnRed",    NEUTRINO_ICON_BUTTON_RED },
			{ "btnGreen",  NEUTRINO_ICON_BUTTON_GREEN },
			{ "btnYellow", NEUTRINO_ICON_BUTTON_YELLOW },
			{ "btnBlue",   NEUTRINO_ICON_BUTTON_BLUE },
			{ NULL, NULL }
		};

		for (int i = 0; btn_keys[i].key != NULL; i++) {
			std::string label = "";
			if (!tableLookup(L, btn_keys[i].key, label) || label.empty())
				continue;
			button_label_cc b;
			b.button = btn_keys[i].icon;
			b.text = label;
			pars.buttons.push_back(b);
		}
	}

	CLuaMenu **udata = (CLuaMenu **) lua_newuserdata(L, sizeof(CLuaMenu *));
	*udata = new CLuaMenu(pars);
	luaL_getmetatable(L, "menu");
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstMenu::MenuAddKey(lua_State *L)
{
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) return 0;
	lua_assert(lua_istable(L, 2));

	std::string action;				tableLookup(L, "action", action);
	std::string id;					tableLookup(L, "id", id);
	lua_Unsigned directkey = CRCInput::RC_nokey;	tableLookup(L, "directkey", directkey);
	if ((!action.empty()) && (directkey != CRCInput::RC_nokey)) {
		CLuaMenuForwarder *forwarder = new CLuaMenuForwarder(L, action, id);
		D->m->addKey(directkey, forwarder, action);
		D->targets.push_back(forwarder);
	}
	return 0;
}

int CLuaInstMenu::MenuAddItem(lua_State *L)
{
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) {
		lua_pushnil(L);
		return 1;
	}
	lua_assert(lua_istable(L, 2));

	CMenuItem *mi = NULL;
	ScriptMenuItem *b = D->add();

	tableLookup(L, "name", b->name);
	std::string type; tableLookup(L, "type", type);
	b->type = type;

	/* The types that are the menu's own are the same for both bindings and
	 * are built where the menu is; what is left here is the item a script
	 * names of its own. */
	int row_id = 0;
	if (!D->addIfPlain(type, b->name)) {
		/* What a script names of its own, read out of its table. The list a
		 * chooser is built from is read into the item; the row itself is
		 * built where the menu is, out of what is read here. */
		lua_item_params pars;
		pars.L = L;
		tableLookup(L, "id", pars.id);
		tableLookup(L, "action", pars.action);
		tableLookup(L, "value", pars.value);
		tableLookup(L, "icon", pars.icon);
		tableLookup(L, "right_icon", pars.right_icon);
		tableLookup(L, "hint", pars.hint);
		tableLookup(L, "hint_icon", pars.hint_icon);
		tableLookup(L, "range", pars.range);
		tableLookup(L, "help", pars.help);
		tableLookup(L, "help2", pars.help2);

		/* the characters a stringinput takes, unless a script narrows them */
		std::string valid_chars = "abcdefghijklmnopqrstuvwxyz0123456789!\"Ã‚Â§$%&/()=?-.@,_: ";
		tableLookup(L, "valid_chars", valid_chars);
		pars.valid_chars = valid_chars;

		pars.enabled = true;
		if (!(tableLookup(L, "enabled", pars.enabled) || tableLookup(L, "active", pars.enabled))) {
			std::string tmp = "true";
			if (tableLookup(L, "enabled", tmp) || tableLookup(L, "active", tmp))
				paramBoolDeprecated(L, tmp.c_str());
			pars.enabled = (tmp == "true" || tmp == "1" || tmp == "yes");
		}

		lua_Unsigned directkey = CRCInput::RC_nokey;
		tableLookup(L, "directkey", directkey);
		pars.directkey = directkey;

		lua_Integer pulldown = 0;
		tableLookup(L, "pulldown", pulldown);
		pars.pulldown = (pulldown != 0);

		lua_Integer size = (type == "stringinput") ? 30 : 0;
		tableLookup(L, "size", size);
		pars.size = (int)size;

		lua_Integer sms = 0;
		tableLookup(L, "sms", sms);
		pars.sms = (sms != 0);

		lua_Integer masked = 0;
		if (tableLookup(L, "masked", masked))
			pars.masked = (masked != 0);
		else
			tableLookup(L, "masked", pars.masked);

		lua_Integer dir_mode = 0;
		tableLookup(L, "dir_mode", dir_mode);
		pars.dir_mode = (dir_mode != 0);

		if (type == "chooser") {
			/* the choices the script wrote: a value to each text */
			lua_pushstring(L, "options");
			lua_gettable(L, -2);
			if (lua_istable(L, -1)) {
				for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 2)) {
					lua_pushvalue(L, -2);
					const char *key = lua_tostring(L, -1);
					const char *val = lua_tostring(L, -2);
					b->options.push_back(std::pair<int, std::string>(atoi(key), val ? val : "ERROR"));
				}
			}
			lua_pop(L, 1);
		} else if (type == "filebrowser") {
			/* and the names it takes, where it named any */
			lua_pushstring(L, "filter");
			lua_gettable(L, -2);
			if (lua_istable(L, -1)) {
				for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 2)) {
					lua_pushvalue(L, -2);
					const char *val = lua_tostring(L, -2);
					if (val)
						pars.filters.push_back(val);
				}
			}
			lua_pop(L, 1);
		}

		mi = D->makeRow(b, pars, D);

		/* What a row of this menu is told of the script, which is the one
		 * thing the script API cannot say for it. */
		if (mi)
			mi->setLua(L, pars.action, pars.id);

		row_id = D->addRow(b, mi, pars);
	}
	if (row_id > 0)
		lua_pushinteger(L, row_id);
	else
		lua_pushnil(L);

	return 1;
}

int CLuaInstMenu::MenuExec(lua_State *L)
{
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) return 0;
	CLuaInstance::scriptUiOpen();
	D->m->exec(NULL, "");
	D->m->hide();
	CLuaInstance::scriptUiClose();
	return 0;
}

int CLuaInstMenu::MenuHide(lua_State *L)
{
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) return 0;
	D->m->hide();
	return 0;
}

int CLuaInstMenu::MenuSetActive(lua_State *L)
{
	lua_assert(lua_istable(L, 2));
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) return 0;

	lua_Integer id;	tableLookup(L, "item", id);
	bool activ;	tableLookup(L, "activ", activ);

	ScriptMenuItem *item = D->item((int)id);
	if (item)
		item->mi->setActive(activ);
	return 0;
}

int CLuaInstMenu::MenuSetName(lua_State *L)
{
	lua_assert(lua_istable(L, 2));
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) return 0;

	lua_Integer id;		tableLookup(L, "item", id);
	std::string name;	tableLookup(L, "name", name);

	ScriptMenuItem *item = D->item((int)id);
	if (item)
		item->mi->setName(name);
	return 0;
}

int CLuaInstMenu::MenuDelete(lua_State *L)
{
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) return 0;

	while (!D->targets.empty()) {
		delete D->targets.back();
		D->targets.pop_back();
	}

	delete D;
	return 0;
}

int CLuaInstMenu::MenuSetSelected(lua_State *L)
{
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) return 0;
	lua_assert(lua_istable(L, 2));

	lua_Integer preselected;	tableLookup(L, "preselected", preselected);

	D->m->setSelected(preselected);
	return 0;
}

int CLuaInstMenu::MenuGetSelected(lua_State *L)
{
	CLuaMenu *D = MenuCheck(L, 1);

	if (!D) {
		lua_pushnil(L);
		return 1;
	}

	lua_pushinteger(L, D->m->getSelected());
	return 1;
}

int CLuaInstMenu::MenuSetValue(lua_State *L)
{
	lua_assert(lua_istable(L, 2));
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) return 0;

	lua_Integer id;		tableLookup(L, "item", id);
	std::string value;	tableLookup(L, "value", value);

	ScriptMenuItem *item = D->item((int)id);
	if (!item)
		return 0;

	/*
	 * setOption() belongs to CMenuForwarder and the cast to it cannot be
	 * checked at runtime, this directory is built with -fno-rtti. The
	 * chooser family are siblings of CMenuForwarder, not subclasses, so
	 * casting one of them would write through a pointer to an unrelated
	 * type. Ask the item itself which family it belongs to.
	 */
	if (!item->mi->isMenueOptionChooser()) {
		/*
		 * Set the value the entry edits, so that its input opens with it.
		 * Without a text of its own the row shows that value through the
		 * entry's target and follows every later input; a masked row is
		 * told whether a value is set, never the value.
		 */
		item->str_val = value;
		static_cast<CMenuForwarder*>(item->mi)->setOption(item->masked ? ScriptMenu::maskedText(value) : "");
		return 0;
	}

	/*
	 * A chooser paints from the value this binding owns and handed to the
	 * widget as a pointer, so writing it here does what setOption() would
	 * do. The change observer is deliberately left alone: this corrects
	 * the script's own display, it is not a user interaction, and
	 * notifying would call back into the script.
	 */
	if (item->type == "chooser") {
		for (unsigned int i = 0; i < item->options.size(); i++) {
			if (item->options[i].second == value) {
				item->int_val = item->options[i].first;
				return 0;
			}
		}
		fprintf(stderr, "[CLuaInstMenu::%s:%d] setValue: no option named '%s'\n", __func__, __LINE__, value.c_str());
	} else if (item->type == "numeric") {
		item->int_val = atoi(value.c_str());
	} else if (item->type == "string") {
		item->str_val = value;
	} else {
		fprintf(stderr, "[CLuaInstMenu::%s:%d] setValue: unhandled item type '%s'\n", __func__, __LINE__, item->type.c_str());
	}
	return 0;
}

int CLuaInstMenu::MenuPaintItem(lua_State *L)
{
	lua_assert(lua_istable(L, 2));
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) return 0;

	lua_Integer id;		tableLookup(L, "item", id);

	ScriptMenuItem *item = D->item((int)id);
	if (item)
		item->mi->paint();
	return 0;
}
