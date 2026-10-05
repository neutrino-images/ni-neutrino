/*
 * jsonrpc.cpp - JSON-RPC 2.0 messages of the MCP endpoint
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

#include "httpd/mcp/jsonrpc.h"

#include "httpd/json.h"

#include <memory>
#include <string>

namespace httpd
{
namespace mcp
{

namespace
{

bool validId(const JsonValue &id)
{
	return id.type() == ::Json::stringValue || id.type() == ::Json::intValue ||
	       id.type() == ::Json::uintValue;
}

bool nestsWithin(const JsonValue &v, size_t levels)
{
	if (!v.isArray() && !v.isObject())
		return true;
	if (levels == 0)
		return false;
	for (JsonValue::const_iterator it = v.begin(); it != v.end(); ++it)
	{
		if (!nestsWithin(*it, levels - 1))
			return false;
	}
	return true;
}

void writeValue(httpd::Json &j, const JsonValue &v)
{
	switch (v.type())
	{
		case ::Json::nullValue:
			j.null();
			return;
		case ::Json::intValue:
			j.value((long long) v.asInt64());
			return;
		case ::Json::uintValue:
			j.value((unsigned long long) v.asUInt64());
			return;
		case ::Json::realValue:
			j.value(v.asDouble());
			return;
		case ::Json::stringValue:
		{
			const char *begin = NULL;
			const char *end = NULL;
			v.getString(&begin, &end);
			j.value(std::string(begin, end));
			return;
		}
		case ::Json::booleanValue:
			j.value(v.asBool());
			return;
		case ::Json::arrayValue:
			j.beginArray();
			for (JsonValue::const_iterator it = v.begin(); it != v.end(); ++it)
				writeValue(j, *it);
			j.endArray();
			return;
		case ::Json::objectValue:
			j.beginObject();
			for (JsonValue::const_iterator it = v.begin(); it != v.end(); ++it)
			{
				j.key(it.name().c_str());
				writeValue(j, *it);
			}
			j.endObject();
			return;
	}
}

} // namespace

bool parseJson(const std::string &text, size_t max_depth, JsonValue &out)
{
	if (!httpd::isUtf8(text.data(), text.size()))
		return false;

	::Json::CharReaderBuilder builder;
	::Json::CharReaderBuilder::strictMode(&builder.settings_);
	builder.settings_["stackLimit"] = (::Json::UInt) max_depth;
	try
	{
		std::unique_ptr< ::Json::CharReader> reader(builder.newCharReader());
		std::string errors;
		return reader->parse(text.data(), text.data() + text.size(), &out, &errors);
	}
	catch (const ::Json::Exception &)
	{
		// Thrown for nesting past the limit.
		return false;
	}
}

ReadOutcome readMessage(const std::string &body, size_t max_depth, Message &out)
{
	out = Message();
	JsonValue root;
	if (!parseJson(body, max_depth, root))
		return ReadOutcome::ParseError;
	if (!root.isObject())
		return ReadOutcome::InvalidRequest;

	const JsonValue &doc = root;
	const bool has_id = doc.isMember("id");
	if (has_id && validId(doc["id"]))
		out.id = doc["id"];

	const JsonValue &version = doc["jsonrpc"];
	if (!version.isString() || version.asString() != "2.0")
		return ReadOutcome::InvalidRequest;
	if (has_id && !validId(doc["id"]))
		return ReadOutcome::InvalidRequest;

	if (doc.isMember("params"))
	{
		if (!doc["params"].isObject())
			return ReadOutcome::InvalidRequest;
		out.params = doc["params"];
	}

	if (!doc.isMember("method"))
	{
		if (has_id && (doc.isMember("result") || doc.isMember("error")))
		{
			out.kind = MessageKind::Response;
			return ReadOutcome::Ok;
		}
		return ReadOutcome::InvalidRequest;
	}

	const JsonValue &method = doc["method"];
	if (!method.isString() || method.asString().empty())
		return ReadOutcome::InvalidRequest;
	out.method = method.asString();
	out.kind = has_id ? MessageKind::Request : MessageKind::Notification;
	return ReadOutcome::Ok;
}

std::string resultResponse(const JsonValue &id, const std::string &result_json)
{
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.key("jsonrpc");
	j.value("2.0");
	j.key("id");
	writeValue(j, id);
	j.key("result");
	j.raw(result_json.c_str());
	j.endObject();
	return out;
}

std::string errorResponse(const JsonValue &id, int code, const std::string &message,
                          const std::string &data_json)
{
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.key("jsonrpc");
	j.value("2.0");
	if (!id.isNull())
	{
		j.key("id");
		writeValue(j, id);
	}
	j.key("error");
	j.beginObject();
	j.key("code");
	j.value(code);
	j.key("message");
	j.value(message);
	if (!data_json.empty())
	{
		j.key("data");
		j.raw(data_json.c_str());
	}
	j.endObject();
	j.endObject();
	return out;
}

bool toJson(const JsonValue &v, std::string &out)
{
	out.clear();
	if (!nestsWithin(v, httpd::Json::MaxDepth))
		return false;
	httpd::Json j(out);
	writeValue(j, v);
	return true;
}

} // namespace mcp
} // namespace httpd
