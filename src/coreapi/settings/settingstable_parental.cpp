/*
 * settingstable_parental.cpp - parental lock settings, one row per field
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

#include "settingstable.h"
#include "settingsfield.h"

#include <string.h>

namespace coreapi
{

namespace
{

/* The parental lock. While the box itself is locked the rows listed in kLocked
   are refused and the two strictest values are forced. That is a state of the
   running box and not a setting, so no row carries it as a condition. */

// ONSTART (PARENTALLOCK_PROMPT_ONSTART in src/system/settings.h) is not offered.
const EnumValue kPrompt[] =
{
	{ 0, "parentallock.never", NULL, NULL },
	{ 2, "parentallock.changetolocked", NULL, NULL },
	{ 3, "parentallock.onsignal", NULL, NULL }
};

// The age is one of the three the ratings use.
const EnumValue kLockage[] =
{
	{ 12, "parentallock.lockage12", NULL, NULL },
	{ 16, "parentallock.lockage16", NULL, NULL },
	{ 18, "parentallock.lockage18", NULL, NULL }
};

const EnumValue kDefaultLocked[] =
{
	{ 0, "parentallock.defaultunlocked", NULL, NULL },
	{ 1, "parentallock.defaultlocked", NULL, NULL }
};

const Descriptor kSettings[] =
{
	{
		"parentallock_prompt", ValueType::Enum, "parental",
		"parentallock.prompt", "menu.hint_parentallock_prompt",
		0, 0, COREAPI_ENUM(kPrompt), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(parentallock_prompt)
	},
	{
		"parentallock_lockage", ValueType::Enum, "parental",
		"parentallock.lockage", "menu.hint_parentallock_lockage",
		0, 0, COREAPI_ENUM(kLockage), 12, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(parentallock_lockage)
	},
	/* A choice and not a flag: the words are what a new bouquet starts as and
	   not an on and an off, so a flag would carry the values and lose them. */
	{
		"parentallock_defaultlocked", ValueType::Enum, "parental",
		"parentallock.bouquetmode", NULL,
		0, 0, COREAPI_ENUM(kDefaultLocked), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(parentallock_defaultlocked)
	},
	// Seconds a locked channel stays watchable after the pin was given.
	{
		"parentallock_zaptime", ValueType::Int, "parental",
		"parentallock.zaptime", NULL,
		0, 10000, NULL, 0, 60, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(parentallock_zaptime)
	},
	/* The pin itself, which is why it is secret: a read answers nothing and an
	   empty write is refused, so a form that round trips its fields cannot
	   clear it. The pin is four digits and no more, which a String row cannot
	   say. */
	{
		"parentallock_pincode", ValueType::String, "parental",
		"parentallock.changepin", "menu.hint_parentallock_changepin",
		0, 0, NULL, 0, 0, "0000", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(parentallock_pincode)
	},
};

/* What the lock holds. The pin is not among them: it stays changeable on a
   locked box. */
const char *const kLocked[] =
{
	"parentallock_prompt",
	"parentallock_lockage",
	"parentallock_defaultlocked",
	"parentallock_zaptime"
};

} // anonymous namespace

const Descriptor *settingsTableParental(size_t &count)
{
	count = sizeof(kSettings) / sizeof(kSettings[0]);
	return kSettings;
}

const char *const *parentalLockKeys(size_t &count)
{
	count = sizeof(kLocked) / sizeof(kLocked[0]);
	return kLocked;
}

bool heldByParentalLock(const char *key)
{
	if (key == NULL)
		return false;
	for (size_t i = 0; i < sizeof(kLocked) / sizeof(kLocked[0]); ++i)
		if (strcmp(key, kLocked[i]) == 0)
			return true;
	return false;
}

} // namespace coreapi
