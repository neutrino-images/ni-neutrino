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
#include "boxdefaults.h"

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

constexpr EnumValue kLcdInfoLine[] =
{
#if BOXMODEL_H7 || BOXMODEL_BRE2ZE4K
	option(0).label("lcd_info_line_channelnumber"),
#else
	option(0).label("lcd_info_line_channelname"),
#endif
	option(1).label("lcd_info_line_clock")
};

constexpr EnumValue kOffAtZero[] =
{
	option(0).label("options.off")
};

constexpr EnumValue kOffAtMinusOne[] =
{
	option(-1).label("options.off")
};

/* A panel whose driver takes no count reads the number only as a flag, so
   there the setting is an off and an on, nought and one, under a label of
   its own. */
constexpr Shape kLcdScrollFlag = shape(ValueType::Bool, "lcdmenu.scroll")
	.range(0, 1)
	.offered(vfdEnabled);

constexpr EnumValue kLedMode[] =
{
	option(0).label("ledcontroler.off"),
	option(1).label("ledcontroler.on.all"),
	option(2).label("ledcontroler.on.led1"),
	option(3).label("ledcontroler.on.led2")
};

constexpr EnumValue kLcd4lSupport[] =
{
	option(0).label("lcd4l_support_off"),
	option(1).label("lcd4l_support_auto"),
	option(2).label("lcd4l_support_on")
};

#if defined(ENABLE_GRAPHLCD) || defined(ENABLE_LCD4LINUX)
/* Both standby brightnesses are offered only where the box really goes to
   standby. The flag is inverted and reads nought as on, which is why the
   comparison is against nought and not against one. */
constexpr Condition kNotRealShutdown[] =
{
	when("shutdown_real").is(0)
};
#endif

#ifdef ENABLE_GRAPHLCD
long glcdEnableDefault()
{
	return hasGraphicPanel() ? 1 : 0;
}

long glcdScrollSpeedDefault()
{
	if (boxdefault::kScrollSpeedOne)
		return 1;
	if (boxdefault::kScrollSpeedTwo)
		return 2;
	return 5;
}
#endif

constexpr Descriptor kSettings[] =
{
	/* The channel line the front panel shows. The H7 and the BRE2ZE4K word
	   nought as the channel number, every other box as the channel name. */
	enumRow("lcd_info_line")
		.section("display")
		.label("lcd_info_line")
		.hint("menu.hint_vfd_infoline")
		.defaultValue(0)
		.values(kLcdInfoLine)
		.field(COREAPI_NUMBER_FIELD_ON(lcd_info_line, vfdEnabled, NULL)),
	boolRow("lcd_notify_rclock")
		.section("display")
		.label("lcdmenu.notify_rclock")
		.hint("menu.hint_vfd_notify_rclock")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD_ON(lcd_notify_rclock, vfdEnabled, NULL)),
	/* How many times a line too long for the panel is scrolled past, under the
	   label "scroll repeats". Where the driver takes no count the second shape
	   applies: an off and an on under the plain scroll label, which hold the
	   same number to nought and one. Where the panel is not wired up the row
	   is in neither shape, like its neighbours, while the shape stays what
	   the driver takes. */
	intRow("lcd_scroll")
		.section("display")
		.label("lcdmenu.scroll_repeats")
		.hint("menu.hint_vfd_scroll")
		.range(0, 999)
		.defaultValue(1)
		.values(kOffAtZero)
		.field(COREAPI_NUMBER_FIELD_ON(lcd_scroll, vfdCountsScrolls, &kLcdScrollFlag)),
	/* The floor is what off means rather than nought, which is a brightness of
	   none. The ceiling is the panel's and the build decides it. */
	intRow("lcd_dim_brightness")
		.section("display")
		.label("lcdmenu.dim_brightness")
		.hint("menu.hint_vfd_brightnessdim")
#ifdef ENABLE_LCD
		.range(-1, 255)
#else
		.range(-1, 15)
