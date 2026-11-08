/*
 * lua components window
 *
 * (C) 2014-2015 M. Liebmann (micha-bbg)
 * (C) 2014 Thilo Graf (dbt)
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

#include <system/debug.h>
#include <gui/components/cc.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_cc_window.h"

#if LUA_VERSION_NUM >= 503 /* Lua 5.3 */

#ifndef luaL_checkint
#define luaL_checkint luaL_checkinteger
#endif

#endif /* Lua 5.3 */

CLuaInstCCWindow* CLuaInstCCWindow::getInstance()
{
	static CLuaInstCCWindow* LuaInstCCWindow = NULL;

	if(!LuaInstCCWindow)
		LuaInstCCWindow = new CLuaInstCCWindow();
	return LuaInstCCWindow;
}

void CLuaInstCCWindow::CCWindowRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",            CLuaInstCCWindow::CCWindowNew },
		{ "paint",          CLuaInstCCWindow::CCWindowPaint },
		{ "hide",           CLuaInstCCWindow::CCWindowHide },
		{ "setCaption",     CLuaInstCCWindow::CCWindowSetCaption },
		{ "setWindowColor", CLuaInstCCWindow::CCWindowSetWindowColor },
		{ "paintHeader",    CLuaInstCCWindow::CCWindowPaintHeader },
		{ "headerHeight",   CLuaInstCCWindow::CCWindowGetHeaderHeight },
		{ "footerHeight",   CLuaInstCCWindow::CCWindowGetFooterHeight },
		{ "header_height",  CLuaInstCCWindow::CCWindowGetHeaderHeight_dep }, /* function 'header_height' is deprecated */
		{ "footer_height",  CLuaInstCCWindow::CCWindowGetFooterHeight_dep }, /* function 'footer_height' is deprecated */
		{ "setCenterPos",   CLuaInstCCWindow::CCWindowSetCenterPos },
		{ "setDimensionsAll", CLuaInstCCWindow::CCWindowSetDimensionsAll },
		{ "setBodyImage",   CCWindowSetBodyImage },
		{ "__gc",           CLuaInstCCWindow::CCWindowDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "cwindow");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "cwindow");
}

