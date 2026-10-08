/*
	$port: video_setup.h,v 1.4 2009/11/22 15:36:52 tuxbox-cvs Exp $

	video setup implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2009 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

	Copyright (C) 2010-2012 Stefan Seyfried

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


#include "videosettings.h"

#include <global.h>
#include <neutrino.h>

#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/settingitem.h>
#include <gui/osd_setup.h>
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
#include <gui/psisetup.h>
#endif

#include <driver/display.h>
#include <driver/screen_max.h>
#include <driver/display.h>

#include <daemonc/remotecontrol.h>

#include <system/debug.h>
#include <system/helpers.h>

#include <coreapi/base/apply.h>
#include <coreapi/base/deps.h>
#include <coreapi/box/apply_video.h>
#include <coreapi/settings/menuspec.h>
#include <coreapi/settings/predicates.h>
#include <coreapi/settings/videomodes.h>

#include <gui/widget/settingactive.h>

#include <cstdio>
#include <cstring>

#if ENABLE_PIP
#include <gui/pipsetup.h>
#endif
#if ENABLE_QUADPIP
#include <gui/quadpip_setup.h>
#endif
extern CRemoteControl *g_RemoteControl; /* neutrino.cpp */

// Busy is a group whose startup phase is not reached, which the phase makes good.
static void applyRow(const std::string &key)
{
	const coreapi::Status s = coreapi::applyKey(key);
	if (s != coreapi::Status::Ok && s != coreapi::Status::Busy)
		dprintf(DEBUG_NORMAL, "[CVideoSettings] %s: apply failed\n", key.c_str());
}

/* The enabled modes are listed under their names, which no locale holds, so
   their items are built here and not by addSetting; only their apply goes the
   way of the rows. */
class CVideoModeSlotApply : public CChangeObserver
{
	private:
		std::string key;

	public:
		explicit CVideoModeSlotApply(const std::string &k) : key(k) {}
		bool changeNotify(const neutrino_locale_t, void *)
		{
			applyRow(key);
			return false;
		}
		bool changeNotify(const std::string &, void *)
		{
			applyRow(key);
			return false;
		}
};

CVideoSettings::CVideoSettings(int wizard_mode)
{
	frameBuffer = CFrameBuffer::getInstance();

	is_wizard = wizard_mode;

	SyncControlerForwarder = NULL;

	width = 35;
	selected = -1;
}

CVideoSettings::~CVideoSettings()
{
}

int CVideoSettings::exec(CMenuTarget *parent, const std::string &/*actionKey*/)
{
	dprintf(DEBUG_NORMAL, "[CVideoSettings] [%s - %d], init video setup (Mode: %d)...\n", __func__, __LINE__, is_wizard);
	int res = menu_return::RETURN_REPAINT;

	if (parent)
	{
		parent->hide();
	}

	res = showVideoSetup();

	return res;
}

/* The video modes by the number the settings file gives each, the index of
   enabled_video_modes and enabled_auto_modes, with the value the video_Mode
   row gives the mode on this box, or -1 where this box does not draw it.
   Filled once out of the declaration. */
const CMenuOptionChooser::keyval_ext *videoModeSlots()
{
	static CMenuOptionChooser::keyval_ext slots[VIDEOMENU_VIDEOMODE_OPTION_COUNT];
	static bool filled = false;
	if (filled)
		return slots;

	size_t count = 0;
	const char *const *names = coreapi::videoModeNames(count);
	for (int i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++)
	{
		slots[i].key = -1;
		slots[i].value = NONEXISTANT_LOCALE;
		slots[i].valname = (size_t) i < count ? names[i] : "";
	}

	coreapi::Result<coreapi::MenuItemSpec> r = coreapi::menuItem("video_Mode");
	if (!r.ok())
	{
		// Tried again on the next call.
		dprintf(DEBUG_NORMAL, "[CVideoSettings] [%s - %d], no video modes: %s\n", __func__, __LINE__, r.error().message.c_str());
		return slots;
	}
	const std::vector<coreapi::MenuChoice> &offered = r.value().choices;
	for (size_t j = 0; j < offered.size(); j++)
		for (size_t i = 0; i < count && i < (size_t) VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++)
			if (offered[j].label_text == names[i])
				slots[i].key = (int) offered[j].value;
	filled = true;
	return slots;
}

