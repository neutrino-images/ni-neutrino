/*
 * toolguard.cpp - what may never become a tool, and what a tool must say
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

#include "httpd/mcp/toolguard.h"

#include "httpd/router.h"
#include "httpd/schema.h"
#include "httpd/mcp/toolgen.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace httpd
{
namespace mcp
{

namespace
{

// Write-level rows are here because the System rule does not reach them.
const NeverExposed kNever[] = {
	{ Post,   "/api/v1/system/reboot" },
	{ Post,   "/api/v1/system/shutdown" },
	{ Post,   "/api/v1/system/restart" },
	{ Post,   "/api/v1/daemons/{name}/start" },
	{ Post,   "/api/v1/daemons/{name}/stop" },
	{ Post,   "/api/v1/daemons/{name}/restart" },
	{ Post,   "/api/v1/system/reload-setup" },
	{ Post,   "/api/v1/tuner/reset" },
	{ Post,   "/api/v1/osd/remote/key" },
	{ Post,   "/api/v1/settings/secret/clear" },
	{ Put,    "/api/v1/system/webserver" },
	{ Put,    "/api/v1/storage/netfs/{table}/{slot}" },
	{ Delete, "/api/v1/storage/netfs/{table}/{slot}" },
	{ Post,   "/api/v1/scripts/{name}" },
	{ Post,   "/api/v1/login" },
	{ Post,   "/api/v1/logout" },
	{ Post,   "/api/v1/token/media" },
};

// Offered only through the owner's allowlists.
const NeverExposed kGated[] = {
	{ Patch, "/api/v1/settings/{section}" },
	{ Post,  "/api/v1/plugins/{name}/start" },
};

struct Area
{
	const char *path;
	bool        writes_only;
};

const Area kNeverAreas[] = {
	{ "/api/v1/timers", true },
	{ "/api/v1/ai",     false },
};

bool under(const char *path, const char *area)
{
	const size_t n = std::strlen(area);
	return std::strncmp(path, area, n) == 0 && (path[n] == '\0' || path[n] == '/');
}

const char kStandbyPath[] = "/api/v1/system/standby";
const char kComposedPrefix[] = "/mcp/tools/";
const size_t kMaxDescriptionBytes = 1024;
const size_t kMaxDescribedBytes = 4096;
const size_t kMaxToolNameBytes = 64;

bool say(std::string *why, const std::string &what)
{
	if (why != NULL)
		*why = what;
	return false;
}

bool isToolName(const std::string &n)
{
	if (n.empty() || n.size() > kMaxToolNameBytes || n[0] < 'a' || n[0] > 'z')
		return false;
	for (size_t i = 0; i < n.size(); ++i)
	{
		const char c = n[i];
		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
			return false;
	}
	return true;
}

bool takenMethod(Method m)
{
	return m == Get || m == Post || m == Put || m == Patch || m == Delete;
}

bool flagIsSane(const RouteTable &t, const ToolFlag &f, std::string *why)
{
	const std::string at = std::string(t.tag != NULL ? t.tag : "?") + ": " +
		methodName(f.method) + " " + (f.path != NULL ? f.path : "(no path)");

	const Endpoint *ep = flaggedRoute(t, f);
	if (ep == NULL)
		return say(why, at + " names a route its table does not carry");
	if (neverExposed(f.method, f.path))
		return say(why, at + " is never offered as a tool");
	if (ep->auth == AuthLevel::Public)
		return say(why, at + " is reachable without a credential");
	if (ep->auth == AuthLevel::System && ep->method != Get &&
	    !(ep->method == Post && std::strcmp(ep->path, kStandbyPath) == 0) &&
	    !gated(ep->method, ep->path))
		return say(why, at + " changes the box at the system level and is neither standby nor gated");
	if (ep->query_token_ok)
		return say(why, at + " takes a credential out of its address");
	if (!takenMethod(ep->method))
		return say(why, at + " has a method no tool is called with");

	size_t whole = 0;
	for (size_t i = 0; i < ep->param_count; ++i)
	{
		const Param &p = ep->params[i];
		if (p.in == In::BodyBytes)
			return say(why, at + " carries a body no argument can name");
		if (namesWholeBody(p.in) && ++whole > 1)
			return say(why, at + " carries more than one whole body");
		if (p.name == NULL || p.name[0] == '\0' || p.doc == NULL || p.doc[0] == '\0')
			return say(why, at + " has an argument that says nothing about itself");
		for (size_t k = 0; k < i; ++k)
		{
			if (std::strcmp(ep->params[k].name, p.name) == 0)
				return say(why, at + " has two arguments of one name");
		}
	}

	if (f.defaults != NULL)
	{
		const std::string all = f.defaults;
		size_t at_pair = 0;
		while (at_pair < all.size())
		{
			size_t end = all.find('&', at_pair);
			if (end == std::string::npos)
				end = all.size();
			const std::string name = all.substr(at_pair, all.find('=', at_pair) - at_pair);
			bool known = false;
			for (size_t i = 0; i < ep->param_count && !known; ++i)
				known = ep->params[i].in == In::Query && name == ep->params[i].name;
			if (!known)
				return say(why, at + " adds " + name + ", which is not one of its query arguments");
			at_pair = end + 1;
		}
	}

	if (f.image)
	{
		if (ep->method != Get || ep->auth != AuthLevel::Read || (ep->answers & Answers200) == 0)
			return say(why, at + " answers a picture and is not a read");
	}
	else
	{
		if (ep->method == Get && ep->schema == NULL)
			return say(why, at + " reads and declares no answer");
		if ((ep->answers & Answers206) != 0)
			return say(why, at + " answers with part of a file, which no tool can carry");
		// A gated route may answer 207: RouteTools::call() turns it into a tool error.
		if ((ep->answers & Answers207) != 0 && !gated(ep->method, ep->path))
			return say(why, at + " answers with several outcomes, which no tool can carry");
		if ((ep->answers & Answers200) != 0 && ep->schema == NULL)
			return say(why, at + " answers a document it does not describe");
	}
	if (ep->schema != NULL)
	{
		const char *wrong = NULL;
		if (!schemaIsSane(*ep->schema, &wrong))
			return say(why, at + " declares an answer that is wrong: " + wrong);
	}
	if (ep->schema != NULL && (ep->answers & (Answers202 | Answers204)) != 0)
		return say(why, at + " answers with no document where it describes one");

	if (!isToolName(toolName(*ep, f)))
		return say(why, at + " is not a tool name: " + toolName(*ep, f));
	const std::string words = toolDescription(*ep, f);
	if (words.empty())
		return say(why, at + " says nothing about what it does");
	if (words.size() > kMaxDescriptionBytes)
		return say(why, at + " describes itself in too long a text");
	if (describedTool(*ep, f).size() > kMaxDescribedBytes)
		return say(why, at + " states its refusals in too long a text");
	return true;
}

bool tableFlagsAreSane(const RouteTable &t, std::vector<std::string> &names,
                       std::vector<const Endpoint *> &routes, std::string *why)
{
	if (t.tool_count > 0 && t.tools == NULL)
		return say(why, std::string(t.tag != NULL ? t.tag : "?") + " counts tools it does not carry");
	for (size_t i = 0; i < t.tool_count; ++i)
	{
		if (!flagIsSane(t, t.tools[i], why))
			return false;
		const Endpoint *ep = flaggedRoute(t, t.tools[i]);
		const std::string name = toolName(*ep, t.tools[i]);
		for (size_t k = 0; k < names.size(); ++k)
		{
			if (names[k] == name)
				return say(why, "the tool " + name + " is named twice");
		}
		for (size_t k = 0; k < routes.size(); ++k)
		{
			if (routes[k] == ep)
				return say(why, std::string(ep->path) + " is flagged twice");
		}
		names.push_back(name);
		routes.push_back(ep);
	}
	return true;
}

bool composedIsSane(const RouteTable &c, std::string *why)
{
	if (c.count > kMaxComposedTools)
	{
		char most[32];
		std::snprintf(most, sizeof(most), "%lu", (unsigned long) kMaxComposedTools);
		return say(why, std::string("more composed tools than the ceiling of ") + most);
	}
	if (c.tool_count != c.count)
		return say(why, "a composed route that is not offered as a tool");
	for (size_t i = 0; i < c.count; ++i)
	{
		const Endpoint &ep = c.endpoints[i];
		if (std::strncmp(ep.path, kComposedPrefix, sizeof(kComposedPrefix) - 1) != 0)
			return say(why, std::string(ep.path) + " is a composed tool outside /mcp/tools/");
		if (ep.schema == NULL)
			return say(why, std::string(ep.path) + " is a composed tool that declares no answer");
	}
	std::string table_why;
	if (c.count > 0 && !tableIsSane(c, &table_why))
		return say(why, "the composed table: " + table_why);
	return true;
}

} // namespace

const NeverExposed *neverExposedRoutes(size_t *count)
{
	if (count != NULL)
		*count = sizeof(kNever) / sizeof(kNever[0]);
	return kNever;
}

bool neverExposed(Method m, const char *path)
{
	if (path == NULL)
		return false;
	for (size_t i = 0; i < sizeof(kNever) / sizeof(kNever[0]); ++i)
	{
		if (kNever[i].method == m && std::strcmp(kNever[i].path, path) == 0)
			return true;
	}
	for (size_t i = 0; i < sizeof(kNeverAreas) / sizeof(kNeverAreas[0]); ++i)
	{
		if (under(path, kNeverAreas[i].path) && (!kNeverAreas[i].writes_only || m != Get))
			return true;
	}
	return false;
}

const NeverExposed *gatedRoutes(size_t *count)
{
	if (count != NULL)
		*count = sizeof(kGated) / sizeof(kGated[0]);
	return kGated;
}

bool gated(Method m, const char *path)
{
	if (path == NULL)
		return false;
	for (size_t i = 0; i < sizeof(kGated) / sizeof(kGated[0]); ++i)
	{
		if (kGated[i].method == m && std::strcmp(kGated[i].path, path) == 0)
			return true;
	}
	return false;
}

bool toolsAreSane(const RouteTable *const *tables, size_t table_count,
                  const RouteTable &composed, std::string *why)
{
	if (why != NULL)
		why->clear();
	if (table_count > 0 && tables == NULL)
		return say(why, "the list names tables it does not carry");
	if (!composedIsSane(composed, why))
		return false;

	std::vector<std::string> names;
	std::vector<const Endpoint *> routes;
	for (size_t t = 0; t < table_count; ++t)
	{
		if (tables[t] == NULL)
			return say(why, "the list names a table that is not there");
		if (!tableFlagsAreSane(*tables[t], names, routes, why))
			return false;
	}
	return tableFlagsAreSane(composed, names, routes, why);
}

} // namespace mcp
} // namespace httpd
