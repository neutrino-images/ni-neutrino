/*
 * test_oauth_cimd.cpp - tests for client id metadata documents
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
#include "httpd/oauth/cimd.h"
#include "httpd/oauth/oauthtest.h"
#include "httpd/oauth/store.h"

#include <cerrno>
#include <cstdio>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace httpd::oauth;

namespace
{

const char kId[] = "https://claude.ai/oauth/claude-code-client-metadata";

time_t g_now = 1700000000;
time_t fakeClock()
{
	return g_now;
}

// A loopback listener nothing should ever connect to.
struct Trap
{
	int fd;
	int port;
	Trap() : fd(-1), port(0)
	{
		fd = ::socket(AF_INET, SOCK_STREAM, 0);
		REQUIRE(fd >= 0);
		struct sockaddr_in a;
		a.sin_family = AF_INET;
		a.sin_port = 0;
		a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		REQUIRE(::bind(fd, (struct sockaddr *) &a, sizeof(a)) == 0);
		REQUIRE(::listen(fd, 4) == 0);
		socklen_t n = sizeof(a);
		REQUIRE(::getsockname(fd, (struct sockaddr *) &a, &n) == 0);
		port = ntohs(a.sin_port);
		::fcntl(fd, F_SETFL, O_NONBLOCK);
	}
	~Trap()
	{
		::close(fd);
	}
	bool touched() const
	{
		const int c = ::accept(fd, NULL, NULL);
		if (c >= 0)
		{
			::close(c);
			return true;
		}
		return errno != EAGAIN && errno != EWOULDBLOCK;
	}
};

std::string at(const char *scheme, const char *host, int port)
{
	char buf[128];
	std::snprintf(buf, sizeof(buf), "%s://%s:%d/client.json", scheme, host, port);
	return buf;
}

} // namespace

TEST_CASE("a metadata document naming its own url is a client", "[oauth-cimd]")
{
	FakeFetcher f;
	f.body = claudeCodeDocument(kId);
	FetcherInstalled in(&f);
	MetadataClient c;
	REQUIRE(resolveMetadataClient(kId, &c) == Resolve::Ok);
	REQUIRE(c.client_id == kId);
	REQUIRE(c.name == "Claude Code");
	REQUIRE(c.redirect_uris.size() == 2u);
	REQUIRE(f.last == kId);
}

TEST_CASE("a document with no client_name falls back to a capped host name", "[oauth-cimd]")
{
	const std::string host(160, 'a');
	const std::string id = "https://" + host + ".example/client-metadata";
	FakeFetcher f;
	f.body = "{\"client_id\":\"" + id + "\","
	         "\"redirect_uris\":[\"http://localhost/callback\"],"
	         "\"grant_types\":[\"authorization_code\",\"refresh_token\"],"
	         "\"response_types\":[\"code\"],\"token_endpoint_auth_method\":\"none\"}";
	FetcherInstalled in(&f);
	MetadataClient c;
	REQUIRE(resolveMetadataClient(id, &c) == Resolve::Ok);
	REQUIRE(c.name.size() <= kMaxClientNameBytes);
	REQUIRE(c.name == host.substr(0, kMaxClientNameBytes));
}

TEST_CASE("a document that is not about its own url or asks for a secret is refused", "[oauth-cimd]")
{
	const std::string bodies[] = {
		claudeCodeDocument("https://evil.example/client.json"),
		"{\"redirect_uris\":[\"http://localhost/callback\"]}",
		"{\"client_id\":\"" + std::string(kId) + "\",\"redirect_uris\":[\"http://localhost/callback\"],"
		"\"token_endpoint_auth_method\":\"client_secret_post\"}",
		"{\"client_id\":\"" + std::string(kId) + "\",\"redirect_uris\":[\"http://localhost/callback\"],"
		"\"token_endpoint_auth_method\":\"private_key_jwt\"}",
		"{\"client_id\":\"" + std::string(kId) + "\",\"redirect_uris\":[\"http://evil.example/cb\"]}",
		"{\"client_id\":\"" + std::string(kId) + "\",\"redirect_uris\":[\"https://a/b\"],\"pad\":\"" +
		std::string(kMaxMetadataBytes, 'x') + "\"}",
		"not json",
	};
	for (size_t i = 0; i < sizeof(bodies) / sizeof(bodies[0]); ++i)
	{
		INFO(i);
		FakeFetcher f;
		f.body = bodies[i];
		FetcherInstalled in(&f);
		MetadataClient c;
		REQUIRE(resolveMetadataClient(kId, &c) == Resolve::Invalid);
	}
}

TEST_CASE("the documents of chatgpt and claude are clients", "[oauth-cimd]")
{
	const char *ids[] = { kChatGptId, kClaudeId };
	const char *docs[] = { kChatGptDocument, kClaudeDocument };
	const char *names[] = { "ChatGPT", "Claude" };
	const char *returns[] = { "https://chatgpt.com/connector_platform_oauth_redirect",
	                          "https://claude.ai/api/mcp/auth_callback" };
	for (size_t i = 0; i < 2; ++i)
	{
		INFO(ids[i]);
		FakeFetcher f;
		f.body = docs[i];
		FetcherInstalled in(&f);
		MetadataClient c;
		REQUIRE(resolveMetadataClient(ids[i], &c) == Resolve::Ok);
		REQUIRE(c.client_id == ids[i]);
		REQUIRE(c.name == names[i]);
		REQUIRE(c.redirect_uris == std::vector<std::string>(1, returns[i]));
	}
}

TEST_CASE("a document that cannot be a public code client is refused", "[oauth-cimd]")
{
	const std::string head = "{\"client_id\":\"" + std::string(kId) + "\","
	                         "\"redirect_uris\":[\"https://claude.ai/cb\"]";
	const std::string bodies[] = {
		head + ",\"token_endpoint_auth_method\":\"private_key_jwt\","
		"\"token_endpoint_auth_methods_supported\":[\"private_key_jwt\"]}",
		head + ",\"token_endpoint_auth_methods_supported\":[\"private_key_jwt\"]}",
		head + ",\"token_endpoint_auth_method\":\"private_key_jwt\","
		"\"token_endpoint_auth_methods_supported\":[]}",
		head + ",\"token_endpoint_auth_method\":\"private_key_jwt\","
		"\"token_endpoint_auth_methods_supported\":\"none\"}",
		head + ",\"token_endpoint_auth_methods_supported\":[\"none\",\"" + std::string(65, 'n') + "\"]}",
		head + ",\"token_endpoint_auth_methods_supported\":[\"none\",\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\",\"h\"]}",
		head + ",\"grant_types\":[\"refresh_token\",\"urn:ietf:params:oauth:grant-type:jwt-bearer\"]}",
		head + ",\"grant_types\":[]}",
		head + ",\"grant_types\":[\"authorization_code\"],\"response_types\":[\"token\"]}",
		head + ",\"grant_types\":[\"authorization_code\"],\"response_types\":[\"code\",\"token\"]}",
	};
	for (size_t i = 0; i < sizeof(bodies) / sizeof(bodies[0]); ++i)
	{
		INFO(bodies[i]);
		FakeFetcher f;
		f.body = bodies[i];
		FetcherInstalled in(&f);
		MetadataClient c;
		REQUIRE(resolveMetadataClient(kId, &c) == Resolve::Invalid);
	}
}

TEST_CASE("a document may offer more grants than the code flow", "[oauth-cimd]")
{
	FakeFetcher f;
	f.body = "{\"client_id\":\"" + std::string(kId) + "\",\"redirect_uris\":[\"https://claude.ai/cb\"],"
	         "\"grant_types\":[\"authorization_code\",\"client_credentials\","
	         "\"urn:ietf:params:oauth:grant-type:jwt-bearer\"]}";
	FetcherInstalled in(&f);
	MetadataClient c;
	REQUIRE(resolveMetadataClient(kId, &c) == Resolve::Ok);
}

TEST_CASE("a url outside the draft's rules is never fetched", "[oauth-cimd]")
{
	FakeFetcher f;
	f.body = claudeCodeDocument("http://claude.ai/x");
	FetcherInstalled in(&f);
	MetadataClient c;
	REQUIRE(resolveMetadataClient("http://claude.ai/x", &c) == Resolve::BadUrl);
	REQUIRE(resolveMetadataClient("https://claude.ai/", &c) == Resolve::BadUrl);
	REQUIRE(resolveMetadataClient("https://claude.ai/a/../x", &c) == Resolve::BadUrl);
	REQUIRE(f.calls == 0);
}

TEST_CASE("documents are cached for their max age within bounds", "[oauth-cimd]")
{
	FakeFetcher f;
	f.body = claudeCodeDocument(kId);
	f.max_age = 120;
	FetcherInstalled in(&f);
	g_now = 1700000000;
	setCimdClockForTest(&fakeClock);
	MetadataClient c;
	REQUIRE(resolveMetadataClient(kId, &c) == Resolve::Ok);
	g_now += 119;
	REQUIRE(resolveMetadataClient(kId, &c) == Resolve::Ok);
	REQUIRE(f.calls == 1);
	g_now += 2;
	REQUIRE(resolveMetadataClient(kId, &c) == Resolve::Ok);
	REQUIRE(f.calls == 2);

	f.max_age = 0;
	forgetMetadataCacheForTest();
	REQUIRE(resolveMetadataClient(kId, &c) == Resolve::Ok);
	g_now += kMinCacheSeconds - 1;
	REQUIRE(resolveMetadataClient(kId, &c) == Resolve::Ok);
	REQUIRE(f.calls == 3);
}

TEST_CASE("a failed fetch is remembered for a minute", "[oauth-cimd]")
{
	FakeFetcher f;
	f.ok = false;
	FetcherInstalled in(&f);
	g_now = 1700000000;
	setCimdClockForTest(&fakeClock);
	MetadataClient c;
	REQUIRE(resolveMetadataClient(kId, &c) == Resolve::Unreachable);
	g_now += 30;
	REQUIRE(resolveMetadataClient(kId, &c) == Resolve::Unreachable);
	REQUIRE(f.calls == 1);
	g_now += kNegativeSeconds;
	f.ok = true;
	f.body = claudeCodeDocument(kId);
	REQUIRE(resolveMetadataClient(kId, &c) == Resolve::Ok);
	REQUIRE(f.calls == 2);
}

namespace
{

// Asks for another document from inside a fetch, so fetches overlap on one thread.
struct NestingFetcher : Fetcher
{
	std::vector<std::string> next;
	std::vector<Resolve>     seen;
	size_t                   depth;
	NestingFetcher() : depth(0) {}
	Fetched fetch(const std::string &)
	{
		if (depth < next.size())
		{
			MetadataClient c;
			const std::string url = next[depth++];
			seen.push_back(resolveMetadataClient(url, &c));
		}
		return Fetched();
	}
};

} // namespace

TEST_CASE("no more than two fetches run at once", "[oauth-cimd]")
{
	NestingFetcher f;
	f.next.push_back("https://b.example/c.json");
	f.next.push_back("https://c.example/c.json");
	FetcherInstalled in(&f);
	MetadataClient c;
	REQUIRE(resolveMetadataClient("https://a.example/c.json", &c) == Resolve::Unreachable);
	REQUIRE(f.seen.size() == 2u);
	REQUIRE(f.seen[0] == Resolve::Busy);
	REQUIRE(f.seen[1] == Resolve::Unreachable);
	// The slots were given back.
	REQUIRE(resolveMetadataClient("https://d.example/c.json", &c) == Resolve::Unreachable);
}

TEST_CASE("the real fetcher never connects to the loopback", "[oauth-cimd]")
{
	Trap t;
	const char *hosts[] = { "127.0.0.1", "localhost" };
	for (size_t i = 0; i < 2; ++i)
	{
		INFO(hosts[i]);
		const Fetched got = systemFetcher().fetch(at("https", hosts[i], t.port));
		REQUIRE_FALSE(got.ok);
		REQUIRE(got.why.find("address") != std::string::npos);
		REQUIRE_FALSE(t.touched());
	}
}

TEST_CASE("the real fetcher speaks https only", "[oauth-cimd]")
{
	Trap t;
	const Fetched got = systemFetcher().fetch(at("http", "127.0.0.1", t.port));
	REQUIRE_FALSE(got.ok);
	REQUIRE_FALSE(t.touched());
}
