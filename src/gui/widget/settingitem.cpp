/*
 * settingitem.cpp - a setup menu item built from the settings declaration
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "settingitem.h"

#include <coreapi/base/errors.h>
#include <coreapi/settings/menuspec.h>
#include <system/settings.h>
#include <system/localize.h>
#include <system/debug.h>

#include <map>
#include <vector>

extern SNeutrinoSettings g_settings;

neutrino_locale_t localeFromKey(const std::string &key)
{
	if (key.empty())
		return NONEXISTANT_LOCALE;

	// Filled as keys are asked for: the name table has no count visible
	// outside its own unit, and a menu asks for the same few keys again.
	static std::map<std::string, neutrino_locale_t> index;
	std::map<std::string, neutrino_locale_t>::const_iterator it = index.find(key);
	if (it != index.end())
		return it->second;

	neutrino_locale_t locale = CLocaleManager::getLocale(key.c_str());
	// Cached with the miss, so a key nothing names is reported once.
	if (locale == NONEXISTANT_LOCALE)
		dprintf(DEBUG_NORMAL, "[settingitem] no locale named %s\n", key.c_str());
	index[key] = locale;
	return locale;
}

namespace
{

/* The int a widget edits for a row with no int member of its own: filled
   through the row's own read as the item is built, and written through the
   row's own write on every change, before the screen's observer hears of it.
   That is when the screens wrote these values themselves. A read that fails
   leaves the item inactive and showing that there is no value.

   For a row with an int member the widget edits the member and nothing here
   takes part. */
class SettingValue : public CChangeObserver
{
	protected:
		const coreapi::MenuItemSpec spec;
		CChangeObserver *const next;
		int value;
		bool known;

		SettingValue(const coreapi::MenuItemSpec &s, CChangeObserver *observer)
			: spec(s), next(observer), value(0), known(true)
		{
			if (spec.int_pointer != NULL)
				return;
			long v = 0;
			known = coreapi::menuValueRead(spec, g_settings, v);
			// Below the range, so a number chooser shows the special text.
			value = known ? (int) v : (int) spec.min - 1;
			if (!known)
				dprintf(DEBUG_NORMAL, "[settingitem] %s: no value to show\n", spec.key.c_str());
		}

		int *editTarget() { return spec.int_pointer != NULL ? spec.int_pointer(g_settings) : &value; }
		CChangeObserver *notifier() { return spec.int_pointer != NULL ? next : this; }

	private:
		void store()
		{
			if (!coreapi::menuValueWrite(spec, g_settings, value))
				dprintf(DEBUG_NORMAL, "[settingitem] %s: %d not written\n", spec.key.c_str(), value);
		}

	public:
		bool changeNotify(const neutrino_locale_t name, void *data)
		{
			store();
			return next != NULL && next->changeNotify(name, data);
		}
		bool changeNotify(const std::string &name, void *data)
		{
			store();
			return next != NULL && next->changeNotify(name, data);
		}
		bool changeNotify(lua_State *L, const std::string &id, const std::string &action, void *data)
		{
			store();
			return next != NULL && next->changeNotify(L, id, action, data);
		}
};

/* The chooser copies the option structs but keeps each valname as a pointer,
   so the fixed texts live here. As a base before the chooser it is built before
   the chooser copies the options and destroyed after the chooser is gone. */
struct SettingOptions
{
	std::vector<std::string> texts;
	std::vector<CMenuOptionChooser::keyval_ext> entries;

