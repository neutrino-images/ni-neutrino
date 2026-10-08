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

#include <set>
#include <stdint.h>
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
	// Both as the device reports them, trimmed. Empty where it does not say.
	std::string vendor;
	std::string model;
	uint64_t size_bytes;
	// What the kernel says of the medium, which a memory card reader does not
	// set for a card in it. A caller that wants to know whether a card can be
	// taken out asks the device type, not this.
	bool removable;
	// A drive that reads discs, which has no disk label and no health data.
	bool optical;

	DiskInfo() : size_bytes(0), removable(false), optical(false) {}
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

/* One file system the box can put on a disk. The order is the order of the
   setting that picks one, so a position stored there names a row here. */
struct FsTool
{
	std::string fmt;
	std::string fsck;
	std::string fsck_options;
	std::string mkfs;
	std::string mkfs_options;
	std::string mkfs_labelswitch;
	// The tool is on the path and the kernel lists the file system. Where the
	// kernel does not, the names above are left as the table spells them.
	bool fsck_supported;
	bool mkfs_supported;

	FsTool() : fsck_supported(false), mkfs_supported(false) {}
};

// Resolved against the kernel and the path at every call, so a tool installed
// after the program started is seen by the next one.
std::vector<FsTool> fsTools();

// The file systems the kernel lists. Empty when the list cannot be read.
std::set<std::string> kernelFilesystems();

/* Whether a device or one of its partitions is mounted, whether the mount was
   made through its own name or through a link to it. Takes sda1 or /dev/sda1. */
bool isMounted(const std::string &name);

// Mounts a partition and says whether it is mounted afterwards. A disc drive
// is told to close its tray first.
bool mount(const std::string &name);

// Unmounts a partition and says whether it is gone afterwards. A disc drive is
// told to open its tray after.
bool umount(const std::string &name);

// Unmounts every partition of a disk that is mounted. False if one stayed.
bool umountAll(const std::string &disk);

// Opens the tray of a disc drive, or closes it. False when the box has no tool.
bool eject(const std::string &name, bool load);

/* What a screen that formats a disk is told as it goes. The box layer does the
   work and the screen does the drawing, so every call is a thing to show and
   none of them asks for an answer. */
class FormatObserver
{
public:
	virtual ~FormatObserver() {}
	// The disk is released and the first command is about to run.
	virtual void begin() = 0;
	virtual void message(const std::string &text) = 0;
	virtual void global(int percent) = 0;
	virtual void local(int percent) = 0;
	// The partition table is about to be rewritten, so whatever the screen
	// listed of this disk is out of date from here on, however the rest ends.
	virtual void tableChanged() = 0;
	// The commands are over, whatever they came to.
	virtual void end() = 0;
};

enum class FormatResult
{
	Done,
	// The disk is not named like a device, so nothing was touched.
	BadDevice,
	// The name is not a file system this box has a mkfs for.
	UnknownFilesystem,
	// A partition of the disk could not be unmounted.
	Busy,
	NoPartitionTool,
	PartitionFailed,
	MkfsFailed
};

/* Makes one partition spanning the disk and a file system on it, then mounts it.
   With makeDirs the mounted file system gets the directories the box keeps its
   recordings and pictures in. The observer may be NULL. */
FormatResult format(const std::string &disk, const std::string &fs, const std::string &label,
		    bool makeDirs, FormatObserver *observer);

} // namespace storage
} // namespace coreapi

#endif
