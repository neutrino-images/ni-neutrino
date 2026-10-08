/*
	$port: keybind_setup.h,v 1.2 2010/09/07 09:22:36 tuxbox-cvs Exp $

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

#ifndef __keybind_setup__
#define __keybind_setup__

#include <gui/widget/menue.h>
#include <gui/widget/icons.h>

#include <system/setting_helpers.h>

#include <string>

/* The question about the remote control the receiver is now programmed for,
   before being the one it had: true keeps it, no answer is a no. */
bool askKeepRemoteControl(int before);

class CKeybindSetup : public CMenuTarget
{
	private:
		int width;

		int showKeySetup();
		void showKeyBindSetup(CMenuWidget *bindSettings);
		void showKeyBindModeSetup(CMenuWidget *bindSettings_modes);
		void showKeyBindChannellistSetup(CMenuWidget *bindSettings_chlist);
		void showKeyBindQuickzapSetup(CMenuWidget *bindSettings_qzap);
		void showKeyBindMovieplayerSetup(CMenuWidget *bindSettings_mplayer);
		void showKeyBindMoviebrowserSetup(CMenuWidget *bindSettings_mbrowser);
		void showKeyBindSpecialSetup(CMenuWidget *bindSettings_special);
		bool confirmRemoteControl();

	public:
		CKeybindSetup();
		~CKeybindSetup();
		int exec(CMenuTarget *parent, const std::string &actionKey);
		static const char *getMoviePlayerButtonName(const neutrino_msg_t key, bool &active, bool return_title = false);
};

#endif
