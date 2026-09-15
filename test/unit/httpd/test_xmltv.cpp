/*
 * test_xmltv.cpp - tests for XMLTV programme data
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
#include "support/fakes.h"

// The golden comparison below drives compat/'s serialiser, and compat/ is not
// compiled with --disable-legacy-api (src/httpd/Makefile.am).
#include <config.h>

#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/http.h"
#include "httpd/router.h"
#include "httpd/server.h"
#include "httpd/status.h"

#ifndef DISABLE_LEGACY_API
#include "httpd/compat/hookout.h"
#endif

#include "coreapi/epg.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include "support/httpclient.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

using namespace httpd;

namespace
{

// Cleared going in as well as coming out, so a case elsewhere that failed and
// unwound past its own restore cannot decide what these read.
struct ShippedRoutes
{
	ShippedRoutes() { setRoutesForTest(NULL); }
	~ShippedRoutes() { setRoutesForTest(NULL); }

	private:
		ShippedRoutes(const ShippedRoutes &);
		ShippedRoutes &operator=(const ShippedRoutes &);
};

const Endpoint *routeFor(const char *path)
{
	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	for (size_t i = 0; i < tables; ++i)
		for (size_t j = 0; j < t[i]->count; ++j)
			if (std::strcmp(t[i]->endpoints[j].path, path) == 0)
				return &t[i]->endpoints[j];
	return NULL;
}

std::string hex(uint64_t v)
{
	char buf[24];
	std::snprintf(buf, sizeof(buf), "%llx", (unsigned long long) v);
	return std::string(buf);
}

// The whole attribute and not the bare hex, so a channel id that happens to
// read as a run of digits inside some event's timestamp is not mistaken for
// the channel itself: "1001" is both a channel id below and, spelled out in
// full, four digits an unrelated minute and second can share.
std::string channelIdAttr(uint64_t id)
{
	return "id=\"" + hex(id) + "\"";
}

coreapi::ChannelInfo makeChannel(uint64_t id, const char *name, coreapi::ServiceKind kind)
{
	coreapi::ChannelInfo c;
	c.id = id;
	c.name = name;
	c.kind = kind;
	return c;
}

coreapi::EventInfo makeEvent(uint64_t channel_id, uint64_t event_id, time_t start,
                             unsigned duration, const std::string &title,
                             const std::string &description)
{
	coreapi::EventInfo e;
	e.channel_id = channel_id;
	e.event_id = event_id;
	e.start = start;
	e.duration = duration;
	e.title = title;
	e.description = description;
	return e;
}

// Every byte a call to xmltv() left in the file it named, read back and the
// file left exactly as the call put it: nothing here is the cleanup this
// endpoint's caller is answerable for, so a case reading the content also
// removes the name itself once it is done with it.
std::string readAndRemove(const std::string &path)
{
	std::string out;
	FILE *f = std::fopen(path.c_str(), "rb");
	REQUIRE(f != NULL);
	char buf[4096];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
		out.append(buf, n);
	std::fclose(f);
	::unlink(path.c_str());
	return out;
}

// How many of the files this endpoint's own naming scheme could have made are
// sitting in /tmp right now. Matched by prefix and not by a fixed name,
// because the path carries this process's pid and a per-call counter.
size_t xmltvFilesInTmp()
{
	DIR *d = ::opendir("/tmp");
	if (d == NULL)
		return 0;
	const std::string prefix = "coreapi-xmltv.";
	size_t n = 0;
	for (struct dirent *e = ::readdir(d); e != NULL; e = ::readdir(d))
		if (std::strncmp(e->d_name, prefix.c_str(), prefix.size()) == 0)
			++n;
	::closedir(d);
	return n;
}

// How many bytes the file this endpoint is writing right now already holds.
// Matched by the prefix its naming scheme uses and narrowed to this process,
// so a second suite running beside this one is not counted; every call here
// removes its own file, so at most one of them is ever in flight.
size_t xmltvBytesInTmp()
{
	DIR *d = ::opendir("/tmp");
	if (d == NULL)
		return 0;
	char prefix[64];
	std::snprintf(prefix, sizeof(prefix), "coreapi-xmltv.%d.", (int) ::getpid());
	const size_t plen = std::strlen(prefix);
	size_t bytes = 0;
	for (struct dirent *e = ::readdir(d); e != NULL; e = ::readdir(d))
	{
		if (std::strncmp(e->d_name, prefix, plen) != 0)
			continue;
		struct stat st;
		const std::string path = std::string("/tmp/") + e->d_name;
		if (::stat(path.c_str(), &st) == 0)
			bytes += (size_t) st.st_size;
	}
	::closedir(d);
	return bytes;
}

/* One bouquet of two television channels and one radio channel, plus a
   second bouquet excluded for being hidden and a third excluded for not
   being a user bouquet: the three reasons a channel this fixture holds can
   still be missing from the document. */
