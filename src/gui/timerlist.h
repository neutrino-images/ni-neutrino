/*
	Neutrino-GUI  -   DBoxII-Project

	Timerliste by Zwen

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

#ifndef __timerlist__
#define __timerlist__

#include <timerdclient/timerdtypes.h>

#include <driver/display.h>
#include <gui/widget/menue.h>
#include <gui/widget/listhelpers.h>

#include <system/remotetimer.h>

#include <map>
#include <string>
#include <vector>

class CFrameBuffer;
class CTimerdClient;
class CTimerList : public CMenuTarget, public CListHelpers
{
	private:
		CFrameBuffer *frameBuffer;
		int x;
		int y;
		int width;
		int height;
		int font_height;
		int item_height;
		int header_height;
		int footer_height;
		int selected;
		int liststart;
		unsigned int listmaxshow;
		bool visible;
		CComponentsHeader *header;
		int httpConnectTimeout;

		CTimerdClient *Timer;
		CTimerd::TimerList timerlist; // List of timers
		CTimerd::responseGetTimer timerNew;
		int timerNew_standby_on;
		std::string timerNew_channel_name;
		std::string m_weekdaysStr;
		std::string timerNew_recordingDir;
		std::string timerNew_message;
		std::string timerNew_pluginName;

		int timer_apids_dflt;
		int timer_apids_std;
		int timer_apids_ac3;
		int timer_apids_alt;
		std::string timer_recordingDir;

		void paintItem(int pos);
		void paint();
		void paintHead();
		void paintFoot();
		void hide();
		int modifyTimer();
		int newTimer();
		// TODO: properly import the enum CVFD::MODES
		CVFD::MODES saved_displaymode;
		bool RemoteBoxSetup();
		void RemoteBoxSelect();
		bool RemoteBoxAddress(const std::string &rbname, remotebox_address_t &out);
		bool RemoteBoxChanExists(t_channel_id channel_id);
		bool LocalBoxChanExists(t_channel_id channel_id);
		/* What the other box refused with, said to the person in front of this
		   one. A call that came to nothing used to look exactly like one that
		   worked. */
		void RemoteBoxProblem(const std::string &rbname, CRemoteTimerClient &client,
			CRemoteTimerClient::result_t res);
		/* What each remote box calls the channel of each of its timers, read
		   once with the list rather than once per drawn row. */
		std::map<std::string, std::string> rb_channel_names;
		std::string RemoteBoxChannelName(const CTimerd::responseGetTimer &timer) const;
		int item_offset;
		bool changed;
		int bselected;
		CMenuWidget *remboxmenu;
		bool askUserOnRemoteTimerConflict(time_t announceTime, time_t stopTime, char *remotebox_ip);

	public:
		CTimerList();
		~CTimerList();
		void updateEvents(void);
		int show();
		int exec(CMenuTarget *parent, const std::string &actionKey);
		static const char *convertTimerType2String(const CTimerd::CTimerEventTypes type);
		static std::string convertTimerRepeat2String(const CTimerd::CTimerEventRepeat rep);
		static std::string convertChannelId2String(const t_channel_id id);
		/* with_channel_names costs one call per channel the other box has a
		   timer on, so only the screen that draws those names asks for them. */
		void RemoteBoxTimerList(CTimerd::TimerList &timerlist, bool with_channel_names = false);
};

bool askUserOnTimerConflict(time_t announceTime, time_t stopTime, t_channel_id channel_id = 0);

#endif
