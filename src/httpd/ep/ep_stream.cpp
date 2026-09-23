/*
 * ep_stream.cpp - routes for streams and playlists
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
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/livestream.h"
#include "httpd/schema.h"
#include "httpd/status.h"

#include "coreapi/browserplay.h"
#include "coreapi/channels.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/settings/settings.h"
#include "coreapi/streaming.h"
#include "coreapi/base/types.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

namespace httpd
{

namespace
{

// The type a player is handed a list of channels under. One place, so the two
// routes that answer such a list cannot come to name it differently.
const char *playlistContentType()
{
	return "audio/x-mpegurl";
}

const FieldDesc kAddressFields[] = {
	HTTPD_MEMBER("url", FieldType::String,
		"where a player fetches this channel, under the name this request reached the box by"),
	HTTPD_MEMBER("name", FieldType::String, "what the box calls the channel"),
	HTTPD_MEMBER("port", FieldType::UInt,
		"the port the box streams on, which is the port the address above carries"),
};

const Schema kAddressSchema = { "stream-address", HTTPD_FIELDS(kAddressFields) };

/* The authority is the request's own and is never taken from a caller's parameters.
   It is the name that caller reached this box by, so it is the one name an address
   handed back to it is reachable under. A request that named none is refused a floor
   down rather than handed an address with nothing where the box should be. */
Response streamAddress(const Request &r)
{
	const coreapi::ChannelId id = r.asChannelId("id");

	coreapi::Result<std::string> url = coreapi::streaming::urlFor(r.host(), id);
	if (!url.ok())
		return problemFor(url.error());

	coreapi::Result<coreapi::ChannelInfo> channel = coreapi::channels::get(id);
	if (!channel.ok())
		return problemFor(channel.error());

	/* The number is read beside the address rather than picked back out of it.
	   What the address is for is playing, and what the number is for is a
	   caller building an address of its own; taking the first apart to find the
	   second would be this layer reading back what it has just written. */
	coreapi::Result<std::string> port = coreapi::settings::get("streaming_port");
	if (!port.ok())
		return problemFor(port.error());

	Response out = okJson();
	Json j(out.body, 192);
	j.beginObject();
	j.key("url");
	j.value(url.value());
	j.key("name");
	j.value(channel.value().name);
	j.key("port");
	j.value((unsigned long) std::strtoul(port.value().c_str(), NULL, 10));
	j.endObject();
	return out;
}

coreapi::streaming::Scope scopeFor(const Request &r)
{
	if (!r.has("mode"))
		return coreapi::streaming::Scope::CurrentMode;
	const std::string &m = r.asString("mode");
	if (m == "tv")
		return coreapi::streaming::Scope::Tv;
	if (m == "radio")
		return coreapi::streaming::Scope::Radio;
	return coreapi::streaming::Scope::CurrentMode;
}

/* The file the layer below wrote is opened here and removed under its name in the same
   breath, whether or not the open worked: the name was this request's alone, and a
   request that fails between here and the transport sending the answer must not be
   the reason one is left in /tmp. What is left afterwards is the descriptor
   answerFromDescriptor takes over, which is what frees the space once it is done. */
Response playlistAt(const std::string &path)
{
	const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
	::unlink(path.c_str());

	Response out;
	out.code = StatusOk;
	if (!answerFromDescriptor(out, fd))
		return problemResponse(StatusInternalServerError, coreapi::ErrorCode::ChangeRefused,
				       "the playlist could not be read back for sending");
	out.content_type = playlistContentType();
	return out;
}

/* What the file a caller saves is called, which without this is the last
   segment of the route: a browser handed a playlist under /playlist/1001 writes
   1001, and a caller with four of them has four files named after numbers.

   The channel's own name and a suffix, through the one writer of this header
   there is (endpoints.h). What a channel may be called is whatever the stream
   said, which is any bytes at all, so the name is never written into the header
   as itself; that is that writer's whole subject and the reason there is not a
   second one here.

   The identifier stands in where the channel cannot be read. A playlist that
   was written names a channel the box had a moment ago, so this is a lookup
   that lost a race rather than a request for something absent, and answering
   the file with no name at all over it would take a working playlist away for
   the sake of a header. */
