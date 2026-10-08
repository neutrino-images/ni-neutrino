/*
 * endpoint.cpp - the MCP endpoint at /mcp
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

#include <config.h>

#include "httpd/mcp/endpoint.h"

#include "httpd/auth.h"
#include "httpd/json.h"
#include "httpd/status.h"
#include "httpd/mcp/applynotes.h"
#include "httpd/mcp/callrunner.h"
#include "httpd/mcp/headers.h"
#include "httpd/mcp/jsonrpc.h"
#include "httpd/mcp/limits.h"
#include "httpd/mcp/ratelimit.h"
#include "httpd/mcp/toolerror.h"
#include "httpd/mcp/toolgroups.h"
#include "httpd/mcp/wiring.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace httpd
{
namespace mcp
{

namespace
{

Response jsonAnswer(int http_code, const std::string &body)
{
	Response r;
	r.code = http_code;
	r.content_type = "application/json";
	r.body = body;
	addApiHeaders(r);
	return r;
}

Response rpcError(int http_code, const JsonValue &id, int code, const std::string &message,
                  const std::string &data_json = std::string())
{
	return jsonAnswer(http_code, errorResponse(id, code, message, data_json));
}

// Refused before a message was read, so there is no id to answer.
Response transportError(int http_code, int code, const std::string &message)
{
	return rpcError(http_code, JsonValue(), code, message);
}

// Word for word the router's answer for a path it does not have.
Response noSuchPath()
{
	Response r = problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchRoute,
	                             "this server has no such path");
	addApiHeaders(r);
	return r;
}

Response problem(int http_code, coreapi::ErrorCode code, const char *detail)
{
	Response r = problemResponse(http_code, code, detail);
	addApiHeaders(r);
	return r;
}

Response challenge(int http_code, const std::string &metadata_url, const char *error,
                   const std::string &scope)
{
	Response r = problem(http_code, coreapi::ErrorCode::NotPermitted,
	                     "a valid access token for this endpoint is required");
	r.headers.push_back(std::make_pair(std::string("WWW-Authenticate"),
	                                   bearerChallenge(metadata_url, error, scope)));
	return r;
}

Response rateRefusal(unsigned retry_after)
{
	char data[48];
	std::snprintf(data, sizeof(data), "{\"retryAfterSeconds\":%u}", retry_after);
	Response r = rpcError(StatusTooManyRequests, JsonValue(), kRateLimited, "Too many requests", data);
	char after[16];
	std::snprintf(after, sizeof(after), "%u", retry_after);
	r.headers.push_back(std::make_pair(std::string("Retry-After"), std::string(after)));
	return r;
}

Admission refused(const Response &r)
{
	Admission a;
	a.refusal = r;
	return a;
}

const char kModern[] = "2026-07-28";
// Newest first; an initialize naming none of these is answered with the first.
const char *const kLegacy[] = { "2025-11-25", "2025-06-18" };
const size_t kLegacyCount = sizeof(kLegacy) / sizeof(kLegacy[0]);

const char kMetaVersion[] = "io.modelcontextprotocol/protocolVersion";
const char kMetaCapabilities[] = "io.modelcontextprotocol/clientCapabilities";
const char kMetaServerInfo[] = "io.modelcontextprotocol/serverInfo";

// Shared by every tool, so stated here and in no tool description.
const char kInstructions[] =
	"Every tool checks its arguments against its input schema before it acts. An argument that does "
	"not fit is refused with one of missing-parameter, no-such-parameter, duplicate-parameter, "
	"bad-string, bad-int, bad-bool, bad-enum, out-of-range, value-too-long or value-has-zero-byte, "
	"and the message names the argument: correct it and call again. A tool's description lists "
	"only the refusals beyond these.";

/* How long a client may keep the list: it changes with the firmware, and with the level and
   tool groups of the connection, which the owner can change while the box runs. */
const unsigned kListTtlMs = 300000;

enum class Era
{
	Modern,
	Legacy
};

bool isLegacyVersion(const std::string &v)
{
	for (size_t i = 0; i < kLegacyCount; ++i)
	{
		if (v == kLegacy[i])
			return true;
	}
	return false;
}

bool isSupportedVersion(const std::string &v)
{
	return v == kModern || isLegacyVersion(v);
}

const JsonValue &member(const JsonValue &object, const char *name)
{
	if (!object.isObject())
		return JsonValue::nullSingleton();
	return object[name];
}

