/*
 * storage_disks.cpp - which block devices are the user's disks
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

#include "storage_disks.h"
#include "coreapi/box/storage_internal.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

namespace coreapi
{
namespace storage
{

namespace internal
{
const char *sys_block_path = "/sys/block";
const char *dev_dir = "/dev";
const char *sys_dev_block_path = "/sys/dev/block";
const char *root_path = "/";
}

namespace
{

bool startsWith(const std::string &s, const char *prefix)
{
	return s.compare(0, strlen(prefix), prefix) == 0;
}

bool allDigits(const std::string &s)
{
	if (s.empty())
		return false;
	for (size_t i = 0; i < s.size(); i++)
	{
		if (!isdigit((unsigned char) s[i]))
			return false;
	}
	return true;
}

std::string baseName(const std::string &path)
{
	const size_t cut = path.find_last_of('/');
	return cut == std::string::npos ? path : path.substr(cut + 1);
}

// The first word of a one line file in sysfs, empty where it is not there.
std::string firstWord(const std::string &path)
{
	FILE *f = fopen(path.c_str(), "r");
	if (f == NULL)
		return "";
	char word[128] = { 0 };
	if (fscanf(f, "%127s", word) != 1)
		word[0] = 0;
	fclose(f);
	return word;
}

/* The device a name in /dev stands for, as the kernel names it. The root is
   mounted as /dev/root on some boxes, with a node of its own that is a link to
   the partition or is not there at all, so a name that resolves to nothing but
   itself is not an answer. */
std::string kernelName(const std::string &device)
{
	const std::string prefix = std::string(internal::dev_dir) + "/";
	if (!startsWith(device, "/dev/"))
		return "";

	const std::string name = device.substr(5);
	char buf[PATH_MAX];
	if (realpath((prefix + name).c_str(), buf) != NULL)
	{
		// A link to a node somewhere else in the directory, such as the
		// by-label names a mount may have been made through.
		const std::string resolved(buf);
		char dir[PATH_MAX];
		if (realpath(internal::dev_dir, dir) != NULL && startsWith(resolved, (std::string(dir) + "/").c_str()))
		{
			const std::string leaf = resolved.substr(strlen(dir) + 1);
			if (leaf != "root")
				return leaf;
		}
	}
	return name == "root" ? "" : name;
}

/* The partition the root is mounted from when the mount table only says root.
   The number the kernel keeps for the filesystem names the partition in sysfs,
   and the number is taken whole, not masked down to a disk. */
std::string rootByNumber()
{
	struct stat st;
	if (stat(internal::root_path, &st) != 0)
		return "";

	char link[64];
	snprintf(link, sizeof(link), "/%u:%u", major(st.st_dev), minor(st.st_dev));
	char buf[PATH_MAX];
	if (realpath((std::string(internal::sys_dev_block_path) + link).c_str(), buf) == NULL)
		return "";
	return baseName(buf);
}

/* The partition or device the root filesystem is mounted from. False when the
   mount table cannot be read; empty where the root is not on a block device,
   which is the case on a box that boots from flash. */
bool rootDevice(std::string &out)
{
	out.clear();
	std::vector<internal::MountLine> table;
	if (!internal::readMountTable(table))
		return false;

	for (size_t i = 0; i < table.size(); i++)
	{
		if (table[i].mountpoint != "/")
			continue;
		if (!startsWith(table[i].device, "/dev/"))
			return true;
		out = kernelName(table[i].device);
		if (out.empty())
			out = rootByNumber();
		return true;
	}
	return true;
}

// Called for a name that already looks like a disk. A card reader's device says
// in its type what is in it.
bool offered(const std::string &name)
{
	if (startsWith(name, "mmcblk"))
		return firstWord(std::string(internal::sys_block_path) + "/" + name + "/device/type") == "SD";
	return true;
}

} // namespace

bool looksLikeDisk(const std::string &name)
{
	return startsWith(name, "sd") || startsWith(name, "hd") || startsWith(name, "sr") ||
	       startsWith(name, "mmcblk");
}

std::string partitionName(const std::string &dev, int n)
{
	char number[16];
	snprintf(number, sizeof(number), "%d", n);
	const bool joined = !dev.empty() && isdigit((unsigned char) dev[dev.size() - 1]);
	return dev + (joined ? "p" : "") + number;
}

bool ownsDevice(const std::string &disk, const std::string &name)
{
	if (disk.empty() || !startsWith(name, disk.c_str()))
		return false;
	if (name.size() == disk.size())
		return true;

	std::string rest = name.substr(disk.size());
	if (isdigit((unsigned char) disk[disk.size() - 1]))
	{
		if (rest[0] != 'p')
			return false;
		rest = rest.substr(1);
	}
	return allDigits(rest);
}

std::vector<DiskInfo> disks()
{
	std::vector<DiskInfo> out;

	std::string root;
	if (!rootDevice(root))
		return out;

	DIR *d = opendir(internal::sys_block_path);
	if (d == NULL)
		return out;

	std::vector<std::string> names;
	struct dirent *e;
	while ((e = readdir(d)) != NULL)
	{
		const std::string name(e->d_name);
		if (looksLikeDisk(name))
			names.push_back(name);
	}
	closedir(d);
	std::sort(names.begin(), names.end());

	for (size_t i = 0; i < names.size(); i++)
	{
		if (!offered(names[i]))
			continue;
		if (!root.empty() && ownsDevice(names[i], root))
			continue;

		DiskInfo info;
		info.name = names[i];
		out.push_back(info);
	}
	return out;
}

bool isUserDevice(const std::vector<DiskInfo> &listed, const std::string &name)
{
	for (size_t i = 0; i < listed.size(); i++)
	{
		if (ownsDevice(listed[i].name, name))
			return true;
	}
	return false;
}

bool isUserDevice(const std::string &name)
{
	return isUserDevice(disks(), name);
}

} // namespace storage
} // namespace coreapi
