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
constexpr EnumValue kCiMode[] =
{
	option(0).label("ci.mode_0"),
	option(1).label("ci.mode_1"),
	option(2).label("ci.mode_2")
};

#if BOXMODEL_VUPLUS_ALL
// The steps are named by their digits.
constexpr EnumValue kCiDelay[] =
{
	option(16).text("16"),
	option(32).text("32"),
	option(64).text("64"),
	option(128).text("128"),
	option(256).text("256")
};
#endif

constexpr Descriptor kCam[] =
{
	boolRow("ci_standby_reset")
		.section("cam")
		.label("ci.reset_standby")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(ci_standby_reset)),
	boolRow("ci_check_live")
		.section("cam")
		.label("ci.check_live_slot")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(ci_check_live)),
	boolRow("ci_rec_zapto")
		.section("cam")
		.label("ci.rec_zapto")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(ci_rec_zapto)),
	enumRow("ci_mode")
		.section("cam")
		.label("ci.mode")
		.hint("menu.hint_ci_mode")
		.defaultValue(0)
		.values(kCiMode)
		.field(COREAPI_NUMBER_FIELD(ci_mode)),
	/* Which tuner the module reads, as the number the box gives it, and minus
	   one for none. A number and not a choice: the tuners on offer are the ones
	   the running box has, so no table in the source states the values. The
	   ceiling is the largest tuner count any box is built for (the frontend
	   manager's maximum), which is wider than what most boxes offer; the
	   narrower ceilings belong to other builds and refusing a value the box
	   would take is the worse direction. */
	intRow("ci_tuner")
		.section("cam")
		.label("ci.tuner")
		.range(-1, 23)
		.defaultValue(-1)
		.field(COREAPI_NUMBER_FIELD(ci_tuner)),
#if BOXMODEL_VUPLUS_ALL
	// Only where the settings struct has the field, which is on the VU+ boxes.
	enumRow("ci_delay")
		.section("cam")
		.label("ci.delay")
		.defaultValue(128)
		.values(kCiDelay)
		.field(COREAPI_NUMBER_FIELD(ci_delay)),
#endif
};

} // anonymous namespace

const Descriptor *settingsTableCam(size_t &count)
{
	count = sizeof(kCam) / sizeof(kCam[0]);
	return kCam;
}

} // namespace coreapi
