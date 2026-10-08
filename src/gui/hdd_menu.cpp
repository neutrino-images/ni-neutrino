/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2010-2015 Stefan Seyfried
	Copyright (C) 2013-2014 martii
	Copyright (C) 2009-2014 CoolStream International Ltd

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/swap.h>
#include <sys/vfs.h>
#include <sys/wait.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/mount.h>

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>
#include "hdd_menu.h"

#include <cs_api.h> //NI
#include <coreapi/box/storage_disks.h>
#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/progresswindow.h>
#include <gui/widget/keyboard_input.h>
#include <gui/widget/settingitem.h>

#include <system/helpers.h>
#include <system/settings.h>
#include <system/debug.h>

#include <driver/screen_max.h>
#include <driver/record.h>

#define MOUNT_BASE	"/media/"

#define MKFS_LABEL_DEFAULT "records"

CHDDMenuHandler::CHDDMenuHandler()
{
	width = 58;
	show_menu = false;
	in_menu = false;
	lock_refresh = false;
	mkfs_label = MKFS_LABEL_DEFAULT;
}

CHDDMenuHandler::~CHDDMenuHandler()
{
}

CHDDMenuHandler* CHDDMenuHandler::getInstance()
{
	static CHDDMenuHandler* me = NULL;

	if(!me)
		me = new CHDDMenuHandler();

	return me;
}

int CHDDMenuHandler::filterDevName(const char * name)
{
	return coreapi::storage::isUserDevice(name) ? 1 : 0;
}

static std::string readlink(const char *path)
{
	char link[PATH_MAX + 1];
	if (realpath(path, link))
		return std::string(link);
	return "";
}

bool CHDDMenuHandler::is_mounted(const char *dev)
{
	bool res = coreapi::storage::isMounted(dev);
	printf("CHDDMenuHandler::is_mounted: dev [%s] is %s\n", dev, res ? "mounted" : "not mounted");
	return res;
}

void CHDDMenuHandler::getBlkIds()
{
	pid_t pid;
	std::string blkid = find_executable("blkid");
	printf("CHDDMenuHandler::getBlkIds: blkid = %s\n", blkid.c_str());
	if (blkid.empty())
		return;
	std::string pcmd = blkid + " -s TYPE";

	FILE* f = my_popen(pid, pcmd.c_str(), "r");
	if (!f) {
		printf("getBlkIds: cmd [%s] failed\n", pcmd.c_str());
		return;
	}

	hdd_list.clear();
	const std::vector<coreapi::storage::DiskInfo> user_disks = coreapi::storage::disks();
	char buff[512];
	while (fgets(buff, sizeof(buff), f)) {
		std::string ret = buff;
		std::string search = "TYPE=\"";
		size_t pos = ret.find(search);
		if (pos == std::string::npos)
			continue;

		ret = ret.substr(pos + search.length());
		pos = ret.find("\"");
		if (pos != std::string::npos)
			ret = ret.substr(0, pos);

		char *e = strstr(buff + 7, ":");
		if (!e)
			continue;
		*e = 0;

		hdd_s hdd;
		hdd.devname = std::string(buff + 5);
		if (!coreapi::storage::isUserDevice(user_disks, hdd.devname))
			continue;
		hdd.mounted = is_mounted(buff + 5);
		hdd.fmt = ret;
		hdd.desc = hdd.devname + " (" + hdd.fmt + ")";
		printf("device: %s filesystem %s (%s)\n", hdd.devname.c_str(), hdd.fmt.c_str(), hdd.mounted ? "mounted" : "not mounted" );
		hdd_list.push_back(hdd);
	}
	fclose(f);
	waitpid(pid, NULL, 0); /* beware of the zombie apocalypse! */
}

std::string CHDDMenuHandler::getFmtType(std::string name, std::string part)
{
	std::string ret = "";
	std::string dev = name + part;
	for (std::vector<hdd_s>::iterator it = hdd_list.begin(); it != hdd_list.end(); ++it) {
		if (it->devname == dev) {
			ret = it->fmt;
			break;
		}
	}
	printf("getFmtType: dev [%s] fmt [%s]\n", dev.c_str(), ret.c_str());
	return ret;
}

void CHDDMenuHandler::check_kernel_fs()
{
	kernel_fs_list = coreapi::storage::kernelFilesystems();
	if (kernel_fs_list.empty())
		fprintf(stderr, "CHDDMenuHandler::%s: no file systems listed by the kernel\n", __func__);
}

