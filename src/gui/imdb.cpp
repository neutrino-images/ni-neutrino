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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fstream>
#include <iostream>

#include <global.h>
#include <driver/screen_max.h>
#include <gui/components/cc.h>
#include <gui/epgview.h> //bigFonts
#include <system/httptool.h>
#include <system/helpers.h>
#include <eitd/sectionsd.h>

#include <json/json.h>

#include "imdb.h"


CIMDB::CIMDB()
{
	frameBuffer = CFrameBuffer::getInstance();

	surl		= "http://www.google.de/search?q=";
	soutfile	= "/tmp/google.out";
	IMDburl		= "http://www.omdbapi.com/?plot=full&r=json&i=";
	IMDbAPI		= "/tmp/IMDb.json";
	posterfile	= "/tmp/poster.jpg";
	stars_bg	= ICONSDIR "/stars_bg.png";
	stars		= ICONSDIR "/stars.png";

	cc_win		= NULL;
	cc_txt		= NULL;

	imdb_activ	= false;

	//initFrame(); /*not use for epginfo
}

CIMDB::~CIMDB()
{
	cleanup();

	imdb_activ = false;
	if(cc_win)
		delete cc_win;
}

CIMDB* CIMDB::getInstance()
{
	static CIMDB* imdb = NULL;
	if(!imdb)
		imdb = new CIMDB();
	return imdb;
}

std::string CIMDB::utf82url(std::string s)
{
	std::stringstream ss;
	for (size_t i = 0; i < s.length(); ++i)
	{
		if (unsigned(s[i]) <= ' ') {
			ss << '+';
		}
		else if (unsigned(s[i]) <= '\x27') {
			ss << "%" << std::hex << unsigned(s[i]);
		}
		else {
			ss << s[i];
		}
	}
	return ss.str();
}

std::string CIMDB::parseString(std::string search1, std::string search2, std::string str)
{
	std::string ret, search;
	size_t pos_wildcard, pos_firstline, pos_search1, pos_search2;
	pos_wildcard = pos_firstline = pos_search1 = pos_search2 = std::string::npos;

	if((pos_wildcard = search1.find('*')) != std::string::npos)
	{
		search = search1.substr(0, pos_wildcard);
		//std::cout << "wildcard detected" << '\t' << "= " << search << "[*]" << search1.substr(pos_wildcard+1) << std::endl;
	}
	else
		search = search1;

	//std::cout << "search1" << "\t\t\t" << "= " << '"' << search << '"' << std::endl;
	if((pos_search1 = str.find(search)) != std::string::npos)
	{
		//std::cout << "search1 found" << "\t\t" << "= " << '"' << search << '"' << " at pos "<< (int)(pos_search1) << " => " << str << std::endl;

		pos_search1 += search.length();

		if(pos_wildcard != std::string::npos)
		{
			size_t pos_wildcard_ext;
			std::string wildcard_ext = search1.substr(pos_wildcard+1);

			//std::cout << "wildcard_ext" << "\t\t" << "= " << '"' << wildcard_ext << '"' << std::endl;
			if((pos_wildcard_ext = str.find(wildcard_ext,pos_wildcard+1)) != std::string::npos)
			{
				//std::cout << "wildcard_ext found" << "\t" << "= " << '"' << wildcard_ext << '"' << " at pos "<< (int)(pos_wildcard_ext) << " => " << str << std::endl;
				pos_search1 = pos_wildcard_ext + wildcard_ext.length();
			}
			else
			{
				//std::cout << "wildcard_ext not found in line " << acc << " - exit" << std::endl;
				return("");
			}
		}
	}
	else
	{
		//std::cout << "search1 not found in line " << acc << " - exit" << std::endl;
		return("");
	}

	if(pos_search1 != std::string::npos)
	{
		//std::cout << "search2 " << "\t\t" << "= " << '"' << search2 << '"' << std::endl;

		if(search2 == "\n")
		{
			ret = str.substr(pos_search1, str.length() - pos_search1);
			return(ret);
		}

		if((pos_search2 = str.find(search2, pos_search1)) != std::string::npos)
		{
			if(search2.empty())
				pos_search2 = str.length();

			//std::cout << "search2" << "\t\t\t" << "= " << '"' << search2 << '"' << " found at "<< (int)(pos_search2) << " => " << str << std::endl;
			ret = str.substr(pos_search1, pos_search2 - pos_search1);
		}
		//else
			//std::cout << "search2 not found in line " << acc << " - exit" << std::endl;

	}

	return(ret);
}

