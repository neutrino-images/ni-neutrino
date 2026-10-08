/*
 * flagfile.h - a flag that is the existence of a file
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

#ifndef __coreapi_flagfile_h__
#define __coreapi_flagfile_h__

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <unistd.h>

namespace coreapi
{

/* The two things a row of origin FlagFile means, written once for the layer that
   holds the store and for the menu that edits the flag itself. A flag is on
   while its file is there, and nothing is kept in the file. */
inline bool flagFileIsSet(const char *path)
{
	return access(path, F_OK) == 0;
}

/* Whether a program is there to be started: an executable file on the path, as the
   screens that offer a program look for one. This layer cannot reach the system
   helpers they use, and the rule is the same. */
inline bool executableOnPath(const char *name)
{
	const char *env = getenv("PATH");
	std::string path = env != NULL ? env : "/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/usr/local/sbin";
	if (name[0] == '/')
		return access(name, X_OK) == 0;
	size_t from = 0;
	while (from <= path.size())
	{
		size_t to = path.find(':', from);
		if (to == std::string::npos)
			to = path.size();
		if (to > from && access((path.substr(from, to - from) + "/" + name).c_str(), X_OK) == 0)
			return true;
		from = to + 1;
	}
	return false;
}

// Whether a file is there, which a path of a program that is installed in one place is asked with.
inline bool fileIsThere(const char *path)
{
	return access(path, F_OK) == 0;
}

// Made empty where it is to be on, and removed where it is not.
inline bool setFlagFile(const char *path, bool on)
{
	if (on)
	{
		FILE *fd = std::fopen(path, "w");
		if (fd == NULL)
			return false;
		std::fclose(fd);
		return true;
	}
	// A file that is not there is already what was asked for.
	return unlink(path) == 0 || errno == ENOENT;
}

} // namespace coreapi

#endif
