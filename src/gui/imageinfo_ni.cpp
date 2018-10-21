/*
	imageinfo_ni

	(C) 2009-2016 NG-Team
	(C) 2016,2017 NI-Team

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

#include <gui/imageinfo_ni.h>

#include <global.h>
#include <neutrino.h>

#include <driver/fade.h>
#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>
#include <driver/display.h>

#include <sys/utsname.h>

#include <system/flashtool.h>
#include <video.h>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <gui/color_custom.h>
#include <gui/components/cc.h>
#ifdef ENABLE_LUA
#include <gui/lua/lua_api_version.h>
#endif
#include <system/helpers.h>
#include <system/debug.h>
#include <cs_api.h>

#include <linux/version.h>

#include <gui/pictureviewer.h>
extern CPictureViewer * g_PicViewer;

#include <sys/time.h>
static unsigned long time2ms (const struct timeval *tv)
{
	return (tv->tv_sec * 1000) + ((tv->tv_usec + 500) / 1000);
}

long delta_time (struct timeval *tv, struct timeval *last_tv)
{
	return time2ms (tv) - time2ms (last_tv);
}

CImageInfoNI::CImageInfoNI()
{
	Init();
}

static const neutrino_locale_t info_items[] =
{
	LOCALE_IMAGEINFO_IMAGE,
	LOCALE_IMAGEINFO_VERSION,
	/* Commit: */
	LOCALE_IMAGEINFO_KERNEL,
	/* Lua-API: */
	/* yWeb: */
	LOCALE_IMAGEINFO_DATE,
	LOCALE_IMAGEINFO_CREATOR,
	LOCALE_IMAGEINFO_HOMEPAGE
};
int info_items_count = sizeof(info_items)/sizeof(info_items[0]);

void CImageInfoNI::Init(void)
{
	frameBuffer	= CFrameBuffer::getInstance();

	font_head	= SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME;;
	font_small	= SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL;
	font_info	= SNeutrinoSettings::FONT_TYPE_MENU;

	hheight		= g_Font[font_head]->getHeight();
	iheight		= g_Font[font_info]->getHeight();
	sheight		= g_Font[font_small]->getHeight();
	swidth		= g_Font[font_small]->getWidth();

	max_width	= frameBuffer->getScreenWidth(true);
	max_height	= frameBuffer->getScreenHeight(true);

	width		= frameBuffer->getScreenWidth() - 2*OFFSET_INNER_MID;
	height		= frameBuffer->getScreenHeight() - 2*OFFSET_INNER_MID;
	x		= getScreenStartX(width);
	y		= getScreenStartY(height);

	// This single correction saves many offsets in code below
	x		+= OFFSET_INNER_MID;
	width		-= OFFSET_INNER_MID;

	systemfs	= 0;
	revision	= cs_get_revision();
	old_x		= 0;
	old_y		= 0;
	InfoThread	= 0;
	read_old	= 0;
	write_old	= 0;
	net_best	= 0;
	xcpu		= x + width - width/3 - OFFSET_INNER_MID;
	ycpu		= y + OFFSET_INNER_MID + (height/3 /* pig-box height */) + (2 * iheight);
	max_text_width	= xcpu - x - OFFSET_INNER_MID;

	offset		= 0;
	for (int i = 0; i < info_items_count; i++) {
		int tmpoffset = g_Font[font_info]->getRenderWidth(g_Locale->getText(info_items[i]));
		if (tmpoffset > offset)
			offset = tmpoffset;
	}
	offset		= offset + OFFSET_INNER_MID;

	netIfName	= "eth0"; //FIXME; What about the other ports?
}

CImageInfoNI::~CImageInfoNI()
{
	StopInfoThread();
}

