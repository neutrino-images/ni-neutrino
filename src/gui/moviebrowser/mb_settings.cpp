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

	Module Name: mb_settings.cpp

	Description: Settings helpers for moviebrowser

	(C) 2016, 2026 Thilo Graf 'dbt'
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mb.h"
#include "mb_constants.h"

#include <global.h>

#include <gui/widget/msgbox.h>
#include <system/helpers.h>

#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

void CMovieBrowser::initGlobalStorageSettings(void)
{
	m_settings.store.storageDirMovieUsed = true;
	m_settings.store.storageDirRecUsed = true;

	for (int i = 0; i < MB_MAX_DIRS; i++)
	{
		m_settings.store.storageDir[i] = "";
		m_settings.store.storageDirUsed[i] = 0;
	}
}

void CMovieBrowser::initGlobalSettings(void)
{
	m_settings.gui = MB_GUI_MOVIE_INFO;

	m_settings.lastPlayMaxItems = NUMBER_OF_MOVIES_LAST;
	m_settings.lastRecordMaxItems = NUMBER_OF_MOVIES_LAST;

	m_settings.browser_serie_mode = 0;
	m_settings.serie_auto_create = 0;

	m_settings.sorting.item 	= MB_INFO_TITLE;
	m_settings.sorting.direction = MB_DIRECTION_DOWN;

	m_settings.filter.item = MB_INFO_MAX_NUMBER;
	m_settings.filter.optionString = g_Locale->getText(LOCALE_OPTIONS_OFF);
	m_settings.filter.optionVar = 0;

	m_settings.parentalLockAge = MI_PARENTAL_OVER18;
	m_settings.parentalLock = MB_PARENTAL_LOCK_OFF;

	m_settings.reload = true;
	m_settings.remount = false;

	// storage
	initGlobalStorageSettings();

	/***** Browser List **************/
	m_settings.browserFrameHeightGeneral = 65; /* percent */
	m_settings.browserFrameHeightAdditional = 75; /* percent */
	m_settings.browserFrameHeight = m_settings.browserFrameHeightGeneral; /* percent */
	m_settings.browserCutLongRowText = 1;

	m_settings.browserRowNr = 6;
	m_settings.browserRowItem[0] = MB_INFO_TITLE;
	m_settings.browserRowItem[1] = MB_INFO_SPACER;
	m_settings.browserRowItem[2] = MB_INFO_INFO1;
	m_settings.browserRowItem[3] = MB_INFO_SPACER;
	m_settings.browserRowItem[4] = MB_INFO_LENGTH;
	m_settings.browserRowItem[5] = MB_INFO_RECORDDATE;
	m_settings.browserRowItem[6] = MB_INFO_MAX_NUMBER;
	m_settings.browserRowItem[7] = MB_INFO_MAX_NUMBER;
	m_settings.browserRowItem[8] = MB_INFO_MAX_NUMBER;
	m_settings.browserRowItem[9] = MB_INFO_MAX_NUMBER;

	m_settings.browserRowWidth[0] = 36; // title
	m_settings.browserRowWidth[1] = 1;  // spacer
	m_settings.browserRowWidth[2] = 36; // info1
	m_settings.browserRowWidth[3] = 1;  // spacer
	m_settings.browserRowWidth[4] = 11; // length
	m_settings.browserRowWidth[5] = 14; // recorddate
	m_settings.browserRowWidth[6] = m_defaultRowWidth[m_settings.browserRowItem[6]];
	m_settings.browserRowWidth[7] = m_defaultRowWidth[m_settings.browserRowItem[7]];
	m_settings.browserRowWidth[8] = m_defaultRowWidth[m_settings.browserRowItem[8]];
	m_settings.browserRowWidth[9] = m_defaultRowWidth[m_settings.browserRowItem[9]];

	m_settings.browserAdditional = 0;

	m_settings.ts_only = 1;
}