struct Fixture
{
	InstalledDependencies deps;

	static const uint64_t kTv1 = 0x1001;
	static const uint64_t kTv2 = 0x1002;
	static const uint64_t kRadio1 = 0x2001;
	static const uint64_t kHiddenChannel = 0x3001;
	static const uint64_t kProviderChannel = 0x4001;

	Fixture()
	{
		coreapi::BouquetInfo shown;
		shown.id = 1;
		shown.name = "Favourites";
		shown.hidden = false;
		shown.user_bouquet = true;
		deps.channels.bouquets.push_back(shown);

		coreapi::BouquetInfo hidden;
		hidden.id = 2;
		hidden.name = "Hidden";
		hidden.hidden = true;
		hidden.user_bouquet = true;
		deps.channels.bouquets.push_back(hidden);

		coreapi::BouquetInfo provider;
		provider.id = 3;
		provider.name = "Providers";
		provider.hidden = false;
		provider.user_bouquet = false;
		deps.channels.bouquets.push_back(provider);

		coreapi::ChannelList shown_members;
		shown_members.push_back(makeChannel(kTv1, "Info & News", coreapi::ServiceKind::Tv));
		shown_members.push_back(makeChannel(kTv2, "Second <Live>", coreapi::ServiceKind::Tv));
		shown_members.push_back(makeChannel(kRadio1, "Radio \"One\"", coreapi::ServiceKind::Radio));
		deps.channels.bouquet_members[1] = shown_members;

		coreapi::ChannelList hidden_members;
		hidden_members.push_back(makeChannel(kHiddenChannel, "hidden", coreapi::ServiceKind::Tv));
		deps.channels.bouquet_members[2] = hidden_members;

		coreapi::ChannelList provider_members;
		provider_members.push_back(makeChannel(kProviderChannel, "provider", coreapi::ServiceKind::Tv));
		deps.channels.bouquet_members[3] = provider_members;

		deps.epg.events.push_back(makeEvent(kTv1, 1, 1000, 1800, "Morning Show", "what's on now"));
		deps.epg.events.push_back(makeEvent(kTv1, 2, 2800, 1800, "Second event", "later"));
		deps.epg.events.push_back(makeEvent(kRadio1, 3, 1000, 3600, "Music", "tunes"));
	}

	private:
		Fixture(const Fixture &);
		Fixture &operator=(const Fixture &);
};

#ifndef DISABLE_LEGACY_API

std::string expectedTimestamp(time_t t)
{
	struct tm tmv;
	gmtime_r(&t, &tmv);
	char buf[32];
	strftime(buf, sizeof(buf), "%Y%m%d%H%M%S %z", &tmv);
	return std::string(buf);
}

bool expectedIsRadio(coreapi::ServiceKind k)
{
	return k == coreapi::ServiceKind::Radio || k == coreapi::ServiceKind::WebRadio;
}

/* Built through the control API's own tag writer rather than by retyping its
   escaping and its bracket shapes by hand, so what this is held to is the
   algorithm the legacy generator is made of and not somebody's memory of it.
   Kind::All is the one kind whose filter the legacy generator's own mode
   handling agrees with (see the report for the other two), so this is where
   a byte for byte match is asked for. */
