/*
 * test_mcp_schema.cpp - the schemas a tool is described with
 *
 * Copyright (C) 2026 NI-Team
 *
 * License: GPL-2.0-or-later, see COPYING.
 */

#include "support/catch.hpp"

#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/router.h"
#include "httpd/schema.h"
#include "httpd/doc/openapi.h"
#include "httpd/mcp/toolschema.h"

#include "jsoncpp/json/json.h"

#include <string>
#include <vector>

using namespace httpd;

namespace
{

::Json::Value parsed(const std::string &text)
{
	::Json::Value v;
	::Json::Reader reader;
	const bool ok = reader.parse(text, v);
	INFO(text);
	REQUIRE(ok);
	return v;
}

Response nothing(const Request &)
{
	return noContent();
}

void twoChoices(std::vector<std::string> &out)
{
	out.push_back("first");
	out.push_back("second");
}

const Param kEveryKind[] = {
	// A segment is required whatever its row says.
	HTTPD_PARAM_AS_WRITTEN("id", ParamType::ChannelId, In::Path, false, "the channel", 0, 0, NULL),
	HTTPD_QUERY_IN("n", ParamType::Int, "a count", -3, 10),
	HTTPD_QUERY_IN("u", ParamType::UInt, "an amount", 2, 9),
	HTTPD_QUERY("u0", ParamType::UInt, "an unbounded amount"),
	HTTPD_QUERY_FROM_SET("mode", "the list", "tv,radio", "tv: the television channels"),
	HTTPD_BODY_REQUIRED("at", ParamType::Time, "a moment"),
	HTTPD_BODY("on", ParamType::Bool, "a switch"),
	HTTPD_BODY_TEXT("words", "some words", 40),
	HTTPD_SEGMENT_FROM_ASKED_SET("pick", "one of the asked", &twoChoices),
};

const Param kWholeBody[] = {
	HTTPD_SEGMENT_TEXT("bouquet", "the bouquet", 20),
	HTTPD_BODY_IS_LIST_OF("channels", ParamType::ChannelId, "the members", 0, 10),
};

const Endpoint kProbe = { Method::Put, "/api/v1/s/{id}/{pick}", AuthLevel::Write, "probe", NULL,
	HTTPD_PARAMS(kEveryKind), NULL, &nothing, false, Answers204, HTTPD_NO_REFUSALS };
const Endpoint kFill = { Method::Put, "/api/v1/s/{bouquet}", AuthLevel::Write, "fill", NULL,
	HTTPD_PARAMS(kWholeBody), NULL, &nothing, false, Answers204, HTTPD_NO_REFUSALS };

const FieldDesc kLeafFields[] = {
	HTTPD_MEMBER("t", FieldType::Time, "when"),
	HTTPD_MEMBER_OF_SET("k", "a,b", "which", "a: the first\nb: the second"),
};
const Schema kLeaf = { "leaf", HTTPD_FIELDS(kLeafFields) };

const FieldDesc kRootFields[] = {
	HTTPD_MEMBER("id", FieldType::ChannelId, "whose"),
	HTTPD_MEMBER_OPTIONAL("size", FieldType::UInt, "how big"),
	HTTPD_OBJECT("one", &kLeaf, "a leaf"),
	HTTPD_LIST_OF("many", &kLeaf, "leaves"),
	HTTPD_LIST_OF_VALUES("ns", ElementType::Int, "numbers"),
	HTTPD_NAMED_LISTS_OPTIONAL("by_name", &kLeaf, "leaves under names the answer chooses"),
};
const Schema kRoot = { "root", HTTPD_FIELDS(kRootFields) };

const FieldDesc kDoneFields[] = {
	HTTPD_MEMBER_OF_SET("status", "done,accepted", "what became of the request",
		"done: the box carried the request out\n"
		"accepted: the request was handed on to be carried out"),
};
const Schema kDone = { "done", HTTPD_FIELDS(kDoneFields) };

const char *methodKey(Method m)
{
	switch (m)
	{
		case Get:    return "get";
		case Post:   return "post";
		case Put:    return "put";
		case Patch:  return "patch";
		case Delete: return "delete";
		default:     return "";
	}
}

bool hasRef(const ::Json::Value &v)
{
	if (v.isObject())
	{
		if (v.isMember("$ref"))
			return true;
		const ::Json::Value::Members names = v.getMemberNames();
		for (size_t i = 0; i < names.size(); ++i)
			if (hasRef(v[names[i]]))
				return true;
	}
	if (v.isArray())
		for (::Json::ArrayIndex i = 0; i < v.size(); ++i)
			if (hasRef(v[i]))
				return true;
	return false;
}

const char kSchemaPrefix[] = "#/components/schemas/";

// The document's schema with every reference replaced by what it names.
::Json::Value inlined(const ::Json::Value &v, const ::Json::Value &schemas, size_t depth)
{
	REQUIRE(depth < 32);
	if (v.isArray())
	{
		::Json::Value out(::Json::arrayValue);
		for (::Json::ArrayIndex i = 0; i < v.size(); ++i)
			out.append(inlined(v[i], schemas, depth + 1));
		return out;
	}
	if (!v.isObject())
		return v;

	::Json::Value out(::Json::objectValue);
	if (v.isMember("$ref"))
	{
		const std::string ref = v["$ref"].asString();
		REQUIRE(ref.compare(0, sizeof(kSchemaPrefix) - 1, kSchemaPrefix) == 0);
		const ::Json::Value &target = schemas[ref.substr(sizeof(kSchemaPrefix) - 1)];
		REQUIRE(target.isObject());
		out = inlined(target, schemas, depth + 1);
	}
	const ::Json::Value::Members names = v.getMemberNames();
	for (size_t i = 0; i < names.size(); ++i)
	{
		// The same words are in the description.
		if (names[i] == "$ref" || names[i] == "x-enum-descriptions")
			continue;
		out[names[i]] = inlined(v[names[i]], schemas, depth + 1);
	}
	return out;
}

} // namespace

