/*
 * cimd.h - clients identified by the URL of their metadata document
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

#ifndef __httpd_oauth_cimd_h__
#define __httpd_oauth_cimd_h__

#include <cstddef>
#include <string>
#include <vector>

#include <time.h>

namespace httpd
{
namespace oauth
{

const size_t   kMaxMetadataBytes     = 5120;
const long     kFetchTimeoutMs       = 5000;
const long     kConnectTimeoutMs     = 3000;
const size_t   kMaxCachedDocuments   = 32;
const long     kNegativeSeconds      = 60;
const long     kMinCacheSeconds      = 60;
const long     kMaxCacheSeconds      = 86400;
const long     kDefaultCacheSeconds  = 3600;
const unsigned kMaxConcurrentFetches = 2;

struct Fetched
{
	bool        ok;
	std::string body;
	long        max_age;   // below zero: the answer named none
	std::string why;

	Fetched() : ok(false), max_age(-1) {}
};

class Fetcher
{
	public:
		virtual ~Fetcher() {}
		virtual Fetched fetch(const std::string &url) = 0;
};

Fetcher &systemFetcher();
void installFetcherForTest(Fetcher *f);

struct MetadataClient
{
	std::string client_id;
	std::string name;
	std::vector<std::string> redirect_uris;
};

enum class Resolve
{
	Ok,
	BadUrl,
	Unreachable,
	Invalid,
	Busy
};

Resolve resolveMetadataClient(const std::string &url, MetadataClient *out);
void forgetMetadataCacheForTest();
void setCimdClockForTest(time_t (*clock)());

} // namespace oauth
} // namespace httpd

#endif
