/*
 * recordingsafetysource_real.cpp - pre-roll and post-roll as the box has them set
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

#include "coreapi/base/deps.h"

#include <connection/basicclient.h>
#include <timerdclient/timerdclient.h>

extern CTimerdClient *g_Timerd;

namespace coreapi
{

namespace
{

/* The pair the timer daemon holds, in the seconds it holds them in. The settings
   struct has two members under these names and they are not this: nothing
   loads, saves or fills them, and the daemon is the only holder.

   The client answers a failed exchange by handing back a pair of noughts rather
   than saying so (CTimerdClient::getRecordingSafety), which is a real value
   of this setting and not one to pass on as read. So what is asked first is
   whether the client is there at all. */
class RealRecordingSafety : public RecordingSafetySource
{
	public:
		Status read(int &before, int &after) const
		{
			if (!g_Timerd)
				return Status::NotSupported;

			// Every wait of the exchange cut down to what is left of this
			// moment, as every other call into this daemon is.
			CBasicClient::Deadline bound(TIMER_CALL_TIMEOUT_MS);
			g_Timerd->getRecordingSafety(before, after);
			return Status::Ok;
		}

		Status write(int before, int after)
		{
			if (!g_Timerd)
				return Status::NotSupported;

			CBasicClient::Deadline bound(TIMER_CALL_TIMEOUT_MS);
			/* The daemon acknowledges nothing here, so this says the message
			   went out and no more. Whether it took it has to be read back,
			   which is what the read beside this is for. */
			g_Timerd->setRecordingSafety(before, after);
			return Status::Ok;
		}
};

RealRecordingSafety g_real_recording_safety;

} // anonymous namespace

void installRealRecordingSafetySource()
{
	setRecordingSafetySource(&g_real_recording_safety);
}

} // namespace coreapi
