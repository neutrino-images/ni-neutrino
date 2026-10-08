/*
 * allowlist.h - what an AI client may start and may change
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

#ifndef __httpd_mcp_allowlist_h__
#define __httpd_mcp_allowlist_h__

#include <string>
#include <vector>

namespace httpd
{
namespace mcp
{

struct Allowlists
{
	std::vector<std::string> plugins;
	std::vector<std::string> sections;
};

Allowlists currentAllowlists();
void installAllowlists(const Allowlists &a);

// "secret", "network", "parental", "update", or empty for a section that may be allowed.
std::string sectionDenial(const std::string &section);

bool pluginAllowed(const std::string &name);
bool sectionAllowed(const std::string &section); // listed and not denied

/* The first key of a flat JSON object of settings that no AI client may write, empty for
   none. why, when given, is set to the reason for that key, worded to follow "which". */
std::string deniedKeyIn(const std::string &settings_json, std::string *why = NULL);

} // namespace mcp
} // namespace httpd

#endif
