/*
 * test_keyscolors.cpp - tests for the key and colour settings
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

#include "support/catch.hpp"

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/schema.h"
#include "coreapi/settings/menuspec.h"
#include "coreapi/settings/settings.h"
#include "coreapi/settings/settingstable.h"
#include "support/fakes.h"
#include <system/settings.h>

#include <stdint.h>
#include <stdio.h>

#include <linux/input.h>

#include <string>
#include <vector>

#include <driver/rcinput.h>

/* The key and the colour kinds, held to what their rows promise: the text a colour
   is written in survives a round trip through the steps the screens move it by, a
   write is refused unless it is the row's own width, and a key setting takes the
   codes the input layer names and no other number between its bounds. */

using namespace coreapi;

namespace
{

bool fixtureSaved = false;
bool fixtureSave() { fixtureSaved = true; return true; }

struct RealStore
{
	SNeutrinoSettings values;
	FakeCommandSink   sink;
	InstalledSink     installed;

	RealStore() : values(SNeutrinoSettings()), installed(&sink)
	{
		fixtureSaved = false;
		installRealSettingsSource(&values, fixtureSave);
	}

	~RealStore() { installRealSettingsSource(NULL, NULL); }
};

struct InstalledKeys
{
	explicit InstalledKeys(KeySource *s) { setKeySource(s); }
	~InstalledKeys() { setKeySource(0); }
};

// One key and no key: what a write can be told apart by, with nothing the input layer says.
struct FakeKeys : public KeySource
{
	long only;
	mutable int asked;

	explicit FakeKeys(long k) : only(k), asked(0) {}
	bool known(long code) const
	{
		++asked;
		return code == only || code == (int32_t) CRCInput::RC_nokey;
	}
	std::string name(long) const { return std::string(); }
	std::vector<KeyName> all() const { return std::vector<KeyName>(); }
};

// The first code a remote control can send that has no name.
long unnamedCode()
{
	for (long c = 1; c <= (long) KEY_MAX; ++c)
		if (keySource().name(c).empty())
			return c;
	return 0;
}

std::string decimal0(long v)
{
	char out[32];
	snprintf(out, sizeof(out), "%ld", v);
	return std::string(out);
}

long zero() { return 0; }

void readTextStub(const SNeutrinoSettings &, std::string &) {}
void writeTextStub(SNeutrinoSettings &, const std::string &) {}

const FieldRef kText = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, FieldOrigin::Nowhere, NULL, NULL, NULL };

Descriptor colorRowOf(long channels, const char *def)
{
	Descriptor d = colorRow("k").section("s").defaultValue(def).field(kText);
	d.min = channels;
	d.max = channels;
	return d;
}

} // namespace

TEST_CASE("a colour channel survives a read and a write of itself at every step", "[keyscolors]")
{
	for (unsigned step = 0; step <= kColorSteps; ++step)
	{
		unsigned char in[4] = { (unsigned char) step, (unsigned char) (kColorSteps - step), (unsigned char) step, (unsigned char) step };
		const std::string text = colorText(in, 4);
		REQUIRE(text.size() == 9);

		unsigned char out[4] = { 0, 0, 0, 0 };
		REQUIRE(readColorText(text, 4, out));
		for (size_t i = 0; i < 4; ++i)
			CHECK((unsigned) out[i] == (unsigned) in[i]);
	}
}

TEST_CASE("a colour text is exactly the row's width, hexadecimal, behind a number sign", "[keyscolors]")
{
	unsigned char out[4] = { 7, 7, 7, 7 };
	CHECK(readColorText("#ff8000", 3, out));
	CHECK((unsigned) out[0] == 100);
	CHECK((unsigned) out[1] == 50);
	CHECK((unsigned) out[2] == 0);
	// Both cases, one spelling back.
	CHECK(readColorText("#FF8000cc", 4, out));
	CHECK(colorText(out, 4) == "#ff8000cc");

	unsigned char kept[4] = { 7, 7, 7, 7 };
	CHECK_FALSE(readColorText("#ff8000", 4, kept));
	CHECK_FALSE(readColorText("#ff8000cc", 3, kept));
	CHECK_FALSE(readColorText("ff8000", 3, kept));
	// Of the right length, and still no colour without the sign.
	CHECK_FALSE(readColorText("0ff8000", 3, kept));
	CHECK_FALSE(readColorText("#ff80g0", 3, kept));
	CHECK_FALSE(readColorText("#ff80 0", 3, kept));
	CHECK_FALSE(readColorText("", 3, kept));
	CHECK_FALSE(readColorText("#ff8000", 5, kept));
	// A text refused changes nothing of what it was to fill.
	for (size_t i = 0; i < 4; ++i)
		CHECK((unsigned) kept[i] == 7);
}

