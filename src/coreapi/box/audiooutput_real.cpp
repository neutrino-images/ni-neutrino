/*
 * audiooutput_real.cpp - the audio groups seam bound to the program decoders
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

#include <hardware/audio.h>
#include <hardware/dmx.h>
#include <hardware/video.h>
#include <zapit/client/zapitclient.h>
#include <zapit/zapit.h>

extern cAudio *audioDecoder;
extern cVideo *videoDecoder;
extern cDemux *videoDemux;
extern cDemux *audioDemux;
extern cDemux *pcrDemux;
extern CZapitClient *g_Zapit;

namespace coreapi
{

namespace
{

class RealAudioOutput : public AudioOutput
{
	public:
		Status setSrs(int enable, int noise_manager, int algorithm, int reference_volume)
		{
			if (!audioDecoder)
				return Status::Internal;
			audioDecoder->SetSRS(enable, noise_manager, algorithm, reference_volume);
			return Status::Ok;
		}

		Status enableAnalogOut(int on)
		{
			if (!audioDecoder)
				return Status::Internal;
			audioDecoder->EnableAnalogOut(on ? true : false);
			return Status::Ok;
		}

		Status setHdmiDolby(int value)
		{
			if (!audioDecoder)
				return Status::Internal;
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
			audioDecoder->SetHdmiDD(value ? true : false);
#else
			audioDecoder->SetHdmiDD((HDMI_ENCODED_MODE) value);
#endif
			return Status::Ok;
		}

		Status setSpdifDolby(int value)
		{
			if (!audioDecoder)
				return Status::Internal;
			audioDecoder->SetSpdifDD(value ? true : false);
			return Status::Ok;
		}

		Status setSyncMode(int mode)
		{
			if (!audioDecoder || !videoDecoder)
				return Status::Internal;
			const AVSYNC_TYPE m = (AVSYNC_TYPE) mode;
			videoDecoder->SetSyncMode(m);
			audioDecoder->SetSyncMode(m);
			if (videoDemux)
				videoDemux->SetSyncMode(m);
			if (audioDemux)
				audioDemux->SetSyncMode(m);
			if (pcrDemux)
				pcrDemux->SetSyncMode(m);
			return Status::Ok;
		}

		Status setAudioMode(int mode)
		{
			if (!g_Zapit)
				return Status::Internal;
			g_Zapit->setAudioMode(mode);
			return Status::Ok;
		}

		Status setVolumePercent(int ac3, int pcm)
		{
			CZapit::getInstance()->SetVolumePercent(ac3, pcm);
			return Status::Ok;
		}
};

RealAudioOutput g_real_audio_output;

} // namespace

void installRealAudioOutput()
{
	setAudioOutput(&g_real_audio_output);
}

} // namespace coreapi
