/*
 * problem.cpp - a refusal, read back as the error it was made from
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

#include "httpd/mcp/problem.h"

#include "httpd/json.h"
#include "httpd/status.h"

#include <string>
#include <vector>

namespace httpd
{
namespace mcp
{

bool codeFromWire(const std::string &wire, coreapi::ErrorCode &out)
{
	if (wire.empty())
		return false;
	for (int i = 0; i <= (int) kLastErrorCode; ++i)
	{
		const coreapi::ErrorCode c = (coreapi::ErrorCode) i;
		if (wire == coreapi::codeString(c))
		{
			out = c;
			return true;
		}
	}
	return false;
}

coreapi::Error errorFromProblem(const Response &r)
{
	static const char kPrefix[] = "/errors/";
	const size_t prefix_len = sizeof(kPrefix) - 1;

	std::string type;
	std::string detail;
	std::vector<JsonMember> members;
	if (readFlatObject(r.body, members))
	{
		for (size_t i = 0; i < members.size(); ++i)
		{
			if (members[i].kind != JsonValueKind::String)
				continue;
			if (members[i].name == "type")
				type = members[i].text;
			else if (members[i].name == "detail")
				detail = members[i].text;
		}
	}

	const coreapi::Status status = statusForCode(r.code);
	coreapi::ErrorCode code = coreapi::ErrorCode::BadTable;
	if (type.compare(0, prefix_len, kPrefix) != 0 || !codeFromWire(type.substr(prefix_len), code))
		return coreapi::Error(status, coreapi::ErrorCode::BadTable,
		                      "the route refused without saying why");
	return coreapi::Error(status, code, detail);
}

} // namespace mcp
} // namespace httpd
