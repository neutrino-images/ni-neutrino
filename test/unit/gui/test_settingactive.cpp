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

#include <map>
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
	settleChange(set, "text_row", &text, recordApply, &items, holdsUnlessFailing);

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
	settleChange(set, "colour_row", static_cast<const FakeItem *>(NULL), recordApply,
		     static_cast<std::vector<FakeItem *> *>(NULL), holdsUnlessFailing);
	REQUIRE(g_applied.size() == 1);
}

namespace
{

// What the screen's work after the apply saw: how much had been applied by then
// and whether the dependent row had been judged yet.
size_t g_applied_before_after = 0;
int g_dependent_told_before_after = -1;

} // namespace

TEST_CASE("what the screen does after the apply runs once, after it and before the menu is judged", "[settingactive][settle]")
{
	g_failing.clear();
	g_applied.clear();
	FakeItem mode, dependent;
	SettingActiveSet<FakeItem> set;
	set.add(&mode, "mode", NULL, true);
	set.add(&dependent, "dependent", NULL, true);
	std::vector<FakeItem *> items;
	items.push_back(&mode);
	items.push_back(&dependent);

	int runs = 0;
	g_applied_before_after = 0;
	g_dependent_told_before_after = -1;
	set.afterApply(&mode, [&runs, &dependent]() {
		++runs;
		g_applied_before_after = g_applied.size();
		g_dependent_told_before_after = dependent.told;
		return true;
	});

	g_failing.insert("dependent");
	const bool repaint = settleChange(set, "mode", &mode, recordApply, &items, holdsUnlessFailing);

	REQUIRE(repaint);
	REQUIRE(runs == 1);
	REQUIRE(g_applied_before_after == 1);
	REQUIRE(g_dependent_told_before_after == 0);
	REQUIRE(dependent.told == 1);

	// Another item's change does not run it, and neither does one with no item named.
	REQUIRE_FALSE(settleChange(set, "dependent", &dependent, recordApply, &items, holdsUnlessFailing));
	REQUIRE_FALSE(settleChange(set, "mode", static_cast<const FakeItem *>(NULL), recordApply, &items, holdsUnlessFailing));
	REQUIRE(runs == 1);

	// A forgotten item takes its work with it.
	set.forget(&mode);
	REQUIRE_FALSE(settleChange(set, "mode", &mode, recordApply, &items, holdsUnlessFailing));
	REQUIRE(runs == 1);
}

namespace
{

int g_asked = 0;
bool g_answer = false;

bool fakeAsk()
{
	++g_asked;
	return g_answer;
}

} // namespace

/* The video mode question: the GUI change is applied once by the generic step,
   and a no puts the old mode back and applies once more. */
TEST_CASE("a change asked about after it took effect is kept on yes and put back and applied again on no", "[settingactive][settle]")
{
	g_applied.clear();
	g_asked = 0;
	int value = 5;
	int kept = 5;

	// Unchanged: nothing is asked.
	REQUIRE_FALSE(keepOrRestore(value, kept, "video_Mode", fakeAsk, recordApply));
	REQUIRE(g_asked == 0);

	value = 7;
	g_answer = true;
	REQUIRE_FALSE(keepOrRestore(value, kept, "video_Mode", fakeAsk, recordApply));
	REQUIRE(g_asked == 1);
	REQUIRE(value == 7);
	REQUIRE(kept == 7);
	REQUIRE(g_applied.empty());

	value = 9;
	g_answer = false;
	REQUIRE(keepOrRestore(value, kept, "video_Mode", fakeAsk, recordApply));
	REQUIRE(g_asked == 2);
	REQUIRE(value == 7);
	REQUIRE(kept == 7);
	REQUIRE(g_applied.size() == 1);
	REQUIRE(g_applied[0] == "video_Mode");
}