TEST_CASE("every argument kind is described the way the router reads it", "[mcp-schema]")
{
	std::string text;
	mcp::appendInputSchema(text, kProbe);
	const ::Json::Value s = parsed(text);

	REQUIRE(s["type"].asString() == "object");
	REQUIRE(s["additionalProperties"].asBool() == false);
	const ::Json::Value &p = s["properties"];
	REQUIRE(p.size() == 9);

	REQUIRE(p["id"]["type"].asString() == "string");
	REQUIRE(p["id"]["pattern"].asString() == "^(0[xX])?[0-9a-fA-F]{1,16}$");
	REQUIRE(p["id"]["description"].asString() == "the channel");
	REQUIRE(p["n"]["type"].asString() == "integer");
	REQUIRE(p["n"]["minimum"].asInt() == -3);
	REQUIRE(p["n"]["maximum"].asInt() == 10);
	REQUIRE(p["u"]["minimum"].asInt() == 2);
	REQUIRE(p["u"]["maximum"].asInt() == 9);
	REQUIRE(p["u0"].isMember("minimum"));
	REQUIRE(p["u0"]["minimum"].asInt() == 0);
	REQUIRE_FALSE(p["u0"].isMember("maximum"));
	REQUIRE(p["mode"]["enum"].size() == 2);
	REQUIRE(p["mode"]["enum"][1].asString() == "radio");
	REQUIRE(p["mode"]["description"].asString() == "the list\n\n- `tv`: the television channels");
	REQUIRE_FALSE(p["mode"].isMember("x-enum-descriptions"));
	REQUIRE(p["at"]["type"].asString() == "integer");
	REQUIRE(p["at"]["format"].asString() == "unix-time");
	REQUIRE(p["on"]["type"].asString() == "boolean");
	REQUIRE(p["words"]["maxLength"].asInt() == 40);
	// A set the box is asked for at run time is not stated: a client keeps the list for long.
	REQUIRE(p["pick"]["type"].asString() == "string");
	REQUIRE_FALSE(p["pick"].isMember("enum"));

	REQUIRE(s["required"].size() == 3);
	REQUIRE(s["required"][0].asString() == "id");
	REQUIRE(s["required"][1].asString() == "at");
	REQUIRE(s["required"][2].asString() == "pick");
}

TEST_CASE("a row that is the whole body is described as the array it is", "[mcp-schema]")
{
	std::string text;
	mcp::appendInputSchema(text, kFill);
	const ::Json::Value s = parsed(text);
	REQUIRE(s["properties"].isMember("bouquet"));
	REQUIRE(s["properties"].isMember("channels"));
	REQUIRE(s["properties"]["channels"]["type"].asString() == "array");
	REQUIRE(s["properties"]["channels"]["minItems"].asInt() == 0);
	REQUIRE(s["properties"]["channels"]["maxItems"].asInt() == 10);
}

TEST_CASE("an answer is described inline with its nested shapes", "[mcp-schema]")
{
	std::string text;
	mcp::appendOutputSchema(text, kRoot);
	const ::Json::Value s = parsed(text);

	REQUIRE_FALSE(hasRef(s));
	REQUIRE(s["type"].asString() == "object");
	REQUIRE(s["additionalProperties"].asBool() == false);
	REQUIRE(s["required"].size() == 4);
	for (::Json::ArrayIndex i = 0; i < s["required"].size(); ++i)
		REQUIRE(s["required"][i].asString() != "size");
	const ::Json::Value &p = s["properties"];
	REQUIRE(p["one"]["type"].asString() == "object");
	REQUIRE(p["one"]["properties"]["k"]["enum"][0].asString() == "a");
	REQUIRE(p["one"]["properties"]["k"]["description"].asString() == "which\n\n- `a`: the first\n- `b`: the second");
	REQUIRE(p["one"]["additionalProperties"].asBool() == false);
	REQUIRE(p["many"]["type"].asString() == "array");
	REQUIRE(p["many"]["items"]["properties"]["t"]["format"].asString() == "unix-time");
	REQUIRE(p["ns"]["items"]["type"].asString() == "integer");
	REQUIRE(p["one"]["description"].asString() == "a leaf");
	REQUIRE(p["by_name"]["type"].asString() == "object");
	REQUIRE(p["by_name"]["additionalProperties"]["type"].asString() == "array");
	REQUIRE(p["by_name"]["additionalProperties"]["items"]["properties"]["t"]["format"].asString() == "unix-time");
}

