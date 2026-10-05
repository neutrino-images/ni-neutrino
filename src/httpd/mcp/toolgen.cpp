/*
 * toolgen.cpp - describing a flagged route as a tool
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

#include "httpd/mcp/toolgen.h"

#include "httpd/router.h"
#include "httpd/doc/openapi.h"
#include "httpd/mcp/toolhint.h"
#include "httpd/mcp/toolschema.h"

#include "coreapi/base/errors.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace httpd
{
namespace mcp
{

namespace
{

const size_t kMaxNameBytes = 64;

const char *verbFor(Method m)
{
	switch (m)
	{
		case Get:           return "get";
		case Post:          return "do";
		case Put:           return "set";
		case Patch:         return "change";
		case Delete:        return "delete";
		case Head:          return "call";
		case Options:       return "call";
		case UnknownMethod: return "call";
	}
	return "call";
}

// A row of every kind the router checks, so it states every code its argument checks give.
const Param kEveryCheck[] = {
	HTTPD_QUERY_REQUIRED_IN("n", ParamType::Int, "a number", 0, 1),
	HTTPD_QUERY("b", ParamType::Bool, "a flag"),
	HTTPD_QUERY_FROM_SET("e", "a word", "a", NULL),
};
const Endpoint kEveryCheckRoute = {
	Get, "/", AuthLevel::Read, "", NULL, HTTPD_PARAMS(kEveryCheck), NULL, NULL, false, Answers200,
	HTTPD_NO_REFUSALS
};

bool isArgumentCheck(coreapi::ErrorCode code)
{
	std::vector<std::pair<coreapi::ErrorCode, std::string> > checked;
	parameterRefusals(kEveryCheckRoute, checked);
	for (size_t i = 0; i < checked.size(); ++i)
	{
		if (checked[i].first == code)
			return true;
	}
	return false;
}

void appendSentence(std::string &out, const std::string &text)
{
	out += text;
	if (!text.empty() && text[text.size() - 1] != '.')
		out += '.';
}

} // namespace

const Endpoint *flaggedRoute(const RouteTable &t, const ToolFlag &f)
{
	if (f.path == NULL || t.endpoints == NULL)
		return NULL;
	for (size_t i = 0; i < t.count; ++i)
	{
		const Endpoint &ep = t.endpoints[i];
		if (ep.method == f.method && ep.path != NULL && std::strcmp(ep.path, f.path) == 0)
			return &ep;
	}
	return NULL;
}

std::string derivedToolName(const Endpoint &ep)
{
	static const char kRoot[] = "/api/v1/";
	std::string out = verbFor(ep.method);
	const char *p = ep.path;
	if (std::strncmp(p, kRoot, sizeof(kRoot) - 1) == 0)
		p += sizeof(kRoot) - 1;

	bool owed = true;
	for (; *p != '\0'; ++p)
	{
		const unsigned char ch = (unsigned char) *p;
		if (ch == '{')
		{
			out += "_by";
			owed = true;
			continue;
		}
		const bool upper = ch >= 'A' && ch <= 'Z';
		const bool kept = upper || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9');
		if (!kept)
		{
			owed = true;
			continue;
		}
		if (owed)
		{
			out += '_';
			owed = false;
		}
		out += (char) (upper ? ch - 'A' + 'a' : ch);
	}
	if (out.size() > kMaxNameBytes)
		out.resize(kMaxNameBytes);
	return out;
}

std::string toolName(const Endpoint &ep, const ToolFlag &f)
{
	return f.name != NULL ? std::string(f.name) : derivedToolName(ep);
}

std::string toolDescription(const Endpoint &ep, const ToolFlag &f)
{
	if (f.description != NULL)
		return f.description;
	if (ep.description != NULL && ep.description[0] != '\0')
		return ep.description;
	return ep.summary != NULL ? std::string(ep.summary) : std::string();
}

std::string describedTool(const Endpoint &ep, const ToolFlag &f)
{
	std::string out = toolDescription(ep, f);

	std::vector<openapi::StatedRefusal> stated;
	openapi::statedRefusals(ep, stated);

	std::string own;
	for (size_t i = 0; i < stated.size(); ++i)
	{
		const openapi::StatedRefusal &s = stated[i];
		// Argument checks are in the server's instructions; no other rule reaches a call in process.
		if (s.rule != NULL || isArgumentCheck(s.code))
			continue;
		own += "\n- ";
		own += coreapi::codeString(s.code);
		own += ": ";
		appendSentence(own, s.detail);
		const char *hint = retryHint(s.code, ep);
		if (hint != NULL)
		{
			own += ' ';
			own += hint;
		}
	}

	if (!own.empty())
		out += "\n\nRefusals:" + own;
	return out;
}

std::string toolTitle(const std::string &name)
{
	std::string out = name;
	for (size_t i = 0; i < out.size(); ++i)
		if (out[i] == '_')
			out[i] = ' ';
	if (!out.empty() && out[0] >= 'a' && out[0] <= 'z')
		out[0] = (char) (out[0] - 'a' + 'A');
	return out;
}

void hintsFor(Method m, AuthLevel level, bool &read_only, bool &destructive, bool &idempotent)
{
	read_only = (m == Get);
	destructive = (m == Delete) || (m != Get && level == AuthLevel::System);
	idempotent = (m == Get || m == Put || m == Delete);
}

ToolDef toolFor(const Endpoint &ep, const ToolFlag &f)
{
	ToolDef d;
	d.name = toolName(ep, f);
	d.title = toolTitle(d.name);
	d.description = describedTool(ep, f);
	appendInputSchema(d.input, ep);
	if (ep.schema != NULL)
		appendOutputSchema(d.output, *ep.schema);
	else
		appendDoneSchema(d.output, ep.answers);
	d.image = f.image;
	if (f.image)
		d.output.clear();
	d.level = ep.auth;
	hintsFor(ep.method, ep.auth, d.read_only, d.destructive, d.idempotent);
	return d;
}

} // namespace mcp
} // namespace httpd
