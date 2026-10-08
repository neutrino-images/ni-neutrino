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

#include <dirent.h>
#include <stdlib.h>

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

bool interfaceChoices(std::vector<SettingChoice> &out)
{
	const std::vector<std::string> names = interfaces();
	if (names.empty())
		return false;

	out.clear();
	for (size_t i = 0; i < names.size(); ++i)
	{
		SettingChoice one;
		one.value = (long) i;
		one.text = names[i];
		one.label = names[i];
		out.push_back(one);
	}
	return true;
}

} // namespace network
} // namespace coreapi
