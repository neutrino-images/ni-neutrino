/*
	$port: record_setup.cpp,v 1.7 2010/12/05 22:32:12 tuxbox-cvs Exp $

	record setup implementation - Neutrino-GUI

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


#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include "record_setup.h"
#include <gui/filebrowser.h>
#include <gui/followscreenings.h>
#include <coreapi/base/apply.h>

#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/settingitem.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/stringinput_ext.h>
#include <gui/widget/keyboard_input.h>

#include <driver/screen_max.h>
#include <driver/record.h>

#include <system/debug.h>
#include <system/helpers.h>
#include <system/hddstat.h>

static bool notRecording()
{
	return !CNeutrinoApp::getInstance()->recordingstatus;
}

CRecordSetup::CRecordSetup()
{
	width = 50;
}

CRecordSetup::~CRecordSetup()
{

}

int CRecordSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init record setup\n");
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
	{
		parent->hide();
	}

	if(actionKey == "help_recording")
	{
		ShowMsg(LOCALE_SETTINGS_HELP, LOCALE_RECORDINGMENU_HELP, CMsgBox::mbrBack, CMsgBox::mbBack);
		return res;
	}
#if 0
	if (CNeutrinoApp::getInstance()->recordingstatus)
		DisplayInfoMessage(g_Locale->getText(LOCALE_RECORDINGMENU_RECORD_IS_RUNNING));
	else
#endif
		res = showRecordSetup();

	return res;
}

#if 0
#define RECORDINGMENU_RECORDING_TYPE_OPTION_COUNT 2
const CMenuOptionChooser::keyval RECORDINGMENU_RECORDING_TYPE_OPTIONS[RECORDINGMENU_RECORDING_TYPE_OPTION_COUNT] =
{
	{ CNeutrinoApp::RECORDING_OFF , LOCALE_RECORDINGMENU_OFF },
	{ CNeutrinoApp::RECORDING_FILE, LOCALE_RECORDINGMENU_FILE }
};
#endif

int CRecordSetup::showRecordSetup()
{
	CMenuForwarder * mf;
	//menue init
	CMenuWidget* recordingSettings = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_RECORDSETUP);

	recordingSettings->addIntroItems(LOCALE_MAINSETTINGS_RECORDING);
	CMenuWidget recordingTsSettings(LOCALE_MAINSETTINGS_RECORDING, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_RECORDSETUP_TIMESHIFT);
	showRecordTimeShiftSetup(&recordingTsSettings);

	CMenuWidget recordingTimerSettings(LOCALE_MAINSETTINGS_RECORDING, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_RECORDSETUP_TIMERSETTINGS);
	showRecordTimerSetup(&recordingTimerSettings);

	addSetting(recordingSettings, "network_nfs_recordingdir", notRecording);

	addSetting(recordingSettings, "recording_save_in_channeldir", true, NULL, CRCInput::RC_red); //NI

	//rec hours
	addSetting(recordingSettings, "record_hours");

	// end of recording
	addSetting(recordingSettings, "recording_epg_for_end");

	// already_found
	addSetting(recordingSettings, "recording_already_found_check");

	addSetting(recordingSettings, "recording_slow_warning");

	//NI
	addNumberSetting(recordingSettings, "recording_fill_warning", true, NULL, CRCInput::RC_nokey, false, true);

	addSetting(recordingSettings, "recording_startstop_msg");

	//filename template
	addSetting(recordingSettings, "recordingmenu.filename_template", true, NULL, CRCInput::RC_1, false, false, false,
		   LOCALE_RECORDINGMENU_FILENAME_TEMPLATE_HINT, LOCALE_RECORDINGMENU_FILENAME_TEMPLATE_HINT2);

	addSetting(recordingSettings, "auto_cover");

	addSetting(recordingSettings, "recording_bufsize");
	addSetting(recordingSettings, "recording_bufsize_dmx");

	recordingSettings->addItem(GenericMenuSeparatorLine);

	//timeshift
	mf = new CMenuForwarder(LOCALE_RECORDINGMENU_TIMESHIFT, true, NULL, &recordingTsSettings, NULL, CRCInput::RC_green);
	mf->setHint("", LOCALE_MENU_HINT_RECORD_TIMESHIFT);
	recordingSettings->addItem(mf);

	//timersettings
	mf = new CMenuForwarder(LOCALE_TIMERSETTINGS_SEPARATOR, true, NULL, &recordingTimerSettings, NULL, CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_RECORD_TIMER);
	recordingSettings->addItem(mf);

	CMenuWidget recordingaAudioSettings(LOCALE_MAINSETTINGS_RECORDING, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_RECORDSETUP_AUDIOSETTINGS);
	CMenuWidget recordingaDataSettings(LOCALE_MAINSETTINGS_RECORDING, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_RECORDSETUP_DATASETTINGS);

	//audiosettings
	showRecordAudioSetup(&recordingaAudioSettings);
	mf = new CMenuForwarder(LOCALE_RECORDINGMENU_APIDS, true, NULL, &recordingaAudioSettings, NULL, CRCInput::RC_blue);
	mf->setHint("", LOCALE_MENU_HINT_RECORD_APIDS);
	recordingSettings->addItem(mf);

	//datasettings
	showRecordDataSetup(&recordingaDataSettings);
	mf = new CMenuForwarder(LOCALE_RECORDINGMENU_DATA_PIDS, true, NULL, &recordingaDataSettings, NULL,  CRCInput::RC_2);
	mf->setHint("", LOCALE_MENU_HINT_RECORD_DATA);
	recordingSettings->addItem(mf);

	int res = recordingSettings->exec(NULL, "");
	delete recordingSettings;

	return res;
}

void CRecordSetup::showRecordTimerSetup(CMenuWidget *menu_timersettings)
{
	menu_timersettings->addIntroItems(LOCALE_TIMERSETTINGS_SEPARATOR);

	//start
	addSetting(menu_timersettings, "record_safety_time_before");

	//end
	addSetting(menu_timersettings, "record_safety_time_after");

	//announce
	addSetting(menu_timersettings, "recording_zap_on_announce");

	//zapto
	addSetting(menu_timersettings, "zapto_pre_time");

	menu_timersettings->addItem(GenericMenuSeparatorLine);

	//allow followscreenings
	addSetting(menu_timersettings, "timer_followscreenings");
}


void CRecordSetup::showRecordAudioSetup(CMenuWidget *menu_audiosettings)
{
	//default recording audio pids
	//CMenuWidget * apidMenu = new CMenuWidget(LOCALE_RECORDINGMENU_APIDS, NEUTRINO_ICON_AUDIO);
	//CMenuForwarder* fApidMenu = new CMenuForwarder(LOCALE_RECORDINGMENU_APIDS ,true, NULL, apidMenu);
	menu_audiosettings->addIntroItems(LOCALE_RECORDINGMENU_APIDS);
	addSetting(menu_audiosettings, "recording_audio_pids_std");
	addSetting(menu_audiosettings, "recording_audio_pids_alt");
	addSetting(menu_audiosettings, "recording_audio_pids_ac3");
}

void CRecordSetup::showRecordDataSetup(CMenuWidget *menu_datasettings)
{
	//recording data pids

	//teletext pids
	menu_datasettings->addIntroItems(LOCALE_RECORDINGMENU_DATA_PIDS);
	addSetting(menu_datasettings, "recordingmenu.stream_vtxt_pid");
	addSetting(menu_datasettings, "recordingmenu.stream_subtitle_pids");
}

void CRecordSetup::showRecordTimeShiftSetup(CMenuWidget *menu_ts)
{
	menu_ts->addIntroItems(LOCALE_RECORDINGMENU_TIMESHIFT);

	//timeshift dir
	addSetting(menu_ts, "timeshiftdir", notRecording);

	if (1) //has_hdd
	{
		addSetting(menu_ts, "timeshift_pause");

		addSetting(menu_ts, "timeshift_auto");

		addSetting(menu_ts, "timeshift_delete");

		addSetting(menu_ts, "timeshift_temp");

		//rec hours
		addSetting(menu_ts, "timeshift_hours");
	}
}
