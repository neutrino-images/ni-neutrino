/*
 * fakewebchannels.h - a web channel seam that counts what the groups send
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

#ifndef __support_fakewebchannels_h__
#define __support_fakewebchannels_h__

#include "coreapi/box/apply_webchannels.h"

struct FakeWebChannels : public coreapi::WebChannelsOutput
{
	int reloads;
	int restarts;
	coreapi::Status reload_answer;
	coreapi::Status restart_answer;

	FakeWebChannels() : reloads(0), restarts(0), reload_answer(coreapi::Status::Ok), restart_answer(coreapi::Status::Ok) {}

	coreapi::Status reloadLists() { ++reloads; return reload_answer; }
	coreapi::Status restartStream() { ++restarts; return restart_answer; }
};

#endif
