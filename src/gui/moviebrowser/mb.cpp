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

	Module Name: mb.cpp

	Description: Implementation of the CMovieBrowser class
	             This class provides a filebrowser window to view, select and start a movies from HD.
	             This class does replace the Filebrowser

	Date:	   Nov 2005

	Author: Guenther@tuxbox.berlios.org
		based on code of Steffen Hehn 'McClean'

	(C) 2009-2015 Stefan Seyfried
	(C) 2016      Sven Hoefer

	outsourced:
	(C) 2016, 2026 Thilo Graf 'dbt'
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <driver/screen_max.h>

#include <algorithm>
#include <cmath>
//#include <cstdlib>
#include "mb.h"
#include "mb_constants.h"
#include "mb_functions.h"
#include "mb_help.h"
#include <gui/filebrowser.h>
#include <gui/moviedb/mdb-imdb.h>
#include <gui/moviedb/mdb-tmdb.h>
#include <gui/epgview.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/components/cc.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
#include <sys/stat.h>
#include <gui/nfs.h>
#include <neutrino.h>
#include <sys/vfs.h> // for statfs
#include <sys/mount.h>
#include <utime.h>
//#include <unistd.h>
#include <gui/pictureviewer.h>
#include <driver/record.h>
#include <driver/display.h>
#include <system/helpers.h>
#include <zapit/debug.h>
#include <driver/moviecut.h>
#include <driver/fontrenderer.h>

#include <timerdclient/timerdclient.h>
#include <system/hddstat.h>

#ifdef ENABLE_LCD4LINUX
#include "driver/lcd4l.h"
#endif

extern CPictureViewer * g_PicViewer;
extern bool timeset;

#define TRACE  printf

#define TITLE_BACKGROUND_COLOR ((CFBWindow::color_t)COL_MENUHEAD_PLUS_0)
#define TITLE_FONT_COLOR COL_MENUHEAD_TEXT

#define TITLE_FONT g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]
#define FOOT_FONT g_Font[SNeutrinoSettings::FONT_TYPE_MENU_FOOT]

static MI_MOVIE_INFO* playing_info;
CMovieBrowser::CMovieBrowser(): configfile ('\t')
{
	init();
}

CMovieBrowser::~CMovieBrowser()
{
	//TRACE("[mb] del\n");

	m_dir.clear();

	m_dirNames.clear();

	m_vMovieInfo.clear();
	m_vHandleBrowserList.clear();
	m_vHandleRecordList.clear();
	m_vHandlePlayList.clear();
	m_vHandleSerienames.clear();

	clearListLines();

	if (m_detailsLine)
		delete m_detailsLine;

	if (m_movieCover)
		delete m_movieCover;

	if (m_header)
		delete m_header;
}

void CMovieBrowser::clearListLines()
{
	for (int i = 0; i < MB_MAX_ROWS; i++)
		m_pcBrowser->cleanupRow(&m_FilterLines, i);

	m_browserListLines.Icon.clear();
	m_browserListLines.marked.clear();

	for (int i = 0; i < 3; i++)
	{
		m_pcLastRecord->cleanupRow(&m_recordListLines, i);
		m_pcLastPlay->cleanupRow(&m_playListLines, i);
	}
	m_recordListLines.marked.clear();
	m_playListLines.marked.clear();
}

void CMovieBrowser::clearSelection()
{
	//TRACE("[mb]->%s\n", __func__);
	for (unsigned i = 0; i < m_vMovieInfo.size(); i++)
		m_vMovieInfo[i].marked = false;

	m_pcBrowser->clearMarked();
	m_pcLastPlay->clearMarked();
	m_pcLastRecord->clearMarked();
}

void CMovieBrowser::fileInfoStale(void)
{
	m_file_info_stale = true;
	m_seriename_stale = true;

	 // Also release memory buffers, since we have to reload this stuff next time anyhow
	m_dirNames.clear();

	m_vMovieInfo.clear();
	m_vHandleBrowserList.clear();
	m_vHandleRecordList.clear();
	m_vHandlePlayList.clear();
	m_vHandleSerienames.clear();

	clearListLines();
}

void CMovieBrowser::init(void)
{
	bool reinit_rows = false;
	int i = 0;
	//TRACE("[mb]->init\n");
	initGlobalSettings();
	loadSettings(&m_settings);

	m_file_info_stale = true;
	m_seriename_stale = true;

	framebuffer = CFrameBuffer::getInstance();
	m_pcBrowser = NULL;
	m_pcLastPlay = NULL;
	m_pcLastRecord = NULL;
	m_pcInfo1 = NULL;
	m_pcInfo2 = NULL;
	m_pcFilter = NULL;
	m_header = NULL;
	m_windowFocus = MB_FOCUS_BROWSER;

	m_textTitle = g_Locale->getText(LOCALE_MOVIEBROWSER_HEAD);

	m_currentStartPos = 0;

	m_movieSelectionHandler = NULL;
	m_currentBrowserSelection = 0;
	m_currentRecordSelection = 0;
	m_currentPlaySelection = 0;
	m_prevBrowserSelection = 0;
	m_prevRecordSelection = 0;
	m_prevPlaySelection = 0;

	m_storageType = MB_STORAGE_TYPE_NFS;

	m_parentalLock = m_settings.parentalLock;

	// check g_setting values
	if (m_settings.gui >= MB_GUI_MAX_NUMBER)
		m_settings.gui = MB_GUI_MOVIE_INFO;

	if (m_settings.sorting.direction >= MB_DIRECTION_MAX_NUMBER)
		m_settings.sorting.direction = MB_DIRECTION_DOWN;
	if (m_settings.sorting.item >= MB_INFO_MAX_NUMBER)
		m_settings.sorting.item = MB_INFO_TITLE;

	if (m_settings.filter.item >= MB_INFO_MAX_NUMBER)
		m_settings.filter.item = MB_INFO_MAX_NUMBER;

	if (m_settings.parentalLockAge >= MI_PARENTAL_MAX_NUMBER)
		m_settings.parentalLockAge = MI_PARENTAL_OVER18;
	if (m_settings.parentalLock >= MB_PARENTAL_LOCK_MAX_NUMBER)
		m_settings.parentalLock = MB_PARENTAL_LOCK_OFF;

	m_settings.browserFrameHeight = m_settings.browserAdditional ? m_settings.browserFrameHeightAdditional : m_settings.browserFrameHeightGeneral;
	/* convert from old pixel-based to new percent values */
	if (m_settings.browserFrameHeight > 100)
		m_settings.browserFrameHeight = 50;
	if (m_settings.browserFrameHeight < MIN_BROWSER_FRAME_HEIGHT)
		m_settings.browserFrameHeight = MIN_BROWSER_FRAME_HEIGHT;
	if (m_settings.browserFrameHeight > MAX_BROWSER_FRAME_HEIGHT)
		m_settings.browserFrameHeight = MAX_BROWSER_FRAME_HEIGHT;

	/* the old code had row widths in pixels, not percent. Check if we have
	 * an old configuration (one of the rows hopefully was larger than 100 pixels... */
	for (i = 0; i < m_settings.browserRowNr; i++)
	{
		if (m_settings.browserRowWidth[i] > 100)
		{
			printf("[moviebrowser] old row config detected - converting...\n");
			reinit_rows = true;
			break;
		}
	}
	if (reinit_rows)
	{
		for (i = 0; i < m_settings.browserRowNr; i++)
			m_settings.browserRowWidth[i] = m_defaultRowWidth[m_settings.browserRowItem[i]];
	}

	initFrames();
	initRows();

	/* save settings here, because exec() will load them again... */
	if (reinit_rows)
		saveSettings(&m_settings);

	refreshLastPlayList();
	refreshLastRecordList();
	refreshBrowserList();
	refreshFilterList();
	g_PicViewer->getSupportedImageFormats(PicExts);
	show_mode = MB_SHOW_RECORDS; //FIXME

	filelist.clear();
	filelist_it = filelist.end();
	movielist.clear();

	m_detailsLine = NULL;
	m_movieCover = NULL;

	old_EpgId = 0;
	old_ChannelName.clear();

	m_doRefresh = false;
	m_doLoadMovies = false;
}

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
	//TRACE("[mb]->initGlobalSettings\n");

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

void CMovieBrowser::initFrames(void)
{
	m_pcFontFoot = FOOT_FONT;
	m_pcFontTitle = TITLE_FONT;

	//TRACE("[mb]->%s\n", __func__);
	m_cBoxFrame.iWidth = 			framebuffer->getWindowWidth();
	m_cBoxFrame.iHeight = 			framebuffer->getWindowHeight();
	m_cBoxFrame.iX = 			getScreenStartX(m_cBoxFrame.iWidth);
	m_cBoxFrame.iY = 			getScreenStartY(m_cBoxFrame.iHeight);

	m_cBoxFrameTitleRel.iX =		0;
	m_cBoxFrameTitleRel.iY = 		0;
	m_cBoxFrameTitleRel.iWidth = 		m_cBoxFrame.iWidth;
	m_cBoxFrameTitleRel.iHeight = 		m_pcFontTitle->getHeight();

	const int pic_h = 39;
	m_cBoxFrameTitleRel.iHeight = std::max(m_cBoxFrameTitleRel.iHeight, pic_h);

	m_cBoxFrameBrowserList.iX = 		m_cBoxFrame.iX;
	m_cBoxFrameBrowserList.iY = 		m_cBoxFrame.iY + m_cBoxFrameTitleRel.iHeight;
	if (m_settings.browserAdditional)
		m_cBoxFrameBrowserList.iWidth =	m_cBoxFrame.iWidth / 3 * 2;
	else
		m_cBoxFrameBrowserList.iWidth =	m_cBoxFrame.iWidth;

	m_settings.browserFrameHeight = m_settings.browserAdditional ? m_settings.browserFrameHeightAdditional : m_settings.browserFrameHeightGeneral;
	m_cBoxFrameBrowserList.iHeight = 	m_cBoxFrame.iHeight * m_settings.browserFrameHeight / 100;


	m_cBoxFrameFootRel.iX = 		m_cBoxFrameBrowserList.iX;
	m_cBoxFrameFootRel.iHeight = 		refreshFoot(false);
	m_cBoxFrameFootRel.iY = 		m_cBoxFrameBrowserList.iY + m_cBoxFrameBrowserList.iHeight;
	m_cBoxFrameFootRel.iWidth = 		m_cBoxFrame.iWidth;

	m_cBoxFrameLastPlayList.iX = 		m_cBoxFrameBrowserList.iX;
	m_cBoxFrameLastPlayList.iY = 		m_cBoxFrameBrowserList.iY ;
	m_cBoxFrameLastPlayList.iWidth = 	m_cBoxFrame.iWidth / 2;
	m_cBoxFrameLastPlayList.iHeight = 	m_cBoxFrameBrowserList.iHeight;

	m_cBoxFrameLastRecordList.iX = 		m_cBoxFrameLastPlayList.iX + m_cBoxFrameLastPlayList.iWidth;
	m_cBoxFrameLastRecordList.iY = 		m_cBoxFrameLastPlayList.iY;
	m_cBoxFrameLastRecordList.iWidth = 	m_cBoxFrame.iWidth - m_cBoxFrameLastPlayList.iWidth;
	m_cBoxFrameLastRecordList.iHeight =	m_cBoxFrameLastPlayList.iHeight;

	m_cBoxFrameInfo1.iX = 			m_cBoxFrameBrowserList.iX;
	m_cBoxFrameInfo1.iY = 			m_cBoxFrameFootRel.iY + m_cBoxFrameFootRel.iHeight + OFFSET_INTER;
	m_cBoxFrameInfo1.iWidth = 		m_cBoxFrame.iWidth;
	m_cBoxFrameInfo1.iHeight = 		m_cBoxFrame.iHeight - m_cBoxFrameBrowserList.iHeight - OFFSET_INTER - m_cBoxFrameFootRel.iHeight - m_cBoxFrameTitleRel.iHeight;

	m_cBoxFrameInfo2.iX = 			m_cBoxFrameBrowserList.iX + m_cBoxFrameBrowserList.iWidth;
	m_cBoxFrameInfo2.iY = 			m_cBoxFrameBrowserList.iY;
	m_cBoxFrameInfo2.iWidth = 		m_cBoxFrame.iWidth - m_cBoxFrameBrowserList.iWidth;
	m_cBoxFrameInfo2.iHeight = 		m_cBoxFrameBrowserList.iHeight;

	m_cBoxFrameFilter.iX = 			m_cBoxFrameInfo1.iX;
	m_cBoxFrameFilter.iY = 			m_cBoxFrameInfo1.iY;
	m_cBoxFrameFilter.iWidth = 		m_cBoxFrameInfo1.iWidth;
	m_cBoxFrameFilter.iHeight = 		m_cBoxFrameInfo1.iHeight;
}

