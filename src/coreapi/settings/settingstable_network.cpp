/*
 * settingstable_network.cpp - network settings, one row per field
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

namespace coreapi
{

namespace
{

/* The network section. The descriptor carries no sub-section, so the split
   into interface and time server on one side and the three proxy fields on the
   other is not in the table.

   Most of the network settings are not here. The address, the netmask, the
   broadcast, the gateway, the name server, the host name, the wireless name and
   its key, the DHCP switch and the flag that brings the network up at start are
   all members of CNetworkConfig, which keeps a file of its own.

   Two more groups declare nothing at all. The web server values live in an
   object of their own and are handed to the layer that owns the server's own
   configuration file. The mount entries are held in arrays of structures in
   src/system/settings.h, which no scalar descriptor reaches. */

/* Two values with words of their own rather than the program's on and off, so
   an Enum keeps the names and holds both the numbers and the words. A Bool is
   compared on the numbers alone, and which of the two means one would then rest
   on nothing. */
const EnumValue kNtpEnable[] =
{
	{ NETWORK_NTP_OFF, "options.ntp_off", NULL, NULL },
	{ NETWORK_NTP_ON, "options.ntp_on", NULL, NULL }
};

const Descriptor kNetwork[] =
{
	/* The name of an interface, and what is on offer is what the box has: the
	   list is read from /sys/class/net. A name nothing there matches is dropped
	   and replaced at the next load. */
	{
		"ifname", ValueType::String, "network",
		"networkmenu.select_if", "menu.hint_net_if",
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(ifname)
	},

	// time
	{
		"network_ntpenable", ValueType::Enum, "network",
		"networkmenu.ntpenable", "menu.hint_net_ntpenable",
		0, 0, COREAPI_ENUM(kNtpEnable), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(network_ntpenable)
	},
	{
		"network_ntpserver", ValueType::String, "network",
		"networkmenu.ntpserver", "menu.hint_net_ntpserver",
		0, 0, NULL, 0, 0, "0.de.pool.ntp.org", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(network_ntpserver)
	},
	/* Minutes, kept as text and turned into a number where it is used. Text
	   here because the field is, and a String carries no bound, so the three
	   digits and the ten characters the field takes are stated nowhere a caller
	   can read. */
	{
		"network_ntprefresh", ValueType::String, "network",
		"networkmenu.ntprefresh", "menu.hint_net_ntprefresh",
		0, 0, NULL, 0, 0, "30", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(network_ntprefresh)
	},

	/* The proxy. The three below are one setting in three parts: the loader
	   joins them into one URL, and so does every reader.

	   The name and the password are the credential in that URL and are both
	   declared secret: a name that identifies an account against a password is
	   the half of a credential that names the account, the box shows neither,
	   and the password is typed behind stars. The server is not secret. Which proxy
	   a box goes through is not a credential, and hiding it would leave a
	   frontend unable to show whether one is set at all. */
	{
		"softupdate_proxyserver", ValueType::String, "network",
		"flashupdate.proxyserver", "menu.hint_net_proxyserver",
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(softupdate_proxyserver)
	},
	{
		"softupdate_proxyusername", ValueType::String, "network",
		"flashupdate.proxyusername", "menu.hint_net_proxyuser",
		0, 0, NULL, 0, 0, "", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(softupdate_proxyusername)
	},
	{
		"softupdate_proxypassword", ValueType::String, "network",
		"flashupdate.proxypassword", "menu.hint_net_proxypass",
		0, 0, NULL, 0, 0, "", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(softupdate_proxypassword)
	},
};

} // anonymous namespace

const Descriptor *settingsTableNetwork(size_t &count)
{
	count = sizeof(kNetwork) / sizeof(kNetwork[0]);
	return kNetwork;
}

} // namespace coreapi
