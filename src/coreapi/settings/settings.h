/*
 * settings.h - reading and writing the box settings
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

#ifndef __coreapi_settings_h__
#define __coreapi_settings_h__

#include "coreapi/base/result.h"
#include "coreapi/base/schema.h"

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace coreapi
{
namespace settings
{

/* Answered out of the declaration and not out of the store, so a box with no
   store behind it still answers. A row the schema marks secret keeps its place
   and loses its declared default. Each row is in the shape this box offers it,
   and one the box lacks is in as declared, see rowOnThisBox in schema.h. */
Result<std::vector<Descriptor> > schema();

// The declared row for a key, NULL when nothing declares it. Unlike describe() the
// secret default is not withheld, so only a caller that never forwards it may use this.
const Descriptor *findRow(const std::string &key);

/* Whether the parental lock holds this row right now. Also true when whether the
   box is locked cannot be read, so a failed read never unlocks a row. Asks the
   box only for a row the lock can hold. */
bool lockedNow(const std::string &key);

/* Whether the row's conditions hold on what the store holds now, which is whether
   a frontend should let the row be changed. True for a key nothing declares and
   for a condition nothing can answer, the way set() judges them. */
bool conditionsHoldNow(const std::string &key);

// Each section once, in the order the schema first names it, so a frontend can
// lay out its menu without walking the whole schema.
Result<std::vector<std::string> > sections();

/* The other half of a setting that is one fact in two parts, NULL for one that is not.
   writes says how the pair is written: "both" together or not at all, or "id" where the
   identifier alone is taken and the box fills the other half from it. */
const char *pairPartner(const std::string &key, const char **writes = NULL);

// "tv" or "radio" for a start channel row, the list its channel comes from; NULL otherwise.
const char *startChannelKind(const std::string &key);

// Whether the value names a file or folder on the box.
bool holdsPath(const Descriptor &d);

bool sectionHoldsSecret(const std::string &section);

/* Whether a text is one the rule takes: its length, its characters, and for a
   path the file or folder it names. An empty text is refused where the rule names
   a place, unless the rule says allow_empty. A value equal to current, when given,
   skips the tests about the place, so what is stored never has to be proven
   again. Touches the file system only for a rule that asks for an existing path. */
Result<void> holdsTextRule(const TextRule &rule, const std::string &value,
			   const std::string *current = NULL);

/* Whether a row that takes its list from a provider accepts a text, or a number for the
   second. Only an entry the provider offers now, except the value given as always, which
   is what the row holds without an entry, such as its default. A caller adds the value
   stored, which passes again so that an entry absent at this moment never makes the
   stored setting unwritable, after this has refused. A provider that cannot say, or says
   nothing, holds the row to no list. */
Result<void> holdsOffered(ChoiceSource from, const std::string &text,
			  const std::string *always = NULL);
Result<void> holdsOfferedNumber(ChoiceSource from, long number, const long *always = NULL);

// Whether a folder on a file system of this type (the kernel's type number) is
// acceptable at the given level of MustExist.
bool fileSystemAllowed(MustExist level, long fs_type);

// NotFound for a key nothing declares: a zeroed descriptor would be a lie a
// caller cannot tell from a setting that really has no label and no bounds. A
// secret row comes back with its default withheld, and every row in the shape
// this box offers it.
Result<Descriptor> describe(const std::string &key);

/* The current value, rendered the way the wire carries it. A key the store has never held
   reads as what the row declares, that being what the program's own load does with one. A
   secret row answers the empty string and the store is not asked at all. What a write left
   held reads back before the box has taken it, so a caller that writes and reads is not
   told its own write did nothing; a write this layer answered with an error left nothing.
   A row the box lacks still reads: the value is in the settings file whatever the box has. */
Result<std::string> get(const std::string &key);

/* The settings as they stand now, as a condition reads them, for a list whose entries
   depend on another setting. */
ValueLookup currentValues();

// Each setting a write could not keep, with the answer its own write would have had.
typedef std::vector<std::pair<std::string, Error> > Refusals;

/* What a write of several settings will leave behind, as key and wire text, so each of
   them is judged on that and not on what the store holds before any has landed. Only
   values check() passed and settleBatch() kept belong in it: a value that never lands
   must not be what allows another one. */
struct BatchOverlay
{
	std::vector<std::pair<std::string, std::string> > values;
	/* Credentials a coupling empties because the setting that kept them went off. A write of
	   nothing is refused for a credential everywhere else, so the landing needs to know this
	   one is the coupling's and not a caller's. */
	std::vector<std::string> cleared;
};