#endif
		.defaultValue(0)
		.values(kOffAtMinusOne)
		.field(COREAPI_NUMBER_FIELD_ON(lcd_setting_dim_brightness, canSetPanelBrightness, NULL)),
	/* Minutes kept as text of at most three digits, read with atoi. A String
	   row carries neither the length nor the digits. */
	textRow("lcd_dim_time")
		.section("display")
		.label("lcdmenu.dim_time")
		.hint("menu.hint_vfd_dimtime")
		.defaultValue("0")
		.text(kRuleNumberText3)
		.field(COREAPI_TEXT_FIELD_ON(lcd_setting_dim_time, canSetPanelBrightness, NULL)),
	enumRow("led_tv_mode")
		.section("display")
		.label("ledcontroler.mode.tv")
		.hint("menu.hint_leds_tv")
		.defaultValue(2)
		.values(kLedMode)
		.field(COREAPI_NUMBER_FIELD_ON(led_tv_mode, hasLedMenu, NULL)),
	enumRow("led_standby_mode")
		.section("display")
		.label("ledcontroler.mode.standby")
		.hint("menu.hint_leds_standby")
		.defaultValue(3)
		.values(kLedMode)
		.field(COREAPI_NUMBER_FIELD_ON(led_standby_mode, hasLedMenu, NULL)),
	enumRow("led_deep_mode")
		.section("display")
		.label("ledcontroler.mode.deepstandby")
		.hint("menu.hint_leds_deepstandby")
		.defaultValue(3)
		.values(kLedMode)
		.field(COREAPI_NUMBER_FIELD_ON(led_deep_mode, hasLedMenu, NULL)),
	enumRow("led_rec_mode")
		.section("display")
		.label("ledcontroler.mode.record")
		.hint("menu.hint_leds_record")
		.defaultValue(1)
		.values(kLedMode)
		.field(COREAPI_NUMBER_FIELD_ON(led_rec_mode, hasLedMenu, NULL)),
	boolRow("led_blink")
		.section("display")
		.label("ledcontroler.blink")
		.hint("menu.hint_leds_blink")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD_ON(led_blink, hasLedMenu, NULL)),
	boolRow("backlight_tv")
		.section("display")
		.label("ledcontroler.backlight.tv")
		.hint("menu.hint_leds_tv")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD_ON(backlight_tv, hasBacklight, NULL)),
	boolRow("backlight_standby")
		.section("display")
		.label("ledcontroler.mode.standby")
		.hint("menu.hint_leds_standby")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD_ON(backlight_standby, hasBacklight, NULL)),
	boolRow("backlight_deepstandby")
		.section("display")
		.label("ledcontroler.mode.deepstandby")
		.hint("menu.hint_leds_deepstandby")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD_ON(backlight_deepstandby, hasBacklight, NULL)),
#ifdef ENABLE_GRAPHLCD
	/* The default is the box's own display: the loader asks the hardware whether
	   the panel is a graphical one. Nought is what a box without one gets. */
	boolRow("glcd_enable")
		.section("display")
		.label("glcd.enable")
		.defaultValue(0)
		.defaultFrom(glcdEnableDefault)
		.field(COREAPI_NUMBER_FIELD(glcd_enable)),
	/* An index into the driver list the graphlcd configuration file holds, so
	   no constant states the ceiling. Anything outside it the box turns into
	   nought, which is why a wide one is safe. */
	intRow("glcd_selected_config")
		.section("display")
		.label("glcd.display")
		.range(0, 99)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(glcd_selected_config)),
	textRow("glcd_logodir")
		.section("display")
		.label("glcd.logodir")
		.defaultValue(TARGET_ROOT "/media/sda1/logos")
		.text(kRuleDirectory)
		.field(COREAPI_TEXT_FIELD(glcd_logodir)),
	intRow("glcd_brightness")
		.section("display")
		.label("glcd.brightness")
		.range(0, 10)
		.defaultValue(GLCD_DEFAULT_BRIGHTNESS)
		.field(COREAPI_NUMBER_FIELD(glcd_brightness)),
	intRow("glcd_brightness_standby")
		.section("display")
		.label("glcd.brightness_standby")
		.range(0, 10)
		.defaultValue(GLCD_DEFAULT_BRIGHTNESS_STANDBY)
		.changeableWhen(kNotRealShutdown)
		.field(COREAPI_NUMBER_FIELD(glcd_brightness_standby)),
	intRow("glcd_brightness_dim")
		.section("display")
		.label("glcd.brightness_dim")
		.range(0, 10)
		.defaultValue(GLCD_DEFAULT_BRIGHTNESS_DIM)
		.field(COREAPI_NUMBER_FIELD(glcd_brightness_dim)),
	/* Seconds kept as text, five digits and no more. A String row carries
	   neither. */
	textRow("glcd_brightness_dim_time")
		.section("display")
		.label("glcd.brightness_dim_time")
		.defaultValue(GLCD_DEFAULT_BRIGHTNESS_DIM_TIME)
		.text(kRuleNumberText5)
		.field(COREAPI_TEXT_FIELD(glcd_brightness_dim_time)),
	boolRow("glcd_scroll")
		.section("display")
		.label("glcd.scroll")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(glcd_scroll)),
	/* The default is the box model's: the VU Uno 4K SE gets one and four more
	   VU boxes get two, which default_fn states. Five is what every other box
	   gets. */
	intRow("glcd_scroll_speed")
		.section("display")
		.label("glcd.scroll_speed")
		.range(1, 63)
		.defaultValue(5)
		.defaultFrom(glcdScrollSpeedDefault)
		.field(COREAPI_NUMBER_FIELD(glcd_scroll_speed)),
	boolRow("glcd_mirror_osd")
		.section("display")
		.label("glcd.mirror_osd")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(glcd_mirror_osd)),
	boolRow("glcd_mirror_video")
		.section("display")
		.label("glcd.mirror_video")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(glcd_mirror_video)),
