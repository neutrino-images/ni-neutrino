/*
 * settingstable_audio.cpp - audio settings, one row per field
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

#include <hardware/audio.h>

namespace coreapi
{

namespace
{

/* The audio section. Two of the four pass through settings below are declared
   and the other two are not, and which two is not a choice made here: the
   program gives its settings struct one pair on one hardware and the other pair
   on the other, so a row for the pair this build does not have would not
   compile. */

// The floor of start_volume, shown in words.
const EnumValue kVolumeLastUsed[] =
{
	{ -1, "audiomenu.volume_last_used", NULL, NULL }
};

const EnumValue kAnalogMode[] =
{
	{ 0, "audiomenu.stereo", NULL, NULL },
	{ 1, "audiomenu.monoleft", NULL, NULL },
	{ 2, "audiomenu.monoright", NULL, NULL }
};

#if !(HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE)
const EnumValue kHdmiDD[] =
{
	{ HDMI_ENCODED_OFF, "options.off", NULL, NULL },
	{ HDMI_ENCODED_AUTO, "audiomenu.hdmi_dd_auto", NULL, NULL },
	{ HDMI_ENCODED_FORCED, "audiomenu.hdmi_dd_force", NULL, NULL }
};
#endif

const EnumValue kAvSync[] =
{
	{ AVSYNC_DISABLED, "options.off", NULL, NULL },
	{ AVSYNC_ENABLED, "options.on", NULL, NULL },
	{ AVSYNC_AUDIO_IS_MASTER, "audiomenu.avsync_am", NULL, NULL }
};

const EnumValue kSrsAlgo[] =
{
	{ 0, "audio.srs_algo_light", NULL, NULL },
	{ 1, "audio.srs_algo_normal", NULL, NULL },
#ifdef BOXMODEL_CST_HD2
	{ 2, "audio.srs_algo_heavy", NULL, NULL }
#endif
};

// srs_enable switches the three below, so a nonzero value is what makes them
// editable.
const Condition kSrsOn[] =
{
	{ "srs_enable", CompareOp::Ne, 0, NULL, 0 }
};

const Descriptor kAudio[] =
{
	{
		"audio_AnalogMode", ValueType::Enum, "audio",
		"audiomenu.analog_mode", "menu.hint_audio_analog_mode",
		0, 0, COREAPI_VALUES(kAnalogMode), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audio_AnalogMode)
	},
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	{
		"ac3_pass", ValueType::Bool, "audio",
		"audiomenu.ac3", "menu.hint_audio_ac3",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(ac3_pass)
	},
	{
		"dts_pass", ValueType::Bool, "audio",
		"audiomenu.dts", "menu.hint_audio_dts",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(dts_pass)
	},
#else
	{
		"hdmi_dd", ValueType::Enum, "audio",
		"audiomenu.hdmi_dd", "menu.hint_audio_hdmi_dd",
		0, 0, COREAPI_VALUES(kHdmiDD), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(hdmi_dd, hasHdmi, NULL)
	},
	{
		"spdif_dd", ValueType::Bool, "audio",
		"audiomenu.spdif_dd", "menu.hint_audio_spdif_dd",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(spdif_dd)
	},
#endif
	{
		"audio_DolbyDigital", ValueType::Bool, "audio",
		"audiomenu.dolbydigital", "menu.hint_audio_dd",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audio_DolbyDigital)
	},
	{
		"avsync", ValueType::Enum, "audio",
		"audiomenu.avsync", "menu.hint_audio_avsync",
		0, 0, COREAPI_VALUES(kAvSync), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(avsync)
	},
	{
		"current_volume_step", ValueType::Int, "audio",
		"audiomenu.volume_step", "menu.hint_audio_volstep",
		1, 25, NULL, 0, 5, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(current_volume_step)
	},
	/* The floor is the one value that is not a volume: it says the box keeps
	   whatever was last set.

	   The one row here a restart applies. The program reads it in the pass that
	   loads its settings and nowhere else, where it seeds the running volume,
	   so a change to it moves nothing until the box loads its settings again. */
	{
		"start_volume", ValueType::Int, "audio",
		"audiomenu.volume_start", "menu.hint_audio_volstart",
		-1, 100, COREAPI_VALUES(kVolumeLastUsed), -1, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(start_volume)
	},
	{
		"srs_enable", ValueType::Bool, "audio",
		"audio.srs_iq", "menu.hint_audio_srs",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(srs_enable)
	},
	{
		"srs_algo", ValueType::Enum, "audio",
		"audio.srs_algo", "menu.hint_audio_srs_algo",
		0, 0, COREAPI_VALUES(kSrsAlgo), 1, NULL, false, false, COREAPI_CONDITIONS(kSrsOn),
		COREAPI_NUMBER_FIELD(srs_algo)
	},
#ifndef BOXMODEL_CST_HD2
	{
		"srs_nmgr_enable", ValueType::Bool, "audio",
		"audio.srs_nmgr", "menu.hint_audio_srs_nmgr",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kSrsOn),
		COREAPI_NUMBER_FIELD(srs_nmgr_enable)
	},
#endif
	{
		"srs_ref_volume", ValueType::Int, "audio",
		"audio.srs_volume", "menu.hint_audio_srs_volume",
		1, 100, NULL, 0, 75, NULL, false, false, COREAPI_CONDITIONS(kSrsOn),
		COREAPI_NUMBER_FIELD(srs_ref_volume)
	},
	{
		"audio_volume_percent_ac3", ValueType::Int, "audio",
		"audiomenu.volume_adjustment_ac3", "menu.hint_audio_adjust_vol_ac3",
		0, 100, NULL, 0, 100, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audio_volume_percent_ac3)
	},
	{
		"audio_volume_percent_pcm", ValueType::Int, "audio",
		"audiomenu.volume_adjustment_pcm", "menu.hint_audio_adjust_vol_pcm",
		0, 100, NULL, 0, 100, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audio_volume_percent_pcm)
	},

	/* The volume the box is at, which it writes on every change and reads at
	   the next start where the start volume beside it says to keep it. */
	{
		"current_volume", ValueType::Int, "audio",
		NULL, NULL,
		0, 100, NULL, 0, 75, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(current_volume)
	},
	// Switches the analogue output on and off.
	{
		"analog_out", ValueType::Bool, "audio",
		"audiomenu.analog_out", NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(analog_out)
	},
};

} // anonymous namespace

const Descriptor *settingsTableAudio(size_t &count)
{
	count = sizeof(kAudio) / sizeof(kAudio[0]);
	return kAudio;
}

} // namespace coreapi
