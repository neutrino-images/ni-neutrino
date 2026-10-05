/*
 * cimd.cpp - clients identified by the URL of their metadata document
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

#include "httpd/oauth/cimd.h"

#include "httpd/oauth/clientmeta.h"
#include "httpd/oauth/store.h"
#include "httpd/oauth/uri.h"
#include "httpd/webtv.h"

#include <cstring>
#include <map>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include <curl/curl.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace httpd
{
namespace oauth
{

namespace
{

typedef OpenThreads::ScopedLock<OpenThreads::Mutex> Held;
typedef time_t (*Clock)();

const char kCaBundle[] = "/etc/ssl/certs/ca-certificates.crt";

Clock    clock_            = &realClock;
Fetcher *installed_        = NULL;
unsigned in_flight_        = 0;
pthread_once_t curl_once_  = PTHREAD_ONCE_INIT;

void initCurl()
{
	curl_global_init(CURL_GLOBAL_DEFAULT);
}

struct Cached
{
	MetadataClient client;
	Resolve        result;
	time_t         until;
};

OpenThreads::Mutex &cacheLock()
{
	static OpenThreads::Mutex m;
	return m;
}

std::map<std::string, Cached> &cache()
{
	static std::map<std::string, Cached> c;
	return c;
}

struct Sink
{
	std::string body;
	bool        too_large;
	long        max_age;

	Sink() : too_large(false), max_age(-1)
	{
	}
};

size_t intoSink(char *p, size_t size, size_t n, void *data)
{
	Sink *s = (Sink *) data;
	const size_t bytes = size * n;
	if (s->body.size() + bytes > kMaxMetadataBytes)
	{
		s->too_large = true;
		return 0;
	}
	s->body.append(p, bytes);
	return bytes;
}

// Cache-Control max-age, or nought for no-store and no-cache.
size_t headerSeen(char *p, size_t size, size_t n, void *data)
{
	Sink *s = (Sink *) data;
	const size_t bytes = size * n;
	std::string line(p, bytes);
	for (size_t i = 0; i < line.size(); ++i)
	{
		if (line[i] >= 'A' && line[i] <= 'Z')
			line[i] = (char) (line[i] - 'A' + 'a');
	}
	if (line.compare(0, 14, "cache-control:") != 0)
		return bytes;
	if (line.find("no-store") != std::string::npos || line.find("no-cache") != std::string::npos)
	{
		s->max_age = 0;
		return bytes;
	}
	const size_t at = line.find("max-age=");
	if (at == std::string::npos)
		return bytes;
	long v = 0;
	size_t i = at + 8;
	for (; i < line.size() && line[i] >= '0' && line[i] <= '9' && v < 100000000L; ++i)
		v = v * 10 + (line[i] - '0');
	if (i > at + 8)
		s->max_age = v;
	return bytes;
}

// Not webtv's guard: that one follows redirects and skips certificate checks.
curl_socket_t openGuarded(void *data, curlsocktype purpose, struct curl_sockaddr *address)
{
	bool *refused = (bool *) data;
	char text[INET6_ADDRSTRLEN];
	std::memset(text, 0, sizeof(text));
	bool readable = false;
	if (address->family == AF_INET)
		readable = inet_ntop(AF_INET, &((const struct sockaddr_in *) &address->addr)->sin_addr,
		                     text, sizeof(text)) != NULL;
	else if (address->family == AF_INET6)
		readable = inet_ntop(AF_INET6, &((const struct sockaddr_in6 *) &address->addr)->sin6_addr,
		                     text, sizeof(text)) != NULL;

	bool refuse = (purpose != CURLSOCKTYPE_IPCXN) || !readable;
	if (!refuse)
	{
		try
		{
			refuse = webtv::addressRefused(text);
		}
		catch (...)
		{
			refuse = true;
		}
	}
	if (refuse)
	{
		*refused = true;
		return CURL_SOCKET_BAD;
	}
	const int fd = ::socket(address->family, address->socktype, address->protocol);
	return (fd < 0) ? CURL_SOCKET_BAD : (curl_socket_t) fd;
}

class CurlFetcher : public Fetcher
{
	public:
		Fetched fetch(const std::string &url)
		{
			pthread_once(&curl_once_, &initCurl);
			Fetched f;
			CURL *h = curl_easy_init();
			if (h == NULL)
			{
				f.why = "no transfer handle";
				return f;
			}
			Sink sink;
			bool refused = false;
			struct curl_slist *accept = curl_slist_append(NULL, "Accept: application/json");

			curl_easy_setopt(h, CURLOPT_URL, url.c_str());
			curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
			curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 0L);
#if defined(CURL_AT_LEAST_VERSION) && CURL_AT_LEAST_VERSION(7, 85, 0)
			curl_easy_setopt(h, CURLOPT_PROTOCOLS_STR, "https");
#else
			curl_easy_setopt(h, CURLOPT_PROTOCOLS, (long) CURLPROTO_HTTPS);
#endif
			curl_easy_setopt(h, CURLOPT_TIMEOUT_MS, kFetchTimeoutMs);
			curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT_MS, kConnectTimeoutMs);
			curl_easy_setopt(h, CURLOPT_PROXY, "");
			curl_easy_setopt(h, CURLOPT_NOPROXY, "*");
			curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 1L);
			curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 2L);
			if (::access(kCaBundle, R_OK) == 0)
				curl_easy_setopt(h, CURLOPT_CAINFO, kCaBundle);
			curl_easy_setopt(h, CURLOPT_USERAGENT, "neutrino-ni-web");
			curl_easy_setopt(h, CURLOPT_MAXFILESIZE, (long) kMaxMetadataBytes);
			curl_easy_setopt(h, CURLOPT_OPENSOCKETFUNCTION, &openGuarded);
			curl_easy_setopt(h, CURLOPT_OPENSOCKETDATA, &refused);
			curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, &intoSink);
			curl_easy_setopt(h, CURLOPT_WRITEDATA, &sink);
			curl_easy_setopt(h, CURLOPT_HEADERFUNCTION, &headerSeen);
			curl_easy_setopt(h, CURLOPT_HEADERDATA, &sink);
			if (accept != NULL)
				curl_easy_setopt(h, CURLOPT_HTTPHEADER, accept);

			const CURLcode rc = curl_easy_perform(h);
			long status = 0;
			curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);
			curl_easy_cleanup(h);
			curl_slist_free_all(accept);

			if (refused)
				f.why = "the address is not one this box fetches from";
			else if (sink.too_large)
				f.why = "the document is larger than this box reads";
			else if (rc != CURLE_OK)
				f.why = curl_easy_strerror(rc);
			else if (status != 200)
				f.why = "the document was not answered with 200";
			else
			{
				f.ok = true;
				f.body = sink.body;
				f.max_age = sink.max_age;
			}
			return f;
		}
};

long cacheSeconds(long max_age)
{
	if (max_age < 0)
		return kDefaultCacheSeconds;
	if (max_age < kMinCacheSeconds)
		return kMinCacheSeconds;
	if (max_age > kMaxCacheSeconds)
		return kMaxCacheSeconds;
	return max_age;
}

Resolve judge(const std::string &url, const std::string &body, MetadataClient *out)
{
	if (body.size() > kMaxMetadataBytes)
		return Resolve::Invalid;
	ClientMetadata m;
	if (readClientMetadata(body, &m) != MetaRead::Ok)
		return Resolve::Invalid;
	if (!m.has_client_id || m.client_id != url)
		return Resolve::Invalid;
	if (!documentAllowsPublic(m) || !documentFlowsAcceptable(m))
		return Resolve::Invalid;
	Url u;
	out->client_id = url;
	out->name = !m.client_name.empty() ? m.client_name :
	            (parseUrl(url, &u) ? u.host.substr(0, kMaxClientNameBytes) : url.substr(0, kMaxClientNameBytes));
	out->redirect_uris = m.redirect_uris;
	return Resolve::Ok;
}

// Gives the fetch slot back however the fetch ends.
struct Slot
{
	~Slot()
	{
		Held h(cacheLock());
		--in_flight_;
	}
};

void remember(const std::string &url, const MetadataClient &client, Resolve result, time_t until)
{
	Held h(cacheLock());
	std::map<std::string, Cached> &c = cache();
	if (c.size() >= kMaxCachedDocuments && c.count(url) == 0)
	{
		std::map<std::string, Cached>::iterator soonest = c.begin();
		for (std::map<std::string, Cached>::iterator it = c.begin(); it != c.end(); ++it)
		{
			if (it->second.until < soonest->second.until)
				soonest = it;
		}
		c.erase(soonest);
	}
	Cached entry;
	entry.client = client;
	entry.result = result;
	entry.until = until;
	c[url] = entry;
}

} // namespace

Fetcher &systemFetcher()
{
	static CurlFetcher f;
	return f;
}

void installFetcherForTest(Fetcher *f)
{
	installed_ = f;
}

void setCimdClockForTest(time_t (*clock)())
{
	clock_ = (clock != NULL) ? clock : &realClock;
}

void forgetMetadataCacheForTest()
{
	Held h(cacheLock());
	cache().clear();
}

Resolve resolveMetadataClient(const std::string &url, MetadataClient *out)
{
	if (!cimdUrlAcceptable(url))
		return Resolve::BadUrl;
	const time_t now = clock_();
	{
		Held h(cacheLock());
		std::map<std::string, Cached>::const_iterator it = cache().find(url);
		if (it != cache().end() && it->second.until > now)
		{
			if (it->second.result == Resolve::Ok)
				*out = it->second.client;
			return it->second.result;
		}
		if (in_flight_ >= kMaxConcurrentFetches)
			return Resolve::Busy;
		++in_flight_;
	}

	MetadataClient client;
	Resolve result = Resolve::Unreachable;
	long seconds = kNegativeSeconds;
	{
		Slot slot;
		Fetcher &f = (installed_ != NULL) ? *installed_ : systemFetcher();
		const Fetched got = f.fetch(url);
		if (got.ok)
		{
			result = judge(url, got.body, &client);
			if (result == Resolve::Ok)
				seconds = cacheSeconds(got.max_age);
		}
	}
	remember(url, client, result, now + seconds);
	if (result == Resolve::Ok)
		*out = client;
	return result;
}

} // namespace oauth
} // namespace httpd
