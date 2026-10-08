/*
 * apply_audio.h - what makes a changed audio setting take effect
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

#ifndef __coreapi_apply_audio_h__
#define __coreapi_apply_audio_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

namespace coreapi
{

/* What the audio groups tell the audio decoder, the demuxes that follow its
   clock and the channel daemon. One call per thing they take, numbers as the
   drivers number them, so the groups decide what is sent and when and this only
   carries it. Nothing here asks anybody anything.

   A seam rather than the drivers themselves, so the groups can run where no
   decoder exists. */
struct AudioOutput
{
	virtual ~AudioOutput() {}

	// The surround enhancer: on or off, the noise manager, the algorithm and the reference level.
	virtual Status setSrs(int enable, int noise_manager, int algorithm, int reference_volume) = 0;
	virtual Status enableAnalogOut(int on) = 0;
	/* The Dolby Digital pass through to the HDMI and to the optical output. A
	   box of the newer family has one switch per output, the older one a mode
	   for the HDMI, and the value is whichever the box's setting holds. */
	virtual Status setHdmiDolby(int value) = 0;
	virtual Status setSpdifDolby(int value) = 0;
	// The audio and video decoders and the three demuxes feeding them, which share one clock.
	virtual Status setSyncMode(int mode) = 0;
	// Stereo, left or right of a two-channel programme, kept by the channel daemon for the channel.
	virtual Status setAudioMode(int mode) = 0;
	/* The volume percent the channel daemon starts a channel with, for a Dolby
	   Digital and for a PCM stream, until the channel has one of its own. */
	virtual Status setVolumePercent(int ac3, int pcm) = 0;
};

/* NotSupported for every call while nothing is installed, so a group run
   before the decoders exist fails and says so instead of touching nothing. */
AudioOutput &audioOutput();
void setAudioOutput(AudioOutput *o);

// Binds the accessor above to the program's decoders and channel daemon.
void installRealAudioOutput();

// Forgets what was sent, for a case that needs the first run again.
void resetSentAudio();

/* The surround enhancer's four settings. */
extern const ApplyGroup kSrsApplyGroup;

/* The percent a channel starts at, for Dolby Digital and for PCM. */
extern const ApplyGroup kVolumePercentApplyGroup;

/* The analog output, the pass through switches and the clock the decoders share.
   After the decoders exist. */
extern const ApplyGroup kAudioApplyGroup;

/* The stereo or mono mode. Kept apart because it goes through the channel
   daemon's client, which exists only after the decoders phase. */
extern const ApplyGroup kAudioModeApplyGroup;

} // namespace coreapi

#endif