std::string CIMDB::parseFile(std::string search1, std::string search2, const char* file, std::string firstline, int line_offset)
{
	int line = 0;
	acc = 0;
	std::ifstream fh;
	std::string str, ret, search;
	size_t pos_firstline, pos_search1, pos_search2;
	pos_firstline = pos_search1 = pos_search2 = std::string::npos;

	if(firstline.empty())
		pos_firstline = 0;

	fh.open(file, std::ios::in);
	if(fh.is_open())
	{
		while (!fh.eof())
		{
			getline(fh, str);
			acc++;

			if(pos_firstline == std::string::npos)
			{
				if((pos_firstline = str.find(firstline)) != std::string::npos)
				{
					//std::cout << "firstline found " << str << std::endl;
				}
				continue;
			}

			if(line_offset /*&& pos_firstline != std::string::npos*/)
			{
				if(line+1 != line_offset)
				{
					line++;
					continue;
				}
			}

			ret = parseString(search1,search2,str);

			if(!ret.empty())
				break;
		}
		fh.close();
	}

	return(ret);
}
std::string CIMDB::googleIMDb(std::string searchStr)
{
	CHTTPTool httpTool;
	std::string ret		= "IMDb: google download fehlgeschlagen";
	std::string httpString	= "imdb+";
	char* searchStr_	= (char*) searchStr.c_str();

	while (*searchStr_!=0)
	{
		if ( (*searchStr_==' ') )
		{
			httpString += '+';
		} else
		{
			httpString += *searchStr_;
		}
		searchStr_++;
	}

	std::string url = surl + "IMDb+" + utf82url(httpString);

	if(httpTool.downloadFile(url, soutfile.c_str()))
	{
		ret = parseFile("http://www.imdb.com/title/", ">", soutfile.c_str());

		if(ret.empty())
			ret = parseFile("http://www.imdb.de/title/", ">", soutfile.c_str());

		std::string delimiters = "/&;";
		size_t next = ret.find_first_of(delimiters, 0);
		ret = ret.substr(0, next);
	}

	return ret;
}

void CIMDB::initMap( std::map<std::string, std::string>& my )
{
	Json::Value root;
	Json::Reader reader;

	std::ostringstream ss;
	std::ifstream fh(IMDbAPI.c_str(),std::ifstream::in);
	ss << fh.rdbuf();
	std::string filedata = ss.str();

	bool parsedSuccess = reader.parse(filedata,root,false);

	if(!parsedSuccess)
	{
		std::cout << "Failed to parse JSON\n";
		std::cout << reader.getFormattedErrorMessages() << std::endl;
	}
 
	for(Json::Value::iterator it = root.begin(); it !=root.end(); ++it)
	{
		Json::Value key = it.key();
		Json::Value value = (*it);

		std::string skey = key.asString();
		std::string svalue = value.asString();
		my[skey] = svalue;
	}
}

int CIMDB::getIMDb(const std::string& epgTitle)
{
	CHTTPTool httpTool;
	int ret = 0;

	std::string imdb_ID = googleIMDb(epgTitle);

	if(((imdb_ID.find("IMDb: ")) != std::string::npos))
		return ret;

	std::string url = IMDburl + imdb_ID;

	if(httpTool.downloadFile(url, IMDbAPI.c_str()))
	{
		initMap(m);

		//std::cout << "m now contains " << m.size() << " elements.\n";

		if(m.empty() || m["Response"]!="True")
			return 0;

		//for (std::map<std::string,std::string>::iterator it=m.begin(); it!=m.end(); ++it)
		//	std::cout << it->first << " => " << it->second << '\n';

		//download Poster
		if(httpTool.downloadFile(m["Poster"], posterfile.c_str()))
			ret = 2;
		else {
			ret = 1;
			if (access(posterfile.c_str(), F_OK) == 0)
				unlink(posterfile.c_str());
		}
	}

	return ret;
}

void CIMDB::getIMDbData(std::string& txt)
{
	txt += "Metascore: "+m["Metascore"]+"/100\n";
	txt += "Stimmen: "+m["imdbVotes"]+"\n";
	txt += "Datum: "+m["Released"]+" | "+m["Country"]+" | "+m["Runtime"]+"\n";
	txt += "Genre: "+m["Genre"]+"\n";
	txt += "Awards: "+m["Awards"]+"\n";
	txt += "Regisseur: "+m["Director"]+"\n";
	txt += "Drehbuch: "+m["Writer"]+"\n\n";
	txt += "Darsteller: "+m["Actors"]+"\n";
}

