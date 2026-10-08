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

#include "system/debug.h"
#include "system/helpers.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

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
const char *filesystems_path = "/proc/filesystems";
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

// The first line of a file in sysfs without its padding, empty where it is not
// there. The vendor of a disk is padded with blanks to a fixed width.
std::string firstLine(const std::string &path)
{
	FILE *f = fopen(path.c_str(), "r");
	if (f == NULL)
		return "";
	char line[256] = { 0 };
	if (fgets(line, sizeof(line), f) == NULL)
		line[0] = 0;
	fclose(f);

	std::string out(line);
	while (!out.empty() && isspace((unsigned char) out[out.size() - 1]))
		out.erase(out.size() - 1);
	size_t from = 0;
	while (from < out.size() && isspace((unsigned char) out[from]))
		from++;
	return out.substr(from);
}

std::string inDevDir(const std::string &devpath)
{
	return std::string(internal::dev_dir) + devpath.substr(4);
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

		const std::string base = std::string(internal::sys_block_path) + "/" + names[i];
		DiskInfo info;
		info.name = names[i];
		info.vendor = firstLine(base + "/device/vendor");
		// A device with no vendor says what kind of device it is instead,
		// which is all a card reader's device has to say.
		if (info.vendor.empty())
			info.vendor = firstLine(base + "/device/type");
		info.model = firstLine(base + "/device/model");
		if (info.model.empty())
			info.model = firstLine(base + "/device/name");
		// In sectors of 512 bytes whatever the sector size of the device is.
		info.size_bytes = strtoull(firstLine(base + "/size").c_str(), NULL, 10) * 512ULL;
		info.removable = firstLine(base + "/removable") == "1";
		info.optical = startsWith(names[i], "sr");
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

namespace internal
{

bool plainDeviceName(const std::string &name)
{
	if (name.empty())
		return false;
	for (size_t i = 0; i < name.size(); i++)
	{
		const unsigned char c = (unsigned char) name[i];
		if (!isalnum(c) && c != '_' && c != '-' && c != '.')
			return false;
	}
	return name != "." && name != "..";
}

std::string shellQuote(const std::string &word)
{
	std::string out = "'";
	for (size_t i = 0; i < word.size(); i++)
	{
		if (word[i] == '\'')
			out += "'\\''";
		else
			out += word[i];
	}
	return out + "'";
}

std::string mountCommand(const std::string &name)
{
#ifdef ASSUME_MDEV
	return "ACTION=add MDEV=" + name + " /lib/mdev/fs/mdev-mount";
#else
	return "mount /dev/" + name + " /media/" + name;
#endif
}

std::string umountCommand(const std::string &name)
{
#ifdef ASSUME_MDEV
	return "ACTION=remove MDEV=" + name + " /lib/mdev/fs/mdev-mount";
#else
	return "umount /media/" + name;
#endif
}

std::string mkfsCommand(const std::string &mkfs, const std::string &options,
			const std::string &labelswitch, const std::string &label,
			const std::string &partition)
{
	std::string cmd = mkfs + " " + options + " ";
	if (!labelswitch.empty() && !label.empty())
		cmd += labelswitch + " " + shellQuote(label) + " ";
	return cmd + "/dev/" + partition;
}

} // namespace internal

namespace
{

// The table the setting that picks a file system counts in.
struct FsRow
{
	const char *fmt;
	const char *fsck;
	const char *fsck_options;
	const char *mkfs;
	const char *mkfs_options;
	const char *mkfs_labelswitch;
};

const FsRow kFsRows[] = {
	{ "ext4",  "fsck.ext4",  "-C 1 -f -y", "mkfs.ext4",  "-m 0", "-L" },
	{ "ext3",  "fsck.ext3",  "-C 1 -f -y", "mkfs.ext3",  "-m 0", "-L" },
	{ "ext2",  "fsck.ext2",  "-C 1 -f -y", "mkfs.ext2",  "-m 0", "-L" },
	{ "f2fs",  "fsck.f2fs",  "",           "mkfs.f2fs",  "-f",   "-l" },
	{ "vfat",  "fsck.vfat",  "-a",         "mkfs.vfat",  "",     "-n" },
	{ "exfat", "fsck.exfat", "",           "mkfs.exfat", "",     "-n" },
	{ "xfs",   "xfs_repair", "",           "mkfs.xfs",   "-f",   "-L" },
};

void run(const std::string &cmd)
{
	dprintf(DEBUG_NORMAL, "storage: running [%s]\n", cmd.c_str());
	const int rc = system(cmd.c_str());
	(void) rc;
}

bool writeWord(const char *path, const char *word)
{
	FILE *f = fopen(path, "w");
	if (f == NULL)
		return false;
	fprintf(f, "%s\n", word);
	fclose(f);
	return true;
}

// The program that hears of a device through the kernel is told to ignore it
// while the disk is rewritten, or it would mount each half-made partition.
void hotplug(bool on)
{
#ifndef ASSUME_MDEV
	writeWord("/proc/sys/kernel/hotplug", on ? "/sbin/mdev" : "none");
#else
	(void) on;
#endif
}

const char kNoMountFlag[] = "/tmp/.nomdevmount";

#ifdef ASSUME_MDEV
// The program that mounts what the kernel announces is asked through the
// device's own uevent, because it only learns of a partition that way.
void announce(const std::string &disk, const std::string &partition)
{
	const std::string path = std::string(internal::sys_block_path) + "/" + disk + "/" + partition + "/uevent";
	if (access(path.c_str(), W_OK) != 0)
		return;
	dprintf(DEBUG_NORMAL, "storage: triggering add uevent in %s\n", path.c_str());
	if (!writeWord(path.c_str(), "add"))
		dprintf(DEBUG_NORMAL, "storage: could not open %s\n", path.c_str());
}

void waitForNode(const std::string &node, int seconds)
{
	for (int waited = 0; access(node.c_str(), W_OK) != 0; waited++)
	{
		if (waited >= seconds)
		{
			dprintf(DEBUG_NORMAL, "storage: device %s did not appear\n", node.c_str());
			return;
		}
		if (waited == 0)
			dprintf(DEBUG_NORMAL, "storage: waiting for %s\n", node.c_str());
		sleep(1);
	}
}
#endif

// The partition table command and what it is told on its input. Empty when
// the box has no tool for it.
std::string partitionCommand(const std::string &devnode, std::string &input)
{
	input.clear();
	const std::string sfdisk = find_executable("sfdisk");
	const std::string sgdisk = find_executable("sgdisk");
	const std::string fdisk = find_executable("fdisk");
	if (!sfdisk.empty())
		return "echo 'label: gpt\n;' | " + sfdisk + " -f " + devnode;
	if (!sgdisk.empty())
		return sgdisk + " -Z -N 0 " + devnode;
	if (!fdisk.empty())
	{
		input = "o\nn\np\n1\n2048\n\nw\n";
		return fdisk + " -u " + devnode;
	}
	return "";
}

// Whether the kernel lists it, with the tools resolved. The shipped order.
void resolve(FsTool &t, const std::set<std::string> &kernel)
{
	if (kernel.find(t.fmt) == kernel.end())
		return;
	const std::string fsck = find_executable(t.fsck.c_str());
	if (!fsck.empty())
	{
		t.fsck = fsck;
		t.fsck_supported = true;
	}
	const std::string mkfs = find_executable(t.mkfs.c_str());
	if (!mkfs.empty())
	{
		t.mkfs = mkfs;
		t.mkfs_supported = true;
	}
}

void makeDirectories(const std::string &mounted)
{
	static const char *const dirs[] = {
		"movies", "pictures", "epg", "music", "logos", "logos/events", "plugins"
	};
	for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++)
		safe_mkdir((mounted + "/" + dirs[i]).c_str());
}

} // namespace

