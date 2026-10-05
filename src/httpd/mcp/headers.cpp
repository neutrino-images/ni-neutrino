/*
 * headers.cpp - header rules of the MCP endpoint
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

#include "httpd/mcp/headers.h"

#include "httpd/credentials.h"
#include "httpd/json.h"

#include <cctype>
#include <string>
#include <vector>

namespace httpd
{
namespace mcp
{

namespace
{

const char kSentinelHead[] = "=?base64?";
const char kSentinelTail[] = "?=";

std::string lower(const std::string &s)
{
	std::string out(s);
	for (size_t i = 0; i < out.size(); ++i)
		out[i] = (char) std::tolower((unsigned char) out[i]);
	return out;
}

// RFC 9110 field value bytes.
bool plainByte(unsigned char c)
{
	return c == '\t' || (c >= 0x20 && c <= 0x7e);
}

void appendParam(std::string &out, const char *name, const std::string &value)
{
	if (!out.empty())
		out += ", ";
	out += name;
	out += "=\"";
	for (size_t i = 0; i < value.size(); ++i)
	{
		const unsigned char c = (unsigned char) value[i];
		// A control byte would end the header line.
		if (c < 0x20 || c == 0x7f)
			continue;
		if (c == '"' || c == '\\')
			out += '\\';
		out += (char) c;
	}
	out += '"';
}

} // namespace

bool decodeMirrored(const std::string &header, std::string &out)
{
	out.clear();
	const size_t head = sizeof(kSentinelHead) - 1;
	const size_t tail = sizeof(kSentinelTail) - 1;
	if (header.size() >= head + tail && header.compare(0, head, kSentinelHead) == 0 &&
	    header.compare(header.size() - tail, tail, kSentinelTail) == 0)
	{
		std::vector<unsigned char> bytes;
		if (!httpd::decodeBase64Strict(header.substr(head, header.size() - head - tail), bytes))
			return false;
		const std::string text(bytes.begin(), bytes.end());
		if (!httpd::isUtf8(text.data(), text.size()))
			return false;
		out = text;
		return true;
	}

	for (size_t i = 0; i < header.size(); ++i)
	{
		if (!plainByte((unsigned char) header[i]))
			return false;
	}
	out = header;
	return true;
}

bool isJsonMediaType(const std::string &content_type)
{
	const std::string type = content_type.substr(0, content_type.find(';'));
	size_t begin = 0;
	size_t end = type.size();
	while (begin < end && (type[begin] == ' ' || type[begin] == '\t'))
		++begin;
	while (end > begin && (type[end - 1] == ' ' || type[end - 1] == '\t'))
		--end;
	return lower(type.substr(begin, end - begin)) == "application/json";
}

std::string originOf(const std::string &url)
{
	const std::string l = lower(url);
	std::string scheme;
	if (l.compare(0, 7, "http://") == 0)
		scheme = "http";
	else if (l.compare(0, 8, "https://") == 0)
		scheme = "https";
	else
		return std::string();

	const size_t start = scheme.size() + 3;
	const size_t end = l.find_first_of("/?#", start);
	std::string authority = l.substr(start, (end == std::string::npos) ? std::string::npos : end - start);
	if (authority.empty() || authority.find('@') != std::string::npos)
		return std::string();

	const std::string default_port = (scheme == "http") ? ":80" : ":443";
	if (authority.size() > default_port.size() &&
	    authority.compare(authority.size() - default_port.size(), default_port.size(), default_port) == 0)
		authority.erase(authority.size() - default_port.size());
	return scheme + "://" + authority;
}

std::string bearerChallenge(const std::string &metadata_url, const char *error,
                            const std::string &scope)
{
	std::string params;
	if (!metadata_url.empty())
		appendParam(params, "resource_metadata", metadata_url);
	if (error != NULL)
		appendParam(params, "error", error);
	if (!scope.empty())
		appendParam(params, "scope", scope);
	return params.empty() ? std::string("Bearer") : "Bearer " + params;
}

const char *scopeFor(AuthLevel level)
{
	switch (level)
	{
		case AuthLevel::Public:
		case AuthLevel::Read:
			return "read";
		case AuthLevel::Write:
			return "write";
		case AuthLevel::System:
			return "system";
	}
	return "system";
}

} // namespace mcp
} // namespace httpd
