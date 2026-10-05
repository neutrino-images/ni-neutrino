/*
 * mcpfakes.h - what the MCP endpoint is wired to in the suite
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

#ifndef __test_mcpfakes_h__
#define __test_mcpfakes_h__

#include "httpd/endpoint.h"
#include "httpd/json.h"
#include "httpd/mcp/callrunner.h"
#include "httpd/mcp/contract.h"
#include "httpd/mcp/endpoint.h"
#include "httpd/mcp/jsonrpc.h"
#include "httpd/mcp/limits.h"
#include "httpd/mcp/ratelimit.h"
#include "httpd/mcp/wiring.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>
#include <strings.h>

namespace mcpfake
{

inline httpd::mcp::ToolDef tool(const char *name, httpd::AuthLevel level, bool read_only, bool destructive,
                                bool idempotent, const char *input, const char *output)
{
	httpd::mcp::ToolDef d;
	d.name = name;
	d.title = std::string("Title of ") + name;
	d.description = std::string("Description of ") + name;
	d.input = input;
	d.output = output;
	d.level = level;
	d.read_only = read_only;
	d.destructive = destructive;
	d.idempotent = idempotent;
	d.group = 1u;
	return d;
}

inline unsigned &slowMs()
{
	static unsigned ms = 0;
	return ms;
}

inline std::string deeplyNested(int levels)
{
	return std::string(levels, '[') + std::string(levels, ']');
}

class Tools : public httpd::mcp::ToolSource
{
	public:
		Tools() : calls_(0) {}

		std::vector<httpd::mcp::ToolDef> list()
		{
			const char *obj = "{\"type\":\"object\"}";
			std::vector<httpd::mcp::ToolDef> out;
			// Not in name order, so the order a case sees is the endpoint's.
			out.push_back(tool("whoami", httpd::AuthLevel::Read, true, false, true,
			                   "{\"type\":\"object\",\"additionalProperties\":false}", obj));
			out.push_back(tool("echo", httpd::AuthLevel::Read, true, false, true, obj, ""));
			out.push_back(tool("numbers", httpd::AuthLevel::Read, true, false, true, obj, ""));
			out.push_back(tool("latin1", httpd::AuthLevel::Read, true, false, true, obj, ""));
			out.push_back(tool("controlchars", httpd::AuthLevel::Read, true, false, true, obj, ""));
			out.push_back(tool("deep", httpd::AuthLevel::Read, true, false, true, obj, ""));
			out.push_back(tool("standby", httpd::AuthLevel::Write, false, false, false, obj, ""));
			out.push_back(tool("slow", httpd::AuthLevel::Read, true, false, true, obj, ""));
			out.push_back(tool("throws", httpd::AuthLevel::Read, true, false, true, obj, ""));
			out.push_back(tool("reboot_like", httpd::AuthLevel::System, false, true, false, obj, ""));
			out.push_back(tool("bad name!", httpd::AuthLevel::Read, true, false, true, obj, ""));
			out.push_back(tool("bad_schema", httpd::AuthLevel::Read, true, false, true, "not json", ""));
			out.push_back(tool("array_schema", httpd::AuthLevel::Read, true, false, true,
			                   "{\"type\":\"array\"}", ""));
			out.push_back(tool("echo", httpd::AuthLevel::System, false, true, false, obj, ""));
			return out;
		}

		coreapi::Result<httpd::mcp::JsonText> call(const httpd::mcp::Caller &c, const std::string &name,
		                                           const httpd::mcp::JsonText &args)
		{
			{
				OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock_);
				++calls_;
			}
			if (name == "echo")
				return coreapi::ok(args);
			if (name == "whoami")
			{
				std::string out;
				httpd::Json j(out);
				j.beginObject();
				j.key("client");
				j.value(c.client_id);
				j.key("external");
				j.value(c.external);
				j.key("level");
				j.value((int) c.level);
				j.endObject();
				return coreapi::ok(out);
			}
			if (name == "numbers")
				return coreapi::ok(std::string("[1,2,3]"));
			if (name == "latin1")
				return coreapi::ok(std::string("{\"name\":\"\xe4rger\"}"));
			if (name == "controlchars")
			{
				// A raw control byte and an embedded zero inside a string value, as a
				// tool composing JSON by hand off a broadcast channel name might send.
				std::string raw = "{\"msg\":\"a";
				raw += '\n';
				raw += 'b';
				raw += '\0';
				raw += "c\"}";
				return coreapi::ok(raw);
			}
			if (name == "deep")
				return coreapi::ok(deeplyNested(40));
			if (name == "standby")
				return coreapi::fail<httpd::mcp::JsonText>(coreapi::Status::Conflict,
				                                           coreapi::ErrorCode::BoxInStandby,
				                                           "the box is in standby");
			if (name == "slow")
			{
				usleep(slowMs() * 1000);
				return coreapi::ok(std::string("{\"done\":true}"));
			}
			if (name == "throws")
				throw std::runtime_error("the fake tool failed");
			return coreapi::ok(std::string("{\"ok\":true}"));
		}

		std::string hint(const std::string &name, coreapi::ErrorCode code) const
		{
			if (name == "standby" && code == coreapi::ErrorCode::BoxInStandby)
				return "Ask the user whether to switch the box on, then call again with wake set to true.";
			return std::string();
		}

		int calls()
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock_);
			return calls_;
		}

	private:
		OpenThreads::Mutex lock_;
		int                calls_;
};

// One for the whole run: a worker the endpoint gave up on may still be inside it.
inline Tools &tools()
{
	static Tools t;
	return t;
}

// A case waits for the workers it abandoned, so none outlives it.
inline bool waitForCalls()
{
	for (int i = 0; i < 300 && httpd::mcp::runningCalls() > 0; ++i)
		usleep(10000);
	return httpd::mcp::runningCalls() == 0;
}

struct State
{
	std::string token_resource;   // what the fake tunnel tokens are bound to

	State() : token_resource("https://tv.example.org/mcp") {}
};

inline State &state()
{
	static State s;
	return s;
}

inline httpd::Origin &seenOrigin()
{
	static httpd::Origin o = httpd::Origin::Refused;
	return o;
}

inline std::string &seenResource()
{
	static std::string s;
	return s;
}

inline coreapi::Result<httpd::mcp::Caller> verify(const std::string &bearer, httpd::Origin origin,
                                                  const std::string &resource)
{
	seenOrigin() = origin;
	seenResource() = resource;
	if (bearer == "tok-broken")
		return coreapi::fail<httpd::mcp::Caller>(coreapi::Status::Internal, coreapi::ErrorCode::BoxUnreadable,
		                                         "the token store could not be read");
	httpd::mcp::Caller c;
	if (bearer == "tok-read" || bearer == "tok-other")
		c.level = httpd::AuthLevel::Read;
	else if (bearer == "tok-write")
		c.level = httpd::AuthLevel::Write;
	else if (bearer == "tok-system")
		c.level = httpd::AuthLevel::System;
	else
		return coreapi::fail<httpd::mcp::Caller>(coreapi::Status::Denied, coreapi::ErrorCode::NotPermitted,
		                                         "the token is not valid here");
	if (origin == httpd::Origin::Refused ||
	    (origin == httpd::Origin::Tunnel && resource != state().token_resource))
		return coreapi::fail<httpd::mcp::Caller>(coreapi::Status::Denied, coreapi::ErrorCode::NotPermitted,
		                                         "the token is not valid here");
	c.client_id = (bearer == "tok-other") ? "client-other" : "client-main";
	c.user = "root";
	c.external = (origin == httpd::Origin::Tunnel);
	c.groups = ~0u;
	return coreapi::ok(c);
}

// Wires the endpoint for one case and puts every shared setting back after it.
struct Wired
{
	Wired()
	{
		state() = State();
		seenOrigin() = httpd::Origin::Refused;
		seenResource().clear();
		httpd::mcp::setLimits(httpd::mcp::defaultLimits());
		httpd::mcp::forgetRatesForTest();
		httpd::mcp::setRateClockForTest(0);
		const httpd::mcp::Wiring w = { &tools(), &verify };
		httpd::mcp::install(w);
	}

	~Wired()
	{
		httpd::mcp::uninstall();
		waitForCalls();
		slowMs() = 0;
		httpd::mcp::setLimits(httpd::mcp::defaultLimits());
		httpd::mcp::forgetRatesForTest();
		httpd::mcp::setRateClockForTest(0);
	}

private:
	Wired(const Wired &);
	Wired &operator=(const Wired &);
};

inline httpd::mcp::Head modernHead(const std::string &method, const std::string &name = std::string(),
                                   const std::string &token = "tok-read")
{
	httpd::mcp::Head h;
	h.method = httpd::Post;
	h.origin = httpd::Origin::Lan;
	h.base = "http://box.test";
	h.content_type = "application/json";
	h.authorization = "Bearer " + token;
	h.authorization_count = 1;
	h.protocol_version = "2026-07-28";
	h.protocol_version_count = 1;
	h.mcp_method = method;
	h.mcp_method_count = 1;
	if (!name.empty())
	{
		h.mcp_name = name;
		h.mcp_name_count = 1;
	}
	return h;
}

inline httpd::mcp::Head tunnelHead(const std::string &method, const std::string &name = std::string(),
                                   const std::string &token = "tok-read")
{
	httpd::mcp::Head h = modernHead(method, name, token);
	h.origin = httpd::Origin::Tunnel;
	h.base = "https://tv.example.org";
	return h;
}

// Null when the text is not JSON, which every case reading a member then fails on.
inline const httpd::mcp::JsonValue parsed(const std::string &text)
{
	httpd::mcp::JsonValue v;
	if (!httpd::mcp::parseJson(text, 64, v))
		return httpd::mcp::JsonValue();
	return v;
}

inline int errorCode(const std::string &body)
{
	const httpd::mcp::JsonValue doc = parsed(body);
	if (!doc.isObject() || !doc["error"].isObject())
		return 0;
	const httpd::mcp::JsonValue &code = doc["error"]["code"];
	return code.isInt() ? code.asInt() : 0;
}

inline int errorCode(const httpd::Response &r)
{
	return errorCode(r.body);
}

inline std::string headerOf(const httpd::Response &r, const char *name)
{
	for (size_t i = 0; i < r.headers.size(); ++i)
	{
		if (strcasecmp(r.headers[i].first.c_str(), name) == 0)
			return r.headers[i].second;
	}
	return std::string();
}

inline httpd::mcp::Head legacyHead(const std::string &token = "tok-read")
{
	httpd::mcp::Head h = modernHead("", "", token);
	h.protocol_version = "2025-11-25";
	h.mcp_method.clear();
	h.mcp_method_count = 0;
	return h;
}

// The older handshake, before any version is agreed.
inline httpd::mcp::Head bareHead(const std::string &token = "tok-read")
{
	httpd::mcp::Head h = legacyHead(token);
	h.protocol_version.clear();
	h.protocol_version_count = 0;
	return h;
}

inline std::string modernBody(const std::string &id_json, const std::string &method,
                              const std::string &params = std::string())
{
	std::string b = "{\"jsonrpc\":\"2.0\",\"id\":" + id_json + ",\"method\":\"" + method + "\",\"params\":{";
	if (!params.empty())
		b += params + ",";
	b += "\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\","
	     "\"io.modelcontextprotocol/clientCapabilities\":{}}}}";
	return b;
}

inline std::string legacyBody(const std::string &id_json, const std::string &method,
                              const std::string &params = std::string())
{
	return "{\"jsonrpc\":\"2.0\",\"id\":" + id_json + ",\"method\":\"" + method + "\",\"params\":{" + params + "}}";
}

inline httpd::Response roundTrip(const httpd::mcp::Head &h, const std::string &body)
{
	const httpd::mcp::Admission a = httpd::mcp::admit(h);
	if (!a.admitted)
		return a.refusal;
	return httpd::mcp::answer(h, a, body);
}

} // namespace mcpfake

#endif