TEST_CASE("a byte past the top step is the top step", "[keyscolors]")
{
	// What a theme file can hold that the screens never offered.
	const unsigned char over[3] = { 200, 100, 101 };
	CHECK(colorText(over, 3) == "#ffffff");
}

TEST_CASE("a colour row is sane at three or four channels with a default of the same width", "[keyscolors]")
{
	REQUIRE(descriptorIsSane(colorRowOf(3, "#102030")));
	REQUIRE(descriptorIsSane(colorRowOf(4, "#10203040")));

	REQUIRE_FALSE(descriptorIsSane(colorRowOf(3, "#10203040")));
	REQUIRE_FALSE(descriptorIsSane(colorRowOf(4, "#102030")));
	REQUIRE_FALSE(descriptorIsSane(colorRowOf(3, "102030")));
	REQUIRE_FALSE(descriptorIsSane(colorRowOf(5, "#1020304050")));
	REQUIRE_FALSE(descriptorIsSane(colorRowOf(2, "#1020")));

	Descriptor split = colorRowOf(3, "#102030");
	split.max = 4;
	REQUIRE_FALSE(descriptorIsSane(split));

	Descriptor nodefault = colorRowOf(3, "#102030");
	nodefault.default_string = NULL;
	REQUIRE_FALSE(descriptorIsSane(nodefault));

	// A colour's default is text, so a function giving a number has nothing to give.
	Descriptor fn = colorRowOf(3, "#102030");
	fn.default_fn = zero;
	REQUIRE_FALSE(descriptorIsSane(fn));
}

TEST_CASE("a colour row is held to the origin that carries channels", "[keyscolors]")
{
	const FieldRef text = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, FieldOrigin::Nowhere, NULL, NULL, NULL };
	Descriptor d = colorRowOf(3, "#102030");
	d.field = text;
	REQUIRE(descriptorIsSane(d));

	// An origin of colour bytes without a colour behind it is no field.
	d.field.origin = FieldOrigin::ColorBytes;
	REQUIRE_FALSE(descriptorIsSane(d));

	// A colour that is reached by text is reached as channels, and nothing else is.
	const FieldRef reached = { NULL, NULL, NULL, NULL, readTextStub, writeTextStub, NULL, NULL,
				   "theme.k", FieldOrigin::ColorBytes, NULL, NULL, NULL };
	Descriptor c = colorRowOf(3, "#102030");
	c.field = reached;
	REQUIRE(descriptorIsSane(c));
	c.field.origin = FieldOrigin::Member;
	REQUIRE_FALSE(descriptorIsSane(c));

	Descriptor number = intRow("k").section("s").range(0, 9).defaultValue(3).field(kText);
	number.field = reached;
	REQUIRE_FALSE(descriptorIsSane(number));
}

