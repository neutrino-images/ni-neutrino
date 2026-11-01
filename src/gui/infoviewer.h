/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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


#ifndef __infoview__
#define __infoview__

#include <sectionsdclient/sectionsdclient.h>

#include "infoviewer_base.h"

#include <driver/neutrino_msg_t.h>
#include <system/settings.h>
#include <string>
#include <zapit/channel.h>
#include <gui/components/cc.h>
#include <gui/weather.h>
#include "widget/record_info.h"

class CFrameBuffer;
class COSDFader;
class CInfoViewerBB;
class CInfoViewer : public CInfoViewerBase
{
 private:

	CFrameBuffer *frameBuffer;
	CInfoViewerBB *infoViewerBB;
	CComponentsFrmClock *clock;
	CComponentsShapeSquare *header, *numbox, *body;
	CComponentsTextTransp *txt_curr_start, *txt_curr_event, *txt_curr_rest, *txt_next_start, *txt_next_event, *txt_next_in;
	CRecInfo *rec;
	CWeather *weather;

	bool           gotTime;
	bool           recordModeActive;

#ifndef SKIP_CA_STATUS
	bool           CA_Status;
#endif
	
	int            InfoHeightY;
	bool	       fileplay;

	int            ButtonWidth;

        // dimensions of radiotext window
        int             rt_x;
        int             rt_y;
        int             rt_h;
        int             rt_w;
	
	std::string ChannelName;

	int            ChanNameX;
	int            ChanNameY;
	int            ChanWidth;
	int            ChanHeight;
	int            numbox_offset;
	int            numbox_maxtxtwidth;
	int            analogclock_size; //NI
	int            analogclock_offset; //NI
	fb_pixel_t*    analogclock_buf; //NI

	CSectionsdClient::CurrentNextInfo oldinfo;
        t_channel_id   current_epg_id;

	COSDFader	fader;

	int time_width;
	int time_height;
	int info_time_width;
	int header_height;
	bool newfreq ;
	static const short bar_width = 72;
	static t_event_id last_curr_id, last_next_id;
	uint64_t timeoutEnd;
	void setInfobarTimeout(int timeout_ext = 0);

	CChannelEventList               evtlist;
	CChannelEventList::iterator     eli;

	int lastsnr, lastsig, lasttime;
	CProgressBar *timescale;
	CSignalBox *sigbox;

	bool casysChange;
	bool channellogoChange;

	void paintBackground(int col_Numbox);
	void paintHead();
	void paintBody();
	void show_Data( bool calledFromEvent = false );
	void display_Info(const char *current, const char *next,
			  bool starttimes = true, const int pb_pos = -1,
			  const char *runningStart = NULL, const char *runningRest = NULL,
			  const char *nextStart = NULL, const char *nextDuration = NULL,
			  bool update_current = true, bool update_next = true);
	void initClock();
	void showAnalogClock(int posx,int posy,int dia); //NI
	void showRecordIcon(const bool show);
	void showIcon_Tuner() const;

	void showFailure(t_channel_id failed_channel_id = 0);
	void showMotorMoving(int duration);
   	void showLcdPercentOver();
	int showChannelLogo(const t_channel_id logo_channel_id, const int channel_number_width);
	void enableRadiotext();

	//small infobox, shows a small textbox with a short message text,
	//text must be located in a file named /tmp/infobar.txt
	CComponentsInfoBox *infobar_txt;
	void showInfoFile();
	void killInfobarText();

	void loop(bool show_dot);
	std::string eventname;
	void show_current_next(bool new_chan, int  epgpos);
	void reset_allScala();
	void check_channellogo_ca_SettingsChange();
	void sendNoEpg(const t_channel_id channel_id);
	bool showLivestreamInfo();

	CComponentsWindowMax *ecmInfoBox; //NI

 public:
	void ecmInfoBox_hide() override;
	void ecmInfoBox_show(const char * txt, int w, int h, Font * font) override;

	CInfoViewer();
	virtual ~CInfoViewer();

	void	start() override;
	void	showEpgInfo() override;
	void	showTitle(CZapitChannel * channel, const bool calledFromNumZap = false, int epgpos = 0, bool forcePaintButtonBar = false) override;
	void	showTitle(t_channel_id channel_id, const bool calledFromNumZap = false, int epgpos = 0, bool forcePaintButtonBar = false) override;
	void	showMovieTitle(const int playState, const t_channel_id &channel_id, const std::string &title,
				const std::string &g_file_epg, const std::string &g_file_epg1,
				const int duration, const int curr_pos, const int repeat_mode, const int _zap_mode = IV_MODE_DEFAULT) override;
	void lookAheadEPG(const int ChanNum, const std::string & Channel, const t_channel_id new_channel_id = 0, const bool calledFromNumZap = false); //alpha: fix for nvod subchannel update
	void	killTitle() override;
	void	getEPG(const t_channel_id for_channel_id, CSectionsdClient::CurrentNextInfo &info) override;

	void	showSubchan() override;
	//void	Set_CA_Status(int Status);

	int     handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data) override;

	void    changePB() override;
	void 	ResetPB();
	void    showSNR() override;
	void    Init(void);
	void	setUpdateTimer(uint64_t interval) override;
	void 	ResetModules(bool kill = false) override;
	bool 	hasTimeout() override;

protected:
	//NI: fills _livestreamInfo1/_livestreamInfo2 without painting, see init_livestreamInfo()
	void initLiveStreamInfo() override;
};
#endif