Response accepted()
{
	Response r;
	r.code = StatusAccepted;
	addApiHeaders(r);
	return r;
}

void writeServerInfo(httpd::Json &j)
{
	j.beginObject();
	j.key("name");
	j.value(PACKAGE_NAME);
	j.key("version");
	j.value(PACKAGE_VERSION);
	j.endObject();
}

void writeCapabilities(httpd::Json &j)
{
	j.beginObject();
	j.key("tools");
	j.beginObject();
	j.endObject();
	j.endObject();
}

void writeVersions(httpd::Json &j)
{
	j.beginArray();
	j.value(kModern);
	for (size_t i = 0; i < kLegacyCount; ++i)
		j.value(kLegacy[i]);
	j.endArray();
}

void writeModernTail(httpd::Json &j)
{
	j.key("resultType");
	j.value("complete");
	j.key("_meta");
	j.beginObject();
	j.key(kMetaServerInfo);
	writeServerInfo(j);
	j.endObject();
}

Response unsupportedVersion(const JsonValue &id, const std::string &requested)
{
	std::string data;
	httpd::Json j(data);
	j.beginObject();
	j.key("supported");
	writeVersions(j);
	j.key("requested");
	j.value(requested);
	j.endObject();
	return rpcError(StatusBadRequest, id, kUnsupportedProtocolVersion, "Unsupported protocol version", data);
}

Response initialize(const Message &m)
{
	const JsonValue &requested = member(m.params, "protocolVersion");
	if (!requested.isString())
		return rpcError(StatusOk, m.id, kInvalidParams, "params.protocolVersion must be a string");
	const std::string chosen = isLegacyVersion(requested.asString()) ? requested.asString()
	                                                                 : std::string(kLegacy[0]);

	std::string result;
	httpd::Json j(result);
	j.beginObject();
	j.key("protocolVersion");
	j.value(chosen);
	j.key("capabilities");
	writeCapabilities(j);
	j.key("serverInfo");
	writeServerInfo(j);
	j.key("instructions");
	j.value(kInstructions);
	j.endObject();
	return jsonAnswer(StatusOk, resultResponse(m.id, result));
}

Response discover(const Message &m)
{
	std::string result;
	httpd::Json j(result);
	j.beginObject();
	j.key("supportedVersions");
	writeVersions(j);
	j.key("capabilities");
	writeCapabilities(j);
	j.key("instructions");
	j.value(kInstructions);
	j.key("ttlMs");
	j.value(kListTtlMs);
	j.key("cacheScope");
	j.value("public");
	writeModernTail(j);
	j.endObject();
	return jsonAnswer(StatusOk, resultResponse(m.id, result));
}