void nameThePlaylistAfter(Response &out, coreapi::ChannelId id)
{
	char hex[24];
	std::string name = hexId(id, hex);

	coreapi::Result<coreapi::ChannelInfo> channel = coreapi::channels::get(id);
	if (channel.ok() && !channel.value().name.empty())
		name = channel.value().name;

	out.headers.push_back(std::make_pair(std::string("Content-Disposition"),
					     dispositionFor(name + ".m3u")));
}

Response streamPlaylist(const Request &r)
{
	coreapi::Result<std::string> written =
		coreapi::streaming::playlist(r.host(), scopeFor(r));
	if (!written.ok())
		return problemFor(written.error());
	return playlistAt(written.value());
}

Response streamPlaylistFor(const Request &r)
{
	const coreapi::ChannelId id = r.asChannelId("id");

	coreapi::Result<std::string> written =
		coreapi::streaming::playlistFor(r.host(), id);
	if (!written.ok())
		return problemFor(written.error());

	Response out = playlistAt(written.value());
	// Only over an answer that is the playlist. A refusal carries a document
	// saying what went wrong, and a header telling a browser to save that as a
	// playlist would hand somebody a file holding the reason they have none.
	if (out.code == StatusOk)
		nameThePlaylistAfter(out, id);
	return out;
}

/* THE SPELLINGS THE CHANNEL ROUTE WRITES, and this route takes them back
   rather than looking them up. The box learns a channel's streams when it
   tunes it, so a caller that has just read the channel knows them and this
   route would otherwise be asking the same question a second time and getting
   a different answer for a channel nobody has been on.

   What that leaves is a caller naming codecs a channel does not carry, which
   costs a stream that does not play and nothing else: the picture is never
   re-encoded whatever is asked for, and the sound is decoded by what is in it
   rather than by what was said about it. */
const char kVideoCodecs[] = "mpeg2,h264,hevc,cavs,none,unknown";
const char kAudioCodecs[] = "mp2,ac3,eac3,aac,aacplus,dts,dtshd,lpcm,unknown";

// The widest a pid can be. Zero is the programme association table and never a
// stream, which is why it doubles as the value that means none was named.
const long kMaxPid = 8191;

/* The address the converter reads from, which is this box talking to itself.
   Loopback rather than the name the request arrived under: the reader is a
   program on this box, the answer must not depend on which of its addresses
   somebody typed, and a box whose own name does not resolve still has this
   one. */
std::string localSource(const std::string &port, coreapi::ChannelId id)
{
	char tail[32];
	std::snprintf(tail, sizeof(tail), "/id=%llx", (unsigned long long) id);
	return "http://127.0.0.1:" + port + tail;
}

/* One channel as something a browser can play, which for most of German
   satellite television means the picture untouched and the sound converted.

   The answer is a stream and not a document: this returns the command, and the
   transport runs it and keeps the connection (src/httpd/livestream.h). What
   comes back is a transport stream for a channel with a picture and an ADTS
   stream for radio, so that radio needs nothing in the page but an audio
   element. */