void CHDDMenuHandler::check_dev_tools()
{
	fs_tools = coreapi::storage::fsTools();
	for (unsigned i = 0; i < fs_tools.size(); i++)
		printf("check_dev_tools: %s: fsck (%s) %d mkfs (%s) %d\n", fs_tools[i].fmt.c_str(), fs_tools[i].fsck.c_str(), fs_tools[i].fsck_supported, fs_tools[i].mkfs.c_str(), fs_tools[i].mkfs_supported);
}

coreapi::storage::FsTool * CHDDMenuHandler::get_dev_tool(std::string fmt)
{
	for (unsigned i = 0; i < fs_tools.size(); i++) {
		if (fmt == fs_tools[i].fmt)
			return &fs_tools[i];
	}
	return NULL;
}

bool CHDDMenuHandler::mount_dev(std::string name)
{
	bool res = coreapi::storage::mount(name);
	lock_refresh = true;
	return res;
}

bool CHDDMenuHandler::umount_dev(std::string name)
{
	bool res = coreapi::storage::umount(name);
#ifndef ASSUME_MDEV
	// A refused unmount causes no event, so the next real one has to refresh.
	if (res)
#endif
		lock_refresh = true;
	return res;
}

void CHDDMenuHandler::showHint(std::string &message)
{
	CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, message.c_str());
	hintBox.paint();

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(3);
        neutrino_msg_t      msg;
        neutrino_msg_data_t data;

	while(true) {
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		if ((msg == CRCInput::RC_timeout) || (msg < CRCInput::RC_MaxRC))
			break;
		else if (msg == NeutrinoMessages::EVT_HOTPLUG) {
			g_RCInput->postMsg(msg, data);
			break;
		}
		else if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
			break;
	}
	hintBox.hide();
}

void CHDDMenuHandler::setRecordPath(std::string &dev)
{
	std::string newpath = std::string(MOUNT_BASE) + dev + "/movies";
	if (g_settings.network_nfs_recordingdir == newpath) {
		printf("CHDDMenuHandler::setRecordPath: recordingdir already set to %s\n", newpath.c_str());
		return;
	}
	/* don't annoy if the recordingdir is a symlink pointing to the 'right' location */
	std::string readl = readlink(g_settings.network_nfs_recordingdir.c_str());
	readl = trim(readl);
	if (newpath.compare(readl) == 0) {
		printf("CHDDMenuHandler::%s: recordingdir is a symlink to %s\n",
					__func__, newpath.c_str());
		return;
	}
	bool old_menu = in_menu;
	in_menu = false;
	int res = ShowMsg(LOCALE_RECORDINGMENU_DEFDIR, LOCALE_HDD_SET_RECDIR, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo);
	if(res == CMsgBox::mbrYes) {
		setSettingsText(g_settings.network_nfs_recordingdir, newpath);
		CRecordManager::getInstance()->SetDirectory(g_settings.network_nfs_recordingdir);
		if(g_settings.timeshiftdir.empty())
		{
			std::string timeshiftDir = g_settings.network_nfs_recordingdir + "/.timeshift";
			safe_mkdir(timeshiftDir.c_str());
			printf("New timeshift dir: %s\n", timeshiftDir.c_str());
			CRecordManager::getInstance()->SetTimeshiftDirectory(timeshiftDir);
		}
	}
	in_menu = old_menu;
}

