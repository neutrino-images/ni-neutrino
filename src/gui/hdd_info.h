/*
	hdd_info

	(C) 2009-2016 NG-Team
	(C) 2016 NI-Team

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

#ifndef __hdd_info__
#define __hdd_info__

#include <gui/widget/menue.h>
#include <driver/framebuffer.h>

#include <string>

class CHDDInfoWidget
{
	private:
		CFrameBuffer	*frameBuffer;
		int x, y, width, height;

	public:
		CHDDInfoWidget();
		~CHDDInfoWidget();
		void hide();
		void paint(const std::string &Key);
};

// ----------------------------------------------------------------------------

class CHDDInfoMenu : public CMenuTarget, CHDDInfoWidget
{
	private:
		int width;
		int show();

	public:
		CHDDInfoMenu();
		~CHDDInfoMenu();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

#endif
