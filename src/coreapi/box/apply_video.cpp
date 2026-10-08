/*
 * apply_video.cpp - what makes a changed video or picture setting take effect
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
#include "coreapi/box/sentstate.h"
#include "coreapi/settings/predicates.h"
#include "coreapi/settings/videomodes.h"

#include <system/settings.h>

#include <hardware/video.h>

#include <utility>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

class NoVideoOutput : public VideoOutput
{
	public:
		Status setVideoSystem(int) { return Status::NotSupported; }
		Status setAnalogMode(int) { return Status::NotSupported; }
		Status setAspect(int, int) { return Status::NotSupported; }
		Status setDbdr(int) { return Status::NotSupported; }
		Status setAutoModes(const std::vector<int> &) { return Status::NotSupported; }
		Status setControl(int, int) { return Status::NotSupported; }
		Status setZappingMode(int) { return Status::NotSupported; }
		Status currentVideoSystem(int &) const { return Status::NotSupported; }
		Status scaleSdOsd(int) { return Status::NotSupported; }
		Status setHdmiColorimetry(int) { return Status::NotSupported; }
};

NoVideoOutput g_no_video_output;
VideoOutput *g_video_output = 0;

/* The flags by the driver's number of each standard. The settings number them
   by the position of the mode's name, which is not the driver's number, and a
   mode this box does not draw has no number at all. The first family switches
   between the automatic modes, every other box between the enabled ones. */
std::vector<int> autoModes()
{
	std::vector<int> enabled(VIDEO_STD_MAX + 1, 0);
	for (size_t i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++)
	{
		const int standard = videoModeValue(i);
		if (standard < 0 || standard >= VIDEO_STD_MAX)
			continue;
#if COREAPI_VIDEOMODES == COREAPI_VIDEOMODES_CST_HD2
		enabled[standard] = g_settings.enabled_auto_modes[i];
#else
		enabled[standard] = g_settings.enabled_video_modes[i];
#endif
	}
	return enabled;
}

/* What the groups last put on the box, one entry per thing the drivers take
   (sentstate.h): a change of one key re-sends nothing else, and a hotkey or a
   sibling key does not rewrite the HDMI, analog or zapping state with the same
   value. */
struct VideoState
{
	Sent<int> system;
	Sent<int> analog1;
	Sent<int> analog2;
	Sent<std::pair<int, int> > aspect;
	Sent<int> dbdr;
	Sent<std::vector<int> > auto_modes;
	Sent<int> brightness;
	Sent<int> contrast;
	Sent<int> saturation;
	Sent<int> sd_osd;
	Sent<int> zapping;
	Sent<int> colorimetry;
	Sent<int> psi_contrast;
	Sent<int> psi_saturation;
	Sent<int> psi_brightness;
	Sent<int> psi_tint;
	// The standard on the box before the last run, -1 before any.
	int mode_before;
	VideoState() : mode_before(-1) {}
};

VideoState g_state;
SentFlags g_flags;

bool marked(unsigned mask, VideoSent what)
{
	return (mask & (unsigned) what) != 0;
}

// What other writers marked since the last run is sent again by this one.
void takeStale()
{
	const unsigned m = g_flags.take();
	if (marked(m, VideoSent::System))
		g_state.system.known = false;
	if (marked(m, VideoSent::Analog))
		g_state.analog1.known = g_state.analog2.known = false;
	if (marked(m, VideoSent::Aspect))
		g_state.aspect.known = false;
	if (marked(m, VideoSent::Dbdr))
		g_state.dbdr.known = false;
	if (marked(m, VideoSent::AutoModes))
		g_state.auto_modes.known = false;
	if (marked(m, VideoSent::Picture))
		g_state.brightness.known = g_state.contrast.known = g_state.saturation.known = g_state.sd_osd.known = false;
	if (marked(m, VideoSent::ZappingMode))
		g_state.zapping.known = false;
	if (marked(m, VideoSent::Colorimetry))
		g_state.colorimetry.known = false;
	if (marked(m, VideoSent::Psi))
		g_state.psi_contrast.known = g_state.psi_saturation.known = g_state.psi_brightness.known
			= g_state.psi_tint.known = false;
}

