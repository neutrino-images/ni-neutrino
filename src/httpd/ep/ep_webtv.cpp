/*
 * ep_webtv.cpp - routes that hand a browser a channel played from somewhere else
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

#include "httpd/endpoints.h"

#include "httpd/endpoint.h"
#include "httpd/hlsrewrite.h"
#include "httpd/http.h"
#include "httpd/status.h"
#include "httpd/webtv.h"

#include "coreapi/channels.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <cstdio>
#include <string>

namespace httpd
{

namespace
{

/* The whole of a playlist this reads. A live playlist of a channel is a few
   kilobytes and a long catch up list a few hundred; anything past this is not
   a playlist of a channel, and reading it would be this box holding a
   megabyte per request for whatever a far server chose to send. */
const size_t kPlaylistCap = 1024u * 1024u;

const char *playlistType()
{
	return "application/vnd.apple.mpegurl";
}

/* What a browser is told a fetched part is, where the far server says nothing.
   It is a fallback and never an override: what this box hands on is the far
   server's own document, and the far server's own media type goes with it
   whenever there is one. */
const char *typeFor(hls::UriKind kind)
{
	switch (kind)
	{
		case hls::UriKind::Playlist: return playlistType();
		case hls::UriKind::Segment:  return "video/MP2T";
		case hls::UriKind::Map:      return "video/mp4";
		case hls::UriKind::Key:      return "application/octet-stream";
	}
	return "application/octet-stream";
}

/* Where the addresses of one channel's playlist point after this box has
   rewritten them.

   Root absolute, so that the address a browser resolves does not depend on
   which path this playlist was answered under, and the channel is in it so
   that a token minted for one channel cannot be spent on another. */
std::string partPrefix(coreapi::ChannelId id)
{
	char buf[24];
	std::string out = "/api/v1/webtv/";
	out += hexId(id, buf);
	out += "/part/";
	return out;
}

class PartMinter : public hls::Minter
{
	public:
		PartMinter(coreapi::ChannelId id) : id_(id), prefix_(partPrefix(id)) {}

		bool mint(const std::string &absolute, hls::UriKind kind, std::string &out)
		{
			const std::string token = webtv::mintPart(id_, absolute, kind);
			if (token.empty())
				return false;
			out = prefix_ + token;
			return true;
		}

	private:
		coreapi::ChannelId id_;
		std::string        prefix_;
};

Response refusedAddress()
{
	return problemResponse(StatusForbidden, coreapi::ErrorCode::AddressRefused,
	                       "this channel leads to an address on a network this box does not fetch from");
}

Response unreachable(const char *why)
{
	return problemResponse(StatusServiceUnavailable, coreapi::ErrorCode::UpstreamUnreachable, why);
}

/* Fetches a playlist and hands back the same playlist with every address in it
   pointing at this box.

   The two routes that answer a playlist share this, because they differ only
   in where the address came from: one takes it off the channel and the other
   out of a playlist this box has already read. What happens to it afterwards
   is the same, and two copies of that would be two answers to what a playlist
   this box cannot read is. */
Response playlistFrom(coreapi::ChannelId id, const std::string &url)
{
	if (!webtv::urlAcceptable(url))
		return refusedAddress();

	webtv::Document doc;
	const webtv::Reached got = webtv::fetchBounded(url, kPlaylistCap, doc);

	switch (got)
	{
		case webtv::Reached::Refused:
			return refusedAddress();
		case webtv::Reached::Unreachable:
			return unreachable("the server this channel is played from could not be reached");
		case webtv::Reached::TooLarge:
			return problemResponse(StatusUnsupportedMedia,
			                       coreapi::ErrorCode::PlaylistUnreadable,
			                       "what came back is longer than a playlist this box reads");
		case webtv::Reached::Ok:
			break;
	}

	if (doc.code < 200 || doc.code >= 300)
	{
		char why[128];
		std::snprintf(why, sizeof(why),
		              "the server this channel is played from answered %ld", doc.code);
		return unreachable(why);
	}

	PartMinter minter(id);
	std::string body;
	size_t uris = 0;
	const hls::Rewritten read = hls::rewrite(doc.body, doc.url, minter, body, uris);

	switch (read)
	{
		case hls::Rewritten::NotAPlaylist:
			/* Said as its own refusal rather than as a fault, because it is an
			   answer about the channel and the page acts on it: a channel that
			   is a stream and not a playlist is played through the route
			   beside this one. */
			return problemResponse(StatusUnsupportedMedia, coreapi::ErrorCode::NotAPlaylist,
			                       "the address this channel names does not answer with a playlist");
		case hls::Rewritten::HasByteRange:
			return problemResponse(StatusUnsupportedMedia,
			                       coreapi::ErrorCode::PlaylistUnreadable,
			                       "this playlist addresses stretches of files, which this box does not carry across");
		case hls::Rewritten::BadReference:
			return problemResponse(StatusUnsupportedMedia,
			                       coreapi::ErrorCode::PlaylistUnreadable,
			                       "this playlist carries an address this box cannot resolve");
		case hls::Rewritten::TooManyUris:
			return problemResponse(StatusUnsupportedMedia,
			                       coreapi::ErrorCode::PlaylistUnreadable,
			                       "this playlist carries more addresses than this box holds");
		case hls::Rewritten::Ok:
			break;
	}

	Response out;
	out.code = StatusOk;
	out.content_type = playlistType();
	out.body = body;
	/* A live playlist is a different document every few seconds and the
	   addresses in it stop working, so anything that stored one would hand a
	   player a list of parts this box has already forgotten. */
	out.headers.push_back(std::make_pair(std::string("Cache-Control"),
	                                     std::string("no-store")));
	return out;
}