void CMovieBrowser::initRows(void)
{
	//TRACE("[mb]->%s\n", __func__);

	/*
	   The "last played" / "last recorded" windows have only half the width, so
	   multiply the relative width with 2 and add 1 percent for safety to date row.
	   This addition is just usefull for l a r g e font settings.
	*/

	/***** Last Play List **************/
	m_settings.lastPlayRowNr = 3;
	m_settings.lastPlayRow[0] = MB_INFO_TITLE;
	m_settings.lastPlayRow[1] = MB_INFO_SPACER;
	m_settings.lastPlayRow[2] = MB_INFO_PREVPLAYDATE;
	m_settings.lastPlayRowWidth[2] = m_defaultRowWidth[m_settings.lastPlayRow[2]] * 2 + 1;
	m_settings.lastPlayRowWidth[1] = m_defaultRowWidth[m_settings.lastPlayRow[1]] * 2;
	m_settings.lastPlayRowWidth[0] = 100 - m_settings.lastPlayRowWidth[1] - m_settings.lastPlayRowWidth[2];

	/***** Last Record List **************/
	m_settings.lastRecordRowNr = 3;
	m_settings.lastRecordRow[0] = MB_INFO_TITLE;
	m_settings.lastRecordRow[1] = MB_INFO_SPACER;
	m_settings.lastRecordRow[2] = MB_INFO_RECORDDATE;
	m_settings.lastRecordRowWidth[2] = m_defaultRowWidth[m_settings.lastRecordRow[2]] * 2 + 1;
	m_settings.lastRecordRowWidth[1] = m_defaultRowWidth[m_settings.lastRecordRow[1]] * 2;
	m_settings.lastRecordRowWidth[0] = 100 - m_settings.lastRecordRowWidth[1] - m_settings.lastRecordRowWidth[2];
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
	//TRACE("[mb]->%s\n", __func__);
	bool result = configfile.loadConfig(MOVIEBROWSER_SETTINGS_FILE);
	if (!result) {
		TRACE("CMovieBrowser::loadSettings failed\n");
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
	TRACE("[mb]->%s\n", __func__);

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

int CMovieBrowser::exec(CMenuTarget* parent, const std::string & actionKey)
{
	int returnval = menu_return::RETURN_REPAINT;

	if (actionKey == "loaddefault")
	{
		defaultSettings(&m_settings);
	}
	else if (actionKey == "show_movie_info_menu")
	{
		if (m_movieSelectionHandler != NULL)
			return showMovieInfoMenu(m_movieSelectionHandler);
	}
	else if (actionKey == "show_movie_cut_menu")
	{
		if (m_movieSelectionHandler != NULL)
		{
			if (parent)
				parent->hide();
			return showMovieCutMenu();
		}
	}
	else if (actionKey == "get_db_data")
	{
		if (m_movieSelectionHandler != NULL)
		{
			size_t pos;
			std::string element;

			CHintBox hintBox(LOCALE_MOVIEBROWSER_HEAD, LOCALE_MOVIEBROWSER_MENU_DATA_REQUEST);
			hintBox.paint();

			std::string title = m_movieSelectionHandler->epgTitle;
			CIMDB *imdb = CIMDB::getInstance();
			imdb->getMovieDetails(title);

#if 0
			element = imdb->getIMDbElement("Title");
			if (!element.empty())
			{
				printf("Title\n");
				printf("* old: %s\n", m_movieSelectionHandler->epgTitle.c_str());
				m_movieSelectionHandler->epgTitle = element;
				printf("* new: %s\n", m_movieSelectionHandler->epgTitle.c_str());
			}

			element = imdb->getIMDbElement("Genre");
			if (!element.empty())
			{
				printf("Genre\n");
				printf("* old: %s\n", m_movieSelectionHandler->epgInfo1.c_str());
				m_movieSelectionHandler->epgInfo1 = element;
				printf("* new: %s\n", m_movieSelectionHandler->epgInfo1.c_str());
			}
#endif

			element = imdb->getIMDbElement("Year");
			if (!element.empty())
			{
				printf("Year\n");
				printf("* old: %d\n", m_movieSelectionHandler->productionDate);
				m_movieSelectionHandler->productionDate = atoi(element);
				printf("* new: %d\n", m_movieSelectionHandler->productionDate);
			}

			element = imdb->getIMDbElement("Country");
			if (!element.empty())
			{
				printf("Country\n");
				printf("* old: %s\n", m_movieSelectionHandler->productionCountry.c_str());
				m_movieSelectionHandler->productionCountry = element;
				printf("* new: %s\n", m_movieSelectionHandler->productionCountry.c_str());
			}

			element = imdb->getIMDbElement("imdbRating");
			if (!element.empty())
			{
				if ((pos = element.find_first_of(",.")) != std::string::npos)
					element.replace(pos, 1, ""); // change 8,1 or 8.1 to 81

				printf("Rating\n");
				printf("* old: %d\n", m_movieSelectionHandler->rating);
				m_movieSelectionHandler->rating = atoi(element);
				printf("* new: %d\n", m_movieSelectionHandler->rating);
			}

			std::string poster;
			if ((pos = m_movieSelectionHandler->file.Name.rfind(".")) != std::string::npos)
			{
				printf("Poster\n");
				poster = m_movieSelectionHandler->file.Name.substr(0, pos);
				poster += ".jpg";
				CFileHelpers fh;
				if (fh.copyFile(imdb->getPoster().c_str(), poster.c_str(), 0644))
					printf("* poster: %s\n", poster.c_str());
				else
					printf("* poster: copy error\n");
			}

			imdb = NULL;
			//m_movieInfo.saveMovieInfo(*m_movieSelectionHandler);
			hintBox.hide();
		}
	}
	else if (actionKey == "save_movie_info")
	{
		CLoaderHint *lh = new CLoaderHint(LOCALE_MOVIEBROWSER_MENU_SAVE);
		lh->paint();

		m_movieInfo.saveMovieInfo(*m_movieSelectionHandler);

		delete lh;
	}
	else if (actionKey == "save_movie_info_all")
	{
		std::vector<MI_MOVIE_INFO*> * current_list=NULL;

		if (m_windowFocus == MB_FOCUS_BROWSER)          current_list = &m_vHandleBrowserList;
		else if (m_windowFocus == MB_FOCUS_LAST_PLAY)   current_list = &m_vHandlePlayList;
		else if (m_windowFocus == MB_FOCUS_LAST_RECORD) current_list = &m_vHandleRecordList ;

		if (current_list == NULL || m_movieSelectionHandler == NULL)
			return returnval;

		CLoaderHint *lh = new CLoaderHint(LOCALE_MOVIEBROWSER_INFO_HEAD_UPDATE);
		lh->paint();

		for (unsigned int i = 0; i< current_list->size();i++)
		{
			if (!((*current_list)[i]->parentalLockAge != 0 && movieInfoUpdateAllIfDestEmptyOnly == true) &&
					movieInfoUpdateAll[MB_INFO_TITLE])
				(*current_list)[i]->parentalLockAge = m_movieSelectionHandler->parentalLockAge;

			if (!(!(*current_list)[i]->serieName.empty() && movieInfoUpdateAllIfDestEmptyOnly == true) &&
					movieInfoUpdateAll[MB_INFO_SERIE])
				(*current_list)[i]->serieName = m_movieSelectionHandler->serieName;

			if (!(!(*current_list)[i]->productionCountry.empty() && movieInfoUpdateAllIfDestEmptyOnly == true) &&
					movieInfoUpdateAll[MB_INFO_COUNTRY])
				(*current_list)[i]->productionCountry = m_movieSelectionHandler->productionCountry;

			if (!((*current_list)[i]->genreMajor!=0 && movieInfoUpdateAllIfDestEmptyOnly == true) &&
					movieInfoUpdateAll[MB_INFO_MAJOR_GENRE])
				(*current_list)[i]->genreMajor = m_movieSelectionHandler->genreMajor;

			if (!((*current_list)[i]->rating!=0 && movieInfoUpdateAllIfDestEmptyOnly == true) &&
					movieInfoUpdateAll[MB_INFO_RATING])
				(*current_list)[i]->rating = m_movieSelectionHandler->rating;

			if (!((*current_list)[i]->quality!=0 && movieInfoUpdateAllIfDestEmptyOnly == true) &&
					movieInfoUpdateAll[MB_INFO_QUALITY])
				(*current_list)[i]->quality = m_movieSelectionHandler->quality;

			m_movieInfo.saveMovieInfo(*((*current_list)[i]));
		}

		delete lh;
	}
	else if (actionKey == "reload_movie_info")
	{
		loadMovies(false);
		updateMovieSelection();
	}
	else if (actionKey == "run")
	{
		if (parent) parent->hide();
		exec(NULL);
	}
	else if (actionKey == "book_clear_all")
	{
		m_movieSelectionHandler->bookmarks.start =0;
		m_movieSelectionHandler->bookmarks.end =0;
		m_movieSelectionHandler->bookmarks.lastPlayStop =0;
		for (int i = 0; i < MI_MOVIE_BOOK_USER_MAX; i++)
		{
			m_movieSelectionHandler->bookmarks.user[i].name = g_Locale->getText(LOCALE_MOVIEBROWSER_BOOK_NEW);
			m_movieSelectionHandler->bookmarks.user[i].length =0;
			m_movieSelectionHandler->bookmarks.user[i].pos =0;
		}
	}
	else if(actionKey == "show_menu")
	{
		showMenu(true);
		saveSettings(&m_settings);
	}
	else if (actionKey == "copy_onefile" || actionKey == "copy_several")
	{
		bool onefile = (actionKey == "copy_onefile");
		if ((show_mode == MB_SHOW_RECORDS) && (ShowMsg(LOCALE_MESSAGEBOX_INFO, onefile ? LOCALE_MOVIEBROWSER_COPY : LOCALE_MOVIEBROWSER_COPIES, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo) == CMsgBox::mbrYes))
		{
			CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, LOCALE_MOVIEBROWSER_COPYING);
			hintBox.paint();
			sleep(1); //???
			hintBox.hide();

			framebuffer->paintBackground(); // clear screen

#ifdef ENABLE_LCD4LINUX
			if (g_settings.lcd4l_support)
				CLCD4l::getInstance()->setActionKey("moviebrowser_moviecut");
#endif

			CMovieCut mc;
			bool res = mc.copyMovie(m_movieSelectionHandler, onefile);

#ifdef ENABLE_LCD4LINUX
			if (g_settings.lcd4l_support)
				CLCD4l::getInstance()->setActionKey("moviebrowser");
#endif

			//g_RCInput->clearRCMsg();
			if (res == 0)
				ShowMsg(LOCALE_MESSAGEBOX_ERROR, LOCALE_MOVIEBROWSER_COPY_FAILED, CMsgBox::mbrCancel, CMsgBox::mbCancel, NEUTRINO_ICON_ERROR);
			else
				m_doLoadMovies = true;
			m_doRefresh = true;
		}
	}
	else if (actionKey == "cut")
	{
#if 0
		if ((m_movieSelectionHandler == playing_info) && (NeutrinoModes::mode_ts == CNeutrinoApp::getInstance()->getMode()))
			ShowMsg(LOCALE_MESSAGEBOX_ERROR, "Impossible to cut playing movie.", CMsgBox::mbrCancel, CMsgBox::mbCancel, NEUTRINO_ICON_ERROR);
		else
#endif
		if ((show_mode == MB_SHOW_RECORDS) && (ShowMsg(LOCALE_MESSAGEBOX_INFO, LOCALE_MOVIEBROWSER_CUT, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo) == CMsgBox::mbrYes))
		{
			CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, LOCALE_MOVIEBROWSER_CUTTING);
			hintBox.paint();
			sleep(1); //???
			hintBox.hide();

			framebuffer->paintBackground(); // clear screen

#ifdef ENABLE_LCD4LINUX
			if (g_settings.lcd4l_support)
				CLCD4l::getInstance()->setActionKey("moviebrowser_moviecut");
#endif

			CMovieCut mc;
			bool res = mc.cutMovie(m_movieSelectionHandler);

#ifdef ENABLE_LCD4LINUX
			if (g_settings.lcd4l_support)
				CLCD4l::getInstance()->setActionKey("moviebrowser");
#endif
			//g_RCInput->clearRCMsg();
			if (!res)
				ShowMsg(LOCALE_MESSAGEBOX_ERROR, LOCALE_MOVIEBROWSER_CUT_FAILED, CMsgBox::mbrCancel, CMsgBox::mbCancel, NEUTRINO_ICON_ERROR);
			else
				m_doLoadMovies = true;
			m_doRefresh = true;
		}
	}
	else if (actionKey == "truncate")
	{
		if ((show_mode == MB_SHOW_RECORDS) && m_movieSelectionHandler != NULL)
		{
			if ((m_movieSelectionHandler == playing_info) && (NeutrinoModes::mode_ts == CNeutrinoApp::getInstance()->getMode()))
				ShowMsg(LOCALE_MESSAGEBOX_ERROR, LOCALE_MOVIEBROWSER_TRUNCATE_FAILED_PLAYING, CMsgBox::mbrCancel, CMsgBox::mbCancel, NEUTRINO_ICON_ERROR);
			else if (m_movieSelectionHandler->bookmarks.end == 0)
				ShowMsg(LOCALE_MESSAGEBOX_ERROR, LOCALE_MOVIEBROWSER_BOOK_NO_END, CMsgBox::mbrCancel, CMsgBox::mbCancel, NEUTRINO_ICON_ERROR);
			else
			{
				if (ShowMsg(LOCALE_MESSAGEBOX_INFO, LOCALE_MOVIEBROWSER_TRUNCATE, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo) == CMsgBox::mbrYes)
				{
					CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, LOCALE_MOVIEBROWSER_TRUNCATING);
					hintBox.paint();
					sleep(1); //???
					hintBox.hide();

					framebuffer->paintBackground(); // clear screen

#ifdef ENABLE_LCD4LINUX
					if (g_settings.lcd4l_support)
						CLCD4l::getInstance()->setActionKey("moviebrowser_moviecut");
#endif

					CMovieCut mc;
					bool res = mc.truncateMovie(m_movieSelectionHandler);

#ifdef ENABLE_LCD4LINUX
					if (g_settings.lcd4l_support)
						CLCD4l::getInstance()->setActionKey("moviebrowser");
#endif

					//g_RCInput->clearRCMsg();
					if (!res)
						ShowMsg(LOCALE_MESSAGEBOX_ERROR, LOCALE_MOVIEBROWSER_TRUNCATE_FAILED, CMsgBox::mbrCancel, CMsgBox::mbCancel, NEUTRINO_ICON_ERROR);
					else
						m_doLoadMovies = true;
					m_doRefresh = true;
				}
			}
		}
	}
	else if (actionKey == "delete_movie")
	{
		if (m_movieSelectionHandler != NULL)
		{
			if (onDelete(true /*cursor only*/))
			{
				m_doLoadMovies = true;
				m_doRefresh = true;
				returnval = menu_return::RETURN_EXIT;
			}
			if (m_windowFocus == MB_FOCUS_LAST_PLAY)
				m_pcLastPlay->setSelectedLine(0);
		}
	}
	return returnval;
}

