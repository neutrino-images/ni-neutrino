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
			else if (CNeutrinoApp::getInstance()->listModeKey(msg))
			{
				// do nothing
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

typedef struct items_data_t
{
	neutrino_locale_t locale;
	char value[128];
}
items_data_struct;

items_data_t items_data[] =
{
	{ LOCALE_HDD_INFO_MODEL_FAMILY	, "" }, // 0
	{ LOCALE_HDD_INFO_MODEL		, "" }, // 1
	{ LOCALE_HDD_INFO_SERIAL	, "" }, // 2
	{ LOCALE_HDD_INFO_FIRMWARE	, "" }, // 3
	{ LOCALE_HDD_INFO_CAPACITY	, "" }, // 4
	{ LOCALE_HDD_INFO_SECTOR_SIZE	, "" }, // 5
	{ LOCALE_HDD_INFO_ROTATION_RATE	, "" }, // 6
	{ LOCALE_HDD_INFO_SATA_VERSION	, "" }, // 7
	{ LOCALE_HDD_INFO_TEMPERATURE	, "" }  // 8
};
#define items_count (sizeof(items_data)/sizeof(struct items_data_t))

void CHDDInfoWidget::paint(const std::string &Key)
{
	frameBuffer = CFrameBuffer::getInstance();

	CComponentsHeader header;
	CComponentsFooter footer;

	Font *item_font = g_Font[SNeutrinoSettings::FONT_TYPE_MENU];

	int header_height = header.getHeight();
	int item_height = item_font->getHeight();
	int body_height = items_count*item_height + 2*OFFSET_INNER_SMALL;
	int footer_height = header.getHeight();

	width	= frameBuffer->getScreenWidth()/2;
	height	= header_height + body_height + footer_height;

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

	buf << "smartctl --all /dev/" << Key;

	buffer = NULL;

	if((pipe_reader = popen(buf.str().c_str(), "r")))
	{
		while ((read = getline(&buffer, &len, pipe_reader)) != -1)
		{
			if ((found = strstr(buffer, "Model Family:")))
				sscanf(found+18, "%127[^\n]", (char *) &items_data[0].value);
			else if ((found = strstr(buffer, "Device Model:")))
				sscanf(found+18, "%127[^\n]", (char *) &items_data[1].value);
			else if ((found = strstr(buffer, "Serial Number:")))
				sscanf(found+18, "%127[^\n]", (char *) &items_data[2].value);
			else if ((found = strstr(buffer, "Firmware Version:")))
				sscanf(found+18, "%127[^\n]", (char *) &items_data[3].value);
			else if ((found = strstr(buffer, "User Capacity:")))
				sscanf(found+18, "%127[^\n]", (char *) &items_data[4].value);
			else if ((found = strstr(buffer, "Sector Size:")))
				sscanf(found+18, "%127[^\n]", (char *) &items_data[5].value);
			else if ((found = strstr(buffer, "Rotation Rate:")))
				sscanf(found+18, "%127[^\n]", (char *) &items_data[6].value);
			else if ((found = strstr(buffer, "SATA Version is:")))
				sscanf(found+18, "%127[^\n]", (char *) &items_data[7].value);
			else if ((found = strstr(buffer, "Temperature_Celsius")))
				sscanf(found+83, "%3[^\n]", (char *) &items_data[8].value);
		}
		pclose(pipe_reader);
	}
	else
		printf("[read_smartctl] popen error\n" );

	if(buffer)
		free(buffer);

	// manipulating capacity
	str_capacity = items_data[4].value;
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
	snprintf(items_data[4].value, sizeof(items_data[4].value), "%s", buf.str().c_str());

	// manipulating temperature
	buf.str("");
	if (strcmp(items_data[8].value, "") != 0)
	{
		buf << trim(items_data[8].value) << " Grad Celsius";
		snprintf(items_data[8].value, sizeof(items_data[8].value), "%s", buf.str().c_str());
	}

	// calculate max width of locales
	int w_loc = 0, w_tmp = 0;
	for (unsigned int i = 0; i < items_count; i++)
	{
		w_tmp = item_font->getRenderWidth(g_Locale->getText(items_data[i].locale));
		w_loc = std::max(w_loc, w_tmp);
	}

	// calculate width of separator
	std::string separator = ":";
	int w_sep = item_font->getRenderWidth(separator);

	// calculate max width of values
	int w_val = 0;
	for (unsigned int i = 0; i < items_count; i++)
	{
		if (strcmp(items_data[i].value, "") == 0)
			snprintf(items_data[i].value, sizeof(items_data[i].value), "%s", g_Locale->getText(LOCALE_HDD_INFO_UNKNOWN));
		w_tmp = item_font->getRenderWidth(trim(items_data[i].value));
		w_val = std::max(w_val, w_tmp);
	}

	// recalculate width and xpos
	width = std::max(width, w_loc + w_sep + w_val + 4*OFFSET_INNER_MID);
	if (width > (int) frameBuffer->getScreenWidth())
	{
		// should only happen with very big fonts
		width = frameBuffer->getScreenWidth();
		w_val = width - w_loc - w_sep - 4*OFFSET_INNER_MID;
	}
	x = getScreenStartX(width);

	// header
	buf.str("");
	buf << g_Locale->getText(LOCALE_HDD_INFO_HEAD) << " (" << Key << ")";
	header.setCaption(buf.str());
	header.setIcon(NEUTRINO_ICON_SETTINGS);
	header.setDimensionsAll(x, y, width, header_height);
	header.addContextButton(CComponentsHeader::CC_BTN_EXIT);
	header.enableShadow(CC_SHADOW_RIGHT | CC_SHADOW_CORNER_TOP_RIGHT | CC_SHADOW_CORNER_BOTTOM_RIGHT, -1, true);
	header.paint(CC_SAVE_SCREEN_NO);

	// body
	PaintBoxRel(x, y + header_height, width, body_height, COL_MENUCONTENT_PLUS_0, RADIUS_NONE, CORNER_NONE, CC_SHADOW_ON);

	// footer
	const struct button_label buttons[] =
	{
		{ "", LOCALE_HDD_INFO_INFO }
	};
	footer.enableShadow(CC_SHADOW_ON, -1, true);
	footer.paintButtons(x, y + header_height + body_height, width, footer_height, 1, buttons);

	// paint items
	int x_loc = x + OFFSET_INNER_MID;
	int x_sep = x_loc + w_loc + OFFSET_INNER_MID;
	int x_val = x_sep + w_sep + OFFSET_INNER_MID;
	int y_item = y + header_height + OFFSET_INNER_SMALL;

	for (unsigned int i = 0; i < items_count; i++)
	{
		y_item += item_height;
		item_font->RenderString(x_loc, y_item, w_loc, g_Locale->getText(items_data[i].locale), COL_MENUCONTENTINACTIVE_TEXT);
		item_font->RenderString(x_sep, y_item, w_sep, separator, COL_MENUCONTENTINACTIVE_TEXT);
		item_font->RenderString(x_val, y_item, w_val, items_data[i].value, COL_MENUCONTENT_TEXT);
	}
}

void CHDDInfoWidget::hide()
{
	frameBuffer->paintBackgroundBoxRel(x, y, width + OFFSET_SHADOW, height + OFFSET_SHADOW);
}
