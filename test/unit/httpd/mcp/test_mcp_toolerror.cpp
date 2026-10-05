/*
 * test_mcp_toolerror.cpp - tests for the text of a refused tool call
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

#include "httpd/mcp/toolerror.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"

#include <string>

TEST_CASE("the text of a failed call names the code and the words and the hint", "[mcp]")
{
	const coreapi::Error e(coreapi::Status::Conflict, coreapi::ErrorCode::BoxInStandby, "the box is in standby");
	REQUIRE(httpd::mcp::errorText(e, "Call again with wake set to true.") ==
	        "Error box-in-standby: the box is in standby\nHint: Call again with wake set to true.");
}

TEST_CASE("a refusal without a hint has no hint line", "[mcp]")
{
	const coreapi::Error e(coreapi::Status::Conflict, coreapi::ErrorCode::BoxInStandby, "the box is in standby");
	REQUIRE(httpd::mcp::errorText(e, "") == "Error box-in-standby: the box is in standby");
}

TEST_CASE("a refusal without words names its code alone", "[mcp]")
{
	const coreapi::Error e(coreapi::Status::NotFound, coreapi::ErrorCode::NoSuchChannel, "");
	REQUIRE(httpd::mcp::errorText(e, "List the channels.") == "Error no-such-channel\nHint: List the channels.");
	REQUIRE(httpd::mcp::errorText(e, "") == "Error no-such-channel");
}