int CMovieBrowser::exec(const char* path)
{
	bool res = false;
	menu_ret = menu_return::RETURN_REPAINT;

	TRACE("[mb]->%s\n", __func__);
	int returnDefaultOnTimeout = true;
	neutrino_msg_t msg;
	neutrino_msg_data_t data;

	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, g_Locale->getText(LOCALE_MOVIEBROWSER_HEAD));

	loadSettings(&m_settings);
	initFrames();

	// Clear all, to avoid 'jump' in screen
	m_vHandleBrowserList.clear();
	m_vHandleRecordList.clear();
	m_vHandlePlayList.clear();

	clearListLines();

	m_selectedDir = path;

	if (m_settings.remount == true)
	{
		TRACE("[mb] remount\n");
		/* FIXME: add hintbox ? */
		//umount automount dirs
		for (int i = 0; i < NETWORK_NFS_NR_OF_ENTRIES; i++)
		{
			if (g_settings.network_nfs[i].automount)
				umount2(g_settings.network_nfs[i].local_dir.c_str(), MNT_FORCE);
		}
		CFSMounter::automount();
	}

	if (paint() == false)
		return menu_ret;// paint failed due to less memory, exit

#ifdef ENABLE_LCD4LINUX
	if (g_settings.lcd4l_support)
		CLCD4l::getInstance()->setActionKey("moviebrowser");
#endif

	bool loop = true;
	bool result;
	int timeout = g_settings.timing[SNeutrinoSettings::TIMING_FILEBROWSER];
	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(timeout);
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		result = onButtonPress(msg);
		if (result == false)
		{
			if (msg == CRCInput::RC_timeout && returnDefaultOnTimeout)
			{
				TRACE("[mb] Timerevent\n");
				loop = false;
			}
			else if ((msg == NeutrinoMessages::EVT_TIMER) && (data == g_InfoViewer->sec_timer_id))
			{
				if (timeset)
					refreshTitle();
			}
			else if (msg == CRCInput::RC_ok)
			{
				for (unsigned int i = 0; i < m_vMovieInfo.size(); i++) {
					if (m_vMovieInfo[i].marked) {
						TRACE("[mb] has selected\n");
						res = true;
						break;
					}
				}
				if (res)
					break;

				m_currentStartPos = 0;

				if (m_movieSelectionHandler != NULL)
				{
					// If there is any available bookmark, show the bookmark menu
					if (m_movieSelectionHandler->bookmarks.lastPlayStop != 0 ||
							m_movieSelectionHandler->bookmarks.start != 0)
					{
						TRACE("[mb] stop: %d start:%d \n",m_movieSelectionHandler->bookmarks.lastPlayStop,m_movieSelectionHandler->bookmarks.start);
						m_currentStartPos = showStartPosSelectionMenu(); // display start menu m_currentStartPos =
					}

					if (m_currentStartPos >= 0) {
						playing_info = m_movieSelectionHandler;
						TRACE("[mb] start pos: %d s\n",m_currentStartPos);
						res = true;
						loop = false;
					} else
						refresh();
				}
			}
			else if (CNeutrinoApp::getInstance()->backKey(msg))
			{
				loop = false;
			}
			else if (CNeutrinoApp::getInstance()->listModeKey(msg)) {
				// do nothing
			}
			else if (msg == NeutrinoMessages::STANDBY_ON ||
					msg == NeutrinoMessages::LEAVE_ALL ||
					msg == NeutrinoMessages::SHUTDOWN ||
					msg == NeutrinoMessages::SLEEPTIMER)
			{
				menu_ret = menu_return::RETURN_EXIT_ALL;
				loop = false;
				g_RCInput->postMsg(msg, data);
			}
			else if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
			{
				TRACE("[mb]->exec: getInstance\n");
				menu_ret = menu_return::RETURN_EXIT_ALL;
				loop = false;
			}
		}

		if (msg <= CRCInput::RC_MaxRC)
			timeoutEnd = CRCInput::calcTimeoutEnd(timeout); // calcualate next timeout
	}
	hide();
	//TRACE(" return %d\n",res);

	m_prevBrowserSelection = m_currentBrowserSelection;
	m_prevRecordSelection = m_currentRecordSelection;
	m_prevPlaySelection = m_currentPlaySelection;

	saveSettings(&m_settings);

	// make stale if we should reload the next time, but not if movie has to be played
	if (m_settings.reload == true && res == false)
	{
		TRACE("[mb] force reload next time\n");
		fileInfoStale();
	}

#ifdef ENABLE_LCD4LINUX
	if (g_settings.lcd4l_support)
		CLCD4l::getInstance()->clearActionKey();
#endif

	return (res);
}

void CMovieBrowser::hide(void)
{
	//TRACE("[mb]->%s\n", __func__);
	if (m_header)	{
		delete m_header; m_header = NULL;
	}
	old_EpgId = 0;
	old_ChannelName.clear();
	framebuffer->paintBackground();
	if (m_pcFilter != NULL)
		m_currentFilterSelection = m_pcFilter->getSelectedLine();

	delete m_pcFilter;
	m_pcFilter = NULL;

	if (m_pcBrowser != NULL)
		m_currentBrowserSelection = m_pcBrowser->getSelectedLine();

	delete m_pcBrowser;
	m_pcBrowser = NULL;

	if (m_pcLastPlay != NULL)
		m_currentPlaySelection = m_pcLastPlay->getSelectedLine();

	delete m_pcLastPlay;
	m_pcLastPlay = NULL;

	if (m_pcLastRecord != NULL)
		m_currentRecordSelection = m_pcLastRecord->getSelectedLine();

	delete m_pcLastRecord;
	m_pcLastRecord = NULL;
	delete m_pcInfo1;
	m_pcInfo1 = NULL;
	delete m_pcInfo2;
	m_pcInfo2 = NULL;
}

int CMovieBrowser::paint(void)
{
	TRACE("[mb]->%s\n", __func__);

	Font* font = g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_LIST];
	m_movieSelectionHandler = NULL;

	m_pcBrowser = new CListFrame(&m_browserListLines, font, CListFrame::SCROLL | CListFrame::HEADER_LINE,
			&m_cBoxFrameBrowserList, NULL,
			g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_HEAD]);
	m_pcLastPlay = new CListFrame(&m_playListLines, font, CListFrame::SCROLL | CListFrame::HEADER_LINE | CListFrame::TITLE,
			&m_cBoxFrameLastPlayList, g_Locale->getText(LOCALE_MOVIEBROWSER_HEAD_PLAYLIST),
			g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_HEAD]);
	m_pcLastRecord = new CListFrame(&m_recordListLines, font, CListFrame::SCROLL | CListFrame::HEADER_LINE | CListFrame::TITLE,
			&m_cBoxFrameLastRecordList, g_Locale->getText(LOCALE_MOVIEBROWSER_HEAD_RECORDLIST),
			g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_HEAD]);
	m_pcFilter = new CListFrame(&m_FilterLines, g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_INFO], CListFrame::SCROLL | CListFrame::TITLE,
			&m_cBoxFrameFilter, g_Locale->getText(LOCALE_MOVIEBROWSER_HEAD_FILTER),
			g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_HEAD]);
	m_pcInfo1 = new CTextBox(" ", g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_INFO], CTextBox::TOP | CTextBox::SCROLL, &m_cBoxFrameInfo1);
	m_pcInfo2 = new CTextBox(" ", g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_INFO], CTextBox::TOP | CTextBox::SCROLL, &m_cBoxFrameInfo2);


	if (m_pcBrowser == NULL || m_pcLastPlay == NULL ||
			m_pcLastRecord == NULL || m_pcInfo1 == NULL || m_pcInfo2 == NULL || m_pcFilter == NULL)
	{
		TRACE("[mb] paint, ERROR: not enought memory to allocate windows");
		if (m_pcFilter != NULL)delete m_pcFilter;
		if (m_pcBrowser != NULL)delete m_pcBrowser;
		if (m_pcLastPlay != NULL) delete m_pcLastPlay;
		if (m_pcLastRecord != NULL)delete m_pcLastRecord;
		if (m_pcInfo1 != NULL) delete m_pcInfo1;
		if (m_pcInfo2 != NULL) delete m_pcInfo2;

		m_pcInfo1 = NULL;
		m_pcInfo2 = NULL;
		m_pcLastPlay = NULL;
		m_pcLastRecord = NULL;
		m_pcBrowser = NULL;
		m_pcFilter = NULL;

		return (false);
	}

	m_pcFilter->setBackGroundRadius(RADIUS_LARGE);

	m_pcInfo1->setBackGroundColor(COL_MENUCONTENTDARK_PLUS_0);
	m_pcInfo1->setTextColor(COL_MENUCONTENTDARK_TEXT);
	m_pcInfo1->setBackGroundRadius(RADIUS_LARGE);
	//m_pcInfo1->setTextBorderWidth(OFFSET_INNER_MID, OFFSET_INNER_SMALL);
	m_pcInfo2->setTextBorderWidth(OFFSET_INNER_MID, OFFSET_INNER_SMALL);

	clearSelection();
	if (m_file_info_stale == true) {
		loadMovies();
	} else {
		refreshBrowserList();
		refreshLastPlayList();
		refreshLastRecordList();
		refreshFilterList();
	}

	// get old movie selection and set position in windows
	m_currentBrowserSelection = m_prevBrowserSelection;
	m_currentRecordSelection = m_prevRecordSelection;
	m_currentPlaySelection = m_prevPlaySelection;

	m_pcBrowser->setSelectedLine(m_currentBrowserSelection);
	m_pcLastRecord->setSelectedLine(m_currentRecordSelection);
	m_pcLastPlay->setSelectedLine(m_currentPlaySelection);

	updateMovieSelection();

	refreshTitle();
	refreshFoot();
	refreshLCD();
	if (m_settings.gui == MB_GUI_FILTER)
		m_settings.gui = MB_GUI_MOVIE_INFO;
	onSetGUIWindow(m_settings.gui);

	return (true);
}

void CMovieBrowser::refresh(void)
{
	TRACE("[mb]->%s\n", __func__);

	refreshTitle();

	if (m_pcBrowser != NULL && m_showBrowserFiles == true)
		 m_pcBrowser->refresh();
	if (m_pcLastPlay != NULL && m_showLastPlayFiles == true)
		m_pcLastPlay->refresh();
	if (m_pcLastRecord != NULL && m_showLastRecordFiles == true)
		 m_pcLastRecord->refresh();
	if (m_pcInfo1 != NULL && m_pcInfo2 != NULL && m_showMovieInfo == true)
		refreshMovieInfo();
	if (m_pcFilter != NULL && m_showFilter == true)
		m_pcFilter->refresh();

	refreshFoot();
	refreshLCD();

	m_doRefresh = false;
}

std::string CMovieBrowser::getCurrentDir(void)
{
	return(m_selectedDir);
}

CFile* CMovieBrowser::getSelectedFile(void)
{
	//TRACE("[mb]->%s: %s\n", __func__, m_movieSelectionHandler->file.Name.c_str());

	if (m_movieSelectionHandler != NULL)
		return(&m_movieSelectionHandler->file);
	else
		return(NULL);
}

bool CMovieBrowser::getSelectedFiles(CFileList &flist, P_MI_MOVIE_LIST &mlist)
{
	flist.clear();
	mlist.clear();

	P_MI_MOVIE_LIST *handle_list = &m_vHandleBrowserList;
	if (m_windowFocus == MB_FOCUS_LAST_PLAY)
		handle_list = &m_vHandlePlayList;
	if (m_windowFocus == MB_FOCUS_LAST_RECORD)
		handle_list = &m_vHandleRecordList;

	for (unsigned int i = 0; i < handle_list->size(); i++) {
		if ((*handle_list)[i]->marked) {
			flist.push_back((*handle_list)[i]->file);
			mlist.push_back((*handle_list)[i]);
		}
	}
	return (!flist.empty());
}

std::string CMovieBrowser::getScreenshotName(std::string movie, bool is_dir)
{
	std::string ext;
	std::string ret;
	size_t found;

	if (is_dir)
		found = movie.size();
	else
		found = movie.find_last_of(".");

	if (found == std::string::npos)
		return "";

	std::vector<std::string>::iterator it = PicExts.begin();
	while (it < PicExts.end()) {
		ret = movie;
		ext = *it;
		ret.replace(found, ret.length() - found, ext);
		++it;
		if (!access(ret, F_OK))
			return ret;
	}
	return "";
}

void CMovieBrowser::refreshChannelLogo(void)
{
	TRACE("[mb]->%s:%d\n", __func__, __LINE__);

	// set channel logo
	if (g_settings.channellist_show_channellogo)
	{
		m_header->setChannelLogo(m_movieSelectionHandler->epgId >> 16, m_movieSelectionHandler->channelName, (CCHeaderTypes::cc_logo_alignment_t)g_settings.channellist_show_channellogo);
	}

	if (old_EpgId != m_movieSelectionHandler->epgId >> 16 || old_ChannelName != m_movieSelectionHandler->channelName)
	{
		refreshTitle();
	}
}

