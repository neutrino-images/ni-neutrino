/*
 * menuspec.cpp - a declared setting as a menu item
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

#include "menuspec.h"

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "settings.h"

namespace coreapi
{

Result<MenuItemSpec> menuItem(const std::string &key)
{
	const Descriptor *row = settings::findRow(key);
	if (row == NULL)
		return fail(Status::NotFound, ErrorCode::UnknownSetting, "no such setting");
	Descriptor here;
	if (!rowOnThisBox(*row, here))
		return fail(Status::Conflict, ErrorCode::SettingNotOnThisBox,
			    "this box does not have what the setting controls");
	const Descriptor *d = &here;
	const bool asked = d->field.ask != NULL && d->field.tell != NULL;
	const bool numbered = d->field.read_number != NULL && d->field.write_number != NULL;
	if (d->type == ValueType::String || (d->field.int_pointer == NULL && !asked && !numbered))
		return fail(Status::Internal, ErrorCode::BadTable,
			    "the setting has no number a widget can edit");

	MenuItemSpec spec;
	spec.key = d->key;
	spec.type = d->type;
	if (d->label_key != NULL)
		spec.label_key = d->label_key;
	if (d->hint_key != NULL)
		spec.hint_key = d->hint_key;
	spec.int_pointer = d->field.int_pointer;
	spec.field = d->field;
	spec.locked = settings::lockedNow(key);

	if (d->type == ValueType::Int)
	{
		spec.min = d->min;
		spec.max = d->max;
		const EnumValue *named = namedNumber(*d);
		if (named != NULL)
		{
			MenuChoice c;
			c.value = named->value;
			c.label_key = named->label_key;
			spec.choices.push_back(c);
		}
	}
	else if (d->type == ValueType::Bool && d->values != NULL)
	{
		// A flag that names its own two words, a no and a yes for one.
		for (size_t i = 0; i < d->value_count; ++i)
		{
			MenuChoice c;
			c.value = d->values[i].value;
			if (d->values[i].label_key != NULL)
				c.label_key = d->values[i].label_key;
			if (d->values[i].label_text != NULL)
				c.label_text = d->values[i].label_text;
			spec.choices.push_back(c);
		}
	}
	else if (d->type == ValueType::Bool)
	{
		MenuChoice off;
		off.value = 0;
		off.label_key = "options.off";
		MenuChoice on;
		on.value = 1;
		on.label_key = "options.on";
		spec.choices.push_back(off);
		spec.choices.push_back(on);
	}
	else if (d->type == ValueType::Enum)
	{
		if (d->values != NULL)
		{
			for (size_t i = 0; i < d->value_count; ++i)
			{
				const EnumValue &e = d->values[i];
				if (!entryOffered(e))
					continue;
				MenuChoice c;
				c.value = e.value;
				if (e.label_key != NULL)
					c.label_key = e.label_key;
				if (e.label_text != NULL)
					c.label_text = e.label_text;
				spec.choices.push_back(c);
			}
		}
		if (spec.choices.empty())
			return fail(Status::InvalidArgument, ErrorCode::ChoicesUnavailable,
				    "the setting offers no value");
	}
	return ok(std::move(spec));
}

bool menuValueRead(const MenuItemSpec &spec, const SNeutrinoSettings &s, long &out)
{
	if (spec.field.ask != NULL)
		return spec.field.ask(out);
	if (spec.field.read_number == NULL)
		return false;
	out = spec.field.read_number(s);
	return true;
}

bool menuValueWrite(const MenuItemSpec &spec, SNeutrinoSettings &s, long value)
{
	if (spec.field.tell != NULL)
		return spec.field.tell(value);
	if (spec.field.write_number == NULL)
		return false;
	if (spec.field.fits_number != NULL && !spec.field.fits_number(value))
		return false;
	spec.field.write_number(s, value);
	return true;
}

} // namespace coreapi
