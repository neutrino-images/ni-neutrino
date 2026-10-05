/*
 * toolgroups.h - the fixed table of tool groups
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

#ifndef __httpd_mcp_toolgroups_h__
#define __httpd_mcp_toolgroups_h__

#include "httpd/endpoint.h"
#include "httpd/mcp/contract.h"

#include <cstddef>
#include <string>
#include <vector>

namespace httpd
{
namespace mcp
{

const unsigned GroupProgramme  = 1u;
const unsigned GroupTimers     = 2u;
const unsigned GroupRecordings = 4u;
const unsigned GroupControl    = 8u;
const unsigned GroupBouquets   = 16u;
const unsigned GroupStatus     = 32u;
const unsigned GroupSettings   = 64u;
const unsigned GroupPlugins    = 128u;
const unsigned kAllGroups      = 255u;
const unsigned kDefaultGroups  = GroupProgramme | GroupTimers | GroupRecordings;

struct ToolGroup
{
	const char        *key;
	unsigned           bit;
	AuthLevel          least;
	const char *const *tools;
	size_t             tool_count;
};

const ToolGroup *toolGroups(size_t *count);
const ToolGroup *groupByKey(const std::string &key);
// 0 for a name no group carries.
unsigned groupOfTool(const std::string &name);
// False, *bits untouched, when a key is not in the table; empty text is no group.
bool readGroupKeys(const std::string &space_separated, unsigned *bits);
// In table order.
std::vector<std::string> groupKeys(unsigned bits);
// Empty when every tool is in exactly one group, else the first offender.
std::string toolOutsideGroups(const std::vector<ToolDef> &tools);
// Empty when every name in the table is among tools, else the first missing name.
std::string groupNameWithoutTool(const std::vector<ToolDef> &tools);
// Empty when each group's least is the lowest level among its tools that exist.
std::string groupLeastIsRight(const std::vector<ToolDef> &tools);
// Among tools, how many of this group a connection at this reach would actually be offered.
size_t toolsOfferedIn(const ToolGroup &g, const std::vector<ToolDef> &tools, AuthLevel reach);

} // namespace mcp
} // namespace httpd

#endif