int CLuaInstCCWindow::CCWindowNew(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	/* The parameters a script names in its table. What is done with them is
	 * done by the script API, so that both bindings ask for one window and
	 * cannot come to differ. */
	ScriptWindow::params_t pars;
	pars.icon = std::string(NEUTRINO_ICON_INFO);
	pars.x = 100;
	pars.y = 100;
	pars.dx = 450;
	pars.dy = 250;
	pars.shadow = CC_SHADOW_OFF;
	pars.color_frame = (unsigned long)COL_FRAME_PLUS_0;
	pars.color_body = (unsigned long)COL_MENUCONTENT_PLUS_0;
	pars.color_shadow = (unsigned long)COL_SHADOW_PLUS_0;
	pars.show_header = true;
	pars.show_footer = true;
	std::string tmp1 = "false";

	/* The dimensions come out of the table as an integer and are then kept as
	 * the int the components want. */
	lua_Integer x = (lua_Integer)pars.x, y = (lua_Integer)pars.y;
	lua_Integer dx = (lua_Integer)pars.dx, dy = (lua_Integer)pars.dy;
	tableLookup(L, "x", x);
	tableLookup(L, "y", y);
	tableLookup(L, "dx", dx);
	tableLookup(L, "dy", dy);
	pars.x = (int)x;
	pars.y = (int)y;
	pars.dx = (int)dx;
	pars.dy = (int)dy;

	tableLookup(L, "name", pars.name) || tableLookup(L, "title", pars.name) || tableLookup(L, "caption", pars.name);
	tableLookup(L, "icon", pars.icon);

	lua_Integer has_shadow = (lua_Integer)pars.shadow;
	if (!tableLookup(L, "has_shadow", has_shadow)) {
		tmp1 = "false";
		if (tableLookup(L, "has_shadow", tmp1))
			paramBoolDeprecated(L, tmp1.c_str());
		if ((tmp1 == "true" || tmp1 == "1" || tmp1 == "yes"))
			has_shadow = CC_SHADOW_ON;
	}
	pars.shadow = (int)has_shadow;

	lua_Unsigned color_frame  = (lua_Unsigned)pars.color_frame;
	lua_Unsigned color_body   = (lua_Unsigned)pars.color_body;
	lua_Unsigned color_shadow = (lua_Unsigned)pars.color_shadow;
	tableLookup(L, "color_frame" , color_frame);
	tableLookup(L, "color_body"  , color_body);
	tableLookup(L, "color_shadow", color_shadow);
	pars.color_frame  = (unsigned long)color_frame;
	pars.color_body   = (unsigned long)color_body;
	pars.color_shadow = (unsigned long)color_shadow;

	/* The labels of the footer's buttons, each with the icon of the key it
	 * belongs to, in the order the footer shows them. */
	static const struct { const char *key; const char *icon; } btn_keys[] = {
		{ "btnRed",       NEUTRINO_ICON_BUTTON_RED },
		{ "btnGreen",     NEUTRINO_ICON_BUTTON_GREEN },
		{ "btnYellow",    NEUTRINO_ICON_BUTTON_YELLOW },
		{ "btnBlue",      NEUTRINO_ICON_BUTTON_BLUE },
		{ "btn0",         NEUTRINO_ICON_BUTTON_0 },
		{ "btn1",         NEUTRINO_ICON_BUTTON_1 },
		{ "btn2",         NEUTRINO_ICON_BUTTON_2 },
		{ "btn3",         NEUTRINO_ICON_BUTTON_3 },
		{ "btn4",         NEUTRINO_ICON_BUTTON_4 },
		{ "btn5",         NEUTRINO_ICON_BUTTON_5 },
		{ "btn6",         NEUTRINO_ICON_BUTTON_6 },
		{ "btn7",         NEUTRINO_ICON_BUTTON_7 },
		{ "btn8",         NEUTRINO_ICON_BUTTON_8 },
		{ "btn9",         NEUTRINO_ICON_BUTTON_9 },
		{ "btnPlay",      NEUTRINO_ICON_BUTTON_PLAY },
		{ "btnPlayPause", NEUTRINO_ICON_BUTTON_PLAY },
		{ "btnOk",        NEUTRINO_ICON_BUTTON_OKAY },
		{ "btnSetup",     NEUTRINO_ICON_BUTTON_MENU },
		{ NULL, NULL }
	};

	for (int i = 0; btn_keys[i].key != NULL; i++)
	{
		std::string label = "";
		if (!tableLookup(L, btn_keys[i].key, label) || label.empty())
			continue;
		ScriptWindow::button_t btn;
		btn.button = btn_keys[i].icon;
		btn.text = label;
		pars.buttons.push_back(btn);
	}

	if (!tableLookup(L, "show_header", pars.show_header)) {
		tmp1 = "true";
		if (tableLookup(L, "show_header", tmp1))
			paramBoolDeprecated(L, tmp1.c_str());
		pars.show_header = (tmp1 == "true" || tmp1 == "1" || tmp1 == "yes");
	}
	if (!tableLookup(L, "show_footer", pars.show_footer)) {
		tmp1 = "true";
		if (tableLookup(L, "show_footer", tmp1))
			paramBoolDeprecated(L, tmp1.c_str());
		pars.show_footer = (tmp1 == "true" || tmp1 == "1" || tmp1 == "yes");
	}

	CLuaCCWindow **udata = (CLuaCCWindow **) lua_newuserdata(L, sizeof(CLuaCCWindow *));
	*udata = new CLuaCCWindow();
	(*udata)->sw = new ScriptWindow(pars);

	luaL_getmetatable(L, "cwindow");
	lua_setmetatable(L, -2);
	return 1;
}

CLuaCCWindow *CLuaInstCCWindow::CCWindowCheck(lua_State *L, int n)
{
	return *(CLuaCCWindow **) luaL_checkudata(L, n, "cwindow");
}

