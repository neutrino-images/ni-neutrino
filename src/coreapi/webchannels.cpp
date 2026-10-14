/*
 * webchannels.cpp - the web channel lists the box loads from its own folders
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

#include "coreapi/webchannels.h"

#include <system/settings.h>

#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <list>
#include <set>

extern SNeutrinoSettings g_settings;

namespace coreapi
{
namespace webchannels
{

namespace
{

int isListFile(const struct dirent *entry)
{
	const char *n = entry->d_name;
	const size_t len = strlen(n);
	if (len <= 3)
		return 0;
	if (n[len - 3] == 'x' && n[len - 2] == 'm' && n[len - 1] == 'l')
		return 1;
	if (n[len - 3] == 'm' && n[len - 2] == '3' && n[len - 1] == 'u')
		return 1;
	if (n[len - 2] == 't' && n[len - 1] == 'v')
		return 1;
	return 0;
}

// The var and the shipped folder may be one through a link.
std::string dirKey(const std::string &dir)
{
	char resolved[PATH_MAX];
	if (realpath(dir.c_str(), resolved))
		return resolved;

	std::string normalized = dir;
	while (normalized.length() > 1 && normalized[normalized.length() - 1] == '/')
		normalized.erase(normalized.length() - 1);
	return normalized;
}

std::string baseName(std::string path)
{
	while (path.length() > 1 && path[path.length() - 1] == '/')
		path.erase(path.length() - 1);
	const std::string::size_type pos = path.find_last_of('/');
	return pos == std::string::npos ? path : path.substr(pos + 1);
}

bool hasContent(const std::string &path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0 && st.st_size > 0;
}

bool asChoices(const std::vector<std::string> &files, std::vector<SettingChoice> &out)
{
	if (files.empty())
		return false;
	out.clear();
	for (size_t i = 0; i < files.size(); ++i)
	{
		SettingChoice one;
		one.value = (long) i;
		one.text = files[i];
		one.label = files[i];
		out.push_back(one);
	}
	return true;
}

} // namespace

std::vector<std::string> autoloadDirs(bool radio)
{
	std::vector<std::string> dirs;
	dirs.push_back(radio ? WEBRADIODIR_VAR : WEBTVDIR_VAR);
	dirs.push_back(radio ? WEBRADIODIR : WEBTVDIR);
	return dirs;
}

std::vector<std::string> autoloadIn(const std::vector<std::string> &dirs,
				    const std::vector<std::string> &listed)
{
	std::vector<std::string> found;
	std::set<std::string> names;
	for (size_t i = 0; i < listed.size(); ++i)
		names.insert(baseName(listed[i]));

	std::set<std::string> scanned;
	for (size_t d = 0; d < dirs.size(); ++d)
	{
		if (!scanned.insert(dirKey(dirs[d])).second)
			continue;

		struct dirent **list = NULL;
		const int count = scandir(dirs[d].c_str(), &list, isListFile, alphasort);
		if (count < 0)
			continue;
		for (int i = 0; i < count; ++i)
		{
			const std::string name = list[i]->d_name;
			const std::string path = dirs[d] + "/" + name;
			if (hasContent(path) && names.insert(name).second)
				found.push_back(path);
			free(list[i]);
		}
		free(list);
	}
	return found;
}

std::vector<std::string> autoloaded(bool radio)
{
	if (!(radio ? g_settings.webradio_xml_auto : g_settings.webtv_xml_auto))
		return std::vector<std::string>();
	const std::list<std::string> listed = settingsCopy(radio ? g_settings.webradio_xml : g_settings.webtv_xml);
	return autoloadIn(autoloadDirs(radio), std::vector<std::string>(listed.begin(), listed.end()));
}

bool webtvAutoloaded(std::vector<SettingChoice> &out)
{
	return asChoices(autoloaded(false), out);
}

bool webradioAutoloaded(std::vector<SettingChoice> &out)
{
	return asChoices(autoloaded(true), out);
}

} // namespace webchannels
} // namespace coreapi
