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

	Module Name: mb_exec.cpp

	Description: Action key handling for moviebrowser

	(C) 2016, 2026 Thilo Graf 'dbt'
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mb.h"

#include <global.h>

#include <driver/moviecut.h>
#include <gui/moviedb/mdb-imdb.h>
#include <gui/widget/msgbox.h>
#include <neutrino.h>
#include <system/helpers.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef ENABLE_LCD4LINUX
#include "driver/lcd4l.h"
#endif

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