	SettingOptions(const std::vector<coreapi::MenuChoice> &choices, int *unknown)
	{
		// Every text in place before a pointer into the vector is taken.
		texts.reserve(choices.size());
		for (size_t i = 0; i < choices.size(); i++)
			texts.push_back(choices[i].label_text);

		int lowest = 0;
		for (size_t i = 0; i < choices.size(); i++)
		{
			CMenuOptionChooser::keyval_ext e;
			e.key = (int) choices[i].value;
			if (choices[i].label_key.empty())
			{
				e.value = NONEXISTANT_LOCALE;
				e.valname = texts[i].c_str();
			}
			else
			{
				e.value = localeFromKey(choices[i].label_key);
				e.valname = NULL;
			}
			entries.push_back(e);
			if (i == 0 || e.key < lowest)
				lowest = e.key;
		}

		// The chooser shows and stores its first entry for a value it lacks,
		// so no value gets an entry of its own.
		if (unknown != NULL)
		{
			*unknown = lowest - 1;
			CMenuOptionChooser::keyval_ext e;
			e.key = *unknown;
			e.value = LOCALE_STREAMINFO_NOT_AVAILABLE;
			e.valname = NULL;
			entries.push_back(e);
		}
	}
};

class CSettingChooser : private SettingValue, private SettingOptions, public CMenuOptionChooser
{
	public:
		CSettingChooser(const coreapi::MenuItemSpec &s, bool is_active,
				CChangeObserver *observer, const neutrino_msg_t direct_key, bool opens_list)
			: SettingValue(s, observer),
			  SettingOptions(s.choices, known ? NULL : &value),
			  CMenuOptionChooser(localeFromKey(s.label_key), editTarget(),
					     entries.data(), entries.size(), is_active && known, notifier(), direct_key,
					     NULL, opens_list)
		{
		}

		void setActive(const bool Active) { CMenuOptionChooser::setActive(Active && known); }
};

/* The one value the chooser shows in words: the row's own, or while there is
   no value the one below the range that says so. */
int specialValue(const coreapi::MenuItemSpec &s, bool known)
{
	if (!known)
		return (int) s.min - 1;
	return s.choices.empty() ? 0 : (int) s.choices[0].value;
}

neutrino_locale_t specialName(const coreapi::MenuItemSpec &s, bool known)
{
	if (!known)
		return LOCALE_STREAMINFO_NOT_AVAILABLE;
	return s.choices.empty() ? NONEXISTANT_LOCALE : localeFromKey(s.choices[0].label_key);
}

class CSettingNumberChooser : private SettingValue, public CMenuOptionNumberChooser
{
	public:
		CSettingNumberChooser(const coreapi::MenuItemSpec &s, bool is_active,
				      CChangeObserver *observer, const neutrino_msg_t direct_key, bool slider)
			: SettingValue(s, observer),
			  CMenuOptionNumberChooser(localeFromKey(s.label_key), editTarget(),
						   is_active && known, (int) s.min, (int) s.max, notifier(), direct_key,
						   NULL, 0, specialValue(s, known), specialName(s, known), slider)
		{
		}

		void setActive(const bool Active) { CMenuOptionNumberChooser::setActive(Active && known); }
};

} // namespace

CMenuItem *addSetting(CMenuWidget *menu, const char *key, bool active,
		      CChangeObserver *observer, const neutrino_msg_t direct_key, bool slider, bool pulldown)
{
	coreapi::Result<coreapi::MenuItemSpec> r = coreapi::menuItem(key ? key : "");
	if (!r.ok())
	{
		// A setting the box lacks is no fault of the declaration.
		const int level = (r.error().code == coreapi::ErrorCode::SettingNotOnThisBox) ? DEBUG_INFO : DEBUG_NORMAL;
		dprintf(level, "[settingitem] %s: %s\n", key ? key : "(null)", r.error().message.c_str());
		return NULL;
	}

	const coreapi::MenuItemSpec &spec = r.value();
	active = active && !spec.locked;
	CMenuItem *item = NULL;
	switch (spec.type)
	{
		case coreapi::ValueType::Int:
			item = new CSettingNumberChooser(spec, active, observer, direct_key, slider);
			break;
		case coreapi::ValueType::Bool:
		case coreapi::ValueType::Enum:
			item = new CSettingChooser(spec, active, observer, direct_key, pulldown);
			break;
		default:
			dprintf(DEBUG_NORMAL, "[settingitem] %s: no widget for this type\n", key ? key : "(null)");
			return NULL;
	}

	if (!spec.hint_key.empty())
		item->setHint("", localeFromKey(spec.hint_key));

	menu->addItem(item);
	return item;
}
