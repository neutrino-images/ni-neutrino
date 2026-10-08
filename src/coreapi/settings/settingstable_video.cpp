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
#include "videomodes.h"

#include <hardware/video.h>

namespace coreapi
{

namespace
{

/* The video section. Five of this screen's number choosers sit in an arm the
   preprocessor drops, and the fields behind them are loaded, saved and edited
   by another screen that carries no bound of its own. Those rows cite the
   dropped call, because it is the only statement of a bound there is. */

const EnumValue kVideoFormat[] =
{
	{ DISPLAY_AR_4_3, "videomenu.videoformat_43", NULL, NULL },
	{ DISPLAY_AR_16_9, "videomenu.videoformat_169", NULL, NULL },
	{ DISPLAY_AR_14_9, "videomenu.videoformat_149", NULL, canAspect149 }
};

const EnumValue kVideo43Mode[] =
{
	{ DISPLAY_AR_MODE_PANSCAN, "videomenu.panscan", NULL, NULL },
	{ DISPLAY_AR_MODE_PANSCAN2, "videomenu.panscan2", NULL, canPanScan149 },
	{ DISPLAY_AR_MODE_LETTERBOX, "videomenu.letterbox", NULL, NULL },
	{ DISPLAY_AR_MODE_NONE, "videomenu.fullscreen", NULL, NULL }
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
const EnumValue kVideoMode[] =
{
#if COREAPI_VIDEOMODES == COREAPI_VIDEOMODES_CST_HD1
	{ VIDEO_STD_NTSC, NULL, MODE_NTSC, NULL },
	{ VIDEO_STD_PAL, NULL, MODE_PAL, NULL },
	{ VIDEO_STD_SECAM, NULL, MODE_SECAM, NULL },
	{ VIDEO_STD_480P, NULL, MODE_M480P, NULL },
	{ VIDEO_STD_576P, NULL, MODE_M576P, NULL },
	{ VIDEO_STD_720P50, NULL, MODE_M720P_50HZ, NULL },
	{ VIDEO_STD_720P60, NULL, MODE_M720P_60HZ, NULL },
	{ VIDEO_STD_1080I50, NULL, MODE_M1080I_50HZ, NULL },
	{ VIDEO_STD_1080I60, NULL, MODE_M1080I_60HZ, NULL },
	{ VIDEO_STD_1080P24, NULL, MODE_M1080P_24HZ, NULL },
	{ VIDEO_STD_1080P25, NULL, MODE_M1080P_25HZ, NULL },
	{ VIDEO_STD_AUTO, NULL, MODE_AUTO, NULL }
#elif COREAPI_VIDEOMODES == COREAPI_VIDEOMODES_CST_HD2
	{ VIDEO_STD_NTSC, NULL, MODE_NTSC, NULL },
	{ VIDEO_STD_PAL, NULL, MODE_PAL, NULL },
	{ VIDEO_STD_SECAM, NULL, MODE_SECAM, NULL },
	{ VIDEO_STD_480P, NULL, MODE_M480P, NULL },
	{ VIDEO_STD_576P, NULL, MODE_M576P, NULL },
	{ VIDEO_STD_720P50, NULL, MODE_M720P_50HZ, NULL },
	{ VIDEO_STD_720P60, NULL, MODE_M720P_60HZ, NULL },
	{ VIDEO_STD_1080I50, NULL, MODE_M1080I_50HZ, NULL },
	{ VIDEO_STD_1080I60, NULL, MODE_M1080I_60HZ, NULL },
	{ VIDEO_STD_1080P2397, NULL, MODE_M1080P_2397HZ, NULL },
	{ VIDEO_STD_1080P24, NULL, MODE_M1080P_24HZ, NULL },
	{ VIDEO_STD_1080P25, NULL, MODE_M1080P_25HZ, NULL },
	{ VIDEO_STD_1080P2997, NULL, MODE_M1080P_2997HZ, NULL },
	{ VIDEO_STD_1080P50, NULL, MODE_M1080P_50HZ, NULL },
	{ VIDEO_STD_1080P60, NULL, MODE_M1080P_60HZ, NULL },
	{ VIDEO_STD_AUTO, NULL, MODE_AUTO, NULL }
#elif COREAPI_VIDEOMODES == COREAPI_VIDEOMODES_4K
	{ VIDEO_STD_PAL, NULL, MODE_PAL, NULL },
	{ VIDEO_STD_576P, NULL, MODE_M576P, NULL },
	{ VIDEO_STD_720P50, NULL, MODE_M720P_50HZ, NULL },
	{ VIDEO_STD_720P60, NULL, MODE_M720P_60HZ, NULL },
	{ VIDEO_STD_1080I50, NULL, MODE_M1080I_50HZ, NULL },
	{ VIDEO_STD_1080I60, NULL, MODE_M1080I_60HZ, NULL },
	{ VIDEO_STD_1080P24, NULL, MODE_M1080P_24HZ, NULL },
	{ VIDEO_STD_1080P25, NULL, MODE_M1080P_25HZ, NULL },
	{ VIDEO_STD_1080P50, NULL, MODE_M1080P_50HZ, NULL },
	{ VIDEO_STD_2160P24, NULL, MODE_M2160P_24HZ, NULL },
	{ VIDEO_STD_2160P25, NULL, MODE_M2160P_25HZ, NULL },
	{ VIDEO_STD_2160P30, NULL, MODE_M2160P_30HZ, NULL },
	{ VIDEO_STD_2160P50, NULL, MODE_M2160P_50HZ, NULL }
#elif COREAPI_VIDEOMODES == COREAPI_VIDEOMODES_OSMIO4K
	{ VIDEO_STD_PAL, NULL, MODE_PAL, NULL },
	{ VIDEO_STD_576P, NULL, MODE_M576P, NULL },
	{ VIDEO_STD_720P50, NULL, MODE_M720P_50HZ, NULL },
	{ VIDEO_STD_720P60, NULL, MODE_M720P_60HZ, NULL },
	{ VIDEO_STD_1080I50, NULL, MODE_M1080I_50HZ, NULL },
	{ VIDEO_STD_1080I60, NULL, MODE_M1080I_60HZ, NULL },
	{ VIDEO_STD_1080P24, NULL, MODE_M1080P_24HZ, NULL },
	{ VIDEO_STD_1080P25, NULL, MODE_M1080P_25HZ, NULL },
	{ VIDEO_STD_1080P50, NULL, MODE_M1080P_50HZ, NULL },
	{ VIDEO_STD_1080P60, NULL, MODE_M1080P_60HZ, NULL },
	{ VIDEO_STD_2160P24, NULL, MODE_M2160P_24HZ, NULL },
	{ VIDEO_STD_2160P25, NULL, MODE_M2160P_25HZ, NULL },
	{ VIDEO_STD_2160P30, NULL, MODE_M2160P_30HZ, NULL },
	{ VIDEO_STD_2160P50, NULL, MODE_M2160P_50HZ, NULL }
#else
	// A PC build: 480, 576, 720 and 1080 lines.
	{ VIDEO_STD_NTSC, NULL, MODE_NTSC, NULL },
	{ VIDEO_STD_PAL, NULL, MODE_PAL, NULL },
	{ VIDEO_STD_720P50, NULL, MODE_M720P_50HZ, NULL },
	{ VIDEO_STD_720P60, NULL, MODE_M720P_60HZ, NULL },
	{ VIDEO_STD_1080I50, NULL, MODE_M1080I_50HZ, NULL }
#endif
};

const EnumValue kDbDr[] =
{
	{ 0, "videomenu.dbdr_none", NULL, NULL },
	{ 1, "videomenu.dbdr_deblock", NULL, NULL },
	{ 2, "videomenu.dbdr_both", NULL, NULL }
};

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
const EnumValue kZappingMode[] =
{
	{ 0, "videomenu.zappingmode_mute", NULL, NULL },
	{ 1, "videomenu.zappingmode_hold", NULL, NULL },
	{ 2, "videomenu.zappingmode_mutetilllock", NULL, NULL },
	{ 3, "videomenu.zappingmode_holdtilllock", NULL, NULL }
};

/* The same stored number names another colorimetry on the two arms. Numbers
   and not the HDMI_COLORIMETRY names, which the table checks compile against
   libraries that do not declare; they are the values those names have. */
const EnumValue kHdmiColorimetry[] =
{
#if BOXMODEL_VUPLUS_ARM
	{ 0, "videomenu.hdmi_colorimetry_auto", NULL, NULL },
	{ 1, "videomenu.hdmi_colorimetry_bt709", NULL, NULL },
	{ 2, "videomenu.hdmi_colorimetry_bt470", NULL, NULL }
#else
	{ 0, "videomenu.hdmi_colorimetry_auto", NULL, NULL },
	{ 1, "videomenu.hdmi_colorimetry_bt2020ncl", NULL, NULL },
	{ 2, "videomenu.hdmi_colorimetry_bt2020cl", NULL, NULL },
	{ 3, "videomenu.hdmi_colorimetry_bt709", NULL, NULL }
#endif
};
#endif

// Each hardware library numbers the modes its own way.
const EnumValue kCecMode[] =
{
	{ VIDEO_HDMI_CEC_MODE_OFF, "videomenu.hdmi_cec_mode_off", NULL, NULL },
	{ VIDEO_HDMI_CEC_MODE_TUNER, "videomenu.hdmi_cec_mode_tuner", NULL, NULL },
	{ VIDEO_HDMI_CEC_MODE_RECORDER, "videomenu.hdmi_cec_mode_recorder", NULL, NULL }
};

// The numbers are the ones of VIDEO_HDMI_CEC_VOL in src/driver/hdmi_cec.h, the
// same in every hardware library that names them; not every library does.
const EnumValue kCecVolume[] =
{
	{ 0, "videomenu.hdmi_cec_vol_off", NULL, NULL },
	{ 1, "videomenu.hdmi_cec_vol_audiosystem", NULL, NULL },
	{ 2, "videomenu.hdmi_cec_vol_tv", NULL, NULL }
};

// The three below the mode are offered only while the link is on.
const Condition kCecOn[] =
{
	{ "hdmi_cec_mode", CompareOp::Ne, 0, NULL, 0 }
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
	{ ANALOG_OUT_SCART_SD_RGB, "videomenu.analog_sd_rgb_scart", NULL, t }, \
	{ ANALOG_OUT_CINCH_SD_RGB, "videomenu.analog_sd_rgb_cinch", NULL, t }, \
	{ ANALOG_OUT_SCART_SD_YPRPB, "videomenu.analog_sd_yprpb_scart", NULL, t }, \
	{ ANALOG_OUT_CINCH_SD_YPRPB, "videomenu.analog_sd_yprpb_cinch", NULL, t }, \
	{ ANALOG_OUT_SCART_HD_RGB, "videomenu.analog_hd_rgb_scart", NULL, t }, \
	{ ANALOG_OUT_CINCH_HD_RGB, "videomenu.analog_hd_rgb_cinch", NULL, t }, \
	{ ANALOG_OUT_SCART_HD_YPRPB, "videomenu.analog_hd_yprpb_scart", NULL, t }, \
	{ ANALOG_OUT_CINCH_HD_YPRPB, "videomenu.analog_hd_yprpb_cinch", NULL, t }

#if defined(BOXMODEL_CST_HD2) && defined(ANALOG_MODE)
// One list: the newer boards take the combined modes, revision 6 the one-item list.
const EnumValue kAnalogHd2[] =
{
	{ ANALOG_MODE(BOTH, xD, AUTO), "videomenu.analog_auto", NULL, analogOutputsSplit },
	{ ANALOG_MODE(BOTH, xD, CVBS), "videomenu.analog_cvbs", NULL, analogOutputsSplit },
	{ ANALOG_MODE(BOTH, SD, RGB), "videomenu.analog_sd_rgb", NULL, analogOutputsSplit },
	{ ANALOG_MODE(BOTH, SD, YPRPB), "videomenu.analog_sd_yprpb", NULL, analogOutputsSplit },
	{ ANALOG_MODE(BOTH, HD, RGB), "videomenu.analog_hd_rgb", NULL, analogOutputsSplit },
	{ ANALOG_MODE(BOTH, HD, YPRPB), "videomenu.analog_hd_yprpb", NULL, analogOutputsSplit },
	ANALOG_ONE_ITEM(analogOneItem)
};

const EnumValue kAnalogOneItem[] = { ANALOG_ONE_ITEM(analogOneItem) };
#else
const EnumValue kAnalogOneItem[] = { ANALOG_ONE_ITEM(NULL) };

// A SCART socket of its own, the SD entries on more boards than the HD ones.
const EnumValue kAnalogScart[] =
{
	{ ANALOG_OUT_SCART_SD_RGB, "videomenu.analog_sd_rgb_scart", NULL, scartSdOffered },
	{ ANALOG_OUT_SCART_SD_YPRPB, "videomenu.analog_sd_yprpb_scart", NULL, scartSdOffered },
	{ ANALOG_OUT_SCART_HD_RGB, "videomenu.analog_hd_rgb_scart", NULL, scartHdOffered },
	{ ANALOG_OUT_SCART_HD_YPRPB, "videomenu.analog_hd_yprpb_scart", NULL, scartHdOffered }
};

const Shape kAnalogMode1Scart = { ValueType::Enum, "videomenu.scart", 0, 0, kAnalogScart,
				  sizeof(kAnalogScart) / sizeof(kAnalogScart[0]), "menu.hint_video_scart_mode" };

const EnumValue kAnalogCinch[] =
{
	{ ANALOG_OUT_CINCH_SD_RGB, "videomenu.analog_sd_rgb_cinch", NULL, NULL },
	{ ANALOG_OUT_CINCH_SD_YPRPB, "videomenu.analog_sd_yprpb_cinch", NULL, NULL },
	{ ANALOG_OUT_CINCH_HD_RGB, "videomenu.analog_hd_rgb_cinch", NULL, NULL },
	{ ANALOG_OUT_CINCH_HD_YPRPB, "videomenu.analog_hd_yprpb_cinch", NULL, NULL }
};
#endif

const Descriptor kVideo[] =
{
	{
		"video_Format", ValueType::Enum, "video",
		"videomenu.videoformat", "menu.hint_video_format",
		0, 0, COREAPI_VALUES(kVideoFormat), DISPLAY_AR_16_9, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(video_Format)
	},
	// How a 4:3 picture is put on a wide screen.
	{
		"video_43mode", ValueType::Enum, "video",
		"videomenu.43mode", "menu.hint_video_43mode",
		0, 0, COREAPI_VALUES(kVideo43Mode), DISPLAY_AR_MODE_LETTERBOX, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(video_43mode)
	},
	// Offered only off Coolstream, which is a revision read at run time.
	{
		"video_dbdr", ValueType::Enum, "video",
		"videomenu.dbdr", "menu.hint_video_dbdr",
		0, 0, COREAPI_VALUES(kDbDr), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(video_dbdr)
	},
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	// One box model falls back to 2 instead, which is a model read at run time
	// and not a constant this can carry.
	{
		"zappingmode", ValueType::Enum, "video",
		"videomenu.zappingmode", "menu.hint_video_zappingmode",
		0, 0, COREAPI_VALUES(kZappingMode), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(zappingmode, takesZappingMode, NULL)
	},
	{
		"hdmi_colorimetry", ValueType::Enum, "video",
		"videomenu.hdmi_colorimetry", "menu.hint_video_hdmi_colorimetry",
		0, 0, COREAPI_VALUES(kHdmiColorimetry), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(hdmi_colorimetry, takesHdmiColorimetry, NULL)
	},
	/* The five below: the four values are held to 0..255 and no bound is stated
	   for the step. */
	{
		"video_psi_brightness", ValueType::Int, "video",
		"videomenu.psi.brightness", "menu.hint_video_brightness",
		0, 255, NULL, 0, 128, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(psi_brightness)
	},
	{
		"video_psi_contrast", ValueType::Int, "video",
		"videomenu.psi.contrast", "menu.hint_video_contrast",
		0, 255, NULL, 0, 128, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(psi_contrast)
	},
	{
		"video_psi_saturation", ValueType::Int, "video",
		"videomenu.psi.saturation", "menu.hint_video_saturation",
		0, 255, NULL, 0, 128, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(psi_saturation)
	},
	{
		"video_psi_tint", ValueType::Int, "video",
		"videomenu.psi.tint", "menu.hint_video_tint",
		0, 255, NULL, 0, 128, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(psi_tint)
	},
	// How far one press moves the four above, not a value of its own.
	{
		"video_psi_step", ValueType::Int, "video",
		"videomenu.psi.step", "menu.hint_video_psi_step",
		1, 100, NULL, 0, 2, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(psi_step)
	},
#endif
	// The HDMI link, only where the box can drive it.
	{
		"hdmi_cec_mode", ValueType::Enum, "video",
		"videomenu.hdmi_cec_mode", "menu.hint_cec_mode",
		0, 0, COREAPI_ENUM(kCecMode), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(hdmi_cec_mode, canCec, NULL)
	},
	{
		"hdmi_cec_view_on", ValueType::Bool, "video",
		"videomenu.hdmi_cec_view_on", "menu.hint_cec_view_on",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kCecOn),
		COREAPI_NUMBER_FIELD_ON(hdmi_cec_view_on, canCec, NULL)
	},
	{
		"hdmi_cec_standby", ValueType::Bool, "video",
		"videomenu.hdmi_cec_standby", "menu.hint_cec_standby",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kCecOn),
		COREAPI_NUMBER_FIELD_ON(hdmi_cec_standby, canCec, NULL)
	},
	{
		"hdmi_cec_volume", ValueType::Enum, "video",
		"videomenu.hdmi_cec_volume", "menu.hint_cec_volume",
		0, 0, COREAPI_ENUM(kCecVolume), 0, NULL, false, false, COREAPI_CONDITIONS(kCecOn),
		COREAPI_NUMBER_FIELD_ON(hdmi_cec_volume, canCec, NULL)
	},
#if ENABLE_PIP
	/* The small picture's place and size, edited by dragging a widget rather
	   than as items, so the nine rows below share the label of the item that
	   opens it, and the pair for radio carries the same one again: the widget
	   edits whichever pair the box is in. Only where the box can show a small
	   picture.

	   The ceiling of each is the OSD's own width or height, which the box asks
	   the framebuffer for. Every backend offers at most 1920 by 1080, so those
	   are the widest a value can be. */
	{
		"pip_x", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		0, 1920, NULL, 0, 50, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(pip_x, pipUsable, NULL)
	},
	{
		"pip_y", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		0, 1080, NULL, 0, 50, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(pip_y, pipUsable, NULL)
	},
	{
		"pip_width", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		0, 1920, NULL, 0, 365, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(pip_width, pipUsable, NULL)
	},
	{
		"pip_height", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		0, 1080, NULL, 0, 200, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(pip_height, pipUsable, NULL)
	},
	// The radio pair falls back to the television one rather than to a number.
	{
		"pip_radio_x", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		0, 1920, NULL, 0, 50, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(pip_radio_x, pipUsable, NULL)
	},
	{
		"pip_radio_y", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		0, 1080, NULL, 0, 50, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(pip_radio_y, pipUsable, NULL)
	},
	{
		"pip_radio_width", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		0, 1920, NULL, 0, 365, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(pip_radio_width, pipUsable, NULL)
	},
	{
		"pip_radio_height", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		0, 1080, NULL, 0, 200, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(pip_radio_height, pipUsable, NULL)
	},
	/* Which corner the small picture was last moved to. The floor is the value
	   that means no corner at all and the rest are the four. */
	{
		"pip_rotate_lastpos", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		-1, 3, NULL, 0, -1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(pip_rotate_lastpos, pipUsable, NULL)
	},
#endif
	/* The video mode, out of the table of the family the build is for. The
	   numbering is the driver's own enum and the families do not agree past
	   its twelfth entry, so each table is right for its family and for no
	   other. One more arm decides the default at run time out of the
	   environment, which no row states. */
	{
		"video_Mode", ValueType::Enum, "video",
		"videomenu.videomode", "menu.hint_video_mode",
		0, 0, COREAPI_VALUES(kVideoMode),
#if HAVE_ARM_HARDWARE
		VIDEO_STD_1080P50,
#elif HAVE_CST_HARDWARE && defined(BOXMODEL_CST_HD2)
		VIDEO_STD_1080P24,
#else
		VIDEO_STD_720P50,
#endif
		NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(video_Mode)
	},
	/* The two analog outputs. Which entries the box offers depends on its board
	   revision, see the lists above. Where a board has a SCART socket of its own
	   the first is offered under that label. */
	{
		"analog_mode1", ValueType::Enum, "video",
		"videomenu.analog_mode", "menu.hint_video_analog_mode",
#if defined(BOXMODEL_CST_HD2) && defined(ANALOG_MODE)
		0, 0, COREAPI_VALUES(kAnalogHd2),
		ANALOG_MODE(BOTH, SD, RGB),
		NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(analog_mode1)
#else
		0, 0, COREAPI_VALUES(kAnalogOneItem),
#ifdef ANALOG_MODE
		ANALOG_MODE(BOTH, SD, RGB),
#else
		ANALOG_SD_RGB_SCART,
#endif
		NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(analog_mode1, analogOneItem, &kAnalogMode1Scart)
#endif
	},
	{
		"analog_mode2", ValueType::Enum, "video",
		"videomenu.cinch", "menu.hint_video_cinch_mode",
#if defined(BOXMODEL_CST_HD2) && defined(ANALOG_MODE)
		// This build has no Cinch list of its own.
		0, 0, COREAPI_VALUES(kAnalogOneItem),
#else
		0, 0, COREAPI_VALUES(kAnalogCinch),
#endif
#ifdef ANALOG_MODE
		ANALOG_MODE(CINCH, SD, YPRPB),
#else
		ANALOG_SD_YPRPB_CINCH,
#endif
		NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(analog_mode2, hasAnalogCinch, NULL)
	},
#ifdef BOXMODEL_CST_HD2
	/* The driver takes -128..127 for the three below; the picture stops
	   changing outside this narrower range. Contrast and saturation are
	   multiplied by three on the way to the driver, so the stored number is
	   the one shown rather than the one the driver is given. */
	{
		"brightness", ValueType::Int, "video",
		"videomenu.brightness", "menu.hint_video_brightness",
		-42, 42, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(brightness)
	},
	{
		"contrast", ValueType::Int, "video",
		"videomenu.contrast", "menu.hint_video_contrast",
		-42, 42, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(contrast)
	},
	{
		"saturation", ValueType::Int, "video",
		"videomenu.saturation", "menu.hint_video_saturation",
		-42, 42, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(saturation)
	},
	{
		"enable_sd_osd", ValueType::Bool, "video",
		"videomenu.sdosd", "menu.hint_video_sdosd",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(enable_sd_osd)
	},
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

} // namespace coreapi
