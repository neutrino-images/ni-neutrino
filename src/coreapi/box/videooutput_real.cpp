/*
 * videooutput_real.cpp - the video groups' seam bound to the program's decoders
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

#include "coreapi/box/apply_video.h"

#include <cstdio>

#include <hardware/video.h>
#include <zapit/client/zapitclient.h>

#ifdef BOXMODEL_CST_HD2
#include <sys/ioctl.h>
#include <driver/framebuffer.h>
#include <cnxtfb.h>
#endif

extern cVideo *videoDecoder;
#if ENABLE_PIP
extern cVideo *pipVideoDecoder[3];
#endif
extern CZapitClient *g_Zapit;

namespace coreapi
{

namespace
{

class RealVideoOutput : public VideoOutput
{
	public:
		Status setVideoSystem(int system)
		{
			return applicationSetVideoSystem(system);
		}

		Status setAnalogMode(int mode)
		{
			if (!videoDecoder)
				return Status::Internal;
			videoDecoder->SetVideoMode((analog_mode_t) mode);
			return Status::Ok;
		}

		Status setAspect(int format, int mode43)
		{
			if (!videoDecoder)
				return Status::Internal;
			if (g_Zapit)
				g_Zapit->setMode43(mode43);
			videoDecoder->setAspectRatio(format, mode43);
#if ENABLE_PIP
			if (pipVideoDecoder[0] != NULL)
				pipVideoDecoder[0]->setAspectRatio(format, mode43);
#endif
			return Status::Ok;
		}

		Status setDbdr(int level)
		{
			if (!videoDecoder)
				return Status::Internal;
			videoDecoder->SetDBDR(level);
			return Status::Ok;
		}

		Status setAutoModes(const std::vector<int> &enabled)
		{
			if (!videoDecoder)
				return Status::Internal;
			int modes[VIDEO_STD_MAX + 1] = { 0 };
			for (size_t i = 0; i < enabled.size() && i < (size_t) VIDEO_STD_MAX + 1; i++)
				modes[i] = enabled[i];
			videoDecoder->SetAutoModes(modes);
			return Status::Ok;
		}

		Status setControl(int control, int value)
		{
			if (!videoDecoder)
				return Status::Internal;
			videoDecoder->SetControl(control, value);
			return Status::Ok;
		}

		Status setZappingMode(int mode)
		{
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
			if (!videoDecoder)
				return Status::Internal;
			videoDecoder->SetControl(VIDEO_CONTROL_ZAPPING_MODE, mode);
			return Status::Ok;
#else
			(void) mode;
			return Status::NotSupported;
#endif
		}

		Status currentVideoSystem(int &system) const
		{
			if (!videoDecoder)
				return Status::Internal;
			system = videoDecoder->GetVideoSystem();
			return Status::Ok;
		}

		Status scaleSdOsd(int on)
		{
#ifdef BOXMODEL_CST_HD2
			int val = on;
			if (ioctl(CFrameBuffer::getInstance()->getFileHandle(), FBIO_SCALE_SD_OSD, &val))
			{
				perror("FBIO_SCALE_SD_OSD");
				return Status::Internal;
			}
			return Status::Ok;
#else
			(void) on;
			return Status::NotSupported;
#endif
		}

		Status setHdmiColorimetry(int colorimetry)
		{
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
			if (!videoDecoder)
				return Status::Internal;
			videoDecoder->SetHDMIColorimetry((HDMI_COLORIMETRY) colorimetry);
			return Status::Ok;
#else
			(void) colorimetry;
			return Status::NotSupported;
#endif
		}

};

RealVideoOutput g_real_video_output;

} // namespace

void installRealVideoOutput()
{
	setVideoOutput(&g_real_video_output);
}

} // namespace coreapi