std::string CIMDB::getFilename(CZapitChannel * channel, uint64_t id)
{
	char		fname[512]; // UTF-8
	char		buf[256];
	unsigned int	pos = 0;

	if(check_dir(g_settings.network_nfs_recordingdir.c_str()))
		return ("");

	snprintf(fname, sizeof(fname), "%s/", g_settings.network_nfs_recordingdir.c_str());
	pos = strlen(fname);

	// %C == channel, %T == title, %I == info1, %d == date, %t == time_t
	std::string FilenameTemplate = g_settings.recording_filename_template;
	if (FilenameTemplate.empty())
		FilenameTemplate = "%C_%T_%d_%t";

	StringReplace(FilenameTemplate,"%d","");
	StringReplace(FilenameTemplate,"%t","");
	StringReplace(FilenameTemplate,"__","_");

	std::string channel_name = channel->getName();
	if (!(channel_name.empty())) {
		strcpy(buf, UTF8_TO_FILESYSTEM_ENCODING(channel_name.c_str()));
		ZapitTools::replace_char(buf);
		StringReplace(FilenameTemplate,"%C",buf);
	}
	else
		StringReplace(FilenameTemplate,"%C","no_channel");

	CShortEPGData epgdata;
	if(CEitManager::getInstance()->getEPGidShort(id, &epgdata)) {
		if (!(epgdata.title.empty())) {
			strcpy(buf, epgdata.title.c_str());
			ZapitTools::replace_char(buf);
			StringReplace(FilenameTemplate,"%T",buf);
		}
		else
			StringReplace(FilenameTemplate,"%T","no_title");

		if (!(epgdata.info1.empty())) {
			strcpy(buf, epgdata.info1.c_str());
			ZapitTools::replace_char(buf);
			StringReplace(FilenameTemplate,"%I",buf);
		}
		else
			StringReplace(FilenameTemplate,"%I","no_info");
	}

	strcpy(&(fname[pos]), UTF8_TO_FILESYSTEM_ENCODING(FilenameTemplate.c_str()));

	pos = strlen(fname);

	strcpy(&(fname[pos]), ".jpg");

	return (fname);
}

void CIMDB::StringReplace(std::string &str, const std::string search, const std::string rstr)
{
	std::string::size_type ptr = 0;
	std::string::size_type pos = 0;
	while((ptr = str.find(search,pos)) != std::string::npos){
		str.replace(ptr,search.length(),rstr);
		pos = ptr + rstr.length();
	}
}

void CIMDB::cleanup()
{
	if (access(soutfile.c_str(), F_OK) == 0)
		unlink(soutfile.c_str());
	if (access(posterfile.c_str(), F_OK) == 0)
		unlink(posterfile.c_str());
	imdb_activ = false;
}

void CIMDB::initFrame()
{
	printf("[CIMDB::%s] \r\n", __FUNCTION__);

	fontheight	= g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE]->getHeight();
	h_offset	= 10;
	v_offset	= 10;

	//create window object to get window positions
	if (cc_win == NULL){
		cc_win = new CComponentsWindow();

		//recalc window
		int w = frameBuffer->getScreenWidthRel();
		int h = frameBuffer->getScreenHeightRel();
		int x = getScreenStartX(w);
		int y = getScreenStartY(h);

		//set new dimensions;
		cc_win->setDimensionsAll(x, y, w, h);
		cc_win->Refresh();

		//get window footer object
		CComponentsFooter *winfooter = cc_win->getFooterObject();
		winfooter = cc_win->getFooterObject();
		h_footer = winfooter->getHeight();

		//get window body object
		CComponentsForm *winbody = cc_win->getBodyObject();
		h_body = winbody->getHeight();
		w_body = winbody->getWidth();
	}

	//calc button position
	btn_start = 10;
	btn_width = (cc_win->getWidth()-2*btn_start)/4;
	btn_height = h_footer-(h_footer/4);
}