std::string expectedDocument(FakeChannelSource &channels, FakeEpgSource &epg,
                             coreapi::epg::Kind kind)
{
	httpd::compat::Serializer s;
	s.setOutType(httpd::compat::xml);

	std::string out;
	for (size_t i = 0; i < channels.bouquets.size(); ++i)
	{
		const coreapi::BouquetInfo &b = channels.bouquets[i];
		if (b.hidden || !b.user_bouquet)
			continue;

		std::map<uint32_t, coreapi::ChannelList>::const_iterator bit =
			channels.bouquet_members.find(b.id);
		if (bit == channels.bouquet_members.end())
			continue;

		for (size_t j = 0; j < bit->second.size(); ++j)
		{
			const coreapi::ChannelInfo &c = bit->second[j];
			const bool radio = expectedIsRadio(c.kind);
			if (kind == coreapi::epg::Kind::Tv && radio)
				continue;
			if (kind == coreapi::epg::Kind::Radio && !radio)
				continue;

			const std::string cid = hex(c.id);
			const std::string channelTag = "channel id=\"" + cid + "\"";
			const std::string channelData =
				s.outPair("display-name", s.outValue(c.name, false), true);
			out += s.outObject(channelTag, channelData);

			coreapi::EventList events;
			for (size_t k = 0; k < epg.events.size(); ++k)
				if (epg.events[k].channel_id == c.id)
					events.push_back(epg.events[k]);
			if (events.size() > 50)
				events.resize(50);

			for (size_t k = 0; k < events.size(); ++k)
			{
				const coreapi::EventInfo &e = events[k];
				std::string tag = "programme ";
				tag += "channel=\"" + cid + "\" ";
				tag += "start=\"" + expectedTimestamp(e.start) + "\" ";
				tag += "stop=\"" + expectedTimestamp(e.start + (time_t) e.duration) + "\" ";

				std::string data = s.outPair("title lang=\"de\"", s.outValue(e.title, false), false);
				data += s.outPair("desc lang=\"de\"", s.outValue(e.description, false), true);

				out += s.outArrayItem(tag, data, false);
			}
		}
	}

	std::string whole = s.outObject("tv generator-info-name=\"Neutrino XMLTV Generator v1.0\"", out);
	whole = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE tv SYSTEM \"xmltv.dtd\">\n" + whole;
	s.SendResult(whole);
	return s.result();
}

#endif // DISABLE_LEGACY_API

} // namespace

TEST_CASE("the xmltv route reads at the level a network caller without a credential has", "[xmltv]")
{
	ShippedRoutes shipped;

	const Endpoint *ep = routeFor("/api/v1/epg/xmltv");
	REQUIRE(ep != NULL);
	REQUIRE(ep->method == Get);
	REQUIRE(ep->auth == AuthLevel::Read);

	const Response refused = dispatch(Get, "/api/v1/epg/xmltv", "", "", "8.8.8.8", AuthLevel::Public);
	REQUIRE(refused.code == StatusForbidden);
}

TEST_CASE("only bouquets a viewer would see contribute channels", "[xmltv]")
{
	ShippedRoutes shipped;
	Fixture fx;

	const coreapi::Result<std::string> got = coreapi::epg::xmltv(coreapi::epg::Kind::All);
	REQUIRE(got.ok());
	const std::string doc = readAndRemove(got.value());

	REQUIRE(doc.find(channelIdAttr(Fixture::kTv1)) != std::string::npos);
	REQUIRE(doc.find(channelIdAttr(Fixture::kHiddenChannel)) == std::string::npos);
	REQUIRE(doc.find(channelIdAttr(Fixture::kProviderChannel)) == std::string::npos);
}

TEST_CASE("a mode of tv or radio narrows the channels, both together when it is left out", "[xmltv]")
{
	ShippedRoutes shipped;
	Fixture fx;

	{
		const coreapi::Result<std::string> got = coreapi::epg::xmltv(coreapi::epg::Kind::Tv);
		REQUIRE(got.ok());
		const std::string doc = readAndRemove(got.value());
		REQUIRE(doc.find(channelIdAttr(Fixture::kTv1)) != std::string::npos);
		REQUIRE(doc.find(channelIdAttr(Fixture::kTv2)) != std::string::npos);
		REQUIRE(doc.find(channelIdAttr(Fixture::kRadio1)) == std::string::npos);
	}
	{
		const coreapi::Result<std::string> got = coreapi::epg::xmltv(coreapi::epg::Kind::Radio);
		REQUIRE(got.ok());
		const std::string doc = readAndRemove(got.value());
		REQUIRE(doc.find(channelIdAttr(Fixture::kRadio1)) != std::string::npos);
		REQUIRE(doc.find(channelIdAttr(Fixture::kTv1)) == std::string::npos);
		REQUIRE(doc.find(channelIdAttr(Fixture::kTv2)) == std::string::npos);
	}
	{
		const coreapi::Result<std::string> got = coreapi::epg::xmltv(coreapi::epg::Kind::All);
		REQUIRE(got.ok());
		const std::string doc = readAndRemove(got.value());
		REQUIRE(doc.find(channelIdAttr(Fixture::kTv1)) != std::string::npos);
		REQUIRE(doc.find(channelIdAttr(Fixture::kRadio1)) != std::string::npos);
	}
}

