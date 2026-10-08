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
const EnumValue kNameModeApply[] =
{
	{ SOFTUPDATE_NAME_DEFAULT, "flashupdate.namemode1_default", NULL, NULL },
	{ SOFTUPDATE_NAME_HOSTNAME_TIME, "flashupdate.namemode1_hostname_time", NULL, NULL },
	{ SOFTUPDATE_NAME_ORGNAME_TIME, "flashupdate.namemode1_orgname_time", NULL, NULL }
};

// And the one it keeps.
const EnumValue kNameModeBackup[] =
{
	{ SOFTUPDATE_NAME_DEFAULT, "flashupdate.namemode2_default", NULL, NULL },
	{ SOFTUPDATE_NAME_HOSTNAME_TIME, "flashupdate.namemode2_hostname_time", NULL, NULL }
};

// How often the box looks for new packages.
const EnumValue kAutoCheckPackages[] =
{
	{  -1, "auto_update_check_on_start_only", NULL, NULL },
	{   0, "auto_update_check_off", NULL, NULL },
	{   6, "auto_update_check_6_hours", NULL, NULL },
	{  24, "auto_update_check_daily", NULL, NULL },
	{ 168, "auto_update_check_weekly", NULL, NULL },
	{ 672, "auto_update_check_monthly", NULL, NULL }
};

/* The name of the applied image matters only while the box is told to carry the
   settings over. */
const Condition kApplyingSettings[] =
{
	{ "apply_settings", CompareOp::Ne, 0, NULL, 0 }
};

const Descriptor kUpdate[] =
{
	{
		"softupdate_autocheck", ValueType::Bool, "update",
		"flashupdate.autocheck", "menu.hint_auto_update_check",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(softupdate_autocheck)
	},
#if ENABLE_PKG_MANAGEMENT
	/* Present only in builds with package management. Whether the box has a
	   package manager is a call and not a setting, so the row carries no
	   condition. */
	{
		"softupdate_autocheck_packages", ValueType::Enum, "update",
		"flashupdate.autocheck_packages", "menu.hint_auto_update_check",
		0, 0, COREAPI_ENUM(kAutoCheckPackages), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(softupdate_autocheck_packages)
	},
#endif
	/* Used only by the extended update; the struct and the loader carry all three
	   in every build, so the rows are unconditional. */
	{
		"apply_settings", ValueType::Bool, "update",
		"flashupdate.menu_apply_settings", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(apply_settings)
	},
	{
		"softupdate_name_mode_apply", ValueType::Enum, "update",
		"flashupdate.namemode1", NULL,
		0, 0, COREAPI_ENUM(kNameModeApply), 0, NULL, false, false,
		COREAPI_CONDITIONS(kApplyingSettings),
		COREAPI_NUMBER_FIELD(softupdate_name_mode_apply)
	},
	{
		"softupdate_name_mode_backup", ValueType::Enum, "update",
		"flashupdate.namemode2", NULL,
		0, 0, COREAPI_ENUM(kNameModeBackup), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(softupdate_name_mode_backup)
	},
	/* Thirty characters and a restricted set of them, and a String carries
	   neither, so that is stated nowhere a caller can read. */
	{
		"softupdate_url_file", ValueType::String, "update",
		"flashupdate.url_file", NULL,
		0, 0, NULL, 0, 0, "/var/etc/update.urls", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(softupdate_url_file)
	},
	{
		"update_dir", ValueType::String, "update",
		"extra.update_dir", NULL,
		0, 0, NULL, 0, 0, "/tmp", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(update_dir)
	},
	/* Where the package manager puts what it downloads. No item anywhere names
	   it, so the program has no name for it either; the file browser the
	   manager opens writes it. Its default is the directory beside it rather
	   than a constant, so the literal below is what that one falls back to. */
	{
		"update_dir_opkg", ValueType::String, "update",
		NULL, NULL,
		0, 0, NULL, 0, 0, "/tmp", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(update_dir_opkg)
	},
	/* The seven below are what the image writer puts into the image it builds.
	   Four of them apply only where the box has the partition or the build has
	   the binary, and where it does not the image writer writes nought over the
	   value on its way in. None of that is a comparison against another
	   setting, so none of the rows carries a condition. */
	{
		"flashupdate_createimage_add_var", ValueType::Bool, "update",
		"flashupdate.createimage_add_var", NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(flashupdate_createimage_add_var)
	},
	{
		"flashupdate_createimage_add_root1", ValueType::Bool, "update",
		"flashupdate.createimage_add_root1", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(flashupdate_createimage_add_root1)
	},
	{
		"flashupdate_createimage_add_uldr", ValueType::Bool, "update",
		"flashupdate.createimage_add_uldr", NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(flashupdate_createimage_add_uldr)
	},
	{
		"flashupdate_createimage_add_u_boot", ValueType::Bool, "update",
		"flashupdate.createimage_add_u_boot", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(flashupdate_createimage_add_u_boot)
	},
	{
		"flashupdate_createimage_add_env", ValueType::Bool, "update",
		"flashupdate.createimage_add_env", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(flashupdate_createimage_add_env)
	},
	{
		"flashupdate_createimage_add_spare", ValueType::Bool, "update",
		"flashupdate.createimage_add_spare", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(flashupdate_createimage_add_spare)
	},
	{
		"flashupdate_createimage_add_kernel", ValueType::Bool, "update",
		"flashupdate.createimage_add_kernel", NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(flashupdate_createimage_add_kernel)
	},
};

} // anonymous namespace

const Descriptor *settingsTableUpdate(size_t &count)
{
	count = sizeof(kUpdate) / sizeof(kUpdate[0]);
	return kUpdate;
}

} // namespace coreapi
