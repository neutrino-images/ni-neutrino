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
};

struct MenuItemSpec
{
	std::string key;
	ValueType   type;
	std::string label_key;   // empty: none
	std::string hint_key;    // empty: none
	long        min, max;    // Int and Key
	// Int only: the name of the text that follows the number, and the name of
	// one that holds the whole number as %d. At most one is set, empty is none.
	std::string unit_key;
	std::string format_key;
	size_t      channels;    // Color: three or four
	// Enum and Bool; for Int the one value it shows in words, if any.
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

	MenuItemSpec() : type(ValueType::Int), min(0), max(0), channels(0), int_pointer(NULL), locked(false), text(NULL), secret(false), field() {}
};

/* What a menu needs to draw one declared row, in the shape this box offers it.
   UnknownSetting for an undeclared key, SettingNotOnThisBox for a row the box
   lacks, BadTable for a row whose value is no number a widget can edit, and
   ChoicesUnavailable for an Enum of which the box offers nothing. */
Result<MenuItemSpec> menuItem(const std::string &key);

/* The value the way the row reaches it: a daemon's row asks the daemon, a bit
   of a mask reads its bit and a flag file is whether the file is there. False when the daemon cannot be reached, and out is
   then untouched. Blocking for a daemon's row, so called on the menu's own
   loop as the screens did, never with a lock held. */
bool menuValueRead(const MenuItemSpec &spec, const SNeutrinoSettings &s, long &out);

/* A bit of a mask writes only its bit, a daemon's row tells the daemon once and a
   flag file is made or removed. False for a value the field cannot hold, a
   daemon that did not take it or a file that could not be changed. */
bool menuValueWrite(const MenuItemSpec &spec, SNeutrinoSettings &s, long value);

/* The same pair for a String row, which is read and written whole through the
   row's text field, and for a Color, which is the text #rrggbb or #rrggbbaa of
   its channels. The write does not hold a String to the row's rule: the widget
   that offered it already did, and a caller with text of its own asks
   settings::holdsTextRule first. A colour write is refused for a text that is no
   colour of the row's channel count. False for a row with no text field. */
bool menuTextRead(const MenuItemSpec &spec, const SNeutrinoSettings &s, std::string &out);
bool menuTextWrite(const MenuItemSpec &spec, SNeutrinoSettings &s, const std::string &value);

} // namespace coreapi

#endif