void CMovieBrowser::initMovieCover(void)
{
	TRACE("[mb]->%s:%d\n", __func__, __LINE__);

	CBox movieCoverBox;

	int cover_w = 0;
	int cover_h = 0;

	if (m_windowFocus == MB_FOCUS_BROWSER && m_settings.browserAdditional)
	{
		movieCoverBox = m_cBoxFrameInfo2;

		// we have to align cover to box width
		cover_w = movieCoverBox.iWidth - 2*OFFSET_INNER_MID - OFFSET_SHADOW;
	}
	else
	{
		movieCoverBox = m_cBoxFrameInfo1;

		// we have to align cover to box height
		cover_h = movieCoverBox.iHeight - 2*OFFSET_INNER_MID - OFFSET_SHADOW;
	}

	std::string cover_file = getScreenshotName(m_movieSelectionHandler->file.Name, S_ISDIR(m_movieSelectionHandler->file.Mode));
	if ((cover_file.empty()) && (m_movieSelectionHandler->file.Name.length() > 18))
	{
		std::string cover = m_movieSelectionHandler->file.Name;
		cover.replace((cover.length()-18), 15, ""); //covername without yyyymmdd_hhmmss
		cover_file = getScreenshotName(cover);
	}

	bool got_cover = !cover_file.empty();

	if (m_movieCover)
	{
		if (m_movieCover->getPictureName() != cover_file || !got_cover)
		{
			delete m_movieCover;
			m_movieCover = NULL;
		}
	}

	if (m_settings.gui != MB_GUI_FILTER && got_cover)
	{
		if (m_movieCover == NULL)
		{
			m_movieCover = new CComponentsPicture(0, 0, cover_file, NULL, CC_SHADOW_ON, COL_MENUCONTENTDARK_PLUS_0, 0, COL_SHADOW_PLUS_0,  CFrameBuffer::TM_NONE);
			m_movieCover->enableFrame(true, 1);
		}

		// always align positions and dimensions
		if (m_windowFocus == MB_FOCUS_BROWSER && m_settings.browserAdditional)
		{
			if (m_movieCover->getWidth() < m_movieCover->getHeight())
				cover_w /= 2; // cover is upright, so we use just half width first

			m_movieCover->setHeight(0); // force recalculation
TRACE("[mb]->%s:%d m_movieCover->getHeight(): %d\n", __func__, __LINE__, m_movieCover->getHeight());
			m_movieCover->setWidth(cover_w);
			if (m_movieCover->getHeight() > movieCoverBox.iHeight/3)
				m_movieCover->setHeight(movieCoverBox.iHeight/3); // use maximal one third of box height

			m_movieCover->setXPos(movieCoverBox.iX + (movieCoverBox.iWidth - m_movieCover->getWidth())/2);
			m_movieCover->setYPos(movieCoverBox.iY + OFFSET_INNER_MID);
		}
		else
		{
			m_movieCover->setWidth(0); // force recalculation
TRACE("[mb]->%s:%d m_movieCover->getWidth(): %d\n", __func__, __LINE__, m_movieCover->getWidth());
			m_movieCover->setHeight(cover_h);
			if (m_movieCover->getWidth() > movieCoverBox.iWidth/3)
				m_movieCover->setWidth(movieCoverBox.iWidth/3); // use maximal one third of box width

			m_movieCover->setXPos(movieCoverBox.iX + movieCoverBox.iWidth - m_movieCover->getWidth() - 2*OFFSET_INNER_MID - OFFSET_SHADOW);
			m_movieCover->setYPos(movieCoverBox.iY + (movieCoverBox.iHeight - m_movieCover->getHeight())/2);
		}

		if (!m_movieSelectionHandler->epgInfo2.empty())
		{
			if (m_pcInfo1->OnAfterScrollPage.empty())
			{
				//m_movieCover->enableCache();
				m_pcInfo1->OnAfterScrollPage.connect(sigc::mem_fun(m_movieCover, &CComponentsPicture::paint0));
			}
		}
	}
}

void CMovieBrowser::refreshMovieCover(void)
{
	TRACE("[mb]->%s:%d\n", __func__, __LINE__);

	if (m_movieCover)
		m_movieCover->paint(CC_SAVE_SCREEN_YES);
}

void CMovieBrowser::hideMovieCover(void)
{
	TRACE("[mb]->%s:%d\n", __func__, __LINE__);

	if (m_movieCover)
		m_movieCover->hide();
}

void CMovieBrowser::refreshMovieInfo(void)
{
	TRACE("[mb]->%s m_vMovieInfo.size %d\n", __func__, (int)m_vMovieInfo.size());

	// clear m_pcInfo1 text before new init
	m_pcInfo1->clear();

	// clear m_pcInfo2 text, reset position and dimensions before new init
	m_pcInfo2->movePosition(m_cBoxFrameInfo2.iX, m_cBoxFrameInfo2.iY);
	m_pcInfo2->setWindowMaxDimensions(m_cBoxFrameInfo2.iWidth, m_cBoxFrameInfo2.iHeight);
	m_pcInfo2->setWindowMinDimensions(m_cBoxFrameInfo2.iWidth, m_cBoxFrameInfo2.iHeight);
	m_pcInfo2->clear();

	if (m_windowFocus == MB_FOCUS_BROWSER && m_settings.browserAdditional)
		m_pcInfo1->setTextFont(g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_LIST]);
	else
		m_pcInfo1->setTextFont(g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_INFO]);

	if (m_vMovieInfo.empty() || m_movieSelectionHandler == NULL)
		return;

	refreshChannelLogo();

	initMovieCover();
	int cover_x_offset = 0;
	int cover_y_offset = 0;
	if (m_movieCover)
	{
		cover_x_offset = m_movieCover->getWidth();
		cover_y_offset = m_movieCover->getHeight();
		if (cover_x_offset)
			cover_x_offset += 2*OFFSET_INNER_MID;
		if (cover_y_offset)
			cover_y_offset += 2*OFFSET_INNER_MID;
	}

	std::string pcInfo1_content = " ";
	if (m_windowFocus == MB_FOCUS_BROWSER && m_settings.browserAdditional)
	{
		m_pcInfo2->setWindowMaxDimensions(m_cBoxFrameInfo2.iWidth, m_cBoxFrameInfo2.iHeight - cover_y_offset);
		m_pcInfo2->setWindowMinDimensions(m_cBoxFrameInfo2.iWidth, m_cBoxFrameInfo2.iHeight - cover_y_offset);
		m_pcInfo2->movePosition(m_cBoxFrameInfo2.iX, m_cBoxFrameInfo2.iY + cover_y_offset);
		m_pcInfo2->setText(&m_movieSelectionHandler->epgInfo2);

		pcInfo1_content = m_movieSelectionHandler->epgInfo1;
		pcInfo1_content += "\n";
		pcInfo1_content += m_movieSelectionHandler->channelName;
	}
	else
	{
		pcInfo1_content  = m_movieSelectionHandler->epgInfo2;
	}
	m_pcInfo1->setText(&pcInfo1_content, m_cBoxFrameInfo1.iWidth - cover_x_offset);

	updateInfoSelection();

	refreshMovieCover();

	if (m_windowFocus == MB_FOCUS_BROWSER)
		refreshDetailsLine(m_pcBrowser->getSelectedLineRel());
}

void CMovieBrowser::hideDetailsLine()
{
	if (m_detailsLine)
		m_detailsLine->hide();
}

void CMovieBrowser::refreshDetailsLine(int pos)
{
	if (pos >= 0)
	{
		int fheight = g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_LIST]->getHeight();
		int hheight = m_pcBrowser->getHeaderListHeight();
		int theight = m_pcBrowser->getTitleHeight();

		int xpos  = m_cBoxFrameBrowserList.iX - DETAILSLINE_WIDTH;
		int ypos1 = m_cBoxFrameBrowserList.iY + hheight + theight + pos*fheight + (fheight/2);
		int ypos2 = m_cBoxFrameInfo1.iY + (m_cBoxFrameInfo1.iHeight/2);

		if (m_detailsLine == NULL)
			m_detailsLine = new CComponentsDetailsLine();

		m_detailsLine->setDimensionsAll(xpos, ypos1, ypos2, fheight/2, m_cBoxFrameInfo1.iHeight-2*RADIUS_LARGE);
		m_detailsLine->paint(true);
	}
}

void CMovieBrowser::refreshHDDLevel(bool show)
{
	TRACE("[mb]->%s:%d\n", __func__, __LINE__);

	int percent_used = 0;
	struct statfs s;

	if (getSelectedFile() != NULL)
	{
		if (::statfs(getSelectedFile()->Name.c_str(), &s) == 0)
		{
			if (s.f_blocks > 0)
			{
				uint64_t bytes_total = s.f_blocks * s.f_bsize;
				uint64_t bytes_free  = s.f_bfree  * s.f_bsize;
				uint64_t bytes_used = bytes_total - bytes_free;
				percent_used = (bytes_used * 200 + bytes_total) / 2 / bytes_total;
			}
		}
	}

	if (show)
		m_header->enableProgessBar(percent_used);
	else
		m_header->setProgessBar(percent_used);
}

void CMovieBrowser::refreshLCD(void)
{
	if (m_vMovieInfo.empty() || m_movieSelectionHandler == NULL)
	{
#ifdef ENABLE_LCD4LINUX
		if (g_settings.lcd4l_support)
			CLCD4l::getInstance()->CreateEventFile("", g_settings.lcd4l_convert);
#endif
		CVFD::getInstance()->showMenuText(0, "", -1, true); // UTF-8
	}
	else
	{
#ifdef ENABLE_LCD4LINUX
		if (g_settings.lcd4l_support)
			CLCD4l::getInstance()->CreateEventFile(m_movieSelectionHandler->epgTitle.c_str(), g_settings.lcd4l_convert);
#endif
		CVFD::getInstance()->showMenuText(0, m_movieSelectionHandler->epgTitle.c_str(), -1, true); // UTF-8
	}
}

void CMovieBrowser::refreshFilterList(void)
{
	TRACE("[mb]->refreshFilterList %d\n",m_settings.filter.item);

	std::string string_item;

	m_FilterLines.rows = 1;
	m_pcFilter->cleanupRow(&m_FilterLines, 0);
	m_FilterLines.rowWidth[0] = 100;
	m_FilterLines.lineHeader[0] = "";

	if (m_vMovieInfo.empty())
		return; // exit here if nothing else is to do

	if (m_settings.filter.item == MB_INFO_MAX_NUMBER)
	{
		// show Main List
		string_item = g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_GENRE_MAJOR);
		m_pcFilter->addLine2Row(&m_FilterLines, 0, string_item);

		string_item = g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_INFO1);
		m_pcFilter->addLine2Row(&m_FilterLines, 0, string_item);

		string_item = g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_PATH);
		m_pcFilter->addLine2Row(&m_FilterLines, 0, string_item);

		string_item = g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_SERIE);
		m_pcFilter->addLine2Row(&m_FilterLines, 0, string_item);
	}
	else
	{
		std::string tmp = g_Locale->getText(LOCALE_MOVIEBROWSER_FILTER_OFF);
		m_pcFilter->addLine2Row(&m_FilterLines, 0, tmp);

		if (m_settings.filter.item == MB_INFO_FILEPATH)
		{
			for (unsigned int i = 0 ; i < m_dirNames.size(); i++)
				m_pcFilter->addLine2Row(&m_FilterLines, 0, m_dirNames[i]);
		}
		else if (m_settings.filter.item == MB_INFO_INFO1)
		{
			for (unsigned int i = 0; i < m_vMovieInfo.size(); i++)
			{
				bool found = false;
				for (unsigned int t = 0; t < m_FilterLines.lineArray[0].v_text.size() && found == false; t++)
				{
					if (strcmp(m_FilterLines.lineArray[0].v_text[t].c_str(),m_vMovieInfo[i].epgInfo1.c_str()) == 0)
						found = true;
				}
				if (found == false)
					m_pcFilter->addLine2Row(&m_FilterLines, 0, m_vMovieInfo[i].epgInfo1);
			}
		}
		else if (m_settings.filter.item == MB_INFO_MAJOR_GENRE)
		{
			for (int i = 0; i < GENRE_ALL_COUNT; i++)
			{
				std::string tmpl = g_Locale->getText(GENRE_ALL[i].value);
				m_pcFilter->addLine2Row(&m_FilterLines, 0, tmpl);
			}
		}
		else if (m_settings.filter.item == MB_INFO_SERIE)
		{
			updateSerienames();
			for (unsigned int i = 0; i < m_vHandleSerienames.size(); i++)
				m_pcFilter->addLine2Row(&m_FilterLines, 0, m_vHandleSerienames[i]->serieName);
		}
	}
	m_pcFilter->setLines(&m_FilterLines);
}

void CMovieBrowser::refreshLastPlayList(void) //P2
{
	//TRACE("[mb]->refreshlastPlayList \n");
	std::string string_item;

	// Initialise and clear list array
	m_playListLines.rows = m_settings.lastPlayRowNr;
	for (int row = 0 ;row < m_settings.lastPlayRowNr; row++)
	{
		m_pcLastPlay->cleanupRow(&m_playListLines, row);
		m_playListLines.rowWidth[row] = m_settings.lastPlayRowWidth[row];
		m_playListLines.lineHeader[row] = g_Locale->getText(m_localizedItemName[m_settings.lastPlayRow[row]]);
	}
	m_playListLines.marked.clear();
	m_vHandlePlayList.clear();

	if (m_vMovieInfo.empty()) {
		if (m_pcLastPlay != NULL)
			m_pcLastPlay->setLines(&m_playListLines);
		return; // exit here if nothing else is to do
	}

	MI_MOVIE_INFO* movie_handle;
	// prepare Browser list for sorting and filtering
	for (unsigned int file = 0; file < m_vMovieInfo.size(); file++)
	{
		if (isParentalLock(m_vMovieInfo[file]) == false)
		{
			movie_handle = &(m_vMovieInfo[file]);
			m_vHandlePlayList.push_back(movie_handle);
		}
	}
	// sort the not filtered files
	onSortMovieInfoHandleList(m_vHandlePlayList,MB_INFO_PREVPLAYDATE,MB_DIRECTION_DOWN);

	for (int handle=0; handle < (int) m_vHandlePlayList.size() && handle < m_settings.lastPlayMaxItems ;handle++)
	{
		for (int row = 0; row < m_settings.lastPlayRowNr ;row++)
		{
			if (getMovieInfoItem(*m_vHandlePlayList[handle], m_settings.lastPlayRow[row], &string_item) == false)
			{
				string_item = "n/a";
				if (m_settings.lastPlayRow[row] == MB_INFO_TITLE)
					getMovieInfoItem(*m_vHandlePlayList[handle], MB_INFO_FILENAME, &string_item);
			}
			m_pcLastPlay->addLine2Row(&m_playListLines, row, string_item);
		}
		m_playListLines.marked.push_back(m_vHandlePlayList[handle]->marked);
	}
	m_pcLastPlay->setLines(&m_playListLines);
	m_pcLastPlay->cutRowText(&m_settings.browserCutLongRowText);

	m_currentPlaySelection = m_pcLastPlay->getSelectedLine();
	// update selected movie if browser is in the focus
	if (m_windowFocus == MB_FOCUS_LAST_PLAY)
		updateMovieSelection();
}