int CHDDMenuHandler::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	if (msg == NeutrinoMessages::EVT_HOTPLUG) {
		std::string str((char *) data);
		std::map<std::string,std::string> smap;

		if (!split_config_string(str, smap))
			return messages_return::handled;

		std::string dev;
		std::map<std::string,std::string>::iterator it = smap.find("MDEV");
		if (it != smap.end())
			dev = it->second;
		else {
			it = smap.find("DEVNAME");
			if (it == smap.end())
				return messages_return::handled;
			dev = it->second;
			if (dev.length() > 5)
				dev = dev.substr(5); /* strip off /dev/ */
		}
		printf("CHDDMenuHandler::handleMsg: MDEV=%s\n", dev.c_str());

		it = smap.find("ACTION");
		if (it == smap.end())
			return messages_return::handled;

		bool added = it->second == "add";
		// A device that was just removed is no longer there to be asked
		// what it is, so its name has to do.
		if (it->second == "remove" ? !coreapi::storage::looksLikeDisk(dev) : !filterDevName(dev.c_str()))
			return messages_return::handled;
		bool mounted = false;
		if (added) {
			/* Retry: mount may not yet be visible in /proc/mounts
			   right after the hotplug event */
			for (int i = 0; i < 10 && !mounted; i++) {
				mounted = is_mounted(dev.c_str());
				if (!mounted)
					usleep(200000);
			}
		} else {
			mounted = is_mounted(dev.c_str());
		}
		std::string tmp = dev.substr(0, 2);

		if (added && !mounted && tmp != "sr") {
			std::string message = dev + ": " + g_Locale->getText(LOCALE_HDD_MOUNT_FAILED);
		    //NI
		    if (!g_settings.hdd_format_on_mount_failed)
			showHint(message);
		    else {
			message +=  std::string(" ") + g_Locale->getText(LOCALE_HDD_FORMAT) + std::string(" ?");
			int res = ShowMsg(LOCALE_MESSAGEBOX_INFO, message, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo);
			if(res == CMsgBox::mbrYes) {
				unsigned char * p = new unsigned char[dev.size() + 1];
				if (p) {
					sprintf((char *)p, "%s", dev.c_str());
					g_RCInput->postMsg(NeutrinoMessages::EVT_FORMAT_DRIVE , (neutrino_msg_data_t)p);
					return messages_return::handled | messages_return::cancel_all;
				}
			}
		    }
		} else {
			std::string message = dev + ": " + (added ?
					g_Locale->getText(mounted ? LOCALE_HDD_MOUNT_OK : LOCALE_HDD_MOUNT_FAILED)
					: g_Locale->getText(LOCALE_HDD_UMOUNTED));
			showHint(message);
			if (added && tmp != "sr" && g_settings.hdd_allow_set_recdir) //NI
				setRecordPath(dev);
		}
		if (in_menu && !lock_refresh) {
			show_menu = true;
			return messages_return::handled | messages_return::cancel_all;
		}
		lock_refresh = false;
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_FORMAT_DRIVE) {
		std::string dev((char *) data);
		printf("NeutrinoMessages::EVT_FORMAT_DRIVE: [%s]\n", dev.c_str());
		check_dev_tools();
		getBlkIds();
		scanDevices();
		for (std::map<std::string, std::string>::iterator it = devtitle.begin(); it != devtitle.end(); ++it) {
			if (coreapi::storage::ownsDevice(it->first, dev)) {
				showDeviceMenu(it->first);
				break;
			}
		}
		hdd_list.clear();
		devtitle.clear();
		return messages_return::handled;
	}
	return messages_return::unhandled;
}

int CHDDMenuHandler::exec(CMenuTarget* parent, const std::string &actionkey)
{
	if (parent)
		parent->hide();

	if (actionkey.empty())
		return doMenu();

	std::string dev = actionkey.substr(1);
	printf("CHDDMenuHandler::exec actionkey %s dev %s\n", actionkey.c_str(), dev.c_str());
	if (actionkey[0] == 'm') {
		for (std::vector<hdd_s>::iterator it = hdd_list.begin(); it != hdd_list.end(); ++it) {
			if (it->devname == dev) {
				CHintBox hintbox(it->mounted ? LOCALE_HDD_UMOUNT : LOCALE_HDD_MOUNT, it->devname.c_str());
				hintbox.paint();
				if  (it->mounted)
					umount_dev(it->devname);
				else
					mount_dev(it->devname);

				it->mounted = is_mounted(it->devname.c_str());
				it->cmf->setOption(it->mounted ? umount : mount);
				hintbox.hide();
				return menu_return::RETURN_REPAINT;
			}
		}
	}
	else if (actionkey[0] == 'd') {
		return showDeviceMenu(dev);
	}
	else if (actionkey[0] == 'c') {
		return checkDevice(dev);
	}
	else if (actionkey[0] == 'f') {
		int ret = formatDevice(dev);
#if 0
		std::string devname = "/dev/" + coreapi::storage::partitionName(dev, 1);
		if (show_menu && is_mounted(devname.c_str())) {
			devname = coreapi::storage::partitionName(dev, 1);
			setRecordPath(devname);
		}
#endif
		return ret;
	}
	return menu_return::RETURN_REPAINT;
}

