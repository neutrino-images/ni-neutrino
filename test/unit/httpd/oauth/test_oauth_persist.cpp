/*
 * test_oauth_persist.cpp - tests for the OAuth store on disk
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
#include "httpd/oauth/oauthtest.h"
#include "httpd/oauth/scopes.h"
#include "httpd/oauth/store.h"
#include "httpd/oauth/tokens.h"
#include "httpd/oauth/uri.h"

#include <cstdio>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

using namespace httpd::oauth;

namespace
{

std::string slurp(const std::string &path)
{
	std::string out;
	FILE *f = std::fopen(path.c_str(), "r");
	if (f == NULL)
		return out;
	char buf[4096];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
		out.append(buf, n);
	std::fclose(f);
	return out;
}

void spill(const std::string &path, const std::string &text)
{
	FILE *f = std::fopen(path.c_str(), "w");
	REQUIRE(f != NULL);
	std::fwrite(text.data(), 1, text.size(), f);
	std::fclose(f);
}

struct Cleanup
{
	std::string path;
	explicit Cleanup(const std::string &p) : path(p)
	{
	}
	~Cleanup()
	{
		::unlink(path.c_str());
		::unlink((path + ".bad").c_str());
		::unlink((path + ".new").c_str());
	}
};

time_t gNow = 1000000;

time_t testNow()
{
	return gNow;
}

time_t lastUseOf(Store &s, const std::string &client_id)
{
	const std::vector<Client> all = s.listClients();
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].client_id == client_id)
			return all[i].last_used;
	}
	return -1;
}

std::vector<std::string> redirects()
{
	return std::vector<std::string>(1, "https://claude.ai/api/mcp/auth_callback");
}

} // namespace

TEST_CASE("a missing store file is an empty store", "[oauth-persist]")
{
	const std::string path = oauthTempPath("missing");
	Cleanup c(path);
	Store s;
	REQUIRE(s.open(path));
	REQUIRE(s.listClients().empty());
	REQUIRE(::access(path.c_str(), F_OK) != 0);
}

TEST_CASE("tokens survive a restart and only their digests are on disk", "[oauth-persist]")
{
	const std::string path = oauthTempPath("restart");
	Cleanup cl(path);
	Issued out;
	Client st;
	std::string static_token;
	std::string client_id;
	{
		Store s;
		REQUIRE(s.open(path));
		Client c;
		REQUIRE(s.registerClient("Claude", redirects(), &c));
		client_id = c.client_id;
		REQUIRE(s.issue(c, "root", ScopeRead | ScopeWrite | ScopeOffline, "https://tv.example.org/mcp", &out, NULL));
		REQUIRE(s.createStatic("HA", ScopeRead, "root", &st, &static_token));
	}

	struct stat sb;
	REQUIRE(::stat(path.c_str(), &sb) == 0);
	REQUIRE((sb.st_mode & 0777) == 0600);

	const std::string text = slurp(path);
	REQUIRE(text.compare(0, 14, "ni-web-oauth 2") == 0);
	REQUIRE(text.find(out.access_token) == std::string::npos);
	REQUIRE(text.find(out.refresh_token) == std::string::npos);
	REQUIRE(text.find(static_token) == std::string::npos);
	REQUIRE(text.find(out.access_token.substr(4)) == std::string::npos);
	REQUIRE(text.find(tokenHash(out.access_token)) != std::string::npos);

	Store again;
	REQUIRE(again.open(path));
	TokenFacts t;
	REQUIRE(again.checkToken(out.access_token, &t));
	REQUIRE(t.resource == "https://tv.example.org/mcp");
	REQUIRE(again.checkToken(static_token, &t));
	Issued next;
	REQUIRE(again.refresh(out.refresh_token, client_id, "https://tv.example.org/mcp", 0, &next) == RefreshOutcome::Issued);
}

TEST_CASE("a garbled store is set aside and the next write is readable", "[oauth-persist]")
{
	const std::string path = oauthTempPath("garbled");
	Cleanup cl(path);
	spill(path, "ni-web-oauth 2\nC this line is not a client\n");

	Store s;
	REQUIRE_FALSE(s.open(path));
	REQUIRE(::access((path + ".bad").c_str(), F_OK) == 0);
	REQUIRE(s.listClients().empty());

	Client c;
	REQUIRE(s.registerClient("Claude", redirects(), &c));
	Store again;
	REQUIRE(again.open(path));
	Client back;
	REQUIRE(again.findClient(c.client_id, &back));
}

TEST_CASE("a store cut off mid line is refused whole", "[oauth-persist]")
{
	const std::string path = oauthTempPath("cut");
	Cleanup cl(path);
	{
		Store s;
		REQUIRE(s.open(path));
		Client c;
		REQUIRE(s.registerClient("Claude", redirects(), &c));
		Issued out;
		REQUIRE(s.issue(c, "root", ScopeRead | ScopeOffline, "https://tv.example.org/mcp", &out, NULL));
	}
	const std::string whole = slurp(path);
	spill(path, whole.substr(0, whole.size() - 20));
	Store again;
	REQUIRE_FALSE(again.open(path));
	REQUIRE(again.grantCountForTest() == 0u);
}

TEST_CASE("a token naming a grant that is not there refuses the file", "[oauth-persist]")
{
	const std::string path = oauthTempPath("orphan");
	Cleanup cl(path);
	spill(path, "ni-web-oauth 2\nA " + std::string(64, 'a') + " " + std::string(32, 'b') + " 1 1800000000 1700000000\n");
	Store s;
	REQUIRE_FALSE(s.open(path));
}

TEST_CASE("a file of another version is refused", "[oauth-persist]")
{
	const std::string path = oauthTempPath("version");
	Cleanup cl(path);
	spill(path, "ni-web-oauth 1\n");
	Store s;
	REQUIRE_FALSE(s.open(path));
}

TEST_CASE("a store full of clients at their caps is still a readable file", "[oauth-persist]")
{
	const std::string path = oauthTempPath("full");
	Cleanup cl(path);
	const std::string name(kMaxClientNameBytes, 'n');
	std::vector<std::string> uris;
	for (size_t i = 0; i < kMaxRedirectUris; ++i)
		uris.push_back(std::string(kMaxRedirectUriBytes, 'u'));

	std::vector<std::string> client_ids;
	{
		Store s;
		REQUIRE(s.open(path));
		for (size_t i = 0; i < kMaxRegistered; ++i)
		{
			Client c;
			REQUIRE(s.registerClient(name, uris, &c));
			client_ids.push_back(c.client_id);
		}
	}

	REQUIRE(::access((path + ".bad").c_str(), F_OK) != 0);

	Store again;
	REQUIRE(again.open(path));
	for (size_t i = 0; i < client_ids.size(); ++i)
	{
		Client back;
		REQUIRE(again.findClient(client_ids[i], &back));
	}
}

TEST_CASE("a last use reaches the file only once it is an hour newer than the stored one", "[oauth-persist]")
{
	const std::string path = oauthTempPath("lastuse");
	Cleanup cl(path);
	gNow = 1000000;
	Store s;
	s.setClock(&testNow);
	REQUIRE(s.open(path));
	Client st;
	std::string token;
	REQUIRE(s.createStatic("HA", ScopeRead, "root", &st, &token));
	TokenFacts t;

	gNow += 60;
	REQUIRE(s.checkToken(token, &t));
	const std::string first = slurp(path);
	{
		Store again;
		REQUIRE(again.open(path));
		CHECK(lastUseOf(again, st.client_id) == 1000060);
	}

	gNow += 60;
	REQUIRE(s.checkToken(token, &t));
	gNow += 3540;
	REQUIRE(s.checkToken(token, &t));
	CHECK(slurp(path) == first);

	gNow += 1;
	REQUIRE(s.checkToken(token, &t));
	CHECK(slurp(path) != first);
	Store again;
	REQUIRE(again.open(path));
	CHECK(lastUseOf(again, st.client_id) == 1003661);
}

TEST_CASE("a clean shutdown writes the last use the hour held back", "[oauth-persist]")
{
	const std::string path = oauthTempPath("shutdown");
	Cleanup cl(path);
	gNow = 2000000;
	Store s;
	s.setClock(&testNow);
	REQUIRE(s.open(path));
	Client c;
	REQUIRE(s.registerClient("Claude", redirects(), &c));
	Issued out;
	REQUIRE(s.issue(c, "root", ScopeRead, "https://tv.example.org/mcp", &out, NULL));
	const std::string issued = slurp(path);

	gNow += 600;
	TokenFacts t;
	REQUIRE(s.checkToken(out.access_token, &t));
	CHECK(slurp(path) == issued);

	s.saveUse();
	CHECK(slurp(path) != issued);
	Store again;
	again.setClock(&testNow);
	REQUIRE(again.open(path));
	CHECK(lastUseOf(again, c.client_id) == 2000600);
}

TEST_CASE("a store that cannot be written tries a last use once an hour too", "[oauth-persist]")
{
	gNow = 3000000;
	Store s;
	s.setClock(&testNow);
	REQUIRE(s.open("/nonexistent-ni-oauth-dir/store"));
	Client st;
	std::string token;
	REQUIRE(s.createStatic("HA", ScopeRead, "root", &st, &token));
	TokenFacts t;

	gNow += 60;
	REQUIRE(s.checkToken(token, &t));
	const size_t tried = s.saveAttemptsForTest();
	gNow += 60;
	REQUIRE(s.checkToken(token, &t));
	gNow += 3540;
	REQUIRE(s.checkToken(token, &t));
	CHECK(s.saveAttemptsForTest() == tried);
	gNow += 1;
	REQUIRE(s.checkToken(token, &t));
	CHECK(s.saveAttemptsForTest() == tried + 1);
}

TEST_CASE("a clock that went back never lowers a last use", "[oauth-persist]")
{
	gNow = 4000000;
	Store s;
	s.setClock(&testNow);
	REQUIRE(s.open(std::string()));
	Client st;
	std::string token;
	REQUIRE(s.createStatic("HA", ScopeRead, "root", &st, &token));
	TokenFacts t;
	REQUIRE(s.checkToken(token, &t));
	gNow = 100;
	REQUIRE(s.checkToken(token, &t));
	CHECK(lastUseOf(s, st.client_id) == 4000000);
}

TEST_CASE("the key that signs open requests is made once and kept with the store", "[oauth-persist]")
{
	const std::string path = oauthTempPath("key");
	Cleanup cl(path);
	std::string key;
	{
		Store s;
		REQUIRE(s.open(path));
		key = s.requestKey();
		REQUIRE(key.size() == 32u);
		CHECK(s.requestKey() == key);
	}
	CHECK(slurp(path).find("\nK ") != std::string::npos);
	Store again;
	REQUIRE(again.open(path));
	CHECK(again.requestKey() == key);

	Store other;
	REQUIRE(other.open(std::string()));
	CHECK(other.requestKey().size() == 32u);
	CHECK(other.requestKey() != key);
}
