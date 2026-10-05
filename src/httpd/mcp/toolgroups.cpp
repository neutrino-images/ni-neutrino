/*
 * toolgroups.cpp - the fixed table of tool groups
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

#include "httpd/mcp/toolgroups.h"

namespace httpd
{
namespace mcp
{

namespace
{

const char *const kProgramme[] = { "whats_on", "find_programme", "channel_schedule", "programme_details",
	"list_channels", "current_channel", "epg_grid", "channel_logo", "now_playing" };
const char *const kTimers[] = { "list_timers", "set_timer", "change_timer", "remove_timer", "record_programme" };
const char *const kRecordings[] = { "list_recordings", "stop_recording", "list_archive", "recording_details", "play_recording",
	"delete_recording", "timeshift_start", "timeshift_stop" };
const char *const kControl[] = { "switch_channel", "get_volume", "set_volume", "set_mute", "set_mode",
	"show_message", "screenshot", "standby_state", "set_standby" };
const char *const kBouquets[] = { "list_bouquets", "create_bouquet", "delete_bouquet", "set_bouquet_channels",
	"rename_bouquet", "move_bouquet", "hide_bouquet", "lock_bouquet" };
const char *const kStatus[] = { "box_info", "storage_space", "signal_quality", "list_tuners", "list_plugins",
	"settings_schema", "read_settings" };
const char *const kSettings[] = { "write_settings" };
const char *const kPlugins[] = { "start_plugin" };

#define GROUP_TOOLS(a) (a), (sizeof(a) / sizeof((a)[0]))

const ToolGroup kGroups[] = {
	{ "programme",  GroupProgramme,  AuthLevel::Read,   GROUP_TOOLS(kProgramme) },
	{ "timers",     GroupTimers,     AuthLevel::Read,   GROUP_TOOLS(kTimers) },
	{ "recordings", GroupRecordings, AuthLevel::Read,   GROUP_TOOLS(kRecordings) },
	{ "control",    GroupControl,    AuthLevel::Read,   GROUP_TOOLS(kControl) },
	{ "bouquets",   GroupBouquets,   AuthLevel::Read,   GROUP_TOOLS(kBouquets) },
	{ "status",     GroupStatus,     AuthLevel::Read,   GROUP_TOOLS(kStatus) },
	{ "settings",   GroupSettings,   AuthLevel::System, GROUP_TOOLS(kSettings) },
	{ "plugins",    GroupPlugins,    AuthLevel::System, GROUP_TOOLS(kPlugins) },
};

const size_t kGroupCount = sizeof(kGroups) / sizeof(kGroups[0]);

bool listed(const std::vector<ToolDef> &tools, const char *name, const ToolDef **found)
{
	for (size_t i = 0; i < tools.size(); ++i)
	{
		if (tools[i].name == name)
		{
			if (found != NULL)
				*found = &tools[i];
			return true;
		}
	}
	return false;
}

} // namespace

const ToolGroup *toolGroups(size_t *count)
{
	if (count != NULL)
		*count = kGroupCount;
	return kGroups;
}

const ToolGroup *groupByKey(const std::string &key)
{
	for (size_t i = 0; i < kGroupCount; ++i)
	{
		if (key == kGroups[i].key)
			return &kGroups[i];
	}
	return NULL;
}

unsigned groupOfTool(const std::string &name)
{
	for (size_t i = 0; i < kGroupCount; ++i)
	{
		for (size_t k = 0; k < kGroups[i].tool_count; ++k)
		{
			if (name == kGroups[i].tools[k])
				return kGroups[i].bit;
		}
	}
	return 0;
}

bool readGroupKeys(const std::string &space_separated, unsigned *bits)
{
	unsigned out = 0;
	size_t at = 0;
	while (at < space_separated.size())
	{
		if (space_separated[at] == ' ')
		{
			++at;
			continue;
		}
		size_t end = space_separated.find(' ', at);
		if (end == std::string::npos)
			end = space_separated.size();
		const ToolGroup *g = groupByKey(space_separated.substr(at, end - at));
		if (g == NULL)
			return false;
		out |= g->bit;
		at = end;
	}
	*bits = out;
	return true;
}

std::vector<std::string> groupKeys(unsigned bits)
{
	std::vector<std::string> out;
	for (size_t i = 0; i < kGroupCount; ++i)
	{
		if ((bits & kGroups[i].bit) != 0)
			out.push_back(kGroups[i].key);
	}
	return out;
}

std::string toolOutsideGroups(const std::vector<ToolDef> &tools)
{
	for (size_t i = 0; i < tools.size(); ++i)
	{
		size_t in = 0;
		for (size_t g = 0; g < kGroupCount; ++g)
		{
			for (size_t k = 0; k < kGroups[g].tool_count; ++k)
				in += (tools[i].name == kGroups[g].tools[k]) ? 1 : 0;
		}
		if (in != 1)
			return tools[i].name;
	}
	return std::string();
}

std::string groupNameWithoutTool(const std::vector<ToolDef> &tools)
{
	for (size_t g = 0; g < kGroupCount; ++g)
	{
		for (size_t k = 0; k < kGroups[g].tool_count; ++k)
		{
			if (!listed(tools, kGroups[g].tools[k], NULL))
				return kGroups[g].tools[k];
		}
	}
	return std::string();
}

std::string groupLeastIsRight(const std::vector<ToolDef> &tools)
{
	for (size_t g = 0; g < kGroupCount; ++g)
	{
		bool any = false;
		AuthLevel least = AuthLevel::System;
		for (size_t k = 0; k < kGroups[g].tool_count; ++k)
		{
			const ToolDef *d = NULL;
			if (!listed(tools, kGroups[g].tools[k], &d))
				continue;
			if (!any || (int) d->level < (int) least)
				least = d->level;
			any = true;
		}
		if (any && least != kGroups[g].least)
			return kGroups[g].key;
	}
	return std::string();
}

size_t toolsOfferedIn(const ToolGroup &g, const std::vector<ToolDef> &tools, AuthLevel reach)
{
	size_t n = 0;
	for (size_t i = 0; i < tools.size(); ++i)
	{
		if (tools[i].group == g.bit && (int) tools[i].level <= (int) reach)
			++n;
	}
	return n;
}

} // namespace mcp
} // namespace httpd
