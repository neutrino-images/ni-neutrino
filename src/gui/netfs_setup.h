/*
	netfs_setup

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

#ifndef __netfs_setup__
#define __netfs_setup__

#include <system/fsmounter.h>
#include <gui/widget/menue.h>

class CNETFSMountGui : public CMenuTarget
{

	private:
		int menu(int mt);
		int menuEntry(int mt, int nr);

		std::string       m_entry[NETFS_NR_OF_ENTRIES];
		std::string       ISO_8859_1_entry[NETFS_NR_OF_ENTRIES];

		std::string getEntryString(int mt, int i);

		CMenuForwarder* mountMenuEntry[NETFS_NR_OF_ENTRIES];

		CFSMounter::FS_Support m_nfs_sup;
		CFSMounter::FS_Support m_cifs_sup;

		int vinit(int mt);

	protected:
		int width;

	public:
		CNETFSMountGui();
		int exec(CMenuTarget* parent, const std::string & actionKey);

		int read_config(int mt);
		int write_config(int mt);

		std::string fstabPath;
		std::string autoPath;
};

class CNETFSSetup : public CNETFSMountGui
{
	private:

	public:
		CNETFSSetup(){};
		~CNETFSSetup(){};
		int exec( CMenuTarget* parent, const std::string & actionKey );
};

#endif
