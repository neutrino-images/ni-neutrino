/*
	$port: audioplayer_setup.cpp,v 1.4 2010/12/06 21:00:15 tuxbox-cvs Exp $

	audioplayer setup implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2011 T. Graf 'dbt'
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


#include "audioplayer_setup.h"

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/settingitem.h>
#include <gui/widget/stringinput.h>

#include <gui/audioplayer.h>
#include <gui/filebrowser.h>

#include <driver/screen_max.h>

#include <system/debug.h>


CAudioPlayerSetup::CAudioPlayerSetup()
{
	width = 40;
}

CAudioPlayerSetup::~CAudioPlayerSetup()
{

}

int CAudioPlayerSetup::exec(CMenuTarget* parent, const std::string &/*actionKey*/)
{
	dprintf(DEBUG_DEBUG, "init audioplayer setup\n");
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	res = showAudioPlayerSetup();

	return res;
}


/*shows the audio setup menue*/
int CAudioPlayerSetup::showAudioPlayerSetup()
{
	CMenuWidget* audioplayerSetup = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_AUDIOSETUP);

	audioplayerSetup->addIntroItems(LOCALE_AUDIOPLAYER_INTERNETRADIO_NAME);

	// display order
	addSetting(audioplayerSetup, "audioplayer_display");

	addSetting(audioplayerSetup, "audioplayer_follow");

	addSetting(audioplayerSetup, "audioplayer_select_title_by_name");

	addSetting(audioplayerSetup, "audioplayer_repeat_on");

	addSetting(audioplayerSetup, "audioplayer_show_playlist");

	addSetting(audioplayerSetup, "audioplayer_cover_as_screensaver");

	addSetting(audioplayerSetup, "audioplayer_highprio");
#if 0
	if (CVFD::getInstance()->has_lcd) //FIXME
		audioplayerSetup->addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_SPECTRUM     , &g_settings.spectrum    , MESSAGEBOX_NO_YES_OPTIONS      , MESSAGEBOX_NO_YES_OPTION_COUNT      , true ));
#endif
	addSetting(audioplayerSetup, "network_nfs_audioplayerdir");

	audioplayerSetup->addItem(GenericMenuSeparatorLine);

	// internetradio autostart first entry from favorites
	CMenuItem *autostart = addSetting(audioplayerSetup, "inetradio_autostart");
	if (autostart)
		autostart->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, autostart->hint);

	addSetting(audioplayerSetup, "audioplayer_enable_sc_metadata");

	addSetting(audioplayerSetup, "network_nfs_streamripperdir");

	int res = audioplayerSetup->exec (NULL, "");
	delete audioplayerSetup;
	return res;
}
