/*
 * settingstable_display.cpp - front display settings, one row per field
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

#include "settingstable.h"
#include "settingsfield.h"
#include "predicates.h"

namespace coreapi
{

namespace
{

/* Everything the box shows on a display of its own: the front panel, the
   graphical LCD and the LCD4Linux panel.

   Two thirds of these sit behind a build condition, because that is where the
   settings struct puts the fields for the graphical LCD and for LCD4Linux
   behind them. Neither condition is one check-hardware.sh reads,
   so the rows behind them are compiled by nothing that runs here. */

const EnumValue kLcdInfoLine[] =
{
#if BOXMODEL_H7 || BOXMODEL_BRE2ZE4K
	{ 0, "lcd_info_line_channelnumber", NULL, NULL },
#else
	{ 0, "lcd_info_line_channelname", NULL, NULL },
#endif
	{ 1, "lcd_info_line_clock", NULL, NULL }
};

const EnumValue kOffAtZero[] =
{
	{ 0, "options.off", NULL, NULL }
};

const EnumValue kOffAtMinusOne[] =
{
	{ -1, "options.off", NULL, NULL }
};

/* A panel whose driver takes no count reads the number only as a flag, so
   there the setting is an off and an on, nought and one, under a label of
   its own. */
const Shape kLcdScrollFlag = { ValueType::Bool, "lcdmenu.scroll", 0, 1, NULL, 0, NULL };

const EnumValue kLedMode[] =
{
	{ 0, "ledcontroler.off", NULL, NULL },
	{ 1, "ledcontroler.on.all", NULL, NULL },
	{ 2, "ledcontroler.on.led1", NULL, NULL },
	{ 3, "ledcontroler.on.led2", NULL, NULL }
};

const EnumValue kLcd4lSupport[] =
{
	{ 0, "lcd4l_support_off", NULL, NULL },
	{ 1, "lcd4l_support_auto", NULL, NULL },
	{ 2, "lcd4l_support_on", NULL, NULL }
};

#if defined(ENABLE_GRAPHLCD) || defined(ENABLE_LCD4LINUX)
/* Both standby brightnesses are offered only where the box really goes to
   standby. The flag is inverted and reads nought as on, which is why the
   comparison is against nought and not against one. */
const Condition kNotRealShutdown[] =
{
	{ "shutdown_real", CompareOp::Eq, 0, NULL, 0 }
};
#endif