void CIMDB::showTextWindow(const std::string title, const std::string txt)
{
	if(cc_win == NULL){
		initFrame();
	}

	printf("[CIMDB::%s] \r\n", __FUNCTION__);

	bigFonts(true);

	//set window header
	cc_win->setWindowCaption(title);
	cc_win->setWindowIcon(NEUTRINO_ICON_INFO);
	cc_win->setWindowHeaderButtons(CComponentsHeader::CC_BTN_EXIT);

	//create text item
	if(cc_txt == NULL){
		cc_txt = new CComponentsText();
	}

	cc_txt->setDimensionsAll(0, 0, w_body, h_body);
	cc_txt->setTextBorderWidth(h_offset, 0);
	cc_txt->setText(txt, CTextBox::TOP | CTextBox::SCROLL, g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]);

	/*add text item to window body object ...
	* CComponentsForm *winbody = cc_win->getBodyObject();
	* winbody->addCCItem(cc_txt);
	*
	*...or directly with addWindowItem(cc_txt);
	*/
	cc_win->addWindowItem(cc_txt);

	//get window footer object
	CComponentsForm *winfooter = cc_win->getFooterObject();
	winfooter->setColorBody(COL_INFOBAR_SHADOW_PLUS_1);

	//create button items
	CComponentsButtonGreen	*cc_button_gn = new CComponentsButtonGreen(btn_start, CC_CENTERED, btn_width, btn_height, LOCALE_IMDB_INFO,
		/*parent*/		NULL,
		/*selected*/		false,
		/*enabled*/		true,
		/*has_shadow*/		false,
		/*color_frame*/		winfooter->getColorBody(),
		/*color_body*/		winfooter->getColorBody(),
		/*color_shadow*/	COL_MENUCONTENTDARK_PLUS_0);
	cc_button_gn->doPaintBg(false);
	cc_button_gn->setButtonTextColor(COL_INFOBAR_SHADOW_TEXT);
	cc_button_gn->setColBodyGradient(CC_COLGRAD_OFF);
	cc_button_gn->setFrameThickness(0);

	//add button object to window footer
	winfooter->addCCItem(cc_button_gn);

	//show window
	cc_win->paint();

	bigFonts(false);
}

void CIMDB::hideWindow(bool keep_active)
{
	if (!keep_active)
		imdb_activ = false;

	cc_win->kill();

	//deallocate window object, dletes added cc_items also
	if(cc_win)
		delete cc_win;

	cc_win		= NULL;
	cc_txt		= NULL;
}

//scroll text
void CIMDB::scroll(bool scrollDown)
{
	bigFonts(true);

	//get the textbox instance and use CTexBbox scroll methods
	CTextBox* ctb = NULL;
	if (cc_txt)
		ctb = cc_txt->getCTextBoxObject();
	if (ctb)
	{
		ctb->enableBackgroundPaint(true);
		if (scrollDown)
			ctb->scrollPageDown(1);
		else
			ctb->scrollPageUp(1);
		ctb->enableBackgroundPaint(false);
	}

	bigFonts(false);
}

void CIMDB::showIMDbWindow(const std::string title)
{
	if(cc_win == NULL){
		initFrame();
	}
	printf("[CIMDB::%s] \r\n", __FUNCTION__);

	bigFonts(true);

	std::string print_buffer;
	item_top		= v_offset;
	int h_imdbtitle		= fontheight;
	int left_offset		= h_offset+ 20;
	int w_poster_max	= w_body/4; // max 25%
	int h_poster_max	= h_body- item_top- ((h_imdbtitle+ fontheight)*2 /*space from top to starbar*/);
	int w_starbar		= 160; //starbar picture width
	int h_starbar		= fontheight;
	std::string pg_value	= "0";
	imdb_activ		= true;

	//show splash
	cc_win->setWindowCaption("IMDb: Daten werden geladen ...");
	cc_win->setWindowIcon(NEUTRINO_ICON_INFO);
	cc_win->setWindowHeaderButtons(CComponentsHeader::CC_BTN_EXIT);
	//get window footer object
	CComponentsForm *winfooter = cc_win->getFooterObject();
	winfooter->setColorBody(COL_INFOBAR_SHADOW_PLUS_1);
	cc_win->paint();

	//google, get IMDb-html and poster
	getIMDb(title);

	//get title from IMDb
	print_buffer = "Title";
	getIMDbElement(print_buffer);

	//set window caption
	cc_win->setWindowCaption(title);

	//create title item
	CComponentsText *cc_title = new CComponentsText();
	cc_title->setDimensionsAll(left_offset, item_top, w_body-left_offset, h_imdbtitle);
	cc_title->setText(print_buffer, CTextBox::TOP, g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]);
	//cc_title->setColorBody(COL_MENUCONTENTSELECTED_PLUS_0); /*display text body*/

	//add title item to window object
	cc_win->addWindowItem(cc_title);

	item_top += h_imdbtitle+ fontheight;

	//create poster item
	CComponentsPicture *cc_pic = new CComponentsPicture(left_offset, item_top, w_poster_max, h_poster_max, posterfile);
