/*
 * settingitem.h - a setup menu item built from the settings declaration
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

#ifndef __settingitem_h__
#define __settingitem_h__

#include <gui/widget/menue.h>
#include <driver/rcinput.h>
#include <system/locals.h>

#include <functional>
#include <string>
#include <type_traits>

/* What a screen allows of an item, for reasons that are not the row's own
   conditions (a hardware revision, a recording running). A function is asked on
   every pass, so it answers for the moment. A bool is a decision that does not
   change while the menu is open and is kept as such. A screen that mirrors the
   value of another setting passes a function, or true and lets the row's
   conditions speak: a bool read at build time would hold the item dead after
   that setting changed. */
class ScreenActive
{
	public:
		ScreenActive(bool constant) : fn(constant ? always_true : always_false) {}
		template <class F, class = typename std::enable_if<!std::is_arithmetic<F>::value && !std::is_same<F, ScreenActive>::value>::type>
		ScreenActive(F f) : fn(f) {}
		bool operator()() const { return fn(); }

	private:
		static bool always_true() { return true; }
		static bool always_false() { return false; }
		std::function<bool()> fn;
};

/* Builds the widget for a declared setting and adds it. The item is the
   CMenuOptionChooser or CMenuOptionNumberChooser itself, so a screen can still
   set hint icons, connect OnAfterChangeOption or change its active state.
   NULL when the row cannot be shown, and for a setting this box lacks, which
   adds nothing: a screen that keeps the item checks for NULL. A row in two
   shapes is built in the one this box offers. A row the declaration reports
   locked is inactive whatever active says. slider is handed to a number
   chooser and means nothing to a choice; pulldown is handed to a choice, which
   then opens its list on OK, and means nothing to a number; numeric lets a
   number be typed in as digits and means nothing to a choice. A number shows the
   value its row names in words, off or last used, in those words.

   A row whose value is no int member, a daemon's or a bit of a mask, gets an
   item holding the int itself: read as it is built and written on each change,
   before observer is told. One whose daemon cannot be asked stays inactive
   and shows that there is no value.

   A number shows its row's unit after it, or the row's own format where it
   names one, so a screen sets no number format for a row that declares one.

   A text row gets a forwarder that opens the dialog its rule names: a pin
   entry, a digit entry, a folder or file browser, a hidden-text dialog for a
   credential, or the keyboard, limited to the rule's length. dialog_hint1 and
   dialog_hint2 are the words that dialog shows beside its field; they are not
   the menu hint. A text row without a label is not offered.

   A key row gets the forwarder the keybinding screen showed for it: the key's
   name beside the label and the key chooser behind it. A colour row gets a
   CSettingColorItem, whose chooser works on the channels in a copy of its own
   and writes them back as it is left. For both the observer is told once the
   chooser has been left, the key one only when the key changed. */
CMenuItem *addSetting(CMenuWidget *menu, const char *key, ScreenActive active = true,
		      CChangeObserver *observer = NULL, const neutrino_msg_t direct_key = CRCInput::RC_nokey,
		      bool slider = false, bool pulldown = false, bool numeric = false,
		      neutrino_locale_t dialog_hint1 = NONEXISTANT_LOCALE,
		      neutrino_locale_t dialog_hint2 = NONEXISTANT_LOCALE);

/* The same, for a screen that keeps the item and needs it as what it is, so no
   cast stands at the call. Each takes what addSetting takes of that shape and
   answers NULL, adding nothing, for a row of the other shape: a choice row for
   addNumberSetting, a number row for addChoiceSetting. */
CMenuOptionChooser *addChoiceSetting(CMenuWidget *menu, const char *key, ScreenActive active = true,
				     CChangeObserver *observer = NULL,
				     const neutrino_msg_t direct_key = CRCInput::RC_nokey, bool pulldown = false);
CMenuOptionNumberChooser *addNumberSetting(CMenuWidget *menu, const char *key, ScreenActive active = true,
					   CChangeObserver *observer = NULL,
					   const neutrino_msg_t direct_key = CRCInput::RC_nokey, bool slider = false,
					   bool numeric = false);

class CColorChooser;

/* What addSetting returns for a colour row. A screen that previews the colour
   on a gradient names the mode on this chooser, which the item owns. A static
   cast from the CMenuItem it got is right exactly where the key it passed is
   the key of a colour row. */
class CSettingColorItem : public CMenuForwarder
{
	public:
		CSettingColorItem(const neutrino_locale_t text, const bool active, const char *option,
				  CMenuTarget *target, const char *action_key, const neutrino_msg_t direct_key)
			: CMenuForwarder(text, active, option, target, action_key, direct_key) {}
		virtual CColorChooser *colorChooser() = 0;
};

// NONEXISTANT_LOCALE when unknown or empty. Answers are cached without a lock,
// so this is for the GUI thread only.
neutrino_locale_t localeFromKey(const std::string &key);

#endif
