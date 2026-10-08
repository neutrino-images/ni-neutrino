/*
	$port: keybind_setup.cpp,v 1.4 2010/09/20 10:24:12 tuxbox-cvs Exp $

	keybindings setup implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2010 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/


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

#include <unistd.h>

#include "keybind_setup.h"

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/keyboard_input.h>
#include <gui/widget/settingactive.h>
#include <gui/widget/settingitem.h>

#include <gui/filebrowser.h>

#include <driver/screen_max.h>
#include <driver/screenshot.h>

#include <coreapi/base/apply.h>
#include <coreapi/box/apply_keys.h>
#include <coreapi/settings/menuspec.h>

#include <system/debug.h>
#include <system/helpers.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace
{

/* The rows each submenu offers, in the order it shows them. Labels, hints and
   which key a row holds are the rows' own. */
const char *const kModeKeys[] =
{
	"key_tvradio_mode", "key_power_off", "key_standby_off_add"
};

const char *const kChannellistKeys[] =
{
	"key_list_start", "key_list_end", "key_channelList_cancel", "key_channelList_sort",
	"key_channelList_addrecord", "key_channelList_addremind", "key_bouquet_up", "key_bouquet_down",
	"key_current_transponder"
};

const char *const kQuickzapKeys[] =
{
	"key_quickzap_up", "key_quickzap_down", "key_subchannel_up", "key_subchannel_down",
	"key_zaphistory", "key_lastchannel"
};

/* The movie player's keys, which the infobar also asks for by the remote's
   button. */
const char *const kMoviePlayerKeys[] =
{
	"mpkey.play", "mpkey.pause", "mpkey.stop", "mpkey.forward", "mpkey.rewind", "mpkey.audio",
	"mpkey.subtitle", "mpkey.time", "mpkey.bookmark", "mpkey.goto", "mpkey.next_repeat_mode", "mpkey.plugin"
};

const char *const kMovieBrowserKeys[] =
{
	"mbkey.copy_onefile", "mbkey.copy_several", "mbkey.cut", "mbkey.truncate", "mbkey.toggle_view_cw",
	"mbkey.toggle_view_ccw", "mbkey.cover"
};

const char *const kVideoKeys[] =
{
	"key_next43mode", "key_switchformat"
};

const char *const kNavigationKeys[] =
{
	"key_channelList_pageup", "key_channelList_pagedown"
};

const char *const kVolumeKeys[] =
{
	"key_volumeup", "key_volumedown"
};

/* Under the miscellaneous heading, after the submenu of the special keys. The
   screenshot key is offered only by a build that can take one. */
const char *const kMiscKeys[] =
{
	"key_favorites", "key_timeshift", "key_unlock",
#ifdef SCREENSHOT
	"key_screenshot",
#endif
	"key_sleep",
#if ENABLE_PIP
	"key_pip_close", "key_pip_close_avinput", "key_pip_setup", "key_pip_swap", "key_pip_rotate_cw",
	"key_pip_rotate_ccw",
#endif
	"key_help", "key_record"
};

/* The rest of what the section holds, which the screen offers beside the keys
   or which a file of keys loads along with them. */
const char *const kOtherKeys[] =
{
	"bouquetlist_mode", "sms_channel", "menu_left_exit", "sms_movie", "key_format_mode_active",
	"key_pic_mode_active", "key_pic_size_active", "mode_left_right_key_tv", "movieplayer_bisection_jump",
	"repeat_blocker", "repeat_genericblocker", "longkeypress_duration"
};

template <size_t N>
void addKeys(std::vector<std::string> &out, const char *const (&keys)[N])
{
	for (size_t i = 0; i < N; i++)
		out.push_back(keys[i]);
}

// Every row of the section, for a screen that has to show what a file of keys loaded.
std::vector<std::string> sectionKeys()
{
	std::vector<std::string> all;
	addKeys(all, kModeKeys);
	addKeys(all, kChannellistKeys);
	addKeys(all, kQuickzapKeys);
	addKeys(all, kMoviePlayerKeys);
	addKeys(all, kMovieBrowserKeys);
	addKeys(all, kVideoKeys);
	addKeys(all, kNavigationKeys);
	addKeys(all, kVolumeKeys);
	addKeys(all, kMiscKeys);
	addKeys(all, kOtherKeys);
	return all;
}

