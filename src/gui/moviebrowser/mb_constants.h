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

	Module Name: mb_constants.h

	Description: Shared constants for moviebrowser

	(C) 2016, 2026 Thilo Graf 'dbt'
*/

#ifndef MOVIEBROWSER_CONSTANTS_H_
#define MOVIEBROWSER_CONSTANTS_H_

#include "mb_types.h"

#include <stddef.h>
#include <system/locals.h>

#define NUMBER_OF_MOVIES_LAST 40 // number of movies shown in last record/last play lists
#define MOVIE_SMSKEY_TIMEOUT 800

/* default row size in percent for any element */
#define	MB_ROW_WIDTH_FILENAME		22
#define	MB_ROW_WIDTH_FILEPATH		22
#define	MB_ROW_WIDTH_TITLE		35
#define	MB_ROW_WIDTH_SERIE		15
#define	MB_ROW_WIDTH_INFO1		15
#define	MB_ROW_WIDTH_MAJOR_GENRE	15
#define	MB_ROW_WIDTH_MINOR_GENRE	8
#define	MB_ROW_WIDTH_INFO2		25
#define	MB_ROW_WIDTH_PARENTAL_LOCKAGE	4
#define	MB_ROW_WIDTH_CHANNEL		15
#define	MB_ROW_WIDTH_BOOKMARK		6
#define	MB_ROW_WIDTH_QUALITY		10
#define	MB_ROW_WIDTH_PREVPLAYDATE	12
#define	MB_ROW_WIDTH_RECORDDATE		12
#define	MB_ROW_WIDTH_PRODDATE		8
#define	MB_ROW_WIDTH_COUNTRY		8
#define	MB_ROW_WIDTH_GEOMETRIE		8
#define	MB_ROW_WIDTH_AUDIO		8
#define	MB_ROW_WIDTH_LENGTH		10
#define	MB_ROW_WIDTH_SIZE 		12
#define	MB_ROW_WIDTH_RATING		5
#define	MB_ROW_WIDTH_SPACER		1
#define	MB_ROW_WIDTH_RECORDTIME		10
#define	MB_ROW_WIDTH_ELAPSED		5

extern const CMenuOptionChooser::keyval MESSAGEBOX_BROWSER_ROW_ITEM[];
extern const size_t MESSAGEBOX_BROWSER_ROW_ITEM_COUNT;

extern const CMenuOptionChooser::keyval MESSAGEBOX_PARENTAL_LOCK_OPTIONS[];
extern const size_t MESSAGEBOX_PARENTAL_LOCK_OPTIONS_COUNT;

extern const CMenuOptionChooser::keyval MESSAGEBOX_PARENTAL_LOCKAGE_OPTIONS[];
extern const size_t MESSAGEBOX_PARENTAL_LOCKAGE_OPTION_COUNT;

extern const neutrino_locale_t m_localizedItemName[MB_INFO_MAX_NUMBER+1];
extern const int m_defaultRowWidth[MB_INFO_MAX_NUMBER+1];

#endif /* MOVIEBROWSER_CONSTANTS_H_ */
