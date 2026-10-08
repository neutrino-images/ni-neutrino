/*
	zapit_setup settings menu - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/
	License: GPL

	Copyright (C) 2011-2012 Stefan Seyfried

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

#include "zapit_setup.h"

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <driver/screen_max.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/settingitem.h>
#include <coreapi/settings/settings.h>
#include <system/debug.h>

#include <stdio.h>
#include <string>
#include <utility>
#include <vector>

CZapitSetup::CZapitSetup()
{
	width = 40;
}

CZapitSetup::~CZapitSetup()
{
}

int CZapitSetup::exec(CMenuTarget *parent, const std::string &/*actionKey*/)
{
	printf("[neutrino] init zapit menu setup...\n");
	if (parent)
		parent->hide();

	return showMenu();
}

void CZapitSetup::changeStartChannel(CMenuForwarder *zapit1, CMenuForwarder *zapit2)
{
	zapit1->paint();
	zapit2->paint();
}

int CZapitSetup::showMenu()
{
	// menue init
	CMenuWidget *zapit = new CMenuWidget(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_ZAPIT);
	zapit->addIntroItems(LOCALE_ZAPITSETUP_HEAD);
	// zapit
	CSelectChannelWidget select1;
	CSelectChannelWidget select2;

	// The start channels follow the row's own condition on uselastchannel.
	const bool changeable = coreapi::settings::conditionsHoldNow("startchanneltv");
	/* A forwarder points at a text only where it is not empty when handed over, and keeps
	   a copy otherwise, so each takes a copy and takes it again on a write. */
	CFollowForwarder *zapit1 = new CFollowForwarder(LOCALE_ZAPITSETUP_LAST_TV, changeable, NULL, &select1, "tv", CRCInput::RC_green);
	zapit1->setOption(settingsText(g_settings.StartChannelTV));
	zapit1->setHint("", LOCALE_MENU_HINT_LAST_TV);

	CFollowForwarder *zapit2 = new CFollowForwarder(LOCALE_ZAPITSETUP_LAST_RADIO, changeable, NULL, &select2, "radio", CRCInput::RC_yellow);
	zapit2->setOption(settingsText(g_settings.StartChannelRadio));
	zapit2->setHint("", LOCALE_MENU_HINT_LAST_RADIO);

	addChoiceSetting(zapit, "uselastchannel", true, NULL, CRCInput::RC_red);
	// The rows' conditions judge them on every pass, and a write of a channel shows in them.
	zapit1->follow(zapit, "startchanneltv", std::vector<std::string>(1, "startchanneltv_id"), true,
		       [zapit1]() { zapit1->setOption(settingsText(g_settings.StartChannelTV)); });
	zapit2->follow(zapit, "startchannelradio", std::vector<std::string>(1, "startchannelradio_id"), true,
		       [zapit2]() { zapit2->setOption(settingsText(g_settings.StartChannelRadio)); });
	zapit->addItem(GenericMenuSeparatorLine);
	zapit->addItem(zapit1);
	zapit->addItem(zapit2);
	zapit->addItem(GenericMenuSeparatorLine);
	CMenuOptionChooser *channel_mode = addChoiceSetting(zapit, "channel_mode_initial", true, NULL, CRCInput::RC_1);
	if (channel_mode)
		channel_mode->OnAfterChangeOption.connect(sigc::bind(sigc::mem_fun(*this, &CZapitSetup::changeStartChannel), zapit1, zapit2));

	CMenuOptionChooser *channel_mode_radio = addChoiceSetting(zapit, "channel_mode_initial_radio", true, NULL, CRCInput::RC_2);
	if (channel_mode_radio)
		channel_mode_radio->OnAfterChangeOption.connect(sigc::bind(sigc::mem_fun(*this, &CZapitSetup::changeStartChannel), zapit1, zapit2));

	int res = zapit->exec(NULL, "");
	delete zapit;
	return res;
}

// select menu
CSelectChannelWidget::CSelectChannelWidget()
{
	width = 40;
}

CSelectChannelWidget::~CSelectChannelWidget()
{
}