TEST_CASE("a GUI change with its question applies the row once on yes and twice on no", "[settingactive][settle]")
{
	g_failing.clear();
	g_applied.clear();
	g_asked = 0;
	FakeItem mode;
	SettingActiveSet<FakeItem> set;
	set.add(&mode, "video_Mode", NULL, true);
	std::vector<FakeItem *> items(1, &mode);
	int value = 1;
	int kept = 1;
	set.afterApply(&mode, [&value, &kept]() {
		keepOrRestore(value, kept, "video_Mode", fakeAsk, recordApply);
		return true;
	});

	value = 2;
	g_answer = true;
	settleChange(set, "video_Mode", &mode, recordApply, &items, holdsUnlessFailing);
	REQUIRE(g_applied.size() == 1);
	REQUIRE(g_asked == 1);
	REQUIRE(value == 2);

	g_applied.clear();
	value = 3;
	g_answer = false;
	settleChange(set, "video_Mode", &mode, recordApply, &items, holdsUnlessFailing);
	REQUIRE(g_applied.size() == 2);
	REQUIRE(g_asked == 2);
	REQUIRE(value == 2);
}

/* The remote control: every step of the chooser stays in the item, and leaving the
   menu puts the last value into the setting, applies it and asks once, the way the
   item does it (settled() and settleLeft()). */
TEST_CASE("a deferred item is applied and asked about once when its menu is left", "[settingactive][settle]")
{
	g_failing.clear();
	g_applied.clear();
	g_asked = 0;
	FakeItem hardware, other;
	SettingActiveSet<FakeItem> set;
	set.add(&hardware, "remote_control_hardware", NULL, true);
	set.add(&other, "repeat_blocker", NULL, true);
	int value = 0;
	int kept = 0;
	HeldValue held;
	int *edited = &value;
	set.deferWith(&hardware, [&held, &edited, &value]() { edited = held.hold(&value); }, [&held]() { held.commit(); });
	set.deferApply(&hardware);
	REQUIRE(edited != &value);
	std::vector<FakeItem *> items(1, &hardware);
	set.afterApply(&hardware, [&value, &kept]() {
		keepOrRestore(value, kept, "remote_control_hardware", fakeAsk, recordApply);
		return true;
	});
	// What the item does on a change: a deferred one only keeps it.
	auto step = [&set, &items](FakeItem *item, const std::string &key) {
		if (!set.holdChange(item))
			settleChange(set, key, item, recordApply, &items, holdsUnlessFailing);
	};
	auto leave = [&set](FakeItem *item) {
		std::string key;
		if (set.takePending(item, key))
			settleChange(set, key, item, recordApply, static_cast<std::vector<FakeItem *> *>(NULL), holdsUnlessFailing);
	};

	for (int v = 1; v <= 3; ++v)
	{
		*edited = v;
		step(&hardware, "remote_control_hardware");
	}
	REQUIRE(g_applied.empty());
	REQUIRE(g_asked == 0);
	REQUIRE(value == 0);

	// Another item of the menu is not held.
	step(&other, "repeat_blocker");
	REQUIRE(g_applied.size() == 1);
	g_applied.clear();

	g_answer = false;
	leave(&hardware);
	REQUIRE(g_asked == 1);
	REQUIRE(value == 0);
	REQUIRE(g_applied.size() == 2);
	REQUIRE(g_applied[0] == "remote_control_hardware");
	REQUIRE(g_applied[1] == "remote_control_hardware");

	// Left again without a change: nothing applied, nothing asked.
	leave(&hardware);
	REQUIRE(g_asked == 1);
	REQUIRE(g_applied.size() == 2);
	leave(&other);
	REQUIRE(g_applied.size() == 2);
}

namespace
{

/* The rc group as it reads the settings: the remote control is sent when it differs
   from the one sent last, and the one before is what the question offers back. */
struct FakeRcGroup
{
	const int *hardware;
	int sent;
	int before;
	int sends;
	explicit FakeRcGroup(const int *h) : hardware(h), sent(*h), before(*h), sends(0) {}
	void run()
	{
		before = sent;
		if (*hardware != sent)
		{
			sent = *hardware;
			++sends;
		}
	}
};

} // namespace

