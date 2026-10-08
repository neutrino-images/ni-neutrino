/*
 * shape.cpp - an answer held to the shape its route declares
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

#include "support/catch.hpp"
#include "support/shape.h"

#include <cctype>

using namespace httpd;

// Hexadecimal, no prefix, never empty.
bool isHexIdentifier(const std::string &s)
{
	if (s.empty() || s.size() > 16)
		return false;
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (std::isxdigit((unsigned char) s[i]) == 0)
			return false;
	}
	return true;
}

// Read off the parser's own kind, not off the bytes; a whole number may stand for a Number.
bool typeMatches(const ::Json::Value &v, FieldType t)
{
	const ::Json::ValueType got = v.type();
	const bool whole = (got == ::Json::intValue || got == ::Json::uintValue);

	switch (t)
	{
		case FieldType::Bool:   return got == ::Json::booleanValue;
		case FieldType::Int:    return whole;
		// The one that says what the others do not: a row declaring no sign is
		// answered by a number that has none.
		case FieldType::UInt:   return whole && v.asInt64() >= 0;
		case FieldType::Number: return whole || got == ::Json::realValue;
		case FieldType::String: return got == ::Json::stringValue;
		case FieldType::Time:   return whole;
		/* Text, and the text the pattern in the document states: this is the
		   one kind whose shape a reader is told outright, so a member of it
		   answering anything else is a document the server does not hold to. */
		case FieldType::ChannelId: return got == ::Json::stringValue && isHexIdentifier(v.asString());
		case FieldType::Object: return got == ::Json::objectValue;
		case FieldType::NamedLists: return got == ::Json::objectValue;
		case FieldType::Array:  return got == ::Json::arrayValue;
	}
	// A value cast into the enum from outside it, which no table here writes.
	return false;
}

const char *typeName(FieldType t)
{
	switch (t)
	{
		case FieldType::Bool:   return "bool";
		case FieldType::Int:    return "int";
		case FieldType::UInt:   return "uint";
		case FieldType::Number: return "number";
		case FieldType::String: return "string";
		case FieldType::Time:   return "time";
		case FieldType::ChannelId: return "channel id";
		case FieldType::Object: return "object";
		case FieldType::NamedLists: return "named lists";
		case FieldType::Array:  return "array";
	}
	return "?";
}

// Every required member present, no undeclared member, every value of its declared kind.
void checkShape(const ::Json::Value &v, const Schema &s, const std::string &where)
{
	INFO(where << " against " << s.name);
	REQUIRE(v.isObject());

	for (size_t i = 0; i < s.count; ++i)
	{
		const FieldDesc &f = s.fields[i];
		INFO("member " << f.name);
		if (!f.optional)
			REQUIRE(v.isMember(f.name));
		if (!v.isMember(f.name))
			continue;

		const ::Json::Value &member = v[f.name];
		INFO("declared " << typeName(f.type) << ", arrived as kind " << (int) member.type());
		REQUIRE(typeMatches(member, f.type));

		if (f.type == FieldType::Object)
		{
			REQUIRE(f.nested != NULL);
			checkShape(member, *f.nested, where + "." + f.name);
			continue;
		}
		if (f.type == FieldType::NamedLists)
		{
			REQUIRE(f.nested != NULL);
			const ::Json::Value::Members listed = member.getMemberNames();
			for (size_t n = 0; n < listed.size(); ++n)
			{
				INFO("list " << listed[n]);
				REQUIRE(member[listed[n]].isArray());
				for (::Json::ArrayIndex e = 0; e < member[listed[n]].size(); ++e)
					checkShape(member[listed[n]][e], *f.nested, where + "." + f.name + "." + listed[n]);
			}
			continue;
		}
		if (f.type == FieldType::Array)
		{
			if (f.nested == NULL)
				continue;
			for (::Json::ArrayIndex e = 0; e < member.size(); ++e)
				checkShape(member[e], *f.nested, where + "." + f.name);
		}
	}

	const ::Json::Value::Members names = v.getMemberNames();
	for (size_t i = 0; i < names.size(); ++i)
	{
		bool declared = false;
		for (size_t j = 0; j < s.count && !declared; ++j)
			declared = names[i] == s.fields[j].name;
		INFO("member " << names[i]);
		REQUIRE(declared);
	}
}
