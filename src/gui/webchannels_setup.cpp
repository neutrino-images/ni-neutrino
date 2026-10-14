/*
	WebTV/WebRadio Setup

	Copyright (C) 2012-2013 martii
	Copyright (C)      2018 by vanhofen

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "filebrowser.h"
#include <stdio.h>
#include <limits.h>
#include <global.h>
#include <libgen.h>
#include <neutrino.h>
#include <driver/screen_max.h>
#include <driver/framebuffer.h>
#include <gui/movieplayer.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/keyboard_input.h>
#include <gui/widget/menue_options.h>
#include <zapit/zapit.h>
#include <zapit/bouquets.h>
#include <neutrino_menue.h>
#include "webchannels_setup.h"

#include <stdlib.h>
#include <system/helpers.h>
#include <gui/widget/settingitem.h>
#include <coreapi/box/apply_webchannels.h>
#include <coreapi/settings/settings.h>

extern CBouquetManager *g_bouquetManager;

static void writeList(const char *key, const std::list<std::string> &list, bool asked)
{
	std::vector<std::string> files;
	for (std::list<std::string>::const_iterator it = list.begin(); it != list.end(); ++it)
		if (!it->empty())
			files.push_back(*it);
	writeRow(key, files, asked);
}

// The channel daemon scans the automatic folders itself on every reload.
static void reloadWebChannels(const char *reason)
{
	CZapit::getInstance()->SetWebTVXML(&g_settings.webtv_xml);
	CZapit::getInstance()->SetWebRadioXML(&g_settings.webradio_xml);
	g_bouquetManager->setWebchannelsReloadReason(reason);
	g_Zapit->reinitChannels();
	CNeutrinoApp::getInstance()->xmltv_xml_auto_readepg();
}

coreapi::Status coreapi::applicationReloadWebChannels()
{
	if (!g_bouquetManager || !g_Zapit)
		return Status::Internal;
	reloadWebChannels("settings_reload");
	return Status::Ok;
}

coreapi::Status coreapi::applicationRestartWebStream()
{
	CZapitChannel *cc = CZapit::getInstance()->GetCurrentChannel();
	/* A stream without a script has no size to choose, so starting it over would only
	   interrupt it. */
	if (cc && IS_WEBCHAN(cc->getChannelID()) && !cc->getScriptName().empty())
	{
		/* one transition, no DVB bounce in between - see RestartBackground() */
		CMoviePlayerGui::getInstance().RestartBackground(cc->getUrl(), cc->getName(), cc->getChannelID(), cc->getScriptName());
	}
	return Status::Ok;
}

CWebChannelsSetup::CWebChannelsSetup()
{
	webradio = false;
	width = 55;
	selected = -1;
	item_offset = 0;
	changed = false;
	m = NULL;
}

static const struct button_label CWebChannelsSetupFooterButtons[] =
{
	{ NEUTRINO_ICON_BUTTON_RED,  LOCALE_WEBCHANNELS_XML_DEL },
	{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_WEBCHANNELS_XML_ADD },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_WEBCHANNELS_XML_ENTER },
	{ NEUTRINO_ICON_BUTTON_BLUE, LOCALE_WEBCHANNELS_XML_RELOAD }
};
#define CWebChannelsSetupFooterButtonCount (sizeof(CWebChannelsSetupFooterButtons)/sizeof(CWebChannelsSetupFooterButtons[0]))