template <class T, class Call>
void send(Status &first, VideoSent what, Sent<T> &sent, const T &v, Call call)
{
	sendChanged(first, g_flags, (unsigned) what, sent, v, call);
}

/* The standard on the box before this run: what the group sent last, or where
   another writer changed it since or a send failed, what the decoder runs now.
   The question about a new mode puts this back. */
int modeOnTheBox(VideoOutput &out, int mode)
{
	if (g_state.system.known)
		return g_state.system.value;
	int now = -1;
	if (out.currentVideoSystem(now) == Status::Ok)
		return now;
	return mode;
}

/* In the order the program set these at startup: the standard first, since the
   rest is drawn in it. */
Status runVideo()
{
	takeStale();
	VideoOutput &out = videoOutput();
	Status first = Status::Ok;

	const int mode = g_settings.video_Mode;
	g_state.mode_before = modeOnTheBox(out, mode);
	send(first, VideoSent::System, g_state.system, mode, [&]() { return out.setVideoSystem(mode); });

	const int a1 = g_settings.analog_mode1;
	send(first, VideoSent::Analog, g_state.analog1, a1, [&]() { return out.setAnalogMode(a1); });
#ifndef BOXMODEL_CST_HD2
	// One list for both outputs on that revision, so the second is not its own.
	if (!analogOneItem())
	{
		const int a2 = g_settings.analog_mode2;
		send(first, VideoSent::Analog, g_state.analog2, a2, [&]() { return out.setAnalogMode(a2); });
	}
#endif

	const std::pair<int, int> aspect(g_settings.video_Format, g_settings.video_43mode);
	send(first, VideoSent::Aspect, g_state.aspect, aspect, [&]() { return out.setAspect(aspect.first, aspect.second); });
	const int dbdr = g_settings.video_dbdr;
	send(first, VideoSent::Dbdr, g_state.dbdr, dbdr, [&]() { return out.setDbdr(dbdr); });
	const std::vector<int> modes = autoModes();
	send(first, VideoSent::AutoModes, g_state.auto_modes, modes, [&]() { return out.setAutoModes(modes); });

#ifdef BOXMODEL_CST_HD2
	// The row stores what the menu shows; the driver takes three times that.
	const int b = g_settings.brightness, c = g_settings.contrast * 3, sa = g_settings.saturation * 3;
	const int sd = g_settings.enable_sd_osd;
	send(first, VideoSent::Picture, g_state.brightness, b, [&]() { return out.setControl(VIDEO_CONTROL_BRIGHTNESS, b); });
	send(first, VideoSent::Picture, g_state.contrast, c, [&]() { return out.setControl(VIDEO_CONTROL_CONTRAST, c); });
	send(first, VideoSent::Picture, g_state.saturation, sa, [&]() { return out.setControl(VIDEO_CONTROL_SATURATION, sa); });
	send(first, VideoSent::Picture, g_state.sd_osd, sd, [&]() { return out.scaleSdOsd(sd); });
#endif

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	const int zap = g_settings.zappingmode, col = g_settings.hdmi_colorimetry;
	send(first, VideoSent::ZappingMode, g_state.zapping, zap, [&]() { return out.setZappingMode(zap); });
	send(first, VideoSent::Colorimetry, g_state.colorimetry, col, [&]() { return out.setHdmiColorimetry(col); });
#endif

	return first;
}