int CImageInfoNI::exec(CMenuTarget* parent, const std::string &)
{
	int res = menu_return::RETURN_REPAINT;
	if (parent)
		parent->hide();

	neutrino_msg_t msg;
	neutrino_msg_t postmsg = 0;

	_stat cpu;
	cpu.usr		= 0;
	cpu.nice	= 0;
	cpu.system	= 0;
	cpu.idle	= 0;

	sigBox_pos	= 0;

	COSDFader fader(g_settings.theme.menu_Content_alpha);
	fader.StartFadeIn();
	bool fadeout = false;
	paint();
	paint_pic(xcpu, y + OFFSET_INNER_MID, width/3, height/3);
	StartInfoThread();

	while (1)
	{
		neutrino_msg_data_t data;
		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd_MS(100);
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		if ((msg == NeutrinoMessages::EVT_TIMER) && (data == fader.GetFadeTimer()))
		{
			if (fader.FadeDone())
			{
				break;
			}
			continue;
		}
		if (fadeout && msg == CRCInput::RC_timeout)
		{
			if (fader.StartFadeOut())
			{
				msg = 0;
				continue;
			}
			else
			{
				break;
			}
		}

		if (msg == CRCInput::RC_setup)
		{
			res = menu_return::RETURN_EXIT_ALL;
			fadeout = true;
		}
		else if (CNeutrinoApp::getInstance()->listModeKey(msg))
		{
			postmsg = msg;
			res = menu_return::RETURN_EXIT_ALL;
			fadeout = true;
		}
		else if (msg == (neutrino_msg_t) g_settings.key_screenshot)
		{
			CNeutrinoApp::getInstance()->handleMsg(msg, data);
			continue;
		}
		else if (msg <= CRCInput::RC_MaxRC)
		{
			fadeout = true;
		}

		if (msg > CRCInput::RC_MaxRC && msg != CRCInput::RC_timeout)
		{
			CNeutrinoApp::getInstance()->handleMsg(msg, data);
		}

		Stat_Info(&cpu);
		paint_CPU_Percent(CPU_Percent(&cpu));
	}

	StopInfoThread();

	hide();
	fader.StopFade();

	if (postmsg)
	{
		g_RCInput->postMsg(postmsg, 0);
	}

	return res;
}

void CImageInfoNI::hide()
{
	frameBuffer->paintBackground();
}

void CImageInfoNI::paint_pic(int px, int py, int w, int h)
{
	frameBuffer->paintBoxRel(px - OFFSET_INNER_MID, py - OFFSET_INNER_MID, w + 2*OFFSET_INNER_MID, h + 2*OFFSET_INNER_MID, COL_INFOBAR_PLUS_0);
	g_PicViewer->DisplayImage(ICONSDIR "/start.jpg", px, py, w, h, frameBuffer->TM_NONE);
}

void CImageInfoNI::paintLine(int xpos, int font, std::string text)
{
	g_Font[font]->RenderString(xpos, ypos, max_text_width, text, COL_INFOBAR_TEXT);
}

void CImageInfoNI::clearLine(int xpos, int font)
{
	int font_height = g_Font[font]->getHeight();
	frameBuffer->paintBoxRel(xpos, ypos - font_height, max_text_width, font_height, COL_BLUE/*INFOBAR_PLUS_0*/);
}

void CImageInfoNI::paint()
{
	int xpos = x;
	ypos = y;

	const char * head_string;
	head_string = g_Locale->getText(LOCALE_IMAGEINFO_HEAD);
	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, head_string);

	//background
	frameBuffer->paintBoxRel(0, 0, max_width, max_height, COL_INFOBAR_PLUS_0);

	// just for visual debug
	//frameBuffer->paintBoxRel(xpos, ypos, width, height, COL_INFOBAR_PLUS_1);

	ypos += hheight;
	g_Font[font_head]->RenderString(xpos, ypos, max_text_width, head_string, COL_MENUHEAD_TEXT);

	ypos += iheight/2;

	CConfigFile config('\t');
	config.loadConfig(TARGET_PREFIX "/.version");

	std::string imagename	= config.getString("imagename", "NI-Neutrino-HD");
	std::string homepage	= config.getString("homepage",  "www.neutrino-images.de");
	std::string creator	= config.getString("creator",   "NI-Team");
	std::string version	= config.getString("version",   "n/a");
	std::string commit	= config.getString("commit",    "n/a");
	std::string builddate	= config.getString("builddate", "n/a");

	std::ostringstream imageversion;
	imageversion.str("n/a");

	if (version.compare("n/a") != 0)
	{
		static CFlashVersionInfo versionInfo(version);
		std::string releaseCycle = versionInfo.getReleaseCycle();
		imageversion.str("");
		imageversion << releaseCycle << " (" << versionInfo.getType(true) << ")";
	}
