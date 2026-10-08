/*
	(C)2012-2013 by martii

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

#include <gui/psisetup.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>

#include <gui/color.h>

#include <gui/widget/msgbox.h>
#include <gui/widget/icons.h>
#include <driver/screen_max.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <global.h>
#include <neutrino.h>

#include <coreapi/base/apply.h>
#include <coreapi/base/schema.h>
#include <coreapi/settings/menuspec.h>
#include <coreapi/settings/settings.h>

#include <system/debug.h>

struct PSI_list
{
	const char *key;
	const neutrino_locale_t loc;
	bool selected;
	CProgressBar *scale;
	int x;
	int y;
	int xLoc;
	int yLoc;
	int xBox;
	int yBox;
};

#define PSI_SCALE_COUNT 5
static PSI_list
	psi_list[PSI_SCALE_COUNT] = {
	{ "video_psi_contrast", LOCALE_VIDEOMENU_PSI_CONTRAST, true, NULL, 0, 0, 0, 0, 0, 0 }
	, { "video_psi_saturation", LOCALE_VIDEOMENU_PSI_SATURATION, false, NULL, 0, 0, 0, 0, 0, 0 }
	, { "video_psi_brightness", LOCALE_VIDEOMENU_PSI_BRIGHTNESS, false, NULL, 0, 0, 0, 0, 0, 0 }
	, { "video_psi_tint", LOCALE_VIDEOMENU_PSI_TINT, false, NULL, 0, 0, 0, 0, 0, 0 }
#define PSI_RESET 4
	, { NULL, LOCALE_VIDEOMENU_PSI_RESET, false, NULL, 0, 0, 0, 0, 0, 0 }
};

#define SLIDERWIDTH CFrameBuffer::getInstance()->scale2Res(200)
#define SLIDERHEIGHT CFrameBuffer::getInstance()->scale2Res(15)
#define LOCGAP OFFSET_INNER_MID

CPSISetup::CPSISetup (const neutrino_locale_t Name)
{
	frameBuffer = CFrameBuffer::getInstance ();
	name = Name;
	selected = 0;

	for (int i = 0; i < PSI_RESET; i++)
	{
		psi_list[i].scale = new CProgressBar();
		value[i] = NULL;
		kept[i] = 0;
		lowest[i] = 0;
		highest[i] = 255;
		fallback[i] = 128;
	}

	needsBlit = true;
}

/* Each slider moves its row's own member and the picture group takes it from
   there, so the preview is what any other change of the row does, and a web
   write made while the screen is open is what the slider shows and moves on
   from. */
void CPSISetup::bindRows()
{
	for (int i = 0; i < PSI_RESET; i++)
	{
		coreapi::Result<coreapi::MenuItemSpec> r = coreapi::menuItem(psi_list[i].key);
		value[i] = (r.ok() && r.value().int_pointer != NULL) ? r.value().int_pointer(g_settings) : NULL;
		if (value[i] == NULL)
		{
			dprintf(DEBUG_NORMAL, "[CPSISetup] %s: no value to move\n", psi_list[i].key);
			continue;
		}
		lowest[i] = (int) r.value().min;
		highest[i] = (int) r.value().max;
		kept[i] = *value[i];
		coreapi::Result<coreapi::Descriptor> d = coreapi::settings::describe(psi_list[i].key);
		if (d.ok())
			fallback[i] = (int) coreapi::defaultInt(d.value());
	}
}

// One key is enough: the group sets all four from the rows.
void CPSISetup::applyRows()
{
	const coreapi::Status s = coreapi::applyKey(psi_list[0].key);
	if (s != coreapi::Status::Ok && s != coreapi::Status::Busy)
		dprintf(DEBUG_NORMAL, "[CPSISetup] apply failed\n");
}