int CSelectChannelWidget::exec(CMenuTarget *parent, const std::string &actionKey)
{
	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	if (actionKey == "tv")
	{
		return InitZapitChannelHelper(CZapitClient::MODE_TV);
	}
	else if (actionKey == "radio")
	{
		return InitZapitChannelHelper(CZapitClient::MODE_RADIO);
	}
	else if (strncmp(actionKey.c_str(), "ZCT:", 4) == 0 || strncmp(actionKey.c_str(), "ZCR:", 4) == 0)
	{
		unsigned int cnr = 0;
		t_channel_id channel_id = 0;
		sscanf(&(actionKey[4]), "%u|%" SCNx64 "", &cnr, &channel_id);

		// The name and the identifier are one start channel, so they go in together or not at all.
		const bool tv = strncmp(actionKey.c_str(), "ZCT:", 4) == 0;
		char id_text[24];
		snprintf(id_text, sizeof(id_text), "%llx", (unsigned long long) channel_id);
		std::vector<std::pair<std::string, std::string> > pair;
		pair.push_back(std::make_pair(std::string(tv ? "startchanneltv" : "startchannelradio"),
					      actionKey.substr(actionKey.find_first_of("#") + 1)));
		pair.push_back(std::make_pair(std::string(tv ? "startchanneltv_id" : "startchannelradio_id"), std::string(id_text)));
		coreapi::settings::Refusals failed;
		coreapi::settings::writeBatch(pair, failed, true);
		if (!failed.empty())
		{
			dprintf(DEBUG_NORMAL, "[zapit setup] the start channel was not written\n");
			ShowHint(LOCALE_MESSAGEBOX_ERROR, LOCALE_STRINGINPUT_SAVE_FAILED);
		}

		// leave bouquet/channel menu and show a refreshed zapit menu with current start channel(s)
		g_RCInput->postMsg(CRCInput::RC_timeout, 0);
		return menu_return::RETURN_EXIT;
	}

	return res;
}

extern CBouquetManager *g_bouquetManager;
int CSelectChannelWidget::InitZapitChannelHelper(CZapitClient::channelsMode mode)
{
	std::vector<CMenuWidget *> toDelete;
	CMenuWidget mctv(LOCALE_TIMERLIST_BOUQUETSELECT, NEUTRINO_ICON_SETTINGS, width);
	mctv.addIntroItems();

	for (int i = 0; i < (int) g_bouquetManager->Bouquets.size(); i++)
	{
		CMenuWidget *mwtv = new CMenuWidget(LOCALE_TIMERLIST_CHANNELSELECT, NEUTRINO_ICON_SETTINGS, width);
		toDelete.push_back(mwtv);
		mwtv->addIntroItems();
		ZapitChannelList channels;
		if (mode == CZapitClient::MODE_RADIO)
			g_bouquetManager->Bouquets[i]->getRadioChannels(channels);
		else
			g_bouquetManager->Bouquets[i]->getTvChannels(channels);
		for (int j = 0; j < (int) channels.size(); j++)
		{
			CZapitChannel *channel = channels[j];
			char cChannelId[60] = {0};
			snprintf(cChannelId, sizeof(cChannelId), "ZC%c:%d|%" PRIx64 "#", (mode == CZapitClient::MODE_TV) ? 'T' : 'R', channel->number, channel->getChannelID());

			CMenuForwarder *chan_item = new CMenuForwarder(channel->getName(), true, NULL, this,
				(std::string(cChannelId) + channel->getName()).c_str(), CRCInput::RC_nokey, NULL,
				channel->scrambled ? NEUTRINO_ICON_MARKER_SCRAMBLED : (channel->getUrl().empty() ? NULL : NEUTRINO_ICON_MARKER_STREAMING));
			chan_item->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true);
			mwtv->addItem(chan_item);

		}
		if (!channels.empty() && (!g_bouquetManager->Bouquets[i]->bHidden))
		{
			mctv.addItem(new CMenuForwarder(g_bouquetManager->Bouquets[i]->Name.c_str(), true, NULL, mwtv));
		}
	}
	int res = mctv.exec(NULL, "");

	// delete dynamic created objects
	for (unsigned int count = 0; count < toDelete.size(); count++)
	{
		delete toDelete[count];
	}
	toDelete.clear();
	return res;
}