#ifdef PACKAGE_VERSION
	else
	{
		imageversion.str("");
		imageversion << PACKAGE_VERSION;
	}
#endif

	ypos += iheight;
	paintLine(xpos, font_info, g_Locale->getText(LOCALE_IMAGEINFO_IMAGE));
	paintLine(xpos+offset, font_info, imagename);

	ypos += iheight;
	paintLine(xpos, font_info, g_Locale->getText(LOCALE_IMAGEINFO_VERSION));
	paintLine(xpos+offset, font_info, imageversion.str());

	ypos += iheight;
	paintLine(xpos, font_info, "Commit:");
	paintLine(xpos+offset, font_info, commit);

	struct utsname uts_info;

	ypos += iheight;
	paintLine(xpos, font_info, g_Locale->getText(LOCALE_IMAGEINFO_KERNEL));
	paintLine(xpos+offset, font_info, uname(&uts_info) < 0 ? "n/a" : uts_info.release);

#ifdef ENABLE_LUA
	ypos += iheight;
	paintLine(xpos, font_info, "Lua-API:");
	paintLine(xpos+offset, font_info, to_string(LUA_API_VERSION_MAJOR) + "." + to_string(LUA_API_VERSION_MINOR));
#endif

	ypos += iheight;
	paintLine(xpos, font_info, "yWeb:");
	paintLine(xpos+offset, font_info, getYWebVersion());

	ypos += iheight;
	paintLine(xpos, font_info, g_Locale->getText(LOCALE_IMAGEINFO_DATE));
	paintLine(xpos+offset, font_info, builddate );
	
	ypos += iheight;
	paintLine(xpos, font_info, g_Locale->getText(LOCALE_IMAGEINFO_CREATOR));
	paintLine(xpos+offset, font_info, creator);

	ypos += iheight;
	paintLine(xpos, font_info, g_Locale->getText(LOCALE_IMAGEINFO_HOMEPAGE));
	paintLine(xpos+offset, font_info, homepage);

	ypos += iheight;

#if !HAVE_GENERIC_HARDWARE && !HAVE_ARM_HARDWARE
	get_MTD_Info();
	//paint_MTD_Info(xpos);
	//ypos += sheight;

	paint_DF_Info(xpos);
	ypos+= sheight;
#endif

	paint_Stat_Info_Box(xcpu, ycpu, width/3, height/3);
}

void* CImageInfoNI::InfoProc(void *arg)
{
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,0);
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS,0);

	CImageInfoNI *imageInfo = (CImageInfoNI*) arg;

	while(1) {
		imageInfo->paint_MEM_Info(imageInfo->x, imageInfo->ypos);
		imageInfo->paint_NET_Info(imageInfo->x, imageInfo->ypos);
		sleep(1);
	}
	return 0;
}

void CImageInfoNI::StartInfoThread()
{
	if(!InfoThread) {
		printf("CImageInfoNI::StartInfoThread\n");
		pthread_create (&InfoThread, NULL, InfoProc, (void*) this) ;
		pthread_detach(InfoThread);
	}
}

void CImageInfoNI::StopInfoThread()
{
	if(InfoThread) {
		printf("CImageInfoNI::StopInfoThread\n");
		pthread_cancel(InfoThread);
		InfoThread = 0;
	}
}