void CMovieBrowser::refreshLastRecordList(void) //P2
{
	//TRACE("[mb]->refreshLastRecordList \n");
	std::string string_item;

	// Initialise and clear list array
	m_recordListLines.rows = m_settings.lastRecordRowNr;
	for (int row = 0 ;row < m_settings.lastRecordRowNr; row++)
	{
		m_pcLastRecord->cleanupRow(&m_recordListLines, row);
		m_recordListLines.rowWidth[row] = m_settings.lastRecordRowWidth[row];
		m_recordListLines.lineHeader[row] = g_Locale->getText(m_localizedItemName[m_settings.lastRecordRow[row]]);
	}
	m_recordListLines.marked.clear();
	m_vHandleRecordList.clear();

	if (m_vMovieInfo.empty()) {
		if (m_pcLastRecord != NULL)
			m_pcLastRecord->setLines(&m_recordListLines);
		return; // exit here if nothing else is to do
	}

	MI_MOVIE_INFO* movie_handle;
	// prepare Browser list for sorting and filtering
	for (unsigned int file = 0; file < m_vMovieInfo.size(); file++)
	{
		if (isParentalLock(m_vMovieInfo[file]) == false)
		{
			movie_handle = &(m_vMovieInfo[file]);
			m_vHandleRecordList.push_back(movie_handle);
		}
	}
	// sort the not filtered files
	onSortMovieInfoHandleList(m_vHandleRecordList,MB_INFO_RECORDDATE,MB_DIRECTION_DOWN);

	for (int handle=0; handle < (int) m_vHandleRecordList.size() && handle < m_settings.lastRecordMaxItems ;handle++)
	{
		for (int row = 0; row < m_settings.lastRecordRowNr ;row++)
		{
			if (getMovieInfoItem(*m_vHandleRecordList[handle], m_settings.lastRecordRow[row], &string_item) == false)
			{
				string_item = "n/a";
				if (m_settings.lastRecordRow[row] == MB_INFO_TITLE)
					getMovieInfoItem(*m_vHandleRecordList[handle], MB_INFO_FILENAME, &string_item);
			}
			m_pcLastRecord->addLine2Row(&m_recordListLines, row, string_item);
		}
		m_recordListLines.marked.push_back(m_vHandleRecordList[handle]->marked);
	}

	m_pcLastRecord->setLines(&m_recordListLines);
	m_pcLastRecord->cutRowText(&m_settings.browserCutLongRowText);

	m_currentRecordSelection = m_pcLastRecord->getSelectedLine();
	// update selected movie if browser is in the focus
	if (m_windowFocus == MB_FOCUS_LAST_RECORD)
		updateMovieSelection();
}

void CMovieBrowser::refreshBrowserList(void) //P1
{
	TRACE("[mb]->%s\n", __func__);
	std::string string_item;

	// Initialise and clear list array
	m_browserListLines.rows = m_settings.browserRowNr;
	for (int row = 0; row < m_settings.browserRowNr; row++)
	{
		m_pcBrowser->cleanupRow(&m_browserListLines, row);
		m_browserListLines.rowWidth[row] = m_settings.browserRowWidth[row];
		m_browserListLines.lineHeader[row] = g_Locale->getText(m_localizedItemName[m_settings.browserRowItem[row]]);
	}
	m_browserListLines.Icon.clear();
	m_browserListLines.marked.clear();
	m_vHandleBrowserList.clear();

	if (m_vMovieInfo.empty())
	{
		m_currentBrowserSelection = 0;
		m_movieSelectionHandler = NULL;
		if (m_pcBrowser != NULL)
			m_pcBrowser->setLines(&m_browserListLines);//FIXME last delete test
		return; // exit here if nothing else is to do
	}

	MI_MOVIE_INFO* movie_handle;
	// prepare Browser list for sorting and filtering
	for (unsigned int file=0; file < m_vMovieInfo.size(); file++)
	{
		if (isFiltered(m_vMovieInfo[file]) == false &&
				isParentalLock(m_vMovieInfo[file]) == false &&
				(m_settings.browser_serie_mode == 0 || m_vMovieInfo[file].serieName.empty() || m_settings.filter.item == MB_INFO_SERIE))
		{
			movie_handle = &(m_vMovieInfo[file]);
			m_vHandleBrowserList.push_back(movie_handle);
		}
	}
	// sort the not filtered files
	onSortMovieInfoHandleList(m_vHandleBrowserList,m_settings.sorting.item,MB_DIRECTION_AUTO);



	for (unsigned int handle=0; handle < m_vHandleBrowserList.size() ;handle++)
	{
		for (int row = 0; row < m_settings.browserRowNr ;row++)
		{
			if (getMovieInfoItem(*m_vHandleBrowserList[handle], m_settings.browserRowItem[row], &string_item) == false)
			{
				string_item = "n/a";
				if (m_settings.browserRowItem[row] == MB_INFO_TITLE)
					getMovieInfoItem(*m_vHandleBrowserList[handle], MB_INFO_FILENAME, &string_item);
			}

			CProgressBar* elapsed = NULL;
			CComponentsPicture* seen = NULL;
			if (m_settings.browserRowItem[row] == MB_INFO_PERCENT_ELAPSED)
			{
				getMovieInfoItem(*m_vHandleBrowserList[handle], MB_INFO_PERCENT_ELAPSED, &string_item);

				/*
				 * NOTE: Get threshold offset from record safety settings to trigger the "seen" tag.
				 * Better solutions are welcome!
				*/
				int pre = 0, post = 0;
				float trigger_offset = 0;
				g_Timerd->getRecordingSafety(pre,post);

				if (post < 120)
					post = 120;

				if (m_vHandleBrowserList[handle]->length * 60 > post)
					trigger_offset = round((float)post * 100.0 / (m_vHandleBrowserList[handle]->length * 60.0));

				int elapsed_percent = atoi(string_item);
				string_item.clear(); // reset not needed

				if ((float)elapsed_percent < 100.0-trigger_offset)
				{
					if (elapsed_percent > 0)
					{
						elapsed = new CProgressBar();
						elapsed->setHeight(m_pcBrowser->getLineHeight()/2);
						elapsed->setType(CProgressBar::PB_TIMESCALE);
						elapsed->setValues(elapsed_percent, 100);
						m_pcBrowser->addLine2Row(&m_browserListLines, row, string_item, elapsed);
					}
				}
				else
				{
					seen = new CComponentsPicture(0, 0, 0, m_pcBrowser->getLineHeight(), NEUTRINO_ICON_MARKER_DIALOG_OK);
					seen->doPaintBg(false);
					m_pcBrowser->addLine2Row(&m_browserListLines, row, string_item, seen);
				}
			}
			if (!elapsed && !seen)
				m_pcBrowser->addLine2Row(&m_browserListLines, row, string_item, NULL);
		}
		if (CRecordManager::getInstance()->getRecordInstance(m_vHandleBrowserList[handle]->file.Name) != NULL)
			m_browserListLines.Icon.push_back(NEUTRINO_ICON_MARKER_RECORD);
		else
			m_browserListLines.Icon.push_back("");
		m_browserListLines.marked.push_back(m_vHandleBrowserList[handle]->marked);
	}
	m_pcBrowser->setLines(&m_browserListLines);
	m_pcBrowser->cutRowText(&m_settings.browserCutLongRowText);

	m_currentBrowserSelection = m_pcBrowser->getSelectedLine();
	// update selected movie if browser is in the focus
	if (m_windowFocus == MB_FOCUS_BROWSER)
		updateMovieSelection();
}

void CMovieBrowser::refreshTitle(void)
{
	std::string title = m_textTitle.c_str();
	const char *icon = NEUTRINO_ICON_MOVIEPLAYER;

	TRACE("[mb]->refreshTitle: %s\n", title.c_str());

	int x = m_cBoxFrameTitleRel.iX + m_cBoxFrame.iX;
	int y = m_cBoxFrameTitleRel.iY + m_cBoxFrame.iY;
	int w = m_cBoxFrameTitleRel.iWidth;
	int h = m_cBoxFrameTitleRel.iHeight;

	if (!m_header){
		m_header = new CComponentsHeader(x, y, w, h, title.c_str(), icon, CComponentsHeader::CC_BTN_LEFT | CComponentsHeader::CC_BTN_RIGHT | CComponentsHeader::CC_BTN_HELP);
	}else{
		m_header->setCaption(title.c_str());
	}

	if (timeset)
	{
		m_header->enableClock();
		if (m_header->getClockObject())
			m_header->getClockObject()->setCorner(RADIUS_LARGE, CORNER_TOP_RIGHT);
	}

	m_header->paint(CC_SAVE_SCREEN_NO);
	newHeader = m_header->isPainted();

	refreshHDDLevel(true);
}

int CMovieBrowser::refreshFoot(bool show)
{
	//TRACE("[mb]->refreshButtonLine\n");
	int offset = (m_settings.gui != MB_GUI_LAST_PLAY && m_settings.gui != MB_GUI_LAST_RECORD) ? 0 : 2;
	neutrino_locale_t ok_loc = (m_settings.gui == MB_GUI_FILTER && m_windowFocus == MB_FOCUS_FILTER) ?  LOCALE_BOOKMARKMANAGER_SELECT : LOCALE_MOVIEBROWSER_FOOT_PLAY;
	int ok_loc_len = std::max(FOOT_FONT->getRenderWidth(g_Locale->getText(LOCALE_BOOKMARKMANAGER_SELECT), true),
				  FOOT_FONT->getRenderWidth(g_Locale->getText(LOCALE_MOVIEBROWSER_FOOT_PLAY), true));
	std::string filter_text = g_Locale->getText(LOCALE_MOVIEBROWSER_FOOT_FILTER);
	filter_text += " ";
	filter_text += m_settings.filter.optionString;
	std::string sort_text = g_Locale->getText(LOCALE_MOVIEBROWSER_FOOT_SORT);
	sort_text += " ";
	int sort_text_len = FOOT_FONT->getRenderWidth(sort_text, true);
	int len = 0;
	for (int i = 0; m_localizedItemName[i] != NONEXISTANT_LOCALE; i++)
		len = std::max(len, FOOT_FONT->getRenderWidth(g_Locale->getText(m_localizedItemName[i]), true));
	sort_text_len += len;
	sort_text += g_Locale->getText(m_localizedItemName[m_settings.sorting.item]);

	button_label_ext footerButtons[] = {
		{ NEUTRINO_ICON_BUTTON_RED,		NONEXISTANT_LOCALE,			sort_text.c_str(),	sort_text_len,	false },
		{ NEUTRINO_ICON_BUTTON_GREEN,		NONEXISTANT_LOCALE,			filter_text.c_str(),	0,		true  },
		{ NEUTRINO_ICON_BUTTON_YELLOW,		LOCALE_MOVIEBROWSER_FOOT_FOCUS,		NULL,			0,		false },
		{ NEUTRINO_ICON_BUTTON_BLUE,		LOCALE_MOVIEBROWSER_FOOT_REFRESH,	NULL,			0,		false },
		{ NEUTRINO_ICON_BUTTON_OKAY,		ok_loc,					NULL,			ok_loc_len,	false },
		{ NEUTRINO_ICON_BUTTON_MUTE_SMALL,	LOCALE_FILEBROWSER_DELETE,		NULL,			0,		false },
		{ NEUTRINO_ICON_BUTTON_PLAY,		LOCALE_FILEBROWSER_MARK,		NULL,			0,		false },
		{ NEUTRINO_ICON_BUTTON_MENU_SMALL,	LOCALE_MOVIEBROWSER_FOOT_OPTIONS,	NULL,			0,		false }
	};
	int cnt = sizeof(footerButtons) / sizeof(button_label_ext);

	if (show)
		return paintButtons(footerButtons + offset, cnt - offset, m_cBoxFrameFootRel.iX, m_cBoxFrameFootRel.iY, m_cBoxFrameFootRel.iWidth, m_cBoxFrameFootRel.iHeight, m_cBoxFrameFootRel.iWidth);
	else
		return paintButtons(footerButtons, cnt, 0, 0, 0, 0, 0, false, NULL, NULL);
}

bool CMovieBrowser::onButtonPress(neutrino_msg_t msg)
{
//	TRACE("[mb]->onButtonPress %d\n",msg);
	bool result = onButtonPressMainFrame(msg);
	if (result == false)
	{
		// if Main Frame didnot process the button, the focused window may do
		if (m_windowFocus == MB_FOCUS_BROWSER)
			result = onButtonPressBrowserList(msg);
		else if (m_windowFocus == MB_FOCUS_LAST_PLAY)
			result = onButtonPressLastPlayList(msg);
		else if (m_windowFocus == MB_FOCUS_LAST_RECORD)
			result = onButtonPressLastRecordList(msg);
		else if (m_windowFocus == MB_FOCUS_MOVIE_INFO1 || m_windowFocus == MB_FOCUS_MOVIE_INFO2)
			result = onButtonPressMovieInfoList(msg);
		else if (m_windowFocus == MB_FOCUS_FILTER)
			result = onButtonPressFilterList(msg);
	}
	return (result);
}