int CVideoSettings::showVideoSetup()
{
	// init
	CMenuWidget *videosetup = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width);
	videosetup->setSelected(selected);
	videosetup->setWizardMode(is_wizard);

	const CMenuOptionChooser::keyval_ext *vmodes = videoModeSlots();

	CMenuWidget videomodes(LOCALE_MAINSETTINGS_VIDEO, NEUTRINO_ICON_SETTINGS);
#ifdef BOXMODEL_CST_HD2
	CMenuForwarder *vs_automodes_fw = NULL;
	CMenuWidget automodes(LOCALE_MAINSETTINGS_VIDEO, NEUTRINO_ICON_SETTINGS);
#endif
	CMenuForwarder *vs_videomodes_fw = NULL;
	// One per item, alive as long as the submenus that hold them.
	std::vector<CVideoModeSlotApply> slot_apply;
	slot_apply.reserve(2 * VIDEOMENU_VIDEOMODE_OPTION_COUNT);
	char slot_key[32];

	// video system modes submenue
	if (coreapi::hasHdmi()) // does this make sense on a box without HDMI?
	{
		videomodes.addIntroItems(LOCALE_VIDEOMENU_ENABLED_MODES);

		for (int i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++)
			if (vmodes[i].key != -1)
			{
				snprintf(slot_key, sizeof(slot_key), "enabled_video_mode_%d", i);
				slot_apply.push_back(CVideoModeSlotApply(slot_key));
				videomodes.addItem(new CMenuOptionChooser(vmodes[i].valname, &g_settings.enabled_video_modes[i], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, &slot_apply.back()));
			}

		if (coreapi::hasFormatButton())
		{
			vs_videomodes_fw = new CMenuForwarder(LOCALE_VIDEOMENU_ENABLED_MODES, true, NULL, &videomodes, NULL, CRCInput::RC_red);
			vs_videomodes_fw->setHint("", LOCALE_MENU_HINT_VIDEO_MODES);
		}

#ifdef BOXMODEL_CST_HD2
		automodes.addIntroItems(LOCALE_VIDEOMENU_ENABLED_MODES_AUTO);

		for (int i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT - 1; i++)
		{
			snprintf(slot_key, sizeof(slot_key), "enabled_auto_mode_%d", i);
			slot_apply.push_back(CVideoModeSlotApply(slot_key));
			automodes.addItem(new CMenuOptionChooser(vmodes[i].valname, &g_settings.enabled_auto_modes[i], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, &slot_apply.back()));
		}

		vs_automodes_fw = new CMenuForwarder(LOCALE_VIDEOMENU_ENABLED_MODES_AUTO, true, NULL, &automodes, NULL, CRCInput::RC_green);
		vs_automodes_fw->setHint("", LOCALE_MENU_HINT_VIDEO_MODES_AUTO);
#endif
	}

	neutrino_locale_t tmp_locale = NONEXISTANT_LOCALE;
	// TODO: check the locale
	if (coreapi::menuItem("analog_mode1").ok() || coreapi::menuItem("analog_mode2").ok())
		tmp_locale = LOCALE_VIDEOMENU_TV_SCART;
	// ---------------------------------------
	videosetup->addIntroItems(LOCALE_MAINSETTINGS_VIDEO, tmp_locale);
	// ---------------------------------------
	//videosetup->addItem(vs_scart_sep); // separator scart
	addSetting(videosetup, "analog_mode1"); // analog option or scart
	addSetting(videosetup, "analog_mode2"); // chinch
	//if (tmp_locale != NONEXISTANT_LOCALE)
	//	videosetup->addItem(GenericMenuSeparatorLine);
	// ---------------------------------------
	addSetting(videosetup, "video_43mode"); // 4:3 mode
	addSetting(videosetup, "video_Format"); // display format
	// video system, asked about once it is on the screen, and only for the one picked
	CMenuOptionChooser *video_mode = addChoiceSetting(videosetup, "video_Mode", true, NULL, CRCInput::RC_nokey, true);
	if (video_mode != NULL)
	{
		afterApply(video_mode, [this]() { return confirmVideoMode(); });
		openListOnStep(video_mode);
	}
	addSetting(videosetup, "video_dbdr");
	if (vs_videomodes_fw != NULL)
		videosetup->addItem(vs_videomodes_fw); // video modes submenue
