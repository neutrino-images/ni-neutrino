/*
 * contract.h - what the MCP endpoint, the OAuth server and the tools agree on
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

#ifndef __httpd_mcp_contract_h__
#define __httpd_mcp_contract_h__

#include "httpd/endpoint.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"

#include <string>
#include <vector>

namespace httpd
{
namespace mcp
{

// One serialized JSON value.
typedef std::string JsonText;
// One serialized JSON Schema object; empty means none.
typedef std::string JsonSchema;

struct Caller
{
	std::string client_id;
	std::string user;
	AuthLevel   level;
	bool        external;
	// Bits of the group table (mcp/toolgroups.h); a tool is offered only in one of them.
	unsigned    groups;

	Caller() : level(AuthLevel::Public), external(true), groups(0) {}
};

struct ToolDef
{
	std::string name;
	std::string title;
	std::string description;
	JsonSchema  input;
	JsonSchema  output;
	AuthLevel   level;
	bool        read_only;
	bool        destructive;
	bool        idempotent;
	// The one group bit this tool is offered under, 0 for none.
	unsigned    group;
	// Answers { "mime_type", "data" } that the endpoint sends as image content.
	bool        image;

	ToolDef() : level(AuthLevel::System), read_only(false), destructive(true), idempotent(false),
	            group(0), image(false) {}
};

class ToolSource
{
	public:
		virtual ~ToolSource() {}

		// Every tool, whatever the caller may reach.
		virtual std::vector<ToolDef> list() = 0;

		// Thread-safe; may still run after the request was answered.
		virtual coreapi::Result<JsonText> call(const Caller &c, const std::string &name,
		                                       const JsonText &args) = 0;

		// Empty when there is nothing useful to say.
		virtual std::string hint(const std::string &name, coreapi::ErrorCode code) const = 0;
};

typedef coreapi::Result<Caller> (*VerifyToken)(const std::string &bearer, Origin origin,
                                               const std::string &resource);

inline std::string resourceOf(const std::string &base)
{
	return base.empty() ? std::string() : base + "/mcp";
}

inline std::string resourceMetadataOf(const std::string &base)
{
	return base.empty() ? std::string() : base + "/.well-known/oauth-protected-resource/mcp";
}

ToolSource &boxTools();
const std::string &boxToolsRefusal();

} // namespace mcp

namespace oauth
{

coreapi::Result<mcp::Caller> verifyAccessToken(const std::string &bearer, Origin origin,
                                               const std::string &resource);

} // namespace oauth
} // namespace httpd

#endif
