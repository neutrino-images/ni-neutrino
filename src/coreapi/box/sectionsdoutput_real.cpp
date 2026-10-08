/*
 * sectionsdoutput_real.cpp - the section daemon group on the running box
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

#include "coreapi/box/apply_sectionsd.h"

namespace coreapi
{

namespace
{

class RealSectionsdOutput : public SectionsdOutput
{
	public:
		Status sendConfig()
		{
			return applicationSendSectionsdConfig();
		}
};

RealSectionsdOutput g_real_sectionsd_output;

} // namespace

void installRealSectionsdOutput()
{
	setSectionsdOutput(&g_real_sectionsd_output);
}

} // namespace coreapi
