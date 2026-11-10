/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Dies ist der gemeinsame Nenner der Infobar. Sie liegt in zwei
	Ausfuehrungen vor: der klassischen NI-Fassung (CInfoViewer) und der
	evo-Fassung (CInfoViewerEvo). Welche gebaut wird, sagt
	g_settings.infobar_evo_infoviewer, siehe create(). Alles ausserhalb
	spricht nur mit CInfoViewerBase.

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


#ifndef __infoview_base__
#define __infoview_base__

#include <sectionsdclient/sectionsdclient.h>

#include <driver/neutrino_msg_t.h>
#include <system/settings.h>
#include <string>
#include <zapit/channel.h>
#include <sigc++/signal.h>

class CFrameBuffer;
class Font;

/* What the two infobar implementations have in common: the state the rest of
   the program reads and writes, and the calls it makes. Everything outside
   holds a CInfoViewerBase and never asks which one it got. */
class CInfoViewerBase : public sigc::trackable
{
public:
	enum
	{
		IV_MODE_DEFAULT		= 0,
		IV_MODE_VIRTUAL_ZAP 	= 1,
		IV_MODE_NUMBER_ZAP	= 2
	};/*iv_switch_mode_t*/

	/* the state the outside reads and writes */
	bool     chanready;
	bool     is_visible;

	char     aspectRatio;
	uint32_t sec_timer_id;
	bool     SDT_freq_update;

	int      BoxStartX;
	int      BoxStartY;
	int      BoxEndX;
	int      BoxEndY;
	int      ChanInfoX;
	bool     showButtonBar;
	bool     isVolscale;

	std::string	md5_ecmInfo;

	CInfoViewerBase();
	virtual ~CInfoViewerBase();

	/* Builds the infobar the settings ask for; the caller owns it. */
	static CInfoViewerBase *create();

	/* the calls the outside makes */
	virtual void	start() = 0;
	virtual void	showEpgInfo() = 0;
	virtual void	showTitle(CZapitChannel * channel, const bool calledFromNumZap = false, int epgpos = 0, bool forcePaintButtonBar = false) = 0;
	virtual void	showTitle(t_channel_id channel_id, const bool calledFromNumZap = false, int epgpos = 0, bool forcePaintButtonBar = false) = 0;
	virtual void	showMovieTitle(const int playState, const t_channel_id &channel_id, const std::string &title,
	                               const std::string &g_file_epg, const std::string &g_file_epg1,
	                               const int duration, const int curr_pos, const int repeat_mode, const int _zap_mode = IV_MODE_DEFAULT) = 0;
	virtual void	killTitle() = 0;
	virtual void	showSubchan() = 0;
	virtual void	getEPG(const t_channel_id for_channel_id, CSectionsdClient::CurrentNextInfo &info) = 0;
	virtual int	handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data) = 0;
	virtual bool	hasTimeout() = 0;
	virtual void	changePB() = 0;
	virtual void	ResetModules(bool kill = false) = 0;
	virtual void	setUpdateTimer(uint64_t interval) = 0;

	/* Told by the caller that owns the screen for a while - the channel list.
	   The bar is behind it then, and the parts of it that hold screen area for
	   as long as they are up (the box it keeps for running recordings) are
	   taken down for that time; they come back with the bar's next update.
	   Nothing to do for the classic bar, which has no such part. */
	virtual void	setScreenCovered(bool /*covered*/) {}

	virtual void	ecmInfoBox_hide() = 0;
	virtual void	ecmInfoBox_show(const char * txt, int w, int h, Font * font) = 0;

	virtual void	showSNR() = 0;

	/* the small answers both give the same way */
	void	setSwitchMode(const int& mode)
	{
		zap_mode = mode;
	}
	int	getSwitchMode()
	{
		return zap_mode;
	}
	void    resetSwitchMode()
	{
		setSwitchMode(IV_MODE_DEFAULT);
	}

	CSectionsdClient::CurrentNextInfo getCurrentNextInfo() const { return info_CurrentNext; }

	void init_livestreamInfo() { initLiveStreamInfo(); }
	std::string get_livestreamInfo1() { return _livestreamInfo1; }
	std::string get_livestreamInfo2() { return _livestreamInfo2; }

	uint32_t getUpdateTimer(void)
	{
		return lcdUpdateTimer;
	}
	inline t_channel_id get_current_channel_id(void)
	{
		return current_channel_id;
	}

	sigc::signal<void> OnAfterKillTitle;
	sigc::signal<void> OnEnableRadiotext;

protected:
	virtual void	initLiveStreamInfo() = 0;

	t_channel_id	current_channel_id;
	CSectionsdClient::CurrentNextInfo info_CurrentNext;
	std::string	_livestreamInfo1;
	std::string	_livestreamInfo2;
	uint32_t	lcdUpdateTimer;
	int		zap_mode;
};
#endif
