/*
 * test_aitotp.cpp - the routes that set up, confirm and turn off two-factor sign-in
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
#include "support/httpclient.h"
#include "httpd/auth.h"
#include "httpd/credentials.h"
#include "httpd/endpoint.h"
#include "httpd/mcp/exposure.h"
#include "httpd/oauth/oauthtest.h"
#include "httpd/oauth/totp.h"
#include "httpd/oauth/twofactor.h"
#include "httpd/router.h"
#include "httpd/webconfig.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

namespace
{

const time_t kNow = 1790000000;

std::string signed_in;

struct Box
{
	InstalledDependencies wired;
	std::string path;

	Box()
	{
		char buf[128];
		static int counter = 0;
		std::snprintf(buf, sizeof(buf), "/tmp/ni-aitotp-%d-%d", (int) getpid(), ++counter);
		path = buf;
		std::ofstream f(path.c_str(), std::ios::out | std::ios::trunc);
		f << "port=8081\nusername=root\npassword_hash=" << httpd::hashSecret(kAccountPassword, 1000) << "\n";
		f.close();
		REQUIRE(httpd::load(path));
		httpd::forgetLoginAttemptsForTest();
		httpd::oauth::forgetPendingTotpForTest();
		httpd::oauth::setTotpClockForTest(kNow);
		signed_in = httpd::openSession("root");
		REQUIRE_FALSE(signed_in.empty());
	}
	~Box()
	{
		httpd::closeSession(signed_in);
		signed_in.clear();
		httpd::forgetApiTokens();
		::unlink(path.c_str());
		httpd::oauth::installTotp(std::string(), 0);
		httpd::oauth::forgetPendingTotpForTest();
		httpd::oauth::setTotpClockForTest(0);
		httpd::oauth::setTotpDrawFailsForTest(false);
		httpd::forgetLoginAttemptsForTest();
		httpd::setLoginClockForTest(0);
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

httpd::Response call(httpd::Method m, const char *path, const std::string &body = std::string(),
                     httpd::Origin o = httpd::Origin::Lan, httpd::AuthLevel level = httpd::AuthLevel::System)
{
	return httpd::dispatch(m, path, "", body, "192.168.1.9", level, std::string(), signed_in,
	                       std::string(), std::string(), o);
}

std::vector<std::pair<std::string, std::string> > bearer(const std::string &token)
{
	std::vector<std::pair<std::string, std::string> > h;
	h.push_back(std::make_pair(std::string("Authorization"), "Bearer " + token));
	return h;
}

std::vector<std::pair<std::string, std::string> > session(const std::string &token)
{
	std::vector<std::pair<std::string, std::string> > h;
	h.push_back(std::make_pair(std::string("Cookie"), std::string(httpd::sessionCookieName()) + "=" + token));
	h.push_back(std::make_pair(std::string(httpd::csrfHeaderName()), httpd::csrfFor(token)));
	return h;
}

std::string systemToken()
{
	const std::string t = httpd::randomToken();
	httpd::addApiToken(httpd::tokenLookupPrefix(t), httpd::hashSecret(t), httpd::AuthLevel::System);
	return t;
}

bool has(const httpd::Response &r, const std::string &needle)
{
	return r.body.find(needle) != std::string::npos;
}

std::string codeFor(const std::string &secret, time_t at)
{
	std::string key;
	REQUIRE(httpd::oauth::base32Decode(secret, &key));
	return httpd::oauth::hotp(key, (unsigned long long) (at / httpd::oauth::kTotpStepSeconds), 6);
}

std::string codeBody(const std::string &code)
{
	return "{\"code\":\"" + code + "\"}";
}

std::string passwordBody(const std::string &password)
{
	return "{\"password\":\"" + password + "\"}";
}

std::string startSetup()
{
	const httpd::Response s = call(httpd::Post, "/api/v1/ai/totp/setup");
	REQUIRE(s.code == 200);
	return parsedJson(s.body)["secret"].asString();
}

std::string setUp()
{
	const std::string secret = startSetup();
	REQUIRE(call(httpd::Post, "/api/v1/ai/totp/confirm", codeBody(codeFor(secret, httpd::oauth::totpNow()))).code == 204);
	return secret;
}

} // namespace

TEST_CASE("a setup answers a new secret and its address and turns nothing on", "[ai-totp]")
{
	Box box;
	const httpd::Response r = call(httpd::Post, "/api/v1/ai/totp/setup");
	REQUIRE(r.code == 200);
	const ::Json::Value got = parsedJson(r.body);
	const std::string secret = got["secret"].asString();
	std::string raw;
	REQUIRE(httpd::oauth::base32Decode(secret, &raw));
	CHECK(raw.size() == 20);
	CHECK(got["uri"].asString().find("otpauth://totp/Neutrino:") == 0);
	CHECK(got["uri"].asString().find("?secret=" + secret + "&issuer=Neutrino") != std::string::npos);
	CHECK_FALSE(httpd::oauth::totpActive());
	CHECK(has(call(httpd::Get, "/api/v1/ai/settings"), "\"totp\":false"));
}

TEST_CASE("a setup the box cannot draw a secret for is a server fault and keeps the state", "[ai-totp]")
{
	Box box;
	const std::string secret = setUp();
	httpd::oauth::setTotpDrawFailsForTest(true);
	const httpd::Response r = call(httpd::Post, "/api/v1/ai/totp/setup");
	CHECK(r.code == 500);
	CHECK(has(r, "box-unreadable"));
	CHECK(httpd::oauth::totpActive());
	httpd::oauth::setTotpDrawFailsForTest(false);
	CHECK(call(httpd::Post, "/api/v1/ai/totp/confirm", codeBody(codeFor(secret, kNow))).code == 409);
}

TEST_CASE("setting up two-factor sign-in is refused off the home network and below System", "[ai-totp]")
{
	Box box;
	const httpd::Response tunnel = call(httpd::Post, "/api/v1/ai/totp/setup", "", httpd::Origin::Tunnel);
	CHECK(tunnel.code == 403);
	CHECK(has(tunnel, "not-permitted"));
	CHECK(call(httpd::Post, "/api/v1/ai/totp/setup", "", httpd::Origin::Lan, httpd::AuthLevel::Write).code == 403);
	CHECK(call(httpd::Post, "/api/v1/ai/totp/confirm", codeBody("123456"), httpd::Origin::Tunnel).code == 403);
	CHECK(call(httpd::Post, "/api/v1/ai/totp/disable", passwordBody(kAccountPassword), httpd::Origin::Tunnel).code == 403);
}

TEST_CASE("a system token from the home network cannot change two-factor sign-in", "[ai-totp]")
{
	Box box;
	const std::string token = systemToken();
	RunningServer srv;

	testhttp::Reply r = testhttp::request(srv.port, "POST", "/api/v1/ai/totp/setup", bearer(token), "{}");
	CHECK(r.code == 403);
	CHECK(r.body.find("not-permitted") != std::string::npos);
	CHECK(call(httpd::Post, "/api/v1/ai/totp/confirm", codeBody("123456")).code == 409);

	const std::string pending = startSetup();
	r = testhttp::request(srv.port, "POST", "/api/v1/ai/totp/confirm", bearer(token), codeBody(codeFor(pending, kNow)));
	CHECK(r.code == 403);
	CHECK(r.body.find("not-permitted") != std::string::npos);
	CHECK_FALSE(httpd::oauth::totpActive());

	REQUIRE(call(httpd::Post, "/api/v1/ai/totp/confirm", codeBody(codeFor(pending, kNow))).code == 204);
	r = testhttp::request(srv.port, "POST", "/api/v1/ai/totp/disable", bearer(token), passwordBody(kAccountPassword));
	CHECK(r.code == 403);
	CHECK(r.body.find("not-permitted") != std::string::npos);
	CHECK(httpd::oauth::totpActive());
}

TEST_CASE("a signed-in session from the home network sets up and turns off two-factor sign-in", "[ai-totp]")
{
	Box box;
	RunningServer srv;

	testhttp::Reply r = testhttp::request(srv.port, "POST", "/api/v1/ai/totp/setup", session(signed_in), "{}");
	REQUIRE(r.code == 200);
	const std::string secret = parsedJson(r.body)["secret"].asString();
	r = testhttp::request(srv.port, "POST", "/api/v1/ai/totp/confirm", session(signed_in), codeBody(codeFor(secret, kNow)));
	CHECK(r.code == 204);
	CHECK(httpd::oauth::totpActive());
	r = testhttp::request(srv.port, "POST", "/api/v1/ai/totp/disable", session(signed_in), passwordBody(kAccountPassword));
	CHECK(r.code == 204);
	CHECK_FALSE(httpd::oauth::totpActive());
}

TEST_CASE("a System grant without a live session is refused on every two-factor route", "[ai-totp]")
{
	Box box;
	const std::string secret = setUp();
	httpd::closeSession(signed_in);
	CHECK(call(httpd::Post, "/api/v1/ai/totp/setup").code == 403);
	CHECK(call(httpd::Post, "/api/v1/ai/totp/confirm", codeBody(codeFor(secret, kNow))).code == 403);
	CHECK(call(httpd::Post, "/api/v1/ai/totp/disable", passwordBody(kAccountPassword)).code == 403);
	CHECK(httpd::oauth::totpActive());
}

TEST_CASE("a right code turns the pending secret on and the secret is never answered again", "[ai-totp]")
{
	Box box;
	const std::string secret = setUp();
	CHECK(httpd::oauth::totpActive());
	const httpd::Response settings = call(httpd::Get, "/api/v1/ai/settings");
	CHECK(has(settings, "\"totp\":true"));
	CHECK_FALSE(has(settings, secret));
	CHECK(box.file().find("ai_totp_secret=" + secret + "\n") != std::string::npos);
}

TEST_CASE("a wrong confirm code keeps the state there was", "[ai-totp]")
{
	Box box;
	const std::string first = setUp();
	startSetup();
	const httpd::Response r = call(httpd::Post, "/api/v1/ai/totp/confirm", codeBody("000000x"));
	CHECK(r.code == 400);
	CHECK(has(r, "ai-totp-code-wrong"));
	httpd::oauth::setTotpClockForTest(kNow + 30);
	CHECK(httpd::oauth::useTotpCode(codeFor(first, kNow + 30), false) == httpd::oauth::CodeUse::Accepted);
	CHECK(box.file().find("ai_totp_secret=" + first + "\n") != std::string::npos);
}

TEST_CASE("a confirm without a pending setup or after ten minutes is refused", "[ai-totp]")
{
	Box box;
	httpd::Response r = call(httpd::Post, "/api/v1/ai/totp/confirm", codeBody("123456"));
	CHECK(r.code == 409);
	CHECK(has(r, "ai-totp-no-pending"));
	const std::string secret = startSetup();
	httpd::oauth::setTotpClockForTest(kNow + 601);
	r = call(httpd::Post, "/api/v1/ai/totp/confirm", codeBody(codeFor(secret, kNow + 601)));
	CHECK(r.code == 409);
	CHECK(has(r, "ai-totp-no-pending"));
}

TEST_CASE("a confirm before the box clock is set says so", "[ai-totp]")
{
	Box box;
	httpd::oauth::setTotpClockForTest(httpd::oauth::kTotpClockFloor - 100);
	startSetup();
	const httpd::Response r = call(httpd::Post, "/api/v1/ai/totp/confirm", codeBody("123456"));
	CHECK(r.code == 409);
	CHECK(has(r, "ai-totp-clock-unknown"));
}

TEST_CASE("a confirm whose secret cannot be written is a server fault", "[ai-totp]")
{
	Box box;
	const std::string secret = startSetup();
	::unlink(box.path.c_str());
	const httpd::Response r = call(httpd::Post, "/api/v1/ai/totp/confirm", codeBody(codeFor(secret, kNow)));
	CHECK(r.code == 500);
	CHECK(has(r, "webserver-not-configured"));
	CHECK_FALSE(httpd::oauth::totpActive());
}

TEST_CASE("turning two-factor sign-in off needs the password", "[ai-totp]")
{
	Box box;
	setUp();
	httpd::Response r = call(httpd::Post, "/api/v1/ai/totp/disable", passwordBody("wrong"));
	CHECK(r.code == 403);
	CHECK(has(r, "not-permitted"));
	CHECK(httpd::oauth::totpActive());
	CHECK(call(httpd::Post, "/api/v1/ai/totp/disable", passwordBody(kAccountPassword)).code == 204);
	CHECK_FALSE(httpd::oauth::totpActive());
	CHECK(box.file().find("ai_totp_secret=\n") != std::string::npos);
	CHECK(has(call(httpd::Get, "/api/v1/ai/settings"), "\"totp\":false"));
	r = call(httpd::Post, "/api/v1/ai/totp/disable", passwordBody(kAccountPassword));
	CHECK(r.code == 409);
	CHECK(has(r, "ai-totp-not-set-up"));
}

TEST_CASE("wrong passwords for turning it off are slowed down", "[ai-totp]")
{
	Box box;
	setUp();
	httpd::setLoginClockForTest(kNow);
	int first = -1;
	for (int i = 0; i < 8 && first < 0; ++i)
	{
		const httpd::Response r = call(httpd::Post, "/api/v1/ai/totp/disable", passwordBody("wrong"));
		if (r.code == 429)
		{
			first = i;
			CHECK(has(r, "too-many-attempts"));
		}
	}
	CHECK(first > 0);
	CHECK(httpd::oauth::totpActive());
}

TEST_CASE("turning it off with the configuration file gone is a server fault", "[ai-totp]")
{
	Box box;
	setUp();
	::unlink(box.path.c_str());
	const httpd::Response r = call(httpd::Post, "/api/v1/ai/totp/disable", passwordBody(kAccountPassword));
	CHECK(r.code == 500);
	CHECK(has(r, "webserver-not-configured"));
	CHECK(httpd::oauth::totpActive());
}

TEST_CASE("the body examples the document gives for two-factor sign-in are accepted", "[ai-totp]")
{
	Box box;
	const std::string secret = startSetup();
	const std::map<std::string, std::string> none;
	CHECK(sendBodyExample("POST", "/api/v1/ai/totp/confirm", none, "123456", codeFor(secret, kNow), true, signed_in) == 204);
	CHECK(sendBodyExample("POST", "/api/v1/ai/totp/disable", none, "your-password", kAccountPassword, true, signed_in) == 204);
}

TEST_CASE("the gate hides the two-factor routes from the tunnel", "[ai-totp]")
{
	const httpd::exposure::Policy p = httpd::exposure::policyFrom(tunnelConfig());
	const char *const paths[] = { "/api/v1/ai/totp/setup", "/api/v1/ai/totp/confirm", "/api/v1/ai/totp/disable" };
	for (size_t i = 0; i < 3; ++i)
	{
		INFO(paths[i]);
		CHECK(httpd::exposure::admit(httpd::Origin::Tunnel, paths[i], p) == httpd::exposure::Admit::Hidden);
	}
	ConfigInstalled config(tunnelConfig());
	RunningServer srv;
	CHECK(testhttp::request(srv.port, "POST", "/api/v1/ai/totp/setup").code == 404);
}