Response streamForBrowser(const Request &r)
{
	const coreapi::ChannelId id = r.asChannelId("id");

	coreapi::Result<coreapi::ChannelInfo> channel = coreapi::channels::get(id);
	if (!channel.ok())
		return problemFor(channel.error());

	const std::string &video = r.asString("video");
	const std::string &audio = r.asString("audio");

	const coreapi::browserplay::Path path = coreapi::browserplay::pathFor(video, audio);
	if (path == coreapi::browserplay::Path::NotOffered)
	{
		/* Said rather than answered with a stream nothing decodes. A caller
		   told this offers the raw address to a player outside the browser,
		   which is the honest end of this road and the whole reason the
		   channel route says what the streams are. */
		return problemResponse(StatusNotImplemented,
				       coreapi::ErrorCode::NotPlayableInBrowser,
				       "no browser plays what this channel carries, and this box will not re-encode a picture");
	}

	coreapi::Result<std::string> port = coreapi::settings::get("streaming_port");
	if (!port.ok())
		return problemFor(port.error());

	const bool radio = channel.value().kind == coreapi::ServiceKind::Radio ||
			   channel.value().kind == coreapi::ServiceKind::WebRadio;
	unsigned pid = 0;
	if (r.has("apid"))
		pid = (unsigned) r.asUInt("apid");

	Response out;
	out.code = StatusOk;
	out.content_type = coreapi::browserplay::typeFor(path, radio);
	out.stream_argv = coreapi::browserplay::commandFor(path, radio,
							   localSource(port.value(), id), pid);
	// Live and never the same twice, so nothing between here and the player
	// keeps a minute of it and hands it to the next caller.
	out.headers.push_back(std::make_pair(std::string("Cache-Control"),
					     std::string("no-store")));
	return out;
}

const Param kChannelParams[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId, "the channel, hexadecimal"),
};

const Param kBrowserParams[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId, "the channel, hexadecimal"),
	HTTPD_QUERY_REQUIRED_FROM_SET("video",
		"what the channel's picture is, as the channel route spells it",
		kVideoCodecs),
	HTTPD_QUERY_REQUIRED_FROM_SET("audio",
		"what the channel's sound is, as the channel route spells it",
		kAudioCodecs),
	HTTPD_QUERY_IN("apid", ParamType::UInt,
		"which sound to take, as its pid, when the channel carries several",
		0, kMaxPid),
};

const Param kPlaylistParams[] = {
	// Carried in the query, because the method that fetches a list is written
	// with no body.
	HTTPD_QUERY_FROM_SET("mode", "which half to list, the half the box is showing when it is left out",
		"tv,radio"),
};

const Endpoint kStreamEndpoints[] = {
	{ Method::Get, "/api/v1/stream/playlist", AuthLevel::Read,
	  "every visible user bouquet as one playlist a player opens",
	  HTTPD_PARAMS(kPlaylistParams), NULL, &streamPlaylist, false },
	/* THE SAME LIST UNDER THE NAME ITS FILE HAS. A box reading a playlist back
	   in as a channel list decides what it was handed by the extension of the
	   address it was given and reads nothing it cannot name that way
	   (CBouquetManager::loadWebchannels), so the address above, which ends in
	   no extension and whose last point is the one in an address like
	   192.168.1.5, is turned away there before a byte of it is read. The webtv
	   route names its file the same way and for readers of the same kind. */
	{ Method::Get, "/api/v1/stream/playlist.m3u", AuthLevel::Read,
	  "the same list, under a name a reader that goes by file names accepts",
	  HTTPD_PARAMS(kPlaylistParams), NULL, &streamPlaylist, false },
	{ Method::Get, "/api/v1/stream/playlist/{id}", AuthLevel::Read,
	  "one channel as a playlist a player opens",
	  HTTPD_PARAMS(kChannelParams), NULL, &streamPlaylistFor, false },
	{ Method::Get, "/api/v1/stream/{id}", AuthLevel::Read,
	  "where a player fetches one channel",
	  HTTPD_PARAMS(kChannelParams), &kAddressSchema, &streamAddress, false },
	{ Method::Get, "/api/v1/stream/browser/{id}", AuthLevel::Read,
	  "one channel as a browser can play it, with the sound converted where it has to be",
	  HTTPD_PARAMS(kBrowserParams), NULL, &streamForBrowser, false },
};

} // namespace

extern const RouteTable streamTable = {
	HTTPD_TABLE("stream", kStreamEndpoints)
};

} // namespace httpd