#endif
#ifdef ENABLE_LCD4LINUX
	enumRow("lcd4l_support")
		.section("display")
		.label("lcd4l_support")
		.hint("menu.hint_lcd4l_support")
		.defaultValue(0)
		.values(kLcd4lSupport)
		.field(COREAPI_NUMBER_FIELD(lcd4l_support)),
	textRow("lcd4l_logodir")
		.section("display")
		.label("lcd4l_logodir")
		.hint("menu.hint_lcd4l_logodir")
		.defaultValue(TARGET_ROOT "/media/sda1/logos")
		.text(kRuleDirectory)
		.field(COREAPI_TEXT_FIELD(lcd4l_logodir)),
	/* A number and not a choice: the four panels are named by literal sizes
	   rather than locales, so a choice here would offer words the program does
	   not have. The range is that of the driver's panel enum. */
	intRow("lcd4l_display_type")
		.section("display")
		.label("lcd4l_display_type")
		.hint("menu.hint_lcd4l_display_type")
		.range(0, 3)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(lcd4l_display_type)),
	/* A number and not a choice for a different reason: there are two lists of
	   skins and which one applies depends on the panel. The ceiling is the
	   highest either lists and the values between are ones neither offers. */
	intRow("lcd4l_skin")
		.section("display")
		.label("lcd4l_skin")
		.hint("menu.hint_lcd4l_skin")
		.range(0, 100)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(lcd4l_skin)),
	boolRow("lcd4l_skin_radio")
		.section("display")
		.label("lcd4l_skin_radio")
		.hint("menu.hint_lcd4l_skin_radio")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(lcd4l_skin_radio)),
	/* The ceiling is the panel's and the box asks it at run time: seven for the
	   Pearl panel and ten for the three Samsung ones. Ten is the wider and holds
	   every value either offers. */
	intRow("lcd4l_brightness")
		.section("display")
		.label("lcd4l_brightness")
		.hint("menu.hint_lcd4l_brightness")
		.range(1, 10)
		.defaultValue(7)
		.field(COREAPI_NUMBER_FIELD(lcd4l_brightness)),
	intRow("lcd4l_brightness_standby")
		.section("display")
		.label("lcd4l_brightness_standby")
		.hint("menu.hint_lcd4l_brightness_standby")
		.range(1, 10)
		.defaultValue(3)
		.changeableWhen(kNotRealShutdown)
		.field(COREAPI_NUMBER_FIELD(lcd4l_brightness_standby)),
	boolRow("lcd4l_convert")
		.section("display")
		.label("lcd4l_convert")
		.hint("menu.hint_lcd4l_convert")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(lcd4l_convert)),
	/* Read by the web interface out of the saved file and by nothing in the
	   program, so a written value takes effect when it has been saved and not
	   before. */
	boolRow("lcd4l_screenshots")
		.section("display")
		.label("lcd4l_screenshots")
		.hint("menu.hint_lcd4l_screenshots")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(lcd4l_screenshots)),
#endif

#ifdef ENABLE_GRAPHLCD
	/* The name of the panel theme, picked from a file list and read at start.
	   Its default is a migration name where the box has no settings file yet
	   and empty otherwise, which is what every box that has saved once gets. */
	textRow("glcd_theme_name")
		.section("display")
		.defaultValue("")
		.needsRestart()
		.text(kRuleNameFromList)
		.field(COREAPI_TEXT_FIELD(glcd_theme_name)),
	/* The panel theme's colours, steps from 0 to 100 as the theme colours are. The panel has
	   no alpha. The defaults are the ones its theme file falls back to. */
	colorRow("glcd_theme.glcd_foreground_color")
		.section("display")
		.label("glcd.color_fg")
		.defaultValue("#ffffff")
		.field(COREAPI_COLOR_FIELD(glcd_theme, glcd_foreground_color, false)),
	colorRow("glcd_theme.glcd_background_color")
		.section("display")
		.label("glcd.color_bg")
		.defaultValue("#000000")
		.field(COREAPI_COLOR_FIELD(glcd_theme, glcd_background_color, false)),
	colorRow("glcd_theme.glcd_progressbar_color")
		.section("display")
		.label("glcd.progressbar_color")
		.defaultValue("#fafafa")
		.field(COREAPI_COLOR_FIELD(glcd_theme, glcd_progressbar_color, false)),
#endif
};

} // anonymous namespace

const Descriptor *settingsTableDisplay(size_t &count)
{
	count = sizeof(kSettings) / sizeof(kSettings[0]);
	return kSettings;
}

} // namespace coreapi