/* The channel, and the address it is played from, or the refusal that says why
   there is none. Answers whether there is an address to go on with. */
bool addressOf(coreapi::ChannelId id, std::string &url, Response &refusal)
{
	const coreapi::Result<coreapi::ChannelInfo> channel = coreapi::channels::get(id);
	if (!channel.ok())
	{
		refusal = problemFor(channel.error());
		return false;
	}
	if (channel.value().url.empty())
	{
		refusal = problemResponse(StatusNotFound, coreapi::ErrorCode::NotAWebChannel,
		                          "this channel is one the box tunes to, not one it plays from an address");
		return false;
	}
	url = channel.value().url;
	return true;
}

Response webtvPlaylist(const Request &r)
{
	const coreapi::ChannelId id = r.asChannelId("id");
	std::string url;
	Response refusal;
	if (!addressOf(id, url, refusal))
		return refusal;
	return playlistFrom(id, url);
}

Response webtvStream(const Request &r)
{
	const coreapi::ChannelId id = r.asChannelId("id");
	std::string url;
	Response refusal;
	if (!addressOf(id, url, refusal))
		return refusal;
	if (!webtv::urlAcceptable(url))
		return refusedAddress();
	return webtv::relayOf(url, "application/octet-stream");
}

Response webtvPart(const Request &r)
{
	const coreapi::ChannelId id = r.asChannelId("id");
	const std::string &token = r.asString("token");

	coreapi::ChannelId minted_for = 0;
	std::string url;
	hls::UriKind kind = hls::UriKind::Segment;
	if (!webtv::findPart(token, minted_for, url, kind))
		return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchPart,
		                       "nothing was minted under that name, or what was has aged out");

	/* The channel in the path has to be the channel the token was minted for.
	   Without it a token is a key to an address rather than to an address of
	   one channel, and the identifier in the path would be decoration. */
	if (minted_for != id)
		return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchPart,
		                       "that name was not minted for this channel");

	if (kind == hls::UriKind::Playlist)
		return playlistFrom(id, url);

	return webtv::relayOf(url, typeFor(kind));
}

const Param kChannelParams[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId, "the channel, hexadecimal"),
};

const Param kPartParams[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId, "the channel, hexadecimal"),
	HTTPD_SEGMENT_TEXT("token", "a name this box minted while reading this channel's playlist", 64),
};

/* Read and not public, which is the level the addresses of a stream are handed
   out at beside this. What these answer is a channel of this box, and a box on
   a network where a read costs nothing is a box whose channel list is already
   readable. */
const Endpoint kWebtvEndpoints[] = {
	{ Method::Get, "/api/v1/webtv/{id}/playlist.m3u8", AuthLevel::Read,
	  "one channel's playlist, with every address in it answered by this box",
	  HTTPD_PARAMS(kChannelParams), NULL, &webtvPlaylist, false },
	{ Method::Get, "/api/v1/webtv/{id}/stream", AuthLevel::Read,
	  "one channel's stream, fetched by this box and passed on as it arrives",
	  HTTPD_PARAMS(kChannelParams), NULL, &webtvStream, false },
	{ Method::Get, "/api/v1/webtv/{id}/part/{token}", AuthLevel::Read,
	  "one address out of a playlist this box read, fetched and passed on",
	  HTTPD_PARAMS(kPartParams), NULL, &webtvPart, false },
};

} // namespace

extern const RouteTable webtvTable = {
	HTTPD_TABLE("webtv", kWebtvEndpoints)
};

} // namespace httpd