/* One setting, given the way the wire carries it, so what get() answered can be sent again
   unchanged. The value is held to three things and each refuses a value the next would not:
   what the settings file can carry, what the declaration allows, and what the field behind
   the row can hold.

   The first is what bounds a String. The program writes a setting as its key, a separator
   and the value, reads one back by splitting the line at the first separator and cutting the
   rest off at the first number sign, and escapes nothing either way. So a String may hold
   text of one line: no byte that ends a line and no other control byte, no zero byte, no
   number sign, no space at either end, and at most four kilobytes.

   A secret row refuses an empty value, so a form redrawn from a read that answered nothing
   cannot clear the credential; emptying one is clearSecret. A row lockedNow() holds is
   refused with setting-locked whatever the value, and one the box lacks with
   setting-not-on-this-box. A row in two shapes is held to the one this box offers.

   A value that passed all three is still refused with setting-condition-not-met where the
   row's conditions do not hold. They are judged on batch first and the store after it, so
   a setting and the one it depends on can be written in one batch in either order. Without
   a batch the conditions are judged on what the store holds. A condition nothing can
   answer holds.

   ok says the value passed all three and the conditions, the store took it, and the box
   was asked on its own loop to put it into the program's settings, save them and tell
   whoever applies that section. It does not say the file has been written or that
   anything has been applied: the loop answers nothing and waiting for it from here
   deadlocks. Called with onLoop by a caller that is the program's loop, the value is in
   effect and saved when this returns.

   setting-not-written says the value is not in the box and will not get there, and what this
   call wrote is taken back rather than left for a later write of some other setting to carry
   in. What is taken back is this call's own write, so two requests at once do not undo each
   other. The one thing it cannot promise is that nothing landed.

   A value equal to the stored one, a missing entry counting as its default, skips the
   conditions and is answered ok without a write, a save or an apply. */
Result<void> set(const std::string &key, const std::string &value, const BatchOverlay *batch = NULL,
		 bool onLoop = false, const std::string &who = std::string());

/* set() without the conditions and without the store: the same answer set() gives for
   everything about the key and the value alone, and nothing is written. The first pass of
   a write of several settings, so a value refused for itself never enters the batch. */
Result<void> check(const std::string &key, const std::string &value);

/* What check() would say to emptying a credential a coupling clears: every rule but the
   one against a write of nothing. */
Result<void> checkCleared(const std::string &key);

/* The second pass of a write of several settings, between check() and set(). First runs the
   couplings over batch, which are the settings that cannot be written apart:
   - writing the guide's save on puts its read on, and the module line follows its position;
     a member the caller names with a value that contradicts that is refused together with
     the member that implies it, while an equal value is taken as sent;
   - the name and identifier of a start channel and the city and coordinates of the weather
     are written together or not at all, and a weather pair clears the postal code;
   - the five plugin lists stay one partition: a name written into one list leaves the
     others, and two lists written together naming one plugin are both refused.
   A coupling puts its values into batch, so a caller writes batch afterwards and not the
   members it named. All of them are refused with setting-condition-not-met although the
   schema states no condition for them: the answer names the pair or the clash.

   Then takes out of batch every member whose conditions do not hold on batch and the store,
   and again until a round takes out nothing, because a member taken out can be what another
   one leaned on. The couplings are made over from what is left each round, so what a member
   brought with it leaves with it, and a setting a coupling added that cannot land takes out
   the member that asked for it. Each one taken out, added settings too, is put into refused
   under its own key with setting-condition-not-met. What is left then holds together:
   written with set() and this batch, every member lands on conditions the landed members
   and the store satisfy, whatever order the caller named them in.

   Two things it does not promise. The outcome is not the largest set that could land: two
   members whose conditions each refuse the other's new value are both taken out, although
   either alone might have been allowed. And a store that fails to take a member in the
   write after this cannot be foreseen here, so a member may still land on one that did
   not.

   A member equal to the stored value is judged by no condition and is taken out of batch as
   done, not put into refused, so nothing is written, saved or applied for it. keepUnchanged
   leaves such members in and judges them, for a menu that has put the value into the
   program's settings itself and needs its key applied. */
void settleBatch(BatchOverlay &batch, Refusals &refused, bool keepUnchanged = false);

/* All three passes of a write of several settings: check() on each, settleBatch(), then what
   set() does on what is left, with one save for all of them. The one entry for a caller that
   writes more than one setting, so none can skip a coupling. Every member is in the store
   before the save carries them in, so whoever applies them runs once on the whole batch and
   never on part of it. Each key may be named once. Every setting that did not land, whichever pass
   refused it, is in failed with the answer its own write would have had, and what the
   couplings added is among them when it fails.

   who, for set() too, is the writer as currentWriter() names one; a failure the box meets
   later, putting what landed in force, is reported to that writer alone. */