int CWebChannelsSetup::exec(CMenuTarget *parent, const std::string &actionKey)
{
	int res = menu_return::RETURN_REPAINT;

	if (actionKey == "d" /* delete */)
	{
		selected = m->getSelected();
		if (selected >= item_offset)
		{
			m->removeItem(selected);
			m->hide();
			selected = m->getSelected();
			changed = true;
		}
		return res;
	}
	if (actionKey == "c" /* change */)
	{
		selected = m->getSelected();
		CMenuItem *item = m->getItem(selected);
		CMenuForwarder *f = static_cast<CMenuForwarder *>(item);
		std::string dirname(f->getName());
		if (strstr(dirname.c_str(), "://"))
		{
			std::string entry = dirname;

			CKeyboardInput *e = new CKeyboardInput(LOCALE_WEBCHANNELS_XML_ENTER, &entry, 255);
			e->exec(this, "");
			delete e;

			if (entry.compare(dirname) != 0)
			{
				f->setName(entry);
				changed = true;
			}
		}
		else
		{
			CFileBrowser fileBrowser;
			CFileFilter fileFilter;
			fileFilter.addFilter("xml");
			fileFilter.addFilter("tv");
			fileFilter.addFilter("m3u");
			fileFilter.addFilter("m3u8");
			fileBrowser.Filter = &fileFilter;

			dirname = dirname.substr(0, dirname.rfind('/'));
			if (fileBrowser.exec(dirname.c_str()))
			{
				f->setName(fileBrowser.getSelectedFile()->Name);
				if (webradio)
					setSettingsText(g_settings.last_webradio_dir, dirname);
				else
					setSettingsText(g_settings.last_webtv_dir, dirname);
				changed = true;
			}
		}
		return res;
	}
	if (actionKey == "a" /* add */)
	{
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("xml");
		fileFilter.addFilter("tv");
		fileFilter.addFilter("m3u");
		fileFilter.addFilter("m3u8");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(webradio ? g_settings.last_webradio_dir.c_str() : g_settings.last_webtv_dir.c_str()) == true)
		{
			std::string s = fileBrowser.getSelectedFile()->Name;
			m->addItem(new CMenuForwarder(s, true, NULL, this, "c"));
			if (webradio)
				setSettingsText(g_settings.last_webradio_dir, s.substr(0, s.rfind('/')).c_str());
			else
				setSettingsText(g_settings.last_webtv_dir, s.substr(0, s.rfind('/')).c_str());
			changed = true;
		}
		return res;
	}
	if (actionKey == "e" /* enter */)
	{
		/* The route of the web server this image ships, under the name that
		   ends in an extension: the reader below decides what it was handed by
		   the extension of the address. The old address is still answered by
		   the layer that keeps the former server's routes, so a line stored
		   before this stays good. */
		std::string tpl = "http://xxx.xxx.xxx.xxx/api/v1/stream/playlist.m3u";
		if (webradio)
			tpl += "?mode=radio";
		else
			tpl += "?mode=tv";
		std::string entry = tpl;

		CKeyboardInput *e = new CKeyboardInput(LOCALE_WEBCHANNELS_XML_ENTER, &entry, 255);
		e->exec(this, "");
		delete e;

		if (entry.compare(tpl) != 0)
		{
			m->addItem(new CMenuForwarder(entry, true, NULL, this, "c"));
			changed = true;
		}
		return res;
	}
	if (actionKey == "r" /* reload */)
	{
		changed = true;
		return menu_return::RETURN_EXIT_ALL;
	}
	if (actionKey == "script_path")
	{
		const char *action_str = "ScriptPath";
		std::string dir = g_settings.livestreamScriptPath;
		if (chooserDir(dir, false, action_str))
			writeRow("livestreamScriptPath", dir, true);
		return res;
	}
	if (actionKey == "webtv_menu")
	{
		webradio = false;
	}
	if (actionKey == "webradio_menu")
	{
		webradio = true;
	}

	if (parent)
		parent->hide();

	res = Show();

	return res;
}

int CWebChannelsSetup::Show()
{
	item_offset = 0;
	std::list<std::string> webchannels = settingsCopy(webradio ? g_settings.webradio_xml : g_settings.webtv_xml);

	m = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_STREAMING, width, webradio ? MN_WIDGET_ID_WEBRADIOSETUP : MN_WIDGET_ID_WEBTVSETUP);
	m->addKey(CRCInput::RC_red, this, "d");
	m->addKey(CRCInput::RC_green, this, "a");
	m->addKey(CRCInput::RC_yellow, this, "e");
	m->addKey(CRCInput::RC_blue, this, "r");

	m->addIntroItems(webradio ? LOCALE_WEBRADIO_HEAD : LOCALE_WEBTV_HEAD);

	int shortcut = 1;

#if 0
	bool _mode_webtv = (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webtv) &&
			   (!CZapit::getInstance()->GetCurrentChannel()->getScriptName().empty());

	bool _mode_webradio = (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webradio) &&
			   (!CZapit::getInstance()->GetCurrentChannel()->getScriptName().empty());

	CMenuForwarder *mf;

	mf = new CMenuForwarder(LOCALE_LIVESTREAM_SCRIPTPATH, !_mode_webtv || !_mode_webradio, g_settings.livestreamScriptPath, this, "script_path", CRCInput::convertDigitToKey(shortcut++));
	m->addItem(mf);