#if 0
	cc_pic->setMaxWidth(w_poster_max);
	cc_pic->setMaxHeight(h_poster_max);
	cc_pic->setPictureAlign(CC_ALIGN_TOP | CC_ALIGN_LEFT);
#endif
	cc_pic->setColorBody(COL_MENUCONTENT_PLUS_0);

	//get picture size
	int w_pic = 0, h_pic = 0;
	cc_pic->getSize(&w_pic, &h_pic);
	printf("FIXME! cc_pic->getSize(&w_pic, &h_pic); *** w_pic = %i, h_pic = %i\n",w_pic, h_pic);
	//the Fix
	w_pic = 0;
	h_pic = 0;
	extern CPictureViewer * g_PicViewer;
	g_PicViewer->getSize(posterfile.c_str(), &w_pic, &h_pic);
        printf("THE FIX g_PicViewer->getSize(posterfile.c_str(), &w_pic, &h_pic); *** w_pic = %i, h_pic = %i\n",w_pic, h_pic);


	//add poster item to window object
	cc_win->addWindowItem(cc_pic);

	//rating
	print_buffer = "imdbRating";
	getIMDbElement(print_buffer);

	pg_value = print_buffer;
	if(print_buffer == "N/A"){
		pg_value = "1";
		print_buffer = "Keine Bewertung\n";
	}
	else
		print_buffer += "/10\n";

	size_t pos = pg_value.find_first_of(",.");
	if(pos!= std::string::npos)
		pg_value.replace(pos,1,""); // change 8,1 or 8.1 to 81

	//get IMDb text data
	getIMDbData(print_buffer);

	left_offset += w_pic+ (w_pic == 0 ? 0 : 30);

	//create starbar item
	CProgressBar *cc_starbar = new CProgressBar();
	cc_starbar->setProgress(left_offset, item_top, w_starbar, h_starbar, atoi(pg_value.c_str()), 100);
	cc_starbar->setType(CProgressBar::PB_STARBAR);
	cc_starbar->setFrameThickness(0);

	//add starbar item to window body object
	cc_win->addWindowItem(cc_starbar);

	item_top += h_starbar;
	int h_txt =  h_body- v_offset- h_imdbtitle- fontheight- h_starbar;

	//create imdbtext item
	if (cc_txt == NULL)
		cc_txt = new CComponentsText();
	cc_txt->setDimensionsAll(left_offset, item_top, w_body-left_offset, h_txt);
	cc_txt->setText(print_buffer, CTextBox::TOP | CTextBox::SCROLL, g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]);
	//cc_imdbtext->setColorBody(COL_MENUCONTENTSELECTED_PLUS_0); /*display text body*/

	//add imdbtext item to window body object
	cc_win->addWindowItem(cc_txt);

	//create button items
	CComponentsButtonGreen	*cc_button_gn = new CComponentsButtonGreen(btn_start, 0, btn_width, btn_height, "Bild speichern",
		/*parent*/		NULL,
		/*selected*/		false,
		/*enabled*/		true,
		/*has_shadow*/		false,
		/*color_frame*/		winfooter->getColorBody(),
		/*color_body*/		winfooter->getColorBody(),
		/*color_shadow*/	COL_MENUCONTENTDARK_PLUS_0);
	cc_button_gn->doPaintBg(false);
	cc_button_gn->setButtonTextColor(COL_INFOBAR_SHADOW_TEXT);
	cc_button_gn->setColBodyGradient(CC_COLGRAD_OFF);
	cc_button_gn->setFrameThickness(0);

	//add button object to window footer
	winfooter->addCCItem(cc_button_gn);

	cc_win->paint();

	bigFonts(false);
}

void CIMDB::bigFonts(bool on)
{
	if (g_settings.bigFonts)
	{
		if (on)
			g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getSize() * BIG_FONT_FAKTOR));
		else
			g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getSize() / BIG_FONT_FAKTOR));
	}
}