TEST_CASE("a key row is sane between bounds that hold its default and names no words", "[keyscolors]")
{
	Descriptor d = keyRow("k").section("s").range(-2, 100).defaultValue(5).field(kText);
	REQUIRE(descriptorIsSane(d));

	d.default_int = 101;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.default_int = -3;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.default_int = 5;

	d.min = 101;
	d.max = 100;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.min = -2;

	const EnumValue words[] = { option(-2).label("a") };
	d.values = words;
	d.value_count = 1;
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("the kinds still waiting for their first row are refused", "[keyscolors]")
{
	Descriptor d = intRow("k").section("s").range(0, 9).defaultValue(3).field(kText);
	REQUIRE(descriptorIsSane(d));
	d.type = ValueType::List;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.type = ValueType::Records;
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("the key source takes every code a remote can send and names the keys it knows", "[keyscolors]")
{
	installRealKeySource();
	struct Restore { ~Restore() { setKeySource(0); } } restore;

	const KeySource &src = keySource();
	const long none = (int32_t) CRCInput::RC_nokey;
	CHECK(src.known(none));
	CHECK(src.known((long) CRCInput::RC_ok));
	CHECK(src.known((long) CRCInput::RC_ok | (long) CRCInput::RC_Repeat));
	CHECK(src.name((long) CRCInput::RC_ok) == "ok");
	CHECK(src.name((long) CRCInput::RC_ok | (long) CRCInput::RC_Repeat) == "ok (long)");
	CHECK(src.name(none) == "none");

	// Neither a release nor a code past the table is a key.
	CHECK_FALSE(src.known((long) CRCInput::RC_ok | (long) CRCInput::RC_Release));
	CHECK_FALSE(src.known((long) CRCInput::RC_MaxRC + 1));
	CHECK_FALSE(src.known((long) KEY_MAX + 1));
	CHECK_FALSE(src.known((long) (KEY_MAX + 1) | (long) CRCInput::RC_Repeat));
	CHECK_FALSE(src.known(0));
	CHECK_FALSE(src.known(-1));
	CHECK(src.known((long) KEY_MAX));
	// A code with no name is still a key the chooser can store, plain or held.
	const long unnamed = unnamedCode();
	REQUIRE(unnamed != 0);
	CHECK(src.known(unnamed));
	CHECK(src.known(unnamed | (long) CRCInput::RC_Repeat));
	CHECK(src.name(unnamed).empty());
	CHECK(src.name(unnamed | (long) CRCInput::RC_Repeat).empty());

	const std::vector<KeyName> all = src.all();
	REQUIRE(all.size() > 100);
	CHECK(all[0].code == none);
	size_t held = 0;
	for (size_t i = 0; i < all.size(); ++i)
	{
		CHECK(src.known(all[i].code));
		CHECK_FALSE(all[i].name.empty());
		if (all[i].code > 0 && (all[i].code & (long) CRCInput::RC_Repeat) != 0)
			++held;
		for (size_t j = 0; j < i; ++j)
			REQUIRE(all[j].code != all[i].code);
	}
	// Every named key once plain and once held.
	CHECK(held * 2 == all.size() - 1);
}

TEST_CASE("every key row starts on a key the input layer can deliver", "[keyscolors]")
{
	installRealKeySource();
	struct Restore { ~Restore() { setKeySource(0); } } restore;

	const Descriptor *t = settingsTable();
	size_t keys = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		if (t[i].type != ValueType::Key)
			continue;
		++keys;
		INFO(t[i].key);
		CHECK(keySource().known(t[i].default_int));
		CHECK(keySource().known(defaultInt(t[i])));
		CHECK(t[i].field.int_pointer != NULL);
	}
	// The rows the keybinding screen offers a key chooser for; the six picture in
	// picture keys are there only where that is built.
#if ENABLE_PIP
	CHECK(keys == 56);
#else
	CHECK(keys == 50);
#endif
}

TEST_CASE("a key setting takes any code a remote can send and refuses the rest between its bounds", "[keyscolors]")
{
	RealStore store;
	installRealKeySource();
	struct Restore { ~Restore() { setKeySource(0); } } restore;

	REQUIRE(settings::set("key_power_off", decimal0((long) CRCInput::RC_home)).ok());
	applyPendingSettings();
	CHECK(store.values.key_power_off == (int) CRCInput::RC_home);

	// Held, and no key at all, are keys.
	REQUIRE(settings::set("key_power_off", decimal0((long) CRCInput::RC_ok | (long) CRCInput::RC_Repeat)).ok());
	REQUIRE(settings::set("key_power_off", decimal0((int32_t) CRCInput::RC_nokey)).ok());
	applyPendingSettings();
	CHECK(store.values.key_power_off == (int) (int32_t) CRCInput::RC_nokey);

	// A key with no name is stored as it is: the chooser stores it, and a write of the
	// value a file already holds must not be refused.
	const long unnamed = unnamedCode();
	REQUIRE(unnamed != 0);
	REQUIRE(settings::set("key_power_off", decimal0(unnamed)).ok());
	REQUIRE(settings::set("key_power_off", decimal0(unnamed | (long) CRCInput::RC_Repeat)).ok());
	applyPendingSettings();
	CHECK(store.values.key_power_off == (int) (unnamed | (long) CRCInput::RC_Repeat));

	Result<void> r = settings::check("key_power_off", decimal0((long) CRCInput::RC_ok | (long) CRCInput::RC_Release));
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().code == ErrorCode::NotAListedValue);

	r = settings::check("key_power_off", decimal0((long) KEY_MAX + 1));
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().code == ErrorCode::NotAListedValue);

	r = settings::check("key_power_off", "0");
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().code == ErrorCode::NotAListedValue);

	r = settings::set("key_power_off", decimal0((long) CRCInput::RC_MaxRC + 1));
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().code == ErrorCode::OutOfRange);

	r = settings::set("key_power_off", "ok");
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().code == ErrorCode::NotANumber);

	// A refused write left the value as it was.
	applyPendingSettings();
	CHECK(store.values.key_power_off == (int) (unnamed | (long) CRCInput::RC_Repeat));
}

