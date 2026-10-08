/*
 * settingstable_lists.cpp - the lists, the lists of records and the flag files the program keeps
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

#include "coreapi/base/flagfile.h"

#include <driver/neutrino_msg_t.h>

#include <cstdlib>
#include <cstring>

namespace coreapi
{

namespace
{

/* What the struct keeps as a list is one setting however many entries it holds:
   written whole, so that the entries cannot be told apart from one another by
   anything a caller could name. A list of texts reads and writes as one text with
   a line to each; a list of records as a line to each record with a tab between
   its members. Neither separator can occur inside an entry, which the rules for
   a line of text already refuse.

   The flag files are settings the program keeps as the existence of a file: a
   file made when the setting is on and removed when it is off, with nothing of
   it in the settings file. */

/* What a record of each list holds, in the order its functions carry the members
   and the order the schema names them in. */
constexpr RecordField kUsermenuFields[] =
{
	{ "title", ValueType::String, 0, 0, false, "usermenu.name" },
	/* A key code. Nought is a button with no key: the program keeps that as a code of its
	   own and the program's own load treats nought and that code alike, so both read as
	   nought and nought writes the code, which lets what was read be written back. */
	{ "key", ValueType::Key, 0, 2147483647, false, "usermenu.key" },
	// The numbers of the entries the button opens, separated by commas.
	{ "items", ValueType::String, 0, 0, false, "usermenu.items" }
};

constexpr RecordField kRemoteboxFields[] =
{
	{ "enabled", ValueType::Bool, 0, 0, false, NULL },
	{ "address", ValueType::String, 0, 0, false, NULL },
	{ "name", ValueType::String, 0, 0, false, NULL },
	{ "user", ValueType::String, 0, 0, false, NULL },
	{ "pass", ValueType::String, 0, 0, true, NULL },
	{ "port", ValueType::Int, 1, 65535, false, NULL }
};

/* The pair of functions that carry the user menu out of the struct and back. The
   list holds records the program allocates and open screens point into them (the
   forwarders of the personalize screen hold the address of a title, the editor of a
   button its index), and a write arrives from inside such a screen's message loop. So
   a write never adds, frees or moves a record: it carries as many as there are and
   changes each in place, and the layer refuses any other count before it gets here.
   The screens leave an empty slot behind a button they took away until they tidy up,
   which a read skips, so the nth record is the nth slot that holds one. */
void readUsermenu(const SNeutrinoSettings &s, std::vector<RecordValues> &out)
{
	CSettingsTextGuard lock;
	out.clear();
	for (size_t i = 0; i < s.usermenu.size(); ++i)
	{
		const SNeutrinoSettings::usermenu_t *u = s.usermenu[i];
		if (u == NULL)
			continue;
		char key[24];
		snprintf(key, sizeof(key), "%u", u->key == RC_NOKEY ? 0u : u->key);
		RecordValues r;
		r.push_back(u->title);
		r.push_back(key);
		r.push_back(u->items);
		out.push_back(r);
	}
}

void writeUsermenu(SNeutrinoSettings &s, const std::vector<RecordValues> &in)
{
	CSettingsTextGuard lock;
	std::vector<SNeutrinoSettings::usermenu_t *> slots;
	for (size_t i = 0; i < s.usermenu.size(); ++i)
		if (s.usermenu[i] != NULL)
			slots.push_back(s.usermenu[i]);
	// A screen added or took away a button between the check and now: nothing to carry the write by.
	if (slots.size() != in.size())
	{
		// Reported here because it reaches no caller: the write was answered before this runs.
		fprintf(stderr, "coreapi: usermenu was written and a screen changed the number of buttons meanwhile, so nothing was changed\n");
		return;
	}
	for (size_t i = 0; i < in.size(); ++i)
	{
		slots[i]->title = in[i][0];
		const unsigned long key = strtoul(in[i][1].c_str(), NULL, 10);
		slots[i]->key = key == 0 ? (unsigned int) RC_NOKEY : (unsigned int) key;
		slots[i]->items = in[i][2];
	}
}

