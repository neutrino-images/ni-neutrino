/*
 * store.h - OAuth clients, grants and tokens, kept as digests on disk
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

#ifndef __httpd_oauth_store_h__
#define __httpd_oauth_store_h__

#include "httpd/mcp/toolgroups.h"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include <time.h>

#include <OpenThreads/Mutex>

namespace httpd
{
namespace oauth
{

enum class ClientKind
{
	Registered,
	Metadata,
	Static
};

const char *kindName(ClientKind k);

// The default Store clock. Other modules reuse this instead of their own copy.
time_t realClock();

struct Client
{
	std::string key;
	std::string client_id;
	ClientKind  kind;
	std::string name;
	std::vector<std::string> redirect_uris;
	std::string user;
	// Static: what the token is worth. In a listing: the union of the grants.
	unsigned    scopes;
	// Static: the groups the token offers. In a listing: the union of the grants.
	unsigned    groups;
	std::string token_hash;
	time_t      created;
	time_t      last_used;

	Client() : kind(ClientKind::Registered), scopes(0), groups(mcp::kDefaultGroups), created(0), last_used(0) {}
};

struct Grant
{
	std::string id;
	std::string client_id;
	std::string user;
	unsigned    scopes;
	unsigned    groups;
	std::string resource;
	time_t      created;
	time_t      last_used;

	Grant() : scopes(0), groups(mcp::kDefaultGroups), created(0), last_used(0) {}
};

struct Issued
{
	std::string access_token;
	std::string refresh_token;
	unsigned    scopes;
	long        expires_in;

	Issued() : scopes(0), expires_in(0) {}
};

struct TokenFacts
{
	std::string client_id;
	std::string key;
	// The grant an access token was issued under; empty for a static token.
	std::string grant_id;
	std::string user;
	unsigned    scopes;
	unsigned    groups;
	std::string resource;
	bool        is_static;

	TokenFacts() : scopes(0), groups(0), is_static(false) {}
};

enum class RefreshOutcome
{
	Issued,
	Invalid,
	Reused,
	BadScope,
	Failed
};

const long   kAccessLifetime     = 3600;
const long   kRefreshLifetime    = 30L * 24L * 3600L;
const long   kUnusedClientLife   = 24L * 3600L;
const size_t kMaxRegistered      = 64;
const size_t kMaxStatic          = 32;
const size_t kMaxGrants          = 256;
const size_t kMaxAccessPerGrant  = 8;
const size_t kMaxRotatedPerGrant = 8;
// Enforced in registerClient, createStatic and a metadata client's issue(): keeps
// saveLocked's output a known size even with every cap hit at once.
const size_t kMaxClientNameBytes = 100;
const size_t kMaxRedirectUris    = 8;

class Store
{
	public:
		typedef time_t (*Clock)();
		typedef std::string (*Digest)(const std::string &);

		Store();

		void setClock(Clock c);
		void setDigestForTest(Digest d);

		// Missing file: empty store. Unreadable file: moved to path + ".bad", empty store. Empty path: memory only.
		bool open(const std::string &path);

		bool registerClient(const std::string &name, const std::vector<std::string> &redirect_uris,
		                    Client *out);
		bool findClient(const std::string &client_id, Client *out);
		bool createStatic(const std::string &name, unsigned scopes, const std::string &user,
		                  Client *out, std::string *token, unsigned groups = mcp::kDefaultGroups);
		bool removeClient(const std::string &key);
		std::vector<Client> listClients();
		// False for a key no client carries.
		bool setGroups(const std::string &key, unsigned groups);

		bool issue(const Client &client, const std::string &user, unsigned scopes,
		           const std::string &resource, Issued *out, std::string *grant_id,
		           unsigned groups = mcp::kDefaultGroups);
		RefreshOutcome refresh(const std::string &refresh_token, const std::string &client_id,
		                       const std::string &resource, unsigned narrow, Issued *out);
		void revokeGrant(const std::string &grant_id);
		void revokeToken(const std::string &token, const std::string &client_id);
		// A use is written once it is an hour newer than the stored one; saveUse writes the rest.
		bool checkToken(const std::string &token, TokenFacts *out, bool *failed = NULL);
		void saveUse();

		// 32 bytes that sign open authorize requests, made once and kept in the file. Empty on failure.
		std::string requestKey();

		size_t grantCountForTest();
		size_t tokenCountForTest();
		size_t saveAttemptsForTest();

	private:
		Store(const Store &);
		Store &operator=(const Store &);

		struct AccessRec
		{
			std::string grant_id;
			unsigned    scopes;
			time_t      expires;
			time_t      issued;
		};

		struct RefreshRec
		{
			std::string grant_id;
			time_t      expires;
			time_t      issued;
			bool        rotated;
		};

		time_t now() const;
		void clearLocked();
		bool hasGrantLocked(const std::string &client_id) const;
		void sweepLocked(bool force);
		void dropGrantLocked(const std::string &grant_id);
		void trimLocked(const std::string &grant_id);
		bool mintLocked(const std::string &grant_id, unsigned scopes, bool with_refresh, Issued *out);
		bool saveLocked();
		bool parse(const std::string &text);
		bool useStaleLocked(const std::string &client_id, const std::string &grant_id, time_t t) const;

		std::map<std::string, Client>     clients_;
		std::map<std::string, Grant>      grants_;
		std::map<std::string, AccessRec>  access_;
		std::map<std::string, RefreshRec> refresh_;
		// last_used as the file holds it, by client id and by grant id.
		std::map<std::string, time_t>     saved_client_use_;
		std::map<std::string, time_t>     saved_grant_use_;
		OpenThreads::Mutex lock_;
		Clock       clock_;
		Digest      digest_;
		std::string path_;
		std::string request_key_;
		time_t      last_sweep_;
		size_t      save_attempts_;
};

Store &store();

} // namespace oauth
} // namespace httpd

#endif
