/*
 * apply_glcd.cpp - what makes a changed graphic display setting take effect
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

#include "coreapi/box/apply_glcd.h"
#include "coreapi/box/sentstate.h"

#include <system/settings.h>

#include <atomic>
#include <string>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

class NoGlcdPanel : public GlcdPanel
{
	public:
		Status setEnabled(bool) { return Status::NotSupported; }
		Status setMirrorOsd(bool) { return Status::NotSupported; }
		Status respawn() { return Status::NotSupported; }
		Status reinitFont() { return Status::NotSupported; }
		Status updateBrightness() { return Status::NotSupported; }
		Status update() { return Status::NotSupported; }
		Status panelSize(int &, int &) { return Status::NotSupported; }
		Status recordPanelSize() { return Status::NotSupported; }
};

NoGlcdPanel g_no_glcd_panel;
GlcdPanel *g_glcd_panel = 0;

/* What the service was last told. Making it again, turning it off and on and
   loading a font are not shown to be harmless when repeated, so a sibling key
   must not repeat them. */
struct GlcdState
{
	Sent<int> enable;
	Sent<int> mirror;
	Sent<int> config;
	Sent<std::string> font;
	bool started;
	GlcdState() : started(false) {}
};

GlcdState g_state;

// Written by the loop, read by any thread; nought is unknown.
std::atomic<int> g_panel_width(0);
std::atomic<int> g_panel_height(0);

Status runGlcd()
{
	GlcdSnapshot now;
#ifdef ENABLE_GRAPHLCD
	now.enable = g_settings.glcd_enable;
	now.mirror = g_settings.glcd_mirror_osd;
	now.config = g_settings.glcd_selected_config;
	now.font = settingsText(g_settings.glcd_theme.glcd_font);
#endif
	return applyGlcdSnapshot(now);
}

const char *const kGlcdKeys[] =
{
	"glcd_background_image",
	"glcd_font",
	"glcd_channel_percent",
	"glcd_channel_align",
	"glcd_channel_x_position",
	"glcd_channel_y_position",
	"glcd_logo",
	"glcd_logo_percent",
	"glcd_logo_width_percent",
	"glcd_logo_x_position",
	"glcd_logo_y_position",
	"glcd_epg_percent",
	"glcd_epg_align",
	"glcd_epg_x_position",
	"glcd_epg_y_position",
	"glcd_start",
	"glcd_start_percent",
	"glcd_start_align",
	"glcd_start_x_position",
	"glcd_start_y_position",
	"glcd_end",
	"glcd_end_percent",
	"glcd_end_align",
	"glcd_end_x_position",
	"glcd_end_y_position",
	"glcd_duration",
	"glcd_duration_percent",
	"glcd_duration_align",
	"glcd_duration_x_position",
	"glcd_duration_y_position",
	"glcd_progressbar",
	"glcd_progressbar_percent",
	"glcd_progressbar_width",
	"glcd_progressbar_x_position",
	"glcd_progressbar_y_position",
	"glcd_time",
	"glcd_time_percent",
	"glcd_time_align",
	"glcd_time_x_position",
	"glcd_time_y_position",
	"glcd_icons_percent",
	"glcd_icons_y_position",
	"glcd_icon_ecm_x_position",
	"glcd_icon_cam_x_position",
	"glcd_icon_txt_x_position",
	"glcd_icon_dd_x_position",
	"glcd_icon_mute_x_position",
	"glcd_icon_timer_x_position",
	"glcd_icon_rec_x_position",
	"glcd_icon_ts_x_position",
	"glcd_standby_clock",
	"glcd_standby_clock_digital_y_position",
	"glcd_standby_clock_simple_size",
	"glcd_standby_clock_simple_y_position",
	"glcd_weather",
	"glcd_weather_percent",
	"glcd_weather_curr_temp_x_position",
	"glcd_weather_curr_icon_x_position",
	"glcd_weather_next_temp_x_position",
	"glcd_weather_next_icon_x_position",
	"glcd_weather_y_position",
	"glcd_standby_weather",
	"glcd_standby_weather_percent",
	"glcd_standby_weather_curr_temp_x_position",
	"glcd_standby_weather_curr_icon_x_position",
	"glcd_standby_weather_next_temp_x_position",
	"glcd_standby_weather_next_icon_x_position",
	"glcd_standby_weather_y_position",
	"glcd_position_settings",
	"glcd_enable",
	"glcd_selected_config",
	"glcd_logodir",
	"glcd_brightness",
	"glcd_brightness_standby",
	"glcd_brightness_dim",
	"glcd_brightness_dim_time",
	"glcd_scroll",
	"glcd_scroll_speed",
	"glcd_mirror_osd",
	"glcd_mirror_video",
	"glcd_theme.glcd_foreground_color",
	"glcd_theme.glcd_background_color",
	"glcd_theme.glcd_progressbar_color"
};

} // namespace

GlcdPanel &glcdPanel()
{
	if (!g_glcd_panel)
		return g_no_glcd_panel;
	return *g_glcd_panel;
}

void setGlcdPanel(GlcdPanel *p) { g_glcd_panel = p; }

namespace
{

long boundOf(bool width)
{
	int w = 0, h = 0;
	const long most = width ? kGlcdPanelWidthMax : kGlcdPanelHeightMax;
	if (glcdPanel().panelSize(w, h) != Status::Ok)
		return most;
	const long got = width ? w : h;
	return (got > 0 && got <= most) ? got : most;
}

} // namespace

long glcdPanelWidth(const ValueLookup *) { return boundOf(true); }
long glcdPanelHeight(const ValueLookup *) { return boundOf(false); }

void noteGlcdPanelSize(int width, int height)
{
	g_panel_width = width;
	g_panel_height = height;
}

Status recordedGlcdPanelSize(int &width, int &height)
{
	const int w = g_panel_width;
	const int h = g_panel_height;
	if (w <= 0 || h <= 0)
		return Status::NotSupported;
	width = w;
	height = h;
	return Status::Ok;
}

Status applyGlcdSnapshot(const GlcdSnapshot &now)
{
	GlcdPanel &p = glcdPanel();
	Status first = Status::Ok;
	SentFlags none;

	// The panel may have come up or changed since the last run.
	noteFirst(first, p.recordPanelSize());

	if (!g_state.started)
	{
		// The service is made by the application with these settings.
		g_state.started = true;
		g_state.enable.known = true;
		g_state.enable.value = now.enable;
		g_state.mirror.known = true;
		g_state.mirror.value = now.mirror;
		g_state.config.known = true;
		g_state.config.value = now.config;
		g_state.font.known = true;
		g_state.font.value = now.font;
		return first;
	}

	sendChanged(first, none, 0, g_state.enable, now.enable, [&]() { return p.setEnabled(now.enable != 0); });
	sendChanged(first, none, 0, g_state.mirror, now.mirror, [&]() { return p.setMirrorOsd(now.mirror != 0); });
	sendChanged(first, none, 0, g_state.config, now.config, [&]() { return p.respawn(); });
	sendChanged(first, none, 0, g_state.font, now.font, [&]() { return p.reinitFont(); });
	noteFirst(first, p.updateBrightness());
	noteFirst(first, p.update());
	return first;
}

void resetSentGlcd()
{
	g_state = GlcdState();
	noteGlcdPanelSize(0, 0);
}

/* After the decoders: the service is made right after the panel, before them. */
const ApplyGroup kGlcdApplyGroup = { "glcd", ApplyPhase::Decoders, COREAPI_KEYS(kGlcdKeys), &runGlcd };

} // namespace coreapi