std::string CImageInfoNI::get_systemRoot()
{
	std::fstream fh;
	std::string s;
	std::string root = "";
	const std::string file = "/proc/cmdline";
	const std::string str = "root=mtd:";

	fh.open(file.c_str(), std::ios::in);

	if(!fh.is_open())
	{
		return root;
	}

	while (!fh.eof())
	{
		getline(fh, s);

		std::string::size_type begin = s.find(str) + str.length();
		std::string::size_type end = s.find(' ', begin);
		root = s.substr(begin, end - begin);

		if(!root.empty())
			break;
	}
	fh.close();
	return root;
}

int CImageInfoNI::get_MTD_Info()
{
	FILE *fh;
	char *buffer;
	ssize_t read;
	size_t len;
	int i = 0;

	memset(&mtd_info, 0, sizeof(mtd_info));

	buffer=NULL;
	if (!(fh=fopen("/proc/mtd", "r")))
		return false;

#ifdef BOXMODEL_CS_HD2
	std::string sysfs = get_systemRoot();
#else
	std::string sysfs = "systemFS";
#endif

	while ((read = getline(&buffer, &len, fh)) != -1)
	{
		S_MTD_INFO entry;

		if (strstr(buffer, "mtd") != 0)
		{
			sscanf(buffer, "%7[^:]: %8s %8s \"%39[^\"]",
				entry.dev,
				entry.size,
				entry.erasesize,
				entry.name);
			mtd_info.push_back(entry);

			if (strstr(mtd_info[i].name, sysfs.c_str()))
				systemfs=i;
			i++;
		}
	}
	fclose(fh);
	if(buffer)
		free(buffer);
	return 0;
}

#if 0
void CImageInfoNI::paint_MTD_Info(int posx)
{
	for (int i=0; i<(int)mtd_info.size(); i++)
	{
		char buf[(strlen(mtd_info[i].dev)+strlen(mtd_info[i].name)+3)];

		sprintf((char*) buf, "%s: %s", mtd_info[i].dev, mtd_info[i].name);
		paintLine(posx, font_small, buf);
		ypos+= sheight;
	}
}
#endif

int CImageInfoNI::Stat_Info(_stat *stat_info)
{
	FILE *fh;
	char *ptr;
	char *buffer;
	ssize_t read;
	size_t len;

	stat_info->old_usr	= stat_info->usr;
	stat_info->old_nice	= stat_info->nice;
	stat_info->old_system	= stat_info->system;
	stat_info->old_idle	= stat_info->idle;

	buffer=NULL;
	if (!(fh = fopen("/proc/stat", "r")))
		return false;

	while ((read = getline(&buffer, &len, fh)) != -1)
	{
		if ((ptr = strstr(buffer, "cpu")))
		{
			sscanf(ptr+3, "%lu %i %lu %lu", &stat_info->usr, &stat_info->nice, &stat_info->system, &stat_info->idle);
			break;
		}
	}
	fclose(fh);
	if(buffer)
		free(buffer);
	return true;
}

int CImageInfoNI::CPU_Percent(_stat *cpu)
{
	long unsigned int sys_old = cpu->old_usr + cpu->old_nice + cpu->old_system;
	long unsigned int total_old = cpu->old_usr + cpu->old_nice + cpu->old_system + cpu->old_idle;

	long unsigned int sys_new = cpu->usr + cpu->nice + cpu->system;
	long unsigned int total_new = cpu->usr + cpu->nice + cpu->system + cpu->idle;

	long unsigned int sys_diff = sys_new - sys_old;
	long unsigned int total_diff = total_new - total_old;

	long unsigned int percent = ((float) sys_diff / (float) total_diff) * 100;

	if (sys_diff > 1 || total_diff > 1)
		return(percent);
	else
		return(0);
}

