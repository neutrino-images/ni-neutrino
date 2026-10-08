/*
 * test_oauth_groups.cpp - tests for the tool groups a connection carries
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

#include "httpd/mcp/allowlist.h"
#include "httpd/mcp/contract.h"
#include "httpd/mcp/toolgroups.h"
#include "httpd/oauth/authorize.h"
#include "httpd/oauth/consent.h"
#include "httpd/oauth/store.h"
#include "httpd/oauth/scopes.h"
#include "httpd/oauth/surface.h"
#include "httpd/oauth/token.h"
#include "httpd/oauth/uri.h"
#include "httpd/oauth/verify.h"
#include "httpd/router.h"

#include "support/answers.h"

#include "oauthtest.h"

#include <jsoncpp/json/json.h>

#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

using namespace httpd;

namespace
{

std::vector<std::string> splitLines(const std::string &text)
{
	std::vector<std::string> out;
	size_t i = 0;
	for (;;)
	{
		const size_t end = text.find('\n', i);
		if (end == std::string::npos)
		{
			out.push_back(text.substr(i));
			return out;
		}
		out.push_back(text.substr(i, end - i));
		i = end + 1;
	}
}

std::string joinLines(const std::vector<std::string> &lines)
{
	std::string out;
	for (size_t i = 0; i < lines.size(); ++i)
	{
		if (i != 0)
			out += '\n';
		out += lines[i];
	}
	return out;
}

// A path under /tmp this case owns alone; the destructor removes it and its ".bad".
struct TempFile
{
	std::string path_;

	TempFile() : path_(makePath())
	{
	}
	~TempFile()
	{
		::unlink(path_.c_str());
		::unlink((path_ + ".bad").c_str());
	}

	const std::string &path() const
	{
		return path_;
	}

	std::string text() const
	{
		std::string out;
		FILE *f = std::fopen(path_.c_str(), "r");
		if (f == NULL)
			return out;
		char buf[4096];
		size_t n;
		while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
			out.append(buf, n);
		std::fclose(f);
		return out;
	}

	void write(const std::string &text) const
	{
		FILE *f = std::fopen(path_.c_str(), "w");
		REQUIRE(f != NULL);
		std::fwrite(text.data(), 1, text.size(), f);
		std::fclose(f);
	}

	// Rewrites every line starting with "<kind> " without its last space-separated field.
	void dropLastFieldOf(char kind) const
	{
		std::vector<std::string> lines = splitLines(text());
		const std::string prefix = std::string(1, kind) + " ";
		for (size_t i = 0; i < lines.size(); ++i)
		{
			if (lines[i].compare(0, prefix.size(), prefix) != 0)
				continue;
			const size_t last = lines[i].rfind(' ');
			lines[i] = lines[i].substr(0, last);
		}
		write(joinLines(lines));
	}

	void setLastFieldOf(char kind, const std::string &value) const
	{
		std::vector<std::string> lines = splitLines(text());
		const std::string prefix = std::string(1, kind) + " ";
		for (size_t i = 0; i < lines.size(); ++i)
		{
			if (lines[i].compare(0, prefix.size(), prefix) != 0)
				continue;
			const size_t last = lines[i].rfind(' ');
			lines[i] = lines[i].substr(0, last + 1) + value;
		}
		write(joinLines(lines));
	}

	void setHeader(const std::string &header) const
	{
		std::vector<std::string> lines = splitLines(text());
		if (!lines.empty())
			lines[0] = header;
		write(joinLines(lines));
	}

	bool asideExists() const
	{
		return ::access((path_ + ".bad").c_str(), F_OK) == 0;
	}

	private:
		TempFile(const TempFile &);
		TempFile &operator=(const TempFile &);

		static std::string makePath()
		{
			char buf[] = "/tmp/ni-oauth-groups-XXXXXX";
			const int fd = ::mkstemp(buf);
			if (fd >= 0)
			{
				::close(fd);
				::unlink(buf);
			}
			return std::string(buf);
		}
};

unsigned groupsOfIn(oauth::Store &s, const std::string &access, const std::string &resource)
{
	const coreapi::Result<mcp::Caller> who = oauth::verifyAccessTokenIn(s, access, Origin::Tunnel, resource);
	REQUIRE(who.ok());
	return who.value().groups;
}

// A store (on disk or in memory) with one registered client, ready to issue grants.
struct OAuthBox
{
	oauth::Store  store;
	oauth::Client client;

	OAuthBox()
	{
		setup(std::string());
	}
	explicit OAuthBox(const std::string &path)
	{
		setup(path);
	}

	std::string resource() const
	{
		return "https://tv.example.org/mcp";
	}

	std::string clientKey() const
	{
		return client.key;
	}

	oauth::Issued grant(unsigned scopes, unsigned groups)
	{
		oauth::Issued out;
		REQUIRE(store.issue(client, "root", scopes, resource(), &out, NULL, groups));
		return out;
	}

	oauth::Issued refresh(const std::string &refresh_token)
	{
		oauth::Issued out;
		REQUIRE(store.refresh(refresh_token, client.client_id, resource(), 0, &out) ==
		        oauth::RefreshOutcome::Issued);
		return out;
	}

	unsigned groupsOf(const std::string &access)
	{
		return groupsOfIn(store, access, resource());
	}

	private:
		OAuthBox(const OAuthBox &);
		OAuthBox &operator=(const OAuthBox &);

		void setup(const std::string &path)
		{
			REQUIRE(store.open(path));
			REQUIRE(store.registerClient("Claude",
			        std::vector<std::string>(1, "https://claude.ai/api/mcp/auth_callback"), &client));
		}
};

const char kBase[] = "https://tv.example.org";
const char kCallback[] = "https://claude.ai/api/mcp/auth_callback";
const char kChallenge[] = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM";
const char kVerifier[] = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";

std::string header(const Response &r, const char *name)
{
	for (size_t i = 0; i < r.headers.size(); ++i)
	{
		if (r.headers[i].first == name)
			return r.headers[i].second;
	}
	return std::string();
}

std::string askFor(const oauth::Client &c, const std::string &scope)
{
	std::vector<std::pair<std::string, std::string> > p;
	p.push_back(std::make_pair(std::string("response_type"), std::string("code")));
	p.push_back(std::make_pair(std::string("client_id"), c.client_id));
	p.push_back(std::make_pair(std::string("redirect_uri"), std::string(kCallback)));
	p.push_back(std::make_pair(std::string("code_challenge"), std::string(kChallenge)));
	p.push_back(std::make_pair(std::string("code_challenge_method"), std::string("S256")));
	p.push_back(std::make_pair(std::string("state"), std::string("xyz")));
	p.push_back(std::make_pair(std::string("scope"), scope));
	const std::string url = oauth::withQuery("", p);
	const Response r = oauth::answerAuthorize(url.substr(1), kBase);
	const std::string loc = header(r, "Location");
	const size_t at = loc.find("request=");
	REQUIRE(at != std::string::npos);
	return loc.substr(at + 8);
}

// Registers a client, opens an authorize request for the given scopes, and carries it
// through the consent page to the tokens, as a browser following the flow would.
struct ConsentRun
{
	AccountConfigured  account;
	oauth::Client      client;
	std::string        id;
	oauth::PendingView view;

	explicit ConsentRun(unsigned requested)
	{
		REQUIRE(oauth::store().open(std::string()));
		oauth::forgetAuthorizationStateForTest();
		REQUIRE(oauth::store().registerClient("Claude", std::vector<std::string>(1, kCallback), &client));
		id = askFor(client, oauth::scopeString(oauth::withImplied(requested)));
		REQUIRE(oauth::viewRequest(id, &view));
	}

	std::string form() const
	{
		oauth::ConsentInput in;
		in.method = Get;
		in.query = "request=" + id;
		in.origin = Origin::Lan;
		return oauth::answerConsent(in).body;
	}

	// Exactly the checkbox fields the form ticks, as a browser would submit them.
	std::string untouchedForm() const
	{
		const std::string page = form();
		const std::string marker = " checked";
		std::string out;
		size_t i = 0;
		while ((i = page.find("name=\"", i)) != std::string::npos)
		{
			const size_t name_start = i + 6;
			const size_t name_end = page.find('"', name_start);
			REQUIRE(name_end != std::string::npos);
			const std::string name = page.substr(name_start, name_end - name_start);
			i = name_end + 1;
			if (page.compare(i, marker.size(), marker) == 0)
			{
				if (!out.empty())
					out += '&';
				out += name + "=on";
			}
		}
		return out;
	}

	oauth::Issued approve(const std::string &fields) const
	{
		oauth::ConsentInput in;
		in.method = Post;
		in.body = "request=" + id + "&csrf=" + view.form_token + "&user=root&password=" +
		          kAccountPassword + "&decision=approve&" + fields;
		in.content_type = "application/x-www-form-urlencoded";
		in.cookie = view.form_token;
		in.origin = Origin::Lan;
		const Response r = oauth::answerConsent(in);
		REQUIRE(r.code == oauth::kStatusFound);
		const std::string loc = header(r, "Location");
		oauth::Form back;
		REQUIRE(oauth::parseForm(loc.substr(loc.find('?') + 1), &back));

		std::vector<std::pair<std::string, std::string> > t;
		t.push_back(std::make_pair(std::string("grant_type"), std::string("authorization_code")));
		t.push_back(std::make_pair(std::string("code"), oauth::formValue(back, "code")));
		t.push_back(std::make_pair(std::string("redirect_uri"), std::string(kCallback)));
		t.push_back(std::make_pair(std::string("code_verifier"), std::string(kVerifier)));
		t.push_back(std::make_pair(std::string("client_id"), client.client_id));
		const Response tok = oauth::answerToken(oauth::withQuery("", t).substr(1),
		                                                "application/x-www-form-urlencoded", kBase);
		REQUIRE(tok.code == StatusOk);
		const ::Json::Value parsed = parsedJson(tok.body);
		oauth::Issued out;
		out.access_token = parsed["access_token"].asString();
		if (parsed.isMember("refresh_token"))
			out.refresh_token = parsed["refresh_token"].asString();
		return out;
	}

	unsigned groupsOf(const std::string &access) const
	{
		const coreapi::Result<mcp::Caller> who =
		    oauth::verifyAccessTokenIn(oauth::store(), access, Origin::Tunnel, mcp::resourceOf(kBase));
		REQUIRE(who.ok());
		return who.value().groups;
	}

	private:
		ConsentRun(const ConsentRun &);
		ConsentRun &operator=(const ConsentRun &);
};

} // namespace

TEST_CASE("a static token carries the groups it was made with", "[oauth][groups]")
{
	oauth::Store s;
	REQUIRE(s.open(std::string()));
	oauth::Client c;
	std::string token;
	REQUIRE(s.createStatic("ha", oauth::ScopeRead, "root", &c, &token));
	REQUIRE(c.groups == mcp::kDefaultGroups);
	oauth::Client d;
	std::string t2;
	REQUIRE(s.createStatic("all", oauth::ScopeRead, "root", &d, &t2, mcp::kAllGroups));
	coreapi::Result<mcp::Caller> who = oauth::verifyAccessTokenIn(s, t2, Origin::Lan, std::string());
	REQUIRE(who.ok());
	REQUIRE(who.value().groups == mcp::kAllGroups);
}

TEST_CASE("setting a connection's groups takes effect on its next request", "[oauth][groups]")
{
	oauth::Store s;
	REQUIRE(s.open(std::string()));
	oauth::Client c;
	std::string token;
	REQUIRE(s.createStatic("ha", oauth::ScopeRead, "root", &c, &token));
	REQUIRE(s.setGroups(c.key, mcp::GroupStatus));
	REQUIRE(oauth::verifyAccessTokenIn(s, token, Origin::Lan, std::string()).value().groups == mcp::GroupStatus);
	REQUIRE_FALSE(s.setGroups("00000000000000000000000000000000", mcp::GroupStatus));
}

TEST_CASE("a grant keeps its groups through a refresh and a narrowing reaches refreshed tokens", "[oauth][groups]")
{
	OAuthBox box;
	const oauth::Issued first = box.grant(oauth::ScopeRead | oauth::ScopeWrite | oauth::ScopeOffline,
	                                      mcp::GroupProgramme | mcp::GroupTimers);
	REQUIRE(box.groupsOf(first.access_token) == (mcp::GroupProgramme | mcp::GroupTimers));
	REQUIRE(box.store.setGroups(box.clientKey(), mcp::GroupProgramme));
	const oauth::Issued second = box.refresh(first.refresh_token);
	REQUIRE(box.groupsOf(second.access_token) == mcp::GroupProgramme);
	REQUIRE(box.groupsOf(first.access_token) == mcp::GroupProgramme);
}

TEST_CASE("a store file from before groups is set aside whole", "[oauth][groups]")
{
	TempFile f;
	OAuthBox box(f.path());
	const oauth::Issued issued = box.grant(oauth::ScopeRead | oauth::ScopeOffline, mcp::kAllGroups);
	oauth::Client c;
	std::string token;
	REQUIRE(box.store.createStatic("ha", oauth::ScopeRead, "root", &c, &token, mcp::GroupStatus));
	f.dropLastFieldOf('C');
	f.dropLastFieldOf('G');
	f.setHeader("ni-web-oauth 1");

	oauth::Store again;
	REQUIRE_FALSE(again.open(f.path()));
	REQUIRE(f.asideExists());
	REQUIRE(again.listClients().empty());
	REQUIRE_FALSE(oauth::verifyAccessTokenIn(again, token, Origin::Lan, std::string()).ok());
	REQUIRE_FALSE(oauth::verifyAccessTokenIn(again, issued.access_token, Origin::Tunnel, box.resource()).ok());
}

TEST_CASE("a line without its groups is refused under the new header too", "[oauth][groups]")
{
	{
		TempFile f;
		{
			oauth::Store s;
			REQUIRE(s.open(f.path()));
			oauth::Client c;
			std::string token;
			REQUIRE(s.createStatic("ha", oauth::ScopeRead, "root", &c, &token));
		}
		REQUIRE(f.text().compare(0, 15, "ni-web-oauth 2\n") == 0);
		f.dropLastFieldOf('C');
		oauth::Store again;
		REQUIRE_FALSE(again.open(f.path()));
		REQUIRE(f.asideExists());
		REQUIRE(again.listClients().empty());
	}
	{
		TempFile f;
		OAuthBox box(f.path());
		box.grant(oauth::ScopeRead, mcp::GroupStatus);
		f.dropLastFieldOf('G');
		oauth::Store again;
		REQUIRE_FALSE(again.open(f.path()));
		REQUIRE(f.asideExists());
		REQUIRE(again.listClients().empty());
	}
}

TEST_CASE("a store file naming a group bit the table lacks is not trusted", "[oauth][groups]")
{
	{
		TempFile f;
		{
			oauth::Store s;
			REQUIRE(s.open(f.path()));
			oauth::Client c;
			std::string token;
			REQUIRE(s.createStatic("ha", oauth::ScopeRead, "root", &c, &token));
		}
		f.setLastFieldOf('C', "256");
		oauth::Store again;
		REQUIRE_FALSE(again.open(f.path()));
		REQUIRE(f.asideExists());
		REQUIRE(again.listClients().empty());
	}
	{
		TempFile f;
		OAuthBox box(f.path());
		box.grant(oauth::ScopeRead, mcp::GroupStatus);
		f.setLastFieldOf('G', "256");
		oauth::Store again;
		REQUIRE_FALSE(again.open(f.path()));
		REQUIRE(f.asideExists());
		REQUIRE(again.listClients().empty());
	}
}

TEST_CASE("the consent form offers each group with its tool count and ticks the defaults", "[oauth][groups][consent]")
{
	ConsentRun run(oauth::ScopeRead | oauth::ScopeWrite);
	const std::string page = run.form();
	REQUIRE(page.find("name=\"group_programme\" checked") != std::string::npos);
	REQUIRE(page.find("name=\"group_timers\" checked") != std::string::npos);
	REQUIRE(page.find("name=\"group_recordings\" checked") != std::string::npos);
	REQUIRE(page.find("name=\"group_control\">") != std::string::npos);
	REQUIRE(page.find("name=\"group_settings\">") != std::string::npos);
	REQUIRE(page.find("(8 ") != std::string::npos);
}

namespace
{

// The text of one group's own <label>...</label>, so a count is read next to its group
// and not wherever else the same digit happens to appear on the page.
std::string labelFor(const std::string &page, const char *key)
{
	const size_t at = page.find(std::string("name=\"group_") + key);
	REQUIRE(at != std::string::npos);
	const size_t end = page.find("</label>", at);
	REQUIRE(end != std::string::npos);
	return page.substr(at, end - at);
}

} // namespace

TEST_CASE("a group counts its tools at every level, since any level can be ticked", "[oauth][groups][consent]")
{
	// control holds read tools (get_volume, screenshot, standby_state), write tools
	// (switch_channel, set_volume, set_mute, set_mode, show_message) and one system
	// tool (set_standby).
	ConsentRun run(oauth::ScopeRead);
	const std::string label = labelFor(run.form(), "control");
	REQUIRE(label.find("(9 ") != std::string::npos);
	REQUIRE(label.find("(3 ") == std::string::npos);
}

TEST_CASE("the plugins group counts start_plugin only when the plugin allowlist is not empty, singular right", "[oauth][groups][consent]")
{
	// AccountConfigured (inside ConsentRun) installs its own, empty allowlist; the
	// allowlist a case wants on the page has to be set after that, not before it.
	{
		ConsentRun run(oauth::ScopeSystem);
		REQUIRE(labelFor(run.form(), "plugins").find("(0 Werkzeuge)") != std::string::npos);
	}
	{
		ConsentRun run(oauth::ScopeSystem);
		mcp::Allowlists open;
		open.plugins.push_back("anything");
		mcp::installAllowlists(open);
		REQUIRE(labelFor(run.form(), "plugins").find("(1 Werkzeug)") != std::string::npos);
	}
}

TEST_CASE("the groups ticked on the consent page reach the tokens", "[oauth][groups][consent]")
{
	ConsentRun run(oauth::ScopeRead | oauth::ScopeWrite | oauth::ScopeOffline);
	const oauth::Issued got = run.approve("scope_read=on&scope_write=on&group_programme=on&group_bouquets=on");
	REQUIRE(run.groupsOf(got.access_token) == (mcp::GroupProgramme | mcp::GroupBouquets));
}

TEST_CASE("a group above the granted scope or unknown to the table is dropped", "[oauth][groups][consent]")
{
	ConsentRun run(oauth::ScopeRead);
	const oauth::Issued got = run.approve("scope_read=on&group_programme=on&group_settings=on&group_nonsense=on");
	REQUIRE(run.groupsOf(got.access_token) == mcp::GroupProgramme);
}

TEST_CASE("an untouched consent form grants the default groups", "[oauth][groups][consent]")
{
	ConsentRun run(oauth::ScopeRead);
	const oauth::Issued got = run.approve(run.untouchedForm());
	REQUIRE(run.groupsOf(got.access_token) == mcp::kDefaultGroups);
}

TEST_CASE("the consent page holds to a phone width", "[oauth][groups][consent]")
{
	ConsentRun run(oauth::ScopeRead);
	const std::string page = run.form();
	REQUIRE(page.find("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">") != std::string::npos);
	REQUIRE(page.find("@media (max-width:599px)") != std::string::npos);
	REQUIRE(page.find("min-height:44px") != std::string::npos);
	REQUIRE(page.find("overflow-wrap:anywhere") != std::string::npos);
}

namespace
{

// Opens the store fresh in memory for one case, the way the token API cases do.
struct ApiStore
{
	ApiStore()
	{
		REQUIRE(oauth::store().open(std::string()));
	}
};

Response api(Method m, const std::string &path, const std::string &body = std::string())
{
	return dispatch(m, path, std::string(), body, "192.168.1.9", AuthLevel::System);
}

std::string idOf(const Response &r)
{
	return parsedJson(r.body)["id"].asString();
}

} // namespace

TEST_CASE("the client list and the token form speak groups", "[oauth][groups][api]")
{
	ApiStore fresh;
	Response made = api(Post, "/api/v1/ai/clients", "{\"name\":\"HA\",\"scopes\":\"read\",\"groups\":\"status control\"}");
	REQUIRE(made.code == 201);
	REQUIRE(made.body.find("\"groups\":[\"control\",\"status\"]") != std::string::npos);
	Response plain = api(Post, "/api/v1/ai/clients", "{\"name\":\"Plain\",\"scopes\":\"read\"}");
	REQUIRE(plain.body.find("\"groups\":[\"programme\",\"timers\",\"recordings\"]") != std::string::npos);
	REQUIRE(api(Post, "/api/v1/ai/clients", "{\"name\":\"X\",\"scopes\":\"read\",\"groups\":\"nonsense\"}").code == 400);
	Response listed = api(Get, "/api/v1/ai/clients");
	REQUIRE(listed.body.find("\"groups\":[\"control\",\"status\"]") != std::string::npos);
}

TEST_CASE("an empty groups on create gives none and a body naming groups elsewhere does not", "[oauth][groups][api]")
{
	ApiStore fresh;
	Response none = api(Post, "/api/v1/ai/clients", "{\"name\":\"HA\",\"scopes\":\"read\",\"groups\":\"\"}");
	REQUIRE(none.code == 201);
	REQUIRE(none.body.find("\"groups\":[]") != std::string::npos);
	Response named = api(Post, "/api/v1/ai/clients", "{\"name\":\"groups\",\"scopes\":\"read\"}");
	REQUIRE(named.code == 201);
	REQUIRE(named.body.find("\"groups\":[\"programme\",\"timers\",\"recordings\"]") != std::string::npos);
}

TEST_CASE("a PATCH sets a connection's groups and refuses what it cannot read", "[oauth][groups][api]")
{
	ApiStore fresh;
	const std::string id = idOf(api(Post, "/api/v1/ai/clients", "{\"name\":\"HA\",\"scopes\":\"read\"}"));
	Response r = api(Patch, "/api/v1/ai/clients/" + id, "{\"groups\":\"bouquets programme bouquets\"}");
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find("\"groups\":[\"programme\",\"bouquets\"]") != std::string::npos);
	REQUIRE(api(Patch, "/api/v1/ai/clients/" + id, "{\"groups\":\"\"}").body.find("\"groups\":[]") != std::string::npos);
	Response commas = api(Patch, "/api/v1/ai/clients/" + id, "{\"groups\":\"programme,timers\"}");
	REQUIRE(commas.code == 400);
	REQUIRE(commas.body.find("unknown-group") != std::string::npos);
	REQUIRE(api(Patch, "/api/v1/ai/clients/" + id, "{}").code == 400);
	REQUIRE(api(Patch, "/api/v1/ai/clients/00000000000000000000000000000000", "{\"groups\":\"status\"}").code == 404);
}

TEST_CASE("the groups route answers each group's tools and least level and cost", "[oauth][groups][api]")
{
	Response r = api(Get, "/api/v1/ai/groups");
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find("\"key\":\"programme\"") != std::string::npos);
	REQUIRE(r.body.find("\"least\":\"system\"") != std::string::npos);
	REQUIRE(r.body.find("\"default\":true") != std::string::npos);
	REQUIRE(r.body.find("\"approx_tokens\":") != std::string::npos);
	REQUIRE(r.body.find("\"whats_on\"") != std::string::npos);

	const ::Json::Value parsed = parsedJson(r.body);
	const ::Json::Value &list = parsed["groups"];
	bool found = false;
	for (::Json::ArrayIndex i = 0; i < list.size(); ++i)
	{
		if (list[i]["key"].asString() != "programme")
			continue;
		found = true;
		REQUIRE(list[i]["approx_tokens"].asInt() > 0);
		const ::Json::Value &tools = list[i]["tools"];
		for (::Json::ArrayIndex j = 0; j < tools.size(); ++j)
			REQUIRE(tools[j].asString() != "set_standby");
	}
	REQUIRE(found);
}

TEST_CASE("the groups PATCH body example is accepted", "[oauth][groups][api]")
{
	ApiStore fresh;
	oauth::Client c;
	std::string token;
	REQUIRE(oauth::store().createStatic("ha", oauth::ScopeRead, "root", &c, &token));
	std::map<std::string, std::string> fills;
	fills["id"] = c.key;
	REQUIRE(sendBodyExample("PATCH", "/api/v1/ai/clients/{id}", fills) == 200);
}

TEST_CASE("a caller carries its grant, or a static client's key, as what its writes are made for", "[oauth][writer]")
{
	oauth::Store s;
	REQUIRE(s.open(std::string()));
	oauth::Client c;
	REQUIRE(s.registerClient("Claude", std::vector<std::string>(1, "https://claude.ai/api/mcp/auth_callback"), &c));
	oauth::Issued out;
	std::string grant;
	REQUIRE(s.issue(c, "root", oauth::ScopeRead, "https://tv.example.org/mcp", &out, &grant));
	REQUIRE_FALSE(grant.empty());
	const coreapi::Result<mcp::Caller> who =
		oauth::verifyAccessTokenIn(s, out.access_token, Origin::Tunnel, "https://tv.example.org/mcp");
	REQUIRE(who.ok());
	CHECK(who.value().connection == grant);

	oauth::Client st;
	std::string token;
	REQUIRE(s.createStatic("ha", oauth::ScopeRead, "root", &st, &token));
	const coreapi::Result<mcp::Caller> lan = oauth::verifyAccessTokenIn(s, token, Origin::Lan, std::string());
	REQUIRE(lan.ok());
	CHECK(lan.value().connection == lan.value().client_id);
	CHECK_FALSE(lan.value().connection.empty());
}