// Busy is a group whose startup phase is not reached, which the phase makes good.
void applyRow(const std::string &key)
{
	const coreapi::Status s = coreapi::applyKey(key);
	if (s != coreapi::Status::Ok && s != coreapi::Status::Busy)
		dprintf(DEBUG_NORMAL, "[CKeybindSetup] %s: apply failed\n", key.c_str());
}

/* Where a key lives in the settings, for the readers that must not wait for a
   menu: the remote control's own thread asks whether a press is a long one. The
   rows are found once and the members stay where they are. */
struct KeyMember
{
	const int *member;
	neutrino_locale_t label;
	bool plugin;
};

template <size_t N>
void addMembers(std::vector<KeyMember> &out, const char *const (&keys)[N], bool movie_player)
{
	for (size_t i = 0; i < N; i++)
	{
		coreapi::Result<coreapi::MenuItemSpec> r = coreapi::menuItem(keys[i]);
		if (!r.ok() || r.value().int_pointer == NULL)
			continue;
		KeyMember m;
		m.member = r.value().int_pointer(g_settings);
		m.label = NONEXISTANT_LOCALE;
		m.plugin = movie_player && std::string(keys[i]) == "mpkey.plugin";
		out.push_back(m);
	}
}

const std::vector<KeyMember> &allKeyMembers()
{
	static const std::vector<KeyMember> members = []()
	{
		std::vector<KeyMember> all;
		addMembers(all, kModeKeys, false);
		addMembers(all, kChannellistKeys, false);
		addMembers(all, kQuickzapKeys, false);
		addMembers(all, kMoviePlayerKeys, true);
		addMembers(all, kMovieBrowserKeys, false);
		addMembers(all, kVideoKeys, false);
		addMembers(all, kNavigationKeys, false);
		addMembers(all, kVolumeKeys, false);
		addMembers(all, kMiscKeys, false);
		return all;
	}();
	return members;
}

}

CKeybindSetup::CKeybindSetup()
{
	width = 40;
}

CKeybindSetup::~CKeybindSetup()
{
}

int CKeybindSetup::exec(CMenuTarget *parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init keybindings setup\n");
	int res = menu_return::RETURN_REPAINT;

	if (parent)
	{
		parent->hide();
	}

	if (actionKey == "loadkeys")
	{
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("conf");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(g_settings.backup_dir.c_str()) == true)
		{
			CNeutrinoApp::getInstance()->loadKeys(fileBrowser.getSelectedFile()->Name.c_str());
			printf("[neutrino keybind_setup] new keys: %s\n", fileBrowser.getSelectedFile()->Name.c_str());
			// The file carries the repeat blocking with the keys, and the items show what the settings now hold.
			applyRow("repeat_blocker");
			settingsWrittenElsewhere(sectionKeys());
		}
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "savekeys")
	{
		CFileBrowser fileBrowser;

		char msgtxt[1024];
		snprintf(msgtxt, sizeof(msgtxt), g_Locale->getText(LOCALE_SETTINGS_BACKUP_DIR), g_settings.backup_dir.c_str());

		int result = ShowMsg(LOCALE_EXTRA_SAVECONFIG, msgtxt, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo | CMsgBox::mbCancel);
		if (result == CMsgBox::mbrCancel)
			return res;
		if (result == CMsgBox::mbrNo)
		{
			fileBrowser.Dir_Mode = true;
			if (fileBrowser.exec(g_settings.backup_dir.c_str()) == true)
				setSettingsText(g_settings.backup_dir, fileBrowser.getSelectedFile()->Name);
			else
				return res;
		}

		std::string fname = "keys_" + getBackupSuffix() + ".conf";
		CKeyboardInput *sms = new CKeyboardInput(LOCALE_EXTRA_SAVEKEYS, &fname, 45);
		sms->exec(NULL, "");
		delete sms;

		std::string sname = g_settings.backup_dir + "/" + fname;
		printf("[neutrino keybind_setup] save keys: %s\n", sname.c_str());

		CNeutrinoApp::getInstance()->saveKeys(sname.c_str());

		return menu_return::RETURN_REPAINT;
	}

	res = showKeySetup();

	return res;
}

