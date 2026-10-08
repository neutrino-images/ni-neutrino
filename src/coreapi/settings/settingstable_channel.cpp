/*
 * settingstable_channel.cpp - channel settings, one row per field
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

#include "settingstable.h"
#include "settingsfield.h"

namespace coreapi
{

namespace
{

/* The channel section. The descriptor carries no sub-section, so the split
   into start channel and list mode on one side and web channel rows on the
   other is not in the table. Scan settings are not here: they are objects of
   their own with files of their own.

   The start channel is four rows and two settings. What the box zaps to is
   startchanneltv_id and startchannelradio_id, each sixty four bits wide and so
   carried as the text a channel is named by everywhere else in this layer. The
   two beside them hold the name a person reads.

   A caller is expected to write the pair together, and nothing here couples
   them: one that writes an identifier alone leaves the old name standing beside
   it. Both rows say so, because the alternative is reaching the channel stack
   from the thread that carries a settings write, which is not a thread that
   may take that lock. */

// LIST_MODE_WEB is not on offer. -1 keeps the list mode last used.
const EnumValue kChannelListMode[] =
{
	{ -1, "channellist.remember", NULL, NULL },
	{ LIST_MODE_FAV, "channellist.favs", NULL, NULL },
	{ LIST_MODE_PROV, "channellist.provs", NULL, NULL },
	{ LIST_MODE_SAT, "channellist.sats", NULL, NULL },
	{ LIST_MODE_ALL, "channellist.head", NULL, NULL }
};

/* The four start channel rows apply only while the box is not told to come up
   on the channel it was left on. */
const Condition kNoLastChannel[] =
{
	{ "uselastchannel", CompareOp::Eq, 0, NULL, 0 }
};

/* The loader writes the initial mode over the mode the box was left in
   wherever one is chosen, so the pair below matters only while none is. */
const Condition kNoInitialMode[] =
{
	{ "channel_mode_initial", CompareOp::Lt, 0, NULL, 0 }
};