void CImageInfoNI::paint_Stat_Info_Box(int InfoB_x, int InfoB_y, int InfoB_w, int InfoB_h)
{
	frameBuffer->paintBoxRel(InfoB_x - OFFSET_INNER_MID, InfoB_y - OFFSET_INNER_MID, InfoB_w + 2*OFFSET_INNER_MID, InfoB_h + 2*OFFSET_INNER_MID, COL_INFOBAR_PLUS_0);
	g_Font[font_info]->RenderString(InfoB_x, InfoB_y, InfoB_w, "CPU-Load", COL_INFOBAR_TEXT);

	sigBox_x = InfoB_x;
	sigBox_y = InfoB_y;
	sigBox_w = InfoB_w;
	sigBox_h = InfoB_h;

	frameBuffer->paintBoxRel(sigBox_x,sigBox_y,sigBox_w,sigBox_h, COL_BLACK);
}

void CImageInfoNI::paint_CPU_Percent(int percent)
{
	int x_now = sigBox_pos;
	int sigBox_pos_t = sigBox_pos;
	int yd;

	sigBox_pos = (++sigBox_pos_t) % sigBox_w;

	frameBuffer->paintVLine(sigBox_x+sigBox_pos, sigBox_y, sigBox_y+sigBox_h, COL_WHITE);
	frameBuffer->paintVLine(sigBox_x+x_now,      sigBox_y, sigBox_y+sigBox_h, COL_BLACK);

	yd = y_cpu_pixel(percent, 100, sigBox_h);

	if ((old_x == 0 && old_y == 0) || sigBox_pos == 1)
	{
		old_x = sigBox_x+x_now;
		old_y = sigBox_y+sigBox_h-yd;
	}
	else
	{
		frameBuffer->paintLine(old_x, old_y, sigBox_x+x_now, sigBox_y+sigBox_h-yd, COL_RED);
		old_x = sigBox_x+x_now;
		old_y = sigBox_y+sigBox_h-yd;
	}
}

int CImageInfoNI::y_cpu_pixel(int value, int max_value, int max_y)
{
	int  l;

	if (!max_value) max_value = 1;

	l = (max_y * value ) / max_value;
	if (l > max_y) l = max_y;

	return l;
}

void CImageInfoNI::get_DF_Info()
{
	FILE *pipe_reader;
	char *ptr;
	char *buffer;
	ssize_t read;
	size_t len;

	memset(&image_size, 0, sizeof(image_size));

	buffer=NULL;
	if (!(pipe_reader = popen ("df", "r")))
		printf("[read_df] popen error\n" );

#ifdef BOXMODEL_CS_HD2
	strcpy(mtd_info[systemfs].dev, ("mtd:"+get_systemRoot()).c_str());
#endif

 	while ((read = getline(&buffer, &len, pipe_reader)) != -1)
 	{
		if ((ptr = strstr(buffer, mtd_info[systemfs].dev))) {
			sscanf(ptr+strlen(mtd_info[systemfs].dev), "%ld\t%ld\t%ld\t%i",
				&image_size.blocks,
				&image_size.used,
				&image_size.available,
				&image_size.percent);
		}
	}
	pclose(pipe_reader);
	if(buffer)
		free(buffer);
}

void CImageInfoNI::paint_DF_Info(int posx)
{
	ypos += sheight/2;

	std::ostringstream buf;
	int boxH = sheight - 2*OFFSET_INNER_MIN;
	int boxW = swidth*8;
	int boxX = posx + width/3 - boxW;
	int boxY = ypos - sheight + OFFSET_INNER_MIN;

	get_DF_Info();

	buf << "Imagesize (" << image_size.percent << "%):";
	paintLine(posx, font_small, buf.str());

	CProgressBar pb(boxX, boxY, boxW, boxH);
	pb.setFrameThickness(FRAME_WIDTH_NONE);
	pb.setRgb(70, 100, 90);
	pb.setType(CProgressBar::PB_REDRIGHT);
	pb.setValues(image_size.percent, 100);
	pb.paint(false);

	buf.str("");
	buf.precision(2);
	buf << std::fixed;

	if (image_size.blocks > 1024)
	{
		if (image_size.used > 1024)
			buf << "Total: " << (image_size.blocks/1024.0) << " MB   Used: " << (image_size.used/1024.0) << " MB";
		else
			buf << "Total: " << (image_size.blocks/1024.0) << " MB   Used: " << image_size.used << " KB";
	}
	else
	{
		if (image_size.used > 1024)
			buf << "Total: " << image_size.blocks << " KB   Used: " << (image_size.used/1024.0) << " MB";
		else
			buf << "Total: " << image_size.blocks << " KB   Used: " << image_size.used << " KB";
	}

	ypos+= sheight;
	paintLine(posx, font_small, buf.str());

	buf.str("");
	if (image_size.available > 1024)
		buf << "Free: " << (image_size.available/1024.0) << " MB";
	else
		buf << "Free: " << image_size.available << " KB";

	ypos += sheight;
	paintLine(posx, font_small, buf.str());
}

