/*
 * settingfollow.h - how an item built from a row takes a value written elsewhere
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

#ifndef __settingfollow_h__
#define __settingfollow_h__

/* Header only and free of widgets, so the suite holds the very code the items
   run: the split of the message, the copy a row without an int member keeps,
   and the text and colour an item holds while its own dialog may edit them. */

#include <coreapi/base/schema.h>
#include <coreapi/settings/menuspec.h>
#include <system/settings.h>

#include <cstring>
#include <string>
#include <vector>

extern SNeutrinoSettings g_settings;

/* The keys of a settings-written message: one to a line, the last line with or
   without its newline, empty lines left out. */
inline std::vector<std::string> writtenKeys(const char *text)
{
	std::vector<std::string> keys;
	if (text == NULL)
		return keys;
	const std::string all(text);
	size_t from = 0;
	while (from < all.size())
	{
		size_t end = all.find('\n', from);
		if (end == std::string::npos)
			end = all.size();
		if (end > from)
			keys.push_back(all.substr(from, end - from));
		from = end + 1;
	}
	return keys;
}

/* Whether an item's own dialog is editing what it holds. A dialog edits that
   buffer in place, the text ones character by character, so a write followed
   into it meanwhile would pull the text from under the cursor; it is taken
   once the dialog has ended instead. */
class EditState
{
	public:
		EditState() : editing(false) {}

		void begin() { editing = true; }

		bool mayFollow() const { return !editing; }

		void end() { editing = false; }

	private:
		bool editing;
};

/* The int a row without an int member keeps, read again through the row. A
   value that cannot be read is shown as none, below the range. */
inline void rereadCopy(const coreapi::MenuItemSpec &spec, int &value, bool &known)
{
	if (spec.int_pointer != NULL)
		return;
	long v = 0;
	known = coreapi::menuValueRead(spec, g_settings, v);
	value = known ? (int) v : (int) spec.min - 1;
}

// The text a text row's item holds and its dialog edits.
struct FollowedText
{
	const coreapi::MenuItemSpec spec;
	std::string value;
	EditState edit;

	explicit FollowedText(const coreapi::MenuItemSpec &s) : spec(s), value() { reread(); }

	bool reread() { return coreapi::menuTextRead(spec, g_settings, value); }

	// A write made elsewhere. False while the dialog edits the text.
	bool follow() { return edit.mayFollow() && reread(); }

	void beginEdit() { edit.begin(); }

	bool write() { return coreapi::menuTextWrite(spec, g_settings, value); }

	/* The item shows what the settings hold when the dialog is over, whatever
	   the dialog did: the text it left after an OK, the web's after a cancel,
	   and the old one when the row dropped a text it cannot hold, which it does
	   without saying so. */
	void endEdit()
	{
		edit.end();
		reread();
	}
};

/* The channels a colour row's item holds and its chooser edits. The chooser
   tells its observer whenever it is left, with the channels put back where it
   was cancelled, so a write is what differs from the channels it started on. */
struct FollowedColor
{
	const coreapi::MenuItemSpec spec;
	unsigned char *const steps;
	unsigned char start[4];
	EditState edit;

	FollowedColor(const coreapi::MenuItemSpec &s, unsigned char *channels) : spec(s), steps(channels)
	{
		std::memset(start, 0, sizeof(start));
	}

	size_t count() const { return spec.channels < 4 ? spec.channels : 4; }

	bool reread()
	{
		std::string text;
		return coreapi::menuTextRead(spec, g_settings, text) && coreapi::readColorText(text, count(), steps);
	}

	bool follow() { return edit.mayFollow() && reread(); }

	void beginEdit()
	{
		std::memcpy(start, steps, count());
		edit.begin();
	}

	// What the chooser was left with, written where it is not what it started on.
	bool leave()
	{
		if (std::memcmp(start, steps, count()) == 0)
			return false;
		return coreapi::menuTextWrite(spec, g_settings, coreapi::colorText(steps, count()));
	}

	// As for the text: the item shows what the settings hold once the chooser is over.
	void endEdit()
	{
		edit.end();
		reread();
	}
};

#endif
