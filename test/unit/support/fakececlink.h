/*
 * fakececlink.h - a CEC seam that records what the group sends
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

#ifndef __support_fakececlink_h__
#define __support_fakececlink_h__

#include "coreapi/box/apply_cec.h"

#include <system/settings.h>

#include <string>
#include <vector>

extern SNeutrinoSettings g_settings;

/* Every call the CEC group makes, in order, with the number each carried. */
struct FakeCecLink : public coreapi::CecLink
{
	std::vector<std::string> calls;
	std::vector<int> values;

	// What the mode call answers, so a case can make one send fail.
	coreapi::Status mode_answer;
	FakeCecLink() : mode_answer(coreapi::Status::Ok), takes_destination(true) {}

	coreapi::Status setMode(int mode) { calls.push_back("mode"); values.push_back(mode); return mode_answer; }
	coreapi::Status setAutoStandby(int on) { calls.push_back("standby"); values.push_back(on); return coreapi::Status::Ok; }
	coreapi::Status setAutoView(int on) { calls.push_back("view"); values.push_back(on); return coreapi::Status::Ok; }
	bool takes_destination;
	bool takesAudioDestination() const { return takes_destination; }
	coreapi::Status setAudioDestination(int d) { calls.push_back("destination"); values.push_back(d); return coreapi::Status::Ok; }

	size_t count(const std::string &what) const
	{
		size_t n = 0;
		for (size_t i = 0; i < calls.size(); ++i)
			n += (calls[i] == what) ? 1 : 0;
		return n;
	}

	void forget()
	{
		calls.clear();
		values.clear();
	}
};

/* The link installed and the two settings that ask for it, put back however the case ends. */
struct CecSettingsAndLink
{
	FakeCecLink link;
	int standby, view_on;

	CecSettingsAndLink(int on_standby, int on_view)
		: standby(g_settings.hdmi_cec_standby), view_on(g_settings.hdmi_cec_view_on)
	{
		g_settings.hdmi_cec_standby = on_standby;
		g_settings.hdmi_cec_view_on = on_view;
		coreapi::setCecLink(&link);
	}
	~CecSettingsAndLink()
	{
		coreapi::setCecLink(0);
		g_settings.hdmi_cec_standby = standby;
		g_settings.hdmi_cec_view_on = view_on;
	}
};

#endif