TEST_CASE("a key is checked against what the key source says and not against a table of its own", "[keyscolors]")
{
	RealStore store;
	FakeKeys keys((long) CRCInput::RC_blue);
	InstalledKeys installed(&keys);

	// The one key the source takes, though a table of the input layer would take every named one.
	CHECK(settings::set("key_power_off", decimal0((long) CRCInput::RC_blue)).ok());
	Result<void> r = settings::set("key_power_off", decimal0((long) CRCInput::RC_ok));
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().code == ErrorCode::NotAListedValue);
	CHECK(keys.asked > 0);
}

TEST_CASE("with no key source installed a key is held to the bounds of its row", "[keyscolors]")
{
	RealStore store;
	setKeySource(0);
	// Nothing to ask is no reason to refuse: a layer that cannot say takes the row's word.
	CHECK(settings::set("key_power_off", decimal0((long) CRCInput::RC_blue)).ok());
	CHECK_FALSE(settings::set("key_power_off", decimal0((long) CRCInput::RC_MaxRC + 1)).ok());
}

TEST_CASE("a colour setting is read and written as the text of its channels", "[keyscolors]")
{
	RealStore store;
	store.values.theme.menu_Head_red = 100;
	store.values.theme.menu_Head_green = 50;
	store.values.theme.menu_Head_blue = 0;
	store.values.theme.menu_Head_alpha = 80;

	Result<std::string> got = settings::get("theme.menu_Head");
	REQUIRE(got.ok());
	CHECK(got.value() == "#ff8000cc");

	CHECK_FALSE(settings::set("theme.menu_Head", "#00FF40").ok());
	REQUIRE(settings::set("theme.menu_Head", "#00FF400F").ok());
	// Taken as one spelling the moment it is written, before the loop has carried it.
	got = settings::get("theme.menu_Head");
	REQUIRE(got.ok());
	CHECK(got.value() == "#00ff400f");

	applyPendingSettings();
	CHECK((int) store.values.theme.menu_Head_red == 0);
	CHECK((int) store.values.theme.menu_Head_green == 100);
	CHECK((int) store.values.theme.menu_Head_blue == 25);
	CHECK((int) store.values.theme.menu_Head_alpha == 6);

	// A byte that is no step is written as the nearest step, and reads back as that.
	REQUIRE(settings::set("theme.menu_Head", "#00ff4010").ok());
	got = settings::get("theme.menu_Head");
	REQUIRE(got.ok());
	CHECK(got.value() == "#00ff400f");
	CHECK(fixtureSaved);
}