bool validToolName(const std::string &name)
{
	if (name.empty() || name.size() > 128)
		return false;
	for (size_t i = 0; i < name.size(); ++i)
	{
		const char c = name[i];
		const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		                     (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
		if (!allowed)
			return false;
	}
	return true;
}

// Copied into the answer as it stands, so it has to be a whole object.
bool isObjectSchema(const std::string &text, bool needs_object_type)
{
	JsonValue v;
	if (text.find('\0') != std::string::npos || !parseJson(text, limits().max_json_depth, v) || !v.isObject())
		return false;
	if (!needs_object_type)
		return true;
	const JsonValue &type = member(v, "type");
	return type.isString() && type.asString() == "object";
}

bool byName(const ToolDef &x, const ToolDef &y)
{
	return x.name < y.name;
}

// In name order and once per name; a malformed definition is logged and left out.
std::vector<ToolDef> usableTools(ToolSource *tools)
{
	const std::vector<ToolDef> offered = tools->list();
	std::vector<ToolDef> sound;
	for (size_t i = 0; i < offered.size(); ++i)
	{
		const ToolDef &d = offered[i];
		const char *why = NULL;
		if (!validToolName(d.name))
			why = "its name is not one clients accept";
		else if (!isObjectSchema(d.input, true))
			why = "its input schema is not an object schema";
		else if (!d.output.empty() && !isObjectSchema(d.output, false))
			why = "its output schema is not a JSON object";
		if (why != NULL)
		{
			std::fprintf(stderr, "[mcp] tool %s left out: %s\n", d.name.c_str(), why);
			continue;
		}
		sound.push_back(d);
	}
	std::stable_sort(sound.begin(), sound.end(), byName);

	std::vector<ToolDef> kept;
	for (size_t i = 0; i < sound.size(); ++i)
	{
		if (!kept.empty() && kept.back().name == sound[i].name)
		{
			std::fprintf(stderr, "[mcp] tool %s left out: the name is taken\n", sound[i].name.c_str());
			continue;
		}
		kept.push_back(sound[i]);
	}
	return kept;
}

bool reaches(AuthLevel have, AuthLevel need)
{
	return (int) have >= (int) need;
}

bool offered(const Caller &c, const ToolDef &d)
{
	return (d.group & c.groups) != 0;
}

std::string groupKeyOf(unsigned bit)
{
	const std::vector<std::string> k = groupKeys(bit);
	return k.empty() ? std::string("none") : k[0];
}

void writeTool(httpd::Json &j, const ToolDef &d)
{
	j.beginObject();
	j.key("name");
	j.value(d.name);
	if (!d.title.empty())
	{
		j.key("title");
		j.value(d.title);
	}
	j.key("description");
	j.value(d.description);
	j.key("inputSchema");
	j.raw(d.input.c_str());
	if (!d.image && !d.output.empty())
	{
		j.key("outputSchema");
		j.raw(d.output.c_str());
	}
	j.key("annotations");
	j.beginObject();
	j.key("readOnlyHint");
	j.value(d.read_only);
	j.key("destructiveHint");
	j.value(d.destructive);
	j.key("idempotentHint");
	j.value(d.idempotent);
	// Every tool acts on this box only.
	j.key("openWorldHint");
	j.value(false);
	j.endObject();
	j.endObject();
}

Response listTools(const Message &m, Era era, const Admission &a, ToolSource *tools)
{
	// No list is long enough to page, so no cursor was ever handed out.
	if (!member(m.params, "cursor").isNull())
		return rpcError(StatusOk, m.id, kInvalidParams, "Invalid cursor");

	const std::vector<ToolDef> defs = usableTools(tools);
	std::string result;
	httpd::Json j(result);
	j.beginObject();
	j.key("tools");
	j.beginArray();
	for (size_t i = 0; i < defs.size(); ++i)
	{
		if (offered(a.caller, defs[i]) && reaches(a.caller.level, defs[i].level))
			writeTool(j, defs[i]);
	}
	j.endArray();
	if (era == Era::Modern)
	{
		j.key("ttlMs");
		j.value(kListTtlMs);
		// The list depends on the token's level and its groups.
		j.key("cacheScope");
		j.value("private");
		writeModernTail(j);
	}
	j.endObject();
	return jsonAnswer(StatusOk, resultResponse(m.id, result));
}

Response insufficientScope(const JsonValue &id, const std::string &metadata_url, AuthLevel need)
{
	const std::string scope = scopeFor(need);
	Response r = rpcError(StatusForbidden, id, kInsufficientScope, "This tool needs the " + scope + " scope");
	r.headers.push_back(std::make_pair(std::string("WWW-Authenticate"),
	                                   bearerChallenge(metadata_url, "insufficient_scope", scope)));
	return r;
}

// A text block of its own, so the result's own text and structure stay as the tool answered.
void appendNote(httpd::Json &j, const std::string &note)
{
	if (note.empty())
		return;
	j.beginObject();
	j.key("type");
	j.value("text");
	j.key("text");
	j.value(note);
	j.endObject();
}

std::string toolResult(const std::string &text, bool is_error, const std::string *structured, Era era,
                       const std::string &note = std::string())
{
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.key("content");
	j.beginArray();
	j.beginObject();
	j.key("type");
	j.value("text");
	j.key("text");
	j.value(text);
	j.endObject();
	appendNote(j, note);
	j.endArray();
	if (structured != NULL)
	{
		j.key("structuredContent");
		j.raw(structured->c_str());
	}
	j.key("isError");
	j.value(is_error);
	if (era == Era::Modern)
		writeModernTail(j);
	j.endObject();
	return out;
}

std::string failedResult(const char *message, Era era, const std::string &note)
{
	const coreapi::Error e(coreapi::Status::Internal, coreapi::ErrorCode::BoxUnreadable, message);
	return toolResult(errorText(e, std::string()), true, NULL, era, note);
}

std::string imageResult(const JsonValue &v, Era era, const std::string &note)
{
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.key("content");
	j.beginArray();
	j.beginObject();
	j.key("type");
	j.value("image");
	j.key("data");
	j.value(v["data"].asString());
	j.key("mimeType");
	j.value(v["mime_type"].asString());
	j.endObject();
	if (v["width"].isUInt() && v["height"].isUInt())
	{
		j.beginObject();
		j.key("type");
		j.value("text");
		j.key("text");
		j.value(std::to_string(v["width"].asUInt()) + "x" + std::to_string(v["height"].asUInt()) + " " +
		        v["mime_type"].asString());
		j.endObject();
	}
	appendNote(j, note);
	j.endArray();
	j.key("isError");
	j.value(false);
	if (era == Era::Modern)
		writeModernTail(j);
	j.endObject();
	return out;
}

std::string callResult(const CallAnswer &ans, Era era, unsigned timeout_ms, const ToolSource &tools,
                       const std::string &name, bool image, const std::string &note)
{
	switch (ans.outcome)
	{
		case CallOutcome::Busy:
			return toolResult("The box is still working on earlier requests. Try again in a few seconds.",
			                  true, NULL, era, note);
		case CallOutcome::TimedOut:
		{
			char text[200];
			std::snprintf(text, sizeof(text),
			              "The box did not finish this within %u s. It may still complete; "
			              "read the current state before trying again.",
			              (timeout_ms + 999) / 1000);
			return toolResult(text, true, NULL, era, note);
		}
		case CallOutcome::Done:
			break;
	}

	if (ans.thrown)
		return failedResult("the tool failed", era, note);
	if (!ans.ok)
		return toolResult(errorText(ans.error, tools.hint(name, ans.error.code)), true, NULL, era, note);

	// Re-written, because the parser lets a raw control byte through inside a string.
	JsonValue value;
	if (!parseJson(ans.value, limits().max_json_depth, value))
		return failedResult("the tool answered with something that is not JSON", era, note);

	if (image)
	{
		if (!value.isObject() || !value["mime_type"].isString() || !value["data"].isString())
			return failedResult("the tool answered a picture without its data", era, note);
		return imageResult(value, era, note);
	}

	// The older revisions take structured content only as an object.
	const bool structured = value.isObject() || era == Era::Modern;
	if (!structured)
		return toolResult(ans.value, false, NULL, era, note);

	std::string restructured;
	if (!toJson(value, restructured))
		return failedResult("the tool answered with something that is not JSON", era, note);
	return toolResult(ans.value, false, &restructured, era, note);
}

// Set when the call is to be left running.
struct Later
{
	CallFinished finished;
	void        *cls;
	Deferred    *out;
	bool         started;
};

Response callAnswer(const Deferred &d, const CallAnswer &ans)
{
	const Era era = d.modern ? Era::Modern : Era::Legacy;
	return jsonAnswer(StatusOk,
	                  resultResponse(d.id, callResult(ans, era, d.timeout_ms, *d.tools, d.tool, d.image,
	                                                  takeApplyNote(d.connection))));
}

Response callTool(const Head &h, const Message &m, Era era, const Admission &a, ToolSource *tools, Later *later)
{
	const JsonValue &name = member(m.params, "name");
	if (!name.isString())
		return rpcError(StatusOk, m.id, kInvalidParams, "params.name must be a string");

	if (era == Era::Modern)
	{
		std::string mirrored;
		if (h.mcp_name_count != 1 || !decodeMirrored(h.mcp_name, mirrored))
			return rpcError(StatusBadRequest, m.id, kHeaderMismatch,
			                "Exactly one well-formed Mcp-Name header is required");
		if (mirrored != name.asString())
			return rpcError(StatusBadRequest, m.id, kHeaderMismatch,
			                "Header mismatch: Mcp-Name does not name the tool in the body");
	}

	const bool given = m.params.isObject() && m.params.isMember("arguments");
	const JsonValue &arguments = member(m.params, "arguments");
	if (given && !arguments.isObject())
		return rpcError(StatusOk, m.id, kInvalidParams, "params.arguments must be an object");
	JsonText args = "{}";
	if (given && !toJson(arguments, args))
		return rpcError(StatusOk, m.id, kInvalidParams, "params.arguments is nested too deeply");

	const std::vector<ToolDef> defs = usableTools(tools);
	const ToolDef *def = NULL;
	for (size_t i = 0; i < defs.size() && def == NULL; ++i)
	{
		if (defs[i].name == name.asString())
			def = &defs[i];
	}
	if (def == NULL)
		return rpcError(StatusOk, m.id, kInvalidParams, "Unknown tool: " + name.asString());
	if (!offered(a.caller, *def))
	{
		const coreapi::Error e(coreapi::Status::Denied, coreapi::ErrorCode::GroupNotEnabled,
		                       "the tool " + def->name + " is in the group " + groupKeyOf(def->group) +
		                       ", which is off for this connection; the owner turns it on for this connection "
		                       "in the KI tab of ni-web");
		return jsonAnswer(StatusOk, resultResponse(m.id, toolResult(errorText(e, std::string()), true, NULL, era)));
	}
	if (!reaches(a.caller.level, def->level))
		return insufficientScope(m.id, a.metadata_url, def->level);

	const Limits l = limits();
	Deferred own;
	Deferred &d = (later != NULL) ? *later->out : own;
	d.id = m.id;
	d.modern = (era == Era::Modern);
	d.tool = def->name;
	d.image = def->image;
	d.timeout_ms = l.call_timeout_ms;
	d.tools = tools;
	d.connection = a.caller.connection;
	if (later == NULL)
		return callAnswer(d, runCall(tools, a.caller, def->name, args, l.call_timeout_ms, l.max_running_calls));

	if (later->finished != NULL &&
	    startCall(tools, a.caller, def->name, args, l.call_timeout_ms, l.max_running_calls,
	              later->finished, later->cls))
	{
		later->started = true;
		return Response();
	}
	CallAnswer busy;
	busy.outcome = CallOutcome::Busy;
	return callAnswer(d, busy);
}

Response modern(const Head &h, const Message &m, const Admission &a, ToolSource *tools, Later *later)
{
	if (h.mcp_method_count != 1)
		return rpcError(StatusBadRequest, m.id, kHeaderMismatch, "Exactly one Mcp-Method header is required");
	if (h.mcp_method != m.method)
		return rpcError(StatusBadRequest, m.id, kHeaderMismatch,
		                "Header mismatch: Mcp-Method does not name the method in the body");

	const JsonValue &meta = member(m.params, "_meta");
	const JsonValue &version = member(meta, kMetaVersion);
	if (!version.isString())
		return rpcError(StatusBadRequest, m.id, kInvalidParams, std::string("params._meta has no ") + kMetaVersion);
	if (!isSupportedVersion(version.asString()))
		return unsupportedVersion(m.id, version.asString());
	if (version.asString() != h.protocol_version)
		return rpcError(StatusBadRequest, m.id, kHeaderMismatch,
		                "Header mismatch: MCP-Protocol-Version does not match params._meta");
	if (!member(meta, kMetaCapabilities).isObject())
		return rpcError(StatusBadRequest, m.id, kInvalidParams,
		                std::string("params._meta has no ") + kMetaCapabilities);

	if (m.method == "server/discover")
		return discover(m);
	if (m.method == "tools/list")
		return listTools(m, Era::Modern, a, tools);
	if (m.method == "tools/call")
		return callTool(h, m, Era::Modern, a, tools, later);
	return rpcError(StatusNotFound, m.id, kMethodNotFound, "Method not found");
}

Response legacy(const Head &h, const Message &m, const Admission &a, ToolSource *tools, Later *later)
{
	if (m.method == "ping")
		return jsonAnswer(StatusOk, resultResponse(m.id, "{}"));
	if (m.method == "tools/list")
		return listTools(m, Era::Legacy, a, tools);
	if (m.method == "tools/call")
		return callTool(h, m, Era::Legacy, a, tools, later);
	return rpcError(StatusOk, m.id, kMethodNotFound, "Method not found");
}

} // namespace

