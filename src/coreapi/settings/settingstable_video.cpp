/*
 * settingstable_video.cpp - video settings, one row per field
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
#include "videomodes.h"

#include <hardware/video.h>

#include <string.h>

namespace coreapi
{

namespace
{

/* The video section. Five of this screen's number choosers sit in an arm the
   preprocessor drops, and the fields behind them are loaded, saved and edited
   by another screen that carries no bound of its own. Those rows cite the
   dropped call, because it is the only statement of a bound there is. */

constexpr EnumValue kVideoFormat[] =
{
	option(DISPLAY_AR_4_3).label("videomenu.videoformat_43"),
	option(DISPLAY_AR_16_9).label("videomenu.videoformat_169"),
	option(DISPLAY_AR_14_9).label("videomenu.videoformat_149").availableIf(canAspect149)
};

constexpr EnumValue kVideo43Mode[] =
{
	option(DISPLAY_AR_MODE_PANSCAN).label("videomenu.panscan"),
	option(DISPLAY_AR_MODE_PANSCAN2).label("videomenu.panscan2").availableIf(canPanScan149),
	option(DISPLAY_AR_MODE_LETTERBOX).label("videomenu.letterbox"),
	option(DISPLAY_AR_MODE_NONE).label("videomenu.fullscreen")
};

/* Every video mode the program has a word for, in the order the settings
   file numbers its enabled_video_mode_<n> and enabled_auto_mode_<n> keys.
   Each word is written once, here, and the tables below use the names, so a
   misspelt one does not compile rather than losing its number on one box. */
#define MODE_NTSC "NTSC"
#define MODE_PAL "PAL"
#define MODE_SECAM "SECAM"
#define MODE_M480P "480p"
#define MODE_M576P "576p"
#define MODE_M720P_50HZ "720p 50Hz"
#define MODE_M720P_60HZ "720p 60Hz"
#define MODE_M1080I_50HZ "1080i 50Hz"
#define MODE_M1080I_60HZ "1080i 60Hz"
#define MODE_M1080P_2397HZ "1080p 23.97Hz"
#define MODE_M1080P_24HZ "1080p 24Hz"
#define MODE_M1080P_25HZ "1080p 25Hz"
#define MODE_M1080P_2997HZ "1080p 29.97Hz"
#define MODE_M1080P_50HZ "1080p 50Hz"
#define MODE_M1080P_60HZ "1080p 60Hz"
#define MODE_M2160P_24HZ "2160p 24Hz"
#define MODE_M2160P_25HZ "2160p 25Hz"
#define MODE_M2160P_30HZ "2160p 30Hz"
#define MODE_M2160P_50HZ "2160p 50Hz"
#define MODE_AUTO "Auto"

const char *const kVideoModeNames[] =
{
	MODE_NTSC, MODE_PAL, MODE_SECAM, MODE_M480P, MODE_M576P,
	MODE_M720P_50HZ, MODE_M720P_60HZ, MODE_M1080I_50HZ, MODE_M1080I_60HZ,
	MODE_M1080P_2397HZ, MODE_M1080P_24HZ, MODE_M1080P_25HZ, MODE_M1080P_2997HZ, MODE_M1080P_50HZ, MODE_M1080P_60HZ,
	MODE_M2160P_24HZ, MODE_M2160P_25HZ, MODE_M2160P_30HZ, MODE_M2160P_50HZ,
	MODE_AUTO
};
static_assert(sizeof(kVideoModeNames) / sizeof(kVideoModeNames[0]) == VIDEOMENU_VIDEOMODE_OPTION_COUNT,
	      "one name per numbered video mode");

/* The modes the family draws, in that order and in those words; the conditions
   are in videomodes.h. */
