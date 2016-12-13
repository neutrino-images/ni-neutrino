/*
	infoicons

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
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>

#include <global.h>
#include <neutrino.h>
#include <pthread.h>
#include <system/helpers.h>

#include <gui/widget/hintbox.h>

#include "infoicons.h"

#define STARTUPREFRESH 3

typedef struct icon_data_t
{
	int last_stat;
	int sizeW;
	int sizeH;
	int space;
	const char * name;
	const char * flag;
} icon_data_struct;

static icon_data_t icon[MODE_ICONS_NR_OF_ENTRIES]=
{
	{-1, 0, 0, OFFSET_INNER_NONE , "info0", ""},
	{-1, 0, 0, OFFSET_INNER_SMALL, "info1", ""},
	{-1, 0, 0, OFFSET_INNER_SMALL, "info2", ""},
	{-1, 0, 0, OFFSET_INNER_SMALL, "info3", ""},
	{-1, 0, 0, OFFSET_INNER_SMALL, "info4", ""},
	{-1, 0, 0, OFFSET_INNER_SMALL, "info5", ""},
	{-1, 0, 0, OFFSET_INNER_SMALL, "info6", ""},
	{-1, 0, 0, OFFSET_INNER_SMALL, "info7", ""}
};

CInfoIcons::CInfoIcons()
{
	frameBuffer	= CFrameBuffer::getInstance();

	x		= 0;
	y		= 0;
	width		= 0;
	height		= 0;
	offset		= 0;
	start_i		= 0;
	m_entries	= MODE_ICONS_NR_OF_ENTRIES;
	popup		= 0;

	thrTimer	= 0;
	status		= false;

	Init();
}

CInfoIcons::~CInfoIcons()
{
	if(thrTimer)
		pthread_cancel(thrTimer);
	thrTimer = 0;
}

CInfoIcons* CInfoIcons::getInstance()
{
	static CInfoIcons* InfoIcons = NULL;
	if(!InfoIcons)
		InfoIcons = new CInfoIcons();
	return InfoIcons;
}

void CInfoIcons::Init()
{
	std::ostringstream buf;

	x	= frameBuffer->getScreenX() + OFFSET_INNER_MID;
	y	= frameBuffer->getScreenY();
	offset	= OFFSET_INNER_MID;

	int icons_width  = 0;
	int icons_height = 0;

	for (int i=0; i < m_entries; i++)
	{
		int icon_sizeW	= 0;
		int icon_sizeH	= 0;
		// we assume the _off-partner exist and has same dimensions
		buf.str("");
		buf << icon[i].name << "_on";
		frameBuffer->getIconSize(buf.str().c_str(), &icon_sizeW, &icon_sizeH);

		if (icon_sizeW && icon_sizeH)
		{
			icons_width  += icon[i].space;
			icons_width  += icon_sizeW;
			icons_height  = std::max(icons_height, icon_sizeH);
			icon[i].sizeW = icon_sizeW;
			icon[i].sizeH = icon_sizeH;
			icon[i].flag  = g_settings.mode_icons_flag[i].c_str();
			icon[i].last_stat = file_exists(icon[i].flag);
		}
		else
		{
			m_entries = i;
			break;
		}
	}

	width	= icons_width  + 2*offset;
	height	= icons_height + 2*offset;

	printf("CInfoIcons::Init() reports %i entries\n",m_entries);
}

void CInfoIcons::paintBackground()
{
	if (g_settings.mode_icons_background)
	{
		frameBuffer->paintBoxRel(x + OFFSET_SHADOW, y + OFFSET_SHADOW, width, height, COL_SHADOW_PLUS_0, RADIUS_SMALL);
		frameBuffer->paintBoxRel(x, y, width, height, COL_MENUCONTENT_PLUS_0, RADIUS_SMALL);
	}
	else
	{
		frameBuffer->paintBackgroundBoxRel(x, y, width, height);
	}
}

void CInfoIcons::paintIcons(bool first)
{
	std::ostringstream buf;
	bool reload	= false;
	int startx	= x + offset;
	int starty	= 0;
	popup		= (g_settings.mode_icons_skin == INFOICONS_POPUP) ? 1 : 0;

	if(first)
	{
		if(!popup)
		{
			paintBackground();
			reload = true;
		}
		else
		{
			FILE *fh=NULL;
			if ((fh = fopen("/var/etc/infoicons","r"))!=NULL)
			{
				int i = 0;
				int c;

				while ((c = fgetc(fh)) != EOF && i < MODE_ICONS_NR_OF_ENTRIES)
				{
					if(c=='1')
						icon[i].last_stat = 1;
					else
						icon[i].last_stat = 0;
					i++;
				}
				fclose(fh);
				unlink("/var/etc/infoicons");
			}
		}
	}

	if(!popup)
	{
		//FIXME check for reload while startup
		if (!reload && (start_i < STARTUPREFRESH)) {
			start_i++;
			paintBackground();
			reload = true;
		}
	}

	//check for reload
	if(!reload)
	{
		for (int i=0; i < m_entries; i++)
		{
			if (file_exists(icon[i].flag) != icon[i].last_stat)
			{
				printf("CInfoIcons::paintIcons: %s status change: %d ==> %d\n",icon[i].flag, icon[i].last_stat, file_exists(icon[i].flag));
				reload = true;
				break;
			}
		}
	}

	if(reload)
	{
		if(popup)
			paintBackground();

		for (int i=0; i < m_entries; i++)
		{
			startx += icon[i].space;
			starty = y + ((height - icon[i].sizeH) / 2);

			buf.str("");
			if (file_exists(icon[i].flag))
			{
				buf << icon[i].name << "_on";
				icon[i].last_stat = 1;
			}
			else
			{
				buf << icon[i].name << "_off";
				icon[i].last_stat = 0;
			}

			//printf("CInfoIcons::paintIcons: %s\n",buf);
			frameBuffer->paintIcon(buf.str().c_str(), startx, starty);

			startx += icon[i].sizeW;
		}
	}
}

void CInfoIcons::hideIcons()
{
	frameBuffer->paintBackgroundBoxRel(x, y, width + OFFSET_SHADOW, height + OFFSET_SHADOW);
}

void* CInfoIcons::TimerProc(void *arg)
{
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,0);
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS,0);

	CInfoIcons *InfoIcons = (CInfoIcons*) arg;

	InfoIcons->paintIcons(true);
	while(1) {
		sleep(5);
		InfoIcons->paintIcons(false);
	}
	return 0;
}

void CInfoIcons::StartIcons(bool msg)
{
	if(!thrTimer) {
		printf("CInfoIcons::StartIcons(%d)\n",msg);
		pthread_create (&thrTimer, NULL, TimerProc, (void*) this) ;
		pthread_detach(thrTimer);
		status = true;

		if(msg && popup)
			ShowHint(LOCALE_MESSAGEBOX_INFO,g_Locale->getText(LOCALE_INFOICONS_MSG_START), 400, 3);
	}
}

void CInfoIcons::StopIcons(bool msg)
{
	if(thrTimer) {
		printf("CInfoIcons::StopIcons(%d)\n",msg);
		pthread_cancel(thrTimer);
		thrTimer = 0;
		hideIcons();
		status = false;

		if(msg && popup)
			ShowHint(LOCALE_MESSAGEBOX_INFO,g_Locale->getText(LOCALE_INFOICONS_MSG_STOP), 400, 3);
	}
}

//switching Infoicons on or off depends of current displayed or not
void CInfoIcons::switchInfoIconsOnOff()
{
	if(g_settings.mode_icons) {
		StopIcons(true); 
		g_settings.mode_icons=false;
	} else {
		StartIcons(true);
		g_settings.mode_icons=true;
	}
}

void CInfoIcons::enableInfoIcons(bool enable)
{
	if (g_settings.mode_icons) {
		if (enable)
			StartIcons();
		else
			StopIcons();
	}
}

void CInfoIcons::saveIconstate()
{
	std::ofstream file("/var/etc/infoicons");

	if (file.is_open())
	{
		for (int i=0; i < m_entries; i++) {
			file << icon[i].last_stat;
		}
		file.close();
	}
}