size_t toolDefinitionBytes(const ToolDef &d)
{
	std::string out;
	httpd::Json j(out);
	writeTool(j, d);
	return out.size();
}

bool handles(const std::string &path)
{
	return path == "/mcp" && installed(NULL);
}

size_t maxBodyBytes()
{
	return limits().max_body_bytes;
}

Admission admit(const Head &h)
{
	Wiring w;
	// The gate answers Refused before this is reached; this keeps it so without the gate.
	if (!installed(&w) || h.origin == Origin::Refused)
		return refused(noSuchPath());
	if (h.base.empty())
		return refused(problem(StatusServiceUnavailable, coreapi::ErrorCode::WebserverNotConfigured,
		                       "no address is configured for this endpoint"));

	const bool tunnel = (h.origin == Origin::Tunnel);
	const std::string resource = tunnel ? resourceOf(h.base) : std::string();
	const std::string metadata_url = tunnel ? resourceMetadataOf(h.base) : std::string();
	const std::string least = tunnel ? std::string("read") : std::string();

	// No Origin is a client that is not a browser page; a page has to be the box's own.
	if (h.origin_header_count == 1)
	{
		const std::string from = originOf(h.origin_header);
		if (from.empty() || from != originOf(h.base))
			return refused(transportError(StatusForbidden, kInvalidRequest, "Origin not allowed"));
	}
	if (h.origin_header_count > 1)
		return refused(transportError(StatusForbidden, kInvalidRequest, "Origin not allowed"));

	if (h.method != Post)
	{
		Response r = transportError(StatusMethodNotAllowed, kInvalidRequest,
		                            "Only POST is accepted at this endpoint");
		r.headers.push_back(std::make_pair(std::string("Allow"), std::string("POST")));
		return refused(r);
	}

	if (h.authorization_count > 1)
		return refused(challenge(StatusBadRequest, metadata_url, "invalid_request", least));
	const std::string token = (h.authorization_count == 1) ? bearerToken(h.authorization) : std::string();
	if (token.empty())
		return refused(challenge(StatusUnauthorized, metadata_url, NULL, least));

	const coreapi::Result<Caller> who = w.verify(token, h.origin, resource);
	if (!who.ok())
	{
		if (who.error().status == coreapi::Status::Internal)
			return refused(problem(StatusInternalServerError, coreapi::ErrorCode::BoxUnreadable,
			                       "the token could not be checked"));
		return refused(challenge(StatusUnauthorized, metadata_url, "invalid_token", least));
	}

	unsigned retry_after = 0;
	if (!rateAllows(who.value().client_id, &retry_after))
		return refused(rateRefusal(retry_after));

	if (!isJsonMediaType(h.content_type))
		return refused(transportError(StatusUnsupportedMedia, kInvalidRequest,
		                              "Content-Type must be application/json"));

	Admission a;
	a.admitted = true;
	a.caller = who.value();
	a.resource = resource;
	a.metadata_url = metadata_url;
	return a;
}

