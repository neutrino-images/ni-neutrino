/*
 * couple.cpp - the registry of settings that are written together
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

#include "couple.h"

#include "coreapi/base/errors.h"

#include <cerrno>
#include <cstdlib>

namespace coreapi
{
namespace settings
{

CoupledBatch::CoupledBatch(BatchOverlay &batch, Refusals &refused, std::vector<Addition> &added)
	: batch_(batch), refused_(refused), added_(added)
{
}

const std::string *CoupledBatch::written(const char *key) const
{
	for (size_t i = 0; i < batch_.values.size(); ++i)
	{
		if (batch_.values[i].first == key)
			return &batch_.values[i].second;
	}
	return NULL;
}

namespace
{

bool numberOf(const std::string &text, long &out)
{
	if (text.empty())
		return false;
	errno = 0;
	char *end = NULL;
	const long n = strtol(text.c_str(), &end, 10);
	if (errno != 0 || *end != '\0')
		return false;
	out = n;
	return true;
}

} // anonymous namespace

bool CoupledBatch::writtenNumber(const char *key, long &out) const
{
	const std::string *text = written(key);
	return text != NULL && numberOf(*text, out);
}

bool CoupledBatch::current(const char *key, std::string &out) const
{
	const std::string *text = written(key);
	if (text != NULL)
	{
		out = *text;
		return true;
	}
	Result<std::string> stored = get(key);
	if (!stored.ok())
		return false;
	out = stored.value();
	return true;
}

void CoupledBatch::refuse(const char *key, const char *message)
{
	for (size_t i = 0; i < batch_.values.size(); ++i)
	{
		if (batch_.values[i].first != key)
			continue;
		refused_.push_back(std::make_pair(std::string(key),
			Error(Status::Conflict, ErrorCode::SettingConditionNotMet, message)));
		batch_.values.erase(batch_.values.begin() + i);
		return;
	}
}

void CoupledBatch::link(const char *key, const char *because)
{
	Addition a;
	a.key = key;
	a.trigger = because;
	added_.push_back(a);
}

bool CoupledBatch::put(const char *key, const std::string &value, const char *because)
{
	Result<void> checked = check(key, value);
	if (!checked.ok())
	{
		for (size_t i = 0; i < batch_.values.size(); ++i)
		{
			if (batch_.values[i].first != because)
				continue;
			refused_.push_back(std::make_pair(std::string(because), checked.error()));
			batch_.values.erase(batch_.values.begin() + i);
			break;
		}
		return false;
	}

	for (size_t i = 0; i < batch_.values.size(); ++i)
	{
		if (batch_.values[i].first == key)
		{
			batch_.values[i].second = value;
			return true;
		}
	}
	batch_.values.push_back(std::make_pair(std::string(key), value));
	link(key, because);
	return true;
}

bool CoupledBatch::imply(const char *key, const std::string &value, const char *because)
{
	const std::string *named = written(key);
	if (named != NULL)
	{
		long have = 0;
		long want = 0;
		if (numberOf(*named, have) && numberOf(value, want) ? have == want : *named == value)
			return true;
		static const char *const kSaid = "the setting contradicts another one written with it";
		refuse(key, kSaid);
		refuse(because, kSaid);
		return false;
	}
	return put(key, value, because);
}

bool CoupledBatch::requirePair(const char *first, const char *second)
{
	const bool has_first = written(first) != NULL;
	const bool has_second = written(second) != NULL;
	if (has_first && has_second)
		return true;
	if (has_first)
		refuse(first, "the setting is one fact with another and is written together with it");
	if (has_second)
		refuse(second, "the setting is one fact with another and is written together with it");
	return false;
}

namespace
{

const Coupling kCouplings[] =
{
	coupleEpg,
	coupleOsd,
	coupleChannel,
	coupleWeather,
	couplePlugins
};

void partnersFrom(const KeyPair *pairs, size_t n, std::vector<std::string> &keys)
{
	const size_t had = keys.size();
	for (size_t i = 0; i < n; ++i)
	{
		for (size_t k = 0; k < had; ++k)
		{
			const char *other = NULL;
			if (keys[k] == pairs[i].first)
				other = pairs[i].second;
			else if (keys[k] == pairs[i].second)
				other = pairs[i].first;
			if (other == NULL)
				continue;
			bool present = false;
			for (size_t m = 0; m < keys.size() && !present; ++m)
				present = keys[m] == other;
			if (!present)
				keys.push_back(other);
		}
	}
}

} // anonymous namespace

void addPartners(std::vector<std::string> &keys)
{
	partnersFrom(kChannelPairs, kChannelPairCount, keys);
	partnersFrom(kWeatherPairs, kWeatherPairCount, keys);
}

size_t couplingCount()
{
	return sizeof(kCouplings) / sizeof(kCouplings[0]);
}

void applyCouplings(BatchOverlay &batch, Refusals &refused, std::vector<Addition> &added)
{
	CoupledBatch coupled(batch, refused, added);
	for (size_t i = 0; i < sizeof(kCouplings) / sizeof(kCouplings[0]); ++i)
		kCouplings[i](coupled);
}

} // namespace settings
} // namespace coreapi