// used by driver/rcinput.cpp
bool checkLongPress(uint32_t key)
{
	if (g_settings.longkeypress_duration == LONGKEYPRESS_OFF)
		return false;
	if (key == CRCInput::RC_standby)
		return true;
	key |= CRCInput::RC_Repeat;
	const std::vector<KeyMember> &keys = allKeyMembers();
	for (size_t i = 0; i < keys.size(); i++)
		if ((uint32_t)*keys[i].member == key)
			return true;
	for (std::vector<SNeutrinoSettings::usermenu_t *>::iterator it = g_settings.usermenu.begin(); it != g_settings.usermenu.end(); ++it)
		if (*it && (uint32_t)((*it)->key) == key)
			return true;
	return false;
}

/* The name the setting's own list gives a remote control, which is what the
   question about a new one calls them by. */
static std::string remoteControlName(int value)
{
	coreapi::Result<coreapi::MenuItemSpec> r = coreapi::menuItem("remote_control_hardware");
	if (r.ok())
	{
		const std::vector<coreapi::MenuChoice> &choices = r.value().choices;
		for (size_t i = 0; i < choices.size(); i++)
			if (choices[i].value == value)
				return g_Locale->getText(localeFromKey(choices[i].label_key));
	}
	return "";
}

/* The receiver is programmed for the new remote control before this asks, so
   the answer can be given with the one in hand. Anything but a yes puts the one
   from right before the change back, which is the one written from elsewhere if
   that came in between. */
bool CKeybindSetup::confirmRemoteControl()
{
	int kept = coreapi::remoteHardwareBeforeLastChange();
	if (kept < 0)
		return true;
	const int before = kept;
	keepOrRestore(g_settings.remote_control_hardware, kept, "remote_control_hardware",
		      [before]() { return askKeepRemoteControl(before); }, applyRow);
	return true;
}

bool askKeepRemoteControl(int before)
{
	std::string msg = g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_MSG_PART1);
	msg += remoteControlName(before);
	msg += g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_MSG_PART2);
	msg += remoteControlName(g_settings.remote_control_hardware);
	msg += g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_MSG_PART3);
	return ShowMsg(LOCALE_MESSAGEBOX_INFO, msg, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo,
		       NEUTRINO_ICON_INFO, 450, 15, true) == CMsgBox::mbrYes;
}

int CKeybindSetup::showKeySetup()
{
	// keysetup menu
	CMenuWidget *keySettings = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP);
	keySettings->addIntroItems(LOCALE_MAINSETTINGS_KEYBINDING);

	// keybindings menu
	CMenuWidget bindSettings(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING);

	showKeyBindSetup(&bindSettings);
	CMenuForwarder *mf;

	mf = new CMenuForwarder(LOCALE_KEYBINDINGMENU_EDIT, true, NULL, &bindSettings, NULL, CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_KEY_BINDING);
	keySettings->addItem(mf);

	mf = new CMenuForwarder(LOCALE_EXTRA_SAVEKEYS, true, NULL, this, "savekeys", CRCInput::RC_green);
	mf->setHint("", LOCALE_MENU_HINT_KEY_SAVE);
	keySettings->addItem(mf);

	mf = new CMenuForwarder(LOCALE_EXTRA_LOADKEYS, true, NULL, this, "loadkeys", CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_KEY_LOAD);
	keySettings->addItem(mf);

	keySettings->addItem(GenericMenuSeparatorLine);

	// rc tuning
	int shortcut = 1;

	addNumberSetting(keySettings, "longkeypress_duration", true, NULL, CRCInput::convertDigitToKey(shortcut++), false, true);

	// A box whose receiver cannot be programmed has no such row, and takes no shortcut.
	CMenuOptionChooser *hardware = addChoiceSetting(keySettings, "remote_control_hardware", true, NULL, CRCInput::convertDigitToKey(shortcut));
	if (hardware)
	{
		shortcut++;
		afterApply(hardware, [this]() { return confirmRemoteControl(); });
		applyOnLeave(hardware);
	}

	addNumberSetting(keySettings, "repeat_blocker", true, NULL, CRCInput::convertDigitToKey(shortcut++), false, true);
	addNumberSetting(keySettings, "repeat_genericblocker", true, NULL, CRCInput::convertDigitToKey(shortcut++), false, true);

	int res = keySettings->exec(NULL, "");
	if (hardware)
		settleLeft(hardware);

	delete keySettings;
	return res;
}


