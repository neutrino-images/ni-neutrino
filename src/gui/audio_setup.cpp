/*
	$port: audio_setup.cpp,v 1.6 2010/12/06 21:00:15 tuxbox-cvs Exp $

	audio setup implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2009 T. Graf 'dbt'
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


#include "audio_setup.h"

#include <global.h>
#include <neutrino.h>

#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/settingitem.h>
#include <gui/widget/stringinput.h>

#include <driver/screen_max.h>

#include <hardware/audio.h>
#include <zapit/zapit.h>

#include <system/debug.h>

extern CAudioSetupNotifier	* audioSetupNotifier;
extern cAudio *audioDecoder;

CAudioSetup::CAudioSetup(int wizard_mode)
{
	is_wizard = wizard_mode;

	width = 40;
	selected = -1;
}

CAudioSetup::~CAudioSetup()
{

}

int CAudioSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	if (actionKey == "clear_vol_map") {
		CZapit::getInstance()->ClearVolumeMap();
		CZapit::getInstance()->SetVolumePercent(g_settings.audio_volume_percent_ac3, g_settings.audio_volume_percent_pcm);
		return menu_return::RETURN_NONE;
	}

	dprintf(DEBUG_DEBUG, "init audio setup\n");
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
	{
		parent->hide();
	}

	res = showAudioSetup();

	return res;
}

/* audio settings menu */
int CAudioSetup::showAudioSetup()
{
	//menue init
	CMenuWidget* audioSettings = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width);
	audioSettings->setSelected(selected);
	audioSettings->setWizardMode(is_wizard);

	//paint items
	audioSettings->addIntroItems(LOCALE_MAINSETTINGS_AUDIO);
	//---------------------------------------------------------
	//analog modes (stereo, mono l/r...)
	addSetting(audioSettings, "audio_AnalogMode", true, audioSetupNotifier);
	audioSettings->addItem(GenericMenuSeparatorLine);
	//---------------------------------------------------------
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	addSetting(audioSettings, "ac3_pass", true, audioSetupNotifier);
	addSetting(audioSettings, "dts_pass", true, audioSetupNotifier);
#else
	//dd via hdmi
	addSetting(audioSettings, "hdmi_dd", true, audioSetupNotifier);
	//dd via spdif
	addSetting(audioSettings, "spdif_dd", true, audioSetupNotifier);
#endif // HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	//dd subchannel auto on/off
	addSetting(audioSettings, "audio_DolbyDigital", true, audioSetupNotifier);
	//---------------------------------------------------------
	audioSettings->addItem(GenericMenuSeparatorLine);
	//av synch
	addSetting(audioSettings, "avsync", true, audioSetupNotifier);
	//volume steps
	addSetting(audioSettings, "current_volume_step");
	addSetting(audioSettings, "start_volume");
	//---------------------------------------------------------
#if HAVE_CST_HARDWARE
	/* only coolstream has SRS stuff, so only compile it there */
	audioSettings->addItem(GenericMenuSeparatorLine);
	//SRS on/off, switching the items below
	CTruVolumeNotifier truevolSetupNotifier;
	addSetting(audioSettings, "srs_enable", true, &truevolSetupNotifier);
	truevolSetupNotifier.add(addSetting(audioSettings, "srs_algo", g_settings.srs_enable, audioSetupNotifier));
#ifndef BOXMODEL_CST_HD2
	truevolSetupNotifier.add(addSetting(audioSettings, "srs_nmgr_enable", g_settings.srs_enable, audioSetupNotifier));
#endif
	truevolSetupNotifier.add(addSetting(audioSettings, "srs_ref_volume", g_settings.srs_enable, audioSetupNotifier));
#endif
	// ac3,pcm and clear volume adjustment
	audioSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_AUDIOMENU_VOLUME_ADJUSTMENT));
	CMenuOptionNumberChooser *adj = static_cast<CMenuOptionNumberChooser *>(addSetting(audioSettings, "audio_volume_percent_ac3", true, audioSetupNotifier));
	if (adj)
		adj->setNumberFormat("%d%%");
	adj = static_cast<CMenuOptionNumberChooser *>(addSetting(audioSettings, "audio_volume_percent_pcm", true, audioSetupNotifier));
	if (adj)
		adj->setNumberFormat("%d%%");

	CMenuForwarder *adj_clear = new CMenuForwarder(LOCALE_AUDIOMENU_VOLUME_ADJUSTMENT_CLEAR, true, NULL, this, "clear_vol_map");
	adj_clear->setHint("", LOCALE_MENU_HINT_AUDIO_ADJUST_VOL_CLEAR);
	audioSettings->addItem(adj_clear);

	int res = audioSettings->exec(NULL, "");
	selected = audioSettings->getSelected();
	delete audioSettings;
	CZapit::getInstance()->SetVolumePercent(g_settings.audio_volume_percent_ac3, g_settings.audio_volume_percent_pcm);

	return res;
}

void CTruVolumeNotifier::add(CMenuItem *item)
{
	if (item)
		toDisable.push_back(item);
}

bool CTruVolumeNotifier::changeNotify(const neutrino_locale_t, void * data)
{
	int active = (*(int *)data);

	for (size_t i = 0; i < toDisable.size(); i++)
		toDisable[i]->setActive(active);

	audioDecoder->SetSRS(g_settings.srs_enable, g_settings.srs_nmgr_enable, g_settings.srs_algo, g_settings.srs_ref_volume);

	return false;
}
