/*
 * test_toolflag.cpp - the tool flags a route table carries
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

#include "support/catch.hpp"

#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/router.h"

#include <cstring>
#include <string>

using namespace httpd;

namespace
{

Response nothing(const Request &)
{
	return noContent();
}

const Endpoint kTwo[] = {
	{ Method::Get, "/api/v1/flag/one", AuthLevel::Read, "the first", NULL, NULL, 0, NULL, &nothing, false, Answers200, HTTPD_NO_REFUSALS },
	{ Method::Delete, "/api/v1/flag/two", AuthLevel::Write, "the second", NULL, NULL, 0, NULL, &nothing, false, Answers204, HTTPD_NO_REFUSALS },
};

const ToolFlag kTools[] = {
	HTTPD_TOOL(Method::Get, "/api/v1/flag/one"),
	HTTPD_TOOL_AS(Method::Delete, "/api/v1/flag/two", "remove_two", "removes the second"),
};

const RouteTable kPlain = { HTTPD_TABLE("flag", kTwo) };
const RouteTable kCounted = { HTTPD_TABLE_N("flag", kTwo, 1) };
const RouteTable kFlagged = { HTTPD_TABLE_WITH_TOOLS("flag", kTwo, kTools) };

} // namespace

TEST_CASE("a table written without tools offers none", "[toolflag]")
{
	REQUIRE(kPlain.count == 2);
	REQUIRE(kPlain.tools == NULL);
	REQUIRE(kPlain.tool_count == 0);
	REQUIRE(kCounted.count == 1);
	REQUIRE(kCounted.tools == NULL);
	REQUIRE(kCounted.tool_count == 0);
}

TEST_CASE("a table written with tools carries them beside its routes", "[toolflag]")
{
	REQUIRE(kFlagged.count == 2);
	REQUIRE(kFlagged.endpoints == kTwo);
	REQUIRE(kFlagged.tool_count == 2);
	REQUIRE(kFlagged.tools == kTools);
	REQUIRE(kFlagged.tools[0].name == NULL);
	REQUIRE(kFlagged.tools[0].description == NULL);
	REQUIRE(kFlagged.tools[1].method == Delete);
	REQUIRE(std::strcmp(kFlagged.tools[1].path, "/api/v1/flag/two") == 0);
	REQUIRE(std::strcmp(kFlagged.tools[1].name, "remove_two") == 0);
	REQUIRE(std::strcmp(kFlagged.tools[1].description, "removes the second") == 0);
	std::string why;
	const bool sane = tableIsSane(kFlagged, &why);
	INFO(why);
	REQUIRE(sane);
}

TEST_CASE("a flag says whether it answers a picture and which query it adds", "[toolflag]")
{
	const httpd::ToolFlag plain = HTTPD_TOOL(httpd::Method::Get, "/a");
	const httpd::ToolFlag named = HTTPD_TOOL_AS(httpd::Method::Get, "/b", "bee", "Bees.");
	const httpd::ToolFlag image = HTTPD_TOOL_IMAGE(httpd::Method::Get, "/c", "sea", "A picture.");
	const httpd::ToolFlag paged = HTTPD_TOOL_DEFAULTS(httpd::Method::Get, "/d", "dee", "Pages.", "limit=5");
	REQUIRE_FALSE(plain.image);
	REQUIRE(plain.defaults == NULL);
	REQUIRE_FALSE(named.image);
	REQUIRE(named.defaults == NULL);
	REQUIRE(image.image);
	REQUIRE(image.defaults == NULL);
	REQUIRE_FALSE(paged.image);
	REQUIRE(std::string(paged.defaults) == "limit=5");
}
