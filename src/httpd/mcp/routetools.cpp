/*
 * routetools.cpp - tools answered by their own routes, in process
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

#include "httpd/mcp/routetools.h"

#include "httpd/auth.h"
#include "httpd/endpoints.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/router.h"
#include "httpd/mcp/allowlist.h"
#include "httpd/mcp/jsonrpc.h"
#include "httpd/mcp/limits.h"
#include "httpd/mcp/picture.h"
#include "httpd/mcp/problem.h"
#include "httpd/mcp/settingsindex.h"
#include "httpd/mcp/toolgen.h"
#include "httpd/mcp/toolgroups.h"
#include "httpd/mcp/toolguard.h"
#include "httpd/mcp/toolhint.h"

#include "coreapi/base/apply.h"
#include "coreapi/base/errors.h"

#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>

namespace httpd
{
namespace mcp
{

namespace
{

coreapi::Error badArg(coreapi::ErrorCode code, const std::string &name, const char *says)
{
	return coreapi::Error(coreapi::Status::InvalidArgument, code, name + " " + says);
}

const Param *paramNamed(const Endpoint &ep, const std::string &name)
{
	for (size_t i = 0; i < ep.param_count; ++i)
	{
		const Param &p = ep.params[i];
		if (p.name != NULL && !namesWholeBody(p.in) && name == p.name)
			return &p;
	}
	return NULL;
}

// The router reads text, so the JSON kind is held here.
bool kindFits(const Param &p, JsonValueKind k, coreapi::ErrorCode &code, const char *&says)
{
	switch (p.type)
	{
		case ParamType::Int:
		case ParamType::UInt:
		case ParamType::Time:
			code = coreapi::ErrorCode::BadInt;
			says = "has to be a JSON number";
			return k == JsonValueKind::Number;
		case ParamType::Bool:
			code = coreapi::ErrorCode::BadBool;
			says = "has to be true or false";
			return k == JsonValueKind::Bool;
		case ParamType::String:
		case ParamType::Enum:
		case ParamType::ChannelId:
			code = coreapi::ErrorCode::BadString;
			says = "has to be a JSON string";
			return k == JsonValueKind::String;
	}
	code = coreapi::ErrorCode::BadTable;
	says = "has a kind this server does not know";
	return false;
}

void appendEncoded(std::string &out, const std::string &s)
{
	static const char kHex[] = "0123456789ABCDEF";
	for (size_t i = 0; i < s.size(); ++i)
	{
		const unsigned char c = (unsigned char) s[i];
		const bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
		if (plain)
		{
			out += (char) c;
			continue;
		}
		out += '%';
		out += kHex[c >> 4];
		out += kHex[c & 15];
	}
}

bool buildRequest(const Endpoint &ep, const std::vector<JsonMember> &args, std::string &path,
                  std::string &query, std::string &body, coreapi::Error &refused)
{
	std::vector<const Param *> rows;
	bool any_body = false;
	for (size_t i = 0; i < args.size(); ++i)
	{
		const Param *p = paramNamed(ep, args[i].name);
		if (p == NULL)
		{
			refused = badArg(coreapi::ErrorCode::NoSuchParameter, args[i].name,
			                 "is not an argument of this tool");
			return false;
		}
		for (size_t k = 0; k < i; ++k)
		{
			if (args[k].name == args[i].name)
			{
				refused = badArg(coreapi::ErrorCode::DuplicateParameter, args[i].name, "is given twice");
				return false;
			}
		}
		coreapi::ErrorCode code = coreapi::ErrorCode::BadString;
		const char *says = "";
		if (!kindFits(*p, args[i].kind, code, says))
		{
			refused = badArg(code, args[i].name, says);
			return false;
		}
		any_body = any_body || p->in == In::Body;
		rows.push_back(p);
	}

	path.clear();
	for (const char *cur = ep.path; *cur != '\0';)
	{
		if (*cur != '{')
		{
			path += *cur++;
			continue;
		}
		const char *brace = std::strchr(cur, '}');
		if (brace == NULL)
		{
			refused = coreapi::Error(coreapi::Status::Internal, coreapi::ErrorCode::BadTable,
			                         "this route's path does not close a brace");
			return false;
		}
		const std::string name(cur + 1, brace);
		const std::string *value = NULL;
		for (size_t i = 0; i < args.size() && value == NULL; ++i)
		{
			if (args[i].name == name)
				value = &args[i].text;
		}
		if (value == NULL || value->empty())
		{
			refused = badArg(coreapi::ErrorCode::MissingParameter, name, "is required");
			return false;
		}
		appendEncoded(path, *value);
		cur = brace + 1;
	}

	query.clear();
	body.clear();
	Json j(body);
	if (any_body)
		j.beginObject();
	for (size_t i = 0; i < args.size(); ++i)
	{
		if (rows[i]->in == In::Query)
		{
			if (!query.empty())
				query += '&';
			appendEncoded(query, args[i].name);
			query += '=';
			appendEncoded(query, args[i].text);
			continue;
		}
		if (rows[i]->in != In::Body)
			continue;
		j.key(args[i].name.c_str());
		switch (args[i].kind)
		{
			case JsonValueKind::String:
				j.value(args[i].text);
				break;
			case JsonValueKind::Number:
				j.raw(args[i].text.c_str());
				break;
			case JsonValueKind::Bool:
				j.value(args[i].text == "true");
				break;
		}
	}
	if (any_body)
		j.endObject();
	return true;
}

bool gateAdmits(const Endpoint &ep, const JsonText &args, coreapi::Error &refused)
{
	JsonValue v;
	if (!parseJson(args, limits().max_json_depth, v) || !v.isObject())
	{
		refused = coreapi::Error(coreapi::Status::InvalidArgument, coreapi::ErrorCode::BadString,
		                         "the arguments are not one JSON object");
		return false;
	}
	if (ep.method == Post)
	{
		const std::string name = v["name"].isString() ? v["name"].asString() : std::string();
		if (!pluginAllowed(name))
		{
			refused = coreapi::Error(coreapi::Status::Denied, coreapi::ErrorCode::PluginNotAllowed,
			                         "the owner has not allowed AI clients to start the plugin " + name);
			return false;
		}
		return true;
	}
	const std::string section = v["section"].isString() ? v["section"].asString() : std::string();
	if (!sectionDenial(section).empty())
	{
		refused = coreapi::Error(coreapi::Status::InvalidArgument, coreapi::ErrorCode::SettingsSectionDenied,
		                         "no AI client may change the settings section " + section);
		return false;
	}
	if (!sectionAllowed(section))
	{
		refused = coreapi::Error(coreapi::Status::Denied, coreapi::ErrorCode::SettingsSectionNotAllowed,
		                         "the owner has not allowed AI clients to change the settings section " + section);
		return false;
	}
	std::string body;
	std::string key;
	std::string why;
	if (v["settings"].isObject() && toJson(v["settings"], body) && !(key = deniedKeyIn(body, &why)).empty())
	{
		refused = coreapi::Error(coreapi::Status::InvalidArgument, coreapi::ErrorCode::SettingsSectionDenied,
		                         "no AI client may change the setting " + key + ", which " + why);
		return false;
	}
	return true;
}

const Param *wholeBodyOf(const Endpoint &ep)
{
	for (size_t i = 0; i < ep.param_count; ++i)
	{
		if (ep.params[i].in == In::BodyList || ep.params[i].in == In::BodyMap)
			return &ep.params[i];
	}
	return NULL;
}

// The whole-body argument comes out as the body; the rest goes back as flat text.
bool takeWholeBody(const Param &p, const JsonText &args, JsonText &rest, std::string &body,
                   coreapi::Error &refused)
{
	JsonValue v;
	if (!parseJson(args, limits().max_json_depth, v) || !v.isObject())
	{
		refused = badArg(coreapi::ErrorCode::BadString, "arguments", "are not one JSON object");
		return false;
	}
	body.clear();
	if (!v.isMember(p.name) || v[p.name].isNull())
	{
		refused = badArg(coreapi::ErrorCode::MissingParameter, p.name, "is required");
		return false;
	}
	const JsonValue &whole = v[p.name];
	const bool list = p.in == In::BodyList;
	bool fits = list ? whole.isArray() : whole.isObject();
	for (JsonValue::const_iterator it = whole.begin(); fits && it != whole.end(); ++it)
		fits = list ? (*it).isString() : ((*it).isString() || (*it).isNumeric() || (*it).isBool());
	if (!fits)
	{
		refused = badArg(coreapi::ErrorCode::BadString, p.name,
		                 list ? "has to be a JSON array of strings" :
		                        "has to be a JSON object of strings, numbers or booleans");
		return false;
	}
	if (!toJson(whole, body))
	{
		refused = badArg(coreapi::ErrorCode::BadString, p.name, "could not be read");
		return false;
	}
	v.removeMember(p.name);
	return toJson(v, rest);
}

coreapi::Result<JsonText> pictureFrom(Response &r)
{
	if (r.code < 200 || r.code > 299)
	{
		if (r.fd >= 0)
			::close(r.fd);
		r.fd = -1;
		return coreapi::fail(errorFromProblem(r));
	}
	// A composed tool already built its own answer; nothing here reads or re-encodes it.
	if (r.fd < 0 && r.content_type == "application/json")
		return coreapi::ok(r.body);
	if (r.content_type != "image/png" && r.content_type != "image/jpeg" && r.content_type != "image/gif")
	{
		if (r.fd >= 0)
			::close(r.fd);
		r.fd = -1;
		return coreapi::fail(coreapi::Status::NotFound, coreapi::ErrorCode::NoSuchLogo,
		                     "the picture could not be read");
	}
	const size_t most = kMaxImageText / 4 * 3;
	std::string bytes;
	bool read_failed = false;
	if (r.fd >= 0)
	{
		char buf[16384];
		ssize_t n = 0;
		while (bytes.size() <= most && (n = ::read(r.fd, buf, sizeof(buf))) > 0)
			bytes.append(buf, (size_t) n);
		read_failed = n < 0;
		::close(r.fd);
		r.fd = -1;
	}
	else
		bytes = r.body;
	if (read_failed || bytes.empty())
		return coreapi::fail(coreapi::Status::NotFound, coreapi::ErrorCode::NoSuchLogo,
		                     "the picture could not be read");
	if (bytes.size() > most)
		return coreapi::fail(coreapi::Status::Internal, coreapi::ErrorCode::OutputTooLarge,
		                     "the picture is larger than an answer may carry");
	std::string out;
	Json j(out, 64 + bytes.size() / 3 * 4);
	j.beginObject();
	j.key("mime_type");
	j.value(r.content_type);
	j.key("data");
	j.value(encodeBase64(bytes));
	j.endObject();
	return coreapi::ok(out);
}

/* A per key answer as text a client can act on: which keys landed, so it does not send them
   again, and for each one that did not its code, detail and the settings it depends on. */
