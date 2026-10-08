/*
 * store.cpp - OAuth clients, grants and tokens, kept as digests on disk
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

#include "httpd/oauth/store.h"

#include "httpd/credentials.h"
#include "httpd/oauth/scopes.h"
#include "httpd/oauth/tokens.h"
#include "httpd/oauth/uri.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <OpenThreads/ScopedLock>

namespace httpd
{
namespace oauth
{

namespace
{

typedef OpenThreads::ScopedLock<OpenThreads::Mutex> Held;

const char   kHeader[]     = "ni-web-oauth 2";
// Worst case: 64 registered + 32 static + 256 metadata clients, each with 8 redirect
// uris of 512 bytes (hex doubled on disk), plus 256 grants with 8 access and 9 refresh
// records: around 4 MB. Well over doubled here for margin.
const size_t kMaxFileBytes = 16u << 20;
const time_t kSweepEvery   = 60;
// Flash wear: a use is written when the stored one is older than this.
const time_t kUseWriteStep = 3600;

std::string hexText(const std::string &s)
{
	if (s.empty())
		return "-";
	static const char d[] = "0123456789abcdef";
	std::string out;
	out.reserve(s.size() * 2);
	for (size_t i = 0; i < s.size(); ++i)
	{
		const unsigned char c = (unsigned char) s[i];
		out += d[c >> 4];
		out += d[c & 15];
	}
	return out;
}

int nibble(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

bool unhexText(const std::string &h, std::string *out)
{
	out->clear();
	if (h == "-")
		return true;
	if (h.empty() || h.size() % 2 != 0)
		return false;
	for (size_t i = 0; i < h.size(); i += 2)
	{
		const int a = nibble(h[i]);
		const int b = nibble(h[i + 1]);
		if (a < 0 || b < 0)
			return false;
		*out += (char) ((a << 4) | b);
	}
	return true;
}

bool isHexOf(const std::string &s, size_t n)
{
	if (s.size() != n)
		return false;
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (nibble(s[i]) < 0)
			return false;
	}
	return true;
}

bool readNumber(const std::string &s, long long *out)
{
	if (s.empty() || s.size() > 18)
		return false;
	long long v = 0;
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] < '0' || s[i] > '9')
			return false;
		v = v * 10 + (s[i] - '0');
	}
	*out = v;
	return true;
}

std::string num(long long v)
{
	char b[24];
	std::snprintf(b, sizeof(b), "%lld", v);
	return std::string(b);
}

std::vector<std::string> split(const std::string &s, char sep)
{
	std::vector<std::string> out;
	size_t i = 0;
	for (;;)
	{
		const size_t end = s.find(sep, i);
		if (end == std::string::npos)
		{
			out.push_back(s.substr(i));
			return out;
		}
		out.push_back(s.substr(i, end - i));
		i = end + 1;
	}
}

std::string joinLines(const std::vector<std::string> &v)
{
	std::string out;
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (i != 0)
			out += '\n';
		out += v[i];
	}
	return out;
}

char kindLetter(ClientKind k)
{
	switch (k)
	{
		case ClientKind::Registered:
			return 'R';
		case ClientKind::Metadata:
			return 'M';
		case ClientKind::Static:
			return 'S';
	}
	return '?';
}

bool kindFromLetter(const std::string &s, ClientKind *k)
{
	if (s == "R")
		*k = ClientKind::Registered;
	else if (s == "M")
		*k = ClientKind::Metadata;
	else if (s == "S")
		*k = ClientKind::Static;
	else
		return false;
	return true;
}

bool redirectsWithinCaps(const std::vector<std::string> &uris)
{
	if (uris.size() > kMaxRedirectUris)
		return false;
	for (size_t i = 0; i < uris.size(); ++i)
	{
		if (uris[i].size() > kMaxRedirectUriBytes)
			return false;
	}
	return true;
}

bool writeWhole(int fd, const std::string &text)
{
	size_t done = 0;
	while (done < text.size())
	{
		const ssize_t n = ::write(fd, text.data() + done, text.size() - done);
		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			return false;
		}
		done += (size_t) n;
	}
	return true;
}

// Old file or new file after a power cut, never half of one.
bool replaceAtomically(const std::string &path, const std::string &text)
{
	const std::string working = path + ".new";
	::unlink(working.c_str());
	const int fd = ::open(working.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, S_IRUSR | S_IWUSR);
	if (fd < 0)
		return false;
	bool ok = writeWhole(fd, text) && ::fsync(fd) == 0;
	if (::close(fd) != 0)
		ok = false;
	if (ok && ::rename(working.c_str(), path.c_str()) != 0)
		ok = false;
	if (!ok)
	{
		::unlink(working.c_str());
		return false;
	}
	const size_t slash = path.rfind('/');
	const std::string dir = (slash == std::string::npos) ? std::string(".")
	                        : (slash == 0 ? std::string("/") : path.substr(0, slash));
	const int dfd = ::open(dir.c_str(), O_RDONLY | O_CLOEXEC);
	if (dfd >= 0)
	{
		(void) ::fsync(dfd);
		::close(dfd);
	}
	return true;
}

// false with missing set: no file. false without: a file that cannot be read.
bool readSmallFile(const std::string &path, std::string *out, bool *missing)
{
	*missing = false;
	const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0)
	{
		*missing = (errno == ENOENT);
		return false;
	}
	bool ok = true;
	char buf[4096];
	for (;;)
	{
		const ssize_t n = ::read(fd, buf, sizeof(buf));
		if (n == 0)
			break;
		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			ok = false;
			break;
		}
		out->append(buf, (size_t) n);
		if (out->size() > kMaxFileBytes)
		{
			ok = false;
			break;
		}
	}
	::close(fd);
	return ok;
}

} // namespace

const char *kindName(ClientKind k)
{
	switch (k)
	{
		case ClientKind::Registered:
			return "registered";
		case ClientKind::Metadata:
			return "metadata";
		case ClientKind::Static:
			return "static";
	}
	return "registered";
}

time_t realClock()
{
	return ::time(NULL);
}

Store::Store() : clock_(&realClock), digest_(&tokenHash), last_sweep_(0), save_attempts_(0)
{
}

void Store::setClock(Clock c)
{
	Held h(lock_);
	clock_ = (c != NULL) ? c : &realClock;
	last_sweep_ = 0;
}

void Store::setDigestForTest(Digest d)
{
	Held h(lock_);
	digest_ = (d != NULL) ? d : &tokenHash;
}

time_t Store::now() const
{
	return clock_();
}

void Store::clearLocked()
{
	clients_.clear();
	grants_.clear();
	access_.clear();
	refresh_.clear();
	saved_client_use_.clear();
	saved_grant_use_.clear();
	request_key_.clear();
	last_sweep_ = 0;
}

bool Store::open(const std::string &path)
{
	Held h(lock_);
	clearLocked();
	path_ = path;
	if (path.empty())
		return true;

	std::string text;
	bool missing = false;
	if (readSmallFile(path, &text, &missing) && parse(text))
		return true;
	clearLocked();
	if (missing)
		return true;

	const std::string aside = path + ".bad";
	(void) ::rename(path.c_str(), aside.c_str());
	std::fprintf(stderr, "[ni-web oauth] %s could not be read and was moved to %s\n",
	             path.c_str(), aside.c_str());
	return false;
}

bool Store::parse(const std::string &text)
{
	const std::vector<std::string> lines = split(text, '\n');
	if (lines.empty() || lines[0] != kHeader)
		return false;
	// A file this writes ends in a newline, so a last piece that is not empty was cut.
	if (!lines.back().empty())
		return false;

	for (size_t i = 1; i + 1 < lines.size(); ++i)
	{
		const std::vector<std::string> f = split(lines[i], ' ');
		long long a = 0;
		long long b = 0;
		long long c = 0;
		if (f[0] == "C" && f.size() == 12)
		{
			Client cl;
			std::string redirects;
			long long d = 0;
			if (!isHexOf(f[1], 32) || !kindFromLetter(f[2], &cl.kind) ||
			    !readNumber(f[3], &a) || !readNumber(f[4], &b) || !readNumber(f[5], &c) ||
			    !unhexText(f[6], &cl.client_id) || !unhexText(f[7], &cl.name) ||
			    !unhexText(f[8], &cl.user) || !unhexText(f[10], &redirects) ||
			    !readNumber(f[11], &d) || d > mcp::kAllGroups)
				return false;
			if (f[9] != "-" && !isHexOf(f[9], 64))
				return false;
			cl.key = f[1];
			cl.created = (time_t) a;
			cl.last_used = (time_t) b;
			cl.scopes = (unsigned) c;
			cl.groups = (unsigned) d;
			cl.token_hash = (f[9] == "-") ? std::string() : f[9];
			if (!redirects.empty())
				cl.redirect_uris = split(redirects, '\n');
			if (cl.client_id.empty() || clients_.count(cl.client_id) != 0)
				return false;
			clients_[cl.client_id] = cl;
			saved_client_use_[cl.client_id] = cl.last_used;
		}
		else if (f[0] == "G" && f.size() == 9)
		{
			Grant g;
			long long d = 0;
			if (!isHexOf(f[1], 32) || !unhexText(f[2], &g.client_id) || !unhexText(f[3], &g.user) ||
			    !readNumber(f[4], &a) || !unhexText(f[5], &g.resource) ||
			    !readNumber(f[6], &b) || !readNumber(f[7], &c) ||
			    !readNumber(f[8], &d) || d > mcp::kAllGroups)
				return false;
			g.id = f[1];
			g.scopes = (unsigned) a;
			g.created = (time_t) b;
			g.last_used = (time_t) c;
			g.groups = (unsigned) d;
			grants_[g.id] = g;
			saved_grant_use_[g.id] = g.last_used;
		}
		else if (f[0] == "K" && f.size() == 2)
		{
			if (!isHexOf(f[1], 64) || !request_key_.empty() || !unhexText(f[1], &request_key_))
				return false;
		}
		else if (f[0] == "A" && f.size() == 6)
		{
			if (!isHexOf(f[1], 64) || !isHexOf(f[2], 32) || !readNumber(f[3], &a) ||
			    !readNumber(f[4], &b) || !readNumber(f[5], &c))
				return false;
			AccessRec r = { f[2], (unsigned) a, (time_t) b, (time_t) c };
			access_[f[1]] = r;
		}
		else if (f[0] == "R" && f.size() == 6)
		{
			if (!isHexOf(f[1], 64) || !isHexOf(f[2], 32) || !readNumber(f[3], &a) ||
			    !readNumber(f[4], &b) || (f[5] != "0" && f[5] != "1"))
				return false;
			RefreshRec r = { f[2], (time_t) a, (time_t) b, f[5] == "1" };
			refresh_[f[1]] = r;
		}
		else
			return false;
	}

	for (std::map<std::string, AccessRec>::const_iterator it = access_.begin(); it != access_.end(); ++it)
	{
		if (grants_.count(it->second.grant_id) == 0)
			return false;
	}
	for (std::map<std::string, RefreshRec>::const_iterator it = refresh_.begin(); it != refresh_.end(); ++it)
	{
		if (grants_.count(it->second.grant_id) == 0)
			return false;
	}
	for (std::map<std::string, Grant>::const_iterator it = grants_.begin(); it != grants_.end(); ++it)
	{
		if (clients_.count(it->second.client_id) == 0)
			return false;
	}
	return true;
}

bool Store::saveLocked()
{
	if (path_.empty())
		return true;
	++save_attempts_;
	std::string text = kHeader;
	text += '\n';
	if (!request_key_.empty())
		text += "K " + hexText(request_key_) + '\n';
	for (std::map<std::string, Client>::const_iterator it = clients_.begin(); it != clients_.end(); ++it)
	{
		const Client &c = it->second;
		text += "C " + c.key + ' ' + kindLetter(c.kind) + ' ' + num(c.created) + ' ' +
		        num(c.last_used) + ' ' + num(c.scopes) + ' ' + hexText(c.client_id) + ' ' +
		        hexText(c.name) + ' ' + hexText(c.user) + ' ' +
		        (c.token_hash.empty() ? std::string("-") : c.token_hash) + ' ' +
		        hexText(joinLines(c.redirect_uris)) + ' ' + num(c.groups) + '\n';
	}
	for (std::map<std::string, Grant>::const_iterator it = grants_.begin(); it != grants_.end(); ++it)
	{
		const Grant &g = it->second;
		text += "G " + g.id + ' ' + hexText(g.client_id) + ' ' + hexText(g.user) + ' ' +
		        num(g.scopes) + ' ' + hexText(g.resource) + ' ' + num(g.created) + ' ' +
		        num(g.last_used) + ' ' + num(g.groups) + '\n';
	}
	for (std::map<std::string, AccessRec>::const_iterator it = access_.begin(); it != access_.end(); ++it)
	{
		text += "A " + it->first + ' ' + it->second.grant_id + ' ' + num(it->second.scopes) + ' ' +
		        num(it->second.expires) + ' ' + num(it->second.issued) + '\n';
	}
	for (std::map<std::string, RefreshRec>::const_iterator it = refresh_.begin(); it != refresh_.end(); ++it)
	{
		text += "R " + it->first + ' ' + it->second.grant_id + ' ' + num(it->second.expires) + ' ' +
		        num(it->second.issued) + ' ' + (it->second.rotated ? "1" : "0") + '\n';
	}
	if (!replaceAtomically(path_, text))
	{
		std::fprintf(stderr, "[ni-web oauth] cannot write %s: %s\n", path_.c_str(), std::strerror(errno));
		return false;
	}
	saved_client_use_.clear();
	saved_grant_use_.clear();
	for (std::map<std::string, Client>::const_iterator it = clients_.begin(); it != clients_.end(); ++it)
		saved_client_use_[it->first] = it->second.last_used;
	for (std::map<std::string, Grant>::const_iterator it = grants_.begin(); it != grants_.end(); ++it)
		saved_grant_use_[it->first] = it->second.last_used;
	return true;
}

bool Store::hasGrantLocked(const std::string &client_id) const
{
	for (std::map<std::string, Grant>::const_iterator it = grants_.begin(); it != grants_.end(); ++it)
	{
		if (it->second.client_id == client_id)
			return true;
	}
	return false;
}

void Store::dropGrantLocked(const std::string &grant_id)
{
	for (std::map<std::string, AccessRec>::iterator it = access_.begin(); it != access_.end();)
	{
		if (it->second.grant_id == grant_id)
			it = access_.erase(it);
		else
			++it;
	}
	for (std::map<std::string, RefreshRec>::iterator it = refresh_.begin(); it != refresh_.end();)
	{
		if (it->second.grant_id == grant_id)
			it = refresh_.erase(it);
		else
			++it;
	}
	grants_.erase(grant_id);
}

void Store::sweepLocked(bool force)
{
	const time_t t = now();
	if (!force && last_sweep_ != 0 && t >= last_sweep_ && t - last_sweep_ < kSweepEvery)
		return;
	last_sweep_ = t;
	bool changed = false;

	for (std::map<std::string, AccessRec>::iterator it = access_.begin(); it != access_.end();)
	{
		if (it->second.expires <= t)
		{
			it = access_.erase(it);
			changed = true;
		}
		else
			++it;
	}
	for (std::map<std::string, RefreshRec>::iterator it = refresh_.begin(); it != refresh_.end();)
	{
		if (it->second.expires <= t)
		{
			it = refresh_.erase(it);
			changed = true;
		}
		else
			++it;
	}
	for (std::map<std::string, Grant>::iterator it = grants_.begin(); it != grants_.end();)
	{
		bool alive = false;
		for (std::map<std::string, AccessRec>::const_iterator a = access_.begin(); !alive && a != access_.end(); ++a)
			alive = a->second.grant_id == it->first;
		for (std::map<std::string, RefreshRec>::const_iterator r = refresh_.begin(); !alive && r != refresh_.end(); ++r)
			alive = r->second.grant_id == it->first && !r->second.rotated;
		if (alive)
			++it;
		else
		{
			const std::string gone = it->first;
			++it;
			dropGrantLocked(gone);
			changed = true;
		}
	}
	for (std::map<std::string, Client>::iterator it = clients_.begin(); it != clients_.end();)
	{
		const Client &c = it->second;
		const bool unused = c.kind != ClientKind::Static && !hasGrantLocked(c.client_id);
		const time_t anchor = c.last_used > c.created ? c.last_used : c.created;
		const bool stale = c.kind == ClientKind::Metadata || anchor + kUnusedClientLife <= t;
		if (unused && stale)
		{
			it = clients_.erase(it);
			changed = true;
		}
		else
			++it;
	}
	if (changed)
		(void) saveLocked();
}

void Store::trimLocked(const std::string &grant_id)
{
	for (;;)
	{
		size_t n = 0;
		std::map<std::string, AccessRec>::iterator oldest = access_.end();
		for (std::map<std::string, AccessRec>::iterator it = access_.begin(); it != access_.end(); ++it)
		{
			if (it->second.grant_id != grant_id)
				continue;
			++n;
			if (oldest == access_.end() || it->second.issued < oldest->second.issued)
				oldest = it;
		}
		if (n <= kMaxAccessPerGrant)
			break;
		access_.erase(oldest);
	}
	for (;;)
	{
		size_t n = 0;
		std::map<std::string, RefreshRec>::iterator oldest = refresh_.end();
		for (std::map<std::string, RefreshRec>::iterator it = refresh_.begin(); it != refresh_.end(); ++it)
		{
			if (it->second.grant_id != grant_id || !it->second.rotated)
				continue;
			++n;
			if (oldest == refresh_.end() || it->second.issued < oldest->second.issued)
				oldest = it;
		}
		if (n <= kMaxRotatedPerGrant)
			break;
		refresh_.erase(oldest);
	}
}

bool Store::mintLocked(const std::string &grant_id, unsigned scopes, bool with_refresh, Issued *out)
{
	const std::string a = mintToken(TokenKind::Access);
	const std::string r = with_refresh ? mintToken(TokenKind::Refresh) : std::string();
	const std::string ah = digest_(a);
	const std::string rh = with_refresh ? digest_(r) : std::string();
	if (a.empty() || ah.empty() || (with_refresh && (r.empty() || rh.empty())))
		return false;
	const time_t t = now();
	AccessRec ar = { grant_id, scopes, t + kAccessLifetime, t };
	access_[ah] = ar;
	if (with_refresh)
	{
		RefreshRec rr = { grant_id, t + kRefreshLifetime, t, false };
		refresh_[rh] = rr;
	}
	trimLocked(grant_id);
	out->access_token = a;
	out->refresh_token = r;
	out->scopes = scopes;
	out->expires_in = kAccessLifetime;
	return true;
}

bool Store::registerClient(const std::string &name, const std::vector<std::string> &redirect_uris,
                           Client *out)
{
	if (name.size() > kMaxClientNameBytes || !redirectsWithinCaps(redirect_uris))
		return false;
	Held h(lock_);
	sweepLocked(false);
	size_t registered = 0;
	std::string oldest;
	time_t oldest_at = 0;
	for (std::map<std::string, Client>::const_iterator it = clients_.begin(); it != clients_.end(); ++it)
	{
		if (it->second.kind != ClientKind::Registered)
			continue;
		++registered;
		if (!hasGrantLocked(it->first) && (oldest.empty() || it->second.created < oldest_at))
		{
			oldest = it->first;
			oldest_at = it->second.created;
		}
	}
	if (registered >= kMaxRegistered)
	{
		if (oldest.empty())
			return false;
		clients_.erase(oldest);
	}

	Client c;
	c.key = newId();
	if (c.key.empty())
		return false;
	c.client_id = "nid_" + c.key;
	c.kind = ClientKind::Registered;
	c.name = name;
	c.redirect_uris = redirect_uris;
	c.created = now();
	clients_[c.client_id] = c;
	(void) saveLocked();
	if (out != NULL)
		*out = c;
	return true;
}

bool Store::findClient(const std::string &client_id, Client *out)
{
	Held h(lock_);
	sweepLocked(false);
	std::map<std::string, Client>::const_iterator it = clients_.find(client_id);
	if (it == clients_.end())
		return false;
	if (out != NULL)
		*out = it->second;
	return true;
}

bool Store::createStatic(const std::string &name, unsigned scopes, const std::string &user,
                         Client *out, std::string *token, unsigned groups)
{
	if (name.size() > kMaxClientNameBytes)
		return false;
	Held h(lock_);
	size_t statics = 0;
	for (std::map<std::string, Client>::const_iterator it = clients_.begin(); it != clients_.end(); ++it)
		statics += (it->second.kind == ClientKind::Static) ? 1 : 0;
	if (statics >= kMaxStatic)
		return false;

	Client c;
	c.key = newId();
	const std::string t = mintToken(TokenKind::Static);
	c.token_hash = digest_(t);
	if (c.key.empty() || t.empty() || c.token_hash.empty())
		return false;
	c.client_id = "static:" + c.key;
	c.kind = ClientKind::Static;
	c.name = name;
	c.user = user;
	c.scopes = withImplied(scopes & ScopeLevels);
	c.groups = groups & mcp::kAllGroups;
	c.created = now();
	clients_[c.client_id] = c;
	(void) saveLocked();
	*out = c;
	*token = t;
	return true;
}

bool Store::removeClient(const std::string &key)
{
	Held h(lock_);
	for (std::map<std::string, Client>::iterator it = clients_.begin(); it != clients_.end(); ++it)
	{
		if (it->second.key != key)
			continue;
		const std::string client_id = it->first;
		std::vector<std::string> doomed;
		for (std::map<std::string, Grant>::const_iterator g = grants_.begin(); g != grants_.end(); ++g)
		{
			if (g->second.client_id == client_id)
				doomed.push_back(g->first);
		}
		for (size_t i = 0; i < doomed.size(); ++i)
			dropGrantLocked(doomed[i]);
		clients_.erase(it);
		(void) saveLocked();
		return true;
	}
	return false;
}

bool Store::setGroups(const std::string &key, unsigned groups)
{
	Held h(lock_);
	for (std::map<std::string, Client>::iterator it = clients_.begin(); it != clients_.end(); ++it)
	{
		if (it->second.key != key)
			continue;
		const unsigned masked = groups & mcp::kAllGroups;
		if (it->second.kind == ClientKind::Static)
			it->second.groups = masked;
		else
		{
			for (std::map<std::string, Grant>::iterator g = grants_.begin(); g != grants_.end(); ++g)
			{
				if (g->second.client_id == it->first)
					g->second.groups = masked;
			}
		}
		(void) saveLocked();
		return true;
	}
	return false;
}

std::vector<Client> Store::listClients()
{
	Held h(lock_);
	sweepLocked(false);
	std::vector<Client> out;
	for (std::map<std::string, Client>::const_iterator it = clients_.begin(); it != clients_.end(); ++it)
	{
		Client c = it->second;
		c.token_hash.clear();
		if (c.kind != ClientKind::Static)
		{
			bool any = false;
			c.scopes = 0;
			c.groups = 0;
			for (std::map<std::string, Grant>::const_iterator g = grants_.begin(); g != grants_.end(); ++g)
			{
				if (g->second.client_id != c.client_id)
					continue;
				any = true;
				c.scopes |= g->second.scopes;
				c.groups |= g->second.groups;
				if (g->second.last_used > c.last_used)
					c.last_used = g->second.last_used;
			}
			if (!any)
				continue;
		}
		out.push_back(c);
	}
	return out;
}

bool Store::issue(const Client &client, const std::string &user, unsigned scopes,
                  const std::string &resource, Issued *out, std::string *grant_id, unsigned groups)
{
	Held h(lock_);
	sweepLocked(false);
	if (client.kind == ClientKind::Static || grants_.size() >= kMaxGrants)
		return false;
	if (client.kind == ClientKind::Metadata &&
	    (client.name.size() > kMaxClientNameBytes || !redirectsWithinCaps(client.redirect_uris)))
		return false;
	std::map<std::string, Client>::iterator c = clients_.find(client.client_id);
	if (c == clients_.end())
	{
		if (client.kind != ClientKind::Metadata)
			return false;
		Client m = client;
		m.key = newId();
		if (m.key.empty())
			return false;
		m.created = now();
		m.last_used = m.created;
		clients_[m.client_id] = m;
	}
	else
	{
		if (c->second.kind == ClientKind::Metadata)
		{
			// The document may have changed since the last grant.
			c->second.name = client.name;
			c->second.redirect_uris = client.redirect_uris;
		}
		c->second.last_used = now();
	}

	Grant g;
	g.id = newId();
	if (g.id.empty())
		return false;
	g.client_id = client.client_id;
	g.user = user;
	g.scopes = scopes;
	g.groups = groups & mcp::kAllGroups;
	g.resource = resource;
	g.created = now();
	g.last_used = g.created;
	grants_[g.id] = g;
	if (!mintLocked(g.id, scopes, (scopes & ScopeOffline) != 0, out))
	{
		dropGrantLocked(g.id);
		(void) saveLocked();
		return false;
	}
	(void) saveLocked();
	if (grant_id != NULL)
		*grant_id = g.id;
	return true;
}

RefreshOutcome Store::refresh(const std::string &refresh_token, const std::string &client_id,
                              const std::string &resource, unsigned narrow, Issued *out)
{
	Held h(lock_);
	sweepLocked(false);
	if (!looksLike(refresh_token, TokenKind::Refresh))
		return RefreshOutcome::Invalid;
	std::map<std::string, RefreshRec>::iterator r = refresh_.find(digest_(refresh_token));
	if (r == refresh_.end())
		return RefreshOutcome::Invalid;
	const std::string grant_id = r->second.grant_id;
	std::map<std::string, Grant>::iterator g = grants_.find(grant_id);
	if (g == grants_.end() || g->second.client_id != client_id)
		return RefreshOutcome::Invalid;
	if (r->second.rotated)
	{
		dropGrantLocked(grant_id);
		(void) saveLocked();
		return RefreshOutcome::Reused;
	}
	if (g->second.resource != resource)
		return RefreshOutcome::Invalid;
	if (r->second.expires <= now())
		return RefreshOutcome::Invalid;

	unsigned scopes = g->second.scopes;
	if (narrow != 0)
	{
		const unsigned wanted = withImplied(narrow);
		if ((wanted & ~g->second.scopes) != 0)
			return RefreshOutcome::BadScope;
		scopes = wanted;
	}
	r->second.rotated = true;
	g->second.last_used = now();
	std::map<std::string, Client>::iterator cl = clients_.find(client_id);
	if (cl != clients_.end())
		cl->second.last_used = g->second.last_used;
	if (!mintLocked(grant_id, scopes, true, out))
	{
		r->second.rotated = false;
		return RefreshOutcome::Failed;
	}
	(void) saveLocked();
	return RefreshOutcome::Issued;
}

void Store::revokeGrant(const std::string &grant_id)
{
	Held h(lock_);
	if (grants_.count(grant_id) == 0)
		return;
	dropGrantLocked(grant_id);
	(void) saveLocked();
}

void Store::revokeToken(const std::string &token, const std::string &client_id)
{
	Held h(lock_);
	const std::string hash = digest_(token);
	if (looksLike(token, TokenKind::Access))
	{
		std::map<std::string, AccessRec>::iterator a = access_.find(hash);
		if (a == access_.end())
			return;
		std::map<std::string, Grant>::const_iterator g = grants_.find(a->second.grant_id);
		if (g == grants_.end() || g->second.client_id != client_id)
			return;
		access_.erase(a);
		(void) saveLocked();
	}
	else if (looksLike(token, TokenKind::Refresh))
	{
		std::map<std::string, RefreshRec>::iterator r = refresh_.find(hash);
		if (r == refresh_.end())
			return;
		std::map<std::string, Grant>::const_iterator g = grants_.find(r->second.grant_id);
		if (g == grants_.end() || g->second.client_id != client_id)
			return;
		// Copied: dropGrantLocked erases the refresh_ node r points into.
		const std::string grant_id = r->second.grant_id;
		// RFC 7009 section 2.1: a revoked refresh token takes the grant's access tokens along.
		dropGrantLocked(grant_id);
		(void) saveLocked();
	}
}

bool Store::checkToken(const std::string &token, TokenFacts *out, bool *failed)
{
	Held h(lock_);
	if (failed != NULL)
		*failed = false;
	const bool access = looksLike(token, TokenKind::Access);
	if (!access && !looksLike(token, TokenKind::Static))
		return false;
	const std::string hash = digest_(token);
	if (hash.empty())
	{
		if (failed != NULL)
			*failed = true;
		return false;
	}
	sweepLocked(false);
	const time_t t = now();
	if (access)
	{
		std::map<std::string, AccessRec>::const_iterator a = access_.find(hash);
		if (a == access_.end() || a->second.expires <= t)
			return false;
		std::map<std::string, Grant>::iterator g = grants_.find(a->second.grant_id);
		if (g == grants_.end())
			return false;
		std::map<std::string, Client>::iterator c = clients_.find(g->second.client_id);
		if (c == clients_.end())
			return false;
		if (t > g->second.last_used)
			g->second.last_used = t;
		if (t > c->second.last_used)
			c->second.last_used = t;
		if (useStaleLocked(c->first, g->first, t))
		{
			(void) saveLocked();
			// Also after a failed write, so a broken disk is tried once an hour and not per call.
			saved_client_use_[c->first] = c->second.last_used;
			saved_grant_use_[g->first] = g->second.last_used;
		}
		out->client_id = c->first;
		out->key = c->second.key;
		out->grant_id = g->first;
		out->user = g->second.user;
		out->scopes = a->second.scopes;
		out->groups = g->second.groups;
		out->resource = g->second.resource;
		out->is_static = false;
		return true;
	}
	for (std::map<std::string, Client>::iterator it = clients_.begin(); it != clients_.end(); ++it)
	{
		if (it->second.kind != ClientKind::Static || it->second.token_hash != hash)
			continue;
		if (t > it->second.last_used)
			it->second.last_used = t;
		if (useStaleLocked(it->first, std::string(), t))
		{
			(void) saveLocked();
			saved_client_use_[it->first] = it->second.last_used;
		}
		out->client_id = it->first;
		out->key = it->second.key;
		out->user = it->second.user;
		out->scopes = it->second.scopes;
		out->groups = it->second.groups;
		out->resource.clear();
		out->grant_id.clear();
		out->is_static = true;
		return true;
	}
	return false;
}

bool Store::useStaleLocked(const std::string &client_id, const std::string &grant_id, time_t t) const
{
	if (path_.empty())
		return false;
	std::map<std::string, time_t>::const_iterator c = saved_client_use_.find(client_id);
	if (c == saved_client_use_.end() || t - c->second > kUseWriteStep)
		return true;
	if (grant_id.empty())
		return false;
	std::map<std::string, time_t>::const_iterator g = saved_grant_use_.find(grant_id);
	return g == saved_grant_use_.end() || t - g->second > kUseWriteStep;
}

void Store::saveUse()
{
	Held h(lock_);
	bool behind = false;
	for (std::map<std::string, Client>::const_iterator it = clients_.begin(); !behind && it != clients_.end(); ++it)
	{
		std::map<std::string, time_t>::const_iterator c = saved_client_use_.find(it->first);
		behind = c == saved_client_use_.end() || c->second != it->second.last_used;
	}
	for (std::map<std::string, Grant>::const_iterator it = grants_.begin(); !behind && it != grants_.end(); ++it)
	{
		std::map<std::string, time_t>::const_iterator g = saved_grant_use_.find(it->first);
		behind = g == saved_grant_use_.end() || g->second != it->second.last_used;
	}
	if (behind)
		(void) saveLocked();
}

std::string Store::requestKey()
{
	Held h(lock_);
	if (request_key_.empty())
	{
		std::string key;
		if (!unhexText(randomToken(32), &key) || key.size() != 32)
			return std::string();
		request_key_ = key;
		(void) saveLocked();
	}
	return request_key_;
}

size_t Store::grantCountForTest()
{
	Held h(lock_);
	return grants_.size();
}

size_t Store::saveAttemptsForTest()
{
	Held h(lock_);
	return save_attempts_;
}

size_t Store::tokenCountForTest()
{
	Held h(lock_);
	return access_.size() + refresh_.size();
}

Store &store()
{
	static Store s;
	return s;
}

} // namespace oauth
} // namespace httpd