int CHDDMenuHandler::showDeviceMenu(std::string dev)
{
	printf("CHDDMenuHandler::showDeviceMenu: dev %s\n", dev.c_str());
	CMenuWidget* hddmenu = new CMenuWidget(devtitle[dev].c_str(), NEUTRINO_ICON_SETTINGS);
	hddmenu->addIntroItems();

	CMenuForwarder * mf;

	std::string fmt_type = getFmtType(coreapi::storage::partitionName(dev, 1));
	bool fsck_enabled = false;
	for (unsigned i = 0; i < fs_tools.size(); i++) {
		if (fmt_type == fs_tools[i].fmt)
			g_settings.hdd_fs = i;
	}
	int cnt = 0;
	bool found = false;
	for (std::vector<hdd_s>::iterator it = hdd_list.begin(); it != hdd_list.end(); ++it) {
		if (coreapi::storage::ownsDevice(dev, it->devname)) {
			printf("found %s partition %s\n", dev.c_str(), it->devname.c_str());
			fsck_enabled = false;
			coreapi::storage::FsTool * devtool = get_dev_tool(it->fmt);
			if (devtool) {
				fsck_enabled = devtool->fsck_supported;
			}

			std::string key = "c" + it->devname;
			mf = new CMenuForwarder(LOCALE_HDD_CHECK, fsck_enabled, it->desc, this, key.c_str());
			mf->setHint("", LOCALE_MENU_HINT_HDD_CHECK);
			hddmenu->addItem(mf);
			found = true;
			cnt++;
		}
	}

	if (found)
		hddmenu->addItem(new CMenuSeparator(CMenuSeparator::LINE));

	// Left out where the box has no mkfs at all.
	addSetting(hddmenu, "hdd_fs", true, NULL, CRCInput::RC_nokey, false, true);

	char hint2[1024];
	snprintf(hint2, sizeof(hint2)-1, g_Locale->getText(LOCALE_HDD_LABEL_HINT2), MKFS_LABEL_DEFAULT);
	CKeyboardInput choseLabel((std::string) g_Locale->getText(LOCALE_HDD_LABEL), &mkfs_label, 0, NULL, NULL, (std::string) g_Locale->getText(LOCALE_HDD_LABEL_HINT1), (std::string) hint2);
	mf = new CMenuForwarder(LOCALE_HDD_LABEL, true, mkfs_label, &choseLabel);
	mf->setHint("", LOCALE_MENU_HINT_HDD_LABEL);
	hddmenu->addItem(mf);

	std::string key = "f" + dev;
	mf = new CMenuForwarder(LOCALE_HDD_FORMAT, true, "", this, key.c_str());
	mf->setHint("", LOCALE_MENU_HINT_HDD_FORMAT);
	hddmenu->addItem(mf);

	int res = hddmenu->exec(NULL, "");
	delete hddmenu;
	return res;
}

bool CHDDMenuHandler::scanDevices()
{
	const std::vector<coreapi::storage::DiskInfo> user_disks = coreapi::storage::disks();
	const int n = user_disks.size();

	for(int i = 0; i < n;i++) {
		char str[281];
		const coreapi::storage::DiskInfo &disk = user_disks[i];
		const int64_t megabytes = disk.size_bytes / 1000000;

		printf("HDD: checking /sys/block/%s\n", disk.name.c_str());

		std::string dev = disk.name.substr(0, 2);
		std::string fmt = getFmtType(disk.name);
		/* epmty cdrom do not appear in blkid output */
		if (fmt.empty() && dev == "sr") {
			hdd_s hdd;
			hdd.devname = disk.name;
			hdd.mounted = false;
			hdd.fmt = "";
			hdd.desc = hdd.devname;
			hdd.cmf = NULL;
			hdd_list.push_back(hdd);
		}

		snprintf(str, sizeof(str), "%s %s %ld %s", disk.vendor.c_str(), disk.model.c_str(), (long)(megabytes < 10000 ? megabytes : megabytes/1000), megabytes < 10000 ? "MB" : "GB");
		printf("HDD: %s\n", str);
		devtitle[disk.name] = str;
	}
	return !devtitle.empty();
}