std::string partlyWritten(const std::string &body)
{
	JsonValue v;
	if (!parseJson(body, limits().max_json_depth, v) || !v.isObject() || !v["results"].isObject())
		return "not every value was written: " + body;
	const JsonValue &results = v["results"];
	std::string landed;
	std::string refused;
	const std::vector<std::string> keys = results.getMemberNames();
	for (size_t i = 0; i < keys.size(); ++i)
	{
		const JsonValue &one = results[keys[i]];
		if (!one.isMember("code"))
		{
			landed += (landed.empty() ? "" : ", ") + keys[i];
			continue;
		}
		refused += "\n- " + keys[i] + ": " + one["code"].asString() + ", " + one["detail"].asString();
		const JsonValue &deps = one["depends_on"];
		for (JsonValue::ArrayIndex d = 0; deps.isArray() && d < deps.size(); ++d)
			refused += (d == 0 ? " (depends on " : ", ") + deps[d].asString() + (d + 1 == deps.size() ? ")" : "");
	}
	return "some settings were written and some were not.\nwritten: " + (landed.empty() ? std::string("none") : landed) +
	       "\nnot written:" + refused;
}

coreapi::Result<JsonText> resultFrom(Response &r, const Endpoint &ep, bool image)
{
	if (image)
		return pictureFrom(r);
	if (r.fd >= 0)
	{
		::close(r.fd);
		r.fd = -1;
		return coreapi::fail(coreapi::Status::Internal, coreapi::ErrorCode::BadTable,
		                     "this route answers with a file, which no tool can carry");
	}
	if (!r.stream_argv.empty() || !r.relay_url.empty() || !r.reload_after.empty())
		return coreapi::fail(coreapi::Status::Internal, coreapi::ErrorCode::BadTable,
		                     "this route answers with something that is not a document");
	if (r.code < 200 || r.code > 299)
		return coreapi::fail(errorFromProblem(r));
	// A results object cannot be a tool's declared structured answer.
	if (r.code == StatusMultiStatus)
		return coreapi::fail(coreapi::Status::Internal, coreapi::ErrorCode::SettingNotWritten,
		                     partlyWritten(r.body));
	if (r.body.empty())
		return coreapi::ok(std::string(r.code == StatusAccepted ? "{\"status\":\"accepted\"}"
		                                                        : "{\"status\":\"done\"}"));
	if (ep.schema == NULL)
		return coreapi::fail(coreapi::Status::Internal, coreapi::ErrorCode::BadTable,
		                     "this route answered a document it does not describe");
	return coreapi::ok(r.body);
}

} // namespace

