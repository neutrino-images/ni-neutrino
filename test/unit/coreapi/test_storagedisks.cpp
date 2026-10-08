/*
 * test_storagedisks.cpp - tests for which block devices are the user's disks
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

#include "support/catch.hpp"
#include "coreapi/storage.h"
#include "coreapi/box/storage_disks.h"
#include "coreapi/box/storage_internal.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

using namespace coreapi;

namespace
{

// A tree standing in for /sys/block, /dev and the mount table, with the
// readers pointed at it for as long as the case runs.
struct FakeBox
{
	std::string dir;
	std::string sys;
	std::string dev;
	std::string table;
	std::string sysdev;
	const char *before_sys;
	const char *before_dev;
	const char *before_table;
	const char *before_sysdev;
	const char *before_root;

	FakeBox()
	    : before_sys(storage::internal::sys_block_path),
	      before_dev(storage::internal::dev_dir),
	      before_table(storage::internal::mounts_path),
	      before_sysdev(storage::internal::sys_dev_block_path),
	      before_root(storage::internal::root_path)
	{
		char tmpl[] = "/tmp/coreapi_disks_XXXXXX";
		if (mkdtemp(tmpl) != NULL)
			dir = tmpl;
		sys = dir + "/sys/block";
		dev = dir + "/dev";
		sysdev = dir + "/sys/dev/block";
		table = dir + "/mounts";
		run("mkdir -p " + sys + " " + dev + " " + sysdev);

		storage::internal::sys_block_path = sys.c_str();
		storage::internal::dev_dir = dev.c_str();
		storage::internal::mounts_path = table.c_str();
		storage::internal::sys_dev_block_path = sysdev.c_str();
		// Not the real root, whose number says nothing about this tree.
		storage::internal::root_path = dir.c_str();
	}

	~FakeBox()
	{
		storage::internal::sys_block_path = before_sys;
		storage::internal::dev_dir = before_dev;
		storage::internal::mounts_path = before_table;
		storage::internal::sys_dev_block_path = before_sysdev;
		storage::internal::root_path = before_root;
		if (!dir.empty())
			run("rm -rf " + dir);
	}

	static void run(const std::string &cmd)
	{
		const int rc = system(cmd.c_str());
		REQUIRE(rc == 0);
	}

	void write(const std::string &path, const std::string &body) const
	{
		FILE *f = fopen(path.c_str(), "w");
		REQUIRE(f != NULL);
		fputs(body.c_str(), f);
		fclose(f);
	}

	void block(const std::string &name, const char *type = NULL) const
	{
		run("mkdir -p " + sys + "/" + name + "/device");
		if (type != NULL)
			write(sys + "/" + name + "/device/type", std::string(type) + "\n");
	}

	void mounts(const std::string &lines) const { write(table, lines); }
};

std::string names(const std::vector<storage::DiskInfo> &all)
{
	std::string out;
	for (size_t i = 0; i < all.size(); i++)
		out += (i ? "," : "") + all[i].name;
	return out;
}

} // namespace

TEST_CASE("an SD card is a disk and the flash the box boots from is not", "[storage][disks]")
{
	FakeBox box;
	box.block("mmcblk0", "MMC");
	box.block("mmcblk1", "SD");
	box.block("sda");
	box.mounts("ubi0:rootfs / ubifs rw 0 0\n");

	REQUIRE(names(storage::disks()) == "mmcblk1,sda");
}

TEST_CASE("a card device that does not say what it is stays out", "[storage][disks]")
{
	FakeBox box;
	box.block("mmcblk0");
	box.mounts("ubi0:rootfs / ubifs rw 0 0\n");

	REQUIRE(storage::disks().empty());
}

TEST_CASE("the root on a partition past the seventh keeps its device out", "[storage][disks]")
{
	FakeBox box;
	// Both report SD, so it is the root and not the type that keeps the first out.
	box.block("mmcblk0", "SD");
	box.block("mmcblk1", "SD");
	box.mounts("/dev/mmcblk0p9 / ext4 rw 0 0\n");

	REQUIRE(names(storage::disks()) == "mmcblk1");
}

TEST_CASE("the root on the internal flash does not hide the SD card", "[storage][disks]")
{
	FakeBox box;
	box.block("mmcblk0", "MMC");
	box.block("mmcblk1", "SD");
	box.mounts("/dev/mmcblk0p2 / ext4 rw 0 0\n");

	REQUIRE(names(storage::disks()) == "mmcblk1");
}

TEST_CASE("the root on one disk does not hide a disk whose name continues it", "[storage][disks]")
{
	FakeBox box;
	box.block("sda");
	box.block("sdaa");
	box.block("hda");
	box.mounts("/dev/sda1 / ext4 rw 0 0\n");

	REQUIRE(names(storage::disks()) == "hda,sdaa");
}

TEST_CASE("devices that are not disks are left out", "[storage][disks]")
{
	FakeBox box;
	box.block("loop0");
	box.block("ram0");
	box.block("mtdblock3");
	box.block("sdb");
	box.mounts("rootfs / rootfs rw 0 0\n");

	REQUIRE(names(storage::disks()) == "sdb");
}

TEST_CASE("the root named only as root is found by its number", "[storage][disks]")
{
	FakeBox box;
	box.block("sda");
	box.block("sdb");
	box.run("mkdir -p " + box.sys + "/sda/sda2");
	box.mounts("/dev/root / ext4 rw 0 0\n");

	struct stat st;
	REQUIRE(stat(box.dir.c_str(), &st) == 0);
	char link[64];
	snprintf(link, sizeof(link), "%u:%u", major(st.st_dev), minor(st.st_dev));
	REQUIRE(symlink("../../block/sda/sda2", (box.sysdev + "/" + link).c_str()) == 0);

	REQUIRE(names(storage::disks()) == "sdb");
}

TEST_CASE("a root mounted through a link in the device directory is found", "[storage][disks]")
{
	FakeBox box;
	box.block("sda");
	box.block("sdb");
	box.write(box.dev + "/sda1", "");
	REQUIRE(symlink("sda1", (box.dev + "/by-label").c_str()) == 0);
	box.mounts("/dev/by-label / ext4 rw 0 0\n");

	REQUIRE(names(storage::disks()) == "sdb");
}

TEST_CASE("a root that cannot be placed leaves no disk out", "[storage][disks]")
{
	FakeBox box;
	box.block("sda");
	box.mounts("/dev/root / ext4 rw 0 0\n");

	// Nothing in the tree answers for the number of the root.
	REQUIRE(names(storage::disks()) == "sda");
}

TEST_CASE("without a mount table there are no disks", "[storage][disks]")
{
	FakeBox box;
	box.block("sda");

	REQUIRE(storage::disks().empty());
}

TEST_CASE("partitions are named the way the kernel names them", "[storage][disks]")
{
	REQUIRE(storage::partitionName("sda", 1) == "sda1");
	REQUIRE(storage::partitionName("sdb", 12) == "sdb12");
	REQUIRE(storage::partitionName("hda", 3) == "hda3");
	REQUIRE(storage::partitionName("mmcblk1", 1) == "mmcblk1p1");
	REQUIRE(storage::partitionName("mmcblk0", 9) == "mmcblk0p9");
}

TEST_CASE("a name belongs to a device by the partition rule and not by its first letters", "[storage][disks]")
{
	REQUIRE(storage::ownsDevice("sda", "sda"));
	REQUIRE(storage::ownsDevice("sda", "sda1"));
	REQUIRE_FALSE(storage::ownsDevice("sda", "sdaa1"));
	REQUIRE_FALSE(storage::ownsDevice("sda", "sdb1"));
	REQUIRE(storage::ownsDevice("mmcblk1", "mmcblk1p1"));
	REQUIRE_FALSE(storage::ownsDevice("mmcblk1", "mmcblk11"));
	REQUIRE_FALSE(storage::ownsDevice("mmcblk1", "mmcblk10p1"));
	REQUIRE_FALSE(storage::ownsDevice("", "sda"));
}

TEST_CASE("a partition of a listed disk is a user device and one of the flash is not", "[storage][disks]")
{
	FakeBox box;
	box.block("mmcblk0", "MMC");
	box.block("mmcblk1", "SD");
	box.mounts("ubi0:rootfs / ubifs rw 0 0\n");

	REQUIRE(storage::isUserDevice("mmcblk1"));
	REQUIRE(storage::isUserDevice("mmcblk1p1"));
	REQUIRE_FALSE(storage::isUserDevice("mmcblk0p1"));
	REQUIRE_FALSE(storage::isUserDevice("loop0"));
}

TEST_CASE("a removed device is still recognised by its name", "[storage][disks]")
{
	REQUIRE(storage::looksLikeDisk("sda1"));
	REQUIRE(storage::looksLikeDisk("mmcblk1p1"));
	REQUIRE(storage::looksLikeDisk("sr0"));
	REQUIRE_FALSE(storage::looksLikeDisk("loop0"));
	REQUIRE_FALSE(storage::looksLikeDisk("ubi0"));
}

TEST_CASE("the line a RAM filesystem leaves for / is covered by the root mounted over it", "[storage][mounts]")
{
	FakeBox box;
	box.mounts("rootfs / rootfs rw 0 0\n"
		   "/dev/root / ext4 rw 0 0\n"
		   "tmpfs /run tmpfs rw 0 0\n");

	Result<std::vector<MountInfo> > r = storage::mounts();
	REQUIRE(r.ok());
	const std::vector<MountInfo> &all = r.value();
	REQUIRE(all.size() == 2);
	REQUIRE(all[0].mountpoint == "/");
	REQUIRE(all[0].device == "/dev/root");
	REQUIRE(all[0].fstype == "ext4");
	REQUIRE(all[1].mountpoint == "/run");
}

TEST_CASE("a disk mounted on two points is listed on both", "[storage][mounts]")
{
	FakeBox box;
	box.mounts("/dev/sda1 /media/sda1 ext4 rw 0 0\n"
		   "/dev/sda1 /mnt/data ext4 rw 0 0\n");

	Result<std::vector<MountInfo> > r = storage::mounts();
	REQUIRE(r.ok());
	REQUIRE(r.value().size() == 2);
	REQUIRE(r.value()[0].mountpoint == "/media/sda1");
	REQUIRE(r.value()[1].mountpoint == "/mnt/data");
}

TEST_CASE("the raw table keeps a mount that a later one covers", "[storage][mounts]")
{
	FakeBox box;
	box.mounts("/dev/sda1 /media/x ext4 rw 0 0\n"
		   "tmpfs /media/x tmpfs rw 0 0\n");

	std::vector<storage::internal::MountLine> covered;
	REQUIRE(storage::internal::readMountTable(covered));
	REQUIRE(covered.size() == 1);
	REQUIRE(covered[0].device == "tmpfs");

	std::vector<storage::internal::MountLine> raw;
	REQUIRE(storage::internal::readMountTable(raw, true));
	REQUIRE(raw.size() == 2);
	REQUIRE(raw[0].device == "/dev/sda1");
}