const Descriptor kSettings[] =
{
	/* The channel line the front panel shows. The H7 and the BRE2ZE4K word
	   nought as the channel number, every other box as the channel name. */
	{
		"lcd_info_line", ValueType::Enum, "display",
		"lcd_info_line", "menu.hint_vfd_infoline",
		0, 0, COREAPI_ENUM(kLcdInfoLine), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd_info_line)
	},
	{
		"lcd_notify_rclock", ValueType::Bool, "display",
		"lcdmenu.notify_rclock", "menu.hint_vfd_notify_rclock",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd_notify_rclock)
	},
	/* How many times a line too long for the panel is scrolled past, under the
	   label "scroll repeats". Where the driver takes no count the second shape
	   applies: an off and an on under the plain scroll label, which hold the
	   same number to nought and one. */
	{
		"lcd_scroll", ValueType::Int, "display",
		"lcdmenu.scroll_repeats", "menu.hint_vfd_scroll",
		0, 999, COREAPI_VALUES(kOffAtZero), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(lcd_scroll, countsScrolls, &kLcdScrollFlag)
	},
	/* The floor is what off means rather than nought, which is a brightness of
	   none. The ceiling is the panel's and the build decides it. */
	{
		"lcd_dim_brightness", ValueType::Int, "display",
		"lcdmenu.dim_brightness", "menu.hint_vfd_brightnessdim",
#ifdef ENABLE_LCD
		-1, 255,
#else
		-1, 15,
#endif
		COREAPI_VALUES(kOffAtMinusOne), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(lcd_setting_dim_brightness, canSetBrightness, NULL)
	},
	/* Minutes kept as text of at most three digits, read with atoi. A String
	   row carries neither the length nor the digits. */
	{
		"lcd_dim_time", ValueType::String, "display",
		"lcdmenu.dim_time", "menu.hint_vfd_dimtime",
		0, 0, NULL, 0, 0, "0", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD_ON(lcd_setting_dim_time, canSetBrightness, NULL)
	},
	{
		"led_tv_mode", ValueType::Enum, "display",
		"ledcontroler.mode.tv", "menu.hint_leds_tv",
		0, 0, COREAPI_ENUM(kLedMode), 2, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(led_tv_mode)
	},
	{
		"led_standby_mode", ValueType::Enum, "display",
		"ledcontroler.mode.standby", "menu.hint_leds_standby",
		0, 0, COREAPI_ENUM(kLedMode), 3, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(led_standby_mode)
	},
	{
		"led_deep_mode", ValueType::Enum, "display",
		"ledcontroler.mode.deepstandby", "menu.hint_leds_deepstandby",
		0, 0, COREAPI_ENUM(kLedMode), 3, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(led_deep_mode)
	},
	{
		"led_rec_mode", ValueType::Enum, "display",
		"ledcontroler.mode.record", "menu.hint_leds_record",
		0, 0, COREAPI_ENUM(kLedMode), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(led_rec_mode)
	},
	{
		"led_blink", ValueType::Bool, "display",
		"ledcontroler.blink", "menu.hint_leds_blink",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(led_blink)
	},
	{
		"backlight_tv", ValueType::Bool, "display",
		"ledcontroler.backlight.tv", "menu.hint_leds_tv",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(backlight_tv)
	},
	{
		"backlight_standby", ValueType::Bool, "display",
		"ledcontroler.mode.standby", "menu.hint_leds_standby",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(backlight_standby)
	},
	{
		"backlight_deepstandby", ValueType::Bool, "display",
		"ledcontroler.mode.deepstandby", "menu.hint_leds_deepstandby",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(backlight_deepstandby)
	},
#ifdef ENABLE_GRAPHLCD
	/* The default is the box's own display and no constant states it: the
	   loader asks the hardware whether the panel is a graphical one. Nought is
	   what a box without one gets. */
	{
		"glcd_enable", ValueType::Bool, "display",
		"glcd.enable", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(glcd_enable)
	},
	/* An index into the driver list the graphlcd configuration file holds, so
	   no constant states the ceiling. Anything outside it the box turns into
	   nought, which is why a wide one is safe. */
	{
		"glcd_selected_config", ValueType::Int, "display",
		"glcd.display", NULL,
		0, 99, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(glcd_selected_config)
	},
	{
		"glcd_logodir", ValueType::String, "display",
		"glcd.logodir", NULL,
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/logos", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(glcd_logodir)
	},
	{
		"glcd_brightness", ValueType::Int, "display",
		"glcd.brightness", NULL,
		0, 10, NULL, 0, GLCD_DEFAULT_BRIGHTNESS, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(glcd_brightness)
	},
	{
		"glcd_brightness_standby", ValueType::Int, "display",
		"glcd.brightness_standby", NULL,
		0, 10, NULL, 0, GLCD_DEFAULT_BRIGHTNESS_STANDBY, NULL, false, false,
		COREAPI_CONDITIONS(kNotRealShutdown),
		COREAPI_NUMBER_FIELD(glcd_brightness_standby)
	},
	{
		"glcd_brightness_dim", ValueType::Int, "display",
		"glcd.brightness_dim", NULL,
		0, 10, NULL, 0, GLCD_DEFAULT_BRIGHTNESS_DIM, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(glcd_brightness_dim)
	},
	/* Seconds kept as text, five digits and no more. A String row carries
	   neither. */
	{
		"glcd_brightness_dim_time", ValueType::String, "display",
		"glcd.brightness_dim_time", NULL,
		0, 0, NULL, 0, 0, GLCD_DEFAULT_BRIGHTNESS_DIM_TIME, false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(glcd_brightness_dim_time)
	},
	{
		"glcd_scroll", ValueType::Bool, "display",
		"glcd.scroll", NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(glcd_scroll)
	},
	/* The default is the box model's: the VU Uno 4K SE gets one and four more
	   VU boxes get two. Five is what every other box gets. */
	{
		"glcd_scroll_speed", ValueType::Int, "display",
		"glcd.scroll_speed", NULL,
		1, 63, NULL, 0, 5, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(glcd_scroll_speed)
	},
	{
		"glcd_mirror_osd", ValueType::Bool, "display",
		"glcd.mirror_osd", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(glcd_mirror_osd)
	},
	{
		"glcd_mirror_video", ValueType::Bool, "display",
		"glcd.mirror_video", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(glcd_mirror_video)
	},
