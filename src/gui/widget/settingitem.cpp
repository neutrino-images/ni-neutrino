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
#include "settingformat.h"
#include "settingfollow.h"

#include <global.h>

#include "settingactive.h"

#include <coreapi/base/apply.h>
#include <coreapi/base/errors.h>
#include <coreapi/base/schema.h>
#include <coreapi/settings/menuspec.h>
#include <coreapi/settings/settings.h>
#include <gui/widget/colorchooser.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>
#include <gui/widget/keychooser.h>
#include <system/settings.h>
#include <system/localize.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <system/sms_input.h>

#include <gui/filebrowser.h>
#include <gui/components/cc_input_dialog.h>
#include <gui/widget/keyboard_input.h>
#include <gui/widget/stringinput.h>

#include <map>
#include <set>
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

SettingActiveSet<CMenuItem> &activeSet()
{
	static SettingActiveSet<CMenuItem> set;
	return set;
}

/* What follows a change of any row the builder makes, whatever its shape: the
   change runs the couplings a web write runs and goes to the apply registry, and
   the menu's other items are judged again, so a text, key or colour row moves
   its dependents as a number does.
   changed is the item whose row it is, for what its screen does after the
   apply; NULL where no screen can ask for that. */
bool settleRow(const std::string &key, CMenuWidget *menu, const CMenuItem *changed = NULL)
{
	return settleChange<CMenuItem>(activeSet(), key, changed,
				[](const std::string &k)
				{
					const coreapi::Status s = coreapi::settings::menuChanged(k);
					// Busy is a group whose phase is not reached yet, which runPhase() makes good.
					if (s != coreapi::Status::Ok && s != coreapi::Status::Busy)
						dprintf(DEBUG_NORMAL, "[settingitem] %s: apply failed\n", k.c_str());
				},
				menu != NULL ? &menu->getItems() : NULL, coreapi::settings::conditionsHoldNow);
}

/* What every item built here does after settings were written elsewhere: take
   its value again from the settings and show it if it is on the screen. */
class SettingFollow
{
	public:
		virtual ~SettingFollow() {}
		// paint is whether the item's menu is the one on top, waiting for a key.
		virtual void followValue(bool paint) = 0;
		// The active state of an item whose menu something else covers.
		virtual void setActiveQuietly(bool active) = 0;
};

// Whether item is the one its menu has selected, so it is drawn as such again.
bool selectedIn(CMenuWidget *menu, const CMenuItem *item)
{
	if (menu == NULL)
		return false;
	const int s = menu->getSelected();
	const std::vector<CMenuItem *> &items = menu->getItems();
	return s >= 0 && (size_t) s < items.size() && items[s] == item;
}

/* The int a widget edits for a row with no int member of its own: filled
   through the row's own read as the item is built, and written through the
   row's own write on every change, before the screen's observer hears of it.
   That is when the screens wrote these values themselves. A read that fails
   leaves the item inactive and showing that there is no value.

   For a row with an int member the widget edits the member and nothing here
   stores. After the screen's own observer has run, the change is handed to the
   apply registry and the menu's other items are judged again, so what a screen
   does for its own sake comes first and the driver sees the final settings. */
class SettingValue : public CChangeObserver
{
	protected:
		const coreapi::MenuItemSpec spec;
		CChangeObserver *const next;
		CMenuWidget *const menu;
		int value;
		bool known;
		// The item this is part of, set by it once it is whole.
		const CMenuItem *self;
		// What the widget edits once the item is deferred.
		HeldValue held;

		SettingValue(const coreapi::MenuItemSpec &s, CChangeObserver *observer, CMenuWidget *owner)
			: spec(s), next(observer), menu(owner), value(0), known(true), self(NULL)
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

		// The copy a row without an int member keeps, taken again; the others edit the member.
		void reread()
		{
			rereadCopy(spec, value, known);
			held.follow();
		}
		CChangeObserver *notifier() { return this; }

	public:
		// A deferred item's change, put into the setting before it is settled.
		void commitHeld()
		{
			held.commit();
			write();
		}

	private:
		void write()
		{
			if (spec.int_pointer != NULL)
				return;
			if (!coreapi::menuValueWrite(spec, g_settings, value))
				dprintf(DEBUG_NORMAL, "[settingitem] %s: %d not written\n", spec.key.c_str(), value);
		}

