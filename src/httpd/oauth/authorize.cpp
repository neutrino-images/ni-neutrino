/*
 * authorize.cpp - the authorization endpoint, requests awaiting consent, and codes
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

#include "httpd/oauth/authorize.h"

#include "httpd/credentials.h"
#include "httpd/http.h"
#include "httpd/mcp/contract.h"
#include "httpd/oauth/cimd.h"
#include "httpd/oauth/scopes.h"
#include "httpd/oauth/surface.h"
#include "httpd/oauth/tokens.h"
#include "httpd/oauth/uri.h"
#include "httpd/status.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <utility>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace httpd
{
namespace oauth
{

namespace
{

typedef OpenThreads::ScopedLock<OpenThreads::Mutex> Held;
typedef std::vector<std::pair<std::string, std::string> > Params;

time_t (*clock_)() = &realClock;

// What an open request carries in its signed id instead of a table entry.
struct Signed
{
	time_t      expires;
	std::string nonce;
	std::string client_id;
	ClientKind  kind;
	std::string redirect_uri;
	std::string state;
	std::string challenge;
	unsigned    requested;
	std::string resource;
	std::string base;
};

struct Pending
{
	PendingView view;
	std::vector<std::string> redirect_uris;
	std::string state;
	std::string challenge;
	std::string nonce;
	time_t      expires;
};

struct CodeRec
{
	CodeGrant   grant;
	time_t      expires;
	bool        used;
	time_t      forget_at;
	std::string grant_id;
};

OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

// Mixed into every signature, so an id from before a restart is no longer open; renewed by the test reset.
std::string &epoch()
{
	static std::string e = randomToken(16);
	return e;
}

// Nonces of requests already answered, until their id would have expired anyway.
std::map<std::string, time_t> &answered()
{
	static std::map<std::string, time_t> a;
	return a;
}

std::map<std::string, CodeRec> &codes()
{
	static std::map<std::string, CodeRec> c;
	return c;
}

void sweepLocked(time_t now)
{
	for (std::map<std::string, time_t>::iterator it = answered().begin(); it != answered().end();)
	{
		if (it->second <= now)
			answered().erase(it++);
		else
			++it;
	}
	for (std::map<std::string, CodeRec>::iterator it = codes().begin(); it != codes().end();)
	{
		const bool gone = it->second.used ? it->second.forget_at <= now : it->second.expires <= now;
		if (gone)
			codes().erase(it++);
		else
			++it;
	}
}

// Shown in the browser instead of following a redirect nobody vouched for.
Response errorPage(int code, const std::string &detail)
{
	Response r;
	r.code = code;
	r.content_type = "text/plain; charset=utf-8";
	r.body = "Diese Anmeldung kann nicht abgeschlossen werden.\n"
	         "This sign-in cannot be completed.\n\n" + detail + "\n";
	r.headers.push_back(std::make_pair(std::string("Cache-Control"), std::string("no-store")));
	addNoSniff(r);
	r.headers.push_back(std::make_pair(std::string("X-Frame-Options"), std::string("DENY")));
	return r;
}

Response redirectTo(const std::string &url)
{
	Response r;
	r.code = kStatusFound;
	r.headers.push_back(std::make_pair(std::string("Location"), url));
	r.headers.push_back(std::make_pair(std::string("Cache-Control"), std::string("no-store")));
	r.headers.push_back(std::make_pair(std::string("Referrer-Policy"), std::string("no-referrer")));
	return r;
}

// RFC 9207 section 2: iss on every authorization response, errors included.
std::string answerUrl(const std::string &redirect_uri, Params params, const std::string &state,
                      const std::string &base)
{
	if (!state.empty())
		params.push_back(std::make_pair(std::string("state"), state));
	params.push_back(std::make_pair(std::string("iss"), base));
	return withQuery(redirect_uri, params);
}

Response sendBack(const std::string &redirect_uri, const char *error, const char *description,
                  const std::string &state, const std::string &base)
{
	Params p;
	p.push_back(std::make_pair(std::string("error"), std::string(error)));
	p.push_back(std::make_pair(std::string("error_description"), std::string(description)));
	return redirectTo(answerUrl(redirect_uri, p, state, base));
}

template <typename Map>
void evictSoonest(Map &m)
{
	typename Map::iterator soonest = m.begin();
	for (typename Map::iterator it = m.begin(); it != m.end(); ++it)
	{
		if (it->second.expires < soonest->second.expires)
			soonest = it;
	}
	if (soonest != m.end())
		m.erase(soonest);
}

void field(std::string &out, const std::string &v)
{
	char len[24];
	std::snprintf(len, sizeof(len), "%lu:", (unsigned long) v.size());
	out += len;
	out += v;
}

bool readField(const std::string &in, size_t *at, std::string *out)
{
	const size_t colon = in.find(':', *at);
	if (colon == std::string::npos || colon == *at || colon - *at > 6)
		return false;
	size_t n = 0;
	for (size_t i = *at; i < colon; ++i)
	{
		if (in[i] < '0' || in[i] > '9')
			return false;
		n = n * 10 + (size_t) (in[i] - '0');
	}
	if (n > in.size() - colon - 1)
		return false;
	*out = in.substr(colon + 1, n);
	*at = colon + 1 + n;
	return true;
}

bool readNumber(const std::string &text, unsigned long long *out)
{
	if (text.empty() || text.size() > 19)
		return false;
	unsigned long long n = 0;
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] < '0' || text[i] > '9')
			return false;
		n = n * 10 + (unsigned long long) (text[i] - '0');
	}
	*out = n;
	return true;
}

const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string b64(const std::string &in)
{
	std::string out;
	size_t i = 0;
	for (; i + 2 < in.size(); i += 3)
	{
		const unsigned v = ((unsigned char) in[i] << 16) | ((unsigned char) in[i + 1] << 8) | (unsigned char) in[i + 2];
		out += kB64[(v >> 18) & 63];
		out += kB64[(v >> 12) & 63];
		out += kB64[(v >> 6) & 63];
		out += kB64[v & 63];
	}
	if (i + 1 == in.size())
	{
		const unsigned v = (unsigned char) in[i] << 16;
		out += kB64[(v >> 18) & 63];
		out += kB64[(v >> 12) & 63];
	}
	else if (i + 2 == in.size())
	{
		const unsigned v = ((unsigned char) in[i] << 16) | ((unsigned char) in[i + 1] << 8);
		out += kB64[(v >> 18) & 63];
		out += kB64[(v >> 12) & 63];
		out += kB64[(v >> 6) & 63];
	}
	return out;
}

int b64Value(char c)
{
	const char *at = c == '\0' ? NULL : (const char *) std::memchr(kB64, c, 64);
	return at == NULL ? -1 : (int) (at - kB64);
}

// Only the spelling b64 writes: no padding, no stray bits in the last character.
bool unb64(const std::string &in, std::string *out)
{
	out->clear();
	if (in.size() % 4 == 1)
		return false;
	unsigned acc = 0;
	int bits = 0;
	for (size_t i = 0; i < in.size(); ++i)
	{
		const int v = b64Value(in[i]);
		if (v < 0)
			return false;
		acc = (acc << 6) | (unsigned) v;
		bits += 6;
		if (bits >= 8)
		{
			bits -= 8;
			*out += (char) ((acc >> bits) & 0xff);
		}
	}
	return (acc & ((1u << bits) - 1)) == 0;
}

std::string hexOf(const std::string &raw)
{
	static const char d[] = "0123456789abcdef";
	std::string out;
	for (size_t i = 0; i < raw.size(); ++i)
	{
		out += d[(unsigned char) raw[i] >> 4];
		out += d[(unsigned char) raw[i] & 15];
	}
	return out;
}

std::string mac(const std::string &key, const std::string &label, const std::string &data)
{
	unsigned char out[EVP_MAX_MD_SIZE];
	unsigned int n = 0;
	const std::string in = label + data;
	if (key.empty() || HMAC(EVP_sha256(), key.data(), (int) key.size(), (const unsigned char *) in.data(),
	                        in.size(), out, &n) == NULL)
		return std::string();
	return std::string((const char *) out, n);
}

std::string currentEpoch()
{
	Held h(lock());
	return epoch();
}

std::string sign(const Signed &r)
{
	const std::string key = store().requestKey();
	std::string payload;
	char num[24];
	std::snprintf(num, sizeof(num), "%lld", (long long) r.expires);
	field(payload, num);
	field(payload, r.nonce);
	field(payload, r.client_id);
	field(payload, kindName(r.kind));
	field(payload, r.redirect_uri);
	field(payload, r.state);
	field(payload, r.challenge);
	std::snprintf(num, sizeof(num), "%u", r.requested);
	field(payload, num);
	field(payload, r.resource);
	field(payload, r.base);
	const std::string m = mac(key, "request:" + currentEpoch(), payload);
	if (m.empty())
		return std::string();
	return b64(payload) + "." + b64(m);
}

bool unsign(const std::string &id, time_t now, Signed *r)
{
	const size_t dot = id.find('.');
	std::string payload;
	std::string m;
	if (dot == std::string::npos || !unb64(id.substr(0, dot), &payload) || !unb64(id.substr(dot + 1), &m))
		return false;
	const std::string want = mac(store().requestKey(), "request:" + currentEpoch(), payload);
	if (want.empty() || m.size() != want.size() || CRYPTO_memcmp(m.data(), want.data(), m.size()) != 0)
		return false;

	std::string f[10];
	size_t at = 0;
	for (size_t i = 0; i < 10; ++i)
	{
		if (!readField(payload, &at, &f[i]))
			return false;
	}
	unsigned long long expires = 0;
	unsigned long long requested = 0;
	if (at != payload.size() || !readNumber(f[0], &expires) || !readNumber(f[7], &requested))
		return false;
	if ((time_t) expires <= now)
		return false;
	r->expires = (time_t) expires;
	r->nonce = f[1];
	r->client_id = f[2];
	if (f[3] == kindName(ClientKind::Registered))
		r->kind = ClientKind::Registered;
	else if (f[3] == kindName(ClientKind::Metadata))
		r->kind = ClientKind::Metadata;
	else
		return false;
	r->redirect_uri = f[4];
	r->state = f[5];
	r->challenge = f[6];
	r->requested = (unsigned) requested;
	r->resource = f[8];
	r->base = f[9];
	return true;
}

// The client as it stands now, so a client removed since the request was opened is gone with it.
bool currentClient(const std::string &client_id, ClientKind kind, std::string *name,
                   std::vector<std::string> *registered)
{
	if (kind == ClientKind::Registered)
	{
		Client known;
		if (!store().findClient(client_id, &known) || known.kind != ClientKind::Registered)
			return false;
		*name = known.name;
		*registered = known.redirect_uris;
		return true;
	}
	MetadataClient m;
	if (resolveMetadataClient(client_id, &m) != Resolve::Ok)
		return false;
	*name = m.name;
	*registered = m.redirect_uris;
	return true;
}

// Signed, unexpired and its client still standing. Without the lock: a metadata client may be fetched.
bool openRequest(const std::string &id, time_t now, Pending *out)
{
	Signed r;
	if (!unsign(id, now, &r))
		return false;
	std::string name;
	std::vector<std::string> registered;
	if (!currentClient(r.client_id, r.kind, &name, &registered))
		return false;
	bool matched = false;
	for (size_t i = 0; i < registered.size() && !matched; ++i)
		matched = redirectUriMatches(registered[i], r.redirect_uri);
	if (!matched)
		return false;

	PendingView &v = out->view;
	v.id = id;
	v.form_token = hexOf(mac(store().requestKey(), "form:", r.nonce)).substr(0, 32);
	if (v.form_token.size() != 32)
		return false;
	v.client_id = r.client_id;
	v.client_name = name;
	v.kind = r.kind;
	v.redirect_uri = r.redirect_uri;
	Url u;
	v.redirect_host = parseUrl(r.redirect_uri, &u) ? u.host : std::string();
	v.loopback_only = true;
	for (size_t i = 0; i < registered.size(); ++i)
	{
		Url reg;
		v.loopback_only = v.loopback_only && parseUrl(registered[i], &reg) && isLoopbackHost(reg.host);
	}
	v.requested = r.requested;
	v.base = r.base;
	v.resource = r.resource;
	out->redirect_uris = registered;
	out->state = r.state;
	out->challenge = r.challenge;
	out->nonce = r.nonce;
	out->expires = r.expires;
	return true;
}

void markAnsweredLocked(const Pending &pd)
{
	if (answered().size() >= kMaxAnswered)
	{
		std::map<std::string, time_t>::iterator soonest = answered().begin();
		for (std::map<std::string, time_t>::iterator it = answered().begin(); it != answered().end(); ++it)
		{
			if (it->second < soonest->second)
				soonest = it;
		}
		answered().erase(soonest);
	}
	answered()[pd.nonce] = pd.expires;
}

} // namespace

Response answerAuthorize(const std::string &query, const std::string &base)
{
	Form p;
	if (!parseForm(query, &p))
		return errorPage(StatusBadRequest, "The request repeats or garbles a parameter.");

	const std::string &client_id = formValue(p, "client_id");
	if (client_id.empty())
		return errorPage(StatusBadRequest, "The request names no client.");

	ClientKind kind = ClientKind::Registered;
	std::string name;
	std::vector<std::string> registered;
	Client known;
	if (store().findClient(client_id, &known) && known.kind == ClientKind::Registered)
	{
		name = known.name;
		registered = known.redirect_uris;
	}
	else if (cimdUrlAcceptable(client_id))
	{
		MetadataClient m;
		switch (resolveMetadataClient(client_id, &m))
		{
			case Resolve::Ok:
				break;
			case Resolve::Busy:
				return errorPage(StatusServiceUnavailable, "The box is busy; try again in a moment.");
			case Resolve::BadUrl:
			case Resolve::Unreachable:
			case Resolve::Invalid:
				return errorPage(StatusBadRequest, "The client's metadata document could not be used.");
		}
		kind = ClientKind::Metadata;
		name = m.name;
		registered = m.redirect_uris;
	}
	else
		return errorPage(StatusBadRequest, "This client is not known here.");

	const std::string &redirect_uri = formValue(p, "redirect_uri");
	bool matched = false;
	for (size_t i = 0; i < registered.size() && !matched; ++i)
		matched = redirectUriMatches(registered[i], redirect_uri);
	if (!matched)
		return errorPage(StatusBadRequest, "The return address is not one this client registered.");

	const std::string &state = formValue(p, "state");
	if (state.size() > kMaxStateBytes)
		return sendBack(redirect_uri, "invalid_request", "state is too long", std::string(), base);
	if (formValue(p, "response_type") != "code")
		return sendBack(redirect_uri, "unsupported_response_type", "only code is offered", state, base);
	const std::string &challenge = formValue(p, "code_challenge");
	if (formValue(p, "code_challenge_method") != "S256" || !challengeWellFormed(challenge))
		return sendBack(redirect_uri, "invalid_request", "PKCE with S256 is required", state, base);

	unsigned requested = ScopeRead;
	if (p.count("scope") != 0 && !parseScopes(formValue(p, "scope"), &requested))
		return sendBack(redirect_uri, "invalid_scope", "a requested scope is not offered", state, base);
	requested = withImplied(requested);
	if ((requested & ScopeLevels) == 0)
		requested |= ScopeRead;

	const std::string resource = mcp::resourceOf(base);
	if (p.count("resource") != 0 && formValue(p, "resource") != resource)
		return sendBack(redirect_uri, "invalid_target", "the resource is not this server", state, base);

	Signed sr;
	sr.nonce = newId();
	sr.client_id = client_id;
	sr.kind = kind;
	sr.redirect_uri = redirect_uri;
	sr.state = state;
	sr.challenge = challenge;
	sr.requested = requested;
	sr.resource = resource;
	sr.base = base;
	sr.expires = clock_() + kPendingLifetime;
	const std::string id = sr.nonce.empty() ? std::string() : sign(sr);
	if (id.empty())
		return sendBack(redirect_uri, "server_error", "no request could be opened", state, base);
	return redirectTo(base + "/oauth/consent?request=" + id);
}

bool viewRequest(const std::string &id, PendingView *out)
{
	const time_t now = clock_();
	Pending pd;
	if (!openRequest(id, now, &pd))
		return false;
	Held h(lock());
	sweepLocked(now);
	if (answered().count(pd.nonce) != 0)
		return false;
	*out = pd.view;
	return true;
}

Decided decideRequest(const std::string &id, bool approve, unsigned granted,
                      const std::string &user, std::string *redirect, unsigned groups)
{
	const time_t now = clock_();
	Pending pd;
	if (!openRequest(id, now, &pd))
		return Decided::NoSuchRequest;
	Held h(lock());
	sweepLocked(now);
	if (answered().count(pd.nonce) != 0)
		return Decided::NoSuchRequest;

	if (!approve)
	{
		Params p;
		p.push_back(std::make_pair(std::string("error"), std::string("access_denied")));
		*redirect = answerUrl(pd.view.redirect_uri, p, pd.state, pd.view.base);
		markAnsweredLocked(pd);
		return Decided::Redirect;
	}

	const unsigned g = withImplied(granted);
	if ((g & ScopeLevels) == 0 || (g & ~(pd.view.requested | ScopeLevels | ScopeOffline)) != 0)
		return Decided::BadScopes;

	const AuthLevel reach = levelFor(g);
	unsigned kept = 0;
	size_t count = 0;
	const mcp::ToolGroup *table = mcp::toolGroups(&count);
	for (size_t i = 0; i < count; ++i)
	{
		if ((groups & table[i].bit) != 0 && (int) table[i].least <= (int) reach)
			kept |= table[i].bit;
	}

	const std::string code = mintToken(TokenKind::Code);
	const std::string hash = tokenHash(code);
	if (code.empty() || hash.empty())
		return Decided::Failed;

	CodeRec rec;
	rec.grant.client_id = pd.view.client_id;
	rec.grant.client_name = pd.view.client_name;
	rec.grant.redirect_uri = pd.view.redirect_uri;
	rec.grant.challenge = pd.challenge;
	rec.grant.resource = pd.view.resource;
	rec.grant.user = user;
	rec.grant.redirect_uris = pd.redirect_uris;
	rec.grant.kind = pd.view.kind;
	rec.grant.scopes = g;
	rec.grant.groups = kept;
	rec.expires = now + kCodeLifetime;
	rec.used = false;
	rec.forget_at = 0;
	if (codes().size() >= kMaxCodes)
		evictSoonest(codes());
	codes()[hash] = rec;

	Params p;
	p.push_back(std::make_pair(std::string("code"), code));
	*redirect = answerUrl(pd.view.redirect_uri, p, pd.state, pd.view.base);
	markAnsweredLocked(pd);
	return Decided::Redirect;
}

Redeem redeemCode(const std::string &code, CodeGrant *out, std::string *grant_of_replay)
{
	if (!looksLike(code, TokenKind::Code))
		return Redeem::Unknown;
	Held h(lock());
	const time_t now = clock_();
	sweepLocked(now);
	std::map<std::string, CodeRec>::iterator it = codes().find(tokenHash(code));
	if (it == codes().end())
		return Redeem::Unknown;
	if (it->second.used)
	{
		*grant_of_replay = it->second.grant_id;
		return Redeem::Replayed;
	}
	it->second.used = true;
	it->second.forget_at = now + kUsedCodeMemory;
	*out = it->second.grant;
	return Redeem::Fresh;
}

void bindCodeToGrant(const std::string &code, const std::string &grant_id)
{
	Held h(lock());
	std::map<std::string, CodeRec>::iterator it = codes().find(tokenHash(code));
	if (it != codes().end())
		it->second.grant_id = grant_id;
}

void forgetAuthorizationStateForTest()
{
	Held h(lock());
	answered().clear();
	codes().clear();
	epoch() = randomToken(16);
}

void setAuthorizeClockForTest(time_t (*clock)())
{
	clock_ = (clock != NULL) ? clock : &realClock;
}

} // namespace oauth
} // namespace httpd