std::set<std::string> kernelFilesystems()
{
	std::set<std::string> out;
	FILE *f = fopen(internal::filesystems_path, "r");
	if (f == NULL)
		return out;
	char line[128]; // lines are shorter
	while (fgets(line, sizeof(line), f))
	{
		size_t l = strlen(line);
		if (l > 0)
			line[l - 1] = 0;
		// "nodev" lines carry a tab before the name, the others start with one.
		char *tab = strchr(line, '\t');
		if (tab)
			out.insert(std::string(tab + 1));
	}
	fclose(f);
	return out;
}

std::vector<FsTool> fsTools()
{
	const std::set<std::string> kernel = kernelFilesystems();
	std::vector<FsTool> out;
	for (size_t i = 0; i < sizeof(kFsRows) / sizeof(kFsRows[0]); i++)
	{
		FsTool t;
		t.fmt = kFsRows[i].fmt;
		t.fsck = kFsRows[i].fsck;
		t.fsck_options = kFsRows[i].fsck_options;
		t.mkfs = kFsRows[i].mkfs;
		t.mkfs_options = kFsRows[i].mkfs_options;
		t.mkfs_labelswitch = kFsRows[i].mkfs_labelswitch;
		resolve(t, kernel);
		out.push_back(t);
	}
	return out;
}

