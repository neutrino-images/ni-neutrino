/*
 * apply_vfd.cpp - what makes a changed front panel setting take effect
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>

#include "coreapi/box/apply_vfd.h"
#include "coreapi/box/sentstate.h"

#include <system/settings.h>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

class NoVfdPanel : public VfdPanel
{
	public:
		Status setBrightness(Brightness, int) { return Status::NotSupported; }
		Status setScrollMode(int) { return Status::NotSupported; }
		Status refreshLeds() { return Status::NotSupported; }
		Status refreshParameters() { return Status::NotSupported; }
		Status setBacklight(int) { return Status::NotSupported; }
		Status showStatusline(int, int) { return Status::NotSupported; }
};

NoVfdPanel g_no_vfd_panel;
VfdPanel *g_vfd_panel = 0;

/* What the panel was last told. The driver does not say that a brightness it is
   given again is nothing, the LEDs and the second line are redrawn, and the
   volume bar of the second line must not appear on a panel that never showed it. */
struct VfdState
{
	Sent<int> normal;
	Sent<int> standby;
	Sent<int> deep;
	Sent<int> scroll;
	Sent<int> led;
	Sent<int> parameters;
	Sent<int> backlight;
	Sent<int> statusline;
	bool started;
	VfdState() : started(false) {}
};

VfdState g_state;
SentFlags g_flags;
const unsigned kBacklightHeld = 1u;

// Remembers a value the panel was brought up with.
void learn(Sent<int> &sent, int v)
{
	sent.known = true;
	sent.value = v;
}

/* Contrast, power and inverse reach the panel in one call, so they are one value to
   compare: a change of any of them sends once and an unrelated key sends nothing. */
int parametersOf()
{
	return g_settings.lcd_setting[SNeutrinoSettings::LCD_CONTRAST] |
	       (g_settings.lcd_setting[SNeutrinoSettings::LCD_POWER] != 0 ? 1 << 16 : 0) |
	       (g_settings.lcd_setting[SNeutrinoSettings::LCD_INVERSE] != 0 ? 1 << 17 : 0);
}

Status runVfd()
{
	VfdPanel &p = vfdPanel();
	Status first = Status::Ok;
	SentFlags none;
	// Only the backlight can be held.

	const int normal = g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS];
	const int standby = g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS];
	const int deep = g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS];
	const int statusline = g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME];

	if (!g_state.started)
	{
		g_state.started = true;
		learn(g_state.normal, normal);
		learn(g_state.standby, standby);
		learn(g_state.deep, deep);
		learn(g_state.led, g_settings.led_tv_mode);
		learn(g_state.parameters, parametersOf());
		learn(g_state.statusline, statusline);
	}

	sendChanged(first, none, 0, g_state.normal, normal, [&]() { return p.setBrightness(VfdPanel::Normal, normal); });
	sendChanged(first, none, 0, g_state.standby, standby, [&]() { return p.setBrightness(VfdPanel::Standby, standby); });
	sendChanged(first, none, 0, g_state.deep, deep, [&]() { return p.setBrightness(VfdPanel::DeepStandby, deep); });

	const int scroll = g_settings.lcd_scroll;
	sendChanged(first, none, 0, g_state.scroll, scroll, [&]() { return p.setScrollMode(scroll); });

	const int led = g_settings.led_tv_mode;
	sendChanged(first, none, 0, g_state.led, led, [&]() { return p.refreshLeds(); });

	const int parameters = parametersOf();
	sendChanged(first, none, 0, g_state.parameters, parameters, [&]() { return p.refreshParameters(); });

#ifndef ENABLE_LCD
	const int backlight = g_settings.backlight_tv;
	sendChanged(first, g_flags, kBacklightHeld, g_state.backlight, backlight, [&]() { return p.setBacklight(backlight); });
#endif

	sendChanged(first, none, 0, g_state.statusline, statusline,
		    [&]() { return p.showStatusline(statusline, g_settings.current_volume); });
	return first;
}

const char *const kVfdKeys[] =
{
	"lcd_brightness",
	"lcd_standbybrightness",
	"lcd_deepbrightness",
	"lcd_scroll",
	"lcd_show_volume",
	"lcd_contrast",
	"lcd_power",
	"lcd_inverse",
	"led_tv_mode",
	"backlight_tv"
};

} // namespace

VfdPanel &vfdPanel()
{
	if (!g_vfd_panel)
		return g_no_vfd_panel;
	return *g_vfd_panel;
}

void setVfdPanel(VfdPanel *p) { g_vfd_panel = p; }

void holdVfdBacklight(bool held) { g_flags.hold(kBacklightHeld, held); }

void resetSentVfd()
{
	g_state = VfdState();
	g_flags.reset();
}

/* After the panel is brought up, which is where the startup used to set the
   scrolling and the backlight. */
const ApplyGroup kVfdApplyGroup = { "vfd", ApplyPhase::Decoders, COREAPI_KEYS(kVfdKeys), &runVfd };

} // namespace coreapi
