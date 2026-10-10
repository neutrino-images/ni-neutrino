/*
	$port: user_menue_setup.cpp,v 1.1 2010/07/30 20:52:31 tuxbox-cvs Exp $

	user_menue setup implementation - Neutrino-GUI
	based up implementation by Günther

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2010 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

	Copyright (C) 2014 Stefan Seyfried

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <driver/rcinput.h>
#include "user_menue_setup.h"

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>
#include <gui/plugins.h>
#include <gui/widget/icons.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/keyboard_input.h>
#include <gui/widget/keychooser.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/settingitem.h>

#include <driver/screen_max.h>

#include <system/helpers.h>
#include <system/debug.h>

#include <coreapi/settings/settings.h>

#include <map>
#include <utility>

static bool usermenu_show = true;
static bool usermenu_show_cam = true; // FIXME -- use hwcaps?
//static bool usermenu_hide = false;

struct keyvals
{
	const int key;
	const neutrino_locale_t value;
	bool &show;
};

static keyvals usermenu_items[] =
{
	{ SNeutrinoSettings::ITEM_NONE,			LOCALE_USERMENU_ITEM_NONE,		usermenu_show },
	{ SNeutrinoSettings::ITEM_BAR,			LOCALE_USERMENU_ITEM_BAR,		usermenu_show },
	{ SNeutrinoSettings::ITEM_EVENTLIST,		LOCALE_EPGMENU_EVENTLIST,		usermenu_show },
	{ SNeutrinoSettings::ITEM_EPGPLUS,		LOCALE_EPGMENU_EPGPLUS,			usermenu_show },
	{ SNeutrinoSettings::ITEM_EVENTINFO,		LOCALE_EPGMENU_EVENTINFO,		usermenu_show },
	{ SNeutrinoSettings::ITEM_EPG_MISC,		LOCALE_USERMENU_ITEM_EPG_MISC,		usermenu_show },
	{ SNeutrinoSettings::ITEM_AUDIO_SELECT,		LOCALE_AUDIOSELECTMENUE_HEAD,		usermenu_show },
	{ SNeutrinoSettings::ITEM_SUBCHANNEL,		LOCALE_NVODSELECTOR_HEAD,		usermenu_show },
	{ SNeutrinoSettings::ITEM_RECORD,		LOCALE_TIMERLIST_TYPE_RECORD,		usermenu_show },
	{ SNeutrinoSettings::ITEM_TIMESHIFT,		LOCALE_RECORDINGMENU_TIMESHIFT,		usermenu_show },
	{ SNeutrinoSettings::ITEM_MOVIEBROWSER,		LOCALE_MOVIEBROWSER_HEAD,		usermenu_show },
	{ SNeutrinoSettings::ITEM_TIMERLIST,		LOCALE_TIMERLIST_NAME,			usermenu_show },
	{ SNeutrinoSettings::ITEM_VTXT,			LOCALE_USERMENU_ITEM_VTXT,		usermenu_show },
	{ SNeutrinoSettings::ITEM_FAVORITES,		LOCALE_FAVORITES_MENUEADD,		usermenu_show },
	{ SNeutrinoSettings::ITEM_STREAMINFO,		LOCALE_EPGMENU_STREAMINFO,		usermenu_show },
	{ SNeutrinoSettings::ITEM_IMAGEINFO,		LOCALE_SERVICEMENU_IMAGEINFO,		usermenu_show },
	{ SNeutrinoSettings::ITEM_BOXINFO,		LOCALE_EXTRA_DBOXINFO,			usermenu_show },
	{ SNeutrinoSettings::ITEM_RCLOCK,		LOCALE_RCLOCK_TITLE,			usermenu_show },
	{ SNeutrinoSettings::ITEM_CAM,			LOCALE_CI_SETTINGS,			usermenu_show_cam },
	{ SNeutrinoSettings::ITEM_CLOCK,		LOCALE_CLOCK_SWITCH_ON,			usermenu_show },
	{ SNeutrinoSettings::ITEM_ECMINFO,		LOCALE_ECMINFO_SHOW,			usermenu_show },
	{ SNeutrinoSettings::ITEM_CAMD_RESET,		LOCALE_CAMD_RESET,			usermenu_show },
	{ SNeutrinoSettings::ITEM_INFOICONS,		LOCALE_INFOICONS_SWITCH_ON,		usermenu_show },
	{ SNeutrinoSettings::ITEM_FILEPLAY_VIDEO,	LOCALE_MOVIEPLAYER_FILEPLAYBACK_VIDEO,	usermenu_show },
	{ SNeutrinoSettings::ITEM_FILEPLAY_AUDIO,	LOCALE_MOVIEPLAYER_FILEPLAYBACK_AUDIO,	usermenu_show },
	{ SNeutrinoSettings::ITEM_AUDIOPLAYER,		LOCALE_AUDIOPLAYER_NAME,		usermenu_show },
	{ SNeutrinoSettings::ITEM_INETPLAYER,		LOCALE_INETRADIO_NAME,			usermenu_show },
	{ SNeutrinoSettings::ITEM_TUNER_RESTART,	LOCALE_SERVICEMENU_RESTART_TUNER,	usermenu_show },
	{ SNeutrinoSettings::ITEM_HDDMENU,		LOCALE_HDD_SETTINGS,			usermenu_show },
	{ SNeutrinoSettings::ITEM_NETSETTINGS,		LOCALE_MAINSETTINGS_NETWORK,		usermenu_show },
	{ SNeutrinoSettings::ITEM_LCD4LINUX,		LOCALE_LCD4L_SUPPORT,			usermenu_show },
	{ SNeutrinoSettings::ITEM_SWUPDATE,		LOCALE_SERVICEMENU_UPDATE,		usermenu_show },
	{ SNeutrinoSettings::ITEM_LIVESTREAM_RESOLUTION, LOCALE_LIVESTREAM_RESOLUTION,		usermenu_show },
	{ SNeutrinoSettings::ITEM_ADZAP,		LOCALE_USERMENU_ITEM_ADZAP,		usermenu_show },
	{ SNeutrinoSettings::ITEM_TESTMENU,		LOCALE_TESTMENU,			usermenu_show },
	{ SNeutrinoSettings::ITEM_GAMES,		LOCALE_MAINMENU_GAMES,			usermenu_show },
	{ SNeutrinoSettings::ITEM_TOOLS,		LOCALE_MAINMENU_TOOLS,			usermenu_show },
	{ SNeutrinoSettings::ITEM_SCRIPTS,		LOCALE_MAINMENU_SCRIPTS,		usermenu_show },
	{ SNeutrinoSettings::ITEM_LUA,			LOCALE_MAINMENU_LUA,			usermenu_show },
	{ SNeutrinoSettings::ITEM_PLUGIN_TYPES,		LOCALE_USERMENU_ITEM_PLUGIN_TYPES,	usermenu_show },
	{ SNeutrinoSettings::ITEM_MAX,			NONEXISTANT_LOCALE,			usermenu_show }
};

CUserMenuSetup::CUserMenuSetup(neutrino_locale_t menue_title, int menue_button)
{
	local = menue_title;
	button = menue_button;
	max_char = 24;
	width = 40;
	if (menue_button < (int) g_settings.usermenu.size())
		pref_name = g_settings.usermenu[button]->title; // set current button name as prefered name
	forwarder = NULL;
	edit_key = CRCInput::RC_nokey;
	key_item = NULL;
	refused = false;

	for (int i = 0; usermenu_items[i].key != SNeutrinoSettings::ITEM_MAX; i++)
	{
		const char *loc = g_Locale->getText(usermenu_items[i].value);
		if (usermenu_items[i].show)
			options.push_back(loc);
		keys[loc] = to_string(usermenu_items[i].key);
		vals[keys[loc]] = loc;
	}

	int number_of_plugins = g_Plugins->getNumberOfPlugins();
	for (int count = 0; count < number_of_plugins; count++)
	{
		const char *loc = g_Plugins->getName(count);
		const char *key = g_Plugins->getFileName(count);
		if (loc && *loc && key && *key)
		{
			options.push_back(loc);
			keys[loc] = key;
			vals[keys[loc]] = loc;
		}
	}
	std::sort(options.begin(), options.end());
}

CUserMenuSetup::~CUserMenuSetup()
{
}

int CUserMenuSetup::exec(CMenuTarget *parent, const std::string &actionKey)
{
	if (actionKey == ">n")
		return editTitle(parent);
	if (actionKey == ">k")
		return editKey(parent);

	if (actionKey == ">d")
	{
		int selected = ums->getSelected();
		if (selected >= item_offset)
		{
			if (parent)
				parent->hide();
			ums->removeItem(selected);
			ums->hide();
			return menu_return::RETURN_REPAINT;
		}
		return menu_return::RETURN_NONE;
	}

	if (parent)
		parent->hide();

	if (actionKey == ">a")
	{
		int selected = ums->getSelected();
		CMenuOptionStringChooser *c = new CMenuOptionStringChooser(std::string(""), NULL, true, NULL, CRCInput::RC_nokey, NULL, true);
		c->setOptions(options);
		std::string n(g_Locale->getText(LOCALE_USERMENU_ITEM_NONE));
		c->setOptionValue(n);
		if (selected >= item_offset)
			ums->insertItem(selected, c);
		else
			ums->addItem(c);
		ums->hide();
		return menu_return::RETURN_REPAINT;
	}

	int res = showSetup();
	// One hint is enough; the name is checked again on the next visit.
	if (!refused)
		checkButtonName();

	return res;
}

static neutrino_locale_t locals[SNeutrinoSettings::ITEM_MAX];
neutrino_locale_t CUserMenuSetup::getLocale(unsigned int key)
{
	if (key >= SNeutrinoSettings::ITEM_MAX)
	{
		key = SNeutrinoSettings::ITEM_NONE;
	}

	static bool initialized = false;
	if (!initialized)
	{
		initialized = true;
		for (int i = 0; i < SNeutrinoSettings::ITEM_MAX; i++)
			locals[i] = NONEXISTANT_LOCALE;
		for (int i = 0; usermenu_items[i].key != SNeutrinoSettings::ITEM_MAX; i++)
			locals[usermenu_items[i].key] = usermenu_items[i].value;
	}
	return locals[key];
}

int CUserMenuSetup::showSetup()
{
	mn_widget_id_t widget_id = (button < SNeutrinoSettings::BUTTON_MAX) ? MN_WIDGET_ID_USERMENU_RED + button : NO_WIDGET_ID;
	ums = new CMenuWidget(local, NEUTRINO_ICON_KEYBINDING, width, widget_id);

	ums->addIntroItems();

	ums->addItem(new CMenuForwarder(LOCALE_USERMENU_NAME, true, NULL, this, ">n"));

	key_item = NULL;
	if (button >= SNeutrinoSettings::BUTTON_MAX)
	{
		key_item = new CMenuForwarder(LOCALE_USERMENU_KEY, true, CRCInput::getKeyNameC(g_settings.usermenu[button]->key), this, ">k");
		ums->addItem(key_item);
	}

	ums->addItem(new CMenuSeparator(CMenuSeparator::STRING | CMenuSeparator::LINE, LOCALE_USERMENU_ITEMS));

	std::vector<std::string> items = ::split(settingsText(g_settings.usermenu[button]->items), ',');
	std::vector<std::string> shown;
	std::map<const CMenuItem *, std::string> unnamed;
	item_offset = ums->getItemsCount();
	for (std::vector<std::string>::iterator it = items.begin(); it != items.end(); ++it)
	{
		CMenuOptionStringChooser *c = new CMenuOptionStringChooser(std::string(""), NULL, true, NULL, CRCInput::RC_nokey, NULL, true);
		c->setTitle(LOCALE_USERMENU_ITEMS);
		c->setOptions(options);
		c->setOptionValue(vals[*it]);
		ums->addItem(c);
		shown.push_back(c->getOptionValue());
		if (shown.back().empty())
			unnamed[c] = *it;
	}

	const struct button_label footerButtons[2] =
	{
		{ NEUTRINO_ICON_BUTTON_RED, LOCALE_BOUQUETEDITOR_DELETE },
		{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_BOUQUETEDITOR_ADD }
	};
	ums->setFooter(footerButtons, 2);
	ums->addKey(CRCInput::RC_red, this, ">d");
	ums->addKey(CRCInput::RC_green, this, ">a");

	int res = ums->exec(NULL, "");
	int items_end = ums->getItemsCount();

	const char *delim = "";
	std::string chosen;
	std::vector<std::string> left;
	std::string none = to_string(SNeutrinoSettings::ITEM_NONE);
	for (int count = item_offset; count < items_end; count++)
	{
		CMenuItem *item = ums->getItem(count);
		const std::string value = static_cast<CMenuOptionStringChooser *>(item)->getOptionValue();
		left.push_back(value);
		std::string lk = keys[value];
		// An entry no option names, such as a plugin not installed now, is kept as it was.
		if (value.empty() && unnamed.count(item))
			lk = unnamed[item];
		if (lk == none || lk.empty())
			continue;
		chosen += delim + lk;
		delim = ",";
	}

	delete ums;
	key_item = NULL;

	// A list left as shown is not written: that keeps a write made elsewhere and entries no option names.
	refused = left != shown && !writeMember(2, chosen, true);

	return res;
}

int CUserMenuSetup::editTitle(CMenuTarget *parent)
{
	const std::string before = settingsText(g_settings.usermenu[button]->title);
	edit_title = before;
	CKeyboardInput name(LOCALE_USERMENU_NAME, &edit_title);
	const int res = name.exec(parent, "");
	// Against the value the dialog opened with: a write made elsewhere while it was open stays.
	if (edit_title != before)
		writeMember(0, edit_title, true);
	return res;
}

int CUserMenuSetup::editKey(CMenuTarget *parent)
{
	const unsigned int before = g_settings.usermenu[button]->key;
	edit_key = before;
	CKeyChooser kc(&edit_key, LOCALE_USERMENU_KEY_SELECT, NEUTRINO_ICON_SETTINGS);
	const int res = kc.exec(parent, "");
	if (edit_key != before)
	{
		// The record keeps no key as 0.
		if (!writeMember(1, to_string(edit_key == (unsigned int) CRCInput::RC_nokey ? 0 : (int) edit_key), true))
			return res;
		const std::string name = CRCInput::getKeyName(g_settings.usermenu[button]->key);
		if (key_item)
			key_item->setOption(name);
		if (forwarder)
			forwarder->setName(name);
	}
	return res;
}

// Only one member changes, so writes made elsewhere to the others stay.
bool CUserMenuSetup::writeMember(size_t member, const std::string &value, bool asked)
{
	// The row skips the empty slots a removed button leaves behind.
	size_t record = 0;
	for (int i = 0; i < button && i < (int) g_settings.usermenu.size(); i++)
		if (g_settings.usermenu[i] != NULL)
			record++;

	coreapi::Result<std::string> now = coreapi::settings::get("usermenu");
	std::vector<std::string> lines;
	std::vector<std::string> members;
	if (now.ok())
	{
		coreapi::settings::splitOn(now.value(), '\n', lines);
		if (record < lines.size())
			coreapi::settings::splitOn(lines[record], '\t', members);
	}
	if (member >= members.size())
	{
		dprintf(DEBUG_NORMAL, "[usermenu] button %d not found in the setting\n", button);
		if (asked)
			ShowHint(LOCALE_MESSAGEBOX_ERROR, LOCALE_STRINGINPUT_SAVE_FAILED);
		return false;
	}
	members[member] = value;
	lines[record] = coreapi::settings::joinOn(members, '\t');
	return writeRow("usermenu", coreapi::settings::joinOn(lines, '\n'), asked);
}

// check button name for details like empty string and show an user message on issue
void CUserMenuSetup::checkButtonName()
{
	// count of configured items
	int used_items = getUsedItemsCount();

	// warn if no items defined and reset menu name, if empty
	if (used_items == 0)
	{
		if (!g_settings.usermenu[button]->title.empty())
		{
			//DisplayInfoMessage(g_Locale->getText(LOCALE_USERMENU_MSG_WARNING_NO_ITEMS));
			writeMember(0, "", false);
		}
		// exit function
		return;
	}

#if 0
	// if found only 1 configured item, ensure that the caption of usermenu is the same like this
	if (used_items == 1)
	{
		bool dummy;
		setSettingsText(g_settings.usermenu[button]->title, CUserMenu::getUserMenuButtonName(button, dummy));
	}
#endif

	if (button < USERMENU_ITEMS_COUNT && g_settings.usermenu[button]->title.empty())
	{
		std::string msg(g_Locale->getText(LOCALE_USERMENU_MSG_INFO_IS_EMPTY));
		msg += g_Locale->getText(usermenu[button].def_name);
		DisplayInfoMessage(msg.c_str());

		writeMember(0, g_Locale->getText(usermenu[button].def_name), false);
	}
}

// get count of used items
int CUserMenuSetup::getUsedItemsCount()
{
	return ::split(g_settings.usermenu[button]->items, ',').size();
}