int CHDDMenuHandler::doMenu()
{
	show_menu = false;
	in_menu = true;

	check_kernel_fs();
	check_dev_tools();

	int ret;
	bool again;
	do {
		getBlkIds();
		scanDevices();

		CMenuWidget* hddmenu = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_DRIVESETUP);

		hddmenu->addIntroItems(LOCALE_HDD_SETTINGS, LOCALE_HDD_EXTENDED_SETTINGS);

		CHDDDestExec hddexec;
		CMenuForwarder * mf = new CMenuForwarder(LOCALE_HDD_ACTIVATE, true, "", &hddexec, NULL, CRCInput::RC_red);
		mf->setHint("", LOCALE_MENU_HINT_HDD_APPLY);
		hddmenu->addItem(mf);

		addSetting(hddmenu, "hdd_sleep");

		std::string hdparm = find_executable("hdparm");
		printf("CHDDMenuHandler::doMenu: hdparm = %s\n", hdparm.c_str());
		struct stat stat_buf;
		bool have_nonbb_hdparm = !::lstat(hdparm.c_str(), &stat_buf) && !S_ISLNK(stat_buf.st_mode);
		if (have_nonbb_hdparm)
			addSetting(hddmenu, "hdd_noise");

		//NI
		int fake_hddpower = 0;
		CTouchFileNotifier * hddpowerNotifier = NULL;
		hddmenu->addItem(new CMenuSeparator());
		if (cs_get_revision() < 8) {
			//NI HDD power (HD1/BSE only)
			const char *flag_hddpower = FLAGDIR "/.hddpower";
			fake_hddpower = file_exists(flag_hddpower);
			hddpowerNotifier = new CTouchFileNotifier(flag_hddpower);
			CMenuOptionChooser *mc = new CMenuOptionChooser(LOCALE_HDD_POWER, &fake_hddpower, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, hddpowerNotifier, CRCInput::RC_yellow);
			mc->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_HDD_POWER);
			hddmenu->addItem(mc);
			hddmenu->addItem(new CMenuSeparator());
		}
		addSetting(hddmenu, "hdd_format_on_mount_failed");
		addSetting(hddmenu, "hdd_wakeup");
		addSetting(hddmenu, "hdd_wakeup_msg");
		hddmenu->addItem(new CMenuSeparator());
		addSetting(hddmenu, "hdd_allow_set_recdir");

		hddmenu->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_HDD_MANAGE));

		for (std::map<std::string, std::string>::iterator it = devtitle.begin(); it != devtitle.end(); ++it) {
			std::string dev = it->first.substr(0, 2);
			bool enabled = !CNeutrinoApp::getInstance()->recordingstatus && dev != "sr";
			std::string key = "d" + it->first;
			mf = new CMenuForwarder(it->first, enabled, it->second, this, key.c_str());
			mf->setHint("", LOCALE_MENU_HINT_HDD_TOOLS);
			hddmenu->addItem(mf);
		}

		if(devtitle.empty()) {
			//if no drives found, select 'back'
			if (hddmenu->getSelected() != -1)
				hddmenu->setSelected(2);
			hddmenu->addItem(new CMenuForwarder(LOCALE_HDD_NOT_FOUND, false));
		}

		if (!hdd_list.empty()) {
			struct stat rec_st, root_st, dev_st;
			memset(&rec_st, 0, sizeof(rec_st));
			memset(&root_st, 0, sizeof(root_st));
			stat(g_settings.network_nfs_recordingdir.c_str(), &rec_st);
			stat("/", &root_st);

			sort(hdd_list.begin(), hdd_list.end(), cmp_hdd_by_name());
			mount = g_Locale->getText(LOCALE_HDD_MOUNT);
			umount = g_Locale->getText(LOCALE_HDD_UMOUNT);
			int shortcut = 1;
			hddmenu->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_HDD_MOUNT_UMOUNT));
			for (std::vector<hdd_s>::iterator it = hdd_list.begin(); it != hdd_list.end(); ++it) {
				const char * rec_icon = NULL;
				if (it->mounted) {
					std::string dst = MOUNT_BASE + it->devname;
					if (!stat(dst.c_str(), &stat_buf) && rec_st.st_dev == stat_buf.st_dev)
						rec_icon = CNeutrinoApp::getInstance()->recordingstatus ? NEUTRINO_ICON_MARKER_RECORD : NEUTRINO_ICON_MARKER_RECORD_GRAY;
				}
				std::string key = "m" + it->devname;
				bool enabled = !rec_icon || !CNeutrinoApp::getInstance()->recordingstatus;
				/* do not allow to unmount the rootfs, and skip filesystems without kernel support */
				memset(&dev_st, 0, sizeof(dev_st));
				if (stat(("/dev/" + it->devname).c_str(), &dev_st) != -1
				    && dev_st.st_rdev == root_st.st_dev)
					enabled = false;
				else if (kernel_fs_list.find(it->fmt) == kernel_fs_list.end())
					enabled = false;
				it->cmf = new CMenuForwarder(it->desc, enabled, it->mounted ? umount : mount , this,
						key.c_str(), CRCInput::convertDigitToKey(shortcut++), NULL, rec_icon);
				hddmenu->addItem(it->cmf);
			}
		}

		ret = hddmenu->exec(NULL, "");
		if (hddpowerNotifier)
			delete hddpowerNotifier;
		delete hddmenu;
		hdd_list.clear();
		devtitle.clear();
		again = show_menu;
		show_menu = false;
	} while (again);
	in_menu = false;
	return ret;
}

