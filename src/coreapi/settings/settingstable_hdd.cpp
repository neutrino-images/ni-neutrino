/*
 * settingstable_hdd.cpp - hard disk settings, one row per field
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
#include "predicates.h"

namespace coreapi
{

namespace
{

/* The hard disk section. Everything else about disks is bound to devices found
   at run time, which is why only these seven are here. The two sleep values
   reach the disk through a separate apply step. */

/* How long the disk waits before it spins down. The last three are not
   minutes: the driver reads them as its own codes. */
const EnumValue kHddSleep[] =
{
	{   0, "options.off", NULL, NULL },
	{  60, "hdd_5min", NULL, NULL },
	{ 120, "hdd_10min", NULL, NULL },
	{ 240, "hdd_20min", NULL, NULL },
	{ 241, "hdd_30min", NULL, NULL },
	{ 242, "hdd_60min", NULL, NULL }
};

// How loud the disk is allowed to be.
const EnumValue kHddNoise[] =
{
	{   0, "options.off", NULL, NULL },
	{ 128, "hdd_slow", NULL, NULL },
	{ 190, "hdd_middle", NULL, NULL },
	{ 254, "hdd_fast", NULL, NULL }
};

/* Positions in the disk manager's tool table, in the same order; named by the
   file system, which the program has no other words for. An entry is offered
   where its mkfs is there. */
const EnumValue kHddFs[] =
{
	{ 0, NULL, "ext4", formatsExt4 },
	{ 1, NULL, "ext3", formatsExt3 },
	{ 2, NULL, "ext2", formatsExt2 },
	{ 3, NULL, "f2fs", formatsF2fs },
	{ 4, NULL, "vfat", formatsVfat },
	{ 5, NULL, "exfat", formatsExfat },
	{ 6, NULL, "xfs", formatsXfs }
};

const Descriptor kHdd[] =
{
	// Which file system the box writes when it formats a disk.
	{
		"hdd_fs", ValueType::Enum, "hdd",
		"hdd_fs", "menu.hint_hdd_fmt",
		0, 0, COREAPI_VALUES(kHddFs), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(hdd_fs)
	},
	{
		"hdd_sleep", ValueType::Enum, "hdd",
		"hdd_sleep", "menu.hint_hdd_sleep",
		0, 0, COREAPI_VALUES(kHddSleep), 60, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(hdd_sleep)
	},
	/* Only a full hdparm takes this one, which is a file the box looks for and
	   not a setting, so the row carries no condition. */
	{
		"hdd_noise", ValueType::Enum, "hdd",
		"hdd_noise", "menu.hint_hdd_noise",
		0, 0, COREAPI_VALUES(kHddNoise), 254, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(hdd_noise)
	},
	{
		"hdd_format_on_mount_failed", ValueType::Bool, "hdd",
		"hdd_format_on_mount_failed", "menu.hint_hdd_format_on_mount_failed",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(hdd_format_on_mount_failed)
	},
	{
		"hdd_wakeup", ValueType::Bool, "hdd",
		"hdd_wakeup", "menu.hint_hdd_wakeup",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(hdd_wakeup)
	},
	{
		"hdd_wakeup_msg", ValueType::Bool, "hdd",
		"hdd_wakeup_msg", "menu.hint_hdd_wakeup_msg",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(hdd_wakeup_msg)
	},
	{
		"hdd_allow_set_recdir", ValueType::Bool, "hdd",
		"hdd_allow_set_recdir", "menu.hint_hdd_allow_set_recdir",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(hdd_allow_set_recdir)
	},
};

} // anonymous namespace

const Descriptor *settingsTableHdd(size_t &count)
{
	count = sizeof(kHdd) / sizeof(kHdd[0]);
	return kHdd;
}

} // namespace coreapi