constexpr EnumValue kVideoMode[] =
{
#if COREAPI_VIDEOMODES == COREAPI_VIDEOMODES_CST_HD1
	option(VIDEO_STD_NTSC).text(MODE_NTSC),
	option(VIDEO_STD_PAL).text(MODE_PAL),
	option(VIDEO_STD_SECAM).text(MODE_SECAM),
	option(VIDEO_STD_480P).text(MODE_M480P),
	option(VIDEO_STD_576P).text(MODE_M576P),
	option(VIDEO_STD_720P50).text(MODE_M720P_50HZ),
	option(VIDEO_STD_720P60).text(MODE_M720P_60HZ),
	option(VIDEO_STD_1080I50).text(MODE_M1080I_50HZ),
	option(VIDEO_STD_1080I60).text(MODE_M1080I_60HZ),
	option(VIDEO_STD_1080P24).text(MODE_M1080P_24HZ),
	option(VIDEO_STD_1080P25).text(MODE_M1080P_25HZ),
	option(VIDEO_STD_AUTO).text(MODE_AUTO)
#elif COREAPI_VIDEOMODES == COREAPI_VIDEOMODES_CST_HD2
	option(VIDEO_STD_NTSC).text(MODE_NTSC),
	option(VIDEO_STD_PAL).text(MODE_PAL),
	option(VIDEO_STD_SECAM).text(MODE_SECAM),
	option(VIDEO_STD_480P).text(MODE_M480P),
	option(VIDEO_STD_576P).text(MODE_M576P),
	option(VIDEO_STD_720P50).text(MODE_M720P_50HZ),
	option(VIDEO_STD_720P60).text(MODE_M720P_60HZ),
	option(VIDEO_STD_1080I50).text(MODE_M1080I_50HZ),
	option(VIDEO_STD_1080I60).text(MODE_M1080I_60HZ),
	option(VIDEO_STD_1080P2397).text(MODE_M1080P_2397HZ),
	option(VIDEO_STD_1080P24).text(MODE_M1080P_24HZ),
	option(VIDEO_STD_1080P25).text(MODE_M1080P_25HZ),
	option(VIDEO_STD_1080P2997).text(MODE_M1080P_2997HZ),
	option(VIDEO_STD_1080P50).text(MODE_M1080P_50HZ),
	option(VIDEO_STD_1080P60).text(MODE_M1080P_60HZ),
	option(VIDEO_STD_AUTO).text(MODE_AUTO)
#elif COREAPI_VIDEOMODES == COREAPI_VIDEOMODES_4K
	option(VIDEO_STD_PAL).text(MODE_PAL),
	option(VIDEO_STD_576P).text(MODE_M576P),
	option(VIDEO_STD_720P50).text(MODE_M720P_50HZ),
	option(VIDEO_STD_720P60).text(MODE_M720P_60HZ),
	option(VIDEO_STD_1080I50).text(MODE_M1080I_50HZ),
	option(VIDEO_STD_1080I60).text(MODE_M1080I_60HZ),
	option(VIDEO_STD_1080P24).text(MODE_M1080P_24HZ),
	option(VIDEO_STD_1080P25).text(MODE_M1080P_25HZ),
	option(VIDEO_STD_1080P50).text(MODE_M1080P_50HZ),
	option(VIDEO_STD_2160P24).text(MODE_M2160P_24HZ),
	option(VIDEO_STD_2160P25).text(MODE_M2160P_25HZ),
	option(VIDEO_STD_2160P30).text(MODE_M2160P_30HZ),
	option(VIDEO_STD_2160P50).text(MODE_M2160P_50HZ)
#elif COREAPI_VIDEOMODES == COREAPI_VIDEOMODES_OSMIO4K
	option(VIDEO_STD_PAL).text(MODE_PAL),
	option(VIDEO_STD_576P).text(MODE_M576P),
	option(VIDEO_STD_720P50).text(MODE_M720P_50HZ),
	option(VIDEO_STD_720P60).text(MODE_M720P_60HZ),
	option(VIDEO_STD_1080I50).text(MODE_M1080I_50HZ),
	option(VIDEO_STD_1080I60).text(MODE_M1080I_60HZ),
	option(VIDEO_STD_1080P24).text(MODE_M1080P_24HZ),
	option(VIDEO_STD_1080P25).text(MODE_M1080P_25HZ),
	option(VIDEO_STD_1080P50).text(MODE_M1080P_50HZ),
	option(VIDEO_STD_1080P60).text(MODE_M1080P_60HZ),
	option(VIDEO_STD_2160P24).text(MODE_M2160P_24HZ),
	option(VIDEO_STD_2160P25).text(MODE_M2160P_25HZ),
	option(VIDEO_STD_2160P30).text(MODE_M2160P_30HZ),
	option(VIDEO_STD_2160P50).text(MODE_M2160P_50HZ)
#else
	// A PC build: 480, 576, 720 and 1080 lines.
	option(VIDEO_STD_NTSC).text(MODE_NTSC),
	option(VIDEO_STD_PAL).text(MODE_PAL),
	option(VIDEO_STD_720P50).text(MODE_M720P_50HZ),
	option(VIDEO_STD_720P60).text(MODE_M720P_60HZ),
	option(VIDEO_STD_1080I50).text(MODE_M1080I_50HZ)
#endif
};

