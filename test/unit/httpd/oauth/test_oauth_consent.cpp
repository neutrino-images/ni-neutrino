/*
 * test_oauth_consent.cpp - tests for the sign-in and consent page
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
#include "support/httpclient.h"
#include "httpd/auth.h"
#include "httpd/oauth/authorize.h"
#include "httpd/oauth/consent.h"
#include "httpd/oauth/oauthtest.h"
#include "httpd/oauth/scopes.h"
#include "httpd/oauth/store.h"
#include "httpd/oauth/surface.h"
#include "httpd/oauth/uri.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using namespace httpd;
using namespace httpd::oauth;

namespace
{

const char kBase[] = "https://tv.example.org";
const char kCallback[] = "https://claude.ai/api/mcp/auth_callback";
const char kChallenge[] = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM";

std::string header(const Response &r, const char *name)
{
	for (size_t i = 0; i < r.headers.size(); ++i)
	{
		if (r.headers[i].first == name)
			return r.headers[i].second;
	}
	return std::string();
}

bool has(const std::string &hay, const std::string &needle)
{
	return hay.find(needle) != std::string::npos;
}

std::string askFor(const Client &c, const char *scope, const std::string &redirect = kCallback)
{
	std::vector<std::pair<std::string, std::string> > p;
	p.push_back(std::make_pair(std::string("response_type"), std::string("code")));
	p.push_back(std::make_pair(std::string("client_id"), c.client_id));
	p.push_back(std::make_pair(std::string("redirect_uri"), redirect));
	p.push_back(std::make_pair(std::string("code_challenge"), std::string(kChallenge)));
	p.push_back(std::make_pair(std::string("code_challenge_method"), std::string("S256")));
	p.push_back(std::make_pair(std::string("state"), std::string("xyz")));
	p.push_back(std::make_pair(std::string("scope"), std::string(scope)));
	const std::string url = withQuery("", p);
	const Response r = answerAuthorize(url.substr(1), kBase);
	const std::string loc = header(r, "Location");
	const size_t at = loc.find("request=");
	REQUIRE(at != std::string::npos);
	return loc.substr(at + 8);
}

struct Fixture
{
	AccountConfigured account;
	Client            client;
	std::string       id;
	PendingView       view;

	explicit Fixture(const char *name = "Claude", const char *scope = "read write")
	{
		store().open(std::string());
		forgetAuthorizationStateForTest();
		REQUIRE(store().registerClient(name, std::vector<std::string>(1, kCallback), &client));
		id = askFor(client, scope);
		REQUIRE(viewRequest(id, &view));
	}

	ConsentInput get(const std::string &lang = std::string()) const
	{
		ConsentInput in;
		in.method = Get;
		in.query = "request=" + id;
		in.accept_language = lang;
		in.peer = "192.168.1.20";
		return in;
	}

	ConsentInput post(const std::string &form) const
	{
		ConsentInput in;
		in.method = Post;
		in.body = form;
		in.content_type = "application/x-www-form-urlencoded";
		in.cookie = view.form_token;
		in.peer = "192.168.1.20";
		return in;
	}

	std::string approve(const char *password, const char *scopes) const
	{
		return "request=" + id + "&csrf=" + view.form_token + "&user=root&password=" + password +
		       "&decision=approve" + scopes;
	}
};

} // namespace

TEST_CASE("the page shows the client the return host and every level", "[oauth-consent]")
{
	Fixture f;
	const Response r = answerConsent(f.get());
	REQUIRE(r.code == StatusOk);
	REQUIRE(r.content_type == "text/html; charset=utf-8");
	REQUIRE(has(r.body, "<html lang=\"de\">"));
	REQUIRE(has(r.body, "Claude"));
	REQUIRE(has(r.body, "claude.ai"));
	REQUIRE(has(r.body, "name=\"scope_read\""));
	REQUIRE(has(r.body, "name=\"scope_write\""));
	REQUIRE(has(r.body, "name=\"scope_system\">"));
	REQUIRE(has(r.body, "name=\"scope_offline_access\" checked>"));
	REQUIRE(has(r.body, "value=\"" + f.view.form_token + "\""));
	REQUIRE(header(r, "Set-Cookie") == std::string(consentCookieName()) + "=" + f.view.form_token +
	        "; Path=/oauth/; HttpOnly; SameSite=Strict; Secure");
}

TEST_CASE("system is offered unticked and every other requested scope ticked", "[oauth-consent]")
{
	Fixture f("Claude", "read write system offline_access");
	const Response r = answerConsent(f.get());
	REQUIRE(r.code == StatusOk);
	REQUIRE(has(r.body, "name=\"scope_read\" checked>"));
	REQUIRE(has(r.body, "name=\"scope_write\" checked>"));
	REQUIRE(has(r.body, "name=\"scope_offline_access\" checked>"));
	REQUIRE(has(r.body, "name=\"scope_system\">"));
	REQUIRE_FALSE(has(r.body, "name=\"scope_system\" checked"));
}

TEST_CASE("the first get shows the scope and group defaults", "[oauth-consent]")
{
	Fixture f;
	const Response r = answerConsent(f.get());
	REQUIRE(has(r.body, "name=\"group_programme\" checked>"));
	REQUIRE(has(r.body, "name=\"group_timers\" checked>"));
	REQUIRE(has(r.body, "name=\"group_recordings\" checked>"));
	REQUIRE(has(r.body, "name=\"group_control\">"));
	REQUIRE_FALSE(has(r.body, "name=\"group_control\" checked"));
}

TEST_CASE("a wrong password re-renders the form with what the owner posted, not the defaults", "[oauth-consent]")
{
	Fixture f;
	const std::string form = "request=" + f.id + "&csrf=" + f.view.form_token + "&user=root&password=wrong" +
	                          "&decision=approve&scope_read=on&group_control=on";
	const Response r = answerConsent(f.post(form));
	REQUIRE(r.code == StatusUnauthorized);
	REQUIRE(has(r.body, "name=\"scope_read\" checked>"));
	REQUIRE(has(r.body, "name=\"scope_write\">"));
	REQUIRE_FALSE(has(r.body, "name=\"scope_write\" checked"));
	REQUIRE(has(r.body, "name=\"group_control\" checked>"));
	REQUIRE(has(r.body, "name=\"group_timers\">"));
	REQUIRE_FALSE(has(r.body, "name=\"group_timers\" checked"));
}

TEST_CASE("a level only read asked for is offered unticked", "[oauth-consent]")
{
	Fixture f("Claude", "read");
	const Response r = answerConsent(f.get());
	REQUIRE(has(r.body, "name=\"scope_read\" checked>"));
	REQUIRE(has(r.body, "name=\"scope_write\">"));
	REQUIRE(has(r.body, "name=\"scope_system\">"));
}

TEST_CASE("a system group is offered with its hint and keeps a posted tick", "[oauth-consent]")
{
	Fixture f;
	const std::string form = "request=" + f.id + "&csrf=" + f.view.form_token + "&user=root&password=wrong" +
	                          "&decision=approve&scope_read=on&scope_write=on&group_settings=on";
	const Response r = answerConsent(f.post(form));
	REQUIRE(r.code == StatusUnauthorized);
	REQUIRE(has(r.body, "name=\"group_settings\" checked>"));
	REQUIRE(has(r.body, "braucht System"));
}

TEST_CASE("the page cannot be framed or cached and runs no script", "[oauth-consent]")
{
	Fixture f;
	const Response r = answerConsent(f.get());
	REQUIRE(header(r, "X-Frame-Options") == "DENY");
	const std::string csp = header(r, "Content-Security-Policy");
	REQUIRE(has(csp, "frame-ancestors 'none'"));
	REQUIRE(has(csp, "default-src 'none'"));
	REQUIRE(has(csp, "form-action 'self' https://claude.ai"));
	REQUIRE_FALSE(has(csp, "script-src"));
	REQUIRE(has(header(r, "Cache-Control"), "no-store"));
	REQUIRE(header(r, "Referrer-Policy") == "no-referrer");
	REQUIRE_FALSE(has(r.body, "<script"));
}

TEST_CASE("the language follows the browser and defaults to german", "[oauth-consent]")
{
	REQUIRE(pickLanguage("") == Lang::De);
	REQUIRE(pickLanguage("en-US,en;q=0.9") == Lang::En);
	REQUIRE(pickLanguage("fr, en;q=0.5, de;q=0.8") == Lang::De);
	REQUIRE(pickLanguage("en;q=0.2, de;q=0.1") == Lang::En);
	REQUIRE(pickLanguage("fr-FR") == Lang::De);
	REQUIRE(pickLanguage("DE-at") == Lang::De);
	REQUIRE(pickLanguage("dex, en") == Lang::En);
	Fixture f;
	const Response r = answerConsent(f.get("en-GB,en;q=0.9"));
	REQUIRE(has(r.body, "<html lang=\"en\">"));
	REQUIRE(has(r.body, "Allow access"));
}

TEST_CASE("names from a client are escaped", "[oauth-consent]")
{
	Fixture f("<script>alert(1)</script>\"'&");
	const Response r = answerConsent(f.get());
	REQUIRE_FALSE(has(r.body, "<script>"));
	REQUIRE(has(r.body, "&lt;script&gt;alert(1)&lt;/script&gt;&quot;&#39;&amp;"));
}

TEST_CASE("a client name hides no bidi mark nor isolate nor C1 control character", "[oauth-consent]")
{
	// U+202E (RLO), U+0080 (a C1 control), U+2066 (an isolate) and U+200E (LRM), each in its
	// own UTF-8 encoding, plus the two valid sequences E2 80 and C2 80 AE run together: a pass
	// that removed C2 80 out of the middle of E2 80 C2 80 AE and then rescanned would read the
	// leftover E2 80 AE as U+202E, so this has to come out as nothing readable either.
	Fixture f("Evil\xe2\x80\xae" "Corp\xc2\x80" "&Co\xe2\x81\xa6" "X\xe2\x80\x8e" "Y"
	          "Attack\xe2\x80\xc2\x80\xae" "End");
	const Response r = answerConsent(f.get());
	REQUIRE(r.code == StatusOk);
	REQUIRE_FALSE(has(r.body, "\xe2\x80\xae"));
	REQUIRE_FALSE(has(r.body, "\xc2\x80"));
	REQUIRE_FALSE(has(r.body, "\xe2\x81\xa6"));
	REQUIRE_FALSE(has(r.body, "\xe2\x80\x8e"));
	REQUIRE(has(r.body, "EvilCorp&amp;CoXYAttack???End"));
}

TEST_CASE("a client with only loopback redirects carries a warning", "[oauth-consent]")
{
	AccountConfigured account;
	store().open(std::string());
	forgetAuthorizationStateForTest();
	Client c;
	REQUIRE(store().registerClient("Local", std::vector<std::string>(1, "http://127.0.0.1/callback"), &c));
	ConsentInput in;
	in.method = Get;
	in.query = "request=" + askFor(c, "read", "http://127.0.0.1:5000/callback");
	const Response r = answerConsent(in);
	REQUIRE(r.code == StatusOk);
	REQUIRE(has(r.body, "class=\"warn\""));
	REQUIRE(has(header(r, "Content-Security-Policy"), "form-action 'self' http://127.0.0.1:*"));
}

TEST_CASE("approving with the right password redirects with a code", "[oauth-consent]")
{
	Fixture f;
	const Response r = answerConsent(f.post(f.approve(kAccountPassword, "&scope_read=on&scope_write=on")));
	REQUIRE(r.code == kStatusFound);
	const std::string loc = header(r, "Location");
	REQUIRE(loc.compare(0, std::string(kCallback).size(), kCallback) == 0);
	REQUIRE(has(loc, "code=nic_"));
	REQUIRE(has(loc, "state=xyz"));
	REQUIRE(has(header(r, "Set-Cookie"), "Max-Age=0"));
}

TEST_CASE("the form is refused without the matching cookie and token", "[oauth-consent]")
{
	Fixture f;
	// A second request, as an attacker who started their own flow would hold.
	Client other;
	REQUIRE(store().registerClient("Other", std::vector<std::string>(1, kCallback), &other));
	PendingView theirs;
	REQUIRE(viewRequest(askFor(other, "read"), &theirs));

	const std::string form = f.approve(kAccountPassword, "&scope_read=on");
	ConsentInput no_cookie = f.post(form);
	no_cookie.cookie.clear();
	ConsentInput wrong_cookie = f.post(form);
	wrong_cookie.cookie = theirs.form_token;
	ConsentInput no_field = f.post("request=" + f.id + "&user=root&password=sofa-2026&decision=approve&scope_read=on");
	ConsentInput foreign_pair = f.post("request=" + f.id + "&csrf=" + theirs.form_token +
	                                   "&user=root&password=sofa-2026&decision=approve&scope_read=on");
	foreign_pair.cookie = theirs.form_token;
	const ConsentInput tries[] = { no_cookie, wrong_cookie, no_field, foreign_pair };
	for (size_t i = 0; i < 4; ++i)
	{
		INFO(i);
		const Response r = answerConsent(tries[i]);
		REQUIRE(r.code == StatusForbidden);
		REQUIRE(header(r, "Location").empty());
	}
	PendingView still;
	REQUIRE(viewRequest(f.id, &still));
}

TEST_CASE("a wrong password grants nothing and keeps the request", "[oauth-consent]")
{
	Fixture f;
	const Response r = answerConsent(f.post(f.approve("wrong", "&scope_read=on")));
	REQUIRE(r.code == StatusUnauthorized);
	REQUIRE(has(r.body, "Benutzer oder Passwort"));
	REQUIRE(header(r, "Location").empty());
	PendingView still;
	REQUIRE(viewRequest(f.id, &still));
}

TEST_CASE("repeated wrong passwords are slowed down", "[oauth-consent]")
{
	Fixture f;
	setLoginClockForTest(1700000000);
	int first_refusal = -1;
	for (int i = 0; i < 12 && first_refusal < 0; ++i)
	{
		const Response r = answerConsent(f.post(f.approve("wrong", "&scope_read=on")));
		if (r.code == StatusTooManyRequests)
		{
			first_refusal = i;
			REQUIRE_FALSE(header(r, "Retry-After").empty());
		}
	}
	REQUIRE(first_refusal > 0);
	const Response right = answerConsent(f.post(f.approve(kAccountPassword, "&scope_read=on")));
	REQUIRE(right.code == StatusTooManyRequests);
	REQUIRE(header(right, "Location").empty());
}

TEST_CASE("nothing ticked, or staying signed in alone, grants nothing", "[oauth-consent]")
{
	Fixture f;
	const char *widen[] = { "&scope_offline_access=on", "" };
	for (size_t i = 0; i < 2; ++i)
	{
		INFO(widen[i]);
		const Response r = answerConsent(f.post(f.approve(kAccountPassword, widen[i])));
		REQUIRE(r.code == StatusBadRequest);
		REQUIRE(header(r, "Location").empty());
	}
	PendingView still;
	REQUIRE(viewRequest(f.id, &still));
}

TEST_CASE("a request already answered cannot be answered again", "[oauth-consent]")
{
	Fixture f;
	const std::string form = f.approve(kAccountPassword, "&scope_read=on");
	REQUIRE(answerConsent(f.post(form)).code == kStatusFound);
	const Response again = answerConsent(f.post(form));
	REQUIRE(again.code == StatusBadRequest);
	REQUIRE(header(again, "Location").empty());
	REQUIRE(has(again.body, "abgelaufen"));
	const Response deny = answerConsent(f.post("request=" + f.id + "&csrf=" + f.view.form_token + "&decision=deny"));
	REQUIRE(deny.code == StatusBadRequest);
}

TEST_CASE("denying needs the form token and no password", "[oauth-consent]")
{
	Fixture f;
	const Response r = answerConsent(f.post("request=" + f.id + "&csrf=" + f.view.form_token + "&decision=deny"));
	REQUIRE(r.code == kStatusFound);
	REQUIRE(has(header(r, "Location"), "error=access_denied"));
}

TEST_CASE("only a form post is taken", "[oauth-consent]")
{
	Fixture f;
	ConsentInput in = f.post(f.approve(kAccountPassword, "&scope_read=on"));
	in.content_type = "application/json";
	REQUIRE(answerConsent(in).code == StatusUnsupportedMedia);
}

TEST_CASE("the browser flow works over a real connection beside a live ni-web session", "[oauth-consent]")
{
	TunnelConfigured config;
	store().open(std::string());
	forgetAuthorizationStateForTest();
	RunningServer srv;
	Client c;
	REQUIRE(store().registerClient("Claude", std::vector<std::string>(1, kCallback), &c));
	const std::string session = openSession("root");
	REQUIRE_FALSE(session.empty());

	char path[512];
	std::snprintf(path, sizeof(path),
	              "/oauth/authorize?response_type=code&client_id=%s&redirect_uri=%s&code_challenge=%s"
	              "&code_challenge_method=S256&state=xyz&scope=read",
	              c.client_id.c_str(), formEncode(kCallback).c_str(), kChallenge);
	const testhttp::Reply a = testhttp::request(srv.port, "GET", path);
	REQUIRE(a.code == 302);
	const std::string loc = a.header("Location");
	const std::string consent = loc.substr(loc.find("/oauth/consent"));

	std::vector<std::pair<std::string, std::string> > h;
	h.push_back(std::make_pair(std::string("Cookie"), std::string(sessionCookieName()) + "=" + session));
	const testhttp::Reply page = testhttp::request(srv.port, "GET", consent, h);
	REQUIRE(page.code == 200);
	const std::string set = page.header("Set-Cookie");
	const std::string token = set.substr(set.find('=') + 1, 32);
	const std::string id = consent.substr(consent.find('=') + 1);

	h.clear();
	h.push_back(std::make_pair(std::string("Cookie"), std::string(sessionCookieName()) + "=" + session +
	                           "; " + consentCookieName() + "=" + token));
	h.push_back(std::make_pair(std::string("Content-Type"), std::string("application/x-www-form-urlencoded")));
	const testhttp::Reply done = testhttp::request(srv.port, "POST", "/oauth/consent", h,
		"request=" + id + "&csrf=" + token + "&user=root&password=sofa-2026&decision=approve&scope_read=on");
	REQUIRE(done.code == 302);
	REQUIRE(done.header("Location").find("code=nic_") != std::string::npos);
	closeSession(session);
}
