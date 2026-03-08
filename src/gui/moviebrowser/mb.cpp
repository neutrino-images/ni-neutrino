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
