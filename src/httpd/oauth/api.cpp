/*
 * api.cpp - the ni-web routes that list, create and revoke AI clients
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

#include "httpd/oauth/api.h"

#include "httpd/auth.h"
#include "httpd/endpoints.h"
#include "httpd/json.h"
#include "httpd/mcp/contract.h"
#include "httpd/mcp/endpoint.h"
#include "httpd/mcp/toolgroups.h"
#include "httpd/oauth/scopes.h"
#include "httpd/oauth/store.h"
#include "httpd/oauth/uri.h"
#include "httpd/schema.h"
#include "httpd/status.h"
#include "httpd/webconfig.h"

#include "coreapi/base/errors.h"

#include <string>
#include <vector>

namespace httpd
{
namespace oauth
{

namespace
{

const FieldDesc kClientFields[] = {
	HTTPD_MEMBER("id", FieldType::String, "what this list and the route that revokes name the client by"),
	HTTPD_MEMBER("name", FieldType::String, "the name the client gave, or what a static token was made for"),
	HTTPD_MEMBER("kind", FieldType::String,
		"how the box knows the client: `registered` through dynamic registration, `metadata` by the address "
		"of its metadata document, `static` a token made with `POST /api/v1/ai/clients`"),
	HTTPD_LIST_OF_VALUES("scopes", ElementType::String, "what the client may do, out of read write system offline_access"),
	HTTPD_LIST_OF_VALUES("groups", ElementType::String,
		"the tool groups the client is offered, out of programme timers recordings control "
		"bouquets status settings plugins"),
	HTTPD_MEMBER("redirect_host", FieldType::String, "where sign-ins of this client return to, empty for a static token"),
	HTTPD_MEMBER("loopback_only", FieldType::Bool, "whether the client returns only to a program on a computer"),
	HTTPD_MEMBER("created", FieldType::Time, "when the client was registered or the token made"),
	HTTPD_MEMBER("last_used", FieldType::Time, "when the client last used this box, nought before the first use"),
};
const Schema kClientSchema = { "ai-client", HTTPD_FIELDS(kClientFields) };

const FieldDesc kClientListFields[] = {
	HTTPD_LIST_OF("clients", &kClientSchema, "every client holding access and every static token"),
};
const Schema kClientListSchema = { "ai-client-list", HTTPD_FIELDS(kClientListFields) };

const FieldDesc kCreatedFields[] = {
	HTTPD_MEMBER("id", FieldType::String, "what the list and the route that revokes name the token by"),
	HTTPD_MEMBER("name", FieldType::String, "what the token was made for"),
	HTTPD_MEMBER("kind", FieldType::String, "static"),
	HTTPD_LIST_OF_VALUES("scopes", ElementType::String, "what the token may do"),
	HTTPD_LIST_OF_VALUES("groups", ElementType::String,
		"the tool groups the client is offered, out of programme timers recordings control "
		"bouquets status settings plugins"),
	HTTPD_MEMBER("token", FieldType::String, "the bearer token, shown this once and never again"),
	HTTPD_MEMBER("created", FieldType::Time, "when the token was made"),
};
const Schema kCreatedSchema = { "ai-client-created", HTTPD_FIELDS(kCreatedFields) };

bool printableName(const std::string &s)
{
	if (s.empty())
		return false;
	for (size_t i = 0; i < s.size(); ++i)
	{
		const unsigned char c = (unsigned char) s[i];
		if (c < 0x20 || c == 0x7f)
			return false;
	}
	return isUtf8(s.data(), s.size());
}

void writeScopes(Json &j, unsigned bits)
{
	const std::vector<std::string> names = scopeNames(bits);
	j.beginArray();
	for (size_t i = 0; i < names.size(); ++i)
		j.value(names[i]);
	j.endArray();
}

void writeGroups(Json &j, unsigned bits)
{
	const std::vector<std::string> keys = mcp::groupKeys(bits);
	j.beginArray();
	for (size_t i = 0; i < keys.size(); ++i)
		j.value(keys[i]);
	j.endArray();
}

// Reads the body itself: Request drops an empty text as if never sent, and a substring scan would match the name inside a value.
bool hasBodyMember(const std::string &body, const char *name)
{
	std::vector<JsonMember> members;
	if (!readFlatObject(body, members))
		return false;
	for (size_t i = 0; i < members.size(); ++i)
	{
		if (members[i].name == name)
			return true;
	}
	return false;
}

void writeClient(Json &j, const Client &c)
{
	Url first;
	const bool has_first = !c.redirect_uris.empty() && parseUrl(c.redirect_uris[0], &first);
	bool loopback = !c.redirect_uris.empty();
	for (size_t k = 0; k < c.redirect_uris.size(); ++k)
	{
		Url u;
		loopback = loopback && parseUrl(c.redirect_uris[k], &u) && isLoopbackHost(u.host);
	}
	j.beginObject();
	j.key("id");
	j.value(c.key);
	j.key("name");
	j.value(c.name);
	j.key("kind");
	j.value(kindName(c.kind));
	j.key("scopes");
	writeScopes(j, c.scopes);
	j.key("groups");
	writeGroups(j, c.groups);
	j.key("redirect_host");
	j.value(has_first ? first.host : std::string());
	j.key("loopback_only");
	j.value(loopback);
	j.key("created");
	j.value((long long) c.created);
	j.key("last_used");
	j.value((long long) c.last_used);
	j.endObject();
}

Response listClientsRoute(const Request &)
{
	const std::vector<Client> list = store().listClients();
	Response out = okJson();
	Json j(out.body, 64 + list.size() * 320);
	j.beginObject();
	j.key("clients");
	j.beginArray();
	for (size_t i = 0; i < list.size(); ++i)
		writeClient(j, list[i]);
	j.endArray();
	j.endObject();
	return out;
}

Response createStaticRoute(const Request &r)
{
	const std::string &name = r.asString("name");
	if (!printableName(name))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::BadString,
		                       "the name must be printable text");
	unsigned bits = 0;
	if (!parseScopes(r.asString("scopes"), &bits) || (bits & ScopeOffline) != 0 || (bits & ScopeLevels) == 0)
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::NotAListedValue,
		                       "scopes are read, write and system, at least one of them");
	unsigned groups = mcp::kDefaultGroups;
	if (hasBodyMember(r.body(), "groups") && !mcp::readGroupKeys(r.asString("groups"), &groups))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::UnknownGroup,
		                       "groups names a group this box does not have");

	std::string user = sessionUser(r.session());
	if (user.empty())
		user = config().username;
	Client c;
	std::string token;
	if (!store().createStatic(name, bits, user, &c, &token, groups))
		return problemResponse(StatusConflict, coreapi::ErrorCode::NoRoomForAResult,
		                       "this box holds as many static tokens as it keeps; revoke one first");

	Response out = okJson();
	out.code = StatusCreated;
	Json j(out.body, 256);
	j.beginObject();
	j.key("id");
	j.value(c.key);
	j.key("name");
	j.value(c.name);
	j.key("kind");
	j.value(kindName(c.kind));
	j.key("scopes");
	writeScopes(j, c.scopes);
	j.key("groups");
	writeGroups(j, c.groups);
	j.key("token");
	j.value(token);
	j.key("created");
	j.value((long long) c.created);
	j.endObject();
	return out;
}

Response removeClientRoute(const Request &r)
{
	if (!store().removeClient(r.asString("id")))
		return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchName,
		                       "no client by that id holds access here");
	return noContent();
}

Response setGroupsRoute(const Request &r)
{
	if (!hasBodyMember(r.body(), "groups"))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::MissingParameter, "name the groups");
	unsigned groups = 0;
	if (!mcp::readGroupKeys(r.asString("groups"), &groups))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::UnknownGroup,
		                       "groups names a group this box does not have");
	if (!store().setGroups(r.asString("id"), groups))
		return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchName,
		                       "no client by that id holds access here");
	const std::vector<Client> all = store().listClients();
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].key != r.asString("id"))
			continue;
		Response out = okJson();
		Json j(out.body, 400);
		writeClient(j, all[i]);
		return out;
	}
	return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchName,
	                       "no client by that id holds access here");
}

const FieldDesc kGroupFields[] = {
	HTTPD_MEMBER("key", FieldType::String, "the group's name"),
	HTTPD_LIST_OF_VALUES("tools", ElementType::String, "the tools of this group the box offers now"),
	HTTPD_MEMBER_OF_SET("least", "read,write,system", "the scope its least demanding tool needs",
		"read: a read scope reaches some of its tools\nwrite: write is needed\nsystem: only a system scope reaches it"),
	HTTPD_MEMBER("default", FieldType::Bool, "whether a new connection starts with it"),
	HTTPD_MEMBER("approx_tokens", FieldType::UInt, "about how many tokens of a model's context its tools' definitions take"),
};
const Schema kGroupSchema = { "ai-group", HTTPD_FIELDS(kGroupFields) };

// A group's least is never Public: every group holds at least one tool, and no tool is Public.
const char *leastName(AuthLevel a)
{
	switch (a)
	{
		case AuthLevel::Read:   return "read";
		case AuthLevel::Write:  return "write";
		default:                return "system";
	}
}
const FieldDesc kGroupListFields[] = {
	HTTPD_LIST_OF("groups", &kGroupSchema, "every tool group, in a fixed order"),
};
const Schema kGroupListSchema = { "ai-group-list", HTTPD_FIELDS(kGroupListFields) };

Response listGroupsRoute(const Request &)
{
	const std::vector<mcp::ToolDef> all = mcp::boxTools().list();
	size_t count = 0;
	const mcp::ToolGroup *table = mcp::toolGroups(&count);
	Response out = okJson();
	Json j(out.body, 2048);
	j.beginObject();
	j.key("groups");
	j.beginArray();
	for (size_t i = 0; i < count; ++i)
	{
		size_t bytes = 0;
		j.beginObject();
		j.key("key");
		j.value(table[i].key);
		j.key("tools");
		j.beginArray();
		for (size_t k = 0; k < all.size(); ++k)
		{
			if (all[k].group != table[i].bit)
				continue;
			j.value(all[k].name);
			bytes += mcp::toolDefinitionBytes(all[k]);
		}
		j.endArray();
		j.key("least");
		j.value(leastName(table[i].least));
		j.key("default");
		j.value((mcp::kDefaultGroups & table[i].bit) != 0);
		j.key("approx_tokens");
		j.value((unsigned long) (bytes / 4));
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

const Param kCreateParams[] = {
	HTTPD_BODY_REQUIRED_TEXT("name", "what the token is for, as the list will show it", 64),
	HTTPD_BODY_REQUIRED_TEXT("scopes", "space separated, out of read write system", 32),
	HTTPD_BODY_TEXT("groups", "space separated tool groups; the three defaults when left out, none when empty", 128),
};

const Param kRemoveParams[] = {
	HTTPD_SEGMENT_TEXT("id", "the client, as the list names it", 64),
};

const Param kSetGroupsParams[] = {
	HTTPD_SEGMENT_TEXT("id", "the client, as the list names it", 64),
	HTTPD_BODY_TEXT("groups", "space separated tool groups; empty for none", 128),
};

const RouteRefusal kCreateRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, BadString, "the name is not 1 to 64 bytes of printable text"),
	HTTPD_REFUSES(InvalidArgument, NotAListedValue, "scopes is not a non-empty subset of read write system"),
	HTTPD_REFUSES(InvalidArgument, UnknownGroup, "groups names a group this box does not have"),
	HTTPD_REFUSES(Conflict, NoRoomForAResult, "the box already holds 32 static tokens"),
};

const RouteRefusal kRemoveRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchName, "no client by that id holds access here"),
};

const RouteRefusal kSetGroupsRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, MissingParameter, "the body names no groups"),
	HTTPD_REFUSES(InvalidArgument, UnknownGroup, "groups names a group this box does not have"),
	HTTPD_REFUSES(NotFound, NoSuchName, "no client by that id holds access here"),
};

// System for all of them: they hand out or take away access up to System.
const Endpoint kOAuthEndpoints[] = {
	{ Method::Get, "/api/v1/ai/clients", AuthLevel::System,
	  "lists the AI clients that hold access to this box and every static token",
	  "Lists every AI client that holds access to this box: clients that signed in through the "
	  "public address (`registered` or `metadata`, by how the box learnt about them) and every "
	  "static token made with `POST /api/v1/ai/clients` (`static`). Nothing secret is listed, "
	  "neither a token nor its digest. `last_used` is `0` until the client first reaches the box.\n\n"
	  "**Related:** `POST /api/v1/ai/clients`, `DELETE /api/v1/ai/clients/{id}`.",
	  NULL, 0, &kClientListSchema, &listClientsRoute, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/ai/clients", AuthLevel::System,
	  "makes a static token for a client in the home network and hands it over once",
	  "Makes a static bearer token for a client in the home network, which sends it as "
	  "`Authorization: Bearer <token>` to `/mcp`. `scopes` is a space separated subset of `read`, "
	  "`write` and `system`. The answer is `201` with the token, shown this once and never "
	  "readable again. A static token is accepted only in the home network, never through the "
	  "public address. `groups` (optional) names the tool groups as a space separated list; left "
	  "out, the token gets `programme timers recordings`; sent empty, it gets none.\n\n"
	  "**Refusals:**\n"
	  "- `400 bad-string`: `name` is not printable text.\n"
	  "- `400 not-a-listed-value`: `scopes` is empty or names anything but `read`, `write` and "
	  "`system`, `offline_access` included.\n"
	  "- `400 unknown-group`: `groups` names a group this box does not have.\n"
	  "- `409 no-room-for-a-result`: the box already holds 32 static tokens; revoke one first.\n\n"
	  "**Related:** `GET /api/v1/ai/clients`, `DELETE /api/v1/ai/clients/{id}`.",
	  HTTPD_PARAMS(kCreateParams), &kCreatedSchema, &createStaticRoute, false,
	  Answers201, HTTPD_REFUSALS_AND_BODY(kCreateRefusals, "{\"name\":\"Home Assistant\",\"scopes\":\"read write\"}") },
	{ Method::Delete, "/api/v1/ai/clients/{id}", AuthLevel::System,
	  "revokes everything a client holds and forgets the client",
	  "Revokes everything the client holds and forgets it: every grant, access token and "
	  "refresh token of a client that signed in, or the static token itself. The client has to "
	  "sign in again, or be given a new static token, to reach the box.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-name`: no client by that `id` holds access here.\n\n"
	  "**Related:** `GET /api/v1/ai/clients`.",
	  HTTPD_PARAMS(kRemoveParams), NULL, &removeClientRoute, false,
	  Answers204, HTTPD_REFUSALS(kRemoveRefusals) },
	{ Method::Patch, "/api/v1/ai/clients/{id}", AuthLevel::System,
	  "sets which tool groups one client is offered",
	  "Sets the tool groups of one client: every grant of a client that signed in, or the static token. "
	  "`groups` is a space separated subset of `programme`, `timers`, `recordings`, `control`, `bouquets`, "
	  "`status`, `settings`, `plugins`; empty offers no tool. The client sees the change on its next request, "
	  "without signing in again. Answers the client as `GET /api/v1/ai/clients` lists it.\n\n"
	  "**Refusals:**\n"
	  "- `400 missing-parameter`: the body names no `groups`.\n"
	  "- `400 unknown-group`: `groups` names a group this box does not have.\n"
	  "- `404 no-such-name`: no client by that `id` holds access here.\n\n"
	  "**Related:** `GET /api/v1/ai/clients`, `GET /api/v1/ai/groups`.",
	  HTTPD_PARAMS(kSetGroupsParams), &kClientSchema, &setGroupsRoute, false,
	  Answers200, HTTPD_REFUSALS_AND_BODY(kSetGroupsRefusals, "{\"groups\":\"programme timers\"}") },
	{ Method::Get, "/api/v1/ai/groups", AuthLevel::System,
	  "the tool groups, their tools and about how much of a model's context each takes",
	  "Lists the fixed tool groups in their order: the tools of each the box offers now, the least scope "
	  "one of them needs, whether a new connection starts with the group, and an estimate of the context "
	  "its tool definitions take (their size in bytes divided by four).\n\n"
	  "**Related:** `PATCH /api/v1/ai/clients/{id}`.",
	  NULL, 0, &kGroupListSchema, &listGroupsRoute, false,
	  Answers200, HTTPD_NO_REFUSALS },
};

} // namespace

extern const RouteTable oauthTable = {
	HTTPD_TABLE("ai", kOAuthEndpoints)
};

} // namespace oauth
} // namespace httpd