const Descriptor kChannel[] =
{
	/* The three rows below reach the box only at start: the first is handed to
	   zapit once and the two list modes are read by the pass that loads them and
	   nowhere else. */
	{
		"uselastchannel", ValueType::Bool, "channel",
		"zapitsetup.last_use", "menu.hint_last_use",
		0, 1, NULL, 0, 1, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(uselastchannel)
	},
	{
		"channel_mode_initial", ValueType::Enum, "channel",
		"zapitsetup.channelmode", "menu.hint_channellist_mode",
		0, 0, COREAPI_ENUM(kChannelListMode), 0, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channel_mode_initial)
	},
	{
		"channel_mode_initial_radio", ValueType::Enum, "channel",
		"zapitsetup.channelmode_radio", "menu.hint_channellist_mode_radio",
		0, 0, COREAPI_ENUM(kChannelListMode), 0, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channel_mode_initial_radio)
	},

	/* The two identifiers, which is what a start channel really is. The name
	   beside each is what a person reads. Handed to zapit once at start, so a
	   written one takes effect at the next. */
	{
		"startchanneltv_id", ValueType::String, "channel",
		NULL, NULL,
		0, 0, NULL, 0, 0, "0", true, false, COREAPI_CONDITIONS(kNoLastChannel),
		COREAPI_CHANNEL_ID_FIELD(startchanneltv_id)
	},
	{
		"startchannelradio_id", ValueType::String, "channel",
		NULL, NULL,
		0, 0, NULL, 0, 0, "0", true, false, COREAPI_CONDITIONS(kNoLastChannel),
		COREAPI_CHANNEL_ID_FIELD(startchannelradio_id)
	},
	/* The name beside each, which is what a person reads. Not what the box
	   zaps to. */
	{
		"startchanneltv", ValueType::String, "channel",
		"zapitsetup.last_tv", "menu.hint_last_tv",
		0, 0, NULL, 0, 0, "", true, false, COREAPI_CONDITIONS(kNoLastChannel),
		COREAPI_TEXT_FIELD(StartChannelTV)
	},
	{
		"startchannelradio", ValueType::String, "channel",
		"zapitsetup.last_radio", "menu.hint_last_radio",
		0, 0, NULL, 0, 0, "", true, false, COREAPI_CONDITIONS(kNoLastChannel),
		COREAPI_TEXT_FIELD(StartChannelRadio)
	},

	/* Web channels. Which of the rows below a menu shows is a matter of which
	   menu was opened and not a setting, so none of them carries a condition.

	   A number of pixels across, and a number rather than a choice: the steps
	   are literal sizes without a locale, so every value between the offered
	   ones is one a frontend can write and the box has no step for. Bound: the
	   first and the last of the steps, which are fewer on the boxes that
	   cannot decode the larger ones. */
	{
		"livestreamResolution", ValueType::Int, "channel",
		"livestream.resolution", NULL,
#if HAVE_CST_HARDWARE
		480, 1920,
#else
		480, 3840,
#endif
		NULL, 0, 1920, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(livestreamResolution)
	},
	{
		"webtv_stream_restart_attempts", ValueType::Int, "channel",
		"webtv.stream_restart_attempts", NULL,
		0, 3, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(webtv_stream_restart_attempts)
	},
	{
		"webtv_dns_diagnostics", ValueType::Bool, "channel",
		"webtv.dns.diagnostics", NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(webtv_dns_diagnostics)
	},
	/* The two below carry no fixed hint: the text depends on the directories
	   found at run time. */
	{
		"webtv_xml_auto", ValueType::Bool, "channel",
		"webtv.xml.auto", NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(webtv_xml_auto)
	},
	{
		"webradio_xml_auto", ValueType::Bool, "channel",
		"webradio.xml.auto", NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(webradio_xml_auto)
	},
	/* No menu offers this one in this build, and the value is live anyway: the
	   movie player still reads it. */
	{
		"livestreamScriptPath", ValueType::String, "channel",
		"livestream.scriptpath", NULL,
		0, 0, NULL, 0, 0, WEBTVDIR, false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(livestreamScriptPath)
	},

	/* The two below are the list the box was left in, which it writes whenever
	   the list is changed and reads once at the next start. The loader writes
	   the initial mode over both where one is chosen, which is the condition
	   each carries. The ceiling is the last of the five list modes. */
	{
		"channel_mode", ValueType::Int, "channel",
		NULL, NULL,
		0, 4, NULL, 0, 0, NULL, true, false, COREAPI_CONDITIONS(kNoInitialMode),
		COREAPI_NUMBER_FIELD(channel_mode)
	},
	{
		"channel_mode_radio", ValueType::Int, "channel",
		NULL, NULL,
		0, 4, NULL, 0, 0, NULL, true, false, COREAPI_CONDITIONS(kNoInitialMode),
		COREAPI_NUMBER_FIELD(channel_mode_radio)
	},
	/* How the channel list is sorted, stepped through and wrapped at the last
	   sort there is. That count is the ceiling here. */
	{
		"channellist_sort_mode", ValueType::Int, "channel",
		NULL, NULL,
		0, 3, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_sort_mode)
	},
	/* The two below are where the file browser last stood when a web channel
	   list was picked, written by the browser itself and read the next time it
	   opens. */
	{
		"last_webtv_dir", ValueType::String, "channel",
		NULL, NULL,
		0, 0, NULL, 0, 0, WEBTVDIR_VAR, false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(last_webtv_dir)
	},
	{
		"last_webradio_dir", ValueType::String, "channel",
		NULL, NULL,
		0, 0, NULL, 0, 0, WEBRADIODIR_VAR, false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(last_webradio_dir)
	},
};

} // anonymous namespace

const Descriptor *settingsTableChannel(size_t &count)
{
	count = sizeof(kChannel) / sizeof(kChannel[0]);
	return kChannel;
}

} // namespace coreapi
