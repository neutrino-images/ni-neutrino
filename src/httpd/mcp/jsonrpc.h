/*
 * jsonrpc.h - JSON-RPC 2.0 messages of the MCP endpoint
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

#ifndef __httpd_mcp_jsonrpc_h__
#define __httpd_mcp_jsonrpc_h__

#include <jsoncpp/json/json.h>

#include <cstddef>
#include <string>

namespace httpd
{
namespace mcp
{

// Kept out of shared headers, where Json would mean httpd::Json.
typedef ::Json::Value JsonValue;

const int kParseError                 = -32700;
const int kInvalidRequest             = -32600;
const int kMethodNotFound             = -32601;
const int kInvalidParams              = -32602;
const int kHeaderMismatch             = -32020;
const int kUnsupportedProtocolVersion = -32022;
// Outside the range JSON-RPC reserves.
const int kInsufficientScope          = -31403;
const int kRateLimited                = -31429;

enum class MessageKind
{
	Request,
	Notification,
	Response
};

struct Message
{
	MessageKind kind;
	JsonValue   id;       // string or integer; null when there is none
	std::string method;
	JsonValue   params;   // object; null when absent

	Message() : kind(MessageKind::Request) {}
};

enum class ReadOutcome
{
	Ok,
	ParseError,
	InvalidRequest
};

// Strict JSON in UTF-8, nested no deeper than max_depth.
bool parseJson(const std::string &text, size_t max_depth, JsonValue &out);

// On InvalidRequest the id is still set when it could be read.
ReadOutcome readMessage(const std::string &body, size_t max_depth, Message &out);

std::string resultResponse(const JsonValue &id, const std::string &result_json);

// A null id is left out; data_json, when not empty, is copied in as it stands.
std::string errorResponse(const JsonValue &id, int code, const std::string &message,
                          const std::string &data_json = std::string());

// Through the server's writer, which mends text that is not UTF-8; false past its depth.
bool toJson(const JsonValue &v, std::string &out);

} // namespace mcp
} // namespace httpd

#endif
