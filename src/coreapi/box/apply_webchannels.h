/*
 * apply_webchannels.h - what makes changed web channel settings take effect
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

#ifndef __coreapi_apply_webchannels_h__
#define __coreapi_apply_webchannels_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

namespace coreapi
{

/* What the web channel groups ask of the box: the lists read again, and the
   running web stream started over at another size. A seam rather than the
   channel daemon and the player, so the groups can run where neither exists. */
struct WebChannelsOutput
{
	virtual ~WebChannelsOutput() {}

	/* The files of the automatic folders are added to the lists, where the
	   switch for the list is on, and the channel daemon reads the lists again. */
	virtual Status reloadLists() = 0;
	// Nothing happens when no web stream is playing.
	virtual Status restartStream() = 0;
};

/* NotSupported for every call while nothing is installed. */
WebChannelsOutput &webChannelsOutput();
void setWebChannelsOutput(WebChannelsOutput *o);

// Binds the seam to the channel daemon and the player of the running box.
void installRealWebChannelsOutput();

// The real side is the application's: it needs the player and the channel lists.
Status applicationReloadWebChannels();
Status applicationRestartWebStream();

// Forgets what the groups were running with, for a case that needs the first run again.
void resetWebChannels();

// The automatic folders: the lists are read again when a switch changes.
extern const ApplyGroup kWebChannelsApplyGroup;

// The picture size of a web stream: a stream playing is started over at the new size.
extern const ApplyGroup kLivestreamApplyGroup;

} // namespace coreapi

#endif
