/*
 * test_mcp_contract.cpp - the declarations the MCP parts share
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

#include "httpd/mcp/contract.h"

#include <type_traits>

// Unevaluated, so neither definition has to be linked yet.
static_assert(std::is_same<httpd::mcp::VerifyToken, decltype(&httpd::oauth::verifyAccessToken)>::value,
	      "VerifyToken and verifyAccessToken disagree");

TEST_CASE("the resource and its metadata are named off one base", "[contract]")
{
	REQUIRE(httpd::mcp::resourceOf("https://tv.example.org") == "https://tv.example.org/mcp");
	REQUIRE(httpd::mcp::resourceMetadataOf("https://tv.example.org")
		== "https://tv.example.org/.well-known/oauth-protected-resource/mcp");
	REQUIRE(httpd::mcp::resourceOf("").empty());
	REQUIRE(httpd::mcp::resourceMetadataOf("").empty());
}

TEST_CASE("a caller and a tool start at the least they may do", "[contract]")
{
	const httpd::mcp::Caller c;
	REQUIRE(c.level == httpd::AuthLevel::Public);
	REQUIRE(c.external);
	REQUIRE(c.groups == 0u);

	const httpd::mcp::ToolDef t;
	REQUIRE(t.level == httpd::AuthLevel::System);
	REQUIRE_FALSE(t.read_only);
	REQUIRE(t.destructive);
	REQUIRE_FALSE(t.idempotent);
	REQUIRE(t.group == 0u);
	REQUIRE_FALSE(t.image);
}
