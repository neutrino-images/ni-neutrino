/*
	plugins_hide

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

#ifndef __plugins_hide__
#define __plugins_hide__

#include <gui/widget/menue.h>

class CPluginsHideNotifier : public CChangeObserver
{
	const char * filename;

	public:
		inline CPluginsHideNotifier(const char * _filename) { filename = _filename; };
		bool changeNotify(const neutrino_locale_t, void * data);
};

class CPluginsHideMenu : public CMenuTarget
{
	private:
		int menu();

	protected:
		int width;

	public:	
		CPluginsHideMenu();
		~CPluginsHideMenu();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};


#endif