int CPSISetup::exec (CMenuTarget * parent, const std::string &)
{
	neutrino_msg_t msg;
	neutrino_msg_data_t data;

	locWidth = 0;
	for (int i = 0; i < PSI_SCALE_COUNT; i++)
	{
		int w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth (g_Locale->getText(psi_list[i].loc)) + OFFSET_INNER_SMALL;
		if (w > locWidth)
			locWidth = w;
	}
	locHeight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight ();
	if (locHeight < SLIDERHEIGHT)
		locHeight = SLIDERHEIGHT + OFFSET_INNER_SMALL;

	sliderOffset = (locHeight - SLIDERHEIGHT) >> 1;

	//            [ SLIDERWIDTH ][5][locwidth    ]
	// [locHeight][XXXXXXXXXXXXX]   [XXXXXXXXXXXX]
	// [locHeight][XXXXXXXXXXXXX]   [XXXXXXXXXXXX]
	// [locHeight][XXXXXXXXXXXXX]   [XXXXXXXXXXXX]
	// [locHeight][XXXXXXXXXXXXX]   [XXXXXXXXXXXX]
	// [locHeight]                  [XXXXXXXXXXXX]

	dx = SLIDERWIDTH + LOCGAP + locWidth;
	dy = PSI_SCALE_COUNT * locHeight + (PSI_SCALE_COUNT - 1) * 2;

	x = frameBuffer->getScreenX() + ((frameBuffer->getScreenWidth() - dx) >> 1);
	y = frameBuffer->getScreenY() + ((frameBuffer->getScreenHeight() - dy) >> 1);

	int res = menu_return::RETURN_REPAINT;
	if (parent)
		parent->hide ();

	for (int i = 0; i < PSI_SCALE_COUNT; i++)
	{
		psi_list[i].x = x;
		psi_list[i].y = y + locHeight * i + i * 2;
		psi_list[i].xBox = psi_list[i].x + SLIDERWIDTH + LOCGAP;
		psi_list[i].yBox = psi_list[i].y;
		psi_list[i].xLoc = psi_list[i].x + SLIDERWIDTH + LOCGAP + OFFSET_INNER_SMALL;
		psi_list[i].yLoc = psi_list[i].y + locHeight - 1;
	}

	for (int i = 0; i < PSI_RESET; i++)
		psi_list[i].scale->reset ();

	bindRows();

	paint();

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] ? g_settings.timing[SNeutrinoSettings::TIMING_MENU] : 0xffff);
	bool loop = true;
	while (loop)
	{
		if(needsBlit) {
			frameBuffer->blit();
			needsBlit = false;
	}
	g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true);
	if ( msg <= CRCInput::RC_MaxRC )
		timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] ? g_settings.timing[SNeutrinoSettings::TIMING_MENU] : 0xffff);
	int i;
	int direction = 1; // down
	switch (msg)
	{
		case CRCInput::RC_up:
			direction = -1;
			/* fall through */
		case CRCInput::RC_down:
			if (selected + direction > -1 && selected + direction < PSI_RESET)
			{
				psi_list[selected].selected = false;
				paintSlider (selected);
				selected += direction;
				psi_list[selected].selected = true;
				paintSlider (selected);
			}
			break;
		case CRCInput::RC_right:
			if (selected < PSI_RESET && value[selected] != NULL && *value[selected] < highest[selected])
			{
				int val = *value[selected] + g_settings.psi_step;
				*value[selected] = (val > highest[selected]) ? highest[selected] : val;
				paintSlider (selected);
				applyRows();
			}
			break;
		case CRCInput::RC_left:
			if (selected < PSI_RESET && value[selected] != NULL && *value[selected] > lowest[selected])
			{
				int val = *value[selected] - g_settings.psi_step;
				*value[selected] = (val < lowest[selected]) ? lowest[selected] : val;
				paintSlider (selected);
				applyRows();
			}
			break;
		case CRCInput::RC_back:
		case CRCInput::RC_home:	// exit -> revert changes
			if (ShowMsg(name, LOCALE_MESSAGEBOX_ACCEPT, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbCancel) == CMsgBox::mbrCancel)
			{
				for (i = 0; i < PSI_RESET; i++)
					if (value[i] != NULL)
						*value[i] = kept[i];
				applyRows();
			}
			/* fall through */
		case CRCInput::RC_ok:
			if (selected != PSI_RESET)
			{
				loop = false;
				break;
			}
			/* fall through */
		case CRCInput::RC_red:
			for (i = 0; i < PSI_RESET; i++)
			{
				if (value[i] != NULL)
					*value[i] = fallback[i];
				paintSlider (i);
			}
			applyRows();
			break;
		default:
			;
		}
	}

	hide ();

	return res;
}

void CPSISetup::hide ()
{
	frameBuffer->paintBackgroundBoxRel (x, y, dx, dy);
	frameBuffer->blit();
}

void CPSISetup::paint ()
{
	for (int i = 0; i < PSI_SCALE_COUNT; i++)
		paintSlider (i);
}

void CPSISetup::paintSlider (int i)
{
	Font *f = g_Font[SNeutrinoSettings::FONT_TYPE_MENU];
	fb_pixel_t fg_col[] = { COL_MENUCONTENTINACTIVE_TEXT, COL_MENUCONTENT_TEXT };

	if (i < PSI_RESET)
	{
		psi_list[i].scale->setProgress(psi_list[i].x, psi_list[i].y + sliderOffset, SLIDERWIDTH, SLIDERHEIGHT,
					       value[i] != NULL ? *value[i] : 0, highest[i]);
		psi_list[i].scale->paint();
		f->RenderString (psi_list[i].xLoc, psi_list[i].yLoc, locWidth, g_Locale->getText(psi_list[i].loc), fg_col[psi_list[i].selected]);
	}
	else
	{
		int fh = f->getHeight();
		frameBuffer->paintIcon (NEUTRINO_ICON_BUTTON_RED, psi_list[i].x, psi_list[i].yLoc - fh + fh/4, 0, (6 * fh)/8);
		f->RenderString (psi_list[i].xLoc, psi_list[i].yLoc, locWidth, g_Locale->getText(psi_list[i].loc), COL_MENUCONTENT_TEXT);
	}
	needsBlit = true;
}

static CPSISetup *inst = NULL;

CPSISetup *CPSISetup::getInstance()
{
	if (!inst)
		inst = new CPSISetup(LOCALE_VIDEOMENU_PSI);
	return inst;
}
