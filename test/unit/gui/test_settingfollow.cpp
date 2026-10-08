/*
 * test_settingfollow.cpp - tests for how an item takes a value written elsewhere
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

#include "support/catch.hpp"
#include "support/fakes.h"

#include "gui/widget/settingfollow.h"

#include <string>
#include <vector>

using namespace coreapi;

TEST_CASE("the keys of a settings-written message are split one to a line", "[follow]")
{
	std::vector<std::string> k = writtenKeys("a\nb\n");
	REQUIRE(k.size() == 2);
	CHECK(k[0] == "a");
	CHECK(k[1] == "b");
	CHECK(writtenKeys("").empty());
	CHECK(writtenKeys(NULL).empty());
	k = writtenKeys("a\nb");
	REQUIRE(k.size() == 2);
	CHECK(k[1] == "b");
	k = writtenKeys("\n\nvideo_Mode\n");
	REQUIRE(k.size() == 1);
	CHECK(k[0] == "video_Mode");
}

namespace
{
// The menu builder asks the box about every row; this one has everything.
struct FollowBox
{
	FakeSystemSource box;
	InstalledSystemSource installed;
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner;
	FollowBox() : installed(&box), installed_tuner(&tuner) {}
};
} // anonymous namespace

/* A row whose value is a bit of a mask has no int member, so its item keeps a
   copy; after a write elsewhere the copy is the written value, so the next
   press goes on from it. */
TEST_CASE("the copy a row without an int member keeps is read again after a write", "[follow]")
{
	FollowBox fb;
	Result<MenuItemSpec> r = menuItem("recording_audio_pids_std");
	REQUIRE(r.ok());
	const MenuItemSpec &spec = r.value();
	REQUIRE(spec.int_pointer == NULL);
	const int kept_mask = g_settings.recording_audio_pids_default;

	int value = 0;
	bool known = false;
	REQUIRE(menuValueWrite(spec, g_settings, 1));
	rereadCopy(spec, value, known);
	REQUIRE(known);
	REQUIRE(value == 1);

	// Written elsewhere: the copy follows.
	REQUIRE(menuValueWrite(spec, g_settings, 0));
	rereadCopy(spec, value, known);
	REQUIRE(value == 0);

	g_settings.recording_audio_pids_default = kept_mask;
}

/* The text dialogs edit the item's text in place, character by character at a
   cursor. A shorter text written from the web while one is open must not land
   in that buffer; a cancel then shows the web's text, an OK keeps what was
   typed, the last write winning. */
TEST_CASE("a text being edited is not followed until its dialog ends", "[follow]")
{
	FollowBox fb;
	Result<MenuItemSpec> r = menuItem("keyboard_layout");
	REQUIRE(r.ok());
	const std::string kept_layout = g_settings.keyboard_layout;
	REQUIRE(menuTextWrite(r.value(), g_settings, "deutsch"));

	FollowedText text(r.value());
	REQUIRE(text.value == "deutsch");

	// The dialog pads to its width and works at a cursor far into it.
	text.beginEdit();
	text.value = "deutschdeutsch12";
	REQUIRE(menuTextWrite(r.value(), g_settings, "en"));
	REQUIRE_FALSE(text.follow());
	REQUIRE(text.value.size() == 16);
	CHECK(text.value.at(10) == 't');
	// Cancelled: the dialog puts its entry text back, and the item then shows the web's.
	text.value = "deutsch";
	text.endEdit();
	REQUIRE(text.value == "en");

	// Left with OK after a web write: what was typed is written and kept.
	text.beginEdit();
	text.value = "english";
	REQUIRE(menuTextWrite(r.value(), g_settings, "francais"));
	REQUIRE_FALSE(text.follow());
	REQUIRE(text.write());
	text.endEdit();
	REQUIRE(text.value == "english");
	REQUIRE(g_settings.keyboard_layout == "english");

	// With no dialog open a write is followed at once.
	REQUIRE(menuTextWrite(r.value(), g_settings, "deutsch"));
	REQUIRE(text.follow());
	REQUIRE(text.value == "deutsch");

	g_settings.keyboard_layout = kept_layout;
}

/* A row can drop what the dialog left: a channel id must not be empty, the
   dialog lets an empty entry through, and the row keeps its old id without
   answering no. The item then shows what the settings hold, which the next
   dialog starts from. */
TEST_CASE("a text the row refuses on OK leaves the item showing the stored value", "[follow]")
{
	FollowBox fb;
	Result<MenuItemSpec> r = menuItem("startchanneltv_id");
	REQUIRE(r.ok());
	std::string kept;
	REQUIRE(menuTextRead(r.value(), g_settings, kept));
	REQUIRE(menuTextWrite(r.value(), g_settings, "1a2b"));

	FollowedText text(r.value());
	REQUIRE(text.value == "1a2b");

	text.beginEdit();
	text.value = "";
	text.write();
	text.endEdit();
	CHECK(text.value == "1a2b");
	std::string now;
	REQUIRE(menuTextRead(r.value(), g_settings, now));
	CHECK(now == "1a2b");

	REQUIRE(menuTextWrite(r.value(), g_settings, kept));
}

/* The colour chooser edits the item's channels in place and tells the item
   whenever it is left, with the channels put back where it was cancelled. */
TEST_CASE("a colour being chosen is not followed until its chooser is left", "[follow]")
{
	FollowBox fb;
	Result<MenuItemSpec> r = menuItem("theme.menu_Head");
	REQUIRE(r.ok());
	std::string kept;
	REQUIRE(menuTextRead(r.value(), g_settings, kept));

	unsigned char steps[4] = { 10, 20, 30, 40 };
	unsigned char web[4] = { 50, 60, 70, 80 };
	REQUIRE(menuTextWrite(r.value(), g_settings, colorText(steps, r.value().channels)));
	FollowedColor color(r.value(), steps);

	color.beginEdit();
	REQUIRE(menuTextWrite(r.value(), g_settings, colorText(web, r.value().channels)));
	REQUIRE_FALSE(color.follow());
	CHECK(steps[0] == 10);
	// Cancelled: the chooser left on what it started on writes nothing; the web's colour shows.
	REQUIRE_FALSE(color.leave());
	color.endEdit();
	CHECK(steps[0] == 50);
	CHECK(steps[2] == 70);

	// Moved and left: written, kept.
	color.beginEdit();
	steps[0] = 5;
	REQUIRE(color.leave());
	color.endEdit();
	CHECK(steps[0] == 5);
	std::string now;
	REQUIRE(menuTextRead(r.value(), g_settings, now));
	CHECK(now == colorText(steps, r.value().channels));

	REQUIRE(menuTextWrite(r.value(), g_settings, kept));
}
