/*
 * apply_audio.cpp - what makes a changed audio setting take effect
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

#include "coreapi/box/apply_audio.h"
#include "coreapi/box/sentstate.h"

#include <system/settings.h>

#include <hardware/audio.h>

#include <vector>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

class NoAudioOutput : public AudioOutput
{
	public:
		Status setSrs(int, int, int, int) { return Status::NotSupported; }
		Status enableAnalogOut(int) { return Status::NotSupported; }
		Status setHdmiDolby(int) { return Status::NotSupported; }
		Status setSpdifDolby(int) { return Status::NotSupported; }
		Status setSyncMode(int) { return Status::NotSupported; }
		Status setAudioMode(int) { return Status::NotSupported; }
		Status setVolumePercent(int, int) { return Status::NotSupported; }
};

NoAudioOutput g_no_audio_output;
AudioOutput *g_audio_output = 0;

/* What the groups last put on the box. The surround enhancer and the pass
   through switches are hardware state nobody has shown to be harmless to write
   again with the value they hold, and the sync mode reaches five objects, so a
   run sends only what differs and a sibling key sends nothing of the others.

   The sync mode starts as sent: the decoders come up in AVSYNC_ENABLED, and the
   startup before the groups wrote it only when the setting was something else. */
struct AudioState
{
	Sent<std::vector<int> > srs;
	Sent<int> analog_out;
	Sent<int> hdmi_dolby;
	Sent<int> spdif_dolby;
	Sent<int> sync;
	AudioState() { sync.known = true; sync.value = AVSYNC_ENABLED; }
};

AudioState g_state;
// Nothing else writes this state, so nothing is ever marked or held.
SentFlags g_flags;

template <class T, class Call>
void send(Status &first, Sent<T> &sent, const T &v, Call call)
{
	sendChanged(first, g_flags, 0, sent, v, call);
}

Status runSrs()
{
	AudioOutput &out = audioOutput();
	Status first = Status::Ok;

	std::vector<int> srs(4);
	srs[0] = g_settings.srs_enable;
	srs[1] = g_settings.srs_nmgr_enable;
	srs[2] = g_settings.srs_algo;
	srs[3] = g_settings.srs_ref_volume;
	send(first, g_state.srs, srs, [&]() { return out.setSrs(srs[0], srs[1], srs[2], srs[3]); });
	return first;
}

// Stores two numbers in the channel daemon, so sending them twice is nothing.
Status runVolumePercent()
{
	return audioOutput().setVolumePercent(g_settings.audio_volume_percent_ac3, g_settings.audio_volume_percent_pcm);
}

Status runAudioMode()
{
	return audioOutput().setAudioMode(g_settings.audio_AnalogMode);
}

/* The order the program set these at startup: the pass through switches, the
   analog output, then the clock. */
Status runAudio()
{
	AudioOutput &out = audioOutput();
	Status first = Status::Ok;

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	const int hdmi = g_settings.ac3_pass ? 1 : 0;
	const int spdif = g_settings.dts_pass ? 1 : 0;
#else
	const int hdmi = g_settings.hdmi_dd;
	const int spdif = g_settings.spdif_dd ? 1 : 0;
#endif
	send(first, g_state.hdmi_dolby, hdmi, [&]() { return out.setHdmiDolby(hdmi); });
	send(first, g_state.spdif_dolby, spdif, [&]() { return out.setSpdifDolby(spdif); });

	const int analog = g_settings.analog_out ? 1 : 0;
	send(first, g_state.analog_out, analog, [&]() { return out.enableAnalogOut(analog); });

	const int sync = g_settings.avsync;
	send(first, g_state.sync, sync, [&]() { return out.setSyncMode(sync); });
	return first;
}

/* Every key a group answers for, whichever box declares it: a key this box lacks
   is never written, so listing it costs nothing. */
const char *const kSrsKeys[] =
{
	"srs_enable",
	"srs_algo",
	"srs_nmgr_enable",
	"srs_ref_volume"
};

const char *const kVolumePercentKeys[] =
{
	"audio_volume_percent_ac3",
	"audio_volume_percent_pcm"
};

const char *const kAudioKeys[] =
{
	"analog_out",
	"ac3_pass",
	"dts_pass",
	"hdmi_dd",
	"spdif_dd",
	"avsync"
};

const char *const kAudioModeKeys[] =
{
	"audio_AnalogMode"
};

} // namespace

AudioOutput &audioOutput()
{
	if (!g_audio_output)
		return g_no_audio_output;
	return *g_audio_output;
}

void setAudioOutput(AudioOutput *o) { g_audio_output = o; }

void resetSentAudio()
{
	g_state = AudioState();
	g_flags.reset();
}

/* After the decoders: CZapit::Start makes them, and the settings it starts them
   with are the ones the groups then confirm. */
const ApplyGroup kSrsApplyGroup = { "srs", ApplyPhase::Decoders, COREAPI_KEYS(kSrsKeys), &runSrs };

const ApplyGroup kVolumePercentApplyGroup = { "volumePercent", ApplyPhase::Decoders, COREAPI_KEYS(kVolumePercentKeys), &runVolumePercent };

const ApplyGroup kAudioApplyGroup = { "audio", ApplyPhase::Decoders, COREAPI_KEYS(kAudioKeys), &runAudio };

// The client of the channel daemon is made after the decoders phase.
const ApplyGroup kAudioModeApplyGroup = { "audioMode", ApplyPhase::Zapit, COREAPI_KEYS(kAudioModeKeys), &runAudioMode };

} // namespace coreapi