void writeBatch(const std::vector<std::pair<std::string, std::string> > &members, Refusals &failed,
		bool onLoop = false, const std::string &who = std::string());

/* What a menu item asks for once it has put a new value of key into the program's settings
   itself, on the program's loop. The couplings run on that value as they do on a write of
   this layer, and what they add that differs from the store is written with the key and
   saved, so a menu and a web write leave the same settings, and the open menus hear which
   settings moved. Whoever applies them runs once. Where the couplings add nothing the key
   alone is applied. */
Status menuChanged(const std::string &key);

/* Sets each listed setting to the default its row declares on this box, as writeBatch() does:
   every value is held to what check() holds one to, the couplings run, and the conditions are
   judged on what the reset leaves. A setting that falls out of that is in refused with the
   answer a write of its default would have had, and the others are written; one the box
   lacks, one that is a credential, and one whose conditions do not hold are among them. A key
   nothing declares is NotFound and nothing is written. One half of a pair that is written
   together is reset with its partner, the partner then being in the reset like any key.

   Reads the rows and not a list kept beside them, so a reset and a fresh start cannot
   disagree about what the default is.

   onLoop is for the caller that runs on the program's own loop, a menu: the writes are then
   in effect and saved when this returns, so the screen it repaints shows them. A write from
   any other thread leaves it false and is carried by a message to the loop. */
Result<void> resetDefaults(const std::vector<std::string> &keys, Refusals &refused, bool onLoop = false);

/* Empties a credential, the one thing set() will not do. The two rules that protect one
   leave no way to remove it: a read answers nothing, so a form redrawn from what it read
   offers nothing back, and a write of nothing is refused so that the round trip cannot wipe
   it.

   not-a-credential rather than no-such-setting for a key that is declared and is not one,
   because the key is right and telling a caller there is no such setting sends it looking
   for a name it already has.

   who is the writer as for writeBatch(). */
Result<void> clearSecret(const std::string &key, const std::string &who = std::string());

/* A row carrying its own list, or naming a provider for one that exists only at run time,
   answers it with each label already resolved to the text the box would show, and without
   the entries the box lacks. A provider's entry for a String row carries the text the row
   stores, and its label is that text where it names no other.

   choices-unavailable covers a setting that offers no set at all, one whose entries
   the box has none of and a provider that cannot say or lists nothing.
   setting-not-on-this-box is a row the box lacks. An empty list is
   not that answer and does not happen, so ok always carries at least one value. */
Result<std::vector<SettingChoice> > choices(const std::string &key);

/* The text a label_key names, in whatever language the box has loaded, through the seam in
   deps.h. False for a NULL key and for one the catalog carries nothing under, answered alike
   so that a caller may run this straight off either field without a NULL check. Neither ever
   hands the key's own spelling back in out, which is what a route that printed label_key
   itself did and is the whole reason this exists. */
bool resolveLabel(const char *key, std::string &out);

/* The text the box shows for a row, resolveLabel() of its label_key. A row that is one of
   several indexed alike, key_0, key_1 and on under one label, has its slot counted from one
   after the text, the way the box numbers its slots, so the rows can be told apart. */
bool rowLabel(const Descriptor &d, std::string &out);

// Called once changed settings are saved, from a menu or a write alike.
void announceSettingsChanged();

/* Every declared setting as the store holds it, a credential included, to be compared with a
   later reading. Never to be answered to a caller outside the box. */
typedef std::map<std::string, std::string> Snapshot;
Snapshot snapshot();

/* The keys whose reading differs from the snapshot, for a caller that replaced
   many settings at once and cannot name what it changed. */
std::vector<std::string> changedSince(const Snapshot &before);

/* Puts in force every group that holds a key changed since the snapshot, so that
   a file loaded over the settings leaves nothing at its old value. The groups
   compare with what they last sent, so a group whose keys all read as before is
   not asked. */
Status applyChangedSince(const Snapshot &before);

/* Takes a snapshot, lets replace put other values over the settings, a file loaded or the
   defaults, and puts in force what changed, as applyChangedSince() does. The one way to
   replace the settings wholesale, so a load and a reset leave every group knowing what it
   last sent. */
Status applyReplaced(const std::function<void()> &replace);

} // namespace settings
} // namespace coreapi

#endif
