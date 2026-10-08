/*
 * applyseams.cpp - the seams the apply groups reach before the first phase
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
#include "coreapi/base/deps.h"
#include "coreapi/box/apply_audio.h"
#include "coreapi/box/apply_cec.h"
#include "coreapi/box/apply_hdd.h"
#include "coreapi/box/apply_record.h"
#include "coreapi/box/apply_webchannels.h"
#include "coreapi/box/apply_misc.h"
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

namespace coreapi
{

/* One call per line, so areas added side by side do not touch each other's line.
   Each seam here needs nothing the program builds after its settings are loaded:
   a real side that reaches a decoder or a daemon does so when a group runs, not
   when it is installed. check-hook.sh reads this body. */
void installApplySeams()
{
	installRealSystemSource();
	installRealVideoOutput();
	installRealAudioOutput();
	installRealCecLink();
	installRealMiscOutput();
	installRealSectionsdOutput();
	installRealRcControl();
	installRealLocalization();
	installRealCiControl();
	installRealServiceControl();
	installRealUpdateCheck();
	installRealPluginLoader();
	installRealWeatherService();
	installRealLcd4lControl();
	installRealVfdPanel();
	installRealGlcdPanel();
	installRealOsdOutput();
	installRealRecordConfigOutput();
	installRealHddControl();
	installRealWebChannelsOutput();
}

} // namespace coreapi