#if 0
static int dev_umount(char *dev)
{
	char buffer[255];
	FILE *f = fopen("/proc/mounts", "r");
	if(f == NULL)
		return -1;
	while (fgets (buffer, 255, f) != NULL) {
		char *p = buffer + strlen(dev);
		if (strstr(buffer, dev) == buffer && *p == ' ') {
			p++;
			char *q = strchr(p, ' ');
			if (q == NULL)
				continue;
			*q = 0x0;
			fclose(f);
			printf("dev_umount %s: umounting %s\n", dev, p);
			return umount(p);
		}
	}
#ifndef ASSUME_MDEV
	/* with mdev, we hopefully don't have to umount anything here... */
	printf("dev_umount %s: not found\n", dev);
#endif
	errno = ENOENT;
	fclose(f);
	return -1;
}

/* unmounts all partitions of a given block device, dev can be /dev/sda, sda or sda4 */
static int umount_all(const char *dev)
{
	char buffer[255];
	int i;
	char *d = strdupa(dev);
	char *p = d + strlen(d) - 1;
	while (isdigit(*p))
		p--;
	*++p = 0x0;
	if (strstr(d, "/dev/") == d)
		d += strlen("/dev/");
	printf("HDD: %s dev = '%s' d = '%s'\n", __func__, dev, d);
	for (i = 1; i < 16; i++)
	{
		sprintf(buffer, "/dev/%s", coreapi::storage::partitionName(d, i).c_str());
		// printf("checking for '%s'\n", buffer);
		if (access(buffer, R_OK))
			continue;	/* device does not exist? */
#ifdef ASSUME_MDEV
		/* we can't use a 'remove' uevent, as that would also remove the device node
		 * which we certainly need for formatting :-) */
		if (! access("/etc/mdev/mdev-mount.sh", X_OK)) {
			sprintf(buffer, "MDEV=%s ACTION=remove /etc/mdev/mdev-mount.sh block", coreapi::storage::partitionName(d, i).c_str());
			printf("-> running '%s'\n", buffer);
			my_system(3, "/bin/sh", "-c", buffer);
		}
#endif
		sprintf(buffer, "/dev/%s", coreapi::storage::partitionName(d, i).c_str());
		/* just to make sure */
		swapoff(buffer);
		if (dev_umount(buffer) && errno != ENOENT)
			fprintf(stderr, "could not umount %s: %m\n", buffer);
	}
	return 0;
}

/* triggers a uevent for all partitions of a given blockdev, dev can be /dev/sda, sda or sda4 */
static int mount_all(const char *dev)
{
	char buffer[255];
	int i, ret = -1;
	char *d = strdupa(dev);
	char *p = d + strlen(d) - 1;
	while (isdigit(*p))
		p--;
	if (strstr(d, "/dev/") == d)
		d += strlen("/dev/");
	*++p = 0x0;
	printf("HDD: %s dev = '%s' d = '%s'\n", __func__, dev, d);
	for (i = 1; i < 16; i++)
	{
#ifdef ASSUME_MDEV
		sprintf(buffer, "/sys/block/%s/%s/uevent", d, coreapi::storage::partitionName(d, i).c_str());
		if (!access(buffer, W_OK)) {
			FILE *f = fopen(buffer, "w");
			if (!f)
				fprintf(stderr, "HDD: %s could not open %s: %m\n", __func__, buffer);
			else {
				printf("-> triggering add uevent in %s\n", buffer);
				fprintf(f, "add\n");
				fclose(f);
				ret = 0;
			}
		}
#endif
	}
	return ret;
}

#ifdef ASSUME_MDEV
static void waitfordev(const char *src, int maxwait)
{
	int waitcount = 0;
	/* wait for the device to show up... */
	while (access(src, W_OK)) {
		if (!waitcount)
			printf("CHDDFmtExec: waiting for %s", src);
		else
			printf(".");
		fflush(stdout);
		waitcount++;
		if (waitcount > maxwait) {
			fprintf(stderr, "CHDDFmtExec: device %s did not appear!\n", src);
			break;
		}
		sleep(1);
	}
	if (waitcount && waitcount <= maxwait)
		printf("\n");
}
#else
static void waitfordev(const char *, int)
{
}
#endif
#endif

