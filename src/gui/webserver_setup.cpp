/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	web server setup menu - Neutrino-GUI

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "webserver_setup.h"

#include <gui/widget/icons.h>
#include <gui/widget/keyboard_input.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/msgbox.h>

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <httpd/webconfig.h>

#include <string>
#include <vector>

/* Nothing here is wrapped in a handler and there is nothing for one to catch.
   The layer below is built with exceptions and this directory is not, so the
   question is worth answering: every call made here answers what it refused and
   what went wrong while it wrote with a bool, which is the shape every other
   screen in this directory reaches a layer below it through. What is left is an
   allocation, and one that fails ends this program wherever it happens. */

CWebserverSetup::CWebserverSetup()
	: width(40)
	, port(0)
	, bind_address()
	, username()
	, password()
{
}

CWebserverSetup::~CWebserverSetup()
{
}

int CWebserverSetup::exec(CMenuTarget *parent, const std::string & /*actionKey*/)
{
	if (parent)
		parent->hide();

	return showSetup();
}

/* What the server is running on and not what its file says, which are the same
   thing on every start and are not the same thing after a save that could not
   be applied. The running values are what somebody standing in front of the
   box can act on. */
void CWebserverSetup::loadConfig()
{
	const httpd::WebSettings s = httpd::currentWebSettings();

	port = s.port;
	bind_address = s.bind_address;
	username = s.username;
	password.clear();
}

/* Says which value was refused and not only that one was. The layer that refused
   it wrote a line about it, and that line is the only thing in this program that
   knows whether the port, the address or the name was the one it could not use.
   It is not in the language of the screen around it, which is a price worth
   paying: a screen that says a save failed and nothing else leaves somebody
   pressing the same keys again. */
void CWebserverSetup::showProblem(const neutrino_locale_t what)
{
	std::string text = g_Locale->getText(what);

	const std::vector<std::string> &said = httpd::configProblems();
	if (!said.empty())
	{
		text += "\n\n";
		text += said[said.size() - 1];
	}

	DisplayErrorMessage(text.c_str());
}

bool CWebserverSetup::saveConfig()
{
	httpd::WebSettings s = httpd::currentWebSettings();

	s.port = port;
	s.bind_address = bind_address;
	s.username = username;

	/* An untouched field leaves the stored form where it is. There is nothing
	   to put in that field to begin with, so a save that read it as a password
	   would take the password off every box whose owner came to change a
	   port. */
	s.change_password = !password.empty();
	s.password = password;

	if (!httpd::saveWebSettings(NI_WEB_SETTINGS_FILE, s))
	{
		showProblem(LOCALE_NETWORKMENU_HTTPD_SAVE_FAILED);
		return false;
	}

	/* Held no longer than the write needs it. What is on the box is the stored
	   form, and a screen that kept the password it was handed would be the one
	   place the plain value outlived the save. */
	password.clear();

	/* The file is written by now whatever this answers, so what a failure here
	   means is that the box is not answering on what its file says, and the
	   file is what it will come up on next time. */
	if (!httpd::reloadAndRestart(NI_WEB_SETTINGS_FILE))
	{
		showProblem(LOCALE_NETWORKMENU_HTTPD_RESTART_FAILED);
		return false;
	}

	return true;
}

int CWebserverSetup::showSetup()
{
	loadConfig();

	const int orig_port = port;
	const std::string orig_bind_address = bind_address;
	const std::string orig_username = username;

	CMenuWidget *menu = new CMenuWidget(LOCALE_MAINSETTINGS_NETWORK, NEUTRINO_ICON_NETWORK, width, MN_WIDGET_ID_WEBSERVERSETUP);
	menu->addIntroItems(LOCALE_NETWORKMENU_HTTPD);

	/* The range is the one the layer below refuses outside of, so a value that
	   leaves this screen is one that can be written. The number pad is offered
	   because a port is a number somebody knows rather than one they search for
	   by holding a key down. */
	CMenuOptionNumberChooser *port_chooser = new CMenuOptionNumberChooser(LOCALE_NETWORKMENU_HTTPD_PORT, &port, true, 1, 65535);
	port_chooser->setNumericInput(true);
	port_chooser->setHint("", LOCALE_MENU_HINT_NET_HTTPD_PORT);
	menu->addItem(port_chooser);

	CKeyboardInput bind_input(LOCALE_NETWORKMENU_HTTPD_HOST, &bind_address);
	CMenuForwarder *bind_fwd = new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_HOST, true, bind_address, &bind_input);
	bind_fwd->setHint("", LOCALE_MENU_HINT_NET_HTTPD_HOST);
	menu->addItem(bind_fwd);

	menu->addItem(GenericMenuSeparatorLine);

	CKeyboardInput user_input(LOCALE_NETWORKMENU_HTTPD_USER, &username);
	CMenuForwarder *user_fwd = new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_USER, true, username, &user_input);
	user_fwd->setHint("", LOCALE_MENU_HINT_NET_HTTPD_USER);
	menu->addItem(user_fwd);

	CKeyboardInput pass_input(LOCALE_NETWORKMENU_HTTPD_PASS, &password);
	CMenuForwarder *pass_fwd = new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_PASS, true, password, &pass_input);
	pass_fwd->setHint("", LOCALE_MENU_HINT_NET_HTTPD_PASS);
	menu->addItem(pass_fwd);

	const int res = menu->exec(NULL, "");
	delete menu;

	/* A password that was typed counts as a change on its own. The other three
	   are compared against what the screen opened on, so leaving a screen
	   nobody edited asks nothing. */
	const bool changed = (orig_port != port)
		|| (orig_bind_address != bind_address)
		|| (orig_username != username)
		|| !password.empty();

	if (changed)
	{
		if (ShowMsg(LOCALE_NETWORKMENU_HTTPD, LOCALE_NETWORKMENU_APPLY_SETTINGS_NOW,
				CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo,
				NEUTRINO_ICON_QUESTION, width) == CMsgBox::mbrYes)
			saveConfig();
		else
			/* Not kept for the next time this screen opens. A password sitting
			   in a member of a screen nobody saved is the plain value living
			   on for no reason at all. */
			password.clear();
	}

	return res;
}