int CImageInfoNI::get_MEM_Info()
{
	FILE *fh;
	char *ptr;
	char *buffer;
	ssize_t read;
	size_t len;

	memset(&mem_info, 0, sizeof(mem_info));

	buffer=NULL;
	if (!(fh = fopen("/proc/meminfo", "r")))
		return false;

	while ((read = getline(&buffer, &len, fh)) != -1)
	{
		if ((ptr = strstr(buffer, "MemTotal:")))
			sscanf(ptr+10, "%i", &mem_info.total);
		else if ((ptr = strstr(buffer, "MemFree:")))
			sscanf(ptr+9, "%i", &mem_info.free);
		else if ((ptr = strstr(buffer, "Buffers:")))
			sscanf(ptr+9, "%i", &mem_info.buffers);
		else if ((ptr = strstr(buffer, "Cached:"))) {
			sscanf(ptr+8, "%i", &mem_info.cached);
			break;
		}
	}
	fclose(fh);
	if(buffer)
		free(buffer);

	mem_info.tfree = mem_info.free + mem_info.buffers + mem_info.cached;
	mem_info.used = mem_info.total - mem_info.tfree;

	return true;
}

int CImageInfoNI::get_MEM_Percent(int total, int used)
{
	return ((used*200 + total) / 2 / total); //this is CST style ;)
}

void CImageInfoNI::paint_MEM_Info(int posx, int posy)
{
	std::ostringstream buf;

	posy += sheight/2;

	int boxH = sheight - 2*OFFSET_INNER_MIN;
	int boxW = swidth*8;
	int boxX = posx + width/3 - boxW;
	int boxY = posy - sheight + OFFSET_INNER_MIN ;

	get_MEM_Info();
	int mem_percent = get_MEM_Percent(mem_info.total, mem_info.used);

	buf << "Memory (" << mem_percent << "%):";
	// progressbar ist in same line - so we not can use max_text_width
	frameBuffer->paintBoxRel(posx, posy - sheight, boxX - posx, sheight, COL_INFOBAR_PLUS_0);
	g_Font[font_small]->RenderString(posx, posy, boxX - posx, buf.str().c_str(), COL_INFOBAR_TEXT);

	CProgressBar pb(boxX, boxY, boxW, boxH);
	pb.setFrameThickness(FRAME_WIDTH_NONE);
	pb.setRgb(70, 100, 90);
	pb.setType(CProgressBar::PB_REDRIGHT);
	pb.setValues(mem_percent, 100);
	pb.paint(false);

	posy+= sheight;
	buf.str("");
	buf.precision(2);
	buf << std::fixed << "Total: " << (mem_info.total/1024.0) << " MB   Used: " << (mem_info.used/1024.0) << " MB";
	frameBuffer->paintBoxRel(posx, posy - sheight, max_text_width, sheight, COL_INFOBAR_PLUS_0);
	g_Font[font_small]->RenderString(posx, posy, max_text_width, buf.str().c_str(), COL_INFOBAR_TEXT);

	posy+= sheight;
	buf.str("");
	buf	<< "Free: " << (mem_info.tfree < 1024 ? mem_info.tfree : (mem_info.tfree/1024.0))
		<< (mem_info.tfree < 1024?" KB ":" MB ")
		<< "(Buffers: " << (mem_info.buffers < 1024 ? mem_info.buffers : (mem_info.buffers/1024.0))
		<< (mem_info.buffers < 1024?" KB, ":" MB, ")
		<< "Cached: " << (mem_info.cached < 1024 ? mem_info.cached : (mem_info.cached/1024.0))
		<< (mem_info.cached < 1024?" KB":" MB)");

	frameBuffer->paintBoxRel(posx, posy - sheight, max_text_width, sheight, COL_INFOBAR_PLUS_0);
	g_Font[font_small]->RenderString(posx, posy, max_text_width, buf.str().c_str(), COL_INFOBAR_TEXT);
}

