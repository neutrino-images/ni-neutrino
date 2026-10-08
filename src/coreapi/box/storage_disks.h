/*
 * storage_disks.h - which block devices are the user's disks
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

#ifndef __coreapi_storage_disks_h__
#define __coreapi_storage_disks_h__

#include <string>
#include <vector>

namespace coreapi
{
namespace storage
{

struct DiskInfo
{
	// The kernel's name for the whole device, sda or mmcblk1.
	std::string name;
};

/* The block devices a user can put files on, in name order.

   Hard disks, USB sticks and optical drives are in. A memory card reader is in
   only for a card that reports itself as an SD card: the flash a box boots from
   reports as MMC through the same driver and is never offered. The device that
   holds the root filesystem is out, found through the name the mount table
   gives it and not through its device number, because the number of a
   partition on a memory card does not say which disk it belongs to.

   Empty when the kernel's lists cannot be read. A list that could not tell
   which device holds the root is not guessed at. */
std::vector<DiskInfo> disks();

/* The name of partition n on a device: sda1, mmcblk1p1. A device whose name
   ends in a digit puts a p before the number so the two cannot run together. */
std::string partitionName(const std::string &dev, int n);

/* Whether a name is the device itself or one of its partitions, by the rule
   partitionName writes. Not a prefix test: sda is not the owner of sdaa1. */
bool ownsDevice(const std::string &disk, const std::string &name);

/* Whether a name is a disk of disks() or a partition of one. */
bool isUserDevice(const std::string &name);

// The same against a list already taken, for a caller that asks about many names.
bool isUserDevice(const std::vector<DiskInfo> &listed, const std::string &name);

/* Whether a name is of the kind a user disk is called, without asking the
   kernel. For a device that has just been removed and so no longer answers: the
   removal of a card still has to be recognised as one. */
bool looksLikeDisk(const std::string &name);

} // namespace storage
} // namespace coreapi

#endif
