/*
	daemon_control

	(C) 2017 NI-Team

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

#ifndef __daemon_control__
#define __daemon_control__

#include <gui/widget/menue.h>

class CDaemonControlMenu : public CMenuTarget
{
	private:
		int show();

	protected:
		int width;

	public:	
		CDaemonControlMenu();
		~CDaemonControlMenu();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};


#endif
