/*
 * endpoint.h - the MCP endpoint at /mcp
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

#ifndef __httpd_mcp_endpoint_h__
#define __httpd_mcp_endpoint_h__

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/mcp/callrunner.h"
#include "httpd/mcp/contract.h"
#include "httpd/mcp/jsonrpc.h"

#include <cstddef>
#include <string>

namespace httpd
{
namespace mcp
{

// The head as text; a count says how often a header arrived, and twice is refused.
struct Head
{
	httpd::Method method;
	Origin        origin;   // the gate's verdict
	std::string   base;     // exposure::baseUrl of this request; empty when none is configured
	std::string   content_type;
	std::string   origin_header;
	unsigned      origin_header_count;
	std::string   authorization;
	unsigned      authorization_count;
	std::string   protocol_version;
	unsigned      protocol_version_count;
	std::string   mcp_method;
	unsigned      mcp_method_count;
	std::string   mcp_name;
	unsigned      mcp_name_count;

	Head()
		: method(httpd::UnknownMethod), origin(Origin::Refused), origin_header_count(0),
		  authorization_count(0), protocol_version_count(0), mcp_method_count(0), mcp_name_count(0)
	{
	}
};

struct Admission
{
	bool        admitted;
	Response    refusal;
	Caller      caller;
	std::string resource;       // empty on the LAN
	std::string metadata_url;   // empty on the LAN

	Admission() : admitted(false) {}
};

// Exactly /mcp, and only once the endpoint is wired.
bool handles(const std::string &path);

size_t maxBodyBytes();

// Everything decidable before the body; a refusal comes back ready to send.
Admission admit(const Head &h);

// The answer to an admitted request once its body is whole.
Response answer(const Head &h, const Admission &a, const std::string &body);

// What the answer to a tools/call left running still needs.
struct Deferred
{
	JsonValue   id;
	bool        modern;
	std::string tool;
	unsigned    timeout_ms;
	ToolSource *tools;
	bool        image;

	Deferred() : modern(true), timeout_ms(0), tools(NULL), image(false) {}
};

/* As answer(), except that a tools/call whose tool was started is not waited for: true, and
   its answer reaches finished(cls, ...) on another thread, for respond() to turn into the response.
   A NULL finished starts no tool and answers the call Busy. */
bool answerLater(const Head &h, const Admission &a, const std::string &body, CallFinished finished,
                 void *cls, Response &now, Deferred &later);

Response respond(const Deferred &later, const CallAnswer &ans);

// The bytes tools/list writes for this tool.
size_t toolDefinitionBytes(const ToolDef &d);

} // namespace mcp
} // namespace httpd

#endif
