/*
 * channelname.cpp - channels and bouquets by the names people say
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

#include "httpd/mcp/channelname.h"

#include "coreapi/channels.h"
#include "coreapi/base/errors.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <stdint.h>

namespace httpd
{
namespace mcp
{

namespace
{

const size_t kNamedCandidates = 8;

bool readHexId(const std::string &s, coreapi::ChannelId &out)
{
	size_t at = (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) ? 2 : 0;
	const size_t digits = s.size() - at;
	if (digits == 0 || digits > 16)
		return false;
	uint64_t v = 0;
	for (; at < s.size(); ++at)
	{
		const char c = s[at];
		int d = -1;
		if (c >= '0' && c <= '9')
			d = c - '0';
		else if (c >= 'a' && c <= 'f')
			d = 10 + c - 'a';
		else if (c >= 'A' && c <= 'F')
			d = 10 + c - 'A';
		if (d < 0)
			return false;
		v = (v << 4) | (uint64_t) d;
	}
	out = (coreapi::ChannelId) v;
	return true;
}

// The user's own list order decides between channels of one name.
bool ranksBefore(const coreapi::ChannelInfo &a, const coreapi::ChannelInfo &b)
{
	if ((a.number > 0) != (b.number > 0))
		return a.number > 0;
	return a.number > 0 && a.number < b.number;
}

std::string ambiguity(const std::string &text, const coreapi::ChannelList &all,
                      const std::vector<size_t> &hits)
{
	std::string out = text + " names several channels: ";
	for (size_t i = 0; i < hits.size() && i < kNamedCandidates; ++i)
	{
		char id[24];
		std::snprintf(id, sizeof(id), "%llx", (unsigned long long) all[hits[i]].id);
		if (i > 0)
			out += ", ";
		out += all[hits[i]].name + " (" + id + ")";
	}
	if (hits.size() > kNamedCandidates)
	{
		char more[32];
		std::snprintf(more, sizeof(more), " and %lu more", (unsigned long) (hits.size() - kNamedCandidates));
		out += more;
	}
	return out + "; say which by its whole name or its id";
}

} // namespace

std::string folded(const std::string &s)
{
	size_t from = 0;
	size_t to = s.size();
	while (from < to && (s[from] == ' ' || s[from] == '\t'))
		++from;
	while (to > from && (s[to - 1] == ' ' || s[to - 1] == '\t'))
		--to;
	std::string out = s.substr(from, to - from);
	for (size_t i = 0; i < out.size(); ++i)
		if (out[i] >= 'A' && out[i] <= 'Z')
			out[i] = (char) (out[i] - 'A' + 'a');
	return out;
}

coreapi::Result<coreapi::ChannelList> everyChannel()
{
	coreapi::ChannelList all;
	for (int half = 0; half < 2; ++half)
	{
		coreapi::Result<coreapi::ChannelList> got = coreapi::channels::list(half == 0);
		if (!got.ok())
			return coreapi::fail(got.error());
		const coreapi::ChannelList part = std::move(got).value();
		all.insert(all.end(), part.begin(), part.end());
	}
	return coreapi::ok(std::move(all));
}

coreapi::Result<coreapi::ChannelInfo> resolveChannel(const std::string &text)
{
	const std::string want = folded(text);
	if (want.empty())
		return coreapi::fail(coreapi::Status::InvalidArgument, coreapi::ErrorCode::MissingParameter,
		                     "say which channel");

	coreapi::ChannelId id = 0;
	if (readHexId(want, id))
	{
		coreapi::Result<coreapi::ChannelInfo> by_id = coreapi::channels::get(id);
		if (by_id.ok() || by_id.error().status != coreapi::Status::NotFound)
			return by_id;
	}

	coreapi::Result<coreapi::ChannelList> lists = everyChannel();
	if (!lists.ok())
		return coreapi::fail(lists.error());
	const coreapi::ChannelList all = std::move(lists).value();

	std::vector<size_t> exact;
	std::vector<size_t> partial;
	for (size_t i = 0; i < all.size(); ++i)
	{
		const std::string name = folded(all[i].name);
		if (name == want)
			exact.push_back(i);
		else if (name.find(want) != std::string::npos)
			partial.push_back(i);
	}

	if (!exact.empty())
	{
		size_t best = exact[0];
		for (size_t k = 1; k < exact.size(); ++k)
		{
			if (ranksBefore(all[exact[k]], all[best]))
				best = exact[k];
		}
		return coreapi::ok(all[best]);
	}
	if (partial.size() == 1)
		return coreapi::ok(all[partial[0]]);
	if (partial.empty())
		return coreapi::fail(coreapi::Status::NotFound, coreapi::ErrorCode::NoSuchChannel,
		                     "no channel is called " + text);
	return coreapi::fail(coreapi::Status::InvalidArgument, coreapi::ErrorCode::AmbiguousChannel,
	                     ambiguity(text, all, partial));
}

coreapi::Result<coreapi::ChannelList> bouquetChannelsNamed(const std::string &text)
{
	coreapi::Result<coreapi::BouquetList> got = coreapi::channels::bouquets();
	if (!got.ok())
		return coreapi::fail(got.error());
	const coreapi::BouquetList all = std::move(got).value();
	const std::string want = folded(text);
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (folded(all[i].name) == want)
			return coreapi::channels::bouquetChannels(all[i].id);
	}
	return coreapi::fail(coreapi::Status::NotFound, coreapi::ErrorCode::NoSuchBouquet,
	                     "no bouquet is called " + text);
}

} // namespace mcp
} // namespace httpd
