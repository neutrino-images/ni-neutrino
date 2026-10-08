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

#include <string>

/* Builds the widget for a declared setting and adds it. The item is the
   CMenuOptionChooser or CMenuOptionNumberChooser itself, so a screen can still
   set hint icons, connect OnAfterChangeOption or change its active state.
   NULL when the row cannot be shown, and for a setting this box lacks, which
   adds nothing: a screen that keeps the item checks for NULL. A row in two
   shapes is built in the one this box offers. A row the declaration reports
   locked is inactive whatever active says. slider is handed to a number
   chooser and means nothing to a choice; pulldown is handed to a choice, which
   then opens its list on OK, and means nothing to a number. A number shows the
   value its row names in words, off or last used, in those words.

   A row whose value is no int member, a daemon's or a bit of a mask, gets an
   item holding the int itself: read as it is built and written on each change,
   before observer is told. One whose daemon cannot be asked stays inactive
   and shows that there is no value. */
CMenuItem *addSetting(CMenuWidget *menu, const char *key, bool active = true,
		      CChangeObserver *observer = NULL, const neutrino_msg_t direct_key = CRCInput::RC_nokey,
		      bool slider = false, bool pulldown = false);

// NONEXISTANT_LOCALE when unknown or empty. Answers are cached without a lock,
// so this is for the GUI thread only.
neutrino_locale_t localeFromKey(const std::string &key);

#endif