#ifdef BOXMODEL_CST_HD2
	videosetup->addItem(vs_automodes_fw); // video auto modes submenue
#endif

	addSetting(videosetup, "brightness");
	addSetting(videosetup, "contrast");
	addSetting(videosetup, "saturation");

	addSetting(videosetup, "enable_sd_osd");
#if ENABLE_PIP
	CPipSetup pip;
	CMenuForwarder *pipsetup = new CMenuForwarder(LOCALE_VIDEOMENU_PIP, coreapi::pipUsable(), NULL, &pip);
	pipsetup->setHint("", LOCALE_MENU_HINT_VIDEO_PIP);
	videosetup->addItem(pipsetup);
#endif

#if ENABLE_QUADPIP
	CMenuForwarder *quadpip = new CMenuForwarder(LOCALE_QUADPIP, coreapi::pipWindows() >= 1, NULL, new CQuadPiPSetup());
	quadpip->setHint(NEUTRINO_ICON_HINT_QUADPIP, LOCALE_MENU_HINT_QUADPIP);
	videosetup->addItem(quadpip);
#endif

	addSetting(videosetup, "zappingmode");
	addSetting(videosetup, "hdmi_colorimetry");

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	videosetup->addItem(GenericMenuSeparatorLine);

	CPSISetup *psiSetup = CPSISetup::getInstance();

#if 0
	CMenuOptionNumberChooser *mc;

	mc = new CMenuOptionNumberChooser(LOCALE_VIDEOMENU_PSI_STEP, (int *)&g_settings.psi_step, true, 1, 100, NULL);
	mc->setHint("", LOCALE_MENU_HINT_VIDEO_PSI_STEP);
	videosetup->addItem(mc);
#endif

	CMenuForwarder *mf = new CMenuForwarder(LOCALE_VIDEOMENU_PSI, true, NULL, psiSetup, NULL);
	mf->setHint("", LOCALE_MENU_HINT_VIDEO_PSI);
	videosetup->addItem(mf);

#if 0
	videosetup->addItem(GenericMenuSeparator);

	mc = new CMenuOptionNumberChooser(LOCALE_VIDEOMENU_PSI_CONTRAST, (int *)&g_settings.psi_contrast, true, 0, 255, psiSetup);
	mc->setHint("", LOCALE_MENU_HINT_VIDEO_CONTRAST);
	videosetup->addItem(mc);

	mc = new CMenuOptionNumberChooser(LOCALE_VIDEOMENU_PSI_SATURATION, (int *)&g_settings.psi_saturation, true, 0, 255, psiSetup);
	mc->setHint("", LOCALE_MENU_HINT_VIDEO_SATURATION);
	videosetup->addItem(mc);

	mc = new CMenuOptionNumberChooser(LOCALE_VIDEOMENU_PSI_BRIGHTNESS, (int *)&g_settings.psi_brightness, true, 0, 255, psiSetup);
	mc->setHint("", LOCALE_MENU_HINT_VIDEO_BRIGHTNESS);
	videosetup->addItem(mc);

	mc = new CMenuOptionNumberChooser(LOCALE_VIDEOMENU_PSI_TINT, (int *)&g_settings.psi_tint, true, 0, 255, psiSetup);
	mc->setHint("", LOCALE_MENU_HINT_VIDEO_TINT);
	videosetup->addItem(mc);
#endif
#endif

	int res = videosetup->exec(NULL, "");
	selected = videosetup->getSelected();
	delete videosetup;
	return res;
}

/* After the video group has put the new mode on the screen: somebody has to see
   it to keep it, and a no or no answer at all puts back the mode the box had
   right before, which is one written from elsewhere if that came in between.
   The menu is drawn again whatever happened, as the screen always did after a
   change of the mode. */
bool CVideoSettings::confirmVideoMode()
{
	int kept = coreapi::videoModeBeforeLastChange();
	if (kept < 0)
		return true;
	keepOrRestore(g_settings.video_Mode, kept, "video_Mode", askKeepVideoMode, applyRow);
	return true;
}

