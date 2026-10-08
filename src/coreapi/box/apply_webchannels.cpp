/*
 * apply_webchannels.cpp - what makes changed web channel settings take effect
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

#include "coreapi/box/apply_webchannels.h"
#include "coreapi/box/sentstate.h"

#include <system/settings.h>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

class NoWebChannelsOutput : public WebChannelsOutput
{
	public:
		Status reloadLists() { return Status::NotSupported; }
		Status restartStream() { return Status::NotSupported; }
};

NoWebChannelsOutput g_no_web_channels_output;
WebChannelsOutput *g_web_channels_output = 0;

/* What the lists and the stream were set up with. The first run only notes it:
   the lists are read when the channel daemon starts, from the settings as they
   are, and a stream is not playing yet. A send that failed is not taken as made,
   so the next run asks again. */
Sent<int> g_lists;
Sent<int> g_size;

Status runWebChannels()
{
	const int now = (g_settings.webtv_xml_auto ? 1 : 0) | (g_settings.webradio_xml_auto ? 2 : 0);
	if (!g_lists.known)
	{
		g_lists.known = true;
		g_lists.value = now;
		return Status::Ok;
	}
	if (!g_lists.differs(now))
		return Status::Ok;

	const Status s = webChannelsOutput().reloadLists();
	if (s == Status::Ok)
		g_lists.value = now;
	return s;
}

Status runLivestream()
{
	const int now = g_settings.livestreamResolution;
	if (!g_size.known)
	{
		g_size.known = true;
		g_size.value = now;
		return Status::Ok;
	}
	if (!g_size.differs(now))
		return Status::Ok;

	const Status s = webChannelsOutput().restartStream();
	if (s == Status::Ok)
		g_size.value = now;
	return s;
}

const char *const kWebChannelsKeys[] =
{
	"webtv_xml_auto",
	"webradio_xml_auto"
};

const char *const kLivestreamKeys[] =
{
	"livestreamResolution"
};

} // namespace

WebChannelsOutput &webChannelsOutput()
{
	if (!g_web_channels_output)
		return g_no_web_channels_output;
	return *g_web_channels_output;
}

void setWebChannelsOutput(WebChannelsOutput *o) { g_web_channels_output = o; }

void resetWebChannels()
{
	g_lists = Sent<int>();
	g_size = Sent<int>();
}

/* After the channel lists are made, which is where the channel daemon has read
   the lists once. */
const ApplyGroup kWebChannelsApplyGroup = { "webChannels", ApplyPhase::Sectionsd, COREAPI_KEYS(kWebChannelsKeys), &runWebChannels };

const ApplyGroup kLivestreamApplyGroup = { "livestream", ApplyPhase::Sectionsd, COREAPI_KEYS(kLivestreamKeys), &runLivestream };

} // namespace coreapi
