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
constexpr EnumValue kHddSleep[] =
{
	option(0).label("options.off"),
	option(60).label("hdd_5min"),
	option(120).label("hdd_10min"),
	option(240).label("hdd_20min"),
	option(241).label("hdd_30min"),
	option(242).label("hdd_60min")
};

// How loud the disk is allowed to be.
constexpr EnumValue kHddNoise[] =
{
	option(0).label("options.off"),
	option(128).label("hdd_slow"),
	option(190).label("hdd_middle"),
	option(254).label("hdd_fast")
};

/* Positions in the disk manager's tool table, in the same order; named by the
   file system, which the program has no other words for. An entry is offered
   where its mkfs is there. */
constexpr EnumValue kHddFs[] =
{
	option(0).text("ext4").availableIf(formatsExt4),
	option(1).text("ext3").availableIf(formatsExt3),
	option(2).text("ext2").availableIf(formatsExt2),
	option(3).text("f2fs").availableIf(formatsF2fs),
	option(4).text("vfat").availableIf(formatsVfat),
	option(5).text("exfat").availableIf(formatsExfat),
	option(6).text("xfs").availableIf(formatsXfs)
};

constexpr Descriptor kHdd[] =
{
	// Which file system the box writes when it formats a disk.
	enumRow("hdd_fs")
		.section("hdd")
		.label("hdd_fs")
		.hint("menu.hint_hdd_fmt")
		.defaultValue(0)
		.values(kHddFs)
		.field(COREAPI_NUMBER_FIELD(hdd_fs)),
	enumRow("hdd_sleep")
		.section("hdd")
		.label("hdd_sleep")
		.hint("menu.hint_hdd_sleep")
		.defaultValue(60)
		.values(kHddSleep)
		.field(COREAPI_NUMBER_FIELD(hdd_sleep)),
	/* Only a full hdparm takes this one, which is a file the box looks for and
	   not a setting, so the row carries no condition. */
	enumRow("hdd_noise")
		.section("hdd")
		.label("hdd_noise")
		.hint("menu.hint_hdd_noise")
		.defaultValue(254)
		.values(kHddNoise)
		.field(COREAPI_NUMBER_FIELD(hdd_noise)),
	boolRow("hdd_format_on_mount_failed")
		.section("hdd")
		.label("hdd_format_on_mount_failed")
		.hint("menu.hint_hdd_format_on_mount_failed")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(hdd_format_on_mount_failed)),
	boolRow("hdd_wakeup")
		.section("hdd")
		.label("hdd_wakeup")
		.hint("menu.hint_hdd_wakeup")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(hdd_wakeup)),
	boolRow("hdd_wakeup_msg")
		.section("hdd")
		.label("hdd_wakeup_msg")
		.hint("menu.hint_hdd_wakeup_msg")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(hdd_wakeup_msg)),
	boolRow("hdd_allow_set_recdir")
		.section("hdd")
		.label("hdd_allow_set_recdir")
		.hint("menu.hint_hdd_allow_set_recdir")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(hdd_allow_set_recdir)),
};

} // anonymous namespace

const Descriptor *settingsTableHdd(size_t &count)
{
	count = sizeof(kHdd) / sizeof(kHdd[0]);
	return kHdd;
}

} // namespace coreapi