constexpr FieldExtra kUsermenuExtra =
{
	0, NULL, NULL, &readUsermenu, &writeUsermenu,
	kUsermenuFields, sizeof(kUsermenuFields) / sizeof(kUsermenuFields[0]), 0, true, false
};

/* The remote boxes a timer can be sent to. Whether one is reachable is found out
   by the screen of the timers and starts at no on every load. */
void readRemoteboxes(const SNeutrinoSettings &s, std::vector<RecordValues> &out)
{
	CSettingsTextGuard lock;
	out.clear();
	for (size_t i = 0; i < s.timer_remotebox_ip.size(); ++i)
	{
		const timer_remotebox_item &b = s.timer_remotebox_ip[i];
		char port[24];
		snprintf(port, sizeof(port), "%u", b.port);
		RecordValues r;
		r.push_back(b.enabled ? "1" : "0");
		r.push_back(b.rbaddress);
		r.push_back(b.rbname);
		r.push_back(b.user);
		r.push_back(b.pass);
		r.push_back(port);
		out.push_back(r);
	}
}

void writeRemoteboxes(SNeutrinoSettings &s, const std::vector<RecordValues> &in)
{
	CSettingsTextGuard lock;
	// As for the user menu: the timer screen holds iterators and the address of a text of a box.
	if (s.timer_remotebox_ip.size() != in.size())
	{
		fprintf(stderr, "coreapi: timer_remotebox_ip was written and a screen changed the number of boxes meanwhile, so nothing was changed\n");
		return;
	}
	for (size_t i = 0; i < in.size(); ++i)
	{
		timer_remotebox_item &b = s.timer_remotebox_ip[i];
		b.enabled = in[i][0] == "1";
		b.rbaddress = in[i][1];
		b.rbname = in[i][2];
		b.user = in[i][3];
		b.pass = in[i][4];
		b.port = (unsigned int) strtoul(in[i][5].c_str(), NULL, 10);
		// Whether it can be reached is found out by the screen of the timers and not said by a write.
	}
}

constexpr FieldExtra kRemoteboxExtra =
{
	0, NULL, NULL, &readRemoteboxes, &writeRemoteboxes,
	kRemoteboxFields, sizeof(kRemoteboxFields) / sizeof(kRemoteboxFields[0]), 0, true, false
};


/* Whether the binary a flag file stands for is on the box. The screens offer the
   flag only where it is, and a flag for a program that is not there starts
   nothing. The daemons are found on the path and the softcams in the one directory
   they are installed to, as the screen that offers them looks. */
bool daemon_fritzcallmonitor() { return executableOnPath("FritzCallMonitor"); }
bool daemon_nfsd() { return executableOnPath("rpc.nfsd"); }
bool daemon_samba() { return executableOnPath("smbd"); }
bool daemon_tuxcald() { return executableOnPath("tuxcald"); }
bool daemon_tuxmaild() { return executableOnPath("tuxmaild"); }
bool daemon_emmrd() { return executableOnPath("emmrd"); }
bool daemon_inadyn() { return executableOnPath("inadyn"); }
bool daemon_dropbear() { return executableOnPath("dropbear"); }
bool daemon_djmount() { return executableOnPath("djmount"); }
bool daemon_ushare() { return executableOnPath("ushare"); }
bool daemon_minidlnad() { return executableOnPath("minidlnad"); }
bool daemon_xupnpd() { return executableOnPath("xupnpd"); }
bool daemon_crond() { return executableOnPath("crond"); }
bool camd_mgcamd() { return fileIsThere("/var/bin/mgcamd"); }
bool camd_doscam() { return fileIsThere("/var/bin/doscam"); }
bool camd_ncam() { return fileIsThere("/var/bin/ncam"); }
bool camd_osmod() { return fileIsThere("/var/bin/osmod"); }
bool camd_oscam() { return fileIsThere("/var/bin/oscam"); }
bool camd_cccam() { return fileIsThere("/var/bin/cccam"); }
bool camd_gbox() { return fileIsThere("/var/bin/gbox"); }

#ifdef ENABLE_LCD4LINUX
// The weather line of the panel is offered only while the weather itself is on.
constexpr Condition kLcd4lWeatherOn[] =
{
	when("weather_enabled").isNot(0)
};
#endif