TEST_CASE("the answer of a route with no document is one of two words", "[mcp-schema]")
{
	std::string text;
	mcp::appendDoneSchema(text);
	const ::Json::Value s = parsed(text);
	REQUIRE(s["properties"]["status"]["enum"].size() == 2);
	REQUIRE(s["properties"]["status"]["enum"][0].asString() == "done");
	REQUIRE(s["properties"]["status"]["enum"][1].asString() == "accepted");

	// Written as the shape writer would write the row it stands for.
	const char *why = NULL;
	REQUIRE(schemaIsSane(kDone, &why));
	std::string shaped;
	mcp::appendOutputSchema(shaped, kDone);
	REQUIRE(text == shaped);
}

TEST_CASE("every shipped argument is described as the document describes it", "[mcp-schema]")
{
	setRoutesForTest(NULL);
	size_t count = 0;
	const RouteTable *const *tables = allRoutes(&count);
	std::string doc_text;
	openapi::appendDocument(doc_text, tables, count, true);
	const ::Json::Value doc = parsed(doc_text);

	size_t compared = 0;
	for (size_t t = 0; t < count; ++t)
	{
		for (size_t e = 0; e < tables[t]->count; ++e)
		{
			const Endpoint &ep = tables[t]->endpoints[e];
			std::string mine_text;
			mcp::appendInputSchema(mine_text, ep);
			const ::Json::Value mine = parsed(mine_text);
			const ::Json::Value &op = doc["paths"][ep.path][methodKey(ep.method)];

			for (size_t i = 0; i < ep.param_count; ++i)
			{
				const Param &p = ep.params[i];
				if (namesWholeBody(p.in))
					continue;
				INFO(ep.path << " " << p.name);
				::Json::Value theirs;
				std::string their_words;
				if (p.in == In::Body)
				{
					theirs = op["requestBody"]["content"]["application/json"]["schema"]["properties"][p.name];
					their_words = theirs["description"].asString();
				}
				else
				{
					for (::Json::ArrayIndex k = 0; k < op["parameters"].size(); ++k)
						if (op["parameters"][k]["name"].asString() == p.name &&
						    op["parameters"][k]["in"].asString() != "header")
						{
							theirs = op["parameters"][k]["schema"];
							their_words = op["parameters"][k]["description"].asString();
						}
				}
				::Json::Value ours = mine["properties"][p.name];
				REQUIRE(ours["description"].asString() == their_words);
				ours.removeMember("description");
				theirs.removeMember("description");
				// The same words are in the description.
				theirs.removeMember("x-enum-descriptions");
				// A set the box is asked for is the document's; a tool takes the name as text.
				if (p.choices != NULL)
				{
					REQUIRE_FALSE(ours.isMember("enum"));
					theirs.removeMember("enum");
				}
				REQUIRE(theirs.isObject());
				REQUIRE(ours == theirs);
				++compared;
			}
		}
	}
	// Fewer means the walk missed the shipped tables.
	REQUIRE(compared >= 100);
}

TEST_CASE("every shipped answer is described as the document describes it", "[mcp-schema]")
{
	setRoutesForTest(NULL);
	size_t count = 0;
	const RouteTable *const *tables = allRoutes(&count);
	std::string doc_text;
	openapi::appendDocument(doc_text, tables, count, true);
	const ::Json::Value doc = parsed(doc_text);
	const ::Json::Value &schemas = doc["components"]["schemas"];

	size_t compared = 0;
	for (size_t t = 0; t < count; ++t)
	{
		for (size_t e = 0; e < tables[t]->count; ++e)
		{
			const Endpoint &ep = tables[t]->endpoints[e];
			if (ep.schema == NULL)
				continue;
			const ::Json::Value &responses = doc["paths"][ep.path][methodKey(ep.method)]["responses"];
			const ::Json::Value::Members codes = responses.getMemberNames();
			for (size_t c = 0; c < codes.size(); ++c)
			{
				const ::Json::Value &content = responses[codes[c]]["content"];
				if (!content.isMember("application/json"))
					continue;
				INFO(ep.path << " " << codes[c]);
				const ::Json::Value theirs = inlined(content["application/json"]["schema"], schemas, 0);
				std::string mine;
				mcp::appendOutputSchema(mine, *ep.schema);
				REQUIRE(parsed(mine) == theirs);
				++compared;
			}
		}
	}
	// Fewer means the walk missed the shipped answers.
	REQUIRE(compared >= 40);
}
