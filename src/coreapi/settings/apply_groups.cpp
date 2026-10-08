/*
 * apply_groups.cpp - where every apply group is registered
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

#include "coreapi/base/apply.h"
#include "coreapi/box/apply_audio.h"
#include "coreapi/box/apply_cec.h"
#include "coreapi/box/apply_channels.h"
#include "coreapi/box/apply_hdd.h"
#include "coreapi/box/apply_misc.h"
#include "coreapi/box/apply_record.h"
#include "coreapi/box/apply_sectionsd.h"
#include "coreapi/box/apply_keys.h"
#include "coreapi/box/apply_lang.h"
#include "coreapi/box/apply_ci.h"
#include "coreapi/box/apply_services.h"
#include "coreapi/box/apply_update.h"
#include "coreapi/box/apply_glcd.h"
#include "coreapi/box/apply_lcd4l.h"
#include "coreapi/box/apply_plugins.h"
#include "coreapi/box/apply_vfd.h"
#include "coreapi/box/apply_osd.h"
#include "coreapi/box/apply_video.h"
#include "coreapi/box/apply_weather.h"
#include "coreapi/box/apply_webchannels.h"

namespace coreapi
{

/* One line per area, added by the stream that moves the area's effects into a
   group. Kept in one function so that the registration point and its place in
   startup, before any phase, are decided once. */
void registerApplyGroups()
{
	registerApplyGroup(&kVideoApplyGroup);
	registerApplyGroup(&kPsiApplyGroup);
	registerApplyGroup(&kSrsApplyGroup);
	registerApplyGroup(&kVolumePercentApplyGroup);
	registerApplyGroup(&kAudioApplyGroup);
	registerApplyGroup(&kAudioModeApplyGroup);
	registerApplyGroup(&kCecApplyGroup);
	registerApplyGroup(&kSectionsdConfigApplyGroup);
	registerApplyGroup(&kTuxtxtApplyGroup);
	registerApplyGroup(&kScanSdtApplyGroup);
	registerApplyGroup(&kStreamPortApplyGroup);
	registerApplyGroup(&kEpgScanApplyGroup);
	registerApplyGroup(&kCpuFreqApplyGroup);
	registerApplyGroup(&kFanApplyGroup);
	registerApplyGroup(&kChannelReloadApplyGroup);
	registerApplyGroup(&kRcApplyGroup);
	registerApplyGroup(&kLanguageApplyGroup);
	registerApplyGroup(&kTimezoneApplyGroup);
	registerApplyGroup(&kGuideLanguageApplyGroup);
	registerApplyGroup(&kCiApplyGroup);
	registerApplyGroup(&kServicesApplyGroup);
	registerApplyGroup(&kUpdateApplyGroup);
	registerApplyGroup(&kPluginsApplyGroup);
	registerApplyGroup(&kWeatherApplyGroup);
	registerApplyGroup(&kLcd4lApplyGroup);
	registerApplyGroup(&kVfdApplyGroup);
	registerApplyGroup(&kGlcdApplyGroup);
	registerApplyGroup(&kFontsApplyGroup);
	registerApplyGroup(&kPaletteApplyGroup);
	registerApplyGroup(&kScreenGeometryApplyGroup);
	registerApplyGroup(&kEventLogoApplyGroup);
	registerApplyGroup(&kChannelListApplyGroup);
	registerApplyGroup(&kInfoViewerApplyGroup);
	registerApplyGroup(&kInfoClockApplyGroup);
	registerApplyGroup(&kInfoIconsApplyGroup);
	registerApplyGroup(&kVolumeBarApplyGroup);
	registerApplyGroup(&kRadioTextApplyGroup);
	registerApplyGroup(&kOsdResolutionApplyGroup);
	registerApplyGroup(&kRecordConfigApplyGroup);
	registerApplyGroup(&kHdIdleApplyGroup);
	registerApplyGroup(&kWebChannelsApplyGroup);
	registerApplyGroup(&kLivestreamApplyGroup);
}

} // namespace coreapi