#endif

	CMenuItem *oc;

	if (!webradio)
	{
		addChoiceSetting(m, "livestreamResolution", true, NULL, CRCInput::convertDigitToKey(shortcut++), true);
		addSetting(m, "webtv_stream_restart_attempts", true, NULL, CRCInput::convertDigitToKey(shortcut++));
		addSetting(m, "webtv_dns_diagnostics", true, NULL, CRCInput::convertDigitToKey(shortcut++));

		m->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, webradio ? LOCALE_WEBRADIO_XML : LOCALE_WEBTV_XML));
	}

	// TODO: show/hide autoloaded content when switching g_settings.webradio/webtv_xml_auto
	// The lists are read again by the web channel group when the switch changes.
	char hint_text[1024];
	if (webradio)
	{
		snprintf(hint_text, sizeof(hint_text) - 1, g_Locale->getText(LOCALE_MENU_HINT_WEBRADIO_XML_AUTO), WEBRADIODIR, WEBRADIODIR_VAR);
		oc = addSetting(m, "webradio_xml_auto", true, NULL, CRCInput::convertDigitToKey(shortcut++));
	}
	else
	{
		snprintf(hint_text, sizeof(hint_text) - 1, g_Locale->getText(LOCALE_MENU_HINT_WEBTV_XML_AUTO), WEBTVDIR, WEBTVDIR_VAR);
		oc = addSetting(m, "webtv_xml_auto", true, NULL, CRCInput::convertDigitToKey(shortcut++));
	}
	if (oc)
		oc->setHint("", hint_text);
	m->addItem(GenericMenuSeparator);

	item_offset = m->getItemsCount();
	// show autoloaded webradio/webtv files
	coreapi::Result<std::vector<coreapi::SettingChoice> > autofiles = coreapi::settings::choices(webradio ? "webradio_xml" : "webtv_xml");
	for (size_t i = 0; autofiles.ok() && i < autofiles.value().size(); i++)
		m->addItem(new CMenuForwarder(autofiles.value()[i].text, false, "auto"));
	if (item_offset < m->getItemsCount())
		m->addItem(GenericMenuSeparator);

	item_offset = m->getItemsCount();
	// show users webradio/webtv files, wherever they lie
	for (std::list<std::string>::iterator it = webchannels.begin(); it != webchannels.end(); ++it)
		m->addItem(new CMenuForwarder(*it, true, NULL, this, "c"));

	m->setFooter(CWebChannelsSetupFooterButtons, CWebChannelsSetupFooterButtonCount);

	int res = m->exec(NULL, "");
	m->hide();

	if (changed)
	{
		CHintBox hint(LOCALE_MESSAGEBOX_INFO, LOCALE_SERVICEMENU_RELOAD_HINT);
		hint.paint();
		webchannels.clear();
		for (int i = item_offset; i < m->getItemsCount(); i++)
		{
			CMenuItem *item = m->getItem(i);
			CMenuForwarder *f = static_cast<CMenuForwarder *>(item);
			webchannels.push_back(f->getName());
		}
		writeList(webradio ? "webradio_xml" : "webtv_xml", webchannels, true);
		reloadWebChannels("manual_menu");
		changed = false;
		hint.hide();
	}

	delete m;
	return res;
}

/* ## CWebTVResolution ############################################# */

CWebTVResolution::CWebTVResolution()
{
	width = 40;
}

int CWebTVResolution::exec(CMenuTarget *parent, const std::string & /*actionKey*/)
{
	if (parent)
		parent->hide();

	return Show();
}

int CWebTVResolution::Show()
{
	m = new CMenuWidget(LOCALE_WEBTV_HEAD, NEUTRINO_ICON_STREAMING, width, MN_WIDGET_ID_LIVESTREAM_RESOLUTION);
	m->addIntroItems(LOCALE_LIVESTREAM_HEAD);

	addChoiceSetting(m, "livestreamResolution", true, NULL, CRCInput::RC_nokey, true);

	int res = m->exec(NULL, "");
	m->hide();
	delete m;

	return res;
}

const char *CWebTVResolution::getResolutionValue()
{
	static std::string name;
	name.clear();
	const coreapi::Result<std::vector<coreapi::SettingChoice> > choices = coreapi::settings::choices("livestreamResolution");
	if (choices.ok())
	{
		for (size_t i = 0; i < choices.value().size(); ++i)
		{
			if (choices.value()[i].value == g_settings.livestreamResolution)
				name = choices.value()[i].label;
		}
	}
	return name.c_str();
}
