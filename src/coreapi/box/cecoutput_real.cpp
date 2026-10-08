/*
 * cecoutput_real.cpp - the CEC group seam bound to the program video decoder
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

#include "coreapi/box/apply_cec.h"

#include <hardware/video.h>

extern cVideo *videoDecoder;

namespace coreapi
{

namespace
{

class RealCecLink : public CecLink
{
	public:
		Status setMode(int mode)
		{
			if (!videoDecoder)
				return Status::Internal;
			videoDecoder->SetCECMode((VIDEO_HDMI_CEC_MODE) mode);
			return Status::Ok;
		}

		Status setAutoStandby(int on)
		{
			if (!videoDecoder)
				return Status::Internal;
			videoDecoder->SetCECAutoStandby(on ? true : false);
			return Status::Ok;
		}

		Status setAutoView(int on)
		{
			if (!videoDecoder)
				return Status::Internal;
			videoDecoder->SetCECAutoView(on ? true : false);
			return Status::Ok;
		}

		bool takesAudioDestination() const
		{
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
			return true;
#else
			return false;
#endif
		}

		Status setAudioDestination(int destination)
		{
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
			if (!videoDecoder)
				return Status::Internal;
			videoDecoder->SetAudioDestination(destination);
			return Status::Ok;
#else
			(void) destination;
			return Status::NotSupported;
#endif
		}
};

RealCecLink g_real_cec_link;

} // namespace

void installRealCecLink()
{
	setCecLink(&g_real_cec_link);
}

} // namespace coreapi
