/*
	imdb

	(C) 2009-2016 NG-Team
	(C) 2016 NI-Team

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

#ifndef __imdb__
#define __imdb__

#include <gui/components/cc.h>

class CIMDB
{
	public:
		CIMDB();
		~CIMDB();
		static CIMDB* getInstance();

		CFrameBuffer *frameBuffer;
		std::string surl;
		std::string soutfile;
		std::string IMDbAPI;
		std::string posterfile;
		std::string stars_bg;
		std::string stars;

		int getIMDb(const std::string& epgTitle);
		std::string getFilename(CZapitChannel * channel, uint64_t id);
		void StringReplace(std::string &str, const std::string search, const std::string rstr);
		void cleanup();

		void getIMDbData(std::string& txt);

		void 	hideWindow(bool keep_active = false);
		void	showIMDbWindow(const std::string title);
		void	showTextWindow(const std::string title, const std::string txt);
		void	scroll(bool scrollDown);
		bool	isActive() { return imdb_activ; };

		virtual void getIMDbElement(std::string& element)  { element = m[element]; };

	private:
		int acc;
		std::string IMDburl;
		std::string googleIMDb(std::string searchStr);
		std::string utf82url(std::string s);
		std::string parseString(std::string search1, std::string search2, std::string str);
		std::string parseFile(std::string search1, std::string search2, const char* file, std::string firstline="", int line_offset=0);
		std::map<std::string, std::string> m;

		CComponentsWindow  	*cc_win;
		CComponentsText		*cc_txt;

		int	item_top, fontheight;
		int	h_body, w_body, h_offset, v_offset;
		int	h_footer;
		int	btn_start, btn_width, btn_height;
		bool 	imdb_activ;
		void 	initFrame();
		void	initMap(std::map<std::string, std::string>& my);
		void	bigFonts(bool on);
};

#endif