void CMovieBrowser::defaultSettings(MB_SETTINGS* /*settings*/)
{
	MBStorageSettings tmp_store;
	tmp_store.storageDirRecUsed = 0;
	tmp_store.storageDirMovieUsed = 0;
	bool keep = false;

	if (ShowMsg(LOCALE_MOVIEBROWSER_LOAD_DEFAULT, LOCALE_MOVIEBROWSER_MESSAGE_KEEP_STORAGE_SETTINGS, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_QUESTION) == CMsgBox::mbrYes)
	{
		tmp_store = m_settings.store;
		keep = true;
	}

	unlink(MOVIEBROWSER_SETTINGS_FILE);
	configfile.clear();
	initGlobalSettings();

	if (keep)
		m_settings.store = tmp_store;
}

bool CMovieBrowser::loadSettings(MB_SETTINGS* settings)
{
	bool result = configfile.loadConfig(MOVIEBROWSER_SETTINGS_FILE);
	if (!result) {
		dprintf(DEBUG_NORMAL, "[mb] loadSettings failed\n");
		return result;
	}

	settings->gui = (MB_GUI)configfile.getInt32("mb_gui", MB_GUI_MOVIE_INFO);

	settings->lastPlayMaxItems = configfile.getInt32("mb_lastPlayMaxItems", NUMBER_OF_MOVIES_LAST);
	settings->lastRecordMaxItems = configfile.getInt32("mb_lastRecordMaxItems", NUMBER_OF_MOVIES_LAST);
	settings->browser_serie_mode = configfile.getInt32("mb_browser_serie_mode", 0);
	settings->serie_auto_create = configfile.getInt32("mb_serie_auto_create", 0);
	settings->ts_only = configfile.getInt32("mb_ts_only", 1);

	settings->sorting.item = (MB_INFO_ITEM)configfile.getInt32("mb_sorting_item", MB_INFO_RECORDDATE);
	settings->sorting.direction = (MB_DIRECTION)configfile.getInt32("mb_sorting_direction", MB_DIRECTION_UP);

	settings->filter.item = (MB_INFO_ITEM)configfile.getInt32("mb_filter_item", MB_INFO_MAX_NUMBER);
	settings->filter.optionString = configfile.getString("mb_filter_optionString", g_Locale->getText(LOCALE_OPTIONS_OFF));
	settings->filter.optionVar = configfile.getInt32("mb_filter_optionVar", 0);

	if (settings->filter.item == MB_INFO_FILEPATH)
	{
		struct stat info;
		if (!(stat(settings->filter.optionString.c_str(), &info) == 0 && S_ISDIR(info.st_mode)))
		{
			//reset filter if directory not exists
			settings->filter.item = MB_INFO_MAX_NUMBER;
			settings->filter.optionString = g_Locale->getText(LOCALE_OPTIONS_OFF);
			settings->filter.optionVar = 0;
		}
	}

	settings->parentalLockAge = (MI_PARENTAL_LOCKAGE)configfile.getInt32("mb_parentalLockAge", MI_PARENTAL_OVER18);
	settings->parentalLock = (MB_PARENTAL_LOCK)configfile.getInt32("mb_parentalLock", MB_PARENTAL_LOCK_ACTIVE);

	settings->store.storageDirRecUsed = (bool)configfile.getInt32("mb_storageDir_rec", true);
	settings->store.storageDirMovieUsed = (bool)configfile.getInt32("mb_storageDir_movie", true);

	settings->reload = (bool)configfile.getInt32("mb_reload", true);
	settings->remount = (bool)configfile.getInt32("mb_remount", false);

	for (int i = 0; i < MB_MAX_DIRS; i++)
	{
		settings->store.storageDir[i] = configfile.getString("mb_dir_" + to_string(i), "");
		settings->store.storageDirUsed[i] = configfile.getInt32("mb_dir_used" + to_string(i), false);
	}
	/* these variables are used for the listframes */
	settings->browserFrameHeightGeneral = configfile.getInt32("mb_browserFrameHeightGeneral", 65);
	settings->browserFrameHeightAdditional = configfile.getInt32("mb_browserFrameHeightAdditional", 75);
	settings->browserFrameHeight = configfile.getInt32("mb_browserFrameHeight", settings->browserFrameHeightGeneral);
	settings->browserCutLongRowText = configfile.getInt32("mb_browserCutLongRowText", 1);
	settings->browserRowNr = configfile.getInt32("mb_browserRowNr", 0);
	for (int i = 0; i < MB_MAX_ROWS && i < settings->browserRowNr; i++)
	{
		settings->browserRowItem[i] = (MB_INFO_ITEM)configfile.getInt32("mb_browserRowItem_" + to_string(i), MB_INFO_MAX_NUMBER);
		settings->browserRowWidth[i] = configfile.getInt32("mb_browserRowWidth_" + to_string(i), 50);
	}
	settings->browserAdditional = configfile.getInt32("mb_browserAdditional", 0);

	return (result);
}

