/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	rejoin/cleanup (C) 2017 TangoCash

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


#ifndef __infoview_evo__
#define __infoview_evo__

#include <sectionsdclient/sectionsdclient.h>

#include "infoviewer_base.h"

#include <driver/rcinput.h>
#include <system/settings.h>
#include <string>
#include <zapit/channel.h>
#include <gui/components/cc.h>
#include <gui/weather.h>
#include <driver/slide.h>
#include "widget/menue.h"

class CFrameBuffer;
class COSDSlide;
class CInfoViewerEvo : public CInfoViewerBase
{
public:
	enum
	{
		// The order of icons from left to right. Here nothing changes!
		BUTTON_RED	= 0,
		BUTTON_GREEN,
		BUTTON_YELLOW,
		BUTTON_BLUE,
		BUTTON_MAX
	};
private:

	CFrameBuffer *frameBuffer;
	CComponentsHeader *header;
	CComponentsShapeSquare *body, *rec, *foot, *ca_bar;
	CComponentsTextTransp *txt_cur_start, *txt_cur_event, *txt_cur_event_rest, *txt_next_start, *txt_next_event, *txt_next_in;
	CComponentsInfoBox *infobar_txt;
	CComponentsWindowMax *ecmInfoBox;
	CComponentsWindowMax *CoverBox;
	CComponentsForm *recordsbox;
	CComponentsTimer *recordsblink;

	CWeather *weather;

	bool           gotTime;
	bool           recordModeActive;

#ifndef SKIP_CA_STATUS
	bool           CA_Status;
#endif

	int            InfoHeightY;
	bool	       fileplay;

	int            ButtonWidth;
	int            ca_h;

	// dimensions of radiotext window
	int             rt_dx;
	int             rt_dy;
	int             rt_x;
	int             rt_y;
	int             rt_h;
	int             rt_w;

	std::string ChannelName;

	int            ChanNameX;
	int            ChanWidth;

	int            ana_clock_size;

	int            ChanNumWidth;
	void           PaintChanNumber();

	CSectionsdClient::CurrentNextInfo oldinfo;
	t_channel_id   current_epg_id;

	COSDSlider	slider;

	int info_time_width;
	int header_height;
	bool newfreq ;
	static const short bar_width = 72;
	static t_event_id last_curr_id, last_next_id;
	uint64_t timeoutEnd;
	void setInfobarTimeout(int timeout_ext = 0);

	CChannelEventList               evtlist;
	CChannelEventList::iterator     eli;

	CProgressBar *timescale;
	CProgressBar *hddscale;
	//NI: signal/noise ratio bars inside the channel number box, see showSNR()
	CSignalBox *sigbox;
	int numbox_offset;
	int numbox_maxtxtwidth;

	bool casysChange;

	void paintHead(t_channel_id channel_id,std::string channel_name);
	void paintBody();
	void show_Data( bool calledFromEvent = false );
	void display_Info(const char *current, const char *next,
	                  bool starttimes = true, const int pb_pos = -1,
	                  const char *runningStart = NULL, const char *runningRest = NULL,
	                  const char *nextStart = NULL, const char *nextDuration = NULL,
	                  bool update_current = true, bool update_next = true);
	void initClock();
	void showRecordIcon(const bool show);

	void showFailure();
	void showMotorMoving(int duration);
	void showLcdPercentOver();
	void enableRadiotext();

	void showInfoFile();
	void killInfobarText();

	void loop();
	std::string eventname;
	void show_current_next(bool new_chan, int  epgpos);
	void check_channellogo_ca_SettingsChange();
	void sendNoEpg(const t_channel_id channel_id);
	bool showLivestreamInfo();

	enum
	{
		// The order of icons from right to left. Here nothing changes!
		ICON_SUBT	= 0,
		ICON_RT,
		ICON_DD,
		ICON_16_9,
		ICON_RES,
		ICON_CA,
		ICON_TUNER,
		ICON_LOGO,
		ICON_MAX
	};

	typedef struct
	{
		bool paint;
		int w;
		int x;
		int cx;
		int h;
		std::string icon;
		std::string text;
		bool active;
	} bbButtonInfoStruct;

