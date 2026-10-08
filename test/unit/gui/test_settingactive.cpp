/*
 * test_settingactive.cpp - tests for the walk that keeps a menu's items in step
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

#include "gui/widget/settingactive.h"

#include <set>
#include <string>
#include <vector>

namespace
{

struct FakeItem
{
	int told;
	bool active;
	FakeItem() : told(0), active(true) {}
	void setActive(bool a) { active = a; ++told; }
};

std::set<std::string> g_failing;
bool g_screen_allows = false;

bool holdsUnlessFailing(const std::string &key) { return g_failing.count(key) == 0; }

} // namespace

TEST_CASE("a change in one row moves a sibling whose condition it decides", "[settingactive]")
{
	g_failing.clear();
	FakeItem master, dependent, separator;
	SettingActiveSet<FakeItem> set;
	set.add(&master, "master", NULL, true);
	set.add(&dependent, "dependent", NULL, true);
	std::vector<FakeItem *> items;
	items.push_back(&separator);
	items.push_back(&master);
	items.push_back(&dependent);

	set.reevaluate(items, holdsUnlessFailing);
	REQUIRE(dependent.told == 0);

	g_failing.insert("dependent");
	set.reevaluate(items, holdsUnlessFailing);
	REQUIRE(dependent.active == false);
	REQUIRE(dependent.told == 1);
	REQUIRE(master.told == 0);
	REQUIRE(separator.told == 0);

	g_failing.clear();
	set.reevaluate(items, holdsUnlessFailing);
	REQUIRE(dependent.active == true);
	REQUIRE(dependent.told == 2);
}

TEST_CASE("a row the screen switched off stays off whatever its conditions say", "[settingactive]")
{
	g_failing.clear();
	FakeItem item;
	item.active = false;
	SettingActiveSet<FakeItem> set;
	set.add(&item, "k", [] { return false; }, false);
	std::vector<FakeItem *> items(1, &item);
	set.reevaluate(items, holdsUnlessFailing);
	REQUIRE(item.active == false);
	REQUIRE(item.told == 0);
}

TEST_CASE("an item that is forgotten is no longer told", "[settingactive]")
{
	g_failing.clear();
	FakeItem item;
	SettingActiveSet<FakeItem> set;
	set.add(&item, "k", NULL, true);
	set.forget(&item);
	g_failing.insert("k");
	std::vector<FakeItem *> items(1, &item);
	set.reevaluate(items, holdsUnlessFailing);
	REQUIRE(item.told == 0);
}

TEST_CASE("every sibling whose state moved is told in one pass", "[settingactive]")
{
	g_failing.clear();
	FakeItem a, b, c;
	SettingActiveSet<FakeItem> set;
	set.add(&a, "a", NULL, true);
	set.add(&b, "b", NULL, true);
	set.add(&c, "c", NULL, true);
	std::vector<FakeItem *> items;
	items.push_back(&a);
	items.push_back(&b);
	items.push_back(&c);

	g_failing.insert("a");
	g_failing.insert("c");
	set.reevaluate(items, holdsUnlessFailing);
	REQUIRE(a.active == false);
	REQUIRE(b.active == true);
	REQUIRE(c.active == false);
}

TEST_CASE("a screen answer that turns true later enables the item", "[settingactive]")
{
	g_failing.clear();
	g_screen_allows = false;
	FakeItem item;
	item.active = false;
	SettingActiveSet<FakeItem> set;
	set.add(&item, "k", [] { return g_screen_allows; }, false);
	std::vector<FakeItem *> items(1, &item);

	set.reevaluate(items, holdsUnlessFailing);
	REQUIRE(item.active == false);
	g_screen_allows = true;
	set.reevaluate(items, holdsUnlessFailing);
	REQUIRE(item.active == true);
	REQUIRE(item.told == 1);
}

TEST_CASE("an item the screen disables stays off when its row condition flips on", "[settingactive]")
{
	g_failing.clear();
	g_screen_allows = false;
	FakeItem item;
	SettingActiveSet<FakeItem> set;
	g_failing.insert("k");
	set.add(&item, "k", [] { return g_screen_allows; }, false);
	item.active = false;
	std::vector<FakeItem *> items(1, &item);

	g_failing.clear();
	set.reevaluate(items, holdsUnlessFailing);
	REQUIRE(item.active == false);
	REQUIRE(item.told == 0);
}

namespace
{

std::vector<std::string> g_applied;

void recordApply(const std::string &key) { g_applied.push_back(key); }

} // namespace

TEST_CASE("a change of a text row is applied once and re-judges the row that depends on it", "[settingactive][settle]")
{
	g_failing.clear();
	g_applied.clear();
	FakeItem text, dependent;
	SettingActiveSet<FakeItem> set;
	set.add(&text, "text_row", NULL, true);
	set.add(&dependent, "dependent", NULL, true);
	std::vector<FakeItem *> items;
	items.push_back(&text);
	items.push_back(&dependent);

	// The text row's new value is what makes the other row's condition fail.
	g_failing.insert("dependent");
	settleChange(set, "text_row", recordApply, &items, holdsUnlessFailing);

	REQUIRE(g_applied.size() == 1);
	REQUIRE(g_applied[0] == "text_row");
	REQUIRE(dependent.active == false);
	REQUIRE(dependent.told == 1);
	REQUIRE(text.told == 0);
}

TEST_CASE("a change in a menu that is not known is still applied", "[settingactive][settle]")
{
	g_applied.clear();
	SettingActiveSet<FakeItem> set;
	settleChange(set, "colour_row", recordApply, static_cast<std::vector<FakeItem *> *>(NULL), holdsUnlessFailing);
	REQUIRE(g_applied.size() == 1);
}
