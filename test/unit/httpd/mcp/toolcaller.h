/*
 * toolcaller.h - who calls a tool in the cases, and what a composed tool answers
 *
 * Copyright (C) 2026 NI-Team
 *
 * License: GPL-2.0-or-later, see COPYING.
 */

#ifndef __test_mcp_toolcaller_h__
#define __test_mcp_toolcaller_h__

#include "support/catch.hpp"
#include "support/shape.h"

#include "httpd/endpoint.h"
#include "httpd/mcp/allowlist.h"
#include "httpd/mcp/composed.h"
#include "httpd/mcp/contract.h"
#include "httpd/mcp/routetools.h"

#include "coreapi/base/errors.h"

#include "jsoncpp/json/json.h"

#include <cstring>
#include <string>

// What the box offers, whatever the allowlists hold.
const size_t kOfferedTools = 48;

inline httpd::mcp::Caller callerAt(httpd::AuthLevel l)
{
	httpd::mcp::Caller c;
	c.client_id = "test";
	c.user = "root";
	c.level = l;
	c.external = false;
	return c;
}

struct ToolClock
{
	explicit ToolClock(time_t t) { httpd::mcp::setToolClockForTest(t); }
	~ToolClock() { httpd::mcp::setToolClockForTest(0); }
};

inline const httpd::Schema &composedSchemaOf(const char *path)
{
	const httpd::RouteTable &t = httpd::mcp::composedTable();
	for (size_t i = 0; i < t.count; ++i)
		if (std::strcmp(t.endpoints[i].path, path) == 0)
			return *t.endpoints[i].schema;
	FAIL("no composed route " << path);
	return *t.endpoints[0].schema;
}

// Held to the shape the route declares.
inline ::Json::Value composedAnswer(httpd::AuthLevel l, const char *tool, const std::string &args,
                                    const char *path)
{
	httpd::mcp::RouteTools tools(NULL, 0, httpd::mcp::composedTable());
	INFO(tools.refusal());
	REQUIRE(tools.refusal().empty());
	const coreapi::Result<std::string> got = tools.call(callerAt(l), tool, args);
	INFO((got.ok() ? got.value() : std::string(coreapi::codeString(got.error().code)) + ": " + got.error().message));
	REQUIRE(got.ok());
	::Json::Value v;
	::Json::Reader r;
	REQUIRE(r.parse(got.value(), v));
	checkShape(v, composedSchemaOf(path), tool);
	return v;
}

inline coreapi::Error composedRefusal(httpd::AuthLevel l, const char *tool, const std::string &args)
{
	httpd::mcp::RouteTools tools(NULL, 0, httpd::mcp::composedTable());
	const coreapi::Result<std::string> got = tools.call(callerAt(l), tool, args);
	REQUIRE_FALSE(got.ok());
	return got.error();
}

#endif