#endif
#ifdef ENABLE_LCD4LINUX
	{
		"lcd4l_support", ValueType::Enum, "display",
		"lcd4l_support", "menu.hint_lcd4l_support",
		0, 0, COREAPI_ENUM(kLcd4lSupport), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd4l_support)
	},
	{
		"lcd4l_logodir", ValueType::String, "display",
		"lcd4l_logodir", "menu.hint_lcd4l_logodir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/logos", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(lcd4l_logodir)
	},
	/* A number and not a choice: the four panels are named by literal sizes
	   rather than locales, so a choice here would offer words the program does
	   not have. The range is that of the driver's panel enum. */
	{
		"lcd4l_display_type", ValueType::Int, "display",
		"lcd4l_display_type", "menu.hint_lcd4l_display_type",
		0, 3, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd4l_display_type)
	},
	/* A number and not a choice for a different reason: there are two lists of
	   skins and which one applies depends on the panel. The ceiling is the
	   highest either lists and the values between are ones neither offers. */
	{
		"lcd4l_skin", ValueType::Int, "display",
		"lcd4l_skin", "menu.hint_lcd4l_skin",
		0, 100, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd4l_skin)
	},
	{
		"lcd4l_skin_radio", ValueType::Bool, "display",
		"lcd4l_skin_radio", "menu.hint_lcd4l_skin_radio",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd4l_skin_radio)
	},
	/* The ceiling is the panel's and the box asks it at run time: seven for the
	   Pearl panel and ten for the three Samsung ones. Ten is the wider and holds
	   every value either offers. */
	{
		"lcd4l_brightness", ValueType::Int, "display",
		"lcd4l_brightness", "menu.hint_lcd4l_brightness",
		1, 10, NULL, 0, 7, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd4l_brightness)
	},
	{
		"lcd4l_brightness_standby", ValueType::Int, "display",
		"lcd4l_brightness_standby", "menu.hint_lcd4l_brightness_standby",
		1, 10, NULL, 0, 3, NULL, false, false,
		COREAPI_CONDITIONS(kNotRealShutdown),
		COREAPI_NUMBER_FIELD(lcd4l_brightness_standby)
	},
	{
		"lcd4l_convert", ValueType::Bool, "display",
		"lcd4l_convert", "menu.hint_lcd4l_convert",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd4l_convert)
	},
	/* Read by the web interface out of the saved file and by nothing in the
	   program, so a written value takes effect when it has been saved and not
	   before. */
	{
		"lcd4l_screenshots", ValueType::Bool, "display",
		"lcd4l_screenshots", "menu.hint_lcd4l_screenshots",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd4l_screenshots)
	},
#endif

#ifdef ENABLE_GRAPHLCD
	/* The name of the panel theme, picked from a file list and read at start.
	   Its default is a migration name where the box has no settings file yet
	   and empty otherwise, which is what every box that has saved once gets. */
	{
		"glcd_theme_name", ValueType::String, "display",
		NULL, NULL,
		0, 0, NULL, 0, 0, "", true, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(glcd_theme_name)
	},
#endif
};

} // anonymous namespace

const Descriptor *settingsTableDisplay(size_t &count)
{
	count = sizeof(kSettings) / sizeof(kSettings[0]);
	return kSettings;
}

} // namespace coreapi
