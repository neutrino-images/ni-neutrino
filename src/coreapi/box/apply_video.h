/*
 * apply_video.h - what makes a changed video or picture setting take effect
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

#ifndef __coreapi_apply_video_h__
#define __coreapi_apply_video_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

#include <vector>

namespace coreapi
{

/* What the video and picture groups tell the decoders. One call per thing the
   drivers take, numbers as the drivers number them, so the groups decide what
   is sent and when and this only carries it. Nothing here asks anybody
   anything: a web write has nobody in front of the television to answer.

   A seam rather than the drivers themselves, so the groups can run where no
   decoder exists. */
struct VideoOutput
{
	virtual ~VideoOutput() {}

	// The television standard, with the screen the box draws redrawn at the size that standard takes.
	virtual Status setVideoSystem(int system) = 0;
	virtual Status setAnalogMode(int mode) = 0;
	/* The shape of the main and the small picture, and the 4:3 mode the channel
	   daemon keeps for the next channel. The daemon is told only once it is up;
	   until then the decoder alone is, which is what it reads on the first zap. */
	virtual Status setAspect(int format, int mode43) = 0;
	virtual Status setDbdr(int level) = 0;
	// One flag per standard, indexed by the driver's number for it.
	virtual Status setAutoModes(const std::vector<int> &enabled) = 0;
	virtual Status setControl(int control, int value) = 0;
	virtual Status setZappingMode(int mode) = 0;
	virtual Status scaleSdOsd(int on) = 0;
	virtual Status setHdmiColorimetry(int colorimetry) = 0;
	// The standard the decoder runs now, whoever put it there.
	virtual Status currentVideoSystem(int &system) const = 0;
};

/* NotSupported for every call while nothing is installed, so a group run
   before the decoders exist fails and says so instead of touching nothing. */
VideoOutput &videoOutput();
void setVideoOutput(VideoOutput *o);

// Binds the accessor above to the program's decoders and channel daemon.
void installRealVideoOutput();

/* The things the video groups put on the box, each kept as what was last sent so
   a run sends only what differs: redrawing at a standard clears the screen, and
   rewriting the HDMI, analog or zapping state with the same value is nothing
   anybody has shown to be harmless on every driver. */
enum class VideoSent : unsigned
{
	System      = 1u << 0,
	Analog      = 1u << 1,
	Aspect      = 1u << 2,
	Dbdr        = 1u << 3,
	AutoModes   = 1u << 4,
	Picture     = 1u << 5,
	ZappingMode = 1u << 6,
	Colorimetry = 1u << 7,
	Psi         = 1u << 8
};

/* Something other than the groups changed that state on the box, so the next run
   sends it again whatever was sent before. Any thread may call it. Every writer
   of the same driver state outside the groups calls it: the channel daemon's own
   standard and aspect commands and the old web interface's aspect call. Not the
   automatic switching of the standard in automatic mode, which changes what the
   decoder runs and not the setting: sending the setting again would undo it.
   The daemon's 4:3 command marks the aspect only where its value is not the
   setting's, since the group sends the setting through that command itself. */
void forgetSentVideo(VideoSent what);

/* Keeps a state for whoever holds it, standby's zapping mode for one: while
   held the groups send nothing for it, and once let go the next run sends the
   setting again. On the loop only. */
void holdVideoState(VideoSent what, bool held);

/* The standard on the box before the group's last run, the same as now when that
   run did not change it and -1 before any run: what the group sent last, or
   what the decoder ran where another writer had changed it. What a question
   about a new mode puts back, so a mode written from elsewhere in between is
   what returns. */
int videoModeBeforeLastChange();

// Forgets what was sent and every hold, for a case that needs the first run again.
void resetSentVideo();

/* Defined by the application, because the size the box draws at belongs to an
   object whose header reaches the GUI and this layer must not: puts the standard
   on the decoder and redraws the screen at the size it takes, without asking. */
Status applicationSetVideoSystem(int system);

/* The video standard, the analog outputs, the picture shape, the deblocking,
   the standards the box may switch to by itself, the picture controls of the
   first family and the zapping mode and colorimetry of the newer ones. */
extern const ApplyGroup kVideoApplyGroup;

// Contrast, saturation, brightness and tint of the boxes that offer them.
extern const ApplyGroup kPsiApplyGroup;

} // namespace coreapi

#endif