void CKeybindSetup::showKeyBindSetup(CMenuWidget *bindSettings)
{
	int shortcut = 1;

	CMenuForwarder *mf;

	bindSettings->addIntroItems(LOCALE_KEYBINDINGMENU_HEAD);

	// modes
	CMenuWidget *bindSettings_modes = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_MODES);
	showKeyBindModeSetup(bindSettings_modes);
	mf = new CMenuDForwarder(LOCALE_KEYBINDINGMENU_MODECHANGE, true, NULL, bindSettings_modes, NULL, CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_KEY_MODECHANGE);
	bindSettings->addItem(mf);

	// channellist keybindings
	CMenuWidget *bindSettings_chlist = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_CHANNELLIST);
	showKeyBindChannellistSetup(bindSettings_chlist);
	mf = new CMenuDForwarder(LOCALE_KEYBINDINGMENU_CHANNELLIST, true, NULL, bindSettings_chlist, NULL, CRCInput::RC_green);
	mf->setHint("", LOCALE_MENU_HINT_KEY_CHANNELLIST);
	bindSettings->addItem(mf);

	// zapping keys quickzap
	CMenuWidget *bindSettings_qzap = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_QUICKZAP);
	showKeyBindQuickzapSetup(bindSettings_qzap);
	mf = new CMenuDForwarder(LOCALE_KEYBINDINGMENU_QUICKZAP, true, NULL, bindSettings_qzap, NULL, CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_KEY_QUICKZAP);
	bindSettings->addItem(mf);

	// movieplayer
	CMenuWidget *bindSettings_mplayer = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_MOVIEPLAYER);
	showKeyBindMovieplayerSetup(bindSettings_mplayer);
	mf = new CMenuDForwarder(LOCALE_MAINMENU_MOVIEPLAYER, true, NULL, bindSettings_mplayer, NULL, CRCInput::RC_blue);
	mf->setHint("", LOCALE_MENU_HINT_KEY_MOVIEPLAYER);
	bindSettings->addItem(mf);

	// moviebrowser
	CMenuWidget *bindSettings_mbrowser = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_MOVIEBROWSER);
	showKeyBindMoviebrowserSetup(bindSettings_mbrowser);
	mf = new CMenuDForwarder(LOCALE_MOVIEBROWSER_HEAD, true, NULL, bindSettings_mbrowser, NULL, CRCInput::RC_nokey);
	mf->setHint("", LOCALE_MENU_HINT_KEY_MOVIEBROWSER);
	bindSettings->addItem(mf);

	// video
	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_VIDEO));
	for (size_t i = 0; i < sizeof(kVideoKeys) / sizeof(kVideoKeys[0]); i++)
		addSetting(bindSettings, kVideoKeys[i]);

	// navigation
	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_NAVIGATION));
	for (size_t i = 0; i < sizeof(kNavigationKeys) / sizeof(kNavigationKeys[0]); i++)
		addSetting(bindSettings, kNavigationKeys[i]);
	addSetting(bindSettings, "menu_left_exit");

	// volume
	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_VOLUME));
	for (size_t i = 0; i < sizeof(kVolumeKeys) / sizeof(kVolumeKeys[0]); i++)
		addSetting(bindSettings, kVolumeKeys[i]);

	// misc
	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_MISC));

	// special keys
	CMenuWidget *bindSettings_special = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_SPECIAL);
	showKeyBindSpecialSetup(bindSettings_special);
	mf = new CMenuDForwarder(LOCALE_KEYBINDINGMENU_SPECIAL_ACTIVE, true, NULL, bindSettings_special, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_KEY_SPECIAL_ACTIVE);
	bindSettings->addItem(mf);

	bindSettings->addItem(new CMenuSeparator());

	// A row the box lacks is left out by the item builder.
	for (size_t i = 0; i < sizeof(kMiscKeys) / sizeof(kMiscKeys[0]); i++)
		addSetting(bindSettings, kMiscKeys[i]);

	bindSettings->addItem(new CMenuSeparator());

	// left/right keys
	addSetting(bindSettings, "mode_left_right_key_tv");
}