/* A sibling of the same group changed while the remote control waits to be settled:
   the group's run for it must not take up the waiting value, or leaving the menu
   finds it sent already and asks nobody. */
TEST_CASE("a deferred remote control is sent and asked about once after a sibling of its group changed", "[settingactive][settle]")
{
	g_failing.clear();
	g_applied.clear();
	g_asked = 0;
	int hardware = 0;
	FakeRcGroup rc(&hardware);
	auto apply = [&rc](const std::string &key) {
		g_applied.push_back(key);
		rc.run();
	};

	FakeItem chooser, repeat;
	SettingActiveSet<FakeItem> set;
	set.add(&chooser, "remote_control_hardware", NULL, true);
	set.add(&repeat, "repeat_blocker", NULL, true);
	HeldValue held;
	int *edited = &hardware;
	set.deferWith(&chooser, [&held, &edited, &hardware]() { edited = held.hold(&hardware); }, [&held]() { held.commit(); });
	set.deferApply(&chooser);
	set.afterApply(&chooser, [&hardware, &rc, &apply]() {
		int kept = rc.before;
		keepOrRestore(hardware, kept, "remote_control_hardware", fakeAsk, apply);
		return true;
	});
	std::vector<FakeItem *> items;
	items.push_back(&chooser);
	items.push_back(&repeat);

	*edited = 2;
	REQUIRE(set.holdChange(&chooser));
	REQUIRE_FALSE(set.holdChange(&repeat));
	settleChange(set, "repeat_blocker", &repeat, apply, &items, holdsUnlessFailing);
	REQUIRE(rc.sends == 0);
	REQUIRE(hardware == 0);

	g_answer = true;
	std::string key;
	REQUIRE(set.takePending(&chooser, key));
	settleChange(set, key, &chooser, apply, static_cast<std::vector<FakeItem *> *>(NULL), holdsUnlessFailing);
	REQUIRE(rc.sends == 1);
	REQUIRE(g_asked == 1);
	REQUIRE(hardware == 2);
}

/* A write made elsewhere while the item is deferred is what it shows and puts in. */
TEST_CASE("a held value takes a write made elsewhere and puts in what it holds", "[settingactive][settle]")
{
	int setting = 1;
	HeldValue held;
	REQUIRE_FALSE(held.holding());
	held.commit();
	REQUIRE(setting == 1);
	int *edited = held.hold(&setting);
	REQUIRE(held.holding());
	REQUIRE(*edited == 1);
	*edited = 4;
	REQUIRE(setting == 1);
	setting = 7;
	held.follow();
	REQUIRE(*edited == 7);
	*edited = 5;
	held.commit();
	REQUIRE(setting == 5);
}

/* A backup load or a reset from the menu: the replacement applies without asking,
   then each asked setting that moved is asked about with the value it had; a no
   puts that back and applies it, a yes keeps the new one, one that did not move is
   not asked about. */
TEST_CASE("settings replaced from the menu ask about each asked setting that moved", "[settingactive][settle]")
{
	g_applied.clear();
	int mode = 1, remote = 0, steady = 5;
	std::vector<int> asked_with;
	std::vector<AskedSetting> asked;
	AskedSetting a;
	a.value = &mode;
	a.key = "video_Mode";
	a.ask = [&asked_with](int before) { asked_with.push_back(before); return false; };
	asked.push_back(a);
	a.value = &remote;
	a.key = "remote_control_hardware";
	a.ask = [&asked_with](int before) { asked_with.push_back(before); return true; };
	asked.push_back(a);
	a.value = &steady;
	a.key = "steady";
	a.ask = [&asked_with](int before) { asked_with.push_back(before); return false; };
	asked.push_back(a);

	int replaced = 0;
	replaceAsking(asked, [&]() {
		++replaced;
		// What applies the replacement is not the question's apply.
		REQUIRE(g_applied.empty());
		mode = 7;
		remote = 2;
	}, recordApply);

	REQUIRE(replaced == 1);
	REQUIRE(asked_with.size() == 2);
	REQUIRE(asked_with[0] == 1);
	REQUIRE(asked_with[1] == 0);
	REQUIRE(mode == 1);
	REQUIRE(remote == 2);
	REQUIRE(steady == 5);
	REQUIRE(g_applied.size() == 1);
	REQUIRE(g_applied[0] == "video_Mode");
}

