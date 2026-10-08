/*
 * menuspec.h - a declared setting as a menu item
 *
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

#ifndef __coreapi_menuspec_h__
#define __coreapi_menuspec_h__

#include "coreapi/base/result.h"
#include "coreapi/base/schema.h"

#include <string>
#include <vector>

namespace coreapi
{

// Keys stay keys: the frontend resolves them when it draws, so a language
// change needs no rebuild of the item. label_text is set instead of label_key
// for an entry the box names itself.
struct MenuChoice
{
	long        value;
	std::string label_key;
	std::string label_text;
	// The text a String row stores for the entry, empty for a number.
	std::string text;
};

struct MenuItemSpec
{
	std::string key;
	ValueType   type;
	std::string label_key;   // empty: none
	std::string hint_key;    // empty: none
	long        min, max;    // Int and Key
	// Int only: min and max are what the box states now and can move, so a write is held to them.
	bool        bounds_vary;
	// Int only: the name of the text that follows the number, empty is none.
	std::string unit_key;
	size_t      channels;    // Color: three or four
	// Enum and Bool; for Int the one value it shows in words, if any. A row that
	// takes its list from a provider, Int or String, has what the provider offered
	// when the item was made, and none where it could not say.
	std::vector<MenuChoice> choices;
	// NULL for a row whose value is not an int member, which the frontend
	// then holds itself and moves through menuValueRead and menuValueWrite.
	int *(*int_pointer)(SNeutrinoSettings &);
	bool        locked;      // shown and not changeable
	// String only: what the text may be, NULL for plain text, and whether it is
	// a credential the frontend should not show as typed.
	const TextRule *text;
	bool        secret;
	FieldRef    field;       // for the two below, not for the frontend
	// The provider itself, for the writes below to ask again: the list may have
	// changed since the item was drawn.
	ChoiceSource choices_from;
	// What the row holds before anything is stored, which a provider's list is not asked about.
	std::string default_text;
	long        default_number;

	MenuItemSpec() : type(ValueType::Int), min(0), max(0), bounds_vary(false), channels(0), int_pointer(NULL), locked(false), text(NULL), secret(false), field(), choices_from(NULL), default_number(0) {}
};

/* What a menu needs to draw one declared row, in the shape this box offers it.
   UnknownSetting for an undeclared key, SettingNotOnThisBox for a row the box
   lacks, BadTable for a row whose value is no number a widget can edit, and
   ChoicesUnavailable for an Enum of which the box offers nothing. */
Result<MenuItemSpec> menuItem(const std::string &key);

/* Whether the item is a list of its values rather than a number: a choice, and a number whose
   values a provider lists, so the menu shows what each stands for and cannot store one the
   box does not have. A number whose provider could not say stays a number. */
bool offeredAsList(const MenuItemSpec &spec);

/* The value the way the row reaches it: a daemon's row asks the daemon, a bit
   of a mask reads its bit and a flag file is whether the file is there. False when the daemon cannot be reached, and out is
   then untouched. Blocking for a daemon's row, so called on the menu's own
   loop as the screens did, never with a lock held. */
bool menuValueRead(const MenuItemSpec &spec, const SNeutrinoSettings &s, long &out);

/* A bit of a mask writes only its bit, a daemon's row tells the daemon once and a
   flag file is made or removed. False for a value the field cannot hold, a
   daemon that did not take it or a file that could not be changed, and for a number
   a provider does not offer unless the row already holds it. */
bool menuValueWrite(const MenuItemSpec &spec, SNeutrinoSettings &s, long value);

/* The same pair for a String row, which is read and written whole through the
   row's text field, and for a Color, which is the text #rrggbb or #rrggbbaa of
   its channels. The write holds a String to the row's rule (settings::holdsTextRule)
   and refuses a text that breaks it, a colour that is no colour of the row's
   channel count and a channel id the field cannot read; nothing is written then. A
   String row with a provider takes only an entry the provider offers, or the value it
   already holds.
   False for a row with no text field. */
bool menuTextRead(const MenuItemSpec &spec, const SNeutrinoSettings &s, std::string &out);
bool menuTextWrite(const MenuItemSpec &spec, SNeutrinoSettings &s, const std::string &value);

} // namespace coreapi

#endif