bool askKeepVideoMode()
{
	CFrameBuffer::getInstance()->paintBackground();
	return ShowMsg(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_VIDEO_MODE_OK), CMsgBox::mbrNo,
		       CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_INFO) == CMsgBox::mbrYes;
}

/* The value a key press steps a declared setting on to, among the ones this box
   offers, and the words it shows. False where the row offers none. */
static bool nextOffered(const char *key, int current, int &value, neutrino_locale_t &text)
{
	coreapi::Result<coreapi::MenuItemSpec> r = coreapi::menuItem(key);
	if (!r.ok() || r.value().choices.empty())
		return false;
	const std::vector<coreapi::MenuChoice> &offered = r.value().choices;
	size_t at = 0;
	for (size_t i = 0; i < offered.size(); i++)
	{
		if (offered[i].value == current)
		{
			at = i;
			break;
		}
	}
	at++;
	if (at >= offered.size())
		at = 0;
	value = (int) offered[at].value;
	text = localeFromKey(offered[at].label_key);
	return true;
}

void CVideoSettings::next43Mode(void)
{
	dprintf(DEBUG_NORMAL, "[CVideoSettings] [%s - %d], setting 4:3 mode...", __func__, __LINE__);
	neutrino_locale_t text;
	int mode;
	if (!nextOffered("video_43mode", g_settings.video_43mode, mode, text))
		return;

	g_settings.video_43mode = mode;
	applyRow("video_43mode");
	ShowHint(LOCALE_VIDEOMENU_43MODE, g_Locale->getText(text), 450, 2);
}

void CVideoSettings::SwitchFormat()
{
	dprintf(DEBUG_NORMAL, "[CVideoSettings] [%s - %d], setting video format...\n", __func__, __LINE__);
	neutrino_locale_t text;
	int format;
	if (!nextOffered("video_Format", g_settings.video_Format, format, text))
		return;

	g_settings.video_Format = format;
	applyRow("video_Format");
	ShowHint(LOCALE_VIDEOMENU_VIDEOFORMAT, g_Locale->getText(text), 450, 2);
}

void CVideoSettings::nextMode(void)
{
	dprintf(DEBUG_NORMAL, "[CVideoSettings] [%s - %d], setting video mode...\n", __func__, __LINE__);
	const CMenuOptionChooser::keyval_ext *vmodes = videoModeSlots();
	const char *text;
	int curmode = 0;
	int i;
	bool disp_cur = 1;
	int res = messages_return::none;

	for (i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++)
	{
		if (vmodes[i].key == g_settings.video_Mode)
		{
			curmode = i;
			break;
		}
	}
	text = vmodes[curmode].valname;

	while (1)
	{
		CVFD::getInstance()->ShowText(text);

		if (res != messages_return::cancel_info) // avoid unnecessary display of messageboxes, when user is trying to press repeated format button
			res = ShowHint(LOCALE_VIDEOMENU_VIDEOMODE, text, 450, 2);

		if (disp_cur && res != messages_return::handled)
			break;

		disp_cur = 0;

		if (res == messages_return::handled)
		{
			i = 0;
			while (true)
			{
				curmode++;
				if (curmode >= VIDEOMENU_VIDEOMODE_OPTION_COUNT)
					curmode = 0;
				if (vmodes[curmode].key == -1)
					continue;
				if (g_settings.enabled_video_modes[curmode])
					break;
				i++;
				if (i >= VIDEOMENU_VIDEOMODE_OPTION_COUNT)
				{
					CVFD::getInstance()->showServicename(g_RemoteControl->getCurrentChannelName(), g_RemoteControl->getCurrentChannelNumber());
					return;
				}
			}

			text = vmodes[curmode].valname;
		}
		else if (res == messages_return::cancel_info)
		{
			g_settings.video_Mode = vmodes[curmode].key;
			//CVFD::getInstance()->ShowText(text);
			applyRow("video_Mode");
			//return;
			disp_cur = 1;
		}
		else
			break;
	}
	CVFD::getInstance()->showServicename(g_RemoteControl->getCurrentChannelName(), g_RemoteControl->getCurrentChannelNumber());
	//ShowHint(LOCALE_VIDEOMENU_VIDEOMODE, text, 450, 2);
}