	enum CAM_DECODE_NUM {UNKNOWN, NA, FTA, LOCAL, CARD, REMOTE};
	void 		paint_cam_icons();
	void*		Thread_paint_cam_icons(void);
	unsigned short int decode;
	int		DecEndx;
	int 		parse_ecmInfo(const char * file);
	int		check_ecmInfo();
	bool	camCI;
	bool	useCI;

	typedef struct
	{
		int x;
		int h;
	} bbIconInfoStruct;

	bbIconInfoStruct bbIconInfo[ICON_MAX];
	bbButtonInfoStruct bbButtonInfo[BUTTON_MAX];
	std::string tmp_bbButtonInfoText[BUTTON_MAX];
	int bbIconMinX, bbButtonMaxX, bbIconMaxH, bbButtonMaxH;

	int BBarY, BBarFontY;
	int hddwidth;
	//int lasthdd, lastsys;
	bool fta;
	int minX;

	bool scrambledErr, scrambledErrSave;
	bool scrambledNoSig, scrambledNoSigSave;
	pthread_t scrambledT;

	void paintFoot(int w = 0);
	void showIcons(const int modus, const std::string & icon);
	bool checkIcon(const char * const icon, int *w, int *h);
	void getIconInfo(void);

	void paint_ca_icons(int, const char*, int&);
	void paint_ca_bar();
	void showOne_CAIcon();

	static void* scrambledThread(void *arg);
	void scrambledCheck(bool force=false);

public:
	int      ChanHeight;
	int      ChanNameY;
	int      time_width;

	bool ecminfo_toggle;
	void ecmInfoBox_hide() override;
	void ecmInfoBox_show(const char * txt, int w, int h, Font * font) override;

	CInfoViewerEvo();
	virtual ~CInfoViewerEvo();

	void	showMovieTitle(const int playState, const t_channel_id &channel_id, const std::string &title,
	                       const std::string &g_file_epg, const std::string &g_file_epg1,
	                       const int duration, const int curr_pos, const int repeat_mode, const int _zap_mode = IV_MODE_DEFAULT) override;

	void	start() override;
	void	showEpgInfo() override;
	void	showTitle(CZapitChannel * channel, const bool calledFromNumZap = false, int epgpos = 0, bool forcePaintButtonBar = false) override;
	void	showTitle(t_channel_id channel_id, const bool calledFromNumZap = false, int epgpos = 0, bool forcePaintButtonBar = false) override;
	void lookAheadEPG(const int ChanNum, const std::string & Channel, const t_channel_id new_channel_id = 0, const bool calledFromNumZap = false); //alpha: fix for nvod subchannel update
	void	killTitle() override;
	void	getEPG(const t_channel_id for_channel_id, CSectionsdClient::CurrentNextInfo &info) override;

	void	showSubchan() override;
	//void	Set_CA_Status(int Status);

	int     handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data) override;

	void    ResetPBars();
	void    Init(void);
	void	setUpdateTimer(uint64_t interval) override;
	void 	ResetModules(bool kill = false) override;
	void	KillModules()
	{
		ResetModules(true);
	};
	bool 	hasTimeout() override;

	int bottom_bar_offset, InfoHeightY_Info, showBBIcons_width;

	void showIcon_CA_Status(int);
	void showIcon_16_9();
	void showIcon_RadioText(bool rt_available);
	void showIcon_SubT();
	void showIcon_Resolution();
	void showIcon_Tuner(void);
	void showIcon_Logo();
	void showIcon_DD(void);
	void showIcon_Tuner() const;

	void showClock_analog(int posx,int posy,int dia);
	void showScale_RecordingDir();
	void showButtons(bool paintFooter = false);
	void showRecords();

	void paintshowButtonBar();
	void getButtonInfo(void);
	void initBBOffset(void);
	// modules
	CComponentsShapeSquare* getFooter(void)
	{
		return foot;
	}
	CComponentsShapeSquare* getCABar(void)
	{
		return ca_bar;
	}

	//NI: reset the progress bar items and the signal box
	void ResetPB();
	//NI: paints frequency and signal/noise ratio bars in the channel number box
	void showSNR() override;

	// This functions used outside.
	void changePB() override
	{
		ResetPBars();
	};

protected:
	//NI: fills _livestreamInfo1/_livestreamInfo2 without painting, see init_livestreamInfo()
	void initLiveStreamInfo() override;
};
#endif
