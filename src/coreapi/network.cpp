/*
 * network.cpp - what the box knows of its network interfaces
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

#include "coreapi/network.h"
#include "coreapi/settings/settings.h"

#include <libnet/libnet.h>

#include <arpa/inet.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <algorithm>

namespace coreapi
{
namespace network
{

namespace
{

/* The loopback, which the system lists beside the real ones, and the entries a
   directory listing carries for itself. The loopback is told by its first two
   letters, which is how the box's own screens always told it. */
bool isInterface(const char *name)
{
	if (name[0] == '.')
		return false;
	if (name[0] == 'l' && name[1] == 'o')
		return false;
	return true;
}

} // namespace

std::vector<std::string> interfacesIn(const std::string &directory)
{
	std::vector<std::string> names;
	struct dirent **list = NULL;
	const int count = scandir(directory.c_str(), &list, NULL, alphasort);
	if (count < 0)
		return names;

	for (int i = 0; i < count; ++i)
	{
		if (isInterface(list[i]->d_name))
			names.push_back(list[i]->d_name);
		free(list[i]);
	}
	free(list);
	return names;
}

std::vector<std::string> interfaces()
{
	return interfacesIn("/sys/class/net");
}

namespace
{

std::string systemAddress(const std::string &name)
{
	std::string dev = name;
	std::string ip;
	std::string mask;
	std::string broadcast;
	netGetIP(dev, ip, mask, broadcast);
	struct in_addr addr;
	if (ip == "0.0.0.0" || inet_pton(AF_INET, ip.c_str(), &addr) != 1)
		return "";
	return ip;
}

std::string statusIn(const std::string &directory, const std::string &name, AddressOf address)
{
	const std::string ip = address(name);
	if (!ip.empty())
		return ip;
	std::string words;
	if (!settings::resolveLabel(stateKeyIn(directory, name), words))
		return "";
	return words;
}

} // namespace

const char *stateKeyIn(const std::string &directory, const std::string &name)
{
	const std::string entry = directory + "/" + name;
	if (access((entry + "/wireless").c_str(), F_OK) == 0)
		return "networkmenu.state_not_connected";

	int carrier = 0;
	FILE *f = fopen((entry + "/carrier").c_str(), "r");
	if (f != NULL)
	{
		if (fscanf(f, "%d", &carrier) != 1)
			carrier = 0;
		fclose(f);
	}
	return carrier ? "networkmenu.state_no_address" : "networkmenu.state_no_cable";
}

std::string interfaceStatus(const std::string &name)
{
	return statusIn("/sys/class/net", name, systemAddress);
}

bool interfaceChoicesIn(const std::string &directory, AddressOf address, std::vector<SettingChoice> &out)
{
	const std::vector<std::string> names = interfacesIn(directory);
	if (names.empty())
		return false;

	out.clear();
	for (size_t i = 0; i < names.size(); ++i)
	{
		SettingChoice one;
		one.value = (long) i;
		one.text = names[i];
		const std::string status = statusIn(directory, names[i], address);
		one.label = status.empty() ? names[i] : names[i] + " (" + status + ")";
		out.push_back(one);
	}
	return true;
}

bool interfaceChoices(std::vector<SettingChoice> &out)
{
	return interfaceChoicesIn("/sys/class/net", systemAddress, out);
}

} // namespace network
} // namespace coreapi
