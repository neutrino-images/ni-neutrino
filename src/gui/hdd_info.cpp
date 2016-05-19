/*
	hdd_info

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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <mntent.h>
#include <dirent.h>
#include <sstream>

#include <gui/hdd_info.h>

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <driver/rcinput.h>
#include <driver/fade.h>

#include <sys/sysinfo.h>
#include <sys/vfs.h>

static int my_filter(const struct dirent * dent)
{
	if(dent->d_name[0] == 's' && dent->d_name[1] == 'd')
		return 1;
	return 0;
}

static char *trim(char *txt)
{
	register int l;
	register char *p1, *p2;

	if (*txt==' ')
	{
		for (p1=p2=txt;
			(*p1==' ') || (*p1=='\t') || (*p1=='\n') || (*p1=='\r');
			p1++){};
		while (*p1)
			*p2++=*p1++;
		*p2='\0';
	}
	if ((l=strlen(txt))>0)
		for (p1=txt+l-1;
			(*p1==' ') || (*p1=='\t') || (*p1=='\n') || (*p1=='\r');
			*p1--='\0'){};
	return(txt);
}

CHDDInfoMenu::CHDDInfoMenu()
{
	width = 35;
}

CHDDInfoMenu::~CHDDInfoMenu()
{

}

int CHDDInfoMenu::exec(CMenuTarget* parent, const std::string &actionKey)
{
	printf("[HDDInfo] exec ationKey %s\n",actionKey.c_str());
	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	if (actionKey!="")
	{
		COSDFader fader(g_settings.theme.menu_Content_alpha);
		fader.StartFadeIn();

		CHDDInfoWidget::paint(actionKey);

		//int res = g_RCInput->messageLoop();
		neutrino_msg_t      msg;
		neutrino_msg_data_t data;

		bool doLoop = true;

		int timeout = g_settings.timing[SNeutrinoSettings::TIMING_MENU];

		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd( timeout == 0 ? 0xFFFF : timeout);

		while (doLoop)
		{
			g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

			if((msg == NeutrinoMessages::EVT_TIMER) && (data == fader.GetFadeTimer())) {
				if(fader.FadeDone()) {
					doLoop = false;
					}
			}
			else if ( ( msg == CRCInput::RC_timeout ) ||
				( msg == CRCInput::RC_home ) ||
				( msg == CRCInput::RC_ok ) ) {
				if(fader.StartFadeOut()) {
					timeoutEnd = CRCInput::calcTimeoutEnd( 1 );
					msg = 0;
				} else
					doLoop = false;
			}
			else if((msg == CRCInput::RC_sat) || (msg == CRCInput::RC_favorites)) {
			}
			else
			{
				int mr = CNeutrinoApp::getInstance()->handleMsg( msg, data );

				if ( mr & messages_return::cancel_all )
				{
					res = menu_return::RETURN_EXIT_ALL;
					doLoop = false;
				}
				else if ( mr & messages_return::unhandled )
				{
					if ((msg <= CRCInput::RC_MaxRC) &&
						(data == 0))                     /* <- button pressed */
					{
						timeoutEnd = CRCInput::calcTimeoutEnd( timeout );
					}
				}
			}
		}

		CHDDInfoWidget::hide();
		fader.StopFade();

		return res;
	}
	res = show();
	return res;
}

