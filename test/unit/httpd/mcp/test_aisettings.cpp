/*
 * test_aisettings.cpp - the route the KI tab reads and writes
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

#include "support/answers.h"
#include "support/catch.hpp"
#include "support/fakes.h"
#include "httpd/doc/openapi.h"
#include "httpd/credentials.h"
#include "httpd/endpoint.h"
#include "httpd/router.h"
#include "httpd/webconfig.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include <unistd.h>

namespace
{

struct Box
{
	InstalledDependencies wired;
	std::string path;

	explicit Box(const std::string &text)
	{
		char buf[128];
		static int counter = 0;
		std::snprintf(buf, sizeof(buf), "/tmp/ni-aiset-%d-%d", (int) getpid(), ++counter);
		path = buf;
		std::ofstream f(path.c_str(), std::ios::out | std::ios::trunc);
		f << text;
		f.close();
		REQUIRE(httpd::load(path));
	}

	~Box()
	{
		::unlink(path.c_str());
		httpd::setConfigForTest(httpd::defaultWebConfig());
	}

	std::string file() const
	{
		std::ifstream f(path.c_str());
		std::ostringstream out;
		out << f.rdbuf();
		return out.str();
	}
};

const char kFile[] =
	"port=8081\nusername=root\n"
	"ai_enabled=true\nai_public_url=https://tv.example.org\n"
	"ai_trusted_proxies=192.168.1.5\nai_allow_lan=false\n";

httpd::Response get(httpd::AuthLevel level = httpd::AuthLevel::System)
{
	return httpd::dispatch(httpd::Get, "/api/v1/ai/settings", "", "", "192.168.1.9", level);
}

httpd::Response put(const std::string &body, const std::string &peer = "192.168.1.9")
{
	return httpd::dispatch(httpd::Put, "/api/v1/ai/settings", "", body, peer, httpd::AuthLevel::System);
}

bool has(const httpd::Response &r, const char *needle)
{
	return r.body.find(needle) != std::string::npos;
}

std::string withPassword(const char *plain)
{
	return std::string(kFile) + "password_hash=" + httpd::hashSecret(plain, 1000) + "\n";
}

} // namespace

TEST_CASE("the AI settings read back as the file holds them", "[exposure]")
{
	Box box(kFile);
	const httpd::Response r = get();
	REQUIRE(r.code == 200);
	CHECK(has(r, "\"enabled\":true"));
	CHECK(has(r, "\"public_url\":\"https://tv.example.org\""));
	CHECK(has(r, "\"trusted_proxies\":\"192.168.1.5/32\""));
	CHECK(has(r, "\"allow_lan\":false"));
	CHECK(has(r, "\"mcp_url\":\"https://tv.example.org/mcp\""));
	CHECK(has(r, "\"tunnel_paths\":[\"/mcp\",\"/oauth/\",\"/.well-known/\"]"));
	CHECK(has(r, "\"port\":8081"));
}

TEST_CASE("an unset public address reads back with no mcp address", "[exposure]")
{
	Box box("port=8081\nusername=root\n");
	const httpd::Response r = get();
	REQUIRE(r.code == 200);
	CHECK(has(r, "{\"enabled\":false,\"public_url\":\"\",\"trusted_proxies\":\"\",\"allow_lan\":true,\"mcp_url\":\"\","));
}

TEST_CASE("the AI settings are the owner's alone", "[exposure]")
{
	Box box(kFile);
	CHECK(get(httpd::AuthLevel::Write).code == 403);
	CHECK(httpd::dispatch(httpd::Put, "/api/v1/ai/settings", "", "{\"enabled\":false}",
	                      "192.168.1.9", httpd::AuthLevel::Write).code == 403);
}

TEST_CASE("a change is written and the server is put on it after the answer", "[exposure]")
{
	Box box(kFile);
	const httpd::Response r = put("{\"public_url\":\"HTTPS://Box.Example.org/\",\"trusted_proxies\":\"10.0.0.7, 10.0.0.8\",\"allow_lan\":true}");
	REQUIRE(r.code == 200);
	CHECK(has(r, "{\"ai\":{\"enabled\":true,"));
	CHECK(has(r, "\"restarting\":true}"));
	CHECK(has(r, "\"public_url\":\"https://box.example.org\""));
	CHECK(r.reload_after == box.path);
	const std::string text = box.file();
	CHECK(text.find("ai_public_url=https://box.example.org\n") != std::string::npos);
	CHECK(text.find("ai_trusted_proxies=10.0.0.7/32,10.0.0.8/32\n") != std::string::npos);
	CHECK(text.find("ai_allow_lan=true\n") != std::string::npos);
	CHECK(text.find("ai_enabled=true\n") != std::string::npos);
}

TEST_CASE("asking for what is in effect moves nothing", "[exposure]")
{
	Box box(kFile);
	const httpd::Response r = put("{\"enabled\":true,\"public_url\":\"https://tv.example.org\"}");
	REQUIRE(r.code == 200);
	CHECK(has(r, "\"restarting\":false"));
	CHECK(r.reload_after.empty());
}

TEST_CASE("the first save on a box that named no AI key puts the server on it", "[exposure]")
{
	Box box("port=8081\nusername=root\n");
	const httpd::Response r = put("{\"enabled\":false}");
	REQUIRE(r.code == 200);
	CHECK(has(r, "\"restarting\":true}"));
	CHECK(r.reload_after == box.path);
}

TEST_CASE("a new public address alone puts the server on it", "[exposure]")
{
	Box box(kFile);
	const httpd::Response r = put("{\"public_url\":\"https://box.example.org\"}");
	REQUIRE(r.code == 200);
	CHECK(has(r, "\"restarting\":true}"));
	CHECK(r.reload_after == box.path);
}

TEST_CASE("an empty public url clears the one that was set", "[exposure]")
{
	Box box(kFile);
	const httpd::Response r = put("{\"public_url\":\"\"}");
	REQUIRE(r.code == 200);
	CHECK(has(r, "\"public_url\":\"\""));
	CHECK(has(r, "\"mcp_url\":\"\""));
	CHECK(r.reload_after == box.path);
	const std::string text = box.file();
	CHECK(text.find("ai_public_url=\n") != std::string::npos);
}

TEST_CASE("an empty proxy list clears the one that was set", "[exposure]")
{
	Box box(kFile);
	const httpd::Response r = put("{\"trusted_proxies\":\"\"}");
	REQUIRE(r.code == 200);
	CHECK(has(r, "\"trusted_proxies\":\"\""));
	CHECK(r.reload_after == box.path);
	const std::string text = box.file();
	CHECK(text.find("ai_trusted_proxies=\n") != std::string::npos);
}

TEST_CASE("a refused value leaves the file as it was", "[exposure]")
{
	Box box(kFile);
	const std::string before = box.file();

	const httpd::Response url = put("{\"public_url\":\"http://tv.example.org\"}");
	CHECK(url.code == 400);
	CHECK(has(url, "ai-public-url-refused"));

	const httpd::Response loop = put("{\"trusted_proxies\":\"127.0.0.1\"}");
	CHECK(loop.code == 400);
	CHECK(has(loop, "ai-trusted-proxies-refused"));

	const httpd::Response wide = put("{\"trusted_proxies\":\"10.0.0.0/8\"}");
	CHECK(wide.code == 400);
	CHECK(has(wide, "ai-trusted-proxies-refused"));

	const httpd::Response none = put("{}");
	CHECK(none.code == 400);
	CHECK(has(none, "missing-parameter"));

	CHECK(put("{\"enabled\":\"maybe\"}").code == 400);
	CHECK(box.file() == before);
}

TEST_CASE("the owner cannot make their own address a tunnel", "[exposure]")
{
	Box box(kFile);
	const std::string before = box.file();
	const httpd::Response plain = put("{\"trusted_proxies\":\"192.168.1.0/24\"}", "192.168.1.9");
	CHECK(plain.code == 409);
	CHECK(has(plain, "ai-caller-would-be-tunnel"));
	const httpd::Response mapped = put("{\"trusted_proxies\":\"192.168.1.9\"}", "::ffff:192.168.1.9");
	CHECK(mapped.code == 409);
	CHECK(box.file() == before);
	CHECK(put("{\"trusted_proxies\":\"192.168.1.0/24\"}", "10.0.0.2").code == 200);
}

TEST_CASE("a configuration file that went away is a server fault", "[exposure]")
{
	Box box(kFile);
	::unlink(box.path.c_str());
	const httpd::Response r = put("{\"enabled\":false}");
	CHECK(r.code == 500);
	CHECK(has(r, "webserver-not-configured"));
	CHECK(::access(box.path.c_str(), F_OK) != 0);
}

TEST_CASE("the body example the document gives for the AI settings is accepted", "[exposure]")
{
	Box box(kFile);
	CHECK(sendBodyExample("PUT", "/api/v1/ai/settings", std::map<std::string, std::string>()) == 200);
}

TEST_CASE("the document groups the AI routes under their own tag after the webserver", "[exposure]")
{
	httpd::setRoutesForTest(NULL);
	const std::string &doc = httpd::openapi::document();
	const size_t web = doc.find("{\"name\":\"webserver\"");
	const size_t ai = doc.find("{\"name\":\"ai\"");
	const size_t daemons = doc.find("{\"name\":\"daemons\"");
	REQUIRE(web != std::string::npos);
	REQUIRE(ai != std::string::npos);
	REQUIRE(daemons != std::string::npos);
	CHECK(web < ai);
	CHECK(ai < daemons);
}

TEST_CASE("a public address is refused while the box login is the shipped password", "[exposure]")
{
	Box box(withPassword("ni"));
	const std::string before = box.file();
	const httpd::Response r = put("{\"public_url\":\"https://box.example.org\"}");
	CHECK(r.code == 409);
	CHECK(has(r, "ai-default-password"));
	CHECK(box.file() == before);

	CHECK(put("{\"allow_lan\":true}").code == 200);
	CHECK(put("{\"public_url\":\"https://tv.example.org\"}").code == 200);
	CHECK(put("{\"public_url\":\"\"}").code == 200);
}

TEST_CASE("the guides name no public address while the tunnel is shut", "[exposure]")
{
	Box box(withPassword("ni"));
	const httpd::Response r = httpd::dispatch(httpd::Get, "/api/v1/ai/guides", "", "", "192.168.1.9",
	                                          httpd::AuthLevel::System);
	REQUIRE(r.code == 200);
	CHECK(has(r, "{\"mcp_url\":\"\","));
}

TEST_CASE("a public address is taken once the box login is not the shipped password", "[exposure]")
{
	Box box(withPassword("sofa-2026"));
	CHECK(put("{\"public_url\":\"https://box.example.org\"}").code == 200);
}

TEST_CASE("the AI settings say whether the box login is the shipped password", "[exposure]")
{
	{
		Box box(withPassword("ni"));
		CHECK(has(get(), "\"default_password\":true"));
	}
	{
		Box box(withPassword("sofa-2026"));
		CHECK(has(get(), "\"default_password\":false"));
	}
	Box none(kFile);
	CHECK(has(get(), "\"default_password\":false"));
}
