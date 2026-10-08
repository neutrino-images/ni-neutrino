/*
 * apply_channels.cpp - what makes the channel lists follow the settings they are built from
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

#include "coreapi/box/apply_channels.h"
#include "coreapi/base/deps.h"

#include <neutrinoMessages.h>
#include <system/settings.h>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

/* What the lists are built from. A setting that decides what they hold is a
   member here and a key below; the run compares the whole, so two settings that
   changed together ask for one rebuild. */
struct ChannelShape
{
	int hd_list;
	int webtv_list;
	int webradio_list;
	int empty_favorites;

	bool operator==(const ChannelShape &o) const
	{
		return hd_list == o.hd_list && webtv_list == o.webtv_list && webradio_list == o.webradio_list
			&& empty_favorites == o.empty_favorites;
	}
};

ChannelShape currentShape()
{
	ChannelShape s;
	s.hd_list = g_settings.make_hd_list;
	s.webtv_list = g_settings.make_webtv_list;
	s.webradio_list = g_settings.make_webradio_list;
	s.empty_favorites = g_settings.show_empty_favorites;
	return s;
}

bool g_built = false;
ChannelShape g_built_from;

/* The lists the loop builds from the settings it reads when the message is
   handled, so the message carries nothing. It is posted rather than the lists
   built here, because a build moves the list the screen on top may be showing
   and only the loop may do that. */
Status runChannelReload()
{
	const ChannelShape now = currentShape();
	if (!g_built)
	{
		g_built_from = now;
		g_built = true;
		return Status::Ok;
	}
	if (g_built_from == now)
		return Status::Ok;

	const Result<void> posted = postCommand(NeutrinoMessages::EVT_SERVICESCHANGED, 0);
	if (!posted.ok())
		return posted.error().status;
	// A refused post is not taken as made, so the next run asks again.
	g_built_from = now;
	return Status::Ok;
}

const char *const kChannelReloadKeys[] =
{
	"make_hd_list",
	"make_webtv_list",
	"make_webradio_list",
	"show_empty_favorites"
};

} // namespace

void resetChannelReload()
{
	g_built = false;
	g_built_from = ChannelShape();
}

/* After the command queue exists, which is where the lists are asked to be built
   again. The first run only notes the shape. */
const ApplyGroup kChannelReloadApplyGroup = { "channelReload", ApplyPhase::Sectionsd, COREAPI_KEYS(kChannelReloadKeys), &runChannelReload };

} // namespace coreapi
