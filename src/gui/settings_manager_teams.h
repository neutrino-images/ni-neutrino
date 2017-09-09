/*
	cross-team settings manager menue - Neutrino-GUI

	Copyright (C) 2011 'defans'
	Homepage: http://www.bluepeercrew.us/

	Copyright (C) 2011 'vanhofen'
	Homepage: http://www.neutrino-images.de/

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

#ifndef __settings_manager_teams__
#define __settings_manager_teams__

#include <gui/widget/menue.h>
#include "gui/filebrowser.h"
#include <string>


class CSettingsManagerTeams : public CMenuTarget
{
	private:
		int width;

		int showMenu();
		void showHelp();
		void doBackup();
		void doRestore();
		void doReboot();

		CFileBrowser fileBrowser;
		CFileFilter fileFilter;

	public:
		CSettingsManagerTeams();
		~CSettingsManagerTeams();

		int exec(CMenuTarget* parent, const std::string & actionKey);
};

#endif
