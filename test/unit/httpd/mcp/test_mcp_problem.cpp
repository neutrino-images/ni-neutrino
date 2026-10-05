/*
 * test_mcp_problem.cpp - a refusal the router wrote, read back as the error
 *
 * Copyright (C) 2026 NI-Team
 *
 * License: GPL-2.0-or-later, see COPYING.
 */

#include "support/catch.hpp"

#include "httpd/http.h"
#include "httpd/status.h"
#include "httpd/mcp/problem.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"

#include <string>

using namespace httpd;

TEST_CASE("every code survives the trip through a problem document", "[mcp-problem]")
{
	for (int i = 0; i <= (int) mcp::kLastErrorCode; ++i)
	{
		const coreapi::ErrorCode c = (coreapi::ErrorCode) i;
		INFO(coreapi::codeString(c));
		const Response r = problemResponse(StatusConflict, c, "said \"so\" \xc3\xbc");
		const coreapi::Error e = mcp::errorFromProblem(r);
		REQUIRE(e.code == c);
		REQUIRE(e.status == coreapi::Status::Conflict);
		REQUIRE(e.message == "said \"so\" \xc3\xbc");
	}
}

TEST_CASE("the walk over codes stops at the last one the header declares", "[mcp-problem]")
{
	const coreapi::ErrorCode past = (coreapi::ErrorCode) ((int) mcp::kLastErrorCode + 1);
	REQUIRE(std::string(coreapi::codeString(past)).empty());
	REQUIRE(std::string(coreapi::codeString(mcp::kLastErrorCode)).size() > 0);
}

TEST_CASE("the status is the one the answer's code projects back to", "[mcp-problem]")
{
	REQUIRE(mcp::errorFromProblem(problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchTimer, "x")).status
		== coreapi::Status::NotFound);
	REQUIRE(mcp::errorFromProblem(problemResponse(StatusBadRequest, coreapi::ErrorCode::BadInt, "x")).status
		== coreapi::Status::InvalidArgument);
	REQUIRE(mcp::errorFromProblem(problemResponse(StatusForbidden, coreapi::ErrorCode::NotPermitted, "x")).status
		== coreapi::Status::Denied);
}

TEST_CASE("an answer that is no problem document reads as a table fault", "[mcp-problem]")
{
	Response r;
	r.code = StatusInternalServerError;
	r.body = "not json";
	const coreapi::Error e = mcp::errorFromProblem(r);
	REQUIRE(e.code == coreapi::ErrorCode::BadTable);
	REQUIRE(e.status == coreapi::Status::Internal);

	r.body = "{\"type\":\"/errors/no-code-is-called-this\",\"detail\":\"x\"}";
	REQUIRE(mcp::errorFromProblem(r).code == coreapi::ErrorCode::BadTable);
}

TEST_CASE("a wire spelling no code has is not read as one", "[mcp-problem]")
{
	coreapi::ErrorCode c = coreapi::ErrorCode::NoSuchChannel;
	REQUIRE_FALSE(mcp::codeFromWire("", c));
	REQUIRE_FALSE(mcp::codeFromWire("no-such-thing-at-all", c));
	REQUIRE(mcp::codeFromWire("box-in-standby", c));
	REQUIRE(c == coreapi::ErrorCode::BoxInStandby);
}