TEST_CASE("a colour of three channels leaves the alpha beside it alone", "[keyscolors]")
{
	RealStore store;
	store.values.theme.menu_Head_Text_alpha = 42;

	REQUIRE(settings::set("theme.menu_Head_Text", "#102030").ok());
	applyPendingSettings();
	CHECK((int) store.values.theme.menu_Head_Text_alpha == 42);
	CHECK((int) store.values.theme.menu_Head_Text_red == 6);

	// And it reads three channels, whatever the struct carries beside them.
	Result<std::string> got = settings::get("theme.menu_Head_Text");
	REQUIRE(got.ok());
	CHECK(got.value().size() == 7);

	Result<void> r = settings::set("theme.menu_Head_Text", "#10203040");
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().code == ErrorCode::BadString);
}

TEST_CASE("a colour that is not the row's width or not hexadecimal is refused whole", "[keyscolors]")
{
	RealStore store;
	store.values.theme.menu_Head_red = 11;

	const char *const bad[] = { "", "#", "ff8000cc", "#ff8000", "#ff8000cc00", "#ff80zzcc", "#ff8000c ", "red", "#ff8000c\n" };
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		INFO(bad[i]);
		Result<void> r = settings::set("theme.menu_Head", bad[i]);
		REQUIRE_FALSE(r.ok());
		CHECK(r.error().code == ErrorCode::BadString);
	}

	// Nothing reached the struct from a request that was refused.
	applyPendingSettings();
	CHECK((int) store.values.theme.menu_Head_red == 11);
}

TEST_CASE("a colour row's default is the colour its channels start on", "[keyscolors]")
{
	const Descriptor *row = settings::findRow("theme.menu_Head");
	REQUIRE(row != NULL);
	CHECK(row->type == ValueType::Color);
	CHECK(colorChannels(*row) == 4);
	CHECK(std::string(row->default_string) == "#0000001a");

	const Descriptor *text = settings::findRow("theme.menu_Content_Text");
	REQUIRE(text != NULL);
	CHECK(colorChannels(*text) == 3);
	CHECK(std::string(text->default_string) == "#fa" "fa" "fa");

	// Every colour row of the table, three channels or four, with the field that carries them.
	size_t colors = 0;
	const Descriptor *t = settingsTable();
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		if (t[i].type != ValueType::Color)
			continue;
		++colors;
		INFO(t[i].key);
		CHECK(t[i].field.origin == FieldOrigin::ColorBytes);
		CHECK(descriptorIsSane(t[i]));
		CHECK(std::string(t[i].key).find("theme.") == 0);
	}
	// Nineteen groups of the theme; the three of the panel exist only where the panel does.
	CHECK(colors == 19);
}

TEST_CASE("a menu item for a key or a colour row says what a widget needs", "[keyscolors]")
{
	Result<MenuItemSpec> key = menuItem("key_power_off");
	REQUIRE(key.ok());
	CHECK(key.value().type == ValueType::Key);
	CHECK(key.value().int_pointer != NULL);
	CHECK(key.value().min == (int32_t) CRCInput::RC_nokey);
	CHECK(key.value().max == (long) CRCInput::RC_MaxRC);

	Result<MenuItemSpec> color = menuItem("theme.menu_Head");
	REQUIRE(color.ok());
	CHECK(color.value().type == ValueType::Color);
	CHECK(color.value().channels == 4);
	CHECK(color.value().int_pointer == NULL);

	SNeutrinoSettings s;
	s.theme.menu_Head_red = 100;
	s.theme.menu_Head_green = 0;
	s.theme.menu_Head_blue = 0;
	s.theme.menu_Head_alpha = 0;
	std::string text;
	REQUIRE(menuTextRead(color.value(), s, text));
	CHECK(text == "#ff000000");

	REQUIRE(menuTextWrite(color.value(), s, "#0000ffff"));
	CHECK((int) s.theme.menu_Head_blue == 100);
	CHECK((int) s.theme.menu_Head_alpha == 100);

	// A text that is no colour of the row's width is refused where the widget can be told.
	CHECK_FALSE(menuTextWrite(color.value(), s, "#0000ff"));
	CHECK((int) s.theme.menu_Head_red == 0);
}