constexpr EnumValue kDbDr[] =
{
	option(0).label("videomenu.dbdr_none"),
	option(1).label("videomenu.dbdr_deblock"),
	option(2).label("videomenu.dbdr_both")
};

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
constexpr EnumValue kZappingMode[] =
{
	option(0).label("videomenu.zappingmode_mute"),
	option(1).label("videomenu.zappingmode_hold"),
	option(2).label("videomenu.zappingmode_mutetilllock"),
	option(3).label("videomenu.zappingmode_holdtilllock")
};

/* The same stored number names another colorimetry on the two arms. Numbers
   and not the HDMI_COLORIMETRY names, which the table checks compile against
   libraries that do not declare; they are the values those names have. */
constexpr EnumValue kHdmiColorimetry[] =
{
#if BOXMODEL_VUPLUS_ARM
	option(0).label("videomenu.hdmi_colorimetry_auto"),
	option(1).label("videomenu.hdmi_colorimetry_bt709"),
	option(2).label("videomenu.hdmi_colorimetry_bt470")
#else
	option(0).label("videomenu.hdmi_colorimetry_auto"),
	option(1).label("videomenu.hdmi_colorimetry_bt2020ncl"),
	option(2).label("videomenu.hdmi_colorimetry_bt2020cl"),
	option(3).label("videomenu.hdmi_colorimetry_bt709")
#endif
};
#endif

// Each hardware library numbers the modes its own way.
constexpr EnumValue kCecMode[] =
{
	option(VIDEO_HDMI_CEC_MODE_OFF).label("videomenu.hdmi_cec_mode_off"),
	option(VIDEO_HDMI_CEC_MODE_TUNER).label("videomenu.hdmi_cec_mode_tuner"),
	option(VIDEO_HDMI_CEC_MODE_RECORDER).label("videomenu.hdmi_cec_mode_recorder")
};

// The numbers are the ones of VIDEO_HDMI_CEC_VOL in src/driver/hdmi_cec.h, the
// same in every hardware library that names them; not every library does.
constexpr EnumValue kCecVolume[] =
{
	option(0).label("videomenu.hdmi_cec_vol_off"),
	option(1).label("videomenu.hdmi_cec_vol_audiosystem"),
	option(2).label("videomenu.hdmi_cec_vol_tv")
};

// The three below the mode are offered only while the link is on.
constexpr Condition kCecOn[] =
{
	when("hdmi_cec_mode").isNot(0)
};

/* The analog outputs, each entry offered on the board revisions that have it.
   One build of the hardware library names a mode by connector, kind and
   format and the other by one name each. */
