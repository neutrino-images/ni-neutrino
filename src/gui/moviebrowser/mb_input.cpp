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

	Module Name: mb_input.cpp

	Description: Input and interaction handlers for moviebrowser

	(C) 2016, Thilo Graf 'dbt'
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mb.h"
#include "mb_constants.h"
#include "mb_functions.h"

#include <global.h>

#include <driver/fontrenderer.h>
#include <driver/record.h>
#include <gui/epgview.h>
#include <gui/filebrowser.h>
#include <gui/moviedb/mdb-tmdb.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>
#include <gui/widget/menue.h>
#include <gui/widget/msgbox.h>
#include <neutrino.h>
#include <system/helpers.h>
#include <timerdclient/timerdclient.h>

#include <cctype>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>


bool CMovieBrowser::onButtonPress(neutrino_msg_t msg)
{
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
					dprintf(DEBUG_DEBUG, "[mb] TMDB: %s : %s\n",m_movieSelectionHandler->file.Name.c_str(),cover_file.c_str());
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

				dprintf(DEBUG_DEBUG, "[mb]->new sorting %d, %s\n", m_settings.sorting.item, g_Locale->getText(m_localizedItemName[m_settings.sorting.item]));

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
			dprintf(DEBUG_DEBUG, "[mb] SMS new key: %c\n", smsKey);
			g_RCInput->getMsg_ms(&msg, &data, MOVIE_SMSKEY_TIMEOUT-100);
		} while ((msg >= CRCInput::RC_1) && (msg <= CRCInput::RC_9));

		int selected = current_frame->getSelectedLine();
		if (msg == CRCInput::RC_timeout || msg == CRCInput::RC_nokey) {
			uint32_t i;
			for (i = selected+1; i < (*current_list).size(); i++) {

				char firstCharOfTitle = (*current_list)[i]->epgTitle.c_str()[0];
				if (tolower(firstCharOfTitle) == smsKey) {
					dprintf(DEBUG_DEBUG, "[mb] SMS found selected=%d i=%d \"%s\"\n", selected, i, (*current_list)[i]->epgTitle.c_str());
					break;
				}
			}
			if (i >= (*current_list).size()) {
				for (i = 0; i < (*current_list).size(); i++) {
					char firstCharOfTitle = (*current_list)[i]->epgTitle.c_str()[0];
					if (tolower(firstCharOfTitle) == smsKey) {
						dprintf(DEBUG_DEBUG, "[mb] SMS found selected=%d i=%d \"%s\"\n", selected, i, (*current_list)[i]->epgTitle.c_str());
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

void CMovieBrowser::onSetGUIWindow(MB_GUI gui)
{
	dprintf(DEBUG_DEBUG, "[mb]->onSetGUIWindow: gui %d -> %d\n", m_settings.gui, gui);
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
	dprintf(DEBUG_DEBUG, "[mb]->onSetFocus: focus %d -> %d \n", m_windowFocus, new_focus);
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
	dprintf(DEBUG_DEBUG, "[mb]->onSetFocusNext: gui %d\n", m_settings.gui);

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