namespace
{

// An item that holds a copy of its value, as a row with no int member does.
struct CopyItem
{
	int told;
	bool active;
	int value;
	int reread;
	CopyItem() : told(0), active(true), value(0), reread(0) {}
	void setActive(bool a) { active = a; ++told; }
	// What a press on the box does: one step on from what the item holds.
	void step() { ++value; }
};

std::map<std::string, int> g_store;

} // namespace

TEST_CASE("a write made elsewhere is taken by the items of its key and their menus are judged again", "[settingactive][follow]")
{
	g_failing.clear();
	g_store.clear();
	CopyItem mode, dependent, other;
	int menu_a = 0, menu_b = 0;
	SettingActiveSet<CopyItem> set;
	set.add(&mode, "mode", NULL, true, &menu_a);
	set.add(&dependent, "dependent", NULL, true, &menu_a);
	set.add(&other, "other", NULL, true, &menu_b);
	int painted = 0;
	set.rereadWith(&mode, [&mode, &painted](bool paint) { ++mode.reread; mode.value = g_store["mode"]; painted += paint; },
		       NULL);
	set.rereadWith(&other, [&other](bool) { ++other.reread; other.value = g_store["other"]; }, NULL);
	std::vector<CopyItem *> items_a;
	items_a.push_back(&mode);
	items_a.push_back(&dependent);

	// The web writes mode 2, which is what makes the dependent row's condition fail.
	g_store["mode"] = 2;
	g_failing.insert("dependent");
	std::set<std::string> written;
	written.insert("mode");
	const std::vector<void *> owners = set.follow(written, &menu_a);

	REQUIRE(mode.reread == 1);
	REQUIRE(mode.value == 2);
	REQUIRE(painted == 1);
	REQUIRE(other.reread == 0);
	REQUIRE(owners.size() == 2);
	CHECK((owners[0] == &menu_a || owners[1] == &menu_a));
	CHECK((owners[0] == &menu_b || owners[1] == &menu_b));

	for (size_t i = 0; i < owners.size(); i++)
		if (owners[i] == &menu_a)
			set.reevaluate(items_a, holdsUnlessFailing);
	REQUIRE(dependent.active == false);
	REQUIRE(dependent.told == 1);

	// A press on the box goes on from what was written, not from the old copy.
	mode.step();
	REQUIRE(mode.value == 3);

	// A forgotten item is not asked again.
	set.forget(&mode);
	g_store["mode"] = 5;
	set.follow(written, &menu_a);
	REQUIRE(mode.reread == 1);
}

/* A menu something else covers (a pulldown, a question, a submenu) takes the
   values and states without being drawn into; it is drawn whole once it is on
   top again. The one on top is drawn. */