namespace
{

Response reply(const Head &h, const Admission &a, const std::string &body, Later *later)
{
	Wiring w;
	if (!a.admitted || !installed(&w))
		return noSuchPath();

	Message m;
	switch (readMessage(body, limits().max_json_depth, m))
	{
		case ReadOutcome::ParseError:
			return transportError(StatusBadRequest, kParseError, "Parse error");
		case ReadOutcome::InvalidRequest:
			return rpcError(StatusBadRequest, m.id, kInvalidRequest, "Invalid request");
		case ReadOutcome::Ok:
			break;
	}

	// Checked before a notification is waved through, so a bad version header is never silent.
	if (h.protocol_version_count > 1)
		return rpcError(StatusBadRequest, m.id, kHeaderMismatch,
		                "Exactly one MCP-Protocol-Version header is allowed");
	if (h.protocol_version_count == 1 && !isSupportedVersion(h.protocol_version))
		return unsupportedVersion(m.id, h.protocol_version);

	if (m.kind != MessageKind::Request)
		return accepted();

	// The older handshake names its version in the body, before a header can.
	if (m.method == "initialize")
		return initialize(m);

	if (h.protocol_version_count == 0)
		return rpcError(StatusBadRequest, m.id, kHeaderMismatch, "An MCP-Protocol-Version header is required");
	if (h.protocol_version == kModern)
		return modern(h, m, a, w.tools, later);
	return legacy(h, m, a, w.tools, later);
}

} // namespace

Response answer(const Head &h, const Admission &a, const std::string &body)
{
	return reply(h, a, body, NULL);
}

bool answerLater(const Head &h, const Admission &a, const std::string &body, CallFinished finished,
                 void *cls, Response &now, Deferred &later)
{
	Later l = { finished, cls, &later, false };
	now = reply(h, a, body, &l);
	return l.started;
}

Response respond(const Deferred &later, const CallAnswer &ans)
{
	return callAnswer(later, ans);
}

} // namespace mcp
} // namespace httpd