#ifdef ANALOG_MODE
#define ANALOG_OUT_SCART_SD_RGB ANALOG_MODE(SCART, SD, RGB)
#define ANALOG_OUT_SCART_SD_YPRPB ANALOG_MODE(SCART, SD, YPRPB)
#define ANALOG_OUT_SCART_HD_RGB ANALOG_MODE(SCART, HD, RGB)
#define ANALOG_OUT_SCART_HD_YPRPB ANALOG_MODE(SCART, HD, YPRPB)
#define ANALOG_OUT_CINCH_SD_RGB ANALOG_MODE(CINCH, SD, RGB)
#define ANALOG_OUT_CINCH_SD_YPRPB ANALOG_MODE(CINCH, SD, YPRPB)
#define ANALOG_OUT_CINCH_HD_RGB ANALOG_MODE(CINCH, HD, RGB)
#define ANALOG_OUT_CINCH_HD_YPRPB ANALOG_MODE(CINCH, HD, YPRPB)
#else
#define ANALOG_OUT_SCART_SD_RGB ANALOG_SD_RGB_SCART
#define ANALOG_OUT_SCART_SD_YPRPB ANALOG_SD_YPRPB_SCART
#define ANALOG_OUT_SCART_HD_RGB ANALOG_HD_RGB_SCART
#define ANALOG_OUT_SCART_HD_YPRPB ANALOG_HD_YPRPB_SCART
#define ANALOG_OUT_CINCH_SD_RGB ANALOG_SD_RGB_CINCH
#define ANALOG_OUT_CINCH_SD_YPRPB ANALOG_SD_YPRPB_CINCH
#define ANALOG_OUT_CINCH_HD_RGB ANALOG_HD_RGB_CINCH
#define ANALOG_OUT_CINCH_HD_YPRPB ANALOG_HD_YPRPB_CINCH
#endif

// Revision 6 takes SCART and Cinch together, in one list.
#define ANALOG_ONE_ITEM(t) \
	option(ANALOG_OUT_SCART_SD_RGB).label("videomenu.analog_sd_rgb_scart").availableIf(t), \
	option(ANALOG_OUT_CINCH_SD_RGB).label("videomenu.analog_sd_rgb_cinch").availableIf(t), \
	option(ANALOG_OUT_SCART_SD_YPRPB).label("videomenu.analog_sd_yprpb_scart").availableIf(t), \
	option(ANALOG_OUT_CINCH_SD_YPRPB).label("videomenu.analog_sd_yprpb_cinch").availableIf(t), \
	option(ANALOG_OUT_SCART_HD_RGB).label("videomenu.analog_hd_rgb_scart").availableIf(t), \
	option(ANALOG_OUT_CINCH_HD_RGB).label("videomenu.analog_hd_rgb_cinch").availableIf(t), \
	option(ANALOG_OUT_SCART_HD_YPRPB).label("videomenu.analog_hd_yprpb_scart").availableIf(t), \
	option(ANALOG_OUT_CINCH_HD_YPRPB).label("videomenu.analog_hd_yprpb_cinch").availableIf(t)

#if defined(BOXMODEL_CST_HD2) && defined(ANALOG_MODE)
// One list: the newer boards take the combined modes, revision 6 the one-item list.
constexpr EnumValue kAnalogHd2[] =
{
	option(ANALOG_MODE(BOTH, xD, AUTO)).label("videomenu.analog_auto").availableIf(analogOutputsSplit),
	option(ANALOG_MODE(BOTH, xD, CVBS)).label("videomenu.analog_cvbs").availableIf(analogOutputsSplit),
	option(ANALOG_MODE(BOTH, SD, RGB)).label("videomenu.analog_sd_rgb").availableIf(analogOutputsSplit),
	option(ANALOG_MODE(BOTH, SD, YPRPB)).label("videomenu.analog_sd_yprpb").availableIf(analogOutputsSplit),
	option(ANALOG_MODE(BOTH, HD, RGB)).label("videomenu.analog_hd_rgb").availableIf(analogOutputsSplit),
	option(ANALOG_MODE(BOTH, HD, YPRPB)).label("videomenu.analog_hd_yprpb").availableIf(analogOutputsSplit),
	ANALOG_ONE_ITEM(analogOneItem)
};

constexpr EnumValue kAnalogOneItem[] = { ANALOG_ONE_ITEM(analogOneItem) };
#else
constexpr EnumValue kAnalogOneItem[] = { ANALOG_ONE_ITEM(NULL) };

// A SCART socket of its own, the SD entries on more boards than the HD ones.
constexpr EnumValue kAnalogScart[] =
{
	option(ANALOG_OUT_SCART_SD_RGB).label("videomenu.analog_sd_rgb_scart").availableIf(scartSdOffered),
	option(ANALOG_OUT_SCART_SD_YPRPB).label("videomenu.analog_sd_yprpb_scart").availableIf(scartSdOffered),
	option(ANALOG_OUT_SCART_HD_RGB).label("videomenu.analog_hd_rgb_scart").availableIf(scartHdOffered),
	option(ANALOG_OUT_SCART_HD_YPRPB).label("videomenu.analog_hd_yprpb_scart").availableIf(scartHdOffered)
};

