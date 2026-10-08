/*
 * miscoutput_real.cpp - the miscellaneous groups on the running box
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

#include "coreapi/box/apply_misc.h"

#include <driver/neutrino_msg_t.h>
#include <driver/scanepg.h>
#include <driver/streamts.h>
#include <libtuxtxt/teletext.h>
#include <zapit/zapit.h>

namespace coreapi
{

namespace
{

class RealMiscOutput : public MiscOutput
{
	public:
		Status setScanSdt(int mode)
		{
			CZapit::getInstance()->SetScanSDT(mode);
			return Status::Ok;
		}

		Status setStreamPort(int port)
		{
			CStreamManager *manager = CStreamManager::getInstance();
			if (manager->GetPort() == port)
				return Status::Ok;
			// SetPort answers false for an unchanged port and for a listener that did not come up.
			return manager->SetPort(port) ? Status::Ok : Status::Internal;
		}

		Status setTeletextCache(bool on)
		{
			if (on)
				tuxtxt_init();
			else
				tuxtxt_close();
			return Status::Ok;
		}

		Status configureEpgFilter()
		{
			CEpgScan::getInstance()->ConfigureEIT();
			return Status::Ok;
		}

		Status startEpgScan()
		{
			CEpgScan::getInstance()->Start();
			return Status::Ok;
		}

		Status clearEpgScan()
		{
			CEpgScan::getInstance()->Clear();
			return Status::Ok;
		}

		Status setCpuFreq(int mhz) { return applicationSetCpuFreq(mhz); }
		Status setFanSpeed(int speed) { return applicationSetFanSpeed(speed); }
};

RealMiscOutput g_real_misc_output;

} // namespace

void installRealMiscOutput()
{
	setMiscOutput(&g_real_misc_output);
}

} // namespace coreapi