constexpr Descriptor kLists[] =
{
	/* The channel lists the box fetches from the net, as files or addresses. The
	   program starts from the one it ships where it has none of its own, which
	   depends on a file being there, so the default is none. */
	listRow("webtv_xml")
		.section("channel")
		.defaultValue("")
		.field(COREAPI_LIST_FIELD(webtv_xml)),
	listRow("webradio_xml")
		.section("channel")
		.defaultValue("")
		.field(COREAPI_LIST_FIELD(webradio_xml)),
	listRow("xmltv_xml")
		.section("misc")
		.defaultValue("")
		.field(COREAPI_LIST_FIELD(xmltv_xml)),
	/* The buttons of the user menu. The program starts from four coloured ones
	   with the entries it ships, whose key codes it takes from the remote control
	   of the box, so the default is none. */
	recordsRow("usermenu")
		.section("misc")
		.defaultValue("")
		.field(COREAPI_RECORDS_FIELD(usermenu, kUsermenuExtra)),
	/* A password among the members makes the whole list a credential: a read of
	   it answers nothing, a write replaces it whole and cannot be nothing. The box
	   lists it on the screen of the timers, which reads the list out of the
	   struct. Under the section that already holds credentials, because a section
	   with one in it is one no AI client may write, and this one must not widen
	   what they may. */
	recordsRow("timer_remotebox_ip")
		.section("misc")
		.defaultValue("")
		.secret()
		.field(COREAPI_RECORDS_FIELD(timer_remotebox_ip, kRemoteboxExtra)),
	// The flag files, each the existence of a file in the directory the program keeps them in.
	boolRow("flag_hddpower")
		.section("hdd")
		.label("hdd_power")
		.hint("menu.hint_hdd_power")
		.defaultValue(0)
		.availableIf(hasHddPowerFlag)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.hddpower")),
	/* The flag files of the daemons and the softcams the box starts at boot. The file is
	   the setting; the services group starts or stops the program when it appears or
	   goes. The SCART picture fix beside them moves the screen's corners and the font
	   scale with it through a coupling, so a write of it from anywhere does both. */
	boolRow("flag_daemon_fritzcallmonitor")
		.section("misc")
		.label("daemon_item.fcm_name")
		.hint("daemon_item.fcm_desc")
		.defaultValue(0)
		.availableIf(&daemon_fritzcallmonitor)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.fritzcallmonitor")),
	boolRow("flag_daemon_nfsd")
		.section("misc")
		.label("daemon_item.nfsserver_name")
		.hint("daemon_item.nfsserver_desc")
		.defaultValue(0)
		.availableIf(&daemon_nfsd)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.nfsd")),
	boolRow("flag_daemon_samba")
		.section("misc")
		.label("daemon_item.sambaserver_name")
		.hint("daemon_item.sambaserver_desc")
		.defaultValue(0)
		.availableIf(&daemon_samba)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.samba")),
	boolRow("flag_daemon_tuxcald")
		.section("misc")
		.label("daemon_item.tuxcald_name")
		.hint("daemon_item.tuxcald_desc")
		.defaultValue(0)
		.availableIf(&daemon_tuxcald)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.tuxcald")),
	boolRow("flag_daemon_tuxmaild")
		.section("misc")
		.label("daemon_item.tuxmaild_name")
		.hint("daemon_item.tuxmaild_desc")
		.defaultValue(0)
		.availableIf(&daemon_tuxmaild)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.tuxmaild")),
	boolRow("flag_daemon_emmrd")
		.section("misc")
		.label("daemon_item.emmremind_name")
		.hint("daemon_item.emmremind_desc")
		.defaultValue(0)
		.availableIf(&daemon_emmrd)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.emmrd")),
	boolRow("flag_daemon_inadyn")
		.section("misc")
		.label("daemon_item.inadyn_name")
		.hint("daemon_item.inadyn_desc")
		.defaultValue(0)
		.availableIf(&daemon_inadyn)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.inadyn")),
	boolRow("flag_daemon_dropbear")
		.section("misc")
		.label("daemon_item.dropbear_name")
		.hint("daemon_item.dropbear_desc")
		.defaultValue(0)
		.availableIf(&daemon_dropbear)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.dropbear")),
	boolRow("flag_daemon_djmount")
		.section("misc")
		.label("daemon_item.djmount_name")
		.hint("daemon_item.djmount_desc")
		.defaultValue(0)
		.availableIf(&daemon_djmount)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.djmount")),
	boolRow("flag_daemon_ushare")
		.section("misc")
		.label("daemon_item.ushare_name")
		.hint("daemon_item.ushare_desc")
		.defaultValue(0)
		.availableIf(&daemon_ushare)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.ushare")),
	boolRow("flag_daemon_minidlnad")
		.section("misc")
		.label("daemon_item.minidlna_name")
		.hint("daemon_item.minidlna_desc")
		.defaultValue(0)
		.availableIf(&daemon_minidlnad)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.minidlnad")),
	boolRow("flag_daemon_xupnpd")
		.section("misc")
		.label("daemon_item.xupnpd_name")
		.hint("daemon_item.xupnpd_desc")
		.defaultValue(0)
		.availableIf(&daemon_xupnpd)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.xupnpd")),
	boolRow("flag_daemon_crond")
		.section("misc")
		.label("daemon_item.crond_name")
		.hint("daemon_item.crond_desc")
		.defaultValue(0)
		.availableIf(&daemon_crond)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.crond")),
	boolRow("flag_camd_mgcamd")
		.section("cam")
		.label("camd_item_mgcamd_name")
		.hint("camd_item_mgcamd_hint")
		.defaultValue(0)
		.availableIf(&camd_mgcamd)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.mgcamd")),
	boolRow("flag_camd_doscam")
		.section("cam")
		.label("camd_item_doscam_name")
		.hint("camd_item_doscam_hint")
		.defaultValue(0)
		.availableIf(&camd_doscam)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.doscam")),
	boolRow("flag_camd_ncam")
		.section("cam")
		.label("camd_item_ncam_name")
		.hint("camd_item_ncam_hint")
		.defaultValue(0)
		.availableIf(&camd_ncam)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.ncam")),
	boolRow("flag_camd_osmod")
		.section("cam")
		.label("camd_item_osmod_name")
		.hint("camd_item_osmod_hint")
		.defaultValue(0)
		.availableIf(&camd_osmod)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.osmod")),
	boolRow("flag_camd_oscam")
		.section("cam")
		.label("camd_item_oscam_name")
		.hint("camd_item_oscam_hint")
		.defaultValue(0)
		.availableIf(&camd_oscam)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.oscam")),
	boolRow("flag_camd_cccam")
		.section("cam")
		.label("camd_item_cccam_name")
		.hint("camd_item_cccam_hint")
		.defaultValue(0)
		.availableIf(&camd_cccam)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.cccam")),
	boolRow("flag_camd_gbox")
		.section("cam")
		.label("camd_item_gbox_name")
		.hint("camd_item_gbox_hint")
		.defaultValue(0)
		.availableIf(&camd_gbox)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.gbox")),
	boolRow("flag_scart_osd_fix")
		.section("osd")
		.label("scart_osd_fix")
		.hint("menu.hint_scart_osd_fix")
		.defaultValue(0)
		.availableIf(hasScartOsdFix)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.scart_osd_fix")),
#ifdef ENABLE_LCD4LINUX
	boolRow("flag_lcd4l_weather")
		.section("display")
		.label("lcd4l_weather")
		.hint("menu.hint_lcd4l_weather")
		.defaultValue(0)
		.changeableWhen(kLcd4lWeatherOn)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.lcd-weather")),
	boolRow("flag_lcd4l_clock_a")
		.section("display")
		.label("lcd4l_clock_a")
		.hint("menu.hint_lcd4l_clock_a")
		.defaultValue(0)
		.field(COREAPI_FLAG_FILE_FIELD(FLAGDIR "/.lcd-clock_a")),
#endif
};

} // anonymous namespace

const Descriptor *settingsTableLists(size_t &count)
{
	count = sizeof(kLists) / sizeof(kLists[0]);
	return kLists;
}

} // namespace coreapi
