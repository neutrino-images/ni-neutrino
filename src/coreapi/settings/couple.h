/*
 * couple.h - settings that are written together
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

#ifndef __coreapi_couple_h__
#define __coreapi_couple_h__

#include "coreapi/base/result.h"
#include "settings.h"

#include <string>
#include <utility>
#include <vector>

namespace coreapi
{
namespace settings
{

/* A setting a coupling added to the batch and the member that made it do so, so that
   an addition which cannot land takes its trigger down with it and a trigger which
   cannot land leaves no addition behind. */
struct Addition
{
	std::string key;
	std::string trigger;
};

/* What one coupling sees and may change of a write of several settings. It reads the
   members the caller named and, for a setting the batch does not name, what the store
   holds, and it changes the batch only through put(), imply() and refuse(), so every
   change is held to the rules a member of the batch is held to. */
class CoupledBatch
{
public:
	CoupledBatch(BatchOverlay &batch, Refusals &refused, std::vector<Addition> &added);

	// The value the batch writes under key, NULL when it names none.
	const std::string *written(const char *key) const;

	// The number the batch writes under key. False when it names none.
	bool writtenNumber(const char *key, long &out) const;

	// The setting as it stands once the batch has landed. False when it cannot be read.
	bool current(const char *key, std::string &out) const;

	/* Puts a value that follows from the member named because, in place of one the caller
	   named for the same setting: the caller's was only ever a copy of what the coupling
	   derives. The value is held to what check() holds a member to, and where it fails
	   the member named because is refused with that answer instead, since a write that
	   cannot be kept whole must not land in part. */
	bool put(const char *key, const std::string &value, const char *because);

	/* Puts a value the member named because requires. A caller who named the same
	   setting with another value asked for two things that cannot both hold, so both
	   members are refused; the same value is no contradiction and is taken as sent. */
	bool imply(const char *key, const std::string &value, const char *because);

	// Notes that the addition under key is also due to the member named because.
	void link(const char *key, const char *because);

	// Takes the member out of the batch and answers it as a condition that does not hold.
	void refuse(const char *key, const char *message);

	/* Two settings that are one fact in two parts. Written alone, either would leave the
	   other describing something else, so a member whose partner the batch does not name
	   is refused. True when both are named and stay in. */
	bool requirePair(const char *first, const char *second);

private:
	BatchOverlay &batch_;
	Refusals &refused_;
	std::vector<Addition> &added_;
};

typedef void (*Coupling)(CoupledBatch &);

/* Runs every coupling once, in the order the registry names them. No coupling puts a
   setting that another one is triggered by, so the order decides nothing and none can
   start another. Called first by settleBatch(), before any condition is judged: a
   condition has to see the batch the couplings made. */
void applyCouplings(BatchOverlay &batch, Refusals &refused, std::vector<Addition> &added);

// How many couplings the registry runs, which bounds how long settling can take.
size_t couplingCount();

// Two settings written together or not at all, as the couplings of the pair name them.
struct KeyPair
{
	const char *first;
	const char *second;
};

extern const KeyPair kChannelPairs[];
extern const size_t kChannelPairCount;
extern const KeyPair kWeatherPairs[];
extern const size_t kWeatherPairCount;

/* Adds to keys the partner of each key that is one half of a pair, so a reset of one
   half is a reset of both. */
void addPartners(std::vector<std::string> &keys);

// One per area, each in its own file.
void coupleEpg(CoupledBatch &b);
void coupleOsd(CoupledBatch &b);
void coupleChannel(CoupledBatch &b);
void coupleWeather(CoupledBatch &b);
void couplePlugins(CoupledBatch &b);

} // namespace settings
} // namespace coreapi

#endif
