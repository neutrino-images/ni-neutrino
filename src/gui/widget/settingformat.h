/*
 * settingformat.h - the printf format a number setting is shown with
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

#ifndef __settingformat_h__
#define __settingformat_h__

#include <coreapi/settings/menuspec.h>

#include <string>

/* The format a number chooser prints its value with, from the unit text of its
   row. Header only and free of widgets so a test can hold it without a screen.

   A unit text follows the number after a space, except a unit that starts with
   a sign such as the percent sign, which sits against the number as the screens
   always wrote it. An empty text means the row names none and the number stands
   alone. The result goes to printf, so a percent sign in a unit is doubled. */
inline std::string settingNumberFormat(const std::string &unit_text)
{
	if (unit_text.empty())
		return "%d";

	std::string format("%d");
	const unsigned char first = (unsigned char) unit_text[0];
	const bool sign = first < 0x80 && !((first >= '0' && first <= '9') || (first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z'));
	if (!sign)
		format += ' ';
	for (size_t i = 0; i < unit_text.size(); i++)
	{
		if (unit_text[i] == '%')
			format += '%';
		format += unit_text[i];
	}
	return format;
}

/* What a row's number is printed with, its unit's name asked of text, a
   function from a locale key to its words. Empty when the row names no unit,
   which leaves the chooser with its plain number. This is the whole of what
   addSetting does with a unit, so a test that gives it a row and a fake text
   source holds the screens to the row. */
template <class Text>
inline std::string settingNumberFormat(const coreapi::MenuItemSpec &spec, Text text)
{
	if (spec.unit_key.empty())
		return std::string();
	return settingNumberFormat(std::string(text(spec.unit_key)));
}

#endif
