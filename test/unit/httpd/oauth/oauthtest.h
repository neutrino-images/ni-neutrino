/*
 * oauthtest.h - shared helpers for the OAuth cases
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

#ifndef __test_oauth_oauthtest_h__
#define __test_oauth_oauthtest_h__

#include <config.h>

#include "support/catch.hpp"
#include "support/fakes.h"
#include "jsoncpp/json/json.h"
#include "httpd/auth.h"
#include "httpd/credentials.h"
#include "httpd/netmatch.h"
#include "httpd/oauth/cimd.h"
#include "httpd/oauth/sourcelimit.h"
#include "httpd/oauth/totp.h"
#include "httpd/oauth/twofactor.h"
#include "httpd/server.h"
#include "httpd/webconfig.h"

#include <cstdio>
#include <ctime>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

// A fresh path in /tmp that does not exist yet.
inline std::string oauthTempPath(const char *stem)
{
	char buf[128];
	std::snprintf(buf, sizeof(buf), "/tmp/ni-oauth-%s-%ld-XXXXXX", stem, (long) ::getpid());
	const int fd = ::mkstemp(buf);
	if (fd >= 0)
	{
		::close(fd);
		::unlink(buf);
	}
	return std::string(buf);
}

// Parses a JSON document, requiring a clean parse.
inline ::Json::Value parsedJson(const std::string &text)
{
	::Json::CharReaderBuilder builder;
	std::unique_ptr< ::Json::CharReader> reader(builder.newCharReader());
	::Json::Value root;
	std::string errs;
	REQUIRE(reader->parse(text.data(), text.data() + text.size(), &root, &errs));
	return root;
}

const char kPublicUrl[] = "https://tv.example.org";
const char kAccountPassword[] = "sofa-2026";

inline httpd::NetPrefix prefixOf(const char *text)
{
	httpd::NetPrefix p;
	REQUIRE(httpd::parsePrefix(text, &p));
	return p;
}

// The box's one account, not on the shipped password: that shuts the tunnel.
inline httpd::WebConfig accountConfig()
{
	httpd::WebConfig c = httpd::defaultWebConfig();
	c.username = "root";
	c.password_hash = httpd::hashSecret(kAccountPassword, 1000);
	REQUIRE_FALSE(c.password_hash.empty());
	return c;
}

// AI access on and no tunnel peer: a loopback request is Origin::Lan.
inline httpd::WebConfig lanConfig()
{
	httpd::WebConfig c = accountConfig();
	c.ai_enabled = true;
	c.ai_named = true;
	c.ai_public_url = kPublicUrl;
	return c;
}

// Loopback is the tunnel peer: every request of the case is Origin::Tunnel.
inline httpd::WebConfig tunnelConfig()
{
	httpd::WebConfig c = lanConfig();
	c.ai_trusted_proxies.push_back(prefixOf("127.0.0.1/32"));
	c.ai_trusted_proxies.push_back(prefixOf("::1/128"));
	return c;
}

// Installed for the life of a case; the previous one comes back however it ends.
struct ConfigInstalled
{
	httpd::WebConfig before;

	explicit ConfigInstalled(const httpd::WebConfig &c) : before(httpd::config())
	{
		httpd::setConfigForTest(c);
		httpd::forgetLoginAttemptsForTest();
		httpd::oauth::forgetSourceLimitsForTest();
	}
	~ConfigInstalled()
	{
		httpd::setConfigForTest(before);
		httpd::forgetLoginAttemptsForTest();
		httpd::setLoginClockForTest(0);
	}

	private:
		ConfigInstalled(const ConfigInstalled &);
		ConfigInstalled &operator=(const ConfigInstalled &);
};

struct AccountConfigured : ConfigInstalled
{
	AccountConfigured() : ConfigInstalled(accountConfig())
	{
	}
};

struct LanConfigured : ConfigInstalled
{
	LanConfigured() : ConfigInstalled(lanConfig())
	{
	}
};

struct TunnelConfigured : ConfigInstalled
{
	TunnelConfigured() : ConfigInstalled(tunnelConfig())
	{
	}
};

// RFC 6238's SHA-1 key in base32.
const char kTotpSecret[] = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";
const time_t kTotpClock = 1790000000;

// Two-factor sign-in on with kTotpSecret at kTotpClock, off again however the case ends.
struct TotpInstalled
{
	TotpInstalled()
	{
		httpd::oauth::setTotpClockForTest(kTotpClock);
		httpd::oauth::installTotp(kTotpSecret, 0);
		httpd::oauth::forgetPendingTotpForTest();
	}
	~TotpInstalled()
	{
		httpd::oauth::installTotp(std::string(), 0);
		httpd::oauth::forgetPendingTotpForTest();
		httpd::oauth::setTotpClockForTest(0);
	}

	private:
		TotpInstalled(const TotpInstalled &);
		TotpInstalled &operator=(const TotpInstalled &);
};

inline std::string totpCodeAt(time_t t)
{
	std::string key;
	REQUIRE(httpd::oauth::base32Decode(kTotpSecret, &key));
	return httpd::oauth::hotp(key, (unsigned long long) (t / httpd::oauth::kTotpStepSeconds),
	                          httpd::oauth::kTotpDigits);
}

inline std::string currentTotpCode()
{
	return totpCodeAt(httpd::oauth::totpNow());
}

// One step on first, so no two calls answer a code already used.
inline std::string nextTotpCode()
{
	httpd::oauth::setTotpClockForTest(httpd::oauth::totpNow() + (time_t) httpd::oauth::kTotpStepSeconds);
	return currentTotpCode();
}

// Six digits that are the code of no step the window takes.
inline std::string wrongTotpCode()
{
	const time_t now = httpd::oauth::totpNow();
	const std::string taken[] = { totpCodeAt(now - 30), totpCodeAt(now), totpCodeAt(now + 30) };
	for (unsigned n = 0;; ++n)
	{
		char code[8];
		std::snprintf(code, sizeof(code), "%06u", n);
		if (taken[0] != code && taken[1] != code && taken[2] != code)
			return code;
	}
}

// A server on a port the kernel chose, stopped however the case ends.
struct RunningServer
{
	InstalledDependencies wired;
	int                   port;

	RunningServer() : port(0)
	{
		httpd::ServerConfig c = httpd::defaultConfig();
		c.port = 0;
		c.bind_address = "127.0.0.1";
		REQUIRE(httpd::start(c));
		port = httpd::boundPort();
	}
	~RunningServer()
	{
		httpd::stop();
	}

	private:
		RunningServer(const RunningServer &);
		RunningServer &operator=(const RunningServer &);
};

inline bool crossOriginHeader(const std::vector<std::pair<std::string, std::string> > &headers)
{
	for (size_t i = 0; i < headers.size(); ++i)
	{
		std::string name = headers[i].first;
		for (size_t k = 0; k < name.size(); ++k)
		{
			if (name[k] >= 'A' && name[k] <= 'Z')
				name[k] = (char) (name[k] - 'A' + 'a');
		}
		if (name.compare(0, 15, "access-control-") == 0)
			return true;
	}
	return false;
}

// Answers what a case put in it and counts how often it was asked.
struct FakeFetcher : httpd::oauth::Fetcher
{
	bool        ok;
	std::string body;
	long        max_age;
	int         calls;
	std::string last;

	FakeFetcher() : ok(true), max_age(-1), calls(0) {}
	httpd::oauth::Fetched fetch(const std::string &url)
	{
		++calls;
		last = url;
		httpd::oauth::Fetched f;
		f.ok = ok;
		f.body = body;
		f.max_age = max_age;
		return f;
	}
};

struct FetcherInstalled
{
	explicit FetcherInstalled(httpd::oauth::Fetcher *f)
	{
		httpd::oauth::installFetcherForTest(f);
		httpd::oauth::forgetMetadataCacheForTest();
	}
	~FetcherInstalled()
	{
		httpd::oauth::installFetcherForTest(NULL);
		httpd::oauth::forgetMetadataCacheForTest();
		httpd::oauth::setCimdClockForTest(NULL);
	}
};

// A metadata document of a client that returns to the loopback.
inline std::string claudeCodeDocument(const std::string &id)
{
	return "{\"client_id\":\"" + id + "\",\"client_name\":\"Claude Code\","
	       "\"redirect_uris\":[\"http://localhost/callback\",\"http://127.0.0.1/callback\"],"
	       "\"grant_types\":[\"authorization_code\",\"refresh_token\"],"
	       "\"response_types\":[\"code\"],\"token_endpoint_auth_method\":\"none\"}";
}

// The documents ChatGPT and Claude publish, as fetched.
const char kChatGptId[] = "https://chatgpt.com/oauth/client.json";
const char kChatGptDocument[] =
	R"({"client_id":"https://chatgpt.com/oauth/client.json","client_uri":"https://chatgpt.com/","redirect_uris":["https://chatgpt.com/connector_platform_oauth_redirect"],"token_endpoint_auth_method":"private_key_jwt","token_endpoint_auth_methods_supported":["none","private_key_jwt"],"grant_types":["authorization_code","refresh_token"],"response_types":["code"],"client_name":"ChatGPT","logo_uri":"https://persistent.oaistatic.com/sonic/misc/openai-logo.png","token_endpoint_auth_signing_alg":"RS256","jwks_uri":"https://chatgpt.com/oauth/jwks.json"})";
const char kClaudeId[] = "https://claude.ai/oauth/mcp-oauth-client-metadata";
const char kClaudeDocument[] =
	R"({"client_id":"https://claude.ai/oauth/mcp-oauth-client-metadata","client_name":"Claude","client_uri":"https://claude.ai","redirect_uris":["https://claude.ai/api/mcp/auth_callback"],"grant_types":["authorization_code","refresh_token","urn:ietf:params:oauth:grant-type:jwt-bearer"],"response_types":["code"],"token_endpoint_auth_method":"none"})";

#endif