void CKeybindSetup::showKeyBindModeSetup(CMenuWidget *bindSettings_modes)
{
	bindSettings_modes->addIntroItems(LOCALE_KEYBINDINGMENU_MODECHANGE);

	// tv/radio, power off, standby off
	const neutrino_msg_t direct[] = { CRCInput::RC_red, CRCInput::RC_green, CRCInput::RC_yellow };
	for (size_t i = 0; i < sizeof(kModeKeys) / sizeof(kModeKeys[0]); i++)
		addSetting(bindSettings_modes, kModeKeys[i], true, NULL, direct[i]);
}

void CKeybindSetup::showKeyBindChannellistSetup(CMenuWidget *bindSettings_chlist)
{
	bindSettings_chlist->addIntroItems(LOCALE_KEYBINDINGMENU_CHANNELLIST);

	addSetting(bindSettings_chlist, "bouquetlist_mode");

	for (size_t i = 0; i < sizeof(kChannellistKeys) / sizeof(kChannellistKeys[0]); i++)
		addSetting(bindSettings_chlist, kChannellistKeys[i]);

	addSetting(bindSettings_chlist, "sms_channel");
}

void CKeybindSetup::showKeyBindQuickzapSetup(CMenuWidget *bindSettings_qzap)
{
	bindSettings_qzap->addIntroItems(LOCALE_KEYBINDINGMENU_QUICKZAP);

	for (size_t i = 0; i < sizeof(kQuickzapKeys) / sizeof(kQuickzapKeys[0]); i++)
		addSetting(bindSettings_qzap, kQuickzapKeys[i]);
}

void CKeybindSetup::showKeyBindMovieplayerSetup(CMenuWidget *bindSettings_mplayer)
{
	bindSettings_mplayer->addIntroItems(LOCALE_MAINMENU_MOVIEPLAYER);

	for (size_t i = 0; i < sizeof(kMoviePlayerKeys) / sizeof(kMoviePlayerKeys[0]); i++)
		addSetting(bindSettings_mplayer, kMoviePlayerKeys[i]);

	bindSettings_mplayer->addItem(GenericMenuSeparatorLine);

	// bisectional jumps
	addNumberSetting(bindSettings_mplayer, "movieplayer_bisection_jump");
}

void CKeybindSetup::showKeyBindMoviebrowserSetup(CMenuWidget *bindSettings_mbrowser)
{
	bindSettings_mbrowser->addIntroItems(LOCALE_MOVIEBROWSER_HEAD);

	for (size_t i = 0; i < sizeof(kMovieBrowserKeys) / sizeof(kMovieBrowserKeys[0]); i++)
		addSetting(bindSettings_mbrowser, kMovieBrowserKeys[i]);

	addSetting(bindSettings_mbrowser, "sms_movie");
}

void CKeybindSetup::showKeyBindSpecialSetup(CMenuWidget *bindSettings_special)
{
	bindSettings_special->addIntroItems(LOCALE_KEYBINDINGMENU_SPECIAL_ACTIVE);
	addSetting(bindSettings_special, "key_format_mode_active");
	addSetting(bindSettings_special, "key_pic_mode_active");
	addSetting(bindSettings_special, "key_pic_size_active");
}

const char *CKeybindSetup::getMoviePlayerButtonName(const neutrino_msg_t key, bool &active, bool return_title)
{
	/* The rows are found and their names looked up once, here on the GUI thread:
	   the names are a lookup the other threads must not make. */
	static std::vector<KeyMember> keys;
	if (keys.empty())
	{
		for (size_t i = 0; i < sizeof(kMoviePlayerKeys) / sizeof(kMoviePlayerKeys[0]); i++)
		{
			coreapi::Result<coreapi::MenuItemSpec> r = coreapi::menuItem(kMoviePlayerKeys[i]);
			if (!r.ok() || r.value().int_pointer == NULL)
				continue;
			KeyMember m;
			m.member = r.value().int_pointer(g_settings);
			m.label = localeFromKey(r.value().label_key);
			m.plugin = std::string(kMoviePlayerKeys[i]) == "mpkey.plugin";
			keys.push_back(m);
		}
	}

	active = false;
	for (size_t i = 0; i < keys.size(); i++)
	{
		if ((uint32_t)*keys[i].member == (unsigned int)key)
		{
			active = true;
			if (!return_title && keys[i].plugin)
				return g_settings.movieplayer_plugin.c_str();
			else
				return g_Locale->getText(keys[i].label);
		}
	}
	return "";
}