void CHDDMenuHandler::showError(neutrino_locale_t err)
{
	ShowMsg(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(err), CMsgBox::mbrOk, CMsgBox::mbOk);
}

/* The window the format progress is drawn in. It is made when the box layer
   starts its first command and goes when the last one ends, so a refusal
   before that shows nothing. */
class CHDDFormatProgress : public coreapi::storage::FormatObserver
{
	public:
		CHDDFormatProgress() : progress(NULL), table_changed(false) {}
		~CHDDFormatProgress() { end(); }

		void begin()
		{
			progress = new CProgressWindow();
			progress->setTitle(LOCALE_HDD_FORMAT);
			progress->exec(NULL, "");
		}
		void message(const std::string &text) { if (progress) progress->showStatusMessageUTF(text.c_str()); }
		void global(int percent) { if (progress) progress->showGlobalStatus(percent); }
		void local(int percent) { if (progress) progress->showLocalStatus(percent); }
		void tableChanged() { table_changed = true; }
		void end()
		{
			if (!progress)
				return;
			progress->hide();
			delete progress;
			progress = NULL;
		}

		bool tableWasChanged() const { return table_changed; }

	private:
		CProgressWindow *progress;
		bool table_changed;
};

int CHDDMenuHandler::formatDevice(std::string dev)
{
	printf("CHDDMenuHandler::formatDevice: dev %s hdd_fs %d\n", dev.c_str(), g_settings.hdd_fs);

	if (g_settings.hdd_fs < 0 || g_settings.hdd_fs >= (int) fs_tools.size())
		return menu_return::RETURN_REPAINT;

	const coreapi::storage::FsTool &devtool = fs_tools[g_settings.hdd_fs];
	if (!devtool.mkfs_supported) {
		printf("CHDDMenuHandler::formatDevice: mkfs.%s is not supported\n", devtool.fmt.c_str());
		return menu_return::RETURN_REPAINT;
	}

	int res = ShowMsg(LOCALE_HDD_FORMAT, g_Locale->getText(LOCALE_HDD_FORMAT_WARN), CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo );
	if(res != CMsgBox::mbrYes)
		return menu_return::RETURN_REPAINT;

	//NI bool srun = my_system(3, "killall", "-9", "smbd");

	CHDDFormatProgress progress;
	coreapi::storage::FormatResult result = coreapi::storage::format(dev, devtool.fmt, mkfs_label,
			mkfs_label == MKFS_LABEL_DEFAULT, &progress);
	progress.end();
	// Only a format that got as far as mounting makes a mount event the menu
	// causes itself. After a refusal the next real event has to refresh it.
	if (result == coreapi::storage::FormatResult::Done)
		lock_refresh = true;
	if (progress.tableWasChanged())
		show_menu = true;

	switch (result) {
		case coreapi::storage::FormatResult::Done:
			break;
		case coreapi::storage::FormatResult::Busy:
			showError(LOCALE_HDD_UMOUNT_WARN);
			break;
		default:
			showError(LOCALE_HDD_FORMAT_FAILED);
			break;
	}

	//NI if (!srun) my_system(1, "smbd");
	if (show_menu)
		return menu_return::RETURN_EXIT_ALL;

	return menu_return::RETURN_REPAINT;
}