TEST_CASE("a channel with no events still gets its own tag, empty of programmes", "[xmltv]")
{
	ShippedRoutes shipped;
	Fixture fx;
	// kTv2 carries no event in the fixture above.

	const coreapi::Result<std::string> got = coreapi::epg::xmltv(coreapi::epg::Kind::All);
	REQUIRE(got.ok());
	const std::string doc = readAndRemove(got.value());

	const std::string tag = "<channel id=\"" + hex(Fixture::kTv2) + "\">";
	REQUIRE(doc.find(tag) != std::string::npos);
}

TEST_CASE("a channel's schedule is capped at fifty programmes", "[xmltv]")
{
	ShippedRoutes shipped;
	Fixture fx;
	fx.deps.epg.events.clear();

	const size_t total = 60;
	for (size_t i = 0; i < total; ++i)
	{
		char title[32];
		std::snprintf(title, sizeof(title), "event-%zu", i);
		fx.deps.epg.events.push_back(
			makeEvent(Fixture::kTv1, i + 1, (time_t)(i * 3600), 3600, title, ""));
	}

	const coreapi::Result<std::string> got = coreapi::epg::xmltv(coreapi::epg::Kind::All);
	REQUIRE(got.ok());
	const std::string doc = readAndRemove(got.value());

	size_t count = 0;
	for (size_t pos = doc.find("<programme "); pos != std::string::npos;
	     pos = doc.find("<programme ", pos + 1))
		++count;
	REQUIRE(count == 50);

	// The kept half, in the order the source hands them over: dropping from
	// the front rather than the back would still leave fifty, so the identity
	// of what stayed is what a case here has to name.
	REQUIRE(doc.find("event-0<") != std::string::npos);
	REQUIRE(doc.find("event-49<") != std::string::npos);
	REQUIRE(doc.find("event-50<") == std::string::npos);
}

TEST_CASE("names and titles go out through the same escaping the control API uses", "[xmltv]")
{
	ShippedRoutes shipped;
	Fixture fx;

	const coreapi::Result<std::string> got = coreapi::epg::xmltv(coreapi::epg::Kind::All);
	REQUIRE(got.ok());
	const std::string doc = readAndRemove(got.value());

	REQUIRE(doc.find("Info &amp; News") != std::string::npos);
	REQUIRE(doc.find("Second &lt;Live&gt;") != std::string::npos);
	REQUIRE(doc.find("Radio &quot;One&quot;") != std::string::npos);
	REQUIRE(doc.find("Info & News") == std::string::npos);
}

TEST_CASE("when the bouquet list cannot be read the endpoint says so instead of guessing", "[xmltv]")
{
	ShippedRoutes shipped;
	Fixture fx;
	fx.deps.channels.bouquets_status = coreapi::Status::Internal;

	const coreapi::Result<std::string> got = coreapi::epg::xmltv(coreapi::epg::Kind::All);
	REQUIRE_FALSE(got.ok());
	REQUIRE(got.error().status == coreapi::Status::Internal);

	const Response r = dispatch(Get, "/api/v1/epg/xmltv", "", "", "127.0.0.1", AuthLevel::Read);
	REQUIRE(r.code == StatusInternalServerError);
	REQUIRE(r.body.find("bouquet-list-unavailable") != std::string::npos);
}

TEST_CASE("the answer is a file handed over by descriptor, not a copy in the response body", "[xmltv]")
{
	ShippedRoutes shipped;
	Fixture fx;

	const Response r = dispatch(Get, "/api/v1/epg/xmltv", "", "", "127.0.0.1", AuthLevel::Read);
	REQUIRE(r.code == StatusOk);
	REQUIRE(r.fd >= 0);
	REQUIRE(r.body.empty());
	REQUIRE(r.length > 0);

	char buf[8192];
	std::string content;
	ssize_t n;
	while ((n = ::read(r.fd, buf, sizeof(buf))) > 0)
		content.append(buf, (size_t) n);
	::close(r.fd);

	REQUIRE(content.size() == r.length);
	REQUIRE(content.find(channelIdAttr(Fixture::kTv1)) != std::string::npos);
}

TEST_CASE("a thousand xmltv answers leave no files behind", "[xmltv]")
{
	ShippedRoutes shipped;
	Fixture fx;

	ServerConfig c = defaultConfig();
	c.port = 0;
	c.bind_address = "127.0.0.1";
	REQUIRE(start(c));
	const int port = boundPort();

	const size_t before = xmltvFilesInTmp();
	for (int i = 0; i < 1000; ++i)
	{
		const testhttp::Reply r = testhttp::request(port, "GET", "/api/v1/epg/xmltv");
		REQUIRE(r.transport_ok);
		REQUIRE(r.code == 200);
	}
	const size_t after = xmltvFilesInTmp();

	stop();

	REQUIRE(after == before);
}

