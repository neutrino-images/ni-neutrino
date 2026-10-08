/*
 * settingsindex.cpp - settings_schema without a section: the sections for one connection
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

#include "httpd/mcp/settingsindex.h"

#include "httpd/json.h"
#include "httpd/mcp/allowlist.h"
#include "httpd/mcp/jsonrpc.h"
#include "httpd/mcp/limits.h"

#include "coreapi/base/errors.h"
#include "coreapi/settings/settings.h"

#include <cstring>
#include <vector>

namespace httpd
{
namespace mcp
{

namespace
{

// The box menu's own heading of each section, so the label reads in the box's language.
struct SectionLabel
{
	const char *section;
	const char *label_key;
};

const SectionLabel kSectionLabels[] = {
	{ "audio", "mainsettings.audio" },
	{ "cam", "ci.settings" },
	{ "channel", "miscsettings.channellist" },
	{ "display", "mainsettings.lcd" },
	{ "general", "mainsettings.language" },
	{ "hdd", "hdd_settings" },
	{ "keybindings", "mainsettings.keybinding" },
	{ "misc", "mainsettings.misc" },
	{ "network", "mainsettings.network" },
	{ "osd", "mainsettings.osd" },
	{ "parental", "parentallock.parentallock" },
	{ "player", "mainsettings.multimedia" },
	{ "recording", "mainsettings.recording" },
	{ "update", "flashupdate.head" },
	{ "video", "mainsettings.video" },
	{ "weather", "miscsettings.infobar_weather" },
};

const char *labelKeyOf(const std::string &section)
{
	for (size_t i = 0; i < sizeof(kSectionLabels) / sizeof(kSectionLabels[0]); ++i)
	{
		if (section == kSectionLabels[i].section)
			return kSectionLabels[i].label_key;
	}
	return NULL;
}

const char kIndexShape[] =
	"{\"type\":\"array\",\"description\":\"answered without section and keys, in place of items\","
	"\"items\":{\"type\":\"object\",\"properties\":{"
	"\"name\":{\"type\":\"string\",\"description\":\"the section, as section takes it\"},"
	"\"label\":{\"type\":\"string\",\"description\":\"what the box menu calls it, where the catalog has it\"},"
	"\"count\":{\"type\":\"integer\",\"minimum\":0,\"description\":\"how many settings it holds\"},"
	"\"readable\":{\"type\":\"boolean\",\"description\":\"whether this connection may read its values\"},"
	"\"writable\":{\"type\":\"boolean\",\"description\":\"whether this connection may change it\"}},"
	"\"required\":[\"name\",\"count\",\"readable\",\"writable\"],\"additionalProperties\":false}}";

} // namespace

bool isSettingsSchemaRoute(const Endpoint &ep)
{
	return ep.method == Get && ep.path != NULL && std::strcmp(ep.path, "/api/v1/settings/schema") == 0;
}

bool asksSettingsIndex(const JsonText &args)
{
	JsonValue v;
	if (!parseJson(args, limits().max_json_depth, v) || !v.isObject())
		return false;
	/* Anything else goes to the route, whose checks name a misspelled or mistyped argument. */
	const std::vector<std::string> names = v.getMemberNames();
	for (size_t i = 0; i < names.size(); ++i)
	{
		const JsonValue &one = v[names[i]];
		// An empty text names nothing, so it asks for no more than null does.
		const bool nothing = one.isNull() || (one.isString() && one.asString().empty());
		if ((names[i] != "section" && names[i] != "keys") || !nothing)
			return false;
	}
	return true;
}

std::string withSettingsIndex(const std::string &output)
{
	JsonValue v;
	JsonValue index;
	if (!parseJson(output, limits().max_json_depth, v) || !v.isObject() ||
	    !parseJson(kIndexShape, limits().max_json_depth, index))
		return output;
	v["properties"]["sections"] = index;
	JsonValue required(::Json::arrayValue);
	const JsonValue &was = v["required"];
	for (JsonValue::ArrayIndex i = 0; was.isArray() && i < was.size(); ++i)
	{
		if (was[i].asString() != "items")
			required.append(was[i]);
	}
	if (required.empty())
		v.removeMember("required");
	else
		v["required"] = required;
	std::string out;
	return toJson(v, out) ? out : output;
}

coreapi::Result<JsonText> settingsIndex(bool can_read, bool can_write)
{
	coreapi::Result<std::vector<std::string> > names = coreapi::settings::sections();
	if (!names.ok())
		return coreapi::fail(names.error());
	coreapi::Result<std::vector<coreapi::Descriptor> > rows = coreapi::settings::schema();
	if (!rows.ok())
		return coreapi::fail(rows.error());

	std::string out;
	Json j(out, 64 + 96 * names.value().size());
	j.beginObject();
	j.key("sections");
	j.beginArray();
	for (size_t i = 0; i < names.value().size(); ++i)
	{
		const std::string &name = names.value()[i];
		long count = 0;
		for (size_t r = 0; r < rows.value().size(); ++r)
			count += (rows.value()[r].section != NULL && name == rows.value()[r].section) ? 1 : 0;
		j.beginObject();
		j.key("name");
		j.value(name);
		std::string label;
		if (coreapi::settings::resolveLabel(labelKeyOf(name), label))
		{
			j.key("label");
			j.value(label);
		}
		j.key("count");
		j.value(count);
		j.key("readable");
		j.value(can_read);
		j.key("writable");
		j.value(can_write && sectionAllowed(name));
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return coreapi::ok(out);
}

} // namespace mcp
} // namespace httpd