int CHDDMenuHandler::checkDevice(std::string dev)
{
	int res;
	FILE * f;
	CProgressWindow * progress;
	int oldpass = 0, pass, step, total;
	int percent = 0, opercent = 0;
	char buf[256] = { 0 };

	bool loop;
	uint64_t timeoutEnd;
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	std::string devname = "/dev/" + dev;

	printf("CHDDMenuHandler::checkDevice: dev %s\n", dev.c_str());

	std::string fmt = getFmtType(dev);
	coreapi::storage::FsTool * devtool = get_dev_tool(fmt);
	if (!devtool || !devtool->fsck_supported)
		return menu_return::RETURN_REPAINT;

	std::string cmd = devtool->fsck + " " + devtool->fsck_options + " " + devname;
	printf("fsck cmd: [%s]\n", cmd.c_str());

	//NI bool srun = my_system(3, "killall", "-9", "smbd");

	res = true;
	if (is_mounted(dev.c_str()))
		res = umount_dev(dev);

	printf("CHDDMenuHandler::checkDevice: umount res %d\n", res);
	if(!res) {
		showError(LOCALE_HDD_UMOUNT_WARN);
		return menu_return::RETURN_REPAINT;
	}

	printf("CHDDMenuHandler::checkDevice: Executing %s\n", cmd.c_str());
	f=popen(cmd.c_str(), "r");
	if(!f) {
		showError(LOCALE_HDD_CHECK_FAILED);
	} else {
		progress = new CProgressWindow();
		progress->setTitle(LOCALE_HDD_CHECK);
		progress->exec(NULL,"");
		progress->showStatusMessageUTF(cmd.c_str());

		while(fgets(buf, 255, f) != NULL)
		{
			if(isdigit(buf[0])) {
				sscanf(buf, "%d %d %d\n", &pass, &step, &total);
				if(total == 0)
					total = 1;
				if(oldpass != pass) {
					oldpass = pass;
					progress->showGlobalStatus(pass > 0 ? (pass-1)*20: 0);
				}
				percent = (step * 100) / total;
				if(opercent != percent) {
					opercent = percent;
	//printf("CHDDChkExec: pass %d : %d\n", pass, percent);
					progress->showLocalStatus(percent);
				}
			}
			else {
				char *t = strrchr(buf, '\n');
				if (t)
					*t = 0;
				if(!strncmp(buf, "Pass", 4)) {
					progress->showStatusMessageUTF(buf);
				}
			}
		}
	//printf("CHDDChkExec: %s\n", buf);
		res = pclose(f);
		if(res)
			showError(LOCALE_HDD_CHECK_FAILED);

		progress->showGlobalStatus(100);
		progress->showStatusMessageUTF(buf);

		timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);
		loop = true;
		while (loop)
		{
			g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);
			if (msg == CRCInput::RC_timeout || msg == CRCInput::RC_ok || CNeutrinoApp::getInstance()->backKey(msg))
				loop = false;
		}

		progress->hide();
		delete progress;
	}

	res = mount_dev(dev);
	printf("CHDDMenuHandler::checkDevice: mount res %d\n", res);

	//NI if (!srun) my_system(1, "smbd");
	return menu_return::RETURN_REPAINT;
}

int CHDDDestExec::exec(CMenuTarget* /*parent*/, const std::string&)
{
	const std::vector<coreapi::storage::DiskInfo> user_disks = coreapi::storage::disks();
	int n = user_disks.size();

	if (g_settings.hdd_sleep > 0 && g_settings.hdd_sleep < 60)
		g_settings.hdd_sleep = 60;

	std::string hdidle = find_executable("hd-idle");
	printf("CHDDDestExec::exec: hd-idle = %s\n", hdidle.c_str());
	if (!hdidle.empty() && g_settings.hdd_sleep > 0) {
		system("kill $(pidof hd-idle)");
		int sleep_seconds = g_settings.hdd_sleep;
		switch (sleep_seconds) {
			case 241:
					sleep_seconds = 30 * 60;
					break;
			case 242:
					sleep_seconds = 60 * 60;
					break;
			default:
					sleep_seconds *= 5;
		}
		if (sleep_seconds)
			my_system(3, hdidle.c_str(), "-i", to_string(sleep_seconds).c_str());

		return menu_return::RETURN_NONE;
	}

	std::string hdparm = find_executable("hdparm");
	printf("CHDDDestExec::exec: hdparm = %s\n", hdparm.c_str());
	if (hdparm.empty())
		return menu_return::RETURN_NONE;

	struct stat stat_buf;
	bool have_nonbb_hdparm = !::lstat(hdparm.c_str(), &stat_buf) && !S_ISLNK(stat_buf.st_mode);

	for (int i = 0; i < n; i++) {
		printf("CHDDDestExec: noise %d sleep %d /dev/%s\n",
			 g_settings.hdd_noise, g_settings.hdd_sleep, user_disks[i].name.c_str());

		char M_opt[50],S_opt[50], opt[261];
		snprintf(S_opt, sizeof(S_opt), "-S%d", g_settings.hdd_sleep);
		snprintf(M_opt, sizeof(M_opt), "-M%d", g_settings.hdd_noise);
		snprintf(opt, sizeof(opt), "/dev/%s", user_disks[i].name.c_str());

		if (have_nonbb_hdparm)
			my_system(4, hdparm.c_str(), M_opt, S_opt, opt);
		else // busybox hdparm doesn't support "-M"
			my_system(3, hdparm.c_str(), S_opt, opt);
	}
	return menu_return::RETURN_NONE;
}