bool isMounted(const std::string &name)
{
	const std::string devpath = startsWith(name, "/dev/") ? name : "/dev/" + name;
	// Every line, so that a mount that another one covers still counts.
	std::vector<internal::MountLine> table;
	if (!internal::readMountTable(table, true))
		return false;

	char buf[PATH_MAX];
	std::string real;
	if (realpath(inDevDir(devpath).c_str(), buf) != NULL)
		real = buf;

	for (size_t i = 0; i < table.size(); i++)
	{
		const std::string &dev = table[i].device;
		// Only real devices are interesting, and a line that names none is
		// no answer for a device that has no node to compare either.
		if (!startsWith(dev, "/"))
			continue;
		if (dev == devpath)
			return true;
		// A mount made through a link, such as by label.
		if (startsWith(dev, "/dev/") && realpath(inDevDir(dev).c_str(), buf) != NULL && real == buf)
			return true;
	}
	return false;
}

bool eject(const std::string &name, bool load)
{
	if (!internal::plainDeviceName(name))
		return false;
	const std::string tool = find_executable("eject");
	if (tool.empty())
		return false;
	run(tool + (load ? " -t /dev/" : " /dev/") + name);
	return true;
}

bool mount(const std::string &name)
{
	if (!internal::plainDeviceName(name))
		return false;
	if (startsWith(name, "sr") && eject(name, true))
		sleep(3);
#ifndef ASSUME_MDEV
	safe_mkdir(("/media/" + name).c_str());
#endif
	run(internal::mountCommand(name));
	return isMounted(name);
}

bool umount(const std::string &name)
{
	if (!internal::plainDeviceName(name))
		return false;
#ifdef ASSUME_MDEV
	run(internal::umountCommand(name));
#else
	if (::umount(("/media/" + name).c_str()))
		return false;
#endif
	if (startsWith(name, "sr"))
		eject(name, false);
	return !isMounted(name);
}

namespace internal
{

std::vector<std::string> partitionsToUnmount(const std::string &disk)
{
	std::vector<std::string> out;
	// Every line, so that a partition under another mount is found and
	// answered as still mounted instead of being taken for gone.
	std::vector<MountLine> table;
	if (!readMountTable(table, true))
		return out;

	// Last mounted first, and each partition once: the unmount works on the
	// partition, so a second line for it would be asked of a path that is
	// already gone and answer a failure for something that worked.
	for (size_t i = table.size(); i-- > 0;)
	{
		if (!startsWith(table[i].device, "/dev/"))
			continue;
		const std::string leaf = kernelName(table[i].device);
		if (leaf.empty() || !ownsDevice(disk, leaf))
			continue;
		if (std::find(out.begin(), out.end(), leaf) == out.end())
			out.push_back(leaf);
	}
	return out;
}

} // namespace internal

bool umountAll(const std::string &disk)
{
	const std::vector<std::string> parts = internal::partitionsToUnmount(disk);
	bool all = true;
	for (size_t i = 0; i < parts.size(); i++)
		all = umount(parts[i]) && all;
	return all;
}