RouteTools::RouteTools(const RouteTable *const *tables, size_t table_count,
                       const RouteTable &composed)
{
	if (!toolsAreSane(tables, table_count, composed, &refusal_))
	{
		if (refusal_.empty())
			refusal_ = "the tools broke a rule";
		return;
	}
	for (size_t t = 0; t < table_count; ++t)
		add(*tables[t]);
	add(composed);
}

const std::string &RouteTools::refusal() const
{
	return refusal_;
}

void RouteTools::add(const RouteTable &t)
{
	for (size_t i = 0; i < t.tool_count; ++i)
	{
		Entry e;
		e.route = flaggedRoute(t, t.tools[i]);
		e.table = &t;
		e.flag = &t.tools[i];
		e.def = toolFor(*e.route, t.tools[i]);
		e.def.group = groupOfTool(e.def.name);
		if (isSettingsSchemaRoute(*e.route))
			e.def.output = withSettingsIndex(e.def.output);
		entries_.push_back(e);
	}
}

const RouteTools::Entry *RouteTools::find(const std::string &name) const
{
	for (size_t i = 0; i < entries_.size(); ++i)
	{
		if (entries_[i].def.name == name)
			return &entries_[i];
	}
	return NULL;
}

// Every tool, gated ones too: a client keeps the list, and the allowlists are checked per call.
std::vector<ToolDef> RouteTools::list()
{
	std::vector<ToolDef> out;
	for (size_t i = 0; i < entries_.size(); ++i)
		out.push_back(entries_[i].def);
	return out;
}