void CImageInfoNI::get_NET_Info(uint64_t *read_akt, long *read_packet, long *dummy, uint64_t *write_akt, long *write_packet)
{
	FILE *file		= NULL;
	char *ptr;
	char *line_buffer	= NULL;
	long *packet_ptr[3]	= {read_packet, write_packet, dummy};
	uint64_t *byte_ptr[2]	= {read_akt, write_akt};
	ssize_t read;
	size_t len;
	char interface[20];
	memset(interface, '\0', sizeof(interface));
	snprintf(interface, sizeof(interface)-1, "%s", netIfName.c_str());

	if ((file = fopen("/proc/net/dev","r"))==NULL)
	{
		printf("Open Proc-File Failure\n"); fflush(stdout);
	}
	else
	{
		while ((read = getline(&line_buffer, &len, file)) != -1)
		{
			if((ptr = strstr(line_buffer, interface))!=NULL)
			{
				dprintf(DEBUG_INFO, "Procline    = %s",line_buffer);

				sscanf(ptr+strlen(interface)+1,"%llu%ld%ld%ld%ld%ld%ld%ld%llu%ld",
					(long long unsigned int*)byte_ptr[0],
					(long*)packet_ptr[0],
					(long *)packet_ptr[2],
					(long *)packet_ptr[2],
					(long *)packet_ptr[2],
					(long *)packet_ptr[2],
					(long *)packet_ptr[2],
					(long *)packet_ptr[2],
					(long long unsigned int*)byte_ptr[1],
					(long*)packet_ptr[1]);
			}
		}
		fclose (file);
	}
	if(line_buffer)
		free(line_buffer);
}