bool CMovieBrowser::onButtonPressMainFrame(neutrino_msg_t msg)
{
	//TRACE("[mb]->onButtonPressMainFrame: %d\n",msg);
	bool result = true;
	neutrino_msg_data_t data;

	if (msg == (neutrino_msg_t) g_settings.mbkey_copy_onefile
	 || msg == (neutrino_msg_t) g_settings.mbkey_copy_several
	 || msg == (neutrino_msg_t) g_settings.mbkey_cut
	 || msg == (neutrino_msg_t) g_settings.mbkey_truncate)
	{
		if (msg == (neutrino_msg_t) g_settings.mbkey_copy_onefile)
			exec(NULL, "copy_onefile");
		else
		if (msg == (neutrino_msg_t) g_settings.mbkey_copy_several)
			exec(NULL, "copy_several");
		else
		if (msg == (neutrino_msg_t) g_settings.mbkey_cut)
			exec(NULL, "cut");
		else
		if (msg == (neutrino_msg_t) g_settings.mbkey_truncate)
			exec(NULL, "truncate");

		if (m_doLoadMovies)
			loadMovies();
		if (m_doRefresh)
			refresh();
	}
	else if (msg == (neutrino_msg_t) g_settings.mbkey_cover)
	{
		if (m_movieSelectionHandler != NULL) {
			std::string cover_file = getScreenshotName(m_movieSelectionHandler->file.Name, S_ISDIR(m_movieSelectionHandler->file.Mode));
			if (!cover_file.empty())
			{
				//delete Cover
				if (ShowMsg(LOCALE_MESSAGEBOX_INFO, LOCALE_MOVIEBROWSER_DELETE_SCREENSHOT, CMsgBox::mbrNo, CMsgBox:: mbYes | CMsgBox::mbNo) == CMsgBox::mbrYes)
				{
					unlink(cover_file.c_str());
					refresh();
				}
			}
			else if (g_settings.tmdb_enabled)
			{
				//add TMDB Cover
				cover_file = m_movieSelectionHandler->file.Name.c_str();
				int ext_pos = 0;
				ext_pos = cover_file.rfind('.');
				if( ext_pos > 0) {
					std::string extension;
					extension = cover_file.substr(ext_pos + 1, cover_file.length() - ext_pos);
					extension = "." + extension;
					str_replace(extension, ".jpg", cover_file);
					printf("TMDB: %s : %s\n",m_movieSelectionHandler->file.Name.c_str(),cover_file.c_str());
					CTMDB* tmdb = CTMDB::getInstance();
					if (tmdb)
					{
						tmdb->setTitle(m_movieSelectionHandler->epgTitle);
						if ((tmdb->getResults() > 0) && (tmdb->hasPoster()))
						{
							if (!cover_file.empty())
								if (tmdb->getSmallPoster(cover_file))
									refresh();
						}
						tmdb->cleanup();
						tmdb = NULL;
					}
				}
			}
		}
	}
	else if (msg == (neutrino_msg_t) g_settings.mbkey_toggle_view_cw)
	{
		if (m_settings.browserAdditional && m_windowFocus == MB_FOCUS_BROWSER)
			onSetFocusNext();
		else
			onSetGUIWindowNext();
	}
	else if (msg == (neutrino_msg_t) g_settings.mbkey_toggle_view_ccw)
	{
		if (m_settings.browserAdditional && m_windowFocus == MB_FOCUS_MOVIE_INFO2)
			onSetFocusNext();
		else
			onSetGUIWindowPrev();
	}
	else if (CNeutrinoApp::getInstance()->backKey(msg))
	{
		if (m_settings.gui == MB_GUI_FILTER)
			onSetGUIWindow(MB_GUI_MOVIE_INFO);
		else
			result = false;
	}
	else if (msg == CRCInput::RC_green)
	{
		if (m_settings.gui == MB_GUI_MOVIE_INFO)
			onSetGUIWindow(MB_GUI_FILTER);
		else if (m_settings.gui == MB_GUI_FILTER)
			onSetGUIWindow(MB_GUI_MOVIE_INFO);
		// no effect if gui is last play or record
	}
	else if (msg == CRCInput::RC_yellow)
	{
		onSetFocusNext();
	}
	else if (msg == CRCInput::RC_blue)
	{
		loadMovies();
		refresh();
	}
	else if (msg == CRCInput::RC_red)
	{
		if (m_settings.gui != MB_GUI_LAST_PLAY && m_settings.gui != MB_GUI_LAST_RECORD)
		{
			// sorting is not avialable for last play and record

			int directkey = 1;
			int selected = -1;
			CMenuSelectorTarget * selector = new CMenuSelectorTarget(&selected);

			CMenuWidget m(LOCALE_MOVIEBROWSER_FOOT_SORT, NEUTRINO_ICON_SETTINGS);
			m.addIntroItems();

			m.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_PREVPLAYDATE, true, NULL, selector, to_string(MB_INFO_PREVPLAYDATE).c_str(), CRCInput::RC_red));
			m.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_RECORDDATE, true, NULL, selector, to_string(MB_INFO_RECORDDATE).c_str(), CRCInput::RC_green));
			m.addItem(new CMenuSeparator(CMenuSeparator::LINE));

			// just show sorting options for displayed rows; sorted by rows
			for (int row = 0; row < MB_MAX_ROWS && row < m_settings.browserRowNr; row++)
			{
				for (unsigned int i = 0; i < MB_INFO_MAX_NUMBER; i++)
				{
					if (sortBy[i] == NULL)
						continue;

					// already added above
					if (i == MB_INFO_PREVPLAYDATE || i == MB_INFO_RECORDDATE)
						continue;

					if (m_settings.browserRowItem[row] == i)
						m.addItem(new CMenuForwarder(g_Locale->getText(m_localizedItemName[i]), true, NULL, selector, to_string(i).c_str(), CRCInput::convertDigitToKey(directkey++)));
				}
			}

			m.enableSaveScreen(true);
			m.exec(NULL, "");

			delete selector;

			if (selected >= 0)
			{
				m_settings.sorting.item = (MB_INFO_ITEM) selected;

				TRACE("[mb]->new sorting %d, %s\n", m_settings.sorting.item, g_Locale->getText(m_localizedItemName[m_settings.sorting.item]));

				refreshBrowserList();
				refreshMovieInfo();
				refreshFoot();
			}
		}
	}
	else if (msg == CRCInput::RC_spkr)
	{
		if (m_movieSelectionHandler != NULL)
		{
			onDelete();
			if (m_windowFocus == MB_FOCUS_LAST_PLAY)
				m_pcLastPlay->setSelectedLine(0);
		}
	}
	else if (msg == CRCInput::RC_help)
	{
		showHelp();
	}
	else if (msg == CRCInput::RC_info)
	{
		if (m_movieSelectionHandler != NULL)
		{
			m_header->kill();
			framebuffer->paintBackground(); //clear whole screen
			g_EpgData->show_mp(m_movieSelectionHandler);
			refresh();
		}
	}
	else if (msg == CRCInput::RC_setup)
	{
		showMenu();
		if (m_doLoadMovies)
			loadMovies();
		if (m_doRefresh)
			refresh();

		saveSettings(&m_settings);
	}
	else if (g_settings.sms_movie && (msg >= CRCInput::RC_1) && (msg <= CRCInput::RC_9))
	{
		unsigned char smsKey = 0;
		SMSKeyInput smsInput;
		smsInput.setTimeout(MOVIE_SMSKEY_TIMEOUT);

		std::vector<MI_MOVIE_INFO*> *current_list = NULL;
		CListFrame *current_frame = NULL;

		if (m_windowFocus == MB_FOCUS_BROWSER)
		{
			current_list = &m_vHandleBrowserList;
			current_frame = m_pcBrowser;
		}
		else if (m_windowFocus == MB_FOCUS_LAST_PLAY)
		{
			current_list = &m_vHandlePlayList;
			current_frame = m_pcLastPlay;
		}
		else if (m_windowFocus == MB_FOCUS_LAST_RECORD)
		{
			current_list = &m_vHandleRecordList;
			current_frame = m_pcLastRecord;
		}

		if (current_list == NULL || current_frame == NULL)
			return result;

		do {
			smsKey = smsInput.handleMsg(msg);
			printf("SMS new key: %c\n", smsKey);
			g_RCInput->getMsg_ms(&msg, &data, MOVIE_SMSKEY_TIMEOUT-100);
		} while ((msg >= CRCInput::RC_1) && (msg <= CRCInput::RC_9));

		int selected = current_frame->getSelectedLine();
		if (msg == CRCInput::RC_timeout || msg == CRCInput::RC_nokey) {
			uint32_t i;
			for (i = selected+1; i < (*current_list).size(); i++) {

				char firstCharOfTitle = (*current_list)[i]->epgTitle.c_str()[0];
				if (tolower(firstCharOfTitle) == smsKey) {
					printf("SMS found selected=%d i=%d \"%s\"\n", selected, i, (*current_list)[i]->epgTitle.c_str());
					break;
				}
			}
			if (i >= (*current_list).size()) {
				for (i = 0; i < (*current_list).size(); i++) {
					char firstCharOfTitle = (*current_list)[i]->epgTitle.c_str()[0];
					if (tolower(firstCharOfTitle) == smsKey) {
						printf("SMS found selected=%d i=%d \"%s\"\n", selected, i, (*current_list)[i]->epgTitle.c_str());
						break;
					}
				}
			}
			if (i < (*current_list).size()) {
				current_frame->setSelectedLine(i);
				updateMovieSelection();
			}

			smsInput.resetOldKey();
		}
	}
	else
	{
		//TRACE("[mb]->onButtonPressMainFrame none\n");
		result = false;
	}

	return (result);
}

void CMovieBrowser::markItem(CListFrame *list)
{
	if(m_movieSelectionHandler != NULL){
		m_movieSelectionHandler->marked = !m_movieSelectionHandler->marked;
		list->setSelectedMarked(m_movieSelectionHandler->marked);
		list->scrollLineDown(1);
	}
}

void CMovieBrowser::scrollBrowserItem(bool next, bool page)
{
	if (next)
		page ? m_pcBrowser->scrollPageDown(1) : m_pcBrowser->scrollLineDown(1);
	else
		page ? m_pcBrowser->scrollPageUp(1) : m_pcBrowser->scrollLineUp(1);
}

bool CMovieBrowser::onButtonPressBrowserList(neutrino_msg_t msg)
{
	//TRACE("[mb]->onButtonPressBrowserList %d\n",msg);
	bool result = true;

	if (msg == CRCInput::RC_up)
		scrollBrowserItem(false, false);
	else if (msg == CRCInput::RC_down)
		scrollBrowserItem(true, false);
	else if (msg == (neutrino_msg_t)g_settings.key_pageup)
		scrollBrowserItem(false, true);
	else if (msg == (neutrino_msg_t)g_settings.key_pagedown)
		scrollBrowserItem(true, true);
	else if (msg == CRCInput::RC_play || msg == CRCInput::RC_playpause)
		markItem(m_pcBrowser);
	else
		result = false;

	if (result == true)
		updateMovieSelection();

	return (result);
}

bool CMovieBrowser::onButtonPressLastPlayList(neutrino_msg_t msg)
{
	//TRACE("[mb]->onButtonPressLastPlayList %d\n",msg);
	bool result = true;

	if (msg==CRCInput::RC_up)
		m_pcLastPlay->scrollLineUp(1);
	else if (msg == CRCInput::RC_down)
		m_pcLastPlay->scrollLineDown(1);
	else if (msg == (neutrino_msg_t)g_settings.key_pageup)
		m_pcLastPlay->scrollPageUp(1);
	else if (msg == (neutrino_msg_t)g_settings.key_pagedown)
		m_pcLastPlay->scrollPageDown(1);
	else if (msg == CRCInput::RC_play || msg == CRCInput::RC_playpause)
		markItem(m_pcLastPlay);
	else
		result = false;

	if (result == true)
		updateMovieSelection();

	return (result);
}

bool CMovieBrowser::onButtonPressLastRecordList(neutrino_msg_t msg)
{
	//TRACE("[mb]->onButtonPressLastRecordList %d\n",msg);
	bool result = true;

	if (msg == CRCInput::RC_up)
		m_pcLastRecord->scrollLineUp(1);
	else if (msg == CRCInput::RC_down)
		m_pcLastRecord->scrollLineDown(1);
	else if (msg == (neutrino_msg_t)g_settings.key_pageup)
		m_pcLastRecord->scrollPageUp(1);
	else if (msg == (neutrino_msg_t)g_settings.key_pagedown)
		m_pcLastRecord->scrollPageDown(1);
	else if (msg == CRCInput::RC_play || msg == CRCInput::RC_playpause)
		markItem(m_pcLastRecord);
	else
		result = false;

	if (result == true)
		updateMovieSelection();

	return (result);
}

bool CMovieBrowser::onButtonPressFilterList(neutrino_msg_t msg)
{
	//TRACE("[mb]->onButtonPressFilterList %d,%d\n",msg,m_settings.filter.item);
	bool result = true;

	if (msg==CRCInput::RC_up)
	{
		m_pcFilter->scrollLineUp(1);
	}
	else if (msg == CRCInput::RC_down)
	{
		m_pcFilter->scrollLineDown(1);
	}
	else if (msg == (neutrino_msg_t)g_settings.key_pageup)
	{
		m_pcFilter->scrollPageUp(1);
	}
	else if (msg == (neutrino_msg_t)g_settings.key_pagedown)
	{
		m_pcFilter->scrollPageDown(1);
	}
	else if (msg == CRCInput::RC_ok)
	{
		int selected_line = m_pcFilter->getSelectedLine();
		if (m_settings.filter.item == MB_INFO_MAX_NUMBER)
		{
			if (selected_line == 0) m_settings.filter.item = MB_INFO_MAJOR_GENRE;
			if (selected_line == 1) m_settings.filter.item = MB_INFO_INFO1;
			if (selected_line == 2) m_settings.filter.item = MB_INFO_FILEPATH;
			if (selected_line == 3) m_settings.filter.item = MB_INFO_SERIE;
			refreshFilterList();
			m_pcFilter->setSelectedLine(0);
		}
		else
		{
			if (selected_line == 0)
			{
				m_settings.filter.item = MB_INFO_MAX_NUMBER;
				m_settings.filter.optionString = g_Locale->getText(LOCALE_OPTIONS_OFF);
				m_settings.filter.optionVar = 0;
				refreshFilterList();
				m_pcFilter->setSelectedLine(0);
				refreshBrowserList();
				refreshLastPlayList();
				refreshLastRecordList();
				refreshFoot();
			}
			else
			{
				updateFilterSelection();
			}
		}
	}
	else
	{
		// default
		result = false;
	}

	return (result);
}