FormatResult format(const std::string &disk, const std::string &fs, const std::string &label,
		    bool makeDirs, FormatObserver *observer)
{
	if (!internal::plainDeviceName(disk))
		return FormatResult::BadDevice;

	const std::vector<FsTool> tools = fsTools();
	const FsTool *tool = NULL;
	for (size_t i = 0; i < tools.size(); i++)
	{
		if (tools[i].fmt == fs)
			tool = &tools[i];
	}
	if (tool == NULL || !tool->mkfs_supported)
		return FormatResult::UnknownFilesystem;

	if (!umountAll(disk))
		return FormatResult::Busy;

	const std::string devnode = "/dev/" + disk;
	const std::string partition = partitionName(disk, 1);

	hotplug(false);
	close(creat(kNoMountFlag, 00660));
	if (observer != NULL)
	{
		observer->begin();
		observer->global(0);
	}

	FormatResult result = FormatResult::Done;
	std::string input;
	const std::string partcmd = partitionCommand(devnode, input);
	if (partcmd.empty())
	{
		result = FormatResult::NoPartitionTool;
	}
	else
	{
		dprintf(DEBUG_NORMAL, "storage::format: executing %s\n", partcmd.c_str());
		if (observer != NULL)
			observer->message(partcmd);
#ifdef ASSUME_MDEV
		// mdev makes it again and the wait below is for that.
		unlink(("/dev/" + partition).c_str());
#endif
		FILE *f = popen(partcmd.c_str(), "w");
		if (f == NULL)
		{
			result = FormatResult::PartitionFailed;
		}
		else
		{
			if (observer != NULL)
				observer->tableChanged();
			fputs(input.c_str(), f);
			if (pclose(f) != 0)
				result = FormatResult::PartitionFailed;
		}
	}

	std::string mkfscmd;
	if (result == FormatResult::Done)
	{
		sleep(2);
#ifdef ASSUME_MDEV
		announce(disk, partition);
		waitForNode("/dev/" + partition, 30);
#endif
		mkfscmd = internal::mkfsCommand(tool->mkfs, tool->mkfs_options, tool->mkfs_labelswitch, label, partition);
		dprintf(DEBUG_NORMAL, "storage::format: mkfs cmd [%s]\n", mkfscmd.c_str());
		if (observer != NULL)
			observer->message(mkfscmd);
		umountAll(disk);

		FILE *f = popen(mkfscmd.c_str(), "r");
		if (f == NULL)
		{
			result = FormatResult::MkfsFailed;
		}
		else
		{
			char buf[256];
			setbuf(f, NULL);
			int n, t, in, pos = 0, stage = 0;
			buf[0] = 0;
			while ((in = fgetc(f)) != EOF)
			{
				buf[pos++] = (char) in;
				buf[pos] = 0;
				if (in == '\b' || in == '\n' || pos >= (int) sizeof(buf) - 1)
					pos = 0; // start a new line
				if (observer == NULL)
					continue;
				switch (stage) {
					case 0:
						if (strcmp(buf, "Writing inode tables:") == 0) {
							stage++;
							observer->global(20);
							observer->message(buf);
						}
						break;
					case 1:
						if (in == '\b' && sscanf(buf, "%d/%d\b", &n, &t) == 2) {
							if (t == 0)
								t = 1;
							const int percent = 100 * n / t;
							observer->local(percent);
							observer->global(20 + percent / 5);
						}
						if (strstr(buf, "done")) {
							stage++;
							pos = 0;
						}
						break;
					case 2:
						if (strstr(buf, "blocks):") && sscanf(buf, "Creating journal (%d blocks):", &n) == 1) {
							observer->local(0);
							observer->global(60);
							observer->message(buf);
							pos = 0;
						}
						if (strstr(buf, "done")) {
							stage++;
							pos = 0;
						}
						break;
					case 3:
						if (strcmp(buf, "Writing superblocks and filesystem accounting information:") == 0) {
							observer->global(80);
							observer->message(buf);
							pos = 0;
						}
						break;
					default:
						break;
				}
			}
			if (observer != NULL)
				observer->local(100);
			const int rc = pclose(f);
			dprintf(DEBUG_NORMAL, "storage::format: mkfs res: %d\n", rc);
			if (observer != NULL)
				observer->global(100);
			if (rc != 0)
				result = FormatResult::MkfsFailed;
		}
	}

	if (result == FormatResult::Done)
	{
		sleep(2);
		const std::string tune2fs = find_executable("tune2fs");
		if (tool->fmt.compare(0, 3, "ext") == 0 && !tune2fs.empty())
		{
			dprintf(DEBUG_NORMAL, "storage::format: executing %s -r 0 -c 0 -i 0 /dev/%s\n", tune2fs.c_str(), partition.c_str());
			my_system(8, tune2fs.c_str(), "-r", "0", "-c", "0", "-i", "0", ("/dev/" + partition).c_str());
		}
	}

	unlink(kNoMountFlag);
	if (observer != NULL)
		observer->end();
	hotplug(true);

	if (result == FormatResult::Done && mount(partition) && makeDirs)
		makeDirectories("/media/" + partition);
	return result;
}

} // namespace storage
} // namespace coreapi