bool CMovieBrowser::saveSettings(MB_SETTINGS* settings)
{
	bool result = true;
	dprintf(DEBUG_DEBUG, "[mb]->%s\n", __func__);

	configfile.setInt32("mb_lastPlayMaxItems", settings->lastPlayMaxItems);
	configfile.setInt32("mb_lastRecordMaxItems", settings->lastRecordMaxItems);
	configfile.setInt32("mb_browser_serie_mode", settings->browser_serie_mode);
	configfile.setInt32("mb_serie_auto_create", settings->serie_auto_create);
	configfile.setInt32("mb_ts_only", settings->ts_only);

	configfile.setInt32("mb_gui", settings->gui);

	configfile.setInt32("mb_sorting_item", settings->sorting.item);
	configfile.setInt32("mb_sorting_direction", settings->sorting.direction);

	configfile.setInt32("mb_filter_item", settings->filter.item);
	configfile.setString("mb_filter_optionString", settings->filter.optionString);
	configfile.setInt32("mb_filter_optionVar", settings->filter.optionVar);

	configfile.setInt32("mb_storageDir_rec", settings->store.storageDirRecUsed);
	configfile.setInt32("mb_storageDir_movie", settings->store.storageDirMovieUsed);

	configfile.setInt32("mb_parentalLockAge", settings->parentalLockAge);
	configfile.setInt32("mb_parentalLock", settings->parentalLock);

	configfile.setInt32("mb_reload", settings->reload);
	configfile.setInt32("mb_remount", settings->remount);

	for (int i = 0; i < MB_MAX_DIRS; i++)
	{
		configfile.setString("mb_dir_" + to_string(i), settings->store.storageDir[i]);
		configfile.setInt32("mb_dir_used" + to_string(i), settings->store.storageDirUsed[i]); // do not save this so far
	}
	/* these variables are used for the listframes */
	configfile.setInt32("mb_browserFrameHeightGeneral", settings->browserFrameHeightGeneral);
	configfile.setInt32("mb_browserFrameHeightAdditional", settings->browserFrameHeightAdditional);
	configfile.setInt32("mb_browserFrameHeight", settings->browserAdditional ? settings->browserFrameHeightAdditional : settings->browserFrameHeightGeneral);
	configfile.setInt32("mb_browserCutLongRowText", settings->browserCutLongRowText);

	configfile.setInt32("mb_browserRowNr",settings->browserRowNr);
	for (int i = 0; i < MB_MAX_ROWS && i < settings->browserRowNr; i++)
	{
		configfile.setInt32("mb_browserRowItem_" + to_string(i), settings->browserRowItem[i]);
		configfile.setInt32("mb_browserRowWidth_" + to_string(i), settings->browserRowWidth[i]);
	}
	configfile.setInt32("mb_browserAdditional", settings->browserAdditional);

	if (configfile.getModifiedFlag())
		configfile.saveConfig(MOVIEBROWSER_SETTINGS_FILE);

	return (result);
}