Status runPsi()
{
	takeStale();
	Status first = Status::Ok;
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	VideoOutput &out = videoOutput();
	const int c = g_settings.psi_contrast, sa = g_settings.psi_saturation;
	const int b = g_settings.psi_brightness, t = g_settings.psi_tint;
	send(first, VideoSent::Psi, g_state.psi_contrast, c, [&]() { return out.setControl(VIDEO_CONTROL_CONTRAST, c); });
	send(first, VideoSent::Psi, g_state.psi_saturation, sa, [&]() { return out.setControl(VIDEO_CONTROL_SATURATION, sa); });
	send(first, VideoSent::Psi, g_state.psi_brightness, b, [&]() { return out.setControl(VIDEO_CONTROL_BRIGHTNESS, b); });
	send(first, VideoSent::Psi, g_state.psi_tint, t, [&]() { return out.setControl(VIDEO_CONTROL_HUE, t); });
#endif
	return first;
}

/* Every key the group answers for, whichever box declares it: a key this box
   lacks is never written, so listing it costs nothing. */
const char *const kVideoKeys[] =
{
	"video_Mode",
	"video_Format",
	"video_43mode",
	"video_dbdr",
	"analog_mode1",
	"analog_mode2",
	"zappingmode",
	"hdmi_colorimetry",
	"brightness",
	"contrast",
	"saturation",
	"enable_sd_osd",
	"enabled_video_mode_0",
	"enabled_video_mode_1",
	"enabled_video_mode_2",
	"enabled_video_mode_3",
	"enabled_video_mode_4",
	"enabled_video_mode_5",
	"enabled_video_mode_6",
	"enabled_video_mode_7",
	"enabled_video_mode_8",
	"enabled_video_mode_9",
	"enabled_video_mode_10",
	"enabled_video_mode_11",
	"enabled_video_mode_12",
	"enabled_video_mode_13",
	"enabled_video_mode_14",
	"enabled_video_mode_15",
	"enabled_video_mode_16",
	"enabled_video_mode_17",
	"enabled_video_mode_18",
	"enabled_video_mode_19",
	"enabled_auto_mode_0",
	"enabled_auto_mode_1",
	"enabled_auto_mode_2",
	"enabled_auto_mode_3",
	"enabled_auto_mode_4",
	"enabled_auto_mode_5",
	"enabled_auto_mode_6",
	"enabled_auto_mode_7",
	"enabled_auto_mode_8",
	"enabled_auto_mode_9",
	"enabled_auto_mode_10",
	"enabled_auto_mode_11",
	"enabled_auto_mode_12",
	"enabled_auto_mode_13",
	"enabled_auto_mode_14",
	"enabled_auto_mode_15",
	"enabled_auto_mode_16",
	"enabled_auto_mode_17",
	"enabled_auto_mode_18",
	"enabled_auto_mode_19"
};

const char *const kPsiKeys[] =
{
	"video_psi_contrast",
	"video_psi_saturation",
	"video_psi_brightness",
	"video_psi_tint"
};

} // namespace

VideoOutput &videoOutput()
{
	if (!g_video_output)
		return g_no_video_output;
	return *g_video_output;
}

void setVideoOutput(VideoOutput *o) { g_video_output = o; }

void forgetSentVideo(VideoSent what)
{
	g_flags.mark((unsigned) what);
}

void holdVideoState(VideoSent what, bool held)
{
	g_flags.hold((unsigned) what, held);
	/* Whoever holds it sets the state directly, so what was sent last is no longer on the
	   decoder; forgotten here, the run after the release sends the setting again even when
	   no run came in between. */
	if (held)
		g_flags.mark((unsigned) what);
}

int videoModeBeforeLastChange()
{
	return g_state.mode_before;
}

void resetSentVideo()
{
	g_state = VideoState();
	g_flags.reset();
}

/* After the decoders: CZapit::Start makes them, and the standard it starts them
   in is the one the group then confirms. */
const ApplyGroup kVideoApplyGroup = { "video", ApplyPhase::Decoders, COREAPI_KEYS(kVideoKeys), &runVideo };

const ApplyGroup kPsiApplyGroup = { "psi", ApplyPhase::Decoders, COREAPI_KEYS(kPsiKeys), &runPsi };

} // namespace coreapi