constexpr Shape kAnalogMode1Scart = shape(ValueType::Enum, "videomenu.scart")
	.values(kAnalogScart)
	.hint("menu.hint_video_scart_mode");

constexpr EnumValue kAnalogCinch[] =
{
	option(ANALOG_OUT_CINCH_SD_RGB).label("videomenu.analog_sd_rgb_cinch"),
	option(ANALOG_OUT_CINCH_SD_YPRPB).label("videomenu.analog_sd_yprpb_cinch"),
	option(ANALOG_OUT_CINCH_HD_RGB).label("videomenu.analog_hd_rgb_cinch"),
	option(ANALOG_OUT_CINCH_HD_YPRPB).label("videomenu.analog_hd_yprpb_cinch")
};
#endif

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
long zappingModeDefault()
{
	return boxdefault::kZappingModeTwo ? 2 : 0;
}
#endif

constexpr Descriptor kVideo[] =
{
	enumRow("video_Format")
		.section("video")
		.label("videomenu.videoformat")
		.hint("menu.hint_video_format")
		.defaultValue(DISPLAY_AR_16_9)
		.values(kVideoFormat)
		.field(COREAPI_NUMBER_FIELD(video_Format)),
	// How a 4:3 picture is put on a wide screen.
	enumRow("video_43mode")
		.section("video")
		.label("videomenu.43mode")
		.hint("menu.hint_video_43mode")
		.defaultValue(DISPLAY_AR_MODE_LETTERBOX)
		.values(kVideo43Mode)
		.field(COREAPI_NUMBER_FIELD(video_43mode)),
	// The revision decides it, and it is read at run time.
	enumRow("video_dbdr")
		.section("video")
		.label("videomenu.dbdr")
		.hint("menu.hint_video_dbdr")
		.defaultValue(0)
		.values(kDbDr)
		.field(COREAPI_NUMBER_FIELD_ON(video_dbdr, hasDbdr, NULL)),
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	// One box model falls back to 2 instead, which default_fn states.
	enumRow("zappingmode")
		.section("video")
		.label("videomenu.zappingmode")
		.hint("menu.hint_video_zappingmode")
		.defaultValue(0)
		.defaultFrom(zappingModeDefault)
		.values(kZappingMode)
		.field(COREAPI_NUMBER_FIELD_ON(zappingmode, takesZappingMode, NULL)),
	enumRow("hdmi_colorimetry")
		.section("video")
		.label("videomenu.hdmi_colorimetry")
		.hint("menu.hint_video_hdmi_colorimetry")
		.defaultValue(0)
		.values(kHdmiColorimetry)
		.field(COREAPI_NUMBER_FIELD_ON(hdmi_colorimetry, takesHdmiColorimetry, NULL)),
	/* The five below: the four values are held to 0..255 and no bound is stated
	   for the step. */
	intRow("video_psi_brightness")
		.section("video")
		.label("videomenu.psi.brightness")
		.hint("menu.hint_video_brightness")
		.range(0, 255)
		.defaultValue(128)
		.field(COREAPI_NUMBER_FIELD(psi_brightness)),
	intRow("video_psi_contrast")
		.section("video")
		.label("videomenu.psi.contrast")
		.hint("menu.hint_video_contrast")
		.range(0, 255)
		.defaultValue(128)
		.field(COREAPI_NUMBER_FIELD(psi_contrast)),
	intRow("video_psi_saturation")
		.section("video")
		.label("videomenu.psi.saturation")
		.hint("menu.hint_video_saturation")
		.range(0, 255)
		.defaultValue(128)
		.field(COREAPI_NUMBER_FIELD(psi_saturation)),
	intRow("video_psi_tint")
		.section("video")
		.label("videomenu.psi.tint")
		.hint("menu.hint_video_tint")
		.range(0, 255)
		.defaultValue(128)
		.field(COREAPI_NUMBER_FIELD(psi_tint)),
	// How far one press moves the four above, not a value of its own.
	intRow("video_psi_step")
		.section("video")
		.label("videomenu.psi.step")
		.hint("menu.hint_video_psi_step")
		.range(1, 100)
		.defaultValue(2)
		.field(COREAPI_NUMBER_FIELD(psi_step)),
#endif
	// The HDMI link, only where the box can drive it.
	enumRow("hdmi_cec_mode")
		.section("video")
		.label("videomenu.hdmi_cec_mode")
		.hint("menu.hint_cec_mode")
		.defaultValue(0)
		.values(kCecMode)
		.field(COREAPI_NUMBER_FIELD_ON(hdmi_cec_mode, canCec, NULL)),
	boolRow("hdmi_cec_view_on")
		.section("video")
		.label("videomenu.hdmi_cec_view_on")
		.hint("menu.hint_cec_view_on")
		.defaultValue(0)
		.changeableWhen(kCecOn)
		.field(COREAPI_NUMBER_FIELD_ON(hdmi_cec_view_on, canCec, NULL)),
	boolRow("hdmi_cec_standby")
		.section("video")
		.label("videomenu.hdmi_cec_standby")
		.hint("menu.hint_cec_standby")
		.defaultValue(0)
		.changeableWhen(kCecOn)
		.field(COREAPI_NUMBER_FIELD_ON(hdmi_cec_standby, canCec, NULL)),
	enumRow("hdmi_cec_volume")
		.section("video")
		.label("videomenu.hdmi_cec_volume")
		.hint("menu.hint_cec_volume")
		.defaultValue(0)
		.values(kCecVolume)
		.changeableWhen(kCecOn)
		.field(COREAPI_NUMBER_FIELD_ON(hdmi_cec_volume, canCec, NULL)),
#if ENABLE_PIP
	/* The small picture's place and size, edited by dragging a widget rather
	   than as items, so the nine rows below share the label of the item that
	   opens it, and the pair for radio carries the same one again: the widget
	   edits whichever pair the box is in. Only where the box can show a small
	   picture.

	   The ceiling of each is the OSD's own width or height, which the box asks
	   the framebuffer for. Every backend offers at most 1920 by 1080, so those
	   are the widest a value can be. */
	intRow("pip_x")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(0, 1920)
		.defaultValue(50)
		.field(COREAPI_NUMBER_FIELD_ON(pip_x, pipUsable, NULL)),
	intRow("pip_y")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(0, 1080)
		.defaultValue(50)
		.field(COREAPI_NUMBER_FIELD_ON(pip_y, pipUsable, NULL)),
	intRow("pip_width")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(0, 1920)
		.defaultValue(365)
		.field(COREAPI_NUMBER_FIELD_ON(pip_width, pipUsable, NULL)),
	intRow("pip_height")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(0, 1080)
		.defaultValue(200)
		.field(COREAPI_NUMBER_FIELD_ON(pip_height, pipUsable, NULL)),
	// The radio pair falls back to the television one rather than to a number.
	intRow("pip_radio_x")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(0, 1920)
		.defaultValue(50)
		.field(COREAPI_NUMBER_FIELD_ON(pip_radio_x, pipUsable, NULL)),
	intRow("pip_radio_y")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(0, 1080)
		.defaultValue(50)
		.field(COREAPI_NUMBER_FIELD_ON(pip_radio_y, pipUsable, NULL)),
	intRow("pip_radio_width")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(0, 1920)
		.defaultValue(365)
		.field(COREAPI_NUMBER_FIELD_ON(pip_radio_width, pipUsable, NULL)),
	intRow("pip_radio_height")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(0, 1080)
		.defaultValue(200)
		.field(COREAPI_NUMBER_FIELD_ON(pip_radio_height, pipUsable, NULL)),
	/* Which corner the small picture was last moved to. The floor is the value
	   that means no corner at all and the rest are the four. */
	intRow("pip_rotate_lastpos")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(-1, 3)
		.defaultValue(-1)
		.field(COREAPI_NUMBER_FIELD_ON(pip_rotate_lastpos, pipUsable, NULL)),
#endif
	/* The video mode, out of the table of the family the build is for. The
	   numbering is the driver's own enum and the families do not agree past
	   its twelfth entry, so each table is right for its family and for no
	   other. One more arm decides the default at run time out of the
	   environment, which no row states. */
	enumRow("video_Mode")
		.section("video")
		.label("videomenu.videomode")
		.hint("menu.hint_video_mode")
#if HAVE_ARM_HARDWARE
		.defaultValue(VIDEO_STD_1080P50)
#elif HAVE_CST_HARDWARE && defined(BOXMODEL_CST_HD2)
		.defaultValue(VIDEO_STD_1080P24)
#else
		.defaultValue(VIDEO_STD_720P50)
#endif
		.values(kVideoMode)
		.field(COREAPI_NUMBER_FIELD(video_Mode)),
	/* The two analog outputs. Which entries the box offers depends on its board
	   revision, see the lists above. Where a board has a SCART socket of its own
	   the first is offered under that label. */
	enumRow("analog_mode1")
		.section("video")
		.label("videomenu.analog_mode")
		.hint("menu.hint_video_analog_mode")
#if defined(BOXMODEL_CST_HD2) && defined(ANALOG_MODE)
		.defaultValue(ANALOG_MODE(BOTH, SD, RGB))
		.values(kAnalogHd2)
		.field(COREAPI_NUMBER_FIELD(analog_mode1)),
#else
#ifdef ANALOG_MODE
		.defaultValue(ANALOG_MODE(BOTH, SD, RGB))
#else
		.defaultValue(ANALOG_SD_RGB_SCART)
#endif
		.values(kAnalogOneItem)
		.field(COREAPI_NUMBER_FIELD_ON(analog_mode1, analogOneItem, &kAnalogMode1Scart)),
#endif
	enumRow("analog_mode2")
		.section("video")
		.label("videomenu.cinch")
		.hint("menu.hint_video_cinch_mode")
#ifdef ANALOG_MODE
		.defaultValue(ANALOG_MODE(CINCH, SD, YPRPB))
#else
		.defaultValue(ANALOG_SD_YPRPB_CINCH)
#endif
#if defined(BOXMODEL_CST_HD2) && defined(ANALOG_MODE)
		// This build has no Cinch list of its own.
		.values(kAnalogOneItem)
#else
		.values(kAnalogCinch)
#endif
		.field(COREAPI_NUMBER_FIELD_ON(analog_mode2, hasAnalogCinch, NULL)),
#ifdef BOXMODEL_CST_HD2
	/* The driver takes -128..127 for the three below; the picture stops
	   changing outside this narrower range. Contrast and saturation are
	   multiplied by three on the way to the driver, so the stored number is
	   the one shown rather than the one the driver is given. */
	intRow("brightness")
		.section("video")
		.label("videomenu.brightness")
		.hint("menu.hint_video_brightness")
		.range(-42, 42)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(brightness)),
	intRow("contrast")
		.section("video")
		.label("videomenu.contrast")
		.hint("menu.hint_video_contrast")
		.range(-42, 42)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(contrast)),
	intRow("saturation")
		.section("video")
		.label("videomenu.saturation")
		.hint("menu.hint_video_saturation")
		.range(-42, 42)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(saturation)),
	boolRow("enable_sd_osd")
		.section("video")
		.label("videomenu.sdosd")
		.hint("menu.hint_video_sdosd")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(enable_sd_osd)),
#endif
};

} // anonymous namespace

const Descriptor *settingsTableVideo(size_t &count)
{
	count = sizeof(kVideo) / sizeof(kVideo[0]);
	return kVideo;
}

const char *const *videoModeNames(size_t &count)
{
	count = sizeof(kVideoModeNames) / sizeof(kVideoModeNames[0]);
	return kVideoModeNames;
}

bool videoModeDrawn(size_t index)
{
	if (index >= sizeof(kVideoModeNames) / sizeof(kVideoModeNames[0]))
		return false;
	for (size_t i = 0; i < sizeof(kVideoMode) / sizeof(kVideoMode[0]); ++i)
	{
		if (kVideoMode[i].label_text != NULL && strcmp(kVideoMode[i].label_text, kVideoModeNames[index]) == 0)
			return true;
	}
	return false;
}

} // namespace coreapi