bool CMovieBrowser::onButtonPressMovieInfoList(neutrino_msg_t msg)
{
//	TRACE("[mb]->onButtonPressEPGInfoList %d\n",msg);
	bool result = true;

	if (msg == CRCInput::RC_up)
		if (m_windowFocus == MB_FOCUS_MOVIE_INFO2 && m_settings.browserAdditional)
			m_pcInfo2->scrollPageUp(1);
		else
			m_pcInfo1->scrollPageUp(1);
	else if (msg == CRCInput::RC_down)
		if (m_windowFocus == MB_FOCUS_MOVIE_INFO2 && m_settings.browserAdditional)
			m_pcInfo2->scrollPageDown(1);
		else
			m_pcInfo1->scrollPageDown(1);
	else
		result = false;

	updateInfoSelection();

	return (result);
}

std::string CMovieBrowser::formatDeleteMsg(MI_MOVIE_INFO *movieinfo, int msgFont, const int boxWidth)
{
	Font *msgFont_ = g_Font[msgFont];
	int msgWidth = boxWidth - 20;
	std::string msg = g_Locale->getText(LOCALE_FILEBROWSER_DODELETE1);
	msg += "\n";

	if (!movieinfo->epgTitle.empty()) {
		int titleW = msgFont_->getRenderWidth(movieinfo->epgTitle);
		int infoW = 0;
		int zW = 0;
		if (!movieinfo->epgInfo1.empty()) {
			infoW = msgFont_->getRenderWidth(movieinfo->epgInfo1);
			zW = msgFont_->getRenderWidth(" ()");
		}

		if ((titleW+infoW+zW) <= msgWidth) {
			/* one line */
			msg += trim(movieinfo->epgTitle);
			if (!movieinfo->epgInfo1.empty()) {
				msg += " (";
				msg += trim(movieinfo->epgInfo1);
				msg += ")";
			}
		}
		else {
			/* two lines */
			msg += cutString(movieinfo->epgTitle, msgFont, msgWidth);
			if (!movieinfo->epgInfo1.empty()) {
				msg += "\n(";
				msg += cutString(movieinfo->epgInfo1, msgFont, msgWidth);
				msg += ")";
			}
		}
	}
	else
		msg += cutString(movieinfo->file.Name, msgFont, msgWidth);

	msg += "\n";
	msg += g_Locale->getText(LOCALE_FILEBROWSER_DODELETE2);

	return msg;
}

bool CMovieBrowser::onDeleteFile(MI_MOVIE_INFO *movieinfo, bool skipAsk)
{
	//TRACE("[onDeleteFile] ");
	bool result = false;

	/* default font for ShowMsg */
	int msgFont = SNeutrinoSettings::FONT_TYPE_MENU;
	/* default width for ShowMsg */
	int msgBoxWidth = 450;

	std::string msg = formatDeleteMsg(movieinfo, msgFont, msgBoxWidth);
	if ((skipAsk || !movieinfo->delAsk) || (ShowMsg(LOCALE_FILEBROWSER_DELETE, msg, CMsgBox::mbrYes, CMsgBox::mbYes|CMsgBox::mbNo, NULL, msgBoxWidth)==CMsgBox::mbrYes))
	{
		CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_MOVIEBROWSER_DELETE_INFO));
		hintBox.paint();
		delFile(movieinfo->file);

		std::string cover_file = getScreenshotName(movieinfo->file.Name, S_ISDIR(movieinfo->file.Mode));
		if (!cover_file.empty())
			unlink(cover_file.c_str());

		CFile file_xml = movieinfo->file;
		if (m_movieInfo.convertTs2XmlName(file_xml.Name))
			unlink(file_xml.Name.c_str());

		hintBox.hide();
		g_RCInput->clearRCMsg();

		TRACE("List size: %d\n", (int)m_vMovieInfo.size());
		for (std::vector<MI_MOVIE_INFO>::iterator mi_it = m_vMovieInfo.begin(); mi_it != m_vMovieInfo.end(); ++mi_it)
		{
			if (
				   mi_it->file.Name == movieinfo->file.Name
				&& mi_it->epgTitle == movieinfo->epgTitle
				&& ( mi_it->epgInfo1 == movieinfo->epgInfo1 || (mi_it->epgInfo1 == " " && movieinfo->epgInfo1.empty()) ) //FIXME if movieinfo->epgInfo1 is empty, epgInfo1 in xml have whitespace
				&& mi_it->length == movieinfo->length
			)
				m_vMovieInfo.erase(mi_it--);
		}

		updateSerienames();
		refreshBrowserList();
		refreshLastPlayList();
		refreshLastRecordList();
		refreshMovieInfo();
		refresh();

		result = true;
	}
	return (result);
}

bool CMovieBrowser::onDelete(bool cursor_only)
{
	//TRACE("[onDelete] ");
	bool result = false;

	MI_MOVIE_INFO *movieinfo;
	movieinfo = NULL;

	getSelectedFiles(filelist, movielist);

	printf("CMovieBrowser::onDelete(%s) filelist  size: %zd\n", cursor_only ? "true" : "false", filelist.size());
	printf("CMovieBrowser::onDelete(%s) movielist size: %zd\n", cursor_only ? "true" : "false", movielist.size());

	if (cursor_only || (filelist.empty() || movielist.empty()))
	{
		printf("CMovieBrowser::onDelete(%s) clearing the lists\n", cursor_only ? "true" : "false");

		filelist.clear();
		movielist.clear();

		printf("CMovieBrowser::onDelete(%s) add the m_movieSelectionHandler\n", cursor_only ? "true" : "false");

		// just add the m_movieSelectionHandler
		filelist.push_back(m_movieSelectionHandler->file);
		movielist.push_back(m_movieSelectionHandler);

		printf("CMovieBrowser::onDelete(%s) filelist  size: %zd\n", cursor_only ? "true" : "false", filelist.size());
		printf("CMovieBrowser::onDelete(%s) movielist size: %zd\n", cursor_only ? "true" : "false", movielist.size());
	}

	MI_MOVIE_LIST dellist;
	MI_MOVIE_LIST::iterator dellist_it;
	dellist.clear();
	unsigned int dellist_cnt = 0;
	for (filelist_it = filelist.begin(); filelist_it != filelist.end(); ++filelist_it)
	{
		unsigned int idx = filelist_it - filelist.begin();
		movieinfo = movielist[idx];
		TRACE("[mb]-> try to delete %d:%s\n", idx, movieinfo->file.Name.c_str());

		if ((!m_vMovieInfo.empty()) && (movieinfo != NULL)) {
			bool toDelete = true;
			CRecordInstance* inst = CRecordManager::getInstance()->getRecordInstance(movieinfo->file.Name);
			if (inst != NULL) {
				std::string delName = movieinfo->epgTitle;
				if (delName.empty())
					delName = movieinfo->file.getFileName();
				char buf1[1024];
				snprintf(buf1, sizeof(buf1), g_Locale->getText(LOCALE_MOVIEBROWSER_ASK_REC_TO_DELETE), delName.c_str());
				if (ShowMsg(LOCALE_RECORDINGMENU_RECORD_IS_RUNNING, buf1,
						CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo, NULL, 450, 30, false) == CMsgBox::mbrNo)
					toDelete = false;
				else {
					CTimerd::RecordingStopInfo recinfo;
					recinfo.channel_id = inst->GetChannelId();
					recinfo.eventID = inst->GetRecordingId();
					CRecordManager::getInstance()->Stop(&recinfo);
					g_Timerd->removeTimerEvent(recinfo.eventID);
					movieinfo->delAsk = false; //delete this file w/o any more question
				}
			}
			if (toDelete)
			{
				dellist.push_back(*movieinfo);
				dellist_cnt++;
			}
		}
	}
	if (!dellist.empty()) {
		bool skipAsk = false;
		if (dellist_cnt > 1)
			skipAsk = (ShowMsg(LOCALE_FILEBROWSER_DELETE, LOCALE_MOVIEBROWSER_DELETE_ALL, CMsgBox::mbrNo, CMsgBox:: mbYes | CMsgBox::mbNo) == CMsgBox::mbrYes);
		for (dellist_it = dellist.begin(); dellist_it != dellist.end(); ++dellist_it)
			result |= onDeleteFile((MI_MOVIE_INFO *)&(*dellist_it), skipAsk);
		dellist.clear();
	}
	return (result);
}

void CMovieBrowser::onSetGUIWindow(MB_GUI gui)
{
	TRACE("[mb]->onSetGUIWindow: gui %d -> %d\n", m_settings.gui, gui);
	m_settings.gui = gui;

	hideDetailsLine();

	m_showMovieInfo = true;
	if (gui == MB_GUI_MOVIE_INFO) {
		m_showBrowserFiles = true;

		m_showLastRecordFiles = false;
		m_showLastPlayFiles = false;
		m_showFilter = false;

		m_pcLastPlay->hide();
		m_pcLastRecord->hide();
		m_pcFilter->hide();
		m_pcBrowser->paint();
		onSetFocus(MB_FOCUS_BROWSER);
	} else if (gui == MB_GUI_LAST_PLAY) {
		clearSelection();

		m_showLastRecordFiles = true;
		m_showLastPlayFiles = true;

		m_showBrowserFiles = false;
		m_showFilter = false;

		m_pcBrowser->hide();
		m_pcFilter->hide();
		m_pcInfo2->hide();
		m_pcLastRecord->paint();
		m_pcLastPlay->paint();

		onSetFocus(MB_FOCUS_LAST_PLAY);
	} else if (gui == MB_GUI_LAST_RECORD) {
		clearSelection();

		m_showLastRecordFiles = true;
		m_showLastPlayFiles = true;

		m_showBrowserFiles = false;
		m_showFilter = false;

		m_pcBrowser->hide();
		m_pcFilter->hide();
		m_pcInfo2->hide();
		m_pcLastRecord->paint();
		m_pcLastPlay->paint();

		onSetFocus(MB_FOCUS_LAST_RECORD);
	} else if (gui == MB_GUI_FILTER) {
		m_showFilter = true;

		m_showMovieInfo = false;

		m_pcInfo1->hide();
		if (m_windowFocus == MB_FOCUS_BROWSER && m_settings.browserAdditional)
		{
			hideMovieCover();
			m_pcInfo2->clear();
		}
		m_pcFilter->paint();

		onSetFocus(MB_FOCUS_FILTER);
	}
	if (m_showMovieInfo) {
		m_pcInfo1->paint();
		if (m_windowFocus == MB_FOCUS_BROWSER && m_settings.browserAdditional)
			m_pcInfo2->paint();
		refreshMovieInfo();
	}
}

void CMovieBrowser::onSetGUIWindowNext(void)
{
	if (m_settings.gui == MB_GUI_MOVIE_INFO)
		onSetGUIWindow(MB_GUI_LAST_PLAY);
	else if (m_settings.gui == MB_GUI_LAST_PLAY)
		onSetGUIWindow(MB_GUI_LAST_RECORD);
	else
		onSetGUIWindow(MB_GUI_MOVIE_INFO);
}

void CMovieBrowser::onSetGUIWindowPrev(void)
{
	if (m_settings.gui == MB_GUI_MOVIE_INFO)
		onSetGUIWindow(MB_GUI_LAST_RECORD);
	else if (m_settings.gui == MB_GUI_LAST_RECORD)
		onSetGUIWindow(MB_GUI_LAST_PLAY);
	else
		onSetGUIWindow(MB_GUI_MOVIE_INFO);
}

void CMovieBrowser::onSetFocus(MB_FOCUS new_focus)
{
	TRACE("[mb]->onSetFocus: focus %d -> %d \n", m_windowFocus, new_focus);
	clearSelection();

	m_windowFocus = new_focus;
	m_pcBrowser->showSelection(false);
	m_pcLastRecord->showSelection(false);
	m_pcLastPlay->showSelection(false);
	m_pcFilter->showSelection(false);

	if (m_windowFocus == MB_FOCUS_BROWSER)
		m_pcBrowser->showSelection(true);
	else if (m_windowFocus == MB_FOCUS_LAST_PLAY)
		m_pcLastPlay->showSelection(true);
	else if (m_windowFocus == MB_FOCUS_LAST_RECORD)
		m_pcLastRecord->showSelection(true);
	else if (m_windowFocus == MB_FOCUS_FILTER)
		m_pcFilter->showSelection(true);

	updateMovieSelection();
	if (m_windowFocus != MB_FOCUS_FILTER)
		updateInfoSelection();
	refreshFoot();
}

void CMovieBrowser::onSetFocusNext(void)
{
	TRACE("[mb]->onSetFocusNext: gui %d\n", m_settings.gui);

	if (m_settings.gui == MB_GUI_FILTER)
	{
		if (m_windowFocus == MB_FOCUS_BROWSER)
			onSetFocus(MB_FOCUS_FILTER);
		else
			onSetFocus(MB_FOCUS_BROWSER);
	}
	else if (m_settings.gui == MB_GUI_MOVIE_INFO)
	{
		if (m_windowFocus == MB_FOCUS_BROWSER)
			if (m_settings.browserAdditional)
				onSetFocus(MB_FOCUS_MOVIE_INFO2);
			else
				onSetFocus(MB_FOCUS_MOVIE_INFO1);
		else
			onSetFocus(MB_FOCUS_BROWSER);
	}
	else if (m_settings.gui == MB_GUI_LAST_PLAY)
	{
		if (m_windowFocus == MB_FOCUS_MOVIE_INFO1 || m_windowFocus == MB_FOCUS_MOVIE_INFO2)
			onSetFocus(MB_FOCUS_LAST_PLAY);
		else if (m_windowFocus == MB_FOCUS_LAST_PLAY)
			onSetFocus(MB_FOCUS_MOVIE_INFO1);
	}
	else if (m_settings.gui == MB_GUI_LAST_RECORD)
	{
		if (m_windowFocus == MB_FOCUS_MOVIE_INFO1 || m_windowFocus == MB_FOCUS_MOVIE_INFO2)
			onSetFocus(MB_FOCUS_LAST_RECORD);
		else if (m_windowFocus == MB_FOCUS_LAST_RECORD)
			onSetFocus(MB_FOCUS_MOVIE_INFO1);
	}
}

