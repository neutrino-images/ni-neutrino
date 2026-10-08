/*
 * settingstable_weather.cpp - weather settings, one row per field
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

namespace coreapi
{

namespace
{

/* The weather section. The weather is one of the online services, beside the
   four the misc section declares; it is a section of its own here because
   these six settings are asked for as one. Three of the six defaults are
   macros a header of the menu code states, which this layer may not include,
   so the literals below carry them. */

/* The three below are editable only while the weather is on. */
const Condition kWeatherOn[] =
{
	{ "weather_enabled", CompareOp::Ne, 0, NULL, 0 }
};

const Descriptor kWeather[] =
{
	/* Can only be on while an API key is present. That is a check on the key
	   itself and not a comparison against another setting, so no condition is
	   carried. The loader turns it off without a key. */
	{
		"weather_enabled", ValueType::Bool, "weather",
		"weather.enabled", "menu.hint_weather_enabled",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(weather_enabled)
	},

#if ENABLE_WEATHER_KEY_MANAGE
	/* The two below sit behind the arm the program loads and saves them under.
	   Where it is off the key is whatever the
	   build was configured with and a written value is lost at the next start,
	   so there is nothing here for a row to offer.

	   The key is a credential and is declared secret, so a read answers nothing
	   whatever is stored. It defaults to the placeholder below only where the
	   build carries no key of its own; one configured with a key falls back to
	   that key instead, which is not a constant this can carry. */
	{
		"weather_api_key", ValueType::String, "weather",
		"weather.api_key", "menu.hint_weather_api_key",
		0, 0, NULL, 0, 0, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(weather_api_key)
	},
	/* No menu offers this one in this build, and its label comes from the item
	   the build leaves out, which is the only statement of it there is. The
	   value is the version part of a URL, so it is text and not a choice. */
	{
		"weather_api_version", ValueType::String, "weather",
		"weather.api_version", "menu.hint_weather_api_version",
		0, 0, NULL, 0, 0, "3.0", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(weather_api_version)
	},
#endif

	/* The name of the place and its coordinates are one setting in two parts,
	   written together by the item that picks a place from a list. Only the
	   coordinates reach the weather itself, so writing the name alone renames
	   what is shown and leaves the forecast where it was. Both are declared,
	   because both are text this layer can carry; there is one item for the pair
	   and both rows name its label. */
	{
		"weather_city", ValueType::String, "weather",
		"weather.location", "menu.hint_weather_location",
		0, 0, NULL, 0, 0, "Berlin", false, false, COREAPI_CONDITIONS(kWeatherOn),
		COREAPI_TEXT_FIELD(weather_city)
	},
	{
		"weather_location", ValueType::String, "weather",
		"weather.location", "menu.hint_weather_location",
		0, 0, NULL, 0, 0, "52.52,13.40", false, false, COREAPI_CONDITIONS(kWeatherOn),
		COREAPI_TEXT_FIELD(weather_location)
	},
	/* Five characters, and a String carries no bound, so that is stated nowhere
	   a caller can read. */
	{
		"weather_postalcode", ValueType::String, "weather",
		"weather.postalcode", "menu.hint_weather_postalcode",
		0, 0, NULL, 0, 0, "10178", false, false, COREAPI_CONDITIONS(kWeatherOn),
		COREAPI_TEXT_FIELD(weather_postalcode)
	},
};

} // anonymous namespace

const Descriptor *settingsTableWeather(size_t &count)
{
	count = sizeof(kWeather) / sizeof(kWeather[0]);
	return kWeather;
}

} // namespace coreapi
