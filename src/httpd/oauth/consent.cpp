/*
 * consent.cpp - the page where the box's owner signs in and decides
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

#include "httpd/oauth/consent.h"

#include "httpd/auth.h"
#include "httpd/credentials.h"
#include "httpd/mcp/toolgroups.h"
#include "httpd/oauth/authorize.h"
#include "httpd/oauth/scopes.h"
#include "httpd/oauth/store.h"
#include "httpd/oauth/surface.h"
#include "httpd/oauth/totp.h"
#include "httpd/oauth/twofactor.h"
#include "httpd/oauth/uri.h"
#include "httpd/status.h"
#include "httpd/webconfig.h"

#include <cstdio>
#include <cstring>
#include <utility>

#include <openssl/crypto.h>

namespace httpd
{
namespace oauth
{

namespace
{

const size_t kMaxPasswordBytes = 512;
const size_t kMaxUserBytes     = 256;
const size_t kMaxCodeBytes     = 16;

struct Texts
{
	const char *lang;
	const char *title;
	const char *wants;
	const char *returns_to;
	const char *loopback;
	const char *allow;
	const char *read;
	const char *write;
	const char *system;
	const char *offline;
	const char *user;
	const char *password;
	const char *approve;
	const char *deny;
	const char *wrong;
	const char *bad_scopes;
	const char *expired;
	const char *stale_form;
	const char *busy;
	const char *failed;
	const char *groups;
	const char *tool;
	const char *tools;
	const char *needs_system;
	const char *code;
	const char *wrong_code;
	const char *no_totp;
	const char *clock;
};

struct GroupWords
{
	const char *key;
	const char *de;
	const char *en;
};

const GroupWords kGroupWords[] = {
	{ "programme", "Programm: was l\xc3\xa4" "uft, Programmf\xc3\xbc" "hrer, Senderlogos",
	               "Programme: what is on, the guide, channel logos" },
	{ "timers", "Timer: anlegen, \xc3\xa4" "ndern, entfernen", "Timers: create, change, remove" },
	{ "recordings", "Aufnahmen: laufende und fertige, abspielen, l\xc3\xb6" "schen, Timeshift",
	                "Recordings: running and finished, play, delete, time shift" },
	{ "control", "Bedienung: umschalten, Lautst\xc3\xa4" "rke, Meldungen, Bildschirmfoto, Standby",
	             "Control: switch channels, volume, messages, screenshot, standby" },
	{ "bouquets", "Bouquets: anlegen, f\xc3\xbc" "llen, umbenennen, l\xc3\xb6" "schen",
	              "Bouquets: create, fill, rename, delete" },
	{ "status", "Box-Zustand: Box, Speicher, Empfang, Tuner, Plugins, Einstellungen lesen",
	            "Box status: box, storage, signal, tuners, plugins, read settings" },
	{ "settings", "Einstellungen \xc3\xa4" "ndern (nur freigegebene Bereiche)",
	              "Change settings (only the sections the owner allowed)" },
	{ "plugins", "Plugins starten (nur freigegebene)", "Start plugins (only the ones the owner allowed)" },
};

const char *groupWords(const char *key, bool de)
{
	for (size_t i = 0; i < sizeof(kGroupWords) / sizeof(kGroupWords[0]); ++i)
		if (std::strcmp(kGroupWords[i].key, key) == 0)
			return de ? kGroupWords[i].de : kGroupWords[i].en;
	return "";
}

const Texts kDe = {
	"de",
	"Zugriff auf die Box",
	" m\xc3\xb6" "chte auf diese Box zugreifen.",
	"Nach der Anmeldung geht es zur\xc3\xbc" "ck zu",
	"Diese Anwendung l\xc3\xa4" "uft auf einem Rechner und nicht als Webdienst. "
	"Erlaube den Zugriff nur, wenn du sie gerade selbst gestartet hast.",
	"Erlauben",
	"Lesen: Sender, Programm, Aufnahmen und Status",
	"Steuern: umschalten, Timer und Aufnahmen anlegen und entfernen",
	"System: Systemfunktionen wie Standby und Diagnose",
	"Angemeldet bleiben",
	"Benutzer",
	"Passwort",
	"Zugriff erlauben",
	"Ablehnen",
	"Benutzer oder Passwort stimmen nicht.",
	"W\xc3\xa4" "hle mindestens eine Berechtigung.",
	"Diese Anmeldeanfrage ist abgelaufen oder wurde schon beantwortet. "
	"Starte die Verbindung in der Anwendung neu.",
	"Das Formular ist nicht mehr g\xc3\xbc" "ltig. Lade die Seite neu.",
	"Zu viele Versuche. Warte einen Moment.",
	"Die Box konnte die Anmeldung nicht abschlie\xc3\x9f" "en.",
	"Werkzeuge",
	"Werkzeug",
	"Werkzeuge",
	"braucht System",
	"Code aus der Authenticator-App",
	"Benutzer, Passwort oder Code falsch.",
	"Zuerst im Heimnetz 2FA einrichten",
	"Die Uhrzeit der Box ist noch nicht gestellt.",
};

const Texts kEn = {
	"en",
	"Access to the box",
	" wants to access this box.",
	"After signing in you return to",
	"This application runs on a computer, not as a web service. "
	"Allow access only if you just started it yourself.",
	"Allow",
	"Read: channels, guide, recordings and status",
	"Control: switch channels, create and remove timers and recordings",
	"System: system functions such as standby and diagnostics",
	"Stay signed in",
	"User",
	"Password",
	"Allow access",
	"Deny",
	"The user or password is wrong.",
	"Choose at least one permission.",
	"This sign-in request has expired or was already answered. "
	"Start the connection again in the application.",
	"The form is no longer valid. Reload the page.",
	"Too many attempts. Wait a moment.",
	"The box could not complete the sign-in.",
	"Tools",
	"tool",
	"tools",
	"needs System",
	"Code from the authenticator app",
	"The user, password or code is wrong.",
	"Set up two-factor sign-in in the home network first",
	"The clock of the box is not set yet.",
};

const Texts &textsFor(Lang l)
{
	return (l == Lang::En) ? kEn : kDe;
}

// q in thousandths, parsed by hand: the process locale may not use a point.
int qValue(const std::string &params)
{
	const size_t at = params.find("q=");
	if (at == std::string::npos)
		return 1000;
	size_t i = at + 2;
	if (i < params.size() && params[i] == '1')
		return 1000;
	if (i >= params.size() || params[i] != '0')
		return 0;
	++i;
	int v = 0;
	int scale = 100;
	if (i < params.size() && params[i] == '.')
	{
		for (++i; i < params.size() && scale > 0 && params[i] >= '0' && params[i] <= '9'; ++i)
		{
			v += (params[i] - '0') * scale;
			scale /= 10;
		}
	}
	return v;
}

std::string trimmedLower(const std::string &s)
{
	size_t a = 0;
	size_t b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t'))
		++a;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t'))
		--b;
	std::string out = s.substr(a, b - a);
	for (size_t i = 0; i < out.size(); ++i)
	{
		if (out[i] >= 'A' && out[i] <= 'Z')
			out[i] = (char) (out[i] - 'A' + 'a');
	}
	return out;
}

bool isLanguage(const std::string &tag, const char *two)
{
	return tag.compare(0, 2, two) == 0 && (tag.size() == 2 || tag[2] == '-');
}

void addHeader(Response &r, const char *name, const std::string &value)
{
	r.headers.push_back(std::make_pair(std::string(name), value));
}

// The byte length of the well formed, minimal UTF-8 sequence starting at s[0] within the
// first "left" bytes, and the codepoint it encodes; 0 for anything short, truncated,
// overlong or otherwise not minimal, the same shape httpd::isUtf8 (json.h) accepts.
size_t decodedCodepoint(const unsigned char *s, size_t left, unsigned *codepoint)
{
	const unsigned char c = s[0];
	if (c < 0x80)
	{
		*codepoint = c;
		return 1;
	}
	size_t need = 0;
	unsigned char low = 0x80;
	unsigned char high = 0xbf;
	unsigned lead = 0;
	if (c >= 0xc2 && c <= 0xdf)
	{
		need = 2;
		lead = c & 0x1f;
	}
	else if (c == 0xe0)
	{
		need = 3;
		low = 0xa0;
		lead = c & 0x0f;
	}
	else if (c >= 0xe1 && c <= 0xec)
	{
		need = 3;
		lead = c & 0x0f;
	}
	else if (c == 0xed)
	{
		need = 3;
		high = 0x9f;
		lead = c & 0x0f;
	}
	else if (c >= 0xee && c <= 0xef)
	{
		need = 3;
		lead = c & 0x0f;
	}
	else if (c == 0xf0)
	{
		need = 4;
		low = 0x90;
		lead = c & 0x07;
	}
	else if (c >= 0xf1 && c <= 0xf3)
	{
		need = 4;
		lead = c & 0x07;
	}
	else if (c == 0xf4)
	{
		need = 4;
		high = 0x8f;
		lead = c & 0x07;
	}
	else
		return 0;
	if (left < need || s[1] < low || s[1] > high)
		return 0;
	for (size_t i = 2; i < need; ++i)
	{
		if (s[i] < 0x80 || s[i] > 0xbf)
			return 0;
	}
	unsigned cp = lead;
	for (size_t i = 1; i < need; ++i)
		cp = (cp << 6) | (unsigned) (s[i] & 0x3f);
	*codepoint = cp;
	return need;
}

bool isNeutralised(unsigned cp)
{
	if (cp >= 0x80 && cp <= 0x9f)
		return true;
	if (cp == 0x200e || cp == 0x200f)
		return true;
	if (cp >= 0x202a && cp <= 0x202e)
		return true;
	return cp >= 0x2066 && cp <= 0x2069;
}

// Drops C1 controls and bidi overrides/isolates from client-supplied text, decoded
// codepoint by codepoint, so a name cannot fake its reading direction or hide part of itself.
std::string sanitizeText(const std::string &s)
{
	std::string out;
	out.reserve(s.size());
	const unsigned char *p = (const unsigned char *) s.data();
	size_t i = 0;
	while (i < s.size())
	{
		unsigned cp = 0;
		const size_t len = decodedCodepoint(p + i, s.size() - i, &cp);
		if (len == 0)
		{
			out += '?';
			++i;
			continue;
		}
		if (!isNeutralised(cp))
			out.append(s, i, len);
		i += len;
	}
	return out;
}

// form-action also covers the redirect after the post; a loopback client may use any port.
std::string redirectOrigin(const std::string &redirect_uri)
{
	Url u;
	if (!parseUrl(redirect_uri, &u))
		return std::string();
	if (u.scheme == "http" && isLoopbackHost(u.host))
		return u.scheme + "://" + u.host + ":*";
	return u.scheme + "://" + u.host + (u.port.empty() ? std::string() : ":" + u.port);
}

Response page(int code, const std::string &body, const std::string &redirect_origin)
{
	Response r;
	r.code = code;
	r.content_type = "text/html; charset=utf-8";
	r.body = body;
	addHeader(r, "Cache-Control", "no-store");
	addHeader(r, "Pragma", "no-cache");
	addHeader(r, "X-Frame-Options", "DENY");
	addHeader(r, "Referrer-Policy", "no-referrer");
	addNoSniff(r);
	std::string csp = "default-src 'none'; style-src 'unsafe-inline'; form-action 'self'";
	if (!redirect_origin.empty())
		csp += " " + redirect_origin;
	csp += "; frame-ancestors 'none'; base-uri 'none'";
	addHeader(r, "Content-Security-Policy", csp);
	return r;
}

std::string head(const Texts &t)
{
	return std::string("<!DOCTYPE html><html lang=\"") + t.lang + "\"><head><meta charset=\"utf-8\">"
	       "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
	       "<title>" + t.title + "</title><style>"
	       "body{font-family:sans-serif;margin:0;padding:16px;background:#f4f4f4;color:#111}"
	       "main{max-width:28rem;margin:0 auto;background:#fff;padding:16px;border-radius:8px}"
	       "label{display:block;margin:.5rem 0}input[type=text],input[type=password]{width:100%;box-sizing:border-box;padding:.4rem}"
	       ".warn{background:#fff3cd;padding:.5rem}.err{background:#f8d7da;padding:.5rem}"
	       "button{margin:.75rem .5rem 0 0;padding:.5rem 1rem}"
	       "p,strong,label{overflow-wrap:anywhere}fieldset{min-width:0}"
	       "@media (max-width:599px){body{padding:8px}main{padding:12px}label{display:flex;align-items:center;gap:.5rem;min-height:44px}"
	       "button{min-height:44px;width:100%;margin:.5rem 0 0}}"
	       "@media (prefers-color-scheme:dark){body{background:#111;color:#eee}main{background:#222}"
	       ".warn{background:#4d3b00}.err{background:#5c1a20}}"
	       "</style></head><body><main><h1>" + t.title + "</h1>";
}

std::string tail()
{
	return "</main></body></html>";
}

std::string messagePage(const Texts &t, const char *message)
{
	return head(t) + "<p class=\"err\" role=\"alert\">" + message + "</p>" + tail();
}

void checkbox(std::string &out, const char *name, const char *label, bool ticked)
{
	out += "<label><input type=\"checkbox\" name=\"";
	out += name;
	out += ticked ? "\" checked> " : "\"> ";
	out += label;
	out += "</label>";
}

// Escapes HTML after sanitizeText; a client-supplied name can carry both kinds of attack.
std::string safeText(const std::string &s)
{
	return htmlEscape(sanitizeText(s));
}

// NULL on the first GET, where the defaults apply; the posted form on every re-render,
// where the owner's own ticks apply instead, including the ones left unticked.
bool postedOn(const Form *posted, const char *name, bool default_on)
{
	if (posted == NULL)
		return default_on;
	return formValue(*posted, name) == "on";
}

std::string formPage(const Texts &t, const PendingView &v, const char *message, const std::string &user,
                      bool ask_code, const Form *posted = NULL)
{
	std::string out = head(t);
	out += "<p><strong>" + safeText(v.client_name) + "</strong>" + t.wants + "</p>";
	out += std::string("<p>") + t.returns_to + " <strong>" + safeText(v.redirect_host) + "</strong></p>";
	if (v.loopback_only)
		out += std::string("<p class=\"warn\">") + t.loopback + "</p>";
	if (message != NULL)
		out += std::string("<p class=\"err\" role=\"alert\">") + message + "</p>";
	out += "<form method=\"post\" action=\"/oauth/consent\">";
	out += "<input type=\"hidden\" name=\"request\" value=\"" + htmlEscape(v.id) + "\">";
	out += "<input type=\"hidden\" name=\"csrf\" value=\"" + htmlEscape(v.form_token) + "\">";
	out += std::string("<fieldset><legend>") + t.allow + "</legend>";
	// Every level and staying signed in are offered, since clients ask only for read; the
	// requested levels start ticked except system, which the owner has to choose.
	checkbox(out, "scope_read", t.read, postedOn(posted, "scope_read", (v.requested & ScopeRead) != 0));
	checkbox(out, "scope_write", t.write, postedOn(posted, "scope_write", (v.requested & ScopeWrite) != 0));
	checkbox(out, "scope_system", t.system, postedOn(posted, "scope_system", false));
	checkbox(out, "scope_offline_access", t.offline, postedOn(posted, "scope_offline_access", true));
	out += "</fieldset>";
	out += std::string("<fieldset><legend>") + t.groups + "</legend>";
	{
		const AuthLevel reach = levelFor(ScopeLevels);
		const bool de = std::string(t.lang) == "de";
		size_t count = 0;
		const mcp::ToolGroup *table = mcp::toolGroups(&count);
		const std::vector<mcp::ToolDef> offered = mcp::boxTools().list();
		for (size_t i = 0; i < count; ++i)
		{
			const char *words = groupWords(table[i].key, de);
			const size_t n = mcp::toolsOfferedIn(table[i], offered, reach);
			char tally[48];
			std::snprintf(tally, sizeof(tally), " (%lu %s)", (unsigned long) n, n == 1 ? t.tool : t.tools);
			const std::string field = std::string("group_") + table[i].key;
			const bool default_on = (mcp::kDefaultGroups & table[i].bit) != 0;
			out += "<label><input type=\"checkbox\" name=\"" + field;
			out += postedOn(posted, field.c_str(), default_on) ? "\" checked> " : "\"> ";
			out += words;
			out += tally;
			if (table[i].least == AuthLevel::System)
				out += std::string(" <small>") + t.needs_system + "</small>";
			out += "</label>";
		}
	}
	out += "</fieldset>";
	out += std::string("<label>") + t.user + " <input type=\"text\" name=\"user\" autocomplete=\"username\" value=\"" +
	       safeText(user) + "\"></label>";
	out += std::string("<label>") + t.password +
	       " <input type=\"password\" name=\"password\" autocomplete=\"current-password\"></label>";
	if (ask_code)
		out += std::string("<label>") + t.code +
		       " <input type=\"text\" name=\"totp\" inputmode=\"numeric\" autocomplete=\"one-time-code\""
		       " maxlength=\"8\"></label>";
	out += std::string("<button type=\"submit\" name=\"decision\" value=\"approve\">") + t.approve + "</button>";
	out += std::string("<button type=\"submit\" name=\"decision\" value=\"deny\">") + t.deny + "</button>";
	out += "</form>";
	return out + tail();
}

std::string cookieFor(const std::string &token)
{
	return std::string(consentCookieName()) + "=" + token + "; Path=/oauth/; HttpOnly; SameSite=Strict; Secure";
}

std::string cookieGone()
{
	return std::string(consentCookieName()) + "=; Path=/oauth/; HttpOnly; SameSite=Strict; Secure; Max-Age=0";
}

bool sameSecret(const std::string &a, const std::string &b)
{
	return !a.empty() && a.size() == b.size() && CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

Response redirectAfterDecision(const std::string &url)
{
	Response r;
	r.code = kStatusFound;
	addHeader(r, "Location", url);
	addHeader(r, "Cache-Control", "no-store");
	addHeader(r, "Referrer-Policy", "no-referrer");
	addHeader(r, "Set-Cookie", cookieGone());
	return r;
}

Response showForm(const ConsentInput &in, const Texts &t)
{
	const bool ask_code = in.origin != Origin::Lan;
	if (ask_code && !totpActive())
		return page(StatusForbidden, messagePage(t, t.no_totp), std::string());
	Form q;
	PendingView v;
	if (!parseForm(in.query, &q) || !viewRequest(formValue(q, "request"), &v))
		return page(StatusBadRequest, messagePage(t, t.expired), std::string());
	Response r = page(StatusOk, formPage(t, v, NULL, std::string(), ask_code), redirectOrigin(v.redirect_uri));
	addHeader(r, "Set-Cookie", cookieFor(v.form_token));
	return r;
}

Response takeDecision(const ConsentInput &in, const Texts &t)
{
	const bool ask_code = in.origin != Origin::Lan;
	if (ask_code && !totpActive())
		return page(StatusForbidden, messagePage(t, t.no_totp), std::string());
	if (!contentTypeIs(in.content_type, "application/x-www-form-urlencoded"))
		return page(StatusUnsupportedMedia, messagePage(t, t.stale_form), std::string());
	Form f;
	PendingView v;
	if (!parseForm(in.body, &f) || !viewRequest(formValue(f, "request"), &v))
		return page(StatusBadRequest, messagePage(t, t.expired), std::string());
	const std::string origin = redirectOrigin(v.redirect_uri);
	if (!sameSecret(formValue(f, "csrf"), v.form_token) || !sameSecret(in.cookie, v.form_token))
		return page(StatusForbidden, messagePage(t, t.stale_form), std::string());

	std::string url;
	const std::string &decision = formValue(f, "decision");
	if (decision == "deny")
	{
		if (decideRequest(v.id, false, 0, std::string(), &url) != Decided::Redirect)
			return page(StatusBadRequest, messagePage(t, t.expired), std::string());
		return redirectAfterDecision(url);
	}
	if (decision != "approve")
		return page(StatusBadRequest, formPage(t, v, t.stale_form, std::string(), ask_code, &f), origin);

	// Before the throttle: an unset clock is the box's fault and no guess.
	if (ask_code && totpNow() < kTotpClockFloor)
		return page(StatusServiceUnavailable, formPage(t, v, t.clock, std::string(), ask_code, &f), origin);

	unsigned retry_after = 0;
	if (beginLoginAttempt(in.peer, &retry_after) != LoginAttempt::Open)
	{
		Response r = page(StatusTooManyRequests, formPage(t, v, t.busy, std::string(), ask_code, &f), origin);
		char seconds[24];
		std::snprintf(seconds, sizeof(seconds), "%u", retry_after);
		addHeader(r, "Retry-After", seconds);
		return r;
	}
	AttemptHeld held(in.peer);

	const WebConfig &cfg = config();
	const std::string &user = formValue(f, "user");
	const std::string &password = formValue(f, "password");
	const std::string &code = formValue(f, "totp");
	const bool short_enough = user.size() <= kMaxUserBytes && password.size() <= kMaxPasswordBytes &&
	                          code.size() <= kMaxCodeBytes;
	// Every half is always asked, so no half answers faster for being wrong.
	const bool name_ok = short_enough && !cfg.username.empty() && user == cfg.username;
	const bool secret_ok = short_enough && verifySecret(password, cfg.password_hash);
	// The code is used up only once the password is right.
	const bool code_ok = !ask_code || (short_enough && useTotpCode(code, name_ok && secret_ok) == CodeUse::Accepted);
	if (!name_ok || !secret_ok || !code_ok)
		return page(StatusUnauthorized, formPage(t, v, ask_code ? t.wrong_code : t.wrong,
		                                         short_enough ? user : std::string(), ask_code, &f), origin);
	held.grant();

	unsigned granted = 0;
	granted |= (formValue(f, "scope_read") == "on") ? ScopeRead : 0u;
	granted |= (formValue(f, "scope_write") == "on") ? ScopeWrite : 0u;
	granted |= (formValue(f, "scope_system") == "on") ? ScopeSystem : 0u;
	granted |= (formValue(f, "scope_offline_access") == "on") ? ScopeOffline : 0u;

	unsigned groups = 0;
	size_t count = 0;
	const mcp::ToolGroup *table = mcp::toolGroups(&count);
	for (size_t i = 0; i < count; ++i)
	{
		if (formValue(f, (std::string("group_") + table[i].key).c_str()) == "on")
			groups |= table[i].bit;
	}

	switch (decideRequest(v.id, true, granted, cfg.username, &url, groups))
	{
		case Decided::Redirect:
			return redirectAfterDecision(url);
		case Decided::BadScopes:
			return page(StatusBadRequest, formPage(t, v, t.bad_scopes, user, ask_code, &f), origin);
		case Decided::NoSuchRequest:
			return page(StatusBadRequest, messagePage(t, t.expired), std::string());
		case Decided::Failed:
			break;
	}
	return page(StatusInternalServerError, messagePage(t, t.failed), std::string());
}

} // namespace

std::string htmlEscape(const std::string &s)
{
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); ++i)
	{
		switch (s[i])
		{
			case '&':
				out += "&amp;";
				break;
			case '<':
				out += "&lt;";
				break;
			case '>':
				out += "&gt;";
				break;
			case '"':
				out += "&quot;";
				break;
			case '\'':
				out += "&#39;";
				break;
			default:
				out += s[i];
		}
	}
	return out;
}

Lang pickLanguage(const std::string &accept_language)
{
	Lang best = Lang::De;
	int best_q = -1;
	size_t i = 0;
	while (i <= accept_language.size())
	{
		size_t end = accept_language.find(',', i);
		if (end == std::string::npos)
			end = accept_language.size();
		const std::string entry = accept_language.substr(i, end - i);
		i = end + 1;
		const size_t semi = entry.find(';');
		const std::string tag = trimmedLower(entry.substr(0, semi));
		const int q = (semi == std::string::npos) ? 1000 : qValue(entry.substr(semi + 1));
		const bool de = isLanguage(tag, "de");
		const bool en = isLanguage(tag, "en");
		if ((de || en) && q > 0 && q > best_q)
		{
			best = de ? Lang::De : Lang::En;
			best_q = q;
		}
	}
	return best;
}

Response answerConsent(const ConsentInput &in)
{
	const Texts &t = textsFor(pickLanguage(in.accept_language));
	if (in.method == Get || in.method == Head)
		return showForm(in, t);
	if (in.method == Post)
		return takeDecision(in, t);
	Response r = page(StatusMethodNotAllowed, messagePage(t, t.stale_form), std::string());
	addHeader(r, "Allow", "GET, POST");
	return r;
}

} // namespace oauth
} // namespace httpd