int CLuaInstCCWindow::CCWindowPaint(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;

	bool do_save_bg = true;
	if (!tableLookup(L, "do_save_bg", do_save_bg)) {
		std::string tmp = "true";
		if (tableLookup(L, "do_save_bg", tmp))
			paramBoolDeprecated(L, tmp.c_str());
		do_save_bg = (tmp == "true" || tmp == "1" || tmp == "yes");
	}
	D->sw->paint(do_save_bg);
	return 0;
}

int CLuaInstCCWindow::CCWindowHide(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;

	bool tmp1 = false;
	std::string tmp2 = "false";
	if ((tableLookup(L, "no_restore", tmp1)) || (tableLookup(L, "no_restore", tmp2)))
		obsoleteHideParameter(L);

	D->sw->hide();
	return 0;
}

int CLuaInstCCWindow::CCWindowSetCaption(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;

	std::string name = "";
	tableLookup(L, "name", name) || tableLookup(L, "title", name) || tableLookup(L, "caption", name);

	lua_Integer alignment = (lua_Integer)DEFAULT_TITLE_ALIGN;
	tableLookup(L, "alignment", alignment);

	D->sw->setCaption(name, (int)alignment);
	return 0;
}

int CLuaInstCCWindow::CCWindowSetWindowColor(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;

	/* A colour named as one of the box's own carries the magic the colour
	 * tables are built with, and the script API resolves it, so that both
	 * bindings resolve it the same way. */
	lua_Unsigned color = 0;
	if (tableLookup(L, "color_frame"  , color))
		D->sw->setColorFrame((unsigned long)color);
	if (tableLookup(L, "color_body"  , color))
		D->sw->setColorBody((unsigned long)color);
	if (tableLookup(L, "color_shadow"  , color))
		D->sw->setColorShadow((unsigned long)color);

	return 0;
}

int CLuaInstCCWindow::CCWindowPaintHeader(lua_State *L)
{
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;

	D->sw->paintHeader();
	return 0;
}

// function 'header_height' is deprecated
int CLuaInstCCWindow::CCWindowGetHeaderHeight_dep(lua_State *L)
{
	functionDeprecated(L, "header_height", "headerHeight");
	return CCWindowGetHeaderHeight(L);
}

// function 'footer_height' is deprecated
int CLuaInstCCWindow::CCWindowGetFooterHeight_dep(lua_State *L)
{
	functionDeprecated(L, "footer_height", "footerHeight");
	return CCWindowGetFooterHeight(L);
}

int CLuaInstCCWindow::CCWindowGetHeaderHeight(lua_State *L)
{
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;

	lua_pushinteger(L, D->sw->headerHeight());
	return 1;
}

int CLuaInstCCWindow::CCWindowGetFooterHeight(lua_State *L)
{
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;

	lua_pushinteger(L, D->sw->footerHeight());
	return 1;
}

int CLuaInstCCWindow::CCWindowSetDimensionsAll(lua_State *L)
{
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;
	lua_Integer x = luaL_checkint(L, 2);
	lua_Integer y = luaL_checkint(L, 3);
	lua_Integer w = luaL_checkint(L, 4);
	lua_Integer h = luaL_checkint(L, 5);
	/* Keeping the window on the screen is not something a binding should
	 * decide for itself, so it is the script API that decides it. */
	D->sw->setDimensionsAll((int)x, (int)y, (int)w, (int)h);
	return 0;
}

int CLuaInstCCWindow::CCWindowSetCenterPos(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;
	lua_Integer  tmp_along_mode, along_mode = CC_ALONG_X | CC_ALONG_Y;
	tableLookup(L, "along_mode", tmp_along_mode);

	if (tmp_along_mode & CC_ALONG_X || tmp_along_mode & CC_ALONG_Y)
		along_mode=tmp_along_mode;

	D->sw->setCenterPos((int)along_mode);
	return 0;
}

int CLuaInstCCWindow::CCWindowSetBodyImage(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;

	std::string image = "";
	if (lua_istable(L, -1))
		tableLookup(L, "image_path", image);

	D->sw->setBodyImage(image);
	return 0;
}

int CLuaInstCCWindow::CCWindowDelete(lua_State *L)
{
	LUA_DEBUG("CLuaInstCCWindow::%s %d\n", __func__, lua_gettop(L));
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}