void CImageInfoNI::paint_NET_Info(int posx, int posy)
{
#ifndef HAVE_GENERIC_HARDWARE
	if (g_settings.ifname != "eth0")
		return;
#endif

	uint64_t read_akt	=0;
	uint64_t write_akt	=0;
	uint64_t delta_read	=0;
	uint64_t delta_write	=0;
	long	read_packet	=0;
	long	write_packet	=0;
	long	dummy		=0;
	char temp_string[200]	="";

	posy += sheight/2;
	posy += 3*sheight + sheight/2; //height meminfo above that

	int boxH = sheight - 2*OFFSET_INNER_MIN;
	int boxW = swidth*8;
	int boxX = posx + width/3 - boxW;
	int boxY = posy - sheight + OFFSET_INNER_MIN ;

	/*
	 * write testing:
	 * dd if=/dev/zero of=/srv/netztest bs=8192 count=8192
	 * 8192+0 records in
	 * 8192+0 records out
	 * 67108864 bytes (64.0MB) copied, 8.658240 seconds, 7.4MB/s
	 *
	 * 1GB file read
	 * dd of=/dev/null if=/srv/netztest bs=8192 count=131072
	 */

	gettimeofday (&tv, NULL);
	get_NET_Info(&read_akt, &read_packet, &dummy, &write_akt, &write_packet);

	long d_time_ms	= delta_time(&tv, &last_tv);

	if(read_old != 0 && read_akt > read_old)
		delta_read = read_akt - read_old;
	if(write_old != 0 && write_akt > write_old)
		delta_write = write_akt - write_old;

	uint64_t rbit_s = (((uint64_t) delta_read * 8000ULL) + ((uint64_t) d_time_ms / 2ULL)) / (uint64_t) d_time_ms;
	uint64_t wbit_s = (((uint64_t) delta_write * 8000ULL) + ((uint64_t) d_time_ms / 2ULL)) / (uint64_t) d_time_ms;

	if((rbit_s+wbit_s) > net_best)
		net_best = rbit_s+wbit_s;

	/*
	 * 100		MBit
	 * 12,5		MByte	(MB)
	 * 12800	KByte	(KB)
	 * 102400	KBit
	 * 13107200	Byte
	 * 104857600	Bit
	 */
	int max_bit	= 104857600;
#ifdef BOXMODEL_CS_HD2
	if (revision == 9) /* Apollo */
		max_bit	= 1073741824;
#endif
	int percent	= ((rbit_s+wbit_s)*100/max_bit);

	dprintf(DEBUG_INFO,"read byte   = %llu - %llu\n",(long long unsigned int)read_akt,(long long unsigned int)read_old);
	dprintf(DEBUG_INFO,"write byte  = %llu - %llu\n",(long long unsigned int)write_akt,(long long unsigned int)write_old);
	dprintf(DEBUG_INFO,"read_packet = %ld\n",read_packet);
	dprintf(DEBUG_INFO,"write_packet= %ld\n",write_packet);
	dprintf(DEBUG_INFO,"delta time  = %10ld ms\n",d_time_ms);
	dprintf(DEBUG_INFO,"read        = %10llu bit/s\n",(long long unsigned int)rbit_s);
	dprintf(DEBUG_INFO,"write       = %10llu bit/s\n",(long long unsigned int)wbit_s);
	dprintf(DEBUG_INFO,"total       = %10llu bit/s (%d%%) / best %llu (%.2f MB/s) = (%d%%)\n\n",
		(long long unsigned int)rbit_s+wbit_s,
		percent,
		(long long unsigned int)net_best,
		net_best/(8.0*1024.0*1024.0),
		(int)(net_best*100/max_bit)
	);

	sprintf(temp_string,"Interface %s (%d%%):", netIfName.c_str(), percent);
	// progressbar ist in same line - so we not can use max_text_width
	frameBuffer->paintBoxRel(posx, posy - sheight, boxX - posx, sheight, COL_INFOBAR_PLUS_0);
	g_Font[font_small]->RenderString(posx, posy, boxX - posx, temp_string, COL_INFOBAR_TEXT);

	CProgressBar pb(boxX, boxY, boxW, boxH);
	pb.setFrameThickness(FRAME_WIDTH_NONE);
	pb.setRgb(70, 100, 90);
	pb.setType(CProgressBar::PB_REDRIGHT);
	pb.setValues(percent, 100);
	pb.paint(false);

	posy+= sheight;

	sprintf(temp_string,"Receive: %llu bit/s   Transmit: %llu bit/s",(long long unsigned int)rbit_s,(long long unsigned int)wbit_s);
	frameBuffer->paintBoxRel(posx, posy - sheight, max_text_width, sheight, COL_INFOBAR_PLUS_0);
	g_Font[font_small]->RenderString(posx, posy, max_text_width, temp_string, COL_INFOBAR_TEXT);

	posy+= sheight;

	sprintf(temp_string,"Maximal: %llu bit/s - %.2f MB/s (%d%%)",(long long unsigned int)net_best,net_best/(8.0*1024.0*1024.0),(int)((net_best*100)/max_bit));
	frameBuffer->paintBoxRel(posx, posy - sheight, max_text_width, sheight, COL_INFOBAR_PLUS_0);
	g_Font[font_small]->RenderString(posx, posy, max_text_width, temp_string, COL_INFOBAR_TEXT);

	last_tv.tv_sec	= tv.tv_sec;
	last_tv.tv_usec	= tv.tv_usec;
	read_old	= read_akt;
	write_old	= write_akt;
}

std::string CImageInfoNI::getYWebVersion()
{
	CConfigFile yV('=', false);
	yV.loadConfig(PRIVATE_HTTPDDIR "/Y_Version.txt");
	return yV.getString("version", "n/a");
}