bool CMovieBrowser::onSortMovieInfoHandleList(std::vector<MI_MOVIE_INFO*>& handle_list, MB_INFO_ITEM sort_item, MB_DIRECTION direction)
{
	//TRACE("sort: %d\n",direction);
	if (handle_list.empty())
		return false;
	if (sortBy[sort_item] == NULL)
		return false;

	if (direction == MB_DIRECTION_AUTO)
	{
		if (sort_item == MB_INFO_QUALITY || sort_item == MB_INFO_PARENTAL_LOCKAGE ||
				sort_item == MB_INFO_PREVPLAYDATE || sort_item == MB_INFO_RECORDDATE ||
				sort_item == MB_INFO_PRODDATE || sort_item == MB_INFO_SIZE ||
				sort_item == MB_INFO_RATING)
			sortDirection = 1;
		else
			sortDirection = 0;
	}
	else if (direction == MB_DIRECTION_UP)
	{
		sortDirection = 0;
	}
	else
	{
		sortDirection = 1;
	}

	//TRACE("sort: %d\n",sortDirection);
	sort(handle_list.begin(), handle_list.end(), sortBy[sort_item]);

	return (true);
}

void CMovieBrowser::updateMovieSelection(void)
{
	//TRACE("[mb]->updateMovieSelection %d\n",m_windowFocus);
	if (m_vMovieInfo.empty()) return;
	bool new_selection = false;

	unsigned int old_movie_selection;
	if (m_windowFocus == MB_FOCUS_BROWSER)
	{
		if (m_vHandleBrowserList.empty())
		{
			// There are no elements in the Filebrowser, clear all handles
			m_currentBrowserSelection = 0;
			m_movieSelectionHandler = NULL;
			new_selection = true;
		}
		else
		{
			old_movie_selection = m_currentBrowserSelection;
			m_currentBrowserSelection = m_pcBrowser->getSelectedLine();
			//TRACE("    sel1:%d\n",m_currentBrowserSelection);
			if (m_currentBrowserSelection != old_movie_selection)
				new_selection = true;

			if (m_currentBrowserSelection < m_vHandleBrowserList.size())
				m_movieSelectionHandler = m_vHandleBrowserList[m_currentBrowserSelection];
		}
	}
	else if (m_windowFocus == MB_FOCUS_LAST_PLAY)
	{
		if (m_vHandlePlayList.empty())
		{
			// There are no elements in the Filebrowser, clear all handles
			m_currentPlaySelection = 0;
			m_movieSelectionHandler = NULL;
			new_selection = true;
		}
		else
		{
			old_movie_selection = m_currentPlaySelection;
			m_currentPlaySelection = m_pcLastPlay->getSelectedLine();
			//TRACE("    sel2:%d\n",m_currentPlaySelection);
			if (m_currentPlaySelection != old_movie_selection)
				new_selection = true;

			if (m_currentPlaySelection < m_vHandlePlayList.size())
				m_movieSelectionHandler = m_vHandlePlayList[m_currentPlaySelection];
		}
	}
	else if (m_windowFocus == MB_FOCUS_LAST_RECORD)
	{
		if (m_vHandleRecordList.empty())
		{
			// There are no elements in the Filebrowser, clear all handles
			m_currentRecordSelection = 0;
			m_movieSelectionHandler = NULL;
			new_selection = true;
		}
		else
		{
			old_movie_selection = m_currentRecordSelection;
			m_currentRecordSelection = m_pcLastRecord->getSelectedLine();
			//TRACE("    sel3:%d\n",m_currentRecordSelection);
			if (m_currentRecordSelection != old_movie_selection)
				new_selection = true;

			if (m_currentRecordSelection < m_vHandleRecordList.size())
				m_movieSelectionHandler = m_vHandleRecordList[m_currentRecordSelection];
		}
	}

	if (new_selection == true)
	{
		//TRACE("new\n");
		refreshHDDLevel();
		refreshMovieInfo();
		refreshLCD();
	}
	//TRACE("\n");
}

void CMovieBrowser::updateInfoSelection(void)
{
	int col_frame;
	int thickness = 2;
	int radius = m_pcInfo1->getBackGroundRadius();

	/*
	   Maybe we should change background of box
	   instead of changing frame color.
	*/
	if (m_windowFocus == MB_FOCUS_MOVIE_INFO1)
		col_frame = COL_MENUCONTENTSELECTED_PLUS_2;
	else
		col_frame = COL_FRAME_PLUS_0;

	// CTextBox can't paint frames, so let's do it "manually"
	framebuffer->paintBoxFrame(m_cBoxFrameInfo1.iX, m_cBoxFrameInfo1.iY, m_cBoxFrameInfo1.iWidth, m_cBoxFrameInfo1.iHeight, thickness, col_frame, radius);
}

void CMovieBrowser::updateFilterSelection(void)
{
	//TRACE("[mb]->updateFilterSelection \n");
	if (m_FilterLines.lineArray[0].v_text.empty()) return;

	bool result = true;
	int selected_line = m_pcFilter->getSelectedLine();
	if (selected_line > 0)
		selected_line--;

	if (m_settings.filter.item == MB_INFO_FILEPATH)
	{
		m_settings.filter.optionString = m_FilterLines.lineArray[0].v_text[selected_line+1];
		m_settings.filter.optionVar = selected_line;
	}
	else if (m_settings.filter.item == MB_INFO_INFO1)
	{
		m_settings.filter.optionString = m_FilterLines.lineArray[0].v_text[selected_line+1];
	}
	else if (m_settings.filter.item == MB_INFO_MAJOR_GENRE)
	{
		m_settings.filter.optionString = g_Locale->getText(GENRE_ALL[selected_line].value);
		m_settings.filter.optionVar = GENRE_ALL[selected_line].key;
	}
	else if (m_settings.filter.item == MB_INFO_SERIE)
	{
		m_settings.filter.optionString = m_FilterLines.lineArray[0].v_text[selected_line+1];
	}
	else
	{
		result = false;
	}
	if (result == true)
	{
		refreshBrowserList();
		refreshLastPlayList();
		refreshLastRecordList();
		refreshFoot();
	}
}

bool CMovieBrowser::isParentalLock(MI_MOVIE_INFO& movie_info)
{
	bool result = false;
	if (m_parentalLock == MB_PARENTAL_LOCK_ACTIVE && m_settings.parentalLockAge <= movie_info.parentalLockAge)
		result = true;

	return (result);
}

bool CMovieBrowser::isFiltered(MI_MOVIE_INFO& movie_info)
{
	bool result = true;

	switch(m_settings.filter.item)
	{
		case MB_INFO_FILEPATH:
			if (m_settings.filter.optionVar == movie_info.dirItNr)
				result = false;
			break;
		case MB_INFO_INFO1:
			if (strcmp(m_settings.filter.optionString.c_str(),movie_info.epgInfo1.c_str()) == 0)
				result = false;
			break;
		case MB_INFO_MAJOR_GENRE:
			if (m_settings.filter.optionVar == movie_info.genreMajor)
				result = false;
			break;
		case MB_INFO_SERIE:
			if (strcmp(m_settings.filter.optionString.c_str(),movie_info.serieName.c_str()) == 0)
				result = false;
			break;
			break;
		default:
				result = false;
			break;
	}
	return (result);
}

std::string CMovieBrowser::replaceInGUI(std::string text)
{
	std::string t(text);

	t = str_replace("\u000a", ", ", t);
	t = str_replace("\u000d", ", ", t);

	return t;
}

bool CMovieBrowser::getMovieInfoItem(MI_MOVIE_INFO& movie_info, MB_INFO_ITEM item, std::string* item_string)
{
	#define MAX_STR_TMP 100
	char str_tmp[MAX_STR_TMP];
	bool result = true;
	*item_string="";
	tm* tm_tmp;

	int i=0;
	int counter=0;

	std::string b;
	bool s, e, u;

	switch(item)
	{
		case MB_INFO_FILENAME: 				// = 0,
			*item_string = movie_info.file.getFileName();
			break;
		case MB_INFO_FILEPATH: 				// = 1,
			if (!m_dirNames.empty())
				*item_string = m_dirNames[movie_info.dirItNr];
			break;
		case MB_INFO_TITLE: 				// = 2,
			*item_string = movie_info.epgTitle;
			if (strcmp("not available",movie_info.epgTitle.c_str()) == 0)
				result = false;
			if (movie_info.epgTitle.empty())
				result = false;
			break;
		case MB_INFO_SERIE: 				// = 3,
			*item_string = movie_info.serieName;
			break;
		case MB_INFO_INFO1:				// = 4,
			*item_string = replaceInGUI(movie_info.epgInfo1);
			break;
		case MB_INFO_MAJOR_GENRE: 			// = 5,
			snprintf(str_tmp, sizeof(str_tmp),"%2d",movie_info.genreMajor);
			*item_string = str_tmp;
			break;
		case MB_INFO_MINOR_GENRE: 			// = 6,
			snprintf(str_tmp, sizeof(str_tmp),"%2d",movie_info.genreMinor);
			*item_string = str_tmp;
			break;
		case MB_INFO_INFO2: 				// = 7,
			*item_string = movie_info.epgInfo2;
			break;
		case MB_INFO_PARENTAL_LOCKAGE: 			// = 8,
			snprintf(str_tmp, sizeof(str_tmp),"%2d",movie_info.parentalLockAge);
			*item_string = str_tmp;
			break;
		case MB_INFO_CHANNEL: 				// = 9,
			*item_string = movie_info.channelName;
			break;
		case MB_INFO_BOOKMARK: 				// = 10,
			b = "";

			s = false;
			if (movie_info.bookmarks.start != 0)
			{
				s = true;
				b += "S";
			}

			e = false;
			if (movie_info.bookmarks.end != 0)
			{
				e = true;
				if (s)
					b += ",";
				b += "E";
			}

			// we just return the number of bookmarks
			for (i = 0; i < MI_MOVIE_BOOK_USER_MAX; i++)
			{
				if (movie_info.bookmarks.user[i].pos != 0)
					counter++;
			}
			u = (counter > 0);
			if (u)
			{
				if (s || e)
					b += ",";
				b += "U[";
				b += to_string(counter);
				b += "]";
			}

			*item_string = b;
			break;
		case MB_INFO_QUALITY: 				// = 11,
			snprintf(str_tmp, sizeof(str_tmp),"%d",movie_info.quality);
			*item_string = str_tmp;
			break;
		case MB_INFO_PREVPLAYDATE: 			// = 12,
			if (movie_info.dateOfLastPlay == 0)
			{
				*item_string = "---";
			}
			else
			{
				tm_tmp = localtime(&movie_info.dateOfLastPlay);
				snprintf(str_tmp, sizeof(str_tmp),"%02d.%02d.%02d",tm_tmp->tm_mday,(tm_tmp->tm_mon)+ 1, tm_tmp->tm_year >= 100 ? tm_tmp->tm_year-100 : tm_tmp->tm_year);
				*item_string = str_tmp;
			}
			break;
		case MB_INFO_RECORDDATE: 			// = 13,
			tm_tmp = localtime(&movie_info.file.Time);
			snprintf(str_tmp, sizeof(str_tmp),"%02d.%02d.%02d",tm_tmp->tm_mday,(tm_tmp->tm_mon) + 1,tm_tmp->tm_year >= 100 ? tm_tmp->tm_year-100 : tm_tmp->tm_year);
			*item_string = str_tmp;
			break;
		case MB_INFO_PRODDATE: 				// = 14,
			snprintf(str_tmp, sizeof(str_tmp),"%d",movie_info.productionDate);
			*item_string = str_tmp;
			break;
		case MB_INFO_COUNTRY: 				// = 15,
			*item_string = movie_info.productionCountry;
			break;
		case MB_INFO_GEOMETRIE: 			// = 16,
			result = false;
			break;
		case MB_INFO_AUDIO: 				// = 17,
			// we just return the number of audiopids
			snprintf(str_tmp, sizeof(str_tmp), "%d", (int)movie_info.audioPids.size());
			*item_string = str_tmp;
			break;
		case MB_INFO_LENGTH: 				// = 18,
			snprintf(str_tmp, sizeof(str_tmp),"%dh %02dm", movie_info.length/60, movie_info.length%60);
			*item_string = str_tmp;
			break;
		case MB_INFO_SIZE: 				// = 19,
			snprintf(str_tmp, sizeof(str_tmp),"%4" PRIu64 "",movie_info.file.Size>>20);
			*item_string = str_tmp;
			break;
		case MB_INFO_RATING: 				// = 20,
			if (movie_info.rating)
			{
				snprintf(str_tmp, sizeof(str_tmp),"%d,%d",movie_info.rating/10, movie_info.rating%10);
				*item_string = str_tmp;
			}
			break;
		case MB_INFO_SPACER: 				// = 21,
			*item_string="";
			break;
		case MB_INFO_RECORDTIME: 			// = 22,
			if (show_mode == MB_SHOW_RECORDS)
			{
				tm_tmp = localtime(&movie_info.file.Time);
				snprintf(str_tmp, sizeof(str_tmp),"%02d:%02d", tm_tmp->tm_hour, tm_tmp->tm_min);
				*item_string = str_tmp;
			}
			break;
		case MB_INFO_PERCENT_ELAPSED:			// = 23,
			*item_string = "";
			if (movie_info.bookmarks.lastPlayStop > 0 && movie_info.length > 0)
			{
				int pos = movie_info.bookmarks.lastPlayStop * 100 / (movie_info.length * 60);
				if (pos > 100)
					pos = 100;
				*item_string = to_string(pos);
			}
			break;
		case MB_INFO_MAX_NUMBER:			// = 24,
		default:
			*item_string="";
			result = false;
			break;
	}
	//TRACE("   getMovieInfoItem: %d,>%s<",item,*item_string.c_str());
	return(result);
}