int CHDDInfoMenu::show()
{
	FILE * f;
	struct dirent **namelist;
	bool hdd_found = 0;
	int n = scandir("/sys/block", &namelist, my_filter, alphasort);
	std::ostringstream buf;

	//menue init
	CMenuWidget* HDDInfo = new CMenuWidget(LOCALE_HDD_INFO_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_HDD_INFO);
	HDDInfo->addIntroItems();

	for(int i = 0; i < n;i++)
	{
		char model[128]="unbekannt";

		buf.str("");
		buf << "/sys/block/" << namelist[i]->d_name << "/device/model";

		f = fopen(buf.str().c_str(), "r");
		if(f) {
			fscanf(f, "%127[^\n]", (char *) &model);
			fclose(f);
		}
		else
			printf("Cant open %s\n", buf.str().c_str());

		buf.str("");
		buf << trim(model) << " (" << namelist[i]->d_name << ")";

		HDDInfo->addItem(new CMenuForwarder(buf.str().c_str(), true, NULL, this, namelist[i]->d_name));

		hdd_found = 1;
		free(namelist[i]);
	}
	if (n >= 0)
		free(namelist);

	if(!hdd_found)
		HDDInfo->addItem(new CMenuForwarder(LOCALE_HDD_NOT_FOUND, false));

	int res = HDDInfo->exec(NULL, "");
	HDDInfo->hide();
	delete HDDInfo;
	return res;
}

// ----------------------------------------------------------------------------

CHDDInfoWidget::CHDDInfoWidget()
{

}

CHDDInfoWidget::~CHDDInfoWidget()
{

}

#define locale_itemsCount 9
static const neutrino_locale_t locale_items[locale_itemsCount] =
{
	LOCALE_HDD_INFO_MODEL_FAMILY	,
	LOCALE_HDD_INFO_MODEL		,
	LOCALE_HDD_INFO_SERIAL		,
	LOCALE_HDD_INFO_FIRMWARE		,
	LOCALE_HDD_INFO_CAPACITY		,
	LOCALE_HDD_INFO_SECTOR_SIZE	,
	LOCALE_HDD_INFO_ROTATION_RATE	,
	LOCALE_HDD_INFO_SATA_VERSION	,
	LOCALE_HDD_INFO_TEMPERATURE
};