		void store()
		{
			if (!held.holding())
				write();
		}

		bool settled()
		{
			if (activeSet().holdChange(self))
				return false;
			return settleRow(spec.key, menu, self);
		}

	public:
		bool changeNotify(const neutrino_locale_t name, void *data)
		{
			store();
			const bool r = next != NULL && next->changeNotify(name, data);
			const bool after = settled();
			return r || after;
		}
		bool changeNotify(const std::string &name, void *data)
		{
			store();
			const bool r = next != NULL && next->changeNotify(name, data);
			const bool after = settled();
			return r || after;
		}
		bool changeNotify(lua_State *L, const std::string &id, const std::string &action, void *data)
		{
			store();
			const bool r = next != NULL && next->changeNotify(L, id, action, data);
			const bool after = settled();
			return r || after;
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

class CSettingChooser : private SettingValue, private SettingOptions, public CMenuOptionChooser, public SettingFollow
{
	public:
		CSettingChooser(const coreapi::MenuItemSpec &s, bool is_active,
				CChangeObserver *observer, const neutrino_msg_t direct_key, bool opens_list,
				CMenuWidget *owner)
			: SettingValue(s, observer, owner),
			  SettingOptions(s.choices, known ? NULL : &value),
			  CMenuOptionChooser(localeFromKey(s.label_key), editTarget(),
					     entries.data(), entries.size(), is_active && known, notifier(), direct_key,
					     NULL, opens_list),
			  list_on_step(false)
		{
			self = this;
		}

		~CSettingChooser() { activeSet().forget(this); }

		using SettingValue::commitHeld;
		void holdValue() { optionValue = held.hold(editTarget()); }

		void openListOnStep() { list_on_step = true; }

		int exec(CMenuTarget *parent)
		{
			if (list_on_step)
				msg = CRCInput::RC_ok;
			return CMenuOptionChooser::exec(parent);
		}

		void setActive(const bool Active) { CMenuOptionChooser::setActive(Active && known); }

		void followValue(bool on_top)
		{
			reread();
			if (on_top && used && x != -1)
				paint(selectedIn(menu, this));
		}

		void setActiveQuietly(bool a) { active = current_active = a && known; }

	private:
		bool list_on_step;
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

std::string textOf(const std::string &key)
{
	const neutrino_locale_t locale = localeFromKey(key);
	return locale == NONEXISTANT_LOCALE ? std::string() : std::string(g_Locale->getText(locale));
}

class CSettingNumberChooser : private SettingValue, public CMenuOptionNumberChooser, public SettingFollow
{
	public:
		CSettingNumberChooser(const coreapi::MenuItemSpec &s, bool is_active,
				      CChangeObserver *observer, const neutrino_msg_t direct_key, bool slider, bool numeric,
				      CMenuWidget *owner)
			: SettingValue(s, observer, owner),
			  CMenuOptionNumberChooser(localeFromKey(s.label_key), editTarget(),
						   is_active && known, (int) s.min, (int) s.max, notifier(), direct_key,
						   NULL, 0, specialValue(s, known), specialName(s, known), slider)
		{
			self = this;
			setNumericInput(numeric);
			// Every word after the first, which the constructor took.
			for (size_t i = 1; known && i < s.choices.size(); i++)
				setLocalizedValue((int) s.choices[i].value, localeFromKey(s.choices[i].label_key));
			// A row that names neither leaves the chooser's own plain number.
			const std::string format = settingNumberFormat(s, textOf);
			if (!format.empty())
				setNumberFormat(format);
		}

		~CSettingNumberChooser() { activeSet().forget(this); }

		using SettingValue::commitHeld;
		void holdValue() { optionValue = held.hold(editTarget()); }

		void setActive(const bool Active) { CMenuOptionNumberChooser::setActive(Active && known); }

		void followValue(bool on_top)
		{
			reread();
			if (on_top && used && x != -1)
				paint(selectedIn(menu, this));
		}

		void setActiveQuietly(bool a) { active = current_active = a && known; }
};

/* The text a String row holds, edited by the widget its rule names and written
   through the row's own field on every change, before the screen's observer
   hears of it. The widget limits what can be typed; the same rule is what the
   web write is held to, so the two cannot drift. */
class SettingText : public CMenuTarget, public CChangeObserver
{
	protected:
		const coreapi::MenuItemSpec spec;
		CChangeObserver *const next;
		CMenuWidget *const menu;
		FollowedText text;
		// What the dialogs edit in place: the followed text itself.
		std::string &value;
		CMenuForwarder *item;
		// What the dialog says beside its field, which is not the menu's hint.
		const neutrino_locale_t hint1, hint2;

		SettingText(const coreapi::MenuItemSpec &s, CChangeObserver *observer, CMenuWidget *owner,
			    neutrino_locale_t dialog_hint1, neutrino_locale_t dialog_hint2)
			: spec(s), next(observer), menu(owner), text(s), value(text.value), item(NULL), hint1(dialog_hint1), hint2(dialog_hint2)
		{
			if (!text.reread())
				dprintf(DEBUG_NORMAL, "[settingitem] %s: no text to show\n", spec.key.c_str());
		}

		/* A credential is not shown as typed, except a pin, which the screens
		   always showed beside its row. */
		std::string shown() const
		{
			const bool pin = spec.text != NULL && spec.text->kind == coreapi::TextKind::Pin;
			if (spec.secret && !pin)
				return std::string();
			/* A file picked by extension is long and its name is what tells it apart, so
			   the item shows the name in brackets as the font items always did. */
			if (spec.text != NULL && spec.text->kind == coreapi::TextKind::File && spec.text->extensions != NULL && !value.empty())
			{
				std::string path(value);
				return "(" + getBaseName(path) + ")";
			}
			return value;
		}

		CMenuTarget *target() { return this; }

	private:
		neutrino_locale_t name() const { return localeFromKey(spec.label_key); }

		std::string dialogHint() const
		{
			std::string said = g_Locale->getText(hint1);
			const std::string more = g_Locale->getText(hint2);
			if (!more.empty())
				said += (said.empty() ? "" : "\n") + more;
			return said;
		}

		coreapi::TextKind kind() const
		{
			return spec.text != NULL ? spec.text->kind : coreapi::TextKind::Plain;
		}

		int size() const { return spec.text != NULL ? (int) spec.text->max_length : 0; }

		void store()
		{
			if (!text.write())
			{
				dprintf(DEBUG_NORMAL, "[settingitem] %s: text not written\n", spec.key.c_str());
				/* The field drops a channel id it cannot read without a word, which an empty
				   one from a dialog is; the person who typed it is told. */
				const bool channel_id = spec.field.origin == coreapi::FieldOrigin::ChannelIdField
					|| (spec.field.extra != NULL && spec.field.extra->channel_id);
				if (channel_id)
					ShowHint(LOCALE_MESSAGEBOX_ERROR, LOCALE_STRINGINPUT_SAVE_FAILED);
			}
			if (item != NULL)
				item->setOption(shown());
		}

		void chooseDirectory(CMenuTarget *parent)
		{
			if (parent != NULL)
				parent->hide();
			const coreapi::MustExist rule = spec.text->must_exist;
			const bool test = rule == coreapi::MustExist::YesNotTmpfs || rule == coreapi::MustExist::YesNotFlash;
			// The update folder is the one that may be in memory.
			if (chooserDir(value, test, "", rule == coreapi::MustExist::YesNotFlash))
				changeNotify(name(), (void *) value.c_str());
		}

		void chooseFile(CMenuTarget *parent)
		{
			if (parent != NULL)
				parent->hide();
			CFileBrowser browser;
			CFileFilter filter;
			if (spec.text->extensions != NULL)
			{
				std::string rest = spec.text->extensions;
				while (!rest.empty())
				{
					const size_t comma = rest.find(',');
					filter.addFilter(rest.substr(0, comma));
					rest = comma == std::string::npos ? std::string() : rest.substr(comma + 1);
				}
				browser.Filter = &filter;
			}
			// Starts in the folder of the file, and at the top for a name with none.
			const size_t slash = value.rfind('/');
			const std::string start = (slash == std::string::npos || slash == 0) ? "/" : value.substr(0, slash);
			if (browser.exec(start.c_str()))
			{
				value = browser.getSelectedFile()->Name;
				changeNotify(name(), (void *) value.c_str());
			}
		}

		int edit(CMenuTarget *parent)
		{
			switch (kind())
			{
				case coreapi::TextKind::Pin:
				{
					CPINChangeWidget dialog(name(), &value, size(), hint1, spec.text->allowed, this);
					dialog.exec(parent, "");
					break;
				}
				case coreapi::TextKind::Directory:
					chooseDirectory(parent);
					break;
				case coreapi::TextKind::File:
#ifdef USE_SMS_INPUT
					/* The update url file is typed by name here, not picked: a box built for
					   the SMS input has no use for a browser on it. The characters are what
					   the input offers, so they stay with the widget; the length is the rule's.
					   Written as the screen wrote it, where the two quotes join to none. */
					if (spec.key == "softupdate_url_file")
					{
						CStringInputSMS dialog(name(), &value, size(), hint1, hint2,
								       "abcdefghijklmnopqrstuvwxyz0123456789!""$%&/()=?-. ", this);
						dialog.exec(parent, "");
						break;
					}
#endif
					chooseFile(parent);
					break;
				default:
					if (spec.secret)
					{
						// A credential is typed on the keyboard with its characters hidden.
						CCTextInputDialog dialog(g_Locale->getText(name()), &value, this);
						dialog.setHintText(dialogHint());
						dialog.setPlaceholder(g_Locale->getText(name()));
						dialog.enableOnScreenKeyboard(true);
						dialog.enablePasswordMode(true);
						dialog.setAllowEmpty(true);
						if (size() > 0)
							dialog.setMaxChars((size_t) size());
						dialog.exec(parent, "");
					}
					else if (spec.text != NULL && spec.text->allowed != NULL && size() > 0)
					{
						CStringInput dialog(name(), &value, size(), hint1, hint2, spec.text->allowed, this);
						dialog.exec(parent, "");
					}
					else
					{
						CKeyboardInput dialog(name(), &value, size(), this, NULL, hint1, hint2);
						dialog.exec(parent, "");
					}
					break;
			}
			return menu_return::RETURN_REPAINT;
		}

	public:
		/* A write made elsewhere while a dialog edits the text is taken once the
		   dialog has ended: the dialogs edit it in place. */
		int exec(CMenuTarget *parent, const std::string &)
		{
			text.beginEdit();
			const int res = edit(parent);
			text.endEdit();
			if (item != NULL)
				item->setOption(shown());
			return res;
		}

		bool changeNotify(const neutrino_locale_t n, void *data)
		{
			store();
			const bool r = next != NULL && next->changeNotify(n, data);
			settleRow(spec.key, menu);
			return r;
		}
		bool changeNotify(const std::string &n, void *data)
		{
			store();
			const bool r = next != NULL && next->changeNotify(n, data);
			settleRow(spec.key, menu);
			return r;
		}
		bool changeNotify(lua_State *L, const std::string &id, const std::string &action, void *data)
		{
			store();
			const bool r = next != NULL && next->changeNotify(L, id, action, data);
			settleRow(spec.key, menu);
			return r;
		}
};

class CSettingText : private SettingText, public CMenuForwarder, public SettingFollow
{
	public:
		CSettingText(const coreapi::MenuItemSpec &s, bool is_active, CChangeObserver *observer, CMenuWidget *owner,
			     const neutrino_msg_t direct_key, neutrino_locale_t dialog_hint1, neutrino_locale_t dialog_hint2)
			: SettingText(s, observer, owner, dialog_hint1, dialog_hint2),
			  CMenuForwarder(localeFromKey(s.label_key), is_active, std::string(), target(), NULL, direct_key)
		{
			item = this;
			setOption(shown());
		}

		~CSettingText() { activeSet().forget(this); }

		void followValue(bool on_top)
		{
			if (!text.follow())
				return;
			setOption(shown());
			if (on_top && used && x != -1)
				paint(selectedIn(menu, this));
		}

		void setActiveQuietly(bool a) { active = current_active = a; }
};

/* The chooser a key row opens, built before the forwarder that points at it and
   kept as long as that is: the forwarder is the menu's to delete, so the
   chooser cannot be one the menu deletes as well. */
struct SettingKeyChooser
{
	CKeyChooser chooser;

	SettingKeyChooser(const coreapi::MenuItemSpec &s)
		: chooser((unsigned int *) s.int_pointer(g_settings), localeFromKey(s.label_key), NEUTRINO_ICON_SETTINGS)
	{
	}
};

/* A key row as an item: the key's name beside the label, and the chooser behind
   it. The chooser edits the member itself, so the observer is only told that it
   changed, once the chooser has been left. */
class CSettingKey : private SettingKeyChooser, public CMenuForwarder, public SettingFollow
{
		const int *const member;
		CChangeObserver *const next;
		CMenuWidget *const menu;
		const std::string rowKey;
		const neutrino_locale_t label;

	public:
		CSettingKey(const coreapi::MenuItemSpec &s, bool is_active,
			    CChangeObserver *observer, CMenuWidget *owner, const neutrino_msg_t direct_key)
			: SettingKeyChooser(s),
			  CMenuForwarder(localeFromKey(s.label_key), is_active, chooser.getKeyName(), &chooser, NULL, direct_key),
			  member(s.int_pointer(g_settings)), next(observer), menu(owner), rowKey(s.key), label(localeFromKey(s.label_key))
		{
		}

		~CSettingKey() { activeSet().forget(this); }

		void followValue(bool on_top)
		{
			setOption(chooser.getKeyName());
			if (on_top && used && x != -1)
				paint(selectedIn(menu, this));
		}

		void setActiveQuietly(bool a) { active = current_active = a; }

		int exec(CMenuTarget *parent)
		{
			const int before = *member;
			const int res = CMenuForwarder::exec(parent);
			if (*member != before)
			{
				if (next != NULL)
					next->changeNotify(label, NULL);
				settleRow(rowKey, menu);
			}
			return res;
		}
};

/* The channels a colour chooser edits, as steps from 0 to 100 in a copy of its own:
   a colour row is no member the chooser could point at. Read from the row as the
   item is built and written back as the chooser is left, before the screen's
   observer hears of it. */
class SettingColor : public CChangeObserver
{
	protected:
		const coreapi::MenuItemSpec spec;
		CChangeObserver *const next;
		CMenuWidget *const menu;
		unsigned char steps[VALUES];
		FollowedColor color;

		SettingColor(const coreapi::MenuItemSpec &s, CChangeObserver *observer, CMenuWidget *owner)
			: spec(s), next(observer), menu(owner), color(s, steps)
		{
			for (size_t i = 0; i < VALUES; i++)
				steps[i] = 0;
			if (!color.reread())
				dprintf(DEBUG_NORMAL, "[settingitem] %s: no colour to show\n", spec.key.c_str());
		}

		size_t channels() const { return spec.channels; }

		unsigned char *alpha() { return channels() == 4 ? &steps[VALUE_A] : NULL; }

	public:
		/* The chooser tells this whenever it is left, cancelled too; a chooser
		   that ends on the channels it started on changed nothing and is told
		   to nobody. */
		bool changeNotify(const neutrino_locale_t name, void *data)
		{
			if (!color.leave())
				return false;
			const bool r = next != NULL && next->changeNotify(name, data);
			settleRow(spec.key, menu);
			return r;
		}
		bool changeNotify(const std::string &name, void *data)
		{
			if (!color.leave())
				return false;
			const bool r = next != NULL && next->changeNotify(name, data);
			settleRow(spec.key, menu);
			return r;
		}
		bool changeNotify(lua_State *L, const std::string &id, const std::string &action, void *data)
		{
			if (!color.leave())
				return false;
			const bool r = next != NULL && next->changeNotify(L, id, action, data);
			settleRow(spec.key, menu);
			return r;
		}
};

struct SettingColorChooser
{
	CColorChooser chooser;

	SettingColorChooser(const coreapi::MenuItemSpec &s, unsigned char *steps, unsigned char *alpha, CChangeObserver *edited)
		: chooser(localeFromKey(s.label_key), &steps[VALUE_R], &steps[VALUE_G], &steps[VALUE_B], alpha, edited)
	{
	}
};

class CSettingColor : private SettingColor, private SettingColorChooser, public CSettingColorItem, public SettingFollow
{
	public:
		CSettingColor(const coreapi::MenuItemSpec &s, bool is_active,
			      CChangeObserver *observer, CMenuWidget *owner, const neutrino_msg_t direct_key)
			: SettingColor(s, observer, owner),
			  SettingColorChooser(s, steps, alpha(), this),
			  CSettingColorItem(localeFromKey(s.label_key), is_active, NULL, &chooser, NULL, direct_key)
		{
		}

		~CSettingColor() { activeSet().forget(this); }

		// A write made elsewhere while the chooser is open is taken once it is left.
		int exec(CMenuTarget *parent)
		{
			color.beginEdit();
			const int res = CSettingColorItem::exec(parent);
			color.endEdit();
			return res;
		}

		void followValue(bool on_top)
		{
			if (!color.follow())
				return;
			if (on_top && used && x != -1)
				paint(selectedIn(menu, this));
		}

		void setActiveQuietly(bool a) { active = current_active = a; }

		CColorChooser *colorChooser() { return &chooser; }
};

} // namespace

/* The one place a row becomes an item. want says which shape the caller can
   take: NULL and nothing added for the other, so a typed call never hands back
   an item cast to what it is not. */
namespace
{

enum Want { WantAny, WantChoice, WantNumber };

CMenuItem *build(CMenuWidget *menu, const char *key, ScreenActive screen, CChangeObserver *observer,
		 const neutrino_msg_t direct_key, bool slider, bool pulldown, bool numeric, neutrino_locale_t dialog_hint1,
		 neutrino_locale_t dialog_hint2, Want want)
{
	coreapi::Result<coreapi::MenuItemSpec> r = coreapi::menuItem(key ? key : "");
	if (!r.ok())
	{
		// A setting the board lacks is no fault of the declaration.
		if (r.error().code == coreapi::ErrorCode::SettingNotOnThisBox)
		{
			dprintf(DEBUG_INFO, "[settingitem] %s: %s\n", key ? key : "(null)", r.error().message.c_str());
			return NULL;
		}
		/* A key this build does not declare is either a row behind arms the
		   build leaves out, which a screen names rather than repeating them, or a
		   key a screen got wrong, which only a key built at run time can be once
		   the suite holds the literal ones. Said once per key, so the second is
		   seen and a menu opened again does not repeat the first. */
		static std::set<std::string> told;
		const std::string named(key ? key : "(null)");
		if (told.insert(named).second)
			dprintf(DEBUG_NORMAL, "[settingitem] %s: %s\n", named.c_str(), r.error().message.c_str());
		return NULL;
	}

	const coreapi::MenuItemSpec &spec = r.value();
	// The lock is a property of the row as declared, the screen's answer is asked
	// again on every pass. Built in the state both give now, so a row whose
	// condition fails is not offered until something changes it.
	const bool locked = spec.locked;
	SettingActiveSet<CMenuItem>::Screen allowed = [screen, locked]() { return !locked && screen(); };
	const bool active = allowed() && coreapi::settings::conditionsHoldNow(spec.key);
	const bool choice = coreapi::offeredAsList(spec);
	const bool number = spec.type == coreapi::ValueType::Int && !choice;
	if ((want == WantNumber && !number) || (want == WantChoice && !choice))
	{
		dprintf(DEBUG_NORMAL, "[settingitem] %s: not a row of the shape asked for\n", key ? key : "(null)");
		return NULL;
	}

	CMenuItem *item = NULL;
	SettingFollow *follow = NULL;
	SettingActiveSet<CMenuItem>::Hold hold;
	SettingActiveSet<CMenuItem>::Commit commit;
	if (number)
	{
		CSettingNumberChooser *c = new CSettingNumberChooser(spec, active, observer, direct_key, slider, numeric, menu);
		item = c;
		follow = c;
		hold = [c]() { c->holdValue(); };
		commit = [c]() { c->commitHeld(); };
	}
	else if (choice)
	{
		CSettingChooser *c = new CSettingChooser(spec, active, observer, direct_key, pulldown, menu);
		item = c;
		follow = c;
		hold = [c]() { c->holdValue(); };
		commit = [c]() { c->commitHeld(); };
	}
	else if (spec.type == coreapi::ValueType::Key)
	{
		// The chooser edits the member, which a key row without one cannot offer.
		if (spec.int_pointer == NULL)
		{
			dprintf(DEBUG_NORMAL, "[settingitem] %s: a key without a member to edit\n", key ? key : "(null)");
			return NULL;
		}
		CSettingKey *k = new CSettingKey(spec, active, observer, menu, direct_key);
		item = k;
		follow = k;
	}
	else if (spec.type == coreapi::ValueType::Color)
	{
		CSettingColor *c = new CSettingColor(spec, active, observer, menu, direct_key);
		item = c;
		follow = c;
	}
	else if (spec.type == coreapi::ValueType::String)
	{
		// A row nothing names has no caption to draw, which is not a row to offer.
		if (spec.label_key.empty())
		{
			dprintf(DEBUG_NORMAL, "[settingitem] %s: a text row without a label\n", key ? key : "(null)");
			return NULL;
		}
		CSettingText *t = new CSettingText(spec, active, observer, menu, direct_key, dialog_hint1, dialog_hint2);
		item = t;
		follow = t;
	}
	else
	{
		dprintf(DEBUG_NORMAL, "[settingitem] %s: no widget for this type\n", key ? key : "(null)");
		return NULL;
	}

	if (!spec.hint_key.empty())
		item->setHint("", localeFromKey(spec.hint_key));

	activeSet().add(item, spec.key, allowed, active, menu);
	activeSet().rereadWith(item, [follow](bool paint) { follow->followValue(paint); },
			       [follow](bool a) { follow->setActiveQuietly(a); });
	activeSet().deferWith(item, hold, commit);
	menu->addItem(item);
	return item;
}

} // namespace

CMenuItem *addSetting(CMenuWidget *menu, const char *key, ScreenActive active,
		      CChangeObserver *observer, const neutrino_msg_t direct_key, bool slider, bool pulldown, bool numeric,
		      neutrino_locale_t dialog_hint1, neutrino_locale_t dialog_hint2)
{
	return build(menu, key, active, observer, direct_key, slider, pulldown, numeric, dialog_hint1, dialog_hint2, WantAny);
}

CMenuOptionChooser *addChoiceSetting(CMenuWidget *menu, const char *key, ScreenActive active,
				     CChangeObserver *observer, const neutrino_msg_t direct_key, bool pulldown)
{
	// The item is built as a CSettingChooser, a CMenuOptionChooser.
	return static_cast<CMenuOptionChooser *>(build(menu, key, active, observer, direct_key, false, pulldown, false, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, WantChoice));
}

void settingsWrittenElsewhere(const std::vector<std::string> &keys)
{
	followWrites<CMenuItem>(activeSet(), keys, CMenuWidget::waiting(),
				[](void *menu) -> const std::vector<CMenuItem *> & { return static_cast<CMenuWidget *>(menu)->getItems(); },
				coreapi::settings::conditionsHoldNow);
}

CFollowForwarder::CFollowForwarder(const neutrino_locale_t text, const bool is_active, const std::string &option,
				   CMenuTarget *target, const char *action_key, const neutrino_msg_t direct_key)
	: CMenuForwarder(text, is_active, option, target, action_key, direct_key)
{
}

CFollowForwarder::CFollowForwarder(const neutrino_locale_t text, const bool is_active, const char *option,
				   CMenuTarget *target, const char *action_key, const neutrino_msg_t direct_key)
	: CMenuForwarder(text, is_active, option, target, action_key, direct_key)
{
}

CFollowForwarder::~CFollowForwarder()
{
	activeSet().forget(this);
}

void CFollowForwarder::follow(CMenuWidget *menu, const std::string &key, const std::vector<std::string> &keys,
			      ScreenActive screen, const std::function<void()> &refresh)
{
	activeSet().add(this, key, screen, active, menu);
	activeSet().rereadOnAlso(this, keys);
	activeSet().rereadWith(this,
			       [this, menu, refresh](bool paint_now)
			       {
				       if (refresh)
					       refresh();
				       if (paint_now && used && x != -1)
					       paint(selectedIn(menu, this));
			       },
			       [this](bool a) { active = current_active = a; });
}

void afterApply(CMenuItem *item, const std::function<bool()> &after)
{
	activeSet().afterApply(item, after);
}

void applyOnLeave(CMenuItem *item)
{
	activeSet().deferApply(item);
}

void openListOnStep(CMenuOptionChooser *item)
{
	// What addChoiceSetting hands back is a CSettingChooser.
	if (item != NULL)
		static_cast<CSettingChooser *>(item)->openListOnStep();
}

void settleLeft(CMenuItem *item)
{
	std::string key;
	// The menu is gone from the screen, so its items are not judged again.
	if (activeSet().takePending(item, key))
		settleRow(key, NULL, item);
}

CMenuOptionNumberChooser *addNumberSetting(CMenuWidget *menu, const char *key, ScreenActive active,
					   CChangeObserver *observer, const neutrino_msg_t direct_key, bool slider, bool numeric)
{
	return static_cast<CMenuOptionNumberChooser *>(build(menu, key, active, observer, direct_key, slider, false, numeric, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, WantNumber));
}