bool RouteTools::offers(const Caller &c, const char *name) const
{
	const Entry *e = find(name);
	return e != NULL && (e->def.group & c.groups) != 0 && (int) c.level >= (int) e->def.level;
}

std::string RouteTools::hint(const std::string &name, coreapi::ErrorCode code) const
{
	const Entry *e = find(name);
	if (e == NULL)
		return std::string();
	const char *text = retryHint(code, *e->route);
	return text != NULL ? std::string(text) : std::string();
}

coreapi::Result<JsonText> RouteTools::call(const Caller &c, const std::string &name,
                                           const JsonText &args)
{
	const Entry *e = find(name);
	if (e == NULL)
		return coreapi::fail(coreapi::Status::NotFound, coreapi::ErrorCode::NoSuchTool,
		                     "no tool is called " + name);
	if ((int) c.level < (int) e->def.level)
		return coreapi::fail(coreapi::Status::Denied, coreapi::ErrorCode::NotPermitted,
		                     std::string("this tool needs the ") + authLevelName(e->def.level) + " scope");

	if (isSettingsSchemaRoute(*e->route) && asksSettingsIndex(args))
		return settingsIndex(offers(c, "read_settings"), offers(c, "write_settings"));

	if (gated(e->route->method, e->route->path))
	{
		coreapi::Error refused;
		if (!gateAdmits(*e->route, args, refused))
			return coreapi::fail(refused);
	}

	JsonText flat = args;
	std::string whole_body;
	const Param *whole = wholeBodyOf(*e->route);
	if (whole != NULL)
	{
		coreapi::Error refused_whole;
		if (!takeWholeBody(*whole, args, flat, whole_body, refused_whole))
			return coreapi::fail(refused_whole);
	}

	// Clients send null for an argument they leave out.
	std::vector<JsonMember> members;
	if (!readFlatObject(flat, members, true))
		return coreapi::fail(coreapi::Status::InvalidArgument, coreapi::ErrorCode::BadString,
		                     "the arguments are not one flat object of strings, numbers and booleans");

	std::string path;
	std::string query;
	std::string body;
	coreapi::Error refused;
	if (!buildRequest(*e->route, members, path, query, body, refused))
		return coreapi::fail(refused);
	if (whole != NULL)
		body = whole_body;

	if (e->flag != NULL && e->flag->defaults != NULL)
	{
		const std::string all = e->flag->defaults;
		size_t at = 0;
		while (at < all.size())
		{
			size_t end = all.find('&', at);
			if (end == std::string::npos)
				end = all.size();
			const std::string pair = all.substr(at, end - at);
			const std::string flag_name = pair.substr(0, pair.find('='));
			bool given = false;
			for (size_t i = 0; i < members.size() && !given; ++i)
				given = members[i].name == flag_name;
			if (!given)
				query += (query.empty() ? "" : "&") + pair;
			at = end + 1;
		}
	}

	// Only this route can match, so no value steers the call to a sibling.
	const RouteTable one = { HTTPD_TABLE_N(e->table->tag, e->route, 1) };
	const coreapi::WriterScope writer(c.connection.empty() ? std::string() : "mcp:" + c.connection);
	Response r = dispatchIn(one, e->route->method, path, query, body, std::string(), c.level,
	                        std::string(), std::string(), std::string(), std::string(),
	                        c.external ? Origin::Tunnel : Origin::Lan);
	return resultFrom(r, *e->route, e->flag != NULL && e->flag->image);
}

} // namespace mcp
} // namespace httpd