void CHDDInfoWidget::paint(const std::string &Key)
{
	frameBuffer = CFrameBuffer::getInstance();

	int hheight	= g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	int mheight	= g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	int sheight	= g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight();

	int offset	= 20;
	int bheight	= offset + locale_itemsCount*mheight + offset;	// body height
	int fheight	= sheight + offset/2;				// footer height

	width	= frameBuffer->getScreenWidth() / 100 * 50;
	height	= hheight + bheight + fheight;

	x	= getScreenStartX(width);
	y	= getScreenStartY(height);

	FILE *pipe_reader;
	char *buffer;
	ssize_t read;
	size_t len;
	char *found;
	std::string str_capacity;
	unsigned long long mb;
	std::ostringstream buf;

	char model_family[128]	= "";
	char model[128]		= "";
	char serial[128]	= "";
	char firmware[128]	= "";
	char capacity[128]	= "";
	char sector_size[128]	= "";
	char rotation_rate[128]	= "";
	char sata_version[128]	= "";
	char temperature[128]	= "";

	buf << "smartctl --all /dev/" << Key;

	buffer=NULL;

	if((pipe_reader = popen(buf.str().c_str(), "r")))
	{
		while ((read = getline(&buffer, &len, pipe_reader)) != -1)
		{
			if ((found = strstr(buffer, "Model Family:")))
				sscanf(found+18, "%127[^\n]", (char *) &model_family);
			else if ((found = strstr(buffer, "Device Model:")))
				sscanf(found+18, "%127[^\n]", (char *) &model);
			else if ((found = strstr(buffer, "Serial Number:")))
				sscanf(found+18, "%127[^\n]", (char *) &serial);
			else if ((found = strstr(buffer, "Firmware Version:")))
				sscanf(found+18, "%127[^\n]", (char *) &firmware);
			else if ((found = strstr(buffer, "User Capacity:")))
				sscanf(found+18, "%127[^\n]", (char *) &capacity);
			else if ((found = strstr(buffer, "Sector Size:")))
				sscanf(found+18, "%127[^\n]", (char *) &sector_size);
			else if ((found = strstr(buffer, "Rotation Rate:")))
				sscanf(found+18, "%127[^\n]", (char *) &rotation_rate);
			else if ((found = strstr(buffer, "SATA Version is:")))
				sscanf(found+18, "%127[^\n]", (char *) &sata_version);
			else if ((found = strstr(buffer, "Temperature_Celsius")))
				sscanf(found+83, "%3[^\n]", (char *) &temperature);
		}
		pclose(pipe_reader);
	}
	else
		printf("[read_smartctl] popen error\n" );

	if(buffer)
		free(buffer);

	// manipulating capacity
	str_capacity = capacity;
	str_capacity.erase(std::remove(str_capacity.begin(), str_capacity.end(), ','), str_capacity.end());
	mb = strtoull(str_capacity.c_str(),NULL,0)/1000000;
	buf.str("");
	if (mb != 0)
	{
		if (mb < 1000)
			buf << mb << " MB";
		else
			buf << mb/1000 << " GB";
	}
	snprintf(capacity, sizeof(capacity), "%s", buf.str().c_str());

	// manipulating temperature
	buf.str("");
	if (strcmp(temperature, "") != 0)
	{
		buf << trim(temperature) << " Grad Celsius";
		snprintf(temperature, sizeof(temperature), "%s", buf.str().c_str());
	}

	// calculate max width of used LOCALES
	int locwidth = 0;
	for (int i = 0; i < locale_itemsCount; i++) {
		int w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(locale_items[i]));
		locwidth = std::max(locwidth, w);
	}

	// calculate width of separator
	std::string separator	= " : ";
	int sepwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(separator);

	// calculate max width of data
	int datwidth = 0, w = 0;
	w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(trim(model_family));
	datwidth = std::max(datwidth, w);
	w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(trim(model));
	datwidth = std::max(datwidth, w);
	w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(trim(serial));
	datwidth = std::max(datwidth, w);
	w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(trim(firmware));
	datwidth = std::max(datwidth, w);
	w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(trim(capacity));
	datwidth = std::max(datwidth, w);
	w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(trim(sector_size));
	datwidth = std::max(datwidth, w);
	w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(trim(rotation_rate));
	datwidth = std::max(datwidth, w);
	w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(trim(sata_version));
	datwidth = std::max(datwidth, w);
	w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(trim(temperature));
	datwidth = std::max(datwidth, w);

	// recalculate width and xpos
	width = std::max(width, offset + locwidth + sepwidth + datwidth + offset);
	if (width > (int) frameBuffer->getScreenWidth())
	{
		// should only happen with very big fonts
		width = frameBuffer->getScreenWidth();
		datwidth = width - offset - locwidth - sepwidth - offset;
	}
	x = getScreenStartX(width);

	fprintf(stderr, "CHDDInfoWidget::CHDDInfoWidget() x = %d, y = %d, width = %d, height = %d\n", x, y, width, height);

	// paint backgrounds
	frameBuffer->paintBoxRel(x, y, width, hheight, COL_MENUHEAD_PLUS_0, RADIUS_LARGE, CORNER_TOP);
	frameBuffer->paintBoxRel(x, y+ hheight, width, bheight, COL_MENUCONTENT_PLUS_0);
	frameBuffer->paintBoxRel(x, y+ hheight+ bheight, width, fheight, COL_INFOBAR_SHADOW_PLUS_1, RADIUS_LARGE, CORNER_BOTTOM);

	// header
	int xpos = x + offset/2;
	int ypos = y + hheight;
	int icol_w = 0, icol_h = 0, icol_o = 0;

	frameBuffer->getIconSize(NEUTRINO_ICON_SETTINGS, &icol_w, &icol_h);
	if ( (icol_w) && (icol_h) )
	{
		frameBuffer->paintIcon(NEUTRINO_ICON_SETTINGS, xpos, y, hheight);
		icol_o = icol_w + offset/2;
	}

	buf.str("");
	buf << g_Locale->getText(LOCALE_HDD_INFO_HEAD) << " (" << Key << ")";
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(xpos + icol_o, ypos, width - offset - icol_o, buf.str(), COL_MENUHEAD_TEXT);

	// locale
	xpos = x + offset;
	ypos += offset;
	for (int i = 0; i < locale_itemsCount; i++) {
		ypos += mheight;
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(xpos, ypos, locwidth, g_Locale->getText(locale_items[i]), COL_MENUCONTENTINACTIVE_TEXT);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(xpos + locwidth, ypos, sepwidth, separator, COL_MENUCONTENTINACTIVE_TEXT);
	}

	// footer with centered content
	int wtmp = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth(g_Locale->getText(LOCALE_HDD_INFO_INFO));
	xpos = x + width/2 - wtmp/2;
	ypos = y + hheight + bheight + offset/4 + sheight;
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(xpos, ypos, wtmp, g_Locale->getText(LOCALE_HDD_INFO_INFO), COL_MENUCONTENTINACTIVE_TEXT);

	// finally paint data
	xpos = x + offset + locwidth + sepwidth;
	ypos = y + hheight + offset;

	ypos += mheight;
	buf.str("");
	buf << model_family;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(xpos, ypos, datwidth, (!strcmp(model_family, "") ? g_Locale->getText(LOCALE_HDD_INFO_UNKNOWN) : buf.str().c_str()), COL_MENUCONTENT_TEXT);

	ypos += mheight;
	buf.str("");
	buf << model;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(xpos, ypos, datwidth, (!strcmp(model, "") ? g_Locale->getText(LOCALE_HDD_INFO_UNKNOWN) : buf.str().c_str()), COL_MENUCONTENT_TEXT);

	ypos += mheight;
	buf.str("");
	buf << serial;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(xpos, ypos, datwidth, (!strcmp(serial, "") ? g_Locale->getText(LOCALE_HDD_INFO_UNKNOWN) : buf.str().c_str()), COL_MENUCONTENT_TEXT);

	ypos += mheight;
	buf.str("");
	buf << firmware;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(xpos, ypos, datwidth, (!strcmp(firmware, "") ? g_Locale->getText(LOCALE_HDD_INFO_UNKNOWN) : buf.str().c_str()), COL_MENUCONTENT_TEXT);

	ypos += mheight;
	buf.str("");
	buf << capacity;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(xpos, ypos, datwidth, (!strcmp(capacity, "") ? g_Locale->getText(LOCALE_HDD_INFO_UNKNOWN) : buf.str().c_str()), COL_MENUCONTENT_TEXT);

	ypos += mheight;
	buf.str("");
	buf << sector_size;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(xpos, ypos, datwidth, (!strcmp(sector_size, "") ? g_Locale->getText(LOCALE_HDD_INFO_UNKNOWN) : buf.str().c_str()), COL_MENUCONTENT_TEXT);

	ypos += mheight;
	buf.str("");
	buf << rotation_rate;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(xpos, ypos, datwidth, (!strcmp(rotation_rate, "") ? g_Locale->getText(LOCALE_HDD_INFO_UNKNOWN) : buf.str().c_str()), COL_MENUCONTENT_TEXT);

	ypos += mheight;
	buf.str("");
	buf << sata_version;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(xpos, ypos, datwidth, (!strcmp(sata_version, "") ? g_Locale->getText(LOCALE_HDD_INFO_UNKNOWN) : buf.str().c_str()), COL_MENUCONTENT_TEXT);

	ypos += mheight;
	buf.str("");
	buf <<  temperature;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(xpos, ypos, datwidth, (!strcmp(temperature, "") ? g_Locale->getText(LOCALE_HDD_INFO_UNKNOWN) : buf.str().c_str()), COL_MENUCONTENT_TEXT);
}

void CHDDInfoWidget::hide()
{
	frameBuffer->paintBackgroundBoxRel(x, y, width, height);
}
