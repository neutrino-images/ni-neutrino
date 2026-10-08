/*
 * apply.h - what makes a changed setting take effect, held in one place
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

#ifndef __coreapi_apply_h__
#define __coreapi_apply_h__

#include "coreapi/base/result.h"

#include <cstddef>
#include <string>
#include <vector>

namespace coreapi
{

/* The points of startup after which a group's driver or daemon can be told
   something. A group names the earliest one it may run after, so a change made
   before that point is not lost: runPhase() runs it when the point is reached. */
enum class ApplyPhase
{
	Framebuffer,
	Decoders,
	Zapit,
	Sectionsd,
	Network,
	Count
};

/* Everything one driver or daemon needs done when any of its settings change.
   A setting that is read where it is used has no group. A group is registered
   once and must outlive the registry; run() reads the settings it needs itself
   and touches drivers and daemons only. */
struct ApplyGroup
{
	const char        *name;
	ApplyPhase         phase;
	const char *const *keys;
	size_t             key_count;
	Status           (*run)();
};

/* Registration is all-or-nothing. Conflict when a key is already in a group,
   which is the one way two groups could run for a single change, and
   InvalidArgument for a group without a name or a run function, or one whose
   phase has already been reached: runPhase() would never run it at startup, so
   the refusal is logged rather than left to show as a setting that is applied
   by writes only. */
Status registerApplyGroup(const ApplyGroup *g);

// NULL: the setting is read where it is used and nothing needs doing.
const ApplyGroup *groupOf(const std::string &key);

/* Ok when the group ran, or when there is no group. Busy when the group's phase
   has not been reached: nothing ran, and runPhase() will run it later. Busy
   here means "deferred", not the "turned away, retry" it means for a command
   sink, so a reply to a caller must not pass it on as the latter. Otherwise
   what the group's run answered. */
Status applyKey(const std::string &key);

/* applyKey for each key, but a group that several of the keys belong to runs
   once. Not-yet-reached groups are skipped the same way applyKey skips them.
   Every group that fails is logged by name and the rest still run, so one
   broken driver does not leave the others unapplied; the answer is the first
   failure, Ok when none failed. */
Status applyBatch(const std::vector<std::string> &keys);

/* Marks the phase reached and runs its groups in the order they were registered,
   so a group that has to follow another is registered after it. Failures are
   logged by name and the answer is the first one, as for applyBatch, because at
   startup nobody else hears of a group that did not take. */
Status runPhase(ApplyPhase p);

/* The one place every group is registered, called once at startup before the first
   runPhase(). A group added anywhere else can miss its phase. */
void registerApplyGroups();

/* Names the calling thread as the one that applies settings: the program's loop,
   which is also where the registry is used. The registry has no lock, so once a
   thread is named, applyBatch() and the drain of written settings refuse to run
   on any other, and say so. Before it is called nothing is refused, which is the
   state of a program that has no loop yet and of a test. */
void bindApplyLoop();

// True on the bound thread, and anywhere while none is bound.
bool onApplyLoop();

/* Empties the registry and forgets every phase and the bound thread. For a test that needs a clean
   registry; nothing in the program calls it. */
void resetApplyRegistry();

} // namespace coreapi

/* Writes an array and its count as one pair, so the two cannot disagree. A group
   is written in src/coreapi/box/apply_<area>.cpp as
   { "name", ApplyPhase::X, COREAPI_KEYS(kNameKeys), &run } with kNameKeys an array
   of string literals in the same file: the scan that holds every setting that
   needs applying to a group reads the keys from that text, and stops on a group
   it cannot read rather than take its keys for none. A group is registered from
   registerApplyGroups only, never from a screen. */
#define COREAPI_KEYS(a) (a), (sizeof(a) / sizeof((a)[0]))

#endif
