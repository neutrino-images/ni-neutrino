/*
 * apppaths.cpp - where the web files live, and which of them may be served
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

#include "apppaths.h"

namespace httpd
{

namespace
{

/* The areas the page has, and nothing else. The names are the first segment of every
   address the page owns. They are English while the page itself speaks German, because
   an address is an identifier and not a word anybody reads out: the API under /api/v1
   already spells most of these the same way.

   The last of them is here in every build, including one compiled without the API
   documentation, where the page does not offer it. A destination the page can be walked
   to but not reloaded on is a bookmark that works until somebody presses reload, and
   whether that destination is worth offering is a decision the page makes out of what
   the box answers about its own build. */
const char *const kAreas[] = {
	"now", "channels", "epg", "timers", "files", "system", "settings", "dev"
};

const std::size_t kAreaCount = sizeof(kAreas) / sizeof(kAreas[0]);

} // namespace

bool answersWithPage(const std::string &path)
{
	if (path.empty() || path[0] != '/')
		return false;

	/* A target ending in the separator is what the directory fallback in
	   static.cpp already answers, and two rules for one target would disagree
	   about which of them is on it. */
	if (path[path.size() - 1] == '/')
		return false;

	const std::size_t second = path.find('/', 1);
	const std::string head = path.substr(1, (second == std::string::npos)
	                                        ? std::string::npos : second - 1);
	bool named = false;
	for (std::size_t i = 0; i < kAreaCount; ++i)
		named = named || (head == kAreas[i]);
	if (!named)
		return false;

	/* A dot in the last segment is what tells a mistyped file from an
	   identifier. A channel is addressed by its hexadecimal id and a section by
	   its name, and neither carries one; a file does. This is the whole of what
	   keeps a typo in a module name a 404. */
	const std::size_t last = path.rfind('/');
	return path.find('.', last + 1) == std::string::npos;
}

const char *const *appAreas(std::size_t *count)
{
	if (count != NULL)
		*count = kAreaCount;
	return kAreas;
}

} // namespace httpd
