/*
 * settingstable_cam.cpp - descrambling settings, one row per field
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

/* The conditional access section. Zapit keeps its own configuration beside
   these six, which is why only these six are here. */

/* What the box does with a module. */
const EnumValue kCiMode[] =
{
	{ 0, "ci.mode_0", NULL, NULL },
	{ 1, "ci.mode_1", NULL, NULL },
	{ 2, "ci.mode_2", NULL, NULL }
};

#if BOXMODEL_VUPLUS_ALL
// The steps are named by their digits.
const EnumValue kCiDelay[] =
{
	{ 16, NULL, "16", NULL },
	{ 32, NULL, "32", NULL },
	{ 64, NULL, "64", NULL },
	{ 128, NULL, "128", NULL },
	{ 256, NULL, "256", NULL }
};
#endif

const Descriptor kCam[] =
{
	{
		"ci_standby_reset", ValueType::Bool, "cam",
		"ci.reset_standby", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(ci_standby_reset)
	},
	{
		"ci_check_live", ValueType::Bool, "cam",
		"ci.check_live_slot", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(ci_check_live)
	},
	{
		"ci_rec_zapto", ValueType::Bool, "cam",
		"ci.rec_zapto", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(ci_rec_zapto)
	},
	{
		"ci_mode", ValueType::Enum, "cam",
		"ci.mode", "menu.hint_ci_mode",
		0, 0, COREAPI_ENUM(kCiMode), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(ci_mode)
	},
	/* Which tuner the module reads, as the number the box gives it, and minus
	   one for none. A number and not a choice: the tuners on offer are the ones
	   the running box has, so no table in the source states the values. The
	   ceiling is the largest tuner count any box is built for (the frontend
	   manager's maximum), which is wider than what most boxes offer; the
	   narrower ceilings belong to other builds and refusing a value the box
	   would take is the worse direction. */
	{
		"ci_tuner", ValueType::Int, "cam",
		"ci.tuner", NULL,
		-1, 23, NULL, 0, -1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(ci_tuner)
	},
#if BOXMODEL_VUPLUS_ALL
	// Only where the settings struct has the field, which is on the VU+ boxes.
	{
		"ci_delay", ValueType::Enum, "cam",
		"ci.delay", NULL,
		0, 0, COREAPI_ENUM(kCiDelay), 128, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(ci_delay)
	},
#endif
};

} // anonymous namespace

const Descriptor *settingsTableCam(size_t &count)
{
	count = sizeof(kCam) / sizeof(kCam[0]);
	return kCam;
}

} // namespace coreapi
