/*
 * settingstable_update.cpp - update settings, one row per field
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
#include <system/settings.h>

namespace coreapi
{

namespace
{

/* The software update section: the update settings, and the seven questions
   about what goes into an image the image writer builds. */

// What the box calls the image it applies.
constexpr EnumValue kNameModeApply[] =
{
	option(SOFTUPDATE_NAME_DEFAULT).label("flashupdate.namemode1_default"),
	option(SOFTUPDATE_NAME_HOSTNAME_TIME).label("flashupdate.namemode1_hostname_time"),
	option(SOFTUPDATE_NAME_ORGNAME_TIME).label("flashupdate.namemode1_orgname_time")
};

// And the one it keeps.
constexpr EnumValue kNameModeBackup[] =
{
	option(SOFTUPDATE_NAME_DEFAULT).label("flashupdate.namemode2_default"),
	option(SOFTUPDATE_NAME_HOSTNAME_TIME).label("flashupdate.namemode2_hostname_time")
};

// How often the box looks for new packages.
constexpr EnumValue kAutoCheckPackages[] =
{
	option(-1).label("auto_update_check_on_start_only"),
	option(0).label("auto_update_check_off"),
	option(6).label("auto_update_check_6_hours"),
	option(24).label("auto_update_check_daily"),
	option(168).label("auto_update_check_weekly"),
	option(672).label("auto_update_check_monthly")
};

/* The name of the applied image matters only while the box is told to carry the
   settings over. */
constexpr Condition kApplyingSettings[] =
{
	when("apply_settings").isNot(0)
};

constexpr Descriptor kUpdate[] =
{
	boolRow("softupdate_autocheck")
		.section("update")
		.label("flashupdate.autocheck")
		.hint("menu.hint_auto_update_check")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(softupdate_autocheck)),
#if ENABLE_PKG_MANAGEMENT
	/* Present only in builds with package management. Whether the box has a
	   package manager is a call and not a setting, so the row carries no
	   condition. */
	enumRow("softupdate_autocheck_packages")
		.section("update")
		.label("flashupdate.autocheck_packages")
		.hint("menu.hint_auto_update_check")
		.defaultValue(0)
		.values(kAutoCheckPackages)
		.field(COREAPI_NUMBER_FIELD(softupdate_autocheck_packages)),
#endif
	/* Used only by the extended update; the struct and the loader carry all three
	   in every build. The first family of box has its own way to apply settings and
	   its screen leaves these two out. */
#ifndef BOXMODEL_CST_HD2
	boolRow("apply_settings")
		.section("update")
		.label("flashupdate.menu_apply_settings")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(apply_settings)),
	enumRow("softupdate_name_mode_apply")
		.section("update")
		.label("flashupdate.namemode1")
		.defaultValue(0)
		.values(kNameModeApply)
		.changeableWhen(kApplyingSettings)
		.field(COREAPI_NUMBER_FIELD(softupdate_name_mode_apply)),
#endif
	enumRow("softupdate_name_mode_backup")
		.section("update")
		.label("flashupdate.namemode2")
		.defaultValue(0)
		.values(kNameModeBackup)
		.field(COREAPI_NUMBER_FIELD(softupdate_name_mode_backup)),
	/* Picked from the files there are, or typed with the SMS input where the
	   box is built for it; the rule follows the same switch as the screen. */
	textRow("softupdate_url_file")
		.section("update")
		.label("flashupdate.url_file")
		.defaultValue("/var/etc/update.urls")
		.text(kRuleUrlFile)
		.field(COREAPI_TEXT_FIELD(softupdate_url_file)),
	textRow("update_dir")
		.section("update")
		.label("extra.update_dir")
		.defaultValue("/tmp")
		.text(kRuleDirectoryUpdate)
		.field(COREAPI_TEXT_FIELD(update_dir)),
	/* Where the package manager puts what it downloads. No item anywhere names
	   it, so the program has no name for it either; the file browser the
	   manager opens writes it. Its default is the directory beside it rather
	   than a constant, so the literal below is what that one falls back to. */
	textRow("update_dir_opkg")
		.section("update")
		.defaultValue("/tmp")
		.text(kRuleDirectoryExists)
		.field(COREAPI_TEXT_FIELD(update_dir_opkg)),
	/* The seven below are what the image writer puts into the image it builds.
	   Four of them apply only where the box has the partition or the build has
	   the binary, and where it does not the image writer writes nought over the
	   value on its way in. None of that is a comparison against another
	   setting, so none of the rows carries a condition. */
	boolRow("flashupdate_createimage_add_var")
		.section("update")
		.label("flashupdate.createimage_add_var")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(flashupdate_createimage_add_var)),
	boolRow("flashupdate_createimage_add_root1")
		.section("update")
		.label("flashupdate.createimage_add_root1")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(flashupdate_createimage_add_root1)),
	boolRow("flashupdate_createimage_add_uldr")
		.section("update")
		.label("flashupdate.createimage_add_uldr")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(flashupdate_createimage_add_uldr)),
	boolRow("flashupdate_createimage_add_u_boot")
		.section("update")
		.label("flashupdate.createimage_add_u_boot")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(flashupdate_createimage_add_u_boot)),
	boolRow("flashupdate_createimage_add_env")
		.section("update")
		.label("flashupdate.createimage_add_env")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(flashupdate_createimage_add_env)),
	boolRow("flashupdate_createimage_add_spare")
		.section("update")
		.label("flashupdate.createimage_add_spare")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(flashupdate_createimage_add_spare)),
	boolRow("flashupdate_createimage_add_kernel")
		.section("update")
		.label("flashupdate.createimage_add_kernel")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(flashupdate_createimage_add_kernel)),
};

} // anonymous namespace

const Descriptor *settingsTableUpdate(size_t &count)
{
	count = sizeof(kUpdate) / sizeof(kUpdate[0]);
	return kUpdate;
}

} // namespace coreapi
