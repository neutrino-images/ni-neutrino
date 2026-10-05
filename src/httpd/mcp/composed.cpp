/*
 * composed.cpp - tools composed on the coreAPI rather than one route each
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

#include "httpd/mcp/composed.h"

#include "httpd/endpoints.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/schema.h"
#include "httpd/status.h"
#include "httpd/mcp/channelname.h"
#include "httpd/mcp/picture.h"

#include "coreapi/channels.h"
#include "coreapi/epg.h"
#include "coreapi/osd.h"
#include "coreapi/timers.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <time.h>

#include <timerdclient/timerdtypes.h>

namespace httpd
{
namespace mcp
{

namespace
{

time_t test_clock = 0;

const size_t kWhatsOnMaxChannels = 50;
const time_t kWhatsOnLookahead = 6 * 60 * 60;
const time_t kFindDefaultSpan = 14 * 24 * 60 * 60;
const size_t kFindDefaultLimit = 20;
const long kFindMaxLimit = 50;
const long kMaxQueryBytes = 255;
const long kMaxNameBytes = 255;

const FieldDesc kProgrammeFields[] = {
	HTTPD_MEMBER("id", FieldType::ChannelId,
		"the programme, hexadecimal; pass it with start to record_programme or programme_details"),
	HTTPD_MEMBER("title", FieldType::String, "what it is called"),
	HTTPD_MEMBER("description", FieldType::String, "the short text, empty where there is none"),
	HTTPD_MEMBER("start", FieldType::Time, "when it begins, seconds since the epoch"),
	HTTPD_MEMBER("end", FieldType::Time, "when it ends, seconds since the epoch"),
};
const Schema kProgrammeSchema = { "programme", HTTPD_FIELDS(kProgrammeFields) };

const FieldDesc kOnChannelFields[] = {
	HTTPD_MEMBER("channel_id", FieldType::ChannelId, "the channel, hexadecimal"),
	HTTPD_MEMBER("channel", FieldType::String, "the channel's name as the box lists it"),
	HTTPD_MEMBER("number", FieldType::Int, "the channel's number in the box's list, nought for none"),
	HTTPD_OBJECT_OPTIONAL("now", &kProgrammeSchema, "what is on at the moment asked about, absent for nothing"),
	HTTPD_OBJECT_OPTIONAL("next", &kProgrammeSchema, "what begins after it, absent where the guide holds nothing"),
};
const Schema kOnChannelSchema = { "on-channel", HTTPD_FIELDS(kOnChannelFields) };

const FieldDesc kWhatsOnFields[] = {
	HTTPD_MEMBER("at", FieldType::Time, "the moment asked about, seconds since the epoch"),
	HTTPD_LIST_OF("items", &kOnChannelSchema, "one row per channel, in the order asked for"),
	HTTPD_MEMBER("truncated", FieldType::Bool, "whether the bouquet held more channels than one answer carries"),
};
const Schema kWhatsOnSchema = { "whats-on", HTTPD_FIELDS(kWhatsOnFields) };

const FieldDesc kFoundFields[] = {
	HTTPD_MEMBER("id", FieldType::ChannelId,
		"the programme, hexadecimal; pass it with start to record_programme or programme_details"),
	HTTPD_MEMBER("channel_id", FieldType::ChannelId, "the channel, hexadecimal"),
	HTTPD_MEMBER("channel", FieldType::String, "the channel's name, empty where the box lists no such channel"),
	HTTPD_MEMBER("title", FieldType::String, "what it is called"),
	HTTPD_MEMBER("description", FieldType::String, "the short text, empty where there is none"),
	HTTPD_MEMBER("start", FieldType::Time, "when it begins, seconds since the epoch"),
	HTTPD_MEMBER("end", FieldType::Time, "when it ends, seconds since the epoch"),
	HTTPD_MEMBER("on_air", FieldType::Bool, "whether it is running now"),
};
const Schema kFoundSchema = { "found-programme", HTTPD_FIELDS(kFoundFields) };

const FieldDesc kFindFields[] = {
	HTTPD_LIST_OF("items", &kFoundSchema, "the programmes found, earliest first"),
	HTTPD_MEMBER("truncated", FieldType::Bool, "whether more matched than the limit allowed"),
};
const Schema kFindSchema = { "find-programme", HTTPD_FIELDS(kFindFields) };

void appendProgramme(Json &j, const coreapi::EventInfo &e)
{
	char id[24];
	j.beginObject();
	j.key("id");
	j.value(hexId(e.event_id, id));
	j.key("title");
	j.value(e.title);
	j.key("description");
	j.value(e.description);
	j.key("start");
	j.value((long long) e.start);
	j.key("end");
	j.value((long long) (e.start + (time_t) e.duration));
	j.endObject();
}

bool sameGuideKey(coreapi::ChannelId a, coreapi::ChannelId b)
{
	return (a & coreapi::GUIDE_KEY_MASK) == (b & coreapi::GUIDE_KEY_MASK);
}

bool startsEarlier(const coreapi::EventInfo &a, const coreapi::EventInfo &b)
{
	return a.start < b.start;
}

std::string nameOf(const coreapi::ChannelList &all, coreapi::ChannelId id)
{
	for (size_t i = 0; i < all.size(); ++i)
		if (all[i].id == id)
			return all[i].name;
	for (size_t i = 0; i < all.size(); ++i)
		if (sameGuideKey(all[i].id, id))
			return all[i].name;
	return std::string();
}

Response whatsOn(const Request &r)
{
	const bool by_channel = r.has("channel");
	const bool by_bouquet = r.has("bouquet");
	if (by_channel && by_bouquet)
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::ConflictingParameters,
		                       "name a channel or a bouquet, not both");

	coreapi::ChannelList chosen;
	bool truncated = false;
	if (by_channel)
	{
		coreapi::Result<coreapi::ChannelInfo> one = resolveChannel(r.asString("channel"));
		if (!one.ok())
			return problemFor(one.error());
		chosen.push_back(one.value());
	}
	else if (by_bouquet)
	{
		coreapi::Result<coreapi::ChannelList> members = bouquetChannelsNamed(r.asString("bouquet"));
		if (!members.ok())
			return problemFor(members.error());
		chosen = std::move(members).value();
		if (chosen.size() > kWhatsOnMaxChannels)
		{
			chosen.resize(kWhatsOnMaxChannels);
			truncated = true;
		}
	}
	else
	{
		coreapi::Result<coreapi::ChannelInfo> playing = coreapi::channels::playing();
		if (!playing.ok())
			return problemFor(playing.error());
		chosen.push_back(playing.value());
	}

	const time_t at = r.has("at") ? r.asTime("at") : toolNow();

	Response out = okJson();
	Json j(out.body, 128 + 512 * chosen.size());
	j.beginObject();
	j.key("at");
	j.value((long long) at);
	j.key("items");
	j.beginArray();
	for (size_t c = 0; c < chosen.size(); ++c)
	{
		coreapi::Result<coreapi::EventList> got =
			coreapi::epg::forChannel(chosen[c].id, at, at + kWhatsOnLookahead);
		if (!got.ok())
			return problemFor(got.error());
		const coreapi::EventList events = std::move(got).value();

		const coreapi::EventInfo *now = NULL;
		const coreapi::EventInfo *next = NULL;
		for (size_t i = 0; i < events.size(); ++i)
		{
			const coreapi::EventInfo &e = events[i];
			const time_t end = e.start + (time_t) e.duration;
			if (e.start <= at && at < end)
			{
				if (now == NULL || e.start > now->start)
					now = &e;
			}
			else if (e.start > at && (next == NULL || e.start < next->start))
				next = &e;
		}

		char id[24];
		j.beginObject();
		j.key("channel_id");
		j.value(hexId(chosen[c].id, id));
		j.key("channel");
		j.value(chosen[c].name);
		j.key("number");
		j.value((long) chosen[c].number);
		if (now != NULL)
		{
			j.key("now");
			appendProgramme(j, *now);
		}
		if (next != NULL)
		{
			j.key("next");
			appendProgramme(j, *next);
		}
		j.endObject();
	}
	j.endArray();
	j.key("truncated");
	j.value(truncated);
	j.endObject();
	return out;
}

Response findProgramme(const Request &r)
{
	const std::string &q = r.asString("query");
	const time_t from = r.has("from") ? r.asTime("from") : toolNow();
	const time_t to = r.has("to") ? r.asTime("to") : from + kFindDefaultSpan;
	const size_t limit = r.has("limit") ? (size_t) r.asUInt("limit") : kFindDefaultLimit;

	const bool narrowed = r.has("channel");
	// The guide names each hit by the channel owning the schedule, which a twin only borrows.
	coreapi::ChannelId schedule = 0;
	if (narrowed)
	{
		coreapi::Result<coreapi::ChannelInfo> one = resolveChannel(r.asString("channel"));
		if (!one.ok())
			return problemFor(one.error());
		schedule = one.value().epg_id != 0 ? one.value().epg_id : one.value().id;
	}

	// Searched to the guide's ceiling, as it answers in its own order and not by start.
	coreapi::Result<coreapi::SearchResult> got =
		coreapi::epg::search(q, from, to, coreapi::epg::MAX_SEARCH_RESULTS);
	if (!got.ok())
		return problemFor(got.error());
	const coreapi::SearchResult found = std::move(got).value();

	coreapi::EventList hits;
	for (size_t i = 0; i < found.events.size(); ++i)
	{
		if (!narrowed || sameGuideKey(found.events[i].channel_id, schedule))
			hits.push_back(found.events[i]);
	}
	std::stable_sort(hits.begin(), hits.end(), startsEarlier);
	bool truncated = found.truncated;
	if (hits.size() > limit)
	{
		hits.resize(limit);
		truncated = true;
	}

	coreapi::ChannelList all;
	if (!hits.empty())
	{
		coreapi::Result<coreapi::ChannelList> lists = everyChannel();
		if (!lists.ok())
			return problemFor(lists.error());
		all = std::move(lists).value();
	}

	const time_t now = toolNow();
	Response out = okJson();
	Json j(out.body, 64 + 400 * hits.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < hits.size(); ++i)
	{
		const coreapi::EventInfo &e = hits[i];
		const time_t end = e.start + (time_t) e.duration;
		char id[24];
		j.beginObject();
		j.key("id");
		j.value(hexId(e.event_id, id));
		j.key("channel_id");
		j.value(hexId(e.channel_id, id));
		j.key("channel");
		j.value(nameOf(all, e.channel_id));
		j.key("title");
		j.value(e.title);
		j.key("description");
		j.value(e.description);
		j.key("start");
		j.value((long long) e.start);
		j.key("end");
		j.value((long long) end);
		j.key("on_air");
		j.value(e.start <= now && now < end);
		j.endObject();
	}
	j.endArray();
	j.key("truncated");
	j.value(truncated);
	j.endObject();
	return out;
}

const FieldDesc kRecordFields[] = {
	HTTPD_MEMBER("timer_id", FieldType::UInt, "the timer that records it, as list_timers names it"),
	HTTPD_MEMBER("already_scheduled", FieldType::Bool, "whether the box already had a recording of this showing"),
	HTTPD_MEMBER("started_now", FieldType::Bool, "whether the programme was running and is recorded from now"),
	HTTPD_MEMBER("title", FieldType::String, "the programme"),
	HTTPD_MEMBER("channel_id", FieldType::ChannelId, "the channel, hexadecimal"),
	HTTPD_MEMBER("channel", FieldType::String, "the channel's name, empty where the box lists no such channel"),
	HTTPD_MEMBER("start", FieldType::Time, "when the recording begins, seconds since the epoch"),
	HTTPD_MEMBER("end", FieldType::Time, "when it ends, seconds since the epoch"),
};
const Schema kRecordSchema = { "record-programme", HTTPD_FIELDS(kRecordFields) };

const FieldDesc kSwitchFields[] = {
	HTTPD_MEMBER("channel_id", FieldType::ChannelId, "the channel the box was asked to play, hexadecimal"),
	HTTPD_MEMBER("channel", FieldType::String, "its name as the box lists it"),
	HTTPD_MEMBER("number", FieldType::Int, "its number in the box's list, nought for none"),
};
const Schema kSwitchSchema = { "switch-channel", HTTPD_FIELDS(kSwitchFields) };

Response recordAnswer(uint32_t timer, bool already, bool started, const coreapi::EventDetail &e,
                      time_t start, time_t stop)
{
	// Decoration after the timer exists; failing here would invite a retry.
	std::string name;
	coreapi::Result<coreapi::ChannelInfo> ch = coreapi::channels::get(e.channel_id);
	if (ch.ok())
		name = ch.value().name;

	char id[24];
	Response out = okJson();
	Json j(out.body, 256);
	j.beginObject();
	j.key("timer_id");
	j.value((unsigned long) timer);
	j.key("already_scheduled");
	j.value(already);
	j.key("started_now");
	j.value(started);
	j.key("title");
	j.value(e.title);
	j.key("channel_id");
	j.value(hexId(e.channel_id, id));
	j.key("channel");
	j.value(name);
	j.key("start");
	j.value((long long) start);
	j.key("end");
	j.value((long long) stop);
	j.endObject();
	return out;
}

Response recordProgramme(const Request &r)
{
	coreapi::Result<coreapi::EventDetail> got = coreapi::epg::event(r.asChannelId("id"), r.asTime("start"));
	if (!got.ok())
		return problemFor(got.error());
	const coreapi::EventDetail e = std::move(got).value();
	const time_t end = e.start + (time_t) e.duration;
	const time_t now = toolNow();
	if (end <= now)
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::TimerInThePast,
		                       "the programme has already ended");

	coreapi::Result<coreapi::TimerList> held = coreapi::timers::list();
	if (!held.ok())
		return problemFor(held.error());
	const coreapi::TimerList timers = std::move(held).value();
	for (size_t i = 0; i < timers.size(); ++i)
	{
		const coreapi::TimerInfo &t = timers[i];
		if (t.type == (int) coreapi::TimerType::Record && t.epg_id == e.event_id && t.epg_start == e.start)
			return recordAnswer(t.id, true, false, e, t.start, t.stop);
	}

	const bool on_air = e.start <= now;
	coreapi::TimerInfo t;
	t.type = (int) (on_air ? coreapi::TimerType::ImmediateRecord : coreapi::TimerType::Record);
	t.channel_id = e.channel_id;
	t.start = on_air ? now : e.start;
	t.stop = end;
	t.title = e.title;
	t.epg_id = e.event_id;
	t.epg_start = e.start;
	t.recording_safety = !on_air && (!r.has("margins") || r.asBool("margins"));

	coreapi::Result<uint32_t> made = coreapi::timers::create(t);
	if (!made.ok())
		return problemFor(made.error());
	return recordAnswer(made.value(), false, on_air, e, t.start, t.stop);
}

Response switchChannel(const Request &r)
{
	coreapi::Result<coreapi::ChannelInfo> one = resolveChannel(r.asString("channel"));
	if (!one.ok())
		return problemFor(one.error());
	const coreapi::ChannelInfo ch = one.value();

	coreapi::Result<void> done = coreapi::channels::zap(ch.id, r.asBool("wake"), r.asBool("stop_playback"));
	if (!done.ok())
		return problemFor(done.error());

	char id[24];
	Response out = okJson();
	Json j(out.body, 128);
	j.beginObject();
	j.key("channel_id");
	j.value(hexId(ch.id, id));
	j.key("channel");
	j.value(ch.name);
	j.key("number");
	j.value((long) ch.number);
	j.endObject();
	return out;
}

const char kTimerKinds[] = "record,zap,reminder";
const char kTimerKindDocs[] =
	"record: records the channel from start to end, widened by the box's margins unless margins is false\n"
	"zap: switches the box to the channel at start\n"
	"reminder: shows message on the television at start";

// NULL for every kind this tool leaves alone.
const char *removableKind(int type)
{
	if (type == (int) coreapi::TimerType::Record || type == (int) coreapi::TimerType::ImmediateRecord)
		return "record";
	if (type == (int) coreapi::TimerType::Zapto)
		return "zap";
	if (type == (int) coreapi::TimerType::Remind)
		return "reminder";
	return NULL;
}

const FieldDesc kSetTimerFields[] = {
	HTTPD_MEMBER("timer_id", FieldType::UInt, "the timer, as list_timers names it"),
	HTTPD_MEMBER_OF_SET("kind", kTimerKinds, "what the timer does", kTimerKindDocs),
	HTTPD_MEMBER("channel_id", FieldType::ChannelId, "the channel, hexadecimal"),
	HTTPD_MEMBER("channel", FieldType::String, "its name as the box lists it"),
	HTTPD_MEMBER("start", FieldType::Time, "when the timer fires, seconds since the epoch"),
	HTTPD_MEMBER_OPTIONAL("end", FieldType::Time,
		"when the recording ends, seconds since the epoch; absent for a zap or a reminder"),
};
const Schema kSetTimerSchema = { "set-timer", HTTPD_FIELDS(kSetTimerFields) };

Response setTimer(const Request &r)
{
	const std::string &kind = r.asString("kind");
	coreapi::TimerType type = coreapi::TimerType::Remind;
	if (kind == "record")
		type = coreapi::TimerType::Record;
	else if (kind == "zap")
		type = coreapi::TimerType::Zapto;
	const bool record = type == coreapi::TimerType::Record;
	const bool reminder = type == coreapi::TimerType::Remind;

	if (record && !r.has("end"))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::MissingParameter,
		                       "a recording needs an end");
	if (!record && r.has("end"))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::NoSuchParameter,
		                       "only a recording has an end");
	// An empty text is never bound, so an empty message arrives as none.
	if (reminder && !r.has("message"))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::MissingParameter,
		                       "a reminder needs a message");
	if (!record && r.has("margins"))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::NoSuchParameter,
		                       "only a recording has margins");
	if (!reminder && r.has("message"))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::NoSuchParameter,
		                       "only a reminder has a message");

	coreapi::Result<coreapi::ChannelInfo> one = resolveChannel(r.asString("channel"));
	if (!one.ok())
		return problemFor(one.error());
	const coreapi::ChannelInfo ch = one.value();

	coreapi::TimerInfo t;
	t.type = (int) type;
	t.channel_id = ch.id;
	t.start = r.asTime("start");
	t.stop = record ? r.asTime("end") : 0;
	t.title = reminder ? r.asString("message") : std::string();
	t.recording_safety = record && (!r.has("margins") || r.asBool("margins"));

	coreapi::Result<uint32_t> made = coreapi::timers::create(t);
	if (!made.ok())
		return problemFor(made.error());

	char id[24];
	Response out = okJson();
	Json j(out.body, 192);
	j.beginObject();
	j.key("timer_id");
	j.value((unsigned long) made.value());
	j.key("kind");
	j.value(removableKind(t.type));
	j.key("channel_id");
	j.value(hexId(ch.id, id));
	j.key("channel");
	j.value(ch.name);
	j.key("start");
	j.value((long long) t.start);
	if (record)
	{
		j.key("end");
		j.value((long long) t.stop);
	}
	j.endObject();
	return out;
}

const FieldDesc kRemoveTimerFields[] = {
	HTTPD_MEMBER("timer_id", FieldType::UInt, "the timer that was removed"),
	HTTPD_MEMBER_OF_SET("kind", kTimerKinds, "what the timer did", kTimerKindDocs),
	HTTPD_MEMBER("title", FieldType::String, "the programme of a recording or a zap, the words of a reminder"),
	HTTPD_MEMBER("start", FieldType::Time, "when it was to fire, seconds since the epoch"),
};
const Schema kRemoveTimerSchema = { "remove-timer", HTTPD_FIELDS(kRemoveTimerFields) };

Response removeTimer(const Request &r)
{
	const uint32_t id = (uint32_t) r.asUInt("id");
	coreapi::Result<coreapi::TimerList> held = coreapi::timers::list();
	if (!held.ok())
		return problemFor(held.error());
	const coreapi::TimerList timers = std::move(held).value();
	size_t at = 0;
	while (at < timers.size() && timers[at].id != id)
		++at;
	// Asked here, so a removal never reaches a timer made since under a guessed id.
	if (at == timers.size())
		return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchTimer, "no timer with that id");
	const coreapi::TimerInfo t = timers[at];
	if (removableKind(t.type) == NULL)
		return problemResponse(StatusForbidden, coreapi::ErrorCode::NotPermitted,
		                       "only a record, zap or reminder timer can be removed here; "
		                       "the user removes the others on the box or in ni-web");

	coreapi::Result<void> done = coreapi::timers::remove(id);
	if (!done.ok())
		return problemFor(done.error());

	Response out = okJson();
	Json j(out.body, 192);
	j.beginObject();
	j.key("timer_id");
	j.value((unsigned long) t.id);
	j.key("kind");
	j.value(removableKind(t.type));
	j.key("title");
	j.value(t.title);
	j.key("start");
	j.value((long long) t.start);
	j.endObject();
	return out;
}

const char kRepeatNames[] = "once,daily,weekly,every_two_weeks,monthly,weekdays,weekend";
const char kRepeatNameDocs[] =
	"once: runs one time\n"
	"daily: every day\n"
	"weekly: every week on the same day\n"
	"every_two_weeks: every second week on the same day\n"
	"monthly: every month on the same date\n"
	"weekdays: Monday to Friday\n"
	"weekend: Saturday and Sunday";

struct RepeatName
{
	const char *name;
	int         value;
};

// The timer daemon's numbers: a plain count below weekdays, a weekday bit set at and above it.
const RepeatName kRepeats[] = {
	{ "once", 0 }, { "daily", 1 }, { "weekly", 2 }, { "every_two_weeks", 3 }, { "monthly", 5 },
	{ "weekdays", 256 + 512 + 1024 + 2048 + 4096 + 8192 }, { "weekend", 256 + 16384 + 32768 },
};

const char *repeatName(int value)
{
	for (size_t i = 0; i < sizeof(kRepeats) / sizeof(kRepeats[0]); ++i)
	{
		if (kRepeats[i].value == value)
			return kRepeats[i].name;
	}
	return "other";
}

const FieldDesc kChangeTimerFields[] = {
	HTTPD_MEMBER("timer_id", FieldType::UInt, "the timer as it is now; a new id when the channel moved"),
	HTTPD_MEMBER_OF_SET("kind", kTimerKinds, "what the timer does", kTimerKindDocs),
	HTTPD_MEMBER("channel_id", FieldType::ChannelId, "the channel, hexadecimal"),
	HTTPD_MEMBER("start", FieldType::Time, "when the timer fires, seconds since the epoch"),
	HTTPD_MEMBER_OPTIONAL("end", FieldType::Time, "when the recording ends; absent for a zap or a reminder"),
	HTTPD_MEMBER("repeat", FieldType::String,
		"how it repeats: once, daily, weekly, every_two_weeks, monthly, weekdays, weekend, or other for a pattern set elsewhere"),
	HTTPD_MEMBER("replaced", FieldType::Bool, "whether the timer was made anew because its channel moved"),
};
const Schema kChangeTimerSchema = { "change-timer", HTTPD_FIELDS(kChangeTimerFields) };

Response changedAnswer(uint32_t id, const coreapi::TimerInfo &t, bool replaced)
{
	char cid[24];
	Response out = okJson();
	Json j(out.body, 224);
	j.beginObject();
	j.key("timer_id");
	j.value((unsigned long) id);
	j.key("kind");
	j.value(removableKind(t.type));
	j.key("channel_id");
	j.value(hexId(t.channel_id, cid));
	j.key("start");
	j.value((long long) t.start);
	if (std::string(removableKind(t.type)) == "record")
	{
		j.key("end");
		j.value((long long) t.stop);
	}
	j.key("repeat");
	j.value(repeatName(t.repeat));
	j.key("replaced");
	j.value(replaced);
	j.endObject();
	return out;
}

Response changeTimer(const Request &r)
{
	const uint32_t id = (uint32_t) r.asUInt("id");
	coreapi::Result<coreapi::TimerList> held = coreapi::timers::list();
	if (!held.ok())
		return problemFor(held.error());
	const coreapi::TimerList timers = std::move(held).value();
	size_t at = 0;
	while (at < timers.size() && timers[at].id != id)
		++at;
	if (at == timers.size())
		return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchTimer, "no timer with that id");
	const coreapi::TimerInfo was = timers[at];
	const char *kind = removableKind(was.type);
	if (kind == NULL)
		return problemResponse(StatusForbidden, coreapi::ErrorCode::NotPermitted,
		                       "only a record, zap or reminder timer can be changed here; "
		                       "the user changes the others on the box or in ni-web");
	const bool record = std::string(kind) == "record";
	if (!record && (r.has("end") || r.has("pad_before") || r.has("pad_after")))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::NoSuchParameter,
		                       "only a recording has an end and padding");
	if (!r.has("start") && !r.has("end") && !r.has("channel") && !r.has("repeat") &&
	    !r.has("pad_before") && !r.has("pad_after"))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::MissingParameter,
		                       "name at least one of start, end, channel, repeat, pad_before, pad_after");

	coreapi::TimerInfo now = was;
	if (r.has("start"))
		now.start = r.asTime("start");
	if (r.has("end"))
		now.stop = r.asTime("end");
	if (r.has("pad_before"))
		now.start -= (time_t) r.asUInt("pad_before") * 60;
	if (r.has("pad_after"))
		now.stop += (time_t) r.asUInt("pad_after") * 60;
	if (r.has("repeat"))
	{
		for (size_t i = 0; i < sizeof(kRepeats) / sizeof(kRepeats[0]); ++i)
		{
			if (r.asString("repeat") == kRepeats[i].name)
				now.repeat = kRepeats[i].value;
		}
	}

	if (r.has("channel"))
	{
		coreapi::Result<coreapi::ChannelInfo> one = resolveChannel(r.asString("channel"));
		if (!one.ok())
			return problemFor(one.error());
		if (one.value().id != was.channel_id)
		{
			// The daemon's own flag, not the clock: a one-off past its start that the
			// daemon has already finished with is free to move, same as coreapi::timers::modify.
			if (record && was.state == (int) CTimerd::TIMERSTATE_ISRUNNING)
				return problemResponse(StatusConflict, coreapi::ErrorCode::RecordingRunning,
				                       "a recording that is running keeps its channel");
			now.channel_id = one.value().id;
			now.id = 0;
			// The old programme no longer applies to the new channel.
			now.epg_id = 0;
			now.epg_start = 0;
			now.title.clear();
			coreapi::Result<uint32_t> made = coreapi::timers::create(now);
			if (!made.ok())
				return problemFor(made.error());
			coreapi::Result<void> gone = coreapi::timers::remove(was.id);
			if (!gone.ok())
			{
				(void) coreapi::timers::remove(made.value());
				return problemFor(gone.error());
			}
			return changedAnswer(made.value(), now, true);
		}
	}

	coreapi::Result<void> done = coreapi::timers::modify(now);
	if (!done.ok())
		return problemFor(done.error());
	return changedAnswer(was.id, now, false);
}

const Param kWhatsOnParams[] = {
	HTTPD_QUERY_TEXT("channel",
		"a channel by its name as the box lists it, or its hexadecimal id; with neither this nor bouquet, the channel the box is playing",
		kMaxNameBytes),
	HTTPD_QUERY_TEXT("bouquet", "a bouquet by its name, for what all of its channels are showing", kMaxNameBytes),
	HTTPD_QUERY("at", ParamType::Time, "the moment to ask about, seconds since the epoch; now when left out"),
};

const Param kFindParams[] = {
	HTTPD_QUERY_REQUIRED_IN("query", ParamType::String,
		"words from the title or the texts of the programme, at least two characters and at most 255 bytes",
		0, kMaxQueryBytes),
	HTTPD_QUERY_TEXT("channel", "only this channel, by its name or its hexadecimal id", kMaxNameBytes),
	HTTPD_QUERY("from", ParamType::Time,
		"the earliest moment the programme may still be running, seconds since the epoch; now when left out"),
	HTTPD_QUERY("to", ParamType::Time,
		"the latest moment it may begin, seconds since the epoch; fourteen days after from when left out"),
	HTTPD_QUERY_IN("limit", ParamType::UInt, "how many at most, twenty when left out", 1, kFindMaxLimit),
};

const char kAmbiguousSample[] =
	"erste names several channels: Das Erste (1), Das Erste HD (2); say which by its whole name or its id";

const RouteRefusal kWhatsOnRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, ConflictingParameters, "name a channel or a bouquet, not both"),
	HTTPD_REFUSES(InvalidArgument, AmbiguousChannel, kAmbiguousSample),
	HTTPD_REFUSES(NotFound, NoSuchChannel, "no channel is called arte"),
	HTTPD_REFUSES(NotFound, NoSuchBouquet, "no bouquet is called Sport"),
	HTTPD_REFUSES(NotFound, NoRunningChannel, "nothing is playing, the box is in standby"),
};

const RouteRefusal kFindRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, ValueTooLong, "parameter query is longer than 255 bytes"),
	HTTPD_REFUSES(InvalidArgument, QueryTooShort, "the search needs at least two characters"),
	HTTPD_REFUSES(InvalidArgument, AmbiguousChannel, kAmbiguousSample),
	HTTPD_REFUSES(NotFound, NoSuchChannel, "no channel is called arte"),
	HTTPD_REFUSES(InvalidArgument, EmptyWindow, "the window ends before it begins"),
};

const Param kRecordParams[] = {
	HTTPD_BODY_REQUIRED("id", ParamType::ChannelId,
		"the programme, hexadecimal, as whats_on or find_programme names it"),
	HTTPD_BODY_REQUIRED("start", ParamType::Time,
		"when that showing begins, as whats_on or find_programme gives it beside the id"),
	HTTPD_BODY("margins", ParamType::Bool,
		"whether the box widens the recording by the margins it is set to; true when left out"),
};

const Param kSwitchParams[] = {
	HTTPD_BODY_REQUIRED_TEXT("channel", "the channel by its name as the box lists it, or its hexadecimal id",
		kMaxNameBytes),
	HTTPD_BODY("wake", ParamType::Bool,
		"whether to switch the box on when it is in standby; without it a box in standby refuses with box-in-standby"),
	HTTPD_BODY("stop_playback", ParamType::Bool,
		"whether to end a file the movie player is playing; without it such a playback refuses with playback-running"),
};

const RouteRefusal kRecordRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchEvent, "the guide holds no event beginning then under that identifier"),
	HTTPD_REFUSES(InvalidArgument, TimerInThePast, "the programme has already ended"),
	HTTPD_REFUSES(Conflict, TimerExists, "the box already has a timer like this one"),
	HTTPD_REFUSES(InvalidArgument, TimerWithoutChannel, "this kind of timer needs a channel"),
};

const RouteRefusal kSwitchRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, AmbiguousChannel, kAmbiguousSample),
	HTTPD_REFUSES(NotFound, NoSuchChannel, "no channel is called arte"),
	HTTPD_REFUSES(Conflict, BoxInStandby, "the box is in standby"),
	HTTPD_REFUSES(Conflict, RecordingHoldsTuner, "a recording holds the tuner this channel needs"),
	HTTPD_REFUSES(Conflict, PlaybackRunning, "something is playing in the movie player"),
};

const long kMaxMessageBytes = 255;

const Param kSetTimerParams[] = {
	HTTPD_BODY_REQUIRED_FROM_SET("kind", "what the timer does when it fires", kTimerKinds, kTimerKindDocs),
	HTTPD_BODY_REQUIRED_TEXT("channel", "the channel by its name as the box lists it, or its hexadecimal id",
		kMaxNameBytes),
	HTTPD_BODY_REQUIRED("start", ParamType::Time, "when the timer fires, seconds since the epoch"),
	HTTPD_BODY("end", ParamType::Time,
		"when the recording ends, seconds since the epoch; record only, and required there"),
	HTTPD_BODY("margins", ParamType::Bool,
		"whether the box widens the recording by the margins it is set to; record only, true when left out"),
	HTTPD_BODY_TEXT("message", "what the reminder shows on the television; reminder only, and required there",
		kMaxMessageBytes),
};

const RouteRefusal kSetTimerRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, MissingParameter, "a recording needs an end"),
	HTTPD_REFUSES(InvalidArgument, NoSuchParameter, "only a recording has an end"),
	HTTPD_REFUSES(InvalidArgument, AmbiguousChannel, kAmbiguousSample),
	HTTPD_REFUSES(NotFound, NoSuchChannel, "no channel is called arte"),
	HTTPD_REFUSES(InvalidArgument, TimerInThePast, "a timer that runs once cannot begin before now"),
	HTTPD_REFUSES(InvalidArgument, RecordingWithoutDuration, "a recording has to end after it begins"),
	HTTPD_REFUSES(Conflict, TimerExists, "the box already has a timer like this one"),
};

// The daemon counts its ids in an int.
const long kMaxTimerId = 2147483647L;

const Param kRemoveTimerParams[] = {
	HTTPD_SEGMENT_IN("id", ParamType::UInt, "the timer, by the id list_timers gives it", 1, kMaxTimerId),
};

const RouteRefusal kRemoveTimerRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchTimer, "no timer with that id"),
	HTTPD_REFUSES(Denied, NotPermitted,
		"only a record, zap or reminder timer can be removed here; the user removes the others on the box or in ni-web"),
	HTTPD_REFUSES(Internal, TimerStillThere, "the box still has the timer"),
};

const Param kChangeTimerParams[] = {
	HTTPD_SEGMENT_IN("id", ParamType::UInt, "the timer, by the id list_timers gives it", 1, kMaxTimerId),
	HTTPD_BODY("start", ParamType::Time, "the new moment it fires, seconds since the epoch"),
	HTTPD_BODY("end", ParamType::Time, "the new end of a recording, seconds since the epoch; record only"),
	HTTPD_BODY_TEXT("channel", "another channel by its name as the box lists it, or its hexadecimal id", kMaxNameBytes),
	HTTPD_BODY_FROM_SET("repeat", "how the timer repeats from now on", kRepeatNames, kRepeatNameDocs),
	HTTPD_BODY_IN("pad_before", ParamType::UInt, "minutes to start earlier; record only", 0, 120),
	HTTPD_BODY_IN("pad_after", ParamType::UInt, "minutes to end later; record only", 0, 120),
};

const RouteRefusal kChangeTimerRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchTimer, "no timer with that id"),
	HTTPD_REFUSES(Denied, NotPermitted,
		"only a record, zap or reminder timer can be changed here; the user changes the others on the box or in ni-web"),
	HTTPD_REFUSES(InvalidArgument, MissingParameter, "name at least one thing to change"),
	HTTPD_REFUSES(InvalidArgument, NoSuchParameter, "only a recording has an end and padding"),
	HTTPD_REFUSES(Conflict, RecordingRunning, "a recording that is running keeps its channel"),
	HTTPD_REFUSES(InvalidArgument, AmbiguousChannel, kAmbiguousSample),
	HTTPD_REFUSES(NotFound, NoSuchChannel, "no channel is called arte"),
	HTTPD_REFUSES(InvalidArgument, TimerInThePast, "a timer that runs once cannot begin before now"),
	HTTPD_REFUSES(InvalidArgument, RecordingWithoutDuration, "a recording has to end after it begins"),
};

const FieldDesc kScreenshotFields[] = {
	HTTPD_MEMBER("mime_type", FieldType::String, "image/jpeg"),
	HTTPD_MEMBER("data", FieldType::String, "the picture, base64"),
	HTTPD_MEMBER("width", FieldType::UInt, "its width in pixels, at most 1280"),
	HTTPD_MEMBER("height", FieldType::UInt, "its height in pixels"),
};
const Schema kScreenshotSchema = { "screenshot", HTTPD_FIELDS(kScreenshotFields) };

// The box writes full HD or more; a few megabytes are already far more than fits.
const size_t kMaxCaptureBytes = 8 * 1024 * 1024;

Response screenshot(const Request &r)
{
	const bool osd = !r.has("osd") || r.asBool("osd");
	const bool video = !r.has("video") || r.asBool("video");
	coreapi::Result<std::string> taken =
		coreapi::osd::screenshotBytes(osd, video, coreapi::PictureFormat::Jpeg, kMaxCaptureBytes);
	if (!taken.ok())
		return problemFor(taken.error());
	const std::string &bytes = taken.value();
	if (bytes.size() < 2 || (unsigned char) bytes[0] != 0xFF || (unsigned char) bytes[1] != 0xD8)
		return problemResponse(StatusInternalServerError, coreapi::ErrorCode::ScreenNotCaptured,
		                       "the picture the box took could not be read");
	std::string fitted;
	unsigned w = 0;
	unsigned h = 0;
	if (!fitJpeg(bytes, 1280, kMaxImageText, fitted, w, h))
		return problemResponse(StatusInternalServerError, coreapi::ErrorCode::OutputTooLarge,
		                       "the picture could not be made small enough to send");
	Response out = okJson();
	Json j(out.body, 64 + fitted.size() / 3 * 4);
	j.beginObject();
	j.key("mime_type");
	j.value("image/jpeg");
	j.key("data");
	j.value(encodeBase64(fitted));
	j.key("width");
	j.value((unsigned long) w);
	j.key("height");
	j.value((unsigned long) h);
	j.endObject();
	return out;
}

const Param kScreenshotParams[] = {
	HTTPD_QUERY("osd", ParamType::Bool, "whether the menus and banners the box draws are in the picture; true when left out"),
	HTTPD_QUERY("video", ParamType::Bool, "whether the television picture is in it; true when left out"),
};

const RouteRefusal kScreenshotRefusals[] = {
	HTTPD_REFUSES(Internal, ScreenNotCaptured, "the picture the box took could not be read"),
	HTTPD_REFUSES(Busy, ScreenNotCaptured, "the box is already taking a picture of its screen"),
	HTTPD_REFUSES(Internal, OutputTooLarge, "the picture could not be made small enough to send"),
};

const Endpoint kComposedEndpoints[] = {
	{ Method::Get, "/mcp/tools/whats_on", AuthLevel::Read,
	  "what is on now and next on a channel, a bouquet or the channel playing",
	  "What is on now and next. Name a channel (its name as the box lists it, for example \"Das "
	  "Erste HD\", or its hexadecimal id) or a bouquet; with neither, the channel the box is "
	  "playing, which is refused with no-running-channel in standby and while a recording or a file plays. at asks about another moment, in seconds since the epoch. Each programme carries "
	  "the id and start that record_programme and programme_details take.",
	  HTTPD_PARAMS(kWhatsOnParams), &kWhatsOnSchema, &whatsOn, false,
	  Answers200, HTTPD_REFUSALS(kWhatsOnRefusals) },
	{ Method::Get, "/mcp/tools/find_programme", AuthLevel::Read,
	  "programmes whose title or text matches, earliest first",
	  "Searches the programme guide for words from a title or description, from now (or from) for "
	  "fourteen days (or until to), optionally on one channel only. Each hit carries the id and "
	  "start that record_programme and programme_details take.",
	  HTTPD_PARAMS(kFindParams), &kFindSchema, &findProgramme, false,
	  Answers200, HTTPD_REFUSALS(kFindRefusals) },
	{ Method::Put, "/mcp/tools/record_programme", AuthLevel::Write,
	  "records one programme by the id and start the guide gives it",
	  "Records one programme, named by the id and start that whats_on or find_programme returned. "
	  "A programme already running is recorded from now to its end. Asking again for the same "
	  "programme records it once; already_scheduled says so.",
	  HTTPD_PARAMS(kRecordParams), &kRecordSchema, &recordProgramme, false,
	  Answers200, HTTPD_REFUSALS(kRecordRefusals) },
	{ Method::Put, "/mcp/tools/switch_channel", AuthLevel::Write,
	  "switches the box to a channel named by its name or its id",
	  "Switches the box to a channel, named by its name as the box lists it or by its hexadecimal "
	  "id. A file the movie player is playing refuses with playback-running unless stop_playback is "
	  "true; a box in standby refuses with box-in-standby unless wake is true.",
	  HTTPD_PARAMS(kSwitchParams), &kSwitchSchema, &switchChannel, false,
	  Answers200, HTTPD_REFUSALS(kSwitchRefusals) },
	{ Method::Post, "/mcp/tools/set_timer", AuthLevel::Write,
	  "makes one recording, zap or reminder timer on a channel named by its name or its id",
	  "Makes one timer on a channel named by its name as the box lists it or by its hexadecimal "
	  "id. kind record records from start to end, widened by the box's margins unless margins is "
	  "false; zap switches to the channel at start; reminder shows message on the television at "
	  "start. Times are seconds since the epoch. To record a programme from the guide, use "
	  "record_programme with its id and start instead. It changes no timer; remove_timer removes "
	  "one.",
	  HTTPD_PARAMS(kSetTimerParams), &kSetTimerSchema, &setTimer, false,
	  Answers200, HTTPD_REFUSALS_AND_BODY(kSetTimerRefusals,
		"{\"kind\":\"record\",\"channel\":\"Das Erste\",\"start\":2000000000,\"end\":2000003600,"
		"\"margins\":true}") },
	{ Method::Delete, "/mcp/tools/remove_timer/{id}", AuthLevel::Write,
	  "removes one record, zap or reminder timer by its id",
	  "Removes one timer by the id list_timers gives it, if it is a record, zap or reminder timer; "
	  "a timer of any other kind is refused with not-permitted and stays. Removing the timer of a "
	  "recording that is running stops that recording. A repeating timer is removed with all its "
	  "future runs.",
	  HTTPD_PARAMS(kRemoveTimerParams), &kRemoveTimerSchema, &removeTimer, false,
	  Answers200, HTTPD_REFUSALS(kRemoveTimerRefusals) },
	{ Method::Patch, "/mcp/tools/change_timer/{id}", AuthLevel::Write,
	  "changes one record, zap or reminder timer by its id",
	  "Changes one timer by the id list_timers gives it, if it is a record, zap or reminder timer: start, "
	  "end (record only), repeat, or the channel. pad_before and pad_after widen a recording by minutes. "
	  "Moving a timer to another channel makes it anew and answers its new id with replaced true; a "
	  "recording that is running keeps its channel. Times are seconds since the epoch.",
	  HTTPD_PARAMS(kChangeTimerParams), &kChangeTimerSchema, &changeTimer, false,
	  Answers200, HTTPD_REFUSALS_AND_BODY(kChangeTimerRefusals, "{\"start\":2000000600,\"pad_after\":10}") },
	{ Method::Get, "/mcp/tools/screenshot", AuthLevel::Read,
	  "a picture of what the television shows now",
	  "A picture of what the television shows now, as an image at most 1280 pixels wide. osd false leaves "
	  "out the menus and banners the box draws; video false leaves out the television picture. Pictures of "
	  "protected channels can be black.",
	  HTTPD_PARAMS(kScreenshotParams), &kScreenshotSchema, &screenshot, false,
	  Answers200, HTTPD_REFUSALS(kScreenshotRefusals) },
};

const ToolFlag kComposedTools[] = {
	HTTPD_TOOL_AS(Method::Get, "/mcp/tools/whats_on", "whats_on", NULL),
	HTTPD_TOOL_AS(Method::Get, "/mcp/tools/find_programme", "find_programme", NULL),
	HTTPD_TOOL_AS(Method::Put, "/mcp/tools/record_programme", "record_programme", NULL),
	HTTPD_TOOL_AS(Method::Put, "/mcp/tools/switch_channel", "switch_channel", NULL),
	HTTPD_TOOL_AS(Method::Post, "/mcp/tools/set_timer", "set_timer", NULL),
	HTTPD_TOOL_AS(Method::Delete, "/mcp/tools/remove_timer/{id}", "remove_timer", NULL),
	HTTPD_TOOL_AS(Method::Patch, "/mcp/tools/change_timer/{id}", "change_timer", NULL),
	HTTPD_TOOL_IMAGE(Method::Get, "/mcp/tools/screenshot", "screenshot", NULL),
};

const RouteTable kComposedTable = {
	HTTPD_TABLE_WITH_TOOLS("composed", kComposedEndpoints, kComposedTools)
};

} // namespace

time_t toolNow()
{
	return test_clock != 0 ? test_clock : time(NULL);
}

void setToolClockForTest(time_t now)
{
	test_clock = now;
}

const RouteTable &composedTable()
{
	return kComposedTable;
}

} // namespace mcp
} // namespace httpd