TEST_CASE("a write made elsewhere draws only into the menu waiting on top", "[settingactive][follow]")
{
	g_failing.clear();
	g_store.clear();
	CopyItem shown, shown_dependent, covered, covered_dependent;
	int top = 0, below = 0;
	SettingActiveSet<CopyItem> set;
	set.add(&shown, "k", NULL, true, &top);
	set.add(&shown_dependent, "topdep", NULL, true, &top);
	set.add(&covered, "k", NULL, true, &below);
	set.add(&covered_dependent, "dep", NULL, true, &below);
	int painted_top = 0, painted_below = 0, quiet = 0;
	set.rereadWith(&shown, [&](bool paint) { shown.value = g_store["k"]; painted_top += paint; }, NULL);
	set.rereadWith(&covered, [&](bool paint) { covered.value = g_store["k"]; painted_below += paint; }, NULL);
	set.rereadWith(&covered_dependent, [](bool) {}, [&](bool a) { covered_dependent.active = a; ++quiet; });
	int quiet_top = 0;
	set.rereadWith(&shown_dependent, [](bool) {}, [&](bool a) { shown_dependent.active = a; ++quiet_top; });
	std::vector<CopyItem *> top_items;
	top_items.push_back(&shown);
	top_items.push_back(&shown_dependent);
	std::vector<CopyItem *> below_items;
	below_items.push_back(&covered);
	below_items.push_back(&covered_dependent);

	g_store["k"] = 4;
	g_failing.insert("dep");
	g_failing.insert("topdep");
	std::vector<std::string> keys(1, "k");
	followWrites(set, keys, &top,
		     [&](void *menu) -> const std::vector<CopyItem *> & { return menu == &top ? top_items : below_items; },
		     holdsUnlessFailing);

	REQUIRE(shown.value == 4);
	REQUIRE(covered.value == 4);
	REQUIRE(painted_top == 1);
	REQUIRE(painted_below == 0);
	// Set quietly and not through setActive, which draws.
	REQUIRE(quiet == 1);
	REQUIRE(covered_dependent.active == false);
	REQUIRE(covered_dependent.told == 0);
	// The menu on top is told, which draws.
	REQUIRE(shown_dependent.told == 1);
	REQUIRE(quiet_top == 0);

	// With no menu waiting, as while a question is open, nothing is drawn.
	g_store["k"] = 6;
	followWrites(set, keys, static_cast<const void *>(NULL),
		     [&](void *menu) -> const std::vector<CopyItem *> & { return menu == &top ? top_items : below_items; },
		     holdsUnlessFailing);
	REQUIRE(shown.value == 6);
	REQUIRE(painted_top == 1);
}

/* A display the screen builds for another setting is no row: it follows writes of the
   keys named for it and is judged by the screen's answer with the rows. */
TEST_CASE("an item built for a display follows the keys named for it and the screen's answer", "[settingactive][follow]")
{
	g_failing.clear();
	g_store.clear();
	CopyItem shown, row;
	int menu = 0;
	SettingActiveSet<CopyItem> set;
	bool allowed = true;
	set.add(&shown, "city", [&allowed]() { return allowed; }, true, &menu);
	set.add(&row, "row", NULL, true, &menu);
	std::vector<std::string> also;
	also.push_back("coords");
	set.rereadOnAlso(&shown, also);
	set.rereadWith(&shown, [&shown](bool) { ++shown.reread; shown.value = g_store["city"]; }, [&shown](bool a) { shown.active = a; });
	std::vector<CopyItem *> items;
	items.push_back(&shown);
	items.push_back(&row);

	// The coordinates are written with the city, or alone from the web.
	g_store["city"] = 7;
	std::vector<std::string> keys(1, "coords");
	followWrites(set, keys, &menu,
		     [&](void *) -> const std::vector<CopyItem *> & { return items; }, holdsUnlessFailing);
	REQUIRE(shown.reread == 1);
	REQUIRE(shown.value == 7);

	keys.assign(1, "unrelated");
	followWrites(set, keys, &menu,
		     [&](void *) -> const std::vector<CopyItem *> & { return items; }, holdsUnlessFailing);
	REQUIRE(shown.reread == 1);

	// The switch that allows it is written elsewhere.
	allowed = false;
	keys.assign(1, "enabled");
	followWrites(set, keys, &menu,
		     [&](void *) -> const std::vector<CopyItem *> & { return items; }, holdsUnlessFailing);
	REQUIRE(shown.active == false);
	REQUIRE(row.told == 0);
}
