/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

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

	***********************************************************

	Module Name: mb_functions.h

	Description: Implementation of the CMovieBrowser class
	             This class provides a filebrowser window to view, select and start a movies from HD.
	             This class does replace the Filebrowser

	Date:	   Nov 2005

	Author: Guenther@tuxbox.berlios.org
		based on code of Steffen Hehn 'McClean'

	(C) 2009-2014 Stefan Seyfried
	(C) 2016      Sven Hoefer

	outsourced:
	(C) 2016, 2026 Thilo Graf 'dbt'
*/

#ifndef __MB_FUNCTIONS__
#define __MB_FUNCTIONS__

#include "mb_types.h"

class MI_MOVIE_INFO;

extern bool sortDirection;
extern bool (* const sortBy[MB_INFO_MAX_NUMBER+1])(const MI_MOVIE_INFO* a, const MI_MOVIE_INFO* b);

#endif /*__MB_FUNCTIONS__*/
