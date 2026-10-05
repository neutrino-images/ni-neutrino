/*
 * test_mcp_routetools.cpp - tools answered by their routes, in process
 *
 * Copyright (C) 2026 NI-Team
 *
 * License: GPL-2.0-or-later, see COPYING.
 */

#include "support/catch.hpp"

#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/http.h"
#include "httpd/schema.h"
#include "httpd/status.h"
#include "httpd/mcp/contract.h"
#include "httpd/mcp/routetools.h"
#include "httpd/mcp/toolschema.h"

#include "toolcaller.h"

#include "coreapi/base/errors.h"

#include "jsoncpp/json/json.h"

#include <cstdlib>
#include <string>

#include <fcntl.h>
#include <unistd.h>

using namespace httpd;

namespace
{

struct Seen
{
	int         reads;
	int         writes;
	int         siblings;
	std::string name;
	std::string q;
	std::string text;
	long        n;
	bool        flag;
	uint64_t    ch;
	int         fd;
};

Seen seen;

void forget()
{
	seen = Seen();
	seen.fd = -1;
}

const FieldDesc kEchoFields[] = {
	HTTPD_MEMBER("echo", FieldType::String, "what came back"),
};
const Schema kEcho = { "echo", HTTPD_FIELDS(kEchoFields) };

Response readProbe(const Request &r)
{
	seen.reads++;
	seen.name = r.asString("name");
	seen.q = r.has("q") ? r.asString("q") : std::string();
	Response out = okJson();
	out.body = "{\"echo\":\"read\"}";
	return out;
}

Response sibling(const Request &)
{
	seen.siblings++;
	Response out = okJson();
	out.body = "{\"echo\":\"sibling\"}";
	return out;
}

Response writeProbe(const Request &r)
{
	seen.writes++;
	seen.name = r.asString("name");
	seen.text = r.asString("text");
	seen.n = r.asInt("n");
	seen.flag = r.asBool("flag");
	seen.ch = r.asChannelId("ch");
	return noContent();
}

Response acceptProbe(const Request &)
{
	return accepted();
}

Response refuseProbe(const Request &)
{
	return problemResponse(StatusConflict, coreapi::ErrorCode::RecordingRunning,
	                       "a recording that is running keeps the start it began at");
}

Response fileProbe(const Request &)
{
	char path[] = "/tmp/mcp-probe-XXXXXX";
	const int fd = ::mkstemp(path);
	if (fd >= 0)
	{
		(void) ::write(fd, "x", 1);
		::unlink(path);
	}
	seen.fd = fd;
	Response out;
	out.code = StatusOk;
	(void) answerFromDescriptor(out, fd);
	return out;
}

const Param kReadParams[] = {
	HTTPD_SEGMENT_TEXT("name", "a name", 64),
	HTTPD_QUERY_TEXT("q", "a query", 64),
};

const Param kWriteParams[] = {
	HTTPD_SEGMENT_TEXT("name", "a name", 64),
	HTTPD_BODY_TEXT("text", "some text", 64),
	HTTPD_BODY_IN("n", ParamType::Int, "a count", 0, 10),
	HTTPD_BODY("flag", ParamType::Bool, "a switch"),
	HTTPD_BODY("ch", ParamType::ChannelId, "a channel"),
};

Response originProbe(const Request &r)
{
	Response out = okJson();
	switch (r.origin())
	{
	case Origin::Lan:
		out.body = "{\"echo\":\"lan\"}";
		break;
	case Origin::Tunnel:
		out.body = "{\"echo\":\"tunnel\"}";
		break;
	default:
		out.body = "{\"echo\":\"refused\"}";
		break;
	}
	return out;
}

const Endpoint kProbes[] = {
	{ Method::Get, "/api/v1/probe/{name}", AuthLevel::Read, "reads", NULL, HTTPD_PARAMS(kReadParams), &kEcho, &readProbe, false, Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/probe/sibling", AuthLevel::Read, "the sibling", NULL, NULL, 0, &kEcho, &sibling, false, Answers200, HTTPD_NO_REFUSALS },
	{ Method::Put, "/api/v1/probe/{name}", AuthLevel::Write, "writes", NULL, HTTPD_PARAMS(kWriteParams), NULL, &writeProbe, false, Answers204, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/probe-accept", AuthLevel::Write, "accepts", NULL, NULL, 0, NULL, &acceptProbe, false, Answers202, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/probe-refuse", AuthLevel::Write, "refuses", NULL, NULL, 0, NULL, &refuseProbe, false, Answers204, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/probe-file", AuthLevel::Read, "a file", NULL, NULL, 0, &kEcho, &fileProbe, false, Answers200, HTTPD_NO_REFUSALS },
};

const ToolFlag kProbeTools[] = {
	HTTPD_TOOL_AS(Method::Get, "/api/v1/probe/{name}", "read_probe", NULL),
	HTTPD_TOOL_AS(Method::Put, "/api/v1/probe/{name}", "write_probe", NULL),
	HTTPD_TOOL_AS(Method::Post, "/api/v1/probe-accept", "accept_probe", NULL),
	HTTPD_TOOL_AS(Method::Post, "/api/v1/probe-refuse", "refuse_probe", NULL),
	HTTPD_TOOL_AS(Method::Get, "/api/v1/probe-file", "file_probe", NULL),
};

const RouteTable kProbeTable = { HTTPD_TABLE_WITH_TOOLS("probe", kProbes, kProbeTools) };
const RouteTable *const kTables[] = { &kProbeTable };
const RouteTable kNoComposed = { HTTPD_TABLE_N("composed", (const Endpoint *) NULL, 0) };

const Endpoint kOriginProbes[] = {
	{ Method::Get, "/api/v1/probe-origin", AuthLevel::Read, "the origin", NULL, NULL, 0, &kEcho, &originProbe, false, Answers200, HTTPD_NO_REFUSALS },
};
const ToolFlag kOriginTools[] = {
	HTTPD_TOOL_AS(Method::Get, "/api/v1/probe-origin", "origin_probe", NULL),
};
const RouteTable kOriginTable = { HTTPD_TABLE_WITH_TOOLS("origin", kOriginProbes, kOriginTools) };
const RouteTable *const kOriginTables[] = { &kOriginTable };

const ToolFlag kBadTools[] = { HTTPD_TOOL_AS(Method::Get, "/api/v1/probe/absent", "absent", NULL) };
const RouteTable kBadTable = { HTTPD_TABLE_WITH_TOOLS("probe", kProbes, kBadTools) };
const RouteTable *const kBadTables[] = { &kBadTable };

} // namespace

TEST_CASE("every tool is listed whatever the caller with its route's level", "[mcp-routetools]")
{
	mcp::RouteTools tools(kTables, 1, kNoComposed);
	REQUIRE(tools.refusal().empty());
	const std::vector<mcp::ToolDef> all = tools.list();
	REQUIRE(all.size() == 5);
	REQUIRE(all[0].name == "read_probe");
	REQUIRE(all[0].level == AuthLevel::Read);
	REQUIRE(all[1].name == "write_probe");
	REQUIRE(all[1].level == AuthLevel::Write);
	REQUIRE(all[4].name == "file_probe");
	REQUIRE(all[4].level == AuthLevel::Read);
	REQUIRE(tools.hint("read_probe", coreapi::ErrorCode::BadString).empty());
}

TEST_CASE("arguments reach the handler byte for byte wherever the route carries them", "[mcp-routetools]")
{
	forget();
	mcp::RouteTools tools(kTables, 1, kNoComposed);
	const std::string odd = "a/b&c=d %25 \xc3\xbc #+";
	const std::string args = "{\"name\":\"" + odd +
		"\",\"text\":\"say \\\"hi\\\"\",\"n\":7,\"flag\":true,\"ch\":\"0x2b66\"}";
	const coreapi::Result<std::string> got = tools.call(callerAt(AuthLevel::Write), "write_probe", args);
	INFO((got.ok() ? got.value() : got.error().message));
	REQUIRE(got.ok());
	REQUIRE(got.value() == "{\"status\":\"done\"}");
	REQUIRE(seen.writes == 1);
	REQUIRE(seen.name == odd);
	REQUIRE(seen.text == "say \"hi\"");
	REQUIRE(seen.n == 7);
	REQUIRE(seen.flag);
	REQUIRE(seen.ch == 0x2b66);

	forget();
	const coreapi::Result<std::string> read =
		tools.call(callerAt(AuthLevel::Read), "read_probe", "{\"name\":\"x\",\"q\":\"" + odd + "\"}");
	REQUIRE(read.ok());
	REQUIRE(read.value() == "{\"echo\":\"read\"}");
	REQUIRE(seen.q == odd);
}

TEST_CASE("a path argument spelling a sibling route is a value and not that route", "[mcp-routetools]")
{
	forget();
	mcp::RouteTools tools(kTables, 1, kNoComposed);
	const coreapi::Result<std::string> got = tools.call(callerAt(AuthLevel::Read), "read_probe", "{\"name\":\"sibling\"}");
	REQUIRE(got.ok());
	REQUIRE(seen.reads == 1);
	REQUIRE(seen.name == "sibling");
	REQUIRE(seen.siblings == 0);
}

TEST_CASE("an argument of the wrong JSON kind never reaches the handler", "[mcp-routetools]")
{
	mcp::RouteTools tools(kTables, 1, kNoComposed);
	struct Row { const char *args; coreapi::ErrorCode code; };
	static const Row rows[] = {
		// A number here would be read as the hex of its decimal digits.
		{ "{\"name\":\"x\",\"ch\":123}",     coreapi::ErrorCode::BadString },
		{ "{\"name\":\"x\",\"n\":\"7\"}",    coreapi::ErrorCode::BadInt },
		{ "{\"name\":\"x\",\"flag\":\"true\"}", coreapi::ErrorCode::BadBool },
		{ "{\"name\":\"x\",\"colour\":\"blue\"}", coreapi::ErrorCode::NoSuchParameter },
		{ "{\"name\":\"x\",\"n\":1,\"n\":2}",  coreapi::ErrorCode::DuplicateParameter },
		{ "{\"text\":\"no name\"}",            coreapi::ErrorCode::MissingParameter },
		{ "{\"name\":\"x\",\"n\":11}",          coreapi::ErrorCode::OutOfRange },
		{ "{\"name\":{\"deep\":1}}",           coreapi::ErrorCode::BadString },
	};
	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
	{
		forget();
		INFO(rows[i].args);
		const coreapi::Result<std::string> got = tools.call(callerAt(AuthLevel::Write), "write_probe", rows[i].args);
		REQUIRE_FALSE(got.ok());
		INFO(coreapi::codeString(got.error().code) << ": " << got.error().message);
		REQUIRE(got.error().code == rows[i].code);
		REQUIRE(got.error().status == coreapi::Status::InvalidArgument);
		REQUIRE(seen.writes == 0);
	}
}

TEST_CASE("an argument sent as null is one left out", "[mcp-routetools]")
{
	mcp::RouteTools tools(kTables, 1, kNoComposed);

	forget();
	const coreapi::Result<std::string> optional =
		tools.call(callerAt(AuthLevel::Read), "read_probe", "{\"name\":\"x\",\"q\":null}");
	INFO((optional.ok() ? optional.value() : optional.error().message));
	REQUIRE(optional.ok());
	REQUIRE(seen.reads == 1);
	REQUIRE(seen.name == "x");
	REQUIRE(seen.q.empty());

	forget();
	const coreapi::Result<std::string> required =
		tools.call(callerAt(AuthLevel::Read), "read_probe", "{\"name\":null,\"q\":\"y\"}");
	REQUIRE_FALSE(required.ok());
	INFO(coreapi::codeString(required.error().code) << ": " << required.error().message);
	REQUIRE(required.error().code == coreapi::ErrorCode::MissingParameter);
	REQUIRE(seen.reads == 0);
}

TEST_CASE("a caller below the tool's level is refused before anything runs", "[mcp-routetools]")
{
	forget();
	mcp::RouteTools tools(kTables, 1, kNoComposed);
	const coreapi::Result<std::string> got =
		tools.call(callerAt(AuthLevel::Read), "write_probe", "{\"name\":\"x\"}");
	REQUIRE_FALSE(got.ok());
	REQUIRE(got.error().status == coreapi::Status::Denied);
	REQUIRE(got.error().code == coreapi::ErrorCode::NotPermitted);
	REQUIRE(got.error().message.find("write") != std::string::npos);
	REQUIRE(seen.writes == 0);
}

TEST_CASE("a tool nobody offers is answered as absent", "[mcp-routetools]")
{
	mcp::RouteTools tools(kTables, 1, kNoComposed);
	const coreapi::Result<std::string> got = tools.call(callerAt(AuthLevel::System), "reboot", "{}");
	REQUIRE_FALSE(got.ok());
	REQUIRE(got.error().status == coreapi::Status::NotFound);
	REQUIRE(got.error().code == coreapi::ErrorCode::NoSuchTool);
	REQUIRE(std::string(coreapi::codeString(coreapi::ErrorCode::NoSuchTool)) == "no-such-tool");
}

TEST_CASE("what the handler answers comes back as the tool's result", "[mcp-routetools]")
{
	mcp::RouteTools tools(kTables, 1, kNoComposed);

	const coreapi::Result<std::string> took = tools.call(callerAt(AuthLevel::Write), "accept_probe", "{}");
	REQUIRE(took.ok());
	REQUIRE(took.value() == "{\"status\":\"accepted\"}");

	const coreapi::Result<std::string> refused = tools.call(callerAt(AuthLevel::Write), "refuse_probe", "{}");
	REQUIRE_FALSE(refused.ok());
	REQUIRE(refused.error().status == coreapi::Status::Conflict);
	REQUIRE(refused.error().code == coreapi::ErrorCode::RecordingRunning);
	REQUIRE(refused.error().message == "a recording that is running keeps the start it began at");
}

TEST_CASE("every status word a tool writes is one the done schema promises", "[mcp-routetools]")
{
	std::string schema;
	mcp::appendDoneSchema(schema);
	::Json::Value doc;
	::Json::Reader reader;
	REQUIRE(reader.parse(schema, doc));
	const ::Json::Value &words = doc["properties"]["status"]["enum"];
	REQUIRE(words.isArray());

	mcp::RouteTools tools(kTables, 1, kNoComposed);
	struct Row { const char *tool; const char *args; };
	static const Row rows[] = {
		{ "accept_probe", "{}" },
		{ "write_probe", "{\"name\":\"x\",\"text\":\"t\",\"n\":1,\"flag\":false,\"ch\":\"0x1\"}" },
	};
	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
	{
		INFO(rows[i].tool);
		const coreapi::Result<std::string> got = tools.call(callerAt(AuthLevel::Write), rows[i].tool, rows[i].args);
		REQUIRE(got.ok());
		::Json::Value answer;
		REQUIRE(reader.parse(got.value(), answer));
		const std::string word = answer["status"].asString();
		INFO(word);
		bool promised = false;
		for (::Json::ArrayIndex k = 0; k < words.size(); ++k)
			promised = promised || words[k].asString() == word;
		REQUIRE(promised);
	}
}

TEST_CASE("a route that answers with a file is no tool answer and the file is closed", "[mcp-routetools]")
{
	forget();
	mcp::RouteTools tools(kTables, 1, kNoComposed);
	const coreapi::Result<std::string> got = tools.call(callerAt(AuthLevel::Read), "file_probe", "{}");
	REQUIRE_FALSE(got.ok());
	REQUIRE(got.error().code == coreapi::ErrorCode::BadTable);
	REQUIRE(seen.fd >= 0);
	REQUIRE(::fcntl(seen.fd, F_GETFD) == -1);
}

TEST_CASE("flags that break a rule offer nothing at all", "[mcp-routetools]")
{
	mcp::RouteTools tools(kBadTables, 1, kNoComposed);
	REQUIRE_FALSE(tools.refusal().empty());
	REQUIRE(tools.list().empty());
	REQUIRE(tools.call(callerAt(AuthLevel::System), "read_probe", "{\"name\":\"x\"}").error().code
		== coreapi::ErrorCode::NoSuchTool);
}

TEST_CASE("a tool call runs its route with the caller's origin", "[mcp][mcp-routetools]")
{
	mcp::RouteTools tools(kOriginTables, 1, kNoComposed);

	mcp::Caller lan = callerAt(AuthLevel::Read);
	lan.external = false;
	const coreapi::Result<std::string> lanGot = tools.call(lan, "origin_probe", "{}");
	INFO((lanGot.ok() ? lanGot.value() : lanGot.error().message));
	REQUIRE(lanGot.ok());
	REQUIRE(lanGot.value() == "{\"echo\":\"lan\"}");

	mcp::Caller tunnel = callerAt(AuthLevel::Read);
	tunnel.external = true;
	const coreapi::Result<std::string> tunnelGot = tools.call(tunnel, "origin_probe", "{}");
	INFO((tunnelGot.ok() ? tunnelGot.value() : tunnelGot.error().message));
	REQUIRE(tunnelGot.ok());
	REQUIRE(tunnelGot.value() == "{\"echo\":\"tunnel\"}");
}