namespace
{

// A single bouquet of count channels, fifty short, SSO-sized events apiece
// (both strings under the sixteen bytes libstdc++ keeps inline), so what
// this fixture costs to hold is close to count*50 fixed-size structs and
// close to nothing else: a generator's own added cost is then the whole of
// what a peak reading taken around its call can still see move.
void buildEventFixture(InstalledDependencies &deps, size_t count)
{
	deps.channels.bouquets.clear();
	deps.channels.bouquet_members.clear();
	deps.epg.events.clear();

	coreapi::BouquetInfo b;
	b.id = 1;
	b.user_bouquet = true;
	deps.channels.bouquets.push_back(b);

	coreapi::ChannelList members;
	for (size_t i = 0; i < count; ++i)
	{
		const uint64_t id = 0x9000 + i;
		members.push_back(makeChannel(id, "chan", coreapi::ServiceKind::Tv));
		for (size_t k = 0; k < 50; ++k)
			deps.epg.events.push_back(
				makeEvent(id, k + 1, (time_t)(k * 3600), 3600, "prog", ""));
	}
	deps.channels.bouquet_members[1] = members;
}

/* The fixture's own schedule, answered the same way, with one reading taken
   on the way: how much of the document was already in the file when the
   generator asked for this channel's events. A generator that writes each
   fragment out as it builds it has put every channel before this one there
   by then; one that holds the document in a string of its own has put
   nothing there, and that string is what grows with the schedule behind it. */
struct WatchedEpgSource : public FakeEpgSource
{
	mutable size_t bytes_when_last_asked;
	mutable size_t asked;

	WatchedEpgSource() : bytes_when_last_asked(0), asked(0) {}

	coreapi::Status eventsForChannel(coreapi::ChannelId id, time_t from, time_t to,
					 coreapi::EventList &out) const
	{
		asked++;
		bytes_when_last_asked = xmltvBytesInTmp();
		return FakeEpgSource::eventsForChannel(id, from, to, out);
	}
};

} // namespace

TEST_CASE("the document is written out as it is built and never held whole", "[xmltv]")
{
	ShippedRoutes shipped;
	Fixture fx;

	// Two hundred channels with the fifty-events-apiece schedule behind them,
	// which is a document of well over a megabyte: large enough that what is
	// in the file when the last channel is reached is either almost all of it
	// or none of it, with nothing in between for a reading to land on.
	const size_t kChannels = 200;
	buildEventFixture(fx.deps, kChannels);

	WatchedEpgSource watched;
	watched.events = fx.deps.epg.events;
	// The channel list the fixture installed stays, so only the guide is
	// swapped here: this case is about what the generator does while it walks
	// that list.
	InstalledEpgSource in_watched(&watched);

	const coreapi::Result<std::string> got = coreapi::epg::xmltv(coreapi::epg::Kind::All);
	REQUIRE(got.ok());
	const size_t total = readAndRemove(got.value()).size();

	// Every channel was asked about, so the last reading really is the one
	// taken at the end of the walk and not one from a run that stopped early.
	REQUIRE(watched.asked == kChannels);
	REQUIRE(total > 1000000);

	/* By the time the last channel's schedule is asked for, everything the
	   document holds except that one channel is already out of the process.
	   A generator that builds the whole thing in one string first is holding
	   all of it instead and the file is still empty, so this is a floor no
	   such generator can reach rather than a margin it might. What is still
	   in the buffer the file is written through is a few kilobytes against
	   the megabyte below, which is why half is far enough from both. */
	INFO("in the file at the last channel " << watched.bytes_when_last_asked
	     << " of " << total << " bytes");
	REQUIRE(watched.bytes_when_last_asked * 2 > total);
}

#ifndef DISABLE_LEGACY_API

TEST_CASE("kind All writes the same document the control API's own serialiser would", "[xmltv]")
{
	ShippedRoutes shipped;
	Fixture fx;

	const coreapi::Result<std::string> got = coreapi::epg::xmltv(coreapi::epg::Kind::All);
	REQUIRE(got.ok());
	const std::string actual = readAndRemove(got.value());

	const std::string expected =
		expectedDocument(fx.deps.channels, fx.deps.epg, coreapi::epg::Kind::All);
	REQUIRE(actual == expected);
}

#endif // DISABLE_LEGACY_API
