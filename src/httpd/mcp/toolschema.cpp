/*
 * toolschema.cpp - JSON Schema for the arguments and answers of a tool
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

#include "httpd/mcp/toolschema.h"

#include "httpd/json.h"

#include <cstring>
#include <string>
#include <vector>

namespace httpd
{
namespace mcp
{

namespace
{

// The OpenAPI document's pattern.
const char kChannelIdPattern[] = "^(0[xX])?[0-9a-fA-F]{1,16}$";

// Stops two shapes that name each other.
const size_t kMaxDepth = 8;

// Not a shape row: a row stating a set needs the code that answers it beside it.
const char kStatusWords[] = "done,accepted";
const char kStatusDoc[] = "what became of the request";
const char kStatusDocs[] =
	"done: the box carried the request out\n"
	"accepted: the request was handed on to be carried out";

void appendEnum(Json &j, const char *values)
{
	j.key("enum");
	j.beginArray();
	for (const char *q = values; q != NULL && *q != '\0';)
	{
		const char *comma = std::strchr(q, ',');
		const size_t n = (comma != NULL) ? (size_t) (comma - q) : std::strlen(q);
		j.value(std::string(q, n));
		if (comma == NULL)
			break;
		q = comma + 1;
	}
	j.endArray();
}

void appendAskedEnum(Json &j, void (*asks)(std::vector<std::string> &))
{
	std::vector<std::string> asked;
	asks(asked);
	if (asked.empty())
		return;
	j.key("enum");
	j.beginArray();
	for (size_t i = 0; i < asked.size(); ++i)
		j.value(asked[i]);
	j.endArray();
}

// The text of value_docs for one value, or empty.
std::string valueDoc(const char *value_docs, const std::string &value)
{
	for (const char *q = value_docs; q != NULL && *q != '\0';)
	{
		const char *nl = std::strchr(q, '\n');
		const size_t n = (nl != NULL) ? (size_t) (nl - q) : std::strlen(q);
		if (n > value.size() + 2 && value.compare(0, value.size(), q, value.size()) == 0 &&
		    q[value.size()] == ':' && q[value.size() + 1] == ' ')
			return std::string(q + value.size() + 2, n - value.size() - 2);
		if (nl == NULL)
			break;
		q = nl + 1;
	}
	return std::string();
}

// The OpenAPI document's wording: the doc, then one line per explained value.
std::string describedSet(const char *doc, const char *values, const char *value_docs)
{
	std::string out = doc;
	bool listed = false;
	for (const char *q = values; q != NULL && *q != '\0';)
	{
		const char *comma = std::strchr(q, ',');
		const std::string v(q, (comma != NULL) ? (size_t) (comma - q) : std::strlen(q));
		const std::string text = valueDoc(value_docs, v);
		if (!text.empty())
		{
			out += listed ? "\n- `" : "\n\n- `";
			out += v + "`: " + text;
			listed = true;
		}
		if (comma == NULL)
			break;
		q = comma + 1;
	}
	return out;
}

void appendArgType(Json &j, const Param &p)
{
	const bool bounded = (p.min != 0 || p.max != 0);

	switch (p.type)
	{
		case ParamType::Int:
			j.key("type");
			j.value("integer");
			if (bounded)
			{
				j.key("minimum");
				j.value(p.min);
				j.key("maximum");
				j.value(p.max);
			}
			return;
		case ParamType::UInt:
			j.key("type");
			j.value("integer");
			j.key("minimum");
			j.value((bounded && p.min > 0) ? p.min : 0L);
			if (bounded)
			{
				j.key("maximum");
				j.value(p.max);
			}
			return;
		case ParamType::Bool:
			j.key("type");
			j.value("boolean");
			return;
		case ParamType::String:
			j.key("type");
			j.value("string");
			if (p.choices != NULL)
				appendAskedEnum(j, p.choices);
			if (p.max > 0)
			{
				j.key("maxLength");
				j.value(p.max);
				j.key("x-max-bytes");
				j.value(p.max);
			}
			return;
		case ParamType::Enum:
			j.key("type");
			j.value("string");
			appendEnum(j, p.values);
			return;
		case ParamType::ChannelId:
			j.key("type");
			j.value("string");
			j.key("pattern");
			j.value(kChannelIdPattern);
			return;
		case ParamType::Time:
			j.key("type");
			j.value("integer");
			j.key("format");
			j.value("unix-time");
			if (bounded)
			{
				j.key("minimum");
				j.value(p.min);
				j.key("maximum");
				j.value(p.max);
			}
			return;
	}
}

void appendWholeBody(Json &j, const Param &p)
{
	if (p.in == In::BodyList)
	{
		j.key("type");
		j.value("array");
		j.key("items");
		j.beginObject();
		Param item = p;
		item.min = 0;
		item.max = 0;
		appendArgType(j, item);
		j.endObject();
		j.key("minItems");
		j.value(p.min);
		j.key("maxItems");
		j.value(p.max);
	}
	else
	{
		j.key("type");
		j.value("object");
		j.key("additionalProperties");
		j.beginObject();
		j.key("type");
		j.value("string");
		j.endObject();
		j.key("minProperties");
		j.value(p.min);
		j.key("maxProperties");
		j.value(p.max);
	}
	if (p.doc != NULL && p.doc[0] != '\0')
	{
		j.key("description");
		j.value(describedSet(p.doc, p.values, p.value_docs));
	}
}

void appendElement(Json &j, ElementType e)
{
	switch (e)
	{
		case ElementType::None:
			return;
		case ElementType::Bool:
			j.key("type");
			j.value("boolean");
			return;
		case ElementType::Int:
			j.key("type");
			j.value("integer");
			return;
		case ElementType::UInt:
			j.key("type");
			j.value("integer");
			j.key("minimum");
			j.value(0L);
			return;
		case ElementType::Number:
			j.key("type");
			j.value("number");
			return;
		case ElementType::String:
			j.key("type");
			j.value("string");
			return;
		case ElementType::Time:
			j.key("type");
			j.value("integer");
			j.key("format");
			j.value("unix-time");
			return;
		case ElementType::ChannelId:
			j.key("type");
			j.value("string");
			j.key("pattern");
			j.value(kChannelIdPattern);
			return;
	}
}

void appendShapeBody(Json &j, const Schema &s, size_t depth);

void appendMember(Json &j, const FieldDesc &f, size_t depth)
{
	j.key(f.name);
	j.beginObject();
	switch (f.type)
	{
		case FieldType::Bool:
			j.key("type");
			j.value("boolean");
			break;
		case FieldType::Int:
			j.key("type");
			j.value("integer");
			break;
		case FieldType::UInt:
			j.key("type");
			j.value("integer");
			j.key("minimum");
			j.value(0L);
			break;
		case FieldType::Number:
			j.key("type");
			j.value("number");
			break;
		case FieldType::String:
			j.key("type");
			j.value("string");
			if (f.values != NULL)
				appendEnum(j, f.values);
			else if (f.asks != NULL)
				appendAskedEnum(j, f.asks);
			break;
		case FieldType::Time:
			j.key("type");
			j.value("integer");
			j.key("format");
			j.value("unix-time");
			break;
		case FieldType::ChannelId:
			j.key("type");
			j.value("string");
			j.key("pattern");
			j.value(kChannelIdPattern);
			break;
		case FieldType::Object:
			if (f.nested != NULL)
				appendShapeBody(j, *f.nested, depth + 1);
			else
			{
				j.key("type");
				j.value("object");
			}
			break;
		case FieldType::NamedLists:
			j.key("type");
			j.value("object");
			if (f.nested != NULL)
			{
				j.key("additionalProperties");
				j.beginObject();
				j.key("type");
				j.value("array");
				j.key("items");
				j.beginObject();
				appendShapeBody(j, *f.nested, depth + 1);
				j.endObject();
				j.endObject();
			}
			break;
		case FieldType::Array:
			j.key("type");
			j.value("array");
			j.key("items");
			j.beginObject();
			if (f.nested != NULL)
				appendShapeBody(j, *f.nested, depth + 1);
			else
				appendElement(j, f.element);
			j.endObject();
			break;
	}
	if (f.doc != NULL && f.doc[0] != '\0')
	{
		j.key("description");
		j.value(describedSet(f.doc, f.values, f.value_docs));
	}
	j.endObject();
}

void appendShapeBody(Json &j, const Schema &s, size_t depth)
{
	j.key("type");
	j.value("object");
	if (depth > kMaxDepth)
		return;

	j.key("properties");
	j.beginObject();
	for (size_t i = 0; i < s.count && s.fields != NULL; ++i)
	{
		if (s.fields[i].name != NULL && s.fields[i].name[0] != '\0')
			appendMember(j, s.fields[i], depth);
	}
	j.endObject();

	size_t required = 0;
	for (size_t i = 0; i < s.count && s.fields != NULL; ++i)
		required += (!s.fields[i].optional && s.fields[i].name != NULL) ? 1 : 0;
	if (required > 0)
	{
		j.key("required");
		j.beginArray();
		for (size_t i = 0; i < s.count && s.fields != NULL; ++i)
		{
			if (!s.fields[i].optional && s.fields[i].name != NULL)
				j.value(s.fields[i].name);
		}
		j.endArray();
	}
	j.key("additionalProperties");
	j.value(false);
}

} // namespace

void appendInputSchema(std::string &out, const Endpoint &ep)
{
	Json j(out);
	j.beginObject();
	j.key("type");
	j.value("object");

	j.key("properties");
	j.beginObject();
	size_t required = 0;
	for (size_t i = 0; i < ep.param_count && ep.params != NULL; ++i)
	{
		const Param &p = ep.params[i];
		if (p.name == NULL || p.name[0] == '\0' || p.in == In::BodyBytes)
			continue;
		j.key(p.name);
		j.beginObject();
		if (p.in == In::BodyList || p.in == In::BodyMap)
		{
			appendWholeBody(j, p);
			++required;
		}
		else
		{
			required += (p.required || p.in == In::Path) ? 1 : 0;
			appendArgType(j, p);
			if (p.doc != NULL && p.doc[0] != '\0')
			{
				j.key("description");
				j.value(describedSet(p.doc, p.values, p.value_docs));
			}
		}
		j.endObject();
	}
	j.endObject();

	if (required > 0)
	{
		j.key("required");
		j.beginArray();
		for (size_t i = 0; i < ep.param_count && ep.params != NULL; ++i)
		{
			const Param &p = ep.params[i];
			if (p.name == NULL || p.name[0] == '\0' || p.in == In::BodyBytes)
				continue;
			if (p.in == In::BodyList || p.in == In::BodyMap)
			{
				j.value(p.name);
				continue;
			}
			if (p.required || p.in == In::Path)
				j.value(p.name);
		}
		j.endArray();
	}

	j.key("additionalProperties");
	j.value(false);
	j.endObject();
}

void appendOutputSchema(std::string &out, const Schema &s)
{
	Json j(out);
	j.beginObject();
	appendShapeBody(j, s, 0);
	j.endObject();
}

void appendDoneSchema(std::string &out)
{
	appendDoneSchema(out, Answers202 | Answers204);
}

void appendDoneSchema(std::string &out, unsigned answers)
{
	const bool accepted = (answers & Answers202) != 0;
	const bool done = (answers & (Answers200 | Answers201 | Answers204)) != 0;
	const char *words = kStatusWords;
	if (accepted && !done)
		words = "accepted";
	else if (done && !accepted)
		words = "done";

	Json j(out);
	j.beginObject();
	j.key("type");
	j.value("object");
	j.key("properties");
	j.beginObject();
	j.key("status");
	j.beginObject();
	j.key("type");
	j.value("string");
	appendEnum(j, words);
	j.key("description");
	j.value(describedSet(kStatusDoc, words, kStatusDocs));
	j.endObject();
	j.endObject();
	j.key("required");
	j.beginArray();
	j.value("status");
	j.endArray();
	j.key("additionalProperties");
	j.value(false);
	j.endObject();
}

} // namespace mcp
} // namespace httpd
