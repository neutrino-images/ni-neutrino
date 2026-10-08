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
#include "httpd/mcp/jsonrpc.h"
#include "httpd/mcp/limits.h"

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

	// Not a flat object: depends_on is a list.
	std::string type;
	std::string detail;
	std::vector<std::string> depends_on;
	JsonValue doc;
	if (parseJson(r.body, limits().max_json_depth, doc) && doc.isObject())
	{
		if (doc["type"].isString())
			type = doc["type"].asString();
		if (doc["detail"].isString())
			detail = doc["detail"].asString();
		const JsonValue &keys = doc["depends_on"];
		for (JsonValue::const_iterator it = keys.begin(); keys.isArray() && it != keys.end(); ++it)
		{
			if ((*it).isString())
				depends_on.push_back((*it).asString());
		}
	}

	const coreapi::Status status = statusForCode(r.code);
	coreapi::ErrorCode code = coreapi::ErrorCode::BadTable;
	if (type.compare(0, prefix_len, kPrefix) != 0 || !codeFromWire(type.substr(prefix_len), code))
		return coreapi::Error(status, coreapi::ErrorCode::BadTable,
		                      "the route refused without saying why");
	coreapi::Error e(status, code, detail);
	e.depends_on = depends_on;
	return e;
}

} // namespace mcp
} // namespace httpd
