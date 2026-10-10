/*
	$Id: osdlang_setup.cpp,v 1.2 2010/09/30 20:13:59 dbt Exp $

	OSD-Language Setup  implementation - Neutrino-GUI

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
#ifdef ENABLE_LCD4LINUX
#include "driver/lcd4l.h"
#endif
#include <unistd.h>

#include "osdlang_setup.h"

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/settingitem.h>

#include <driver/screen_max.h>

#include <system/helpers.h>
#include <system/debug.h>
#include <system/setting_helpers.h>

#include <coreapi/base/apply.h>
#include <coreapi/box/apply_lang.h>
#include <coreapi/settings/menuspec.h>

#include <gui/plugins.h>

namespace coreapi
{

Status applicationLoadLanguage(const std::string &name)
{
	if (g_Plugins != NULL)
		g_Plugins->loadPlugins();
	if (g_Locale->loadLocale(name.c_str()) == CLocaleManager::NO_SUCH_LOCALE)
		return Status::NotFound;
	return Status::Ok;
}

Status applicationLinkTimezone()
{
	CTZChangeNotifier().changeNotify(NONEXISTANT_LOCALE, (void *) "apply");
	return Status::Ok;
}

}


COsdLangSetup::COsdLangSetup(int wizard_mode)
{
	is_wizard = wizard_mode;

	width = 45;
}

COsdLangSetup::~COsdLangSetup()
{

}

int COsdLangSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init international setup\n");
	if(parent != NULL)
		parent->hide();

	if (!actionKey.empty()) {
		const std::string before = settingsText(g_settings.language);
		if (!writeRow("language", actionKey, true))
			return menu_return::RETURN_EXIT;
		// Busy: the startup phase loads it later.
		const coreapi::Status s = coreapi::applyKey("language");
		if (s != coreapi::Status::Ok && s != coreapi::Status::Busy)
		{
			dprintf(DEBUG_NORMAL, "[osdlang_setup] language %s not loaded, keeping %s\n", actionKey.c_str(), before.c_str());
			// The catalog the box starts on when its own is gone.
			if (!writeRow("language", before, false) && !writeRow("language", "deutsch", false))
				dprintf(DEBUG_NORMAL, "[osdlang_setup] no language to go back to\n");
		}
		return menu_return::RETURN_EXIT;
	}

	int res = showLocalSetup();

	return res;
}

//show international settings menu
int COsdLangSetup::showLocalSetup()
{
	//main local setup
	CMenuWidget *localSettings = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_LANGUAGE, width, MN_WIDGET_ID_LANGUAGESETUP);
	localSettings->setWizardMode(is_wizard);

	//add subhead and back button
	localSettings->addIntroItems(LOCALE_LANGUAGESETUP_HEAD);

	//language setup
	CMenuWidget osdl_setup(LOCALE_LANGUAGESETUP_OSD, NEUTRINO_ICON_LANGUAGE, width, MN_WIDGET_ID_LANGUAGESETUP_LOCALE);
	showLanguageSetup(&osdl_setup);

	CFollowForwarder *language = addTextForwarder(localSettings, LOCALE_LANGUAGESETUP_OSD, &osdl_setup, NULL, CRCInput::RC_red,
						     []() { return settingsText(g_settings.language); }, "language");
	language->setHint("", LOCALE_MENU_HINT_OSD_LANGUAGE);

	addSetting(localSettings, "timezone", true, NULL, CRCInput::RC_green, false, true);

	//prefered audio language
	CMenuWidget prefMenu(LOCALE_AUDIOMENU_PREF_LANGUAGES, NEUTRINO_ICON_LANGUAGE, width, MN_WIDGET_ID_LANGUAGESETUP_PREFAUDIO_LANGUAGE);
	//call menue for prefered audio languages
	showPrefMenu(&prefMenu);

	CMenuForwarder *mf = new CMenuForwarder(LOCALE_AUDIOMENU_PREF_LANGUAGES, true, NULL, &prefMenu, NULL, CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_LANG_PREF);
	localSettings->addItem(mf);

	int res = localSettings->exec(NULL, "");
	delete localSettings;
#ifdef ENABLE_LCD4LINUX
	CLCD4l::getInstance()->RestartLCD4lScript();
#endif
	return res;
}


//shows locale setup for language selection
void COsdLangSetup::showLanguageSetup(CMenuWidget *osdl_setup)
{
	osdl_setup->addIntroItems();

	const coreapi::Result<coreapi::MenuItemSpec> row = coreapi::menuItem("language");
	if (!row.ok())
		return;
	const std::vector<coreapi::MenuChoice> &locales = row.value().choices;

	for (size_t i = 0; i < locales.size(); i++)
	{
		const std::string &locale = locales[i].text;
		std::string loc(locale);
		loc.at(0) = toupper(loc.at(0));

		CMenuForwarder *mf = new CMenuForwarder(loc, true, NULL, this, locale.c_str());
		mf->iconName = mf->getActionKey();
		osdl_setup->addItem(mf, locale == g_settings.language);
	}
}

//shows menue for prefered audio/epg languages
void COsdLangSetup::showPrefMenu(CMenuWidget *prefMenu)
{
	prefMenu->addItem(GenericMenuSeparator);
	prefMenu->addItem(GenericMenuBack);
	prefMenu->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_AUDIOMENU_PREF_LANG_HEAD));

	addSetting(prefMenu, "auto_lang");

	char key[16];
	for (int i = 0; i < 3; i++)
	{
		snprintf(key, sizeof(key), "pref_lang_%d", i);
		addSetting(prefMenu, key, true, NULL, CRCInput::convertDigitToKey(i + 1), false, true);
	}

	prefMenu->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_AUDIOMENU_PREF_SUBS_HEAD));
	addSetting(prefMenu, "auto_subs");
	for (int i = 0; i < 3; i++)
	{
		snprintf(key, sizeof(key), "pref_subs_%d", i);
		addSetting(prefMenu, key, true, NULL, CRCInput::convertDigitToKey(i + 4), false, true);
	}
}
