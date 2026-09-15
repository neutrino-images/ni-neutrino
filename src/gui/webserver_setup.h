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

#ifndef __webserver_setup__
#define __webserver_setup__

#include <gui/widget/menue.h>

#include <system/locals.h>

#include <string>

/* Where the box's own web server is set up at the box.

   Four settings and not the nine the screen this replaces offered, because four
   is what the server has. Whether a password is asked for at all is no longer a
   switch, the threads and the log level are the server's own business, and
   nothing here terminates a secure connection. The one a person notices is the
   first: a box whose web interface asked for no password has one after an
   upgrade.

   Nothing of the server's configuration is read or written here. The layer that
   owns that file knows what a value in it may be, and a screen that wrote the
   file itself would be a second place that had to know. */
class CWebserverSetup : public CMenuTarget
{
	private:
		int width;

		int port;
		std::string bind_address;
		std::string username;

		/* Empty when the screen opens, and empty unless somebody types. What the
		   box holds is a stored form, which no amount of reading turns back into
		   the password it was made from, so the empty field means the stored one
		   stays, which is also what somebody who came to change the port needs it
		   to mean. */
		std::string password;

		void loadConfig();
		bool saveConfig();
		void showProblem(const neutrino_locale_t what);
		int showSetup();

	public:
		CWebserverSetup();
		~CWebserverSetup();

		int exec(CMenuTarget *parent, const std::string &actionKey);
};

#endif /* __webserver_setup__ */
