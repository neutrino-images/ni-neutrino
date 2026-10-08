/*
 * test_couple.cpp - tests for the settings that are written together, and the reset to defaults
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
#include "coreapi/settings/settings.h"
#include "coreapi/base/deps.h"
#include "support/fakes.h"

using namespace coreapi;
using coreapi::settings::BatchOverlay;

namespace
{

typedef std::vector<std::pair<std::string, Error> > Refusals;

const std::string *valueOf(const BatchOverlay &b, const char *key)
{
	for (size_t i = 0; i < b.values.size(); ++i)
		if (b.values[i].first == key)
			return &b.values[i].second;
	return NULL;
}

size_t countOf(const BatchOverlay &b, const char *key)
{
	size_t n = 0;
	for (size_t i = 0; i < b.values.size(); ++i)
		if (b.values[i].first == key)
			++n;
	return n;
}

bool refusedAs(const Refusals &r, const char *key, ErrorCode code)
{
	for (size_t i = 0; i < r.size(); ++i)
		if (r[i].first == key && r[i].second.code == code)
			return true;
	return false;
}

BatchOverlay batchOf(const char *k1, const char *v1, const char *k2 = NULL, const char *v2 = NULL,
		     const char *k3 = NULL, const char *v3 = NULL)
{
	BatchOverlay b;
	b.values.push_back(std::make_pair(std::string(k1), std::string(v1)));
	if (k2 != NULL)
		b.values.push_back(std::make_pair(std::string(k2), std::string(v2)));
	if (k3 != NULL)
		b.values.push_back(std::make_pair(std::string(k3), std::string(v3)));
	return b;
}

} // anonymous namespace

TEST_CASE("writing the guide's save on puts its read on with it", "[couple]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.ints["epg_read"] = 0;

	BatchOverlay on = batchOf("epg_save", "1");
	Refusals refused;
	settings::settleBatch(on, refused);
	REQUIRE(refused.empty());
	REQUIRE(valueOf(on, "epg_read") != NULL);
	CHECK(*valueOf(on, "epg_read") == "1");

	// Off asks nothing of the read.
	BatchOverlay off = batchOf("epg_save", "0");
	settings::settleBatch(off, refused);
	CHECK(valueOf(off, "epg_read") == NULL);

	// The same value named beside it is no contradiction and is taken as sent.
	refused.clear();
	BatchOverlay same = batchOf("epg_save", "1", "epg_read", "1");
	settings::settleBatch(same, refused);
	CHECK(refused.empty());
	CHECK(same.values.size() == 2);
}

TEST_CASE("a value that contradicts what a coupling implies is refused with the member that implies it", "[couple]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	Refusals refused;

	BatchOverlay both = batchOf("epg_save", "1", "epg_read", "0");
	settings::settleBatch(both, refused);
	CHECK(refusedAs(refused, "epg_read", ErrorCode::SettingConditionNotMet));
	CHECK(refusedAs(refused, "epg_save", ErrorCode::SettingConditionNotMet));
	CHECK(both.values.empty());

	refused.clear();
	BatchOverlay ecm = batchOf("show_ecm_pos", "0", "show_ecm", "1");
	settings::settleBatch(ecm, refused);
	CHECK(refusedAs(refused, "show_ecm", ErrorCode::SettingConditionNotMet));
	CHECK(refusedAs(refused, "show_ecm_pos", ErrorCode::SettingConditionNotMet));
	CHECK(ecm.values.empty());
}

TEST_CASE("the module line follows its position", "[couple]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	Refusals refused;

	BatchOverlay shown = batchOf("show_ecm_pos", "2");
	settings::settleBatch(shown, refused);
	REQUIRE(valueOf(shown, "show_ecm") != NULL);
	CHECK(*valueOf(shown, "show_ecm") == "1");

	BatchOverlay hidden = batchOf("show_ecm_pos", "0", "show_ecm", "0");
	settings::settleBatch(hidden, refused);
	CHECK(refused.empty());
	CHECK(countOf(hidden, "show_ecm") == 1);
	CHECK(*valueOf(hidden, "show_ecm") == "0");

	// The infobar toggles the flag by itself, so a write of the flag alone stays one.
	BatchOverlay alone = batchOf("show_ecm", "1");
	settings::settleBatch(alone, refused);
	CHECK(alone.values.size() == 1);
	CHECK(refused.empty());
}

TEST_CASE("a start channel's name and identifier are written together", "[couple]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.ints["uselastchannel"] = 0;
	Refusals refused;

	BatchOverlay pair = batchOf("startchanneltv", "Das Erste", "startchanneltv_id", "1234abcd");
	settings::settleBatch(pair, refused);
	CHECK(refused.empty());
	CHECK(pair.values.size() == 2);

	BatchOverlay name = batchOf("startchanneltv", "ZDF");
	settings::settleBatch(name, refused);
	CHECK(refusedAs(refused, "startchanneltv", ErrorCode::SettingConditionNotMet));
	CHECK(name.values.empty());

	// The two pairs are separate facts: a radio pair does not make a lone television name whole.
	refused.clear();
	BatchOverlay mixed = batchOf("startchanneltv", "ZDF", "startchannelradio", "SWR", "startchannelradio_id", "55");
	settings::settleBatch(mixed, refused);
	CHECK(refused.size() == 1);
	CHECK(refusedAs(refused, "startchanneltv", ErrorCode::SettingConditionNotMet));
	CHECK(mixed.values.size() == 2);

	refused.clear();
	BatchOverlay id = batchOf("startchannelradio_id", "55");
	settings::settleBatch(id, refused);
	CHECK(refusedAs(refused, "startchannelradio_id", ErrorCode::SettingConditionNotMet));
}

TEST_CASE("a weather place is written with its coordinates and clears the postal code", "[couple]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.ints["weather_enabled"] = 1;
	Refusals refused;

	BatchOverlay pick = batchOf("weather_city", "Hamburg", "weather_location", "53.55,10.00");
	settings::settleBatch(pick, refused);
	CHECK(refused.empty());
	REQUIRE(valueOf(pick, "weather_postalcode") != NULL);
	CHECK(valueOf(pick, "weather_postalcode")->empty());

	// A postal code written empty beside the pick is no contradiction.
	BatchOverlay empty = batchOf("weather_city", "Hamburg", "weather_location", "53.55,10.00",
				     "weather_postalcode", "");
	settings::settleBatch(empty, refused);
	CHECK(refused.empty());
	CHECK(empty.values.size() == 3);

	// One with text contradicts it: refused with the place, and the coordinates are then alone.
	BatchOverlay named = batchOf("weather_city", "Hamburg", "weather_location", "53.55,10.00",
				     "weather_postalcode", "20095");
	settings::settleBatch(named, refused);
	CHECK(refusedAs(refused, "weather_postalcode", ErrorCode::SettingConditionNotMet));
	CHECK(refusedAs(refused, "weather_city", ErrorCode::SettingConditionNotMet));
	CHECK(refusedAs(refused, "weather_location", ErrorCode::SettingConditionNotMet));
	CHECK(named.values.empty());
	refused.clear();

	BatchOverlay city = batchOf("weather_city", "Hamburg");
	settings::settleBatch(city, refused);
	CHECK(refusedAs(refused, "weather_city", ErrorCode::SettingConditionNotMet));
	CHECK(city.values.empty());

	// The postal code alone is a search entry and is not coupled to anything.
	refused.clear();
	BatchOverlay postal = batchOf("weather_postalcode", "20095");
	settings::settleBatch(postal, refused);
	CHECK(refused.empty());
	CHECK(postal.values.size() == 1);
}

TEST_CASE("the five plugin lists stay one partition", "[couple]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.strings["plugins_tool"] = "b,c";
	s.strings["plugins_lua"] = "d";
	Refusals refused;

	// A name written into a list leaves the lists that held it, and only those.
	BatchOverlay moved = batchOf("plugins_game", "a,b");
	settings::settleBatch(moved, refused);
	CHECK(refused.empty());
	REQUIRE(valueOf(moved, "plugins_tool") != NULL);
	CHECK(*valueOf(moved, "plugins_tool") == "c");
	CHECK(valueOf(moved, "plugins_lua") == NULL);
	CHECK(valueOf(moved, "plugins_script") == NULL);
	CHECK(*valueOf(moved, "plugins_game") == "a,b");

	// A list that is emptied by the move is written as empty and not left out.
	BatchOverlay empty = batchOf("plugins_script", "c,b");
	settings::settleBatch(empty, refused);
	REQUIRE(valueOf(empty, "plugins_tool") != NULL);
	CHECK(valueOf(empty, "plugins_tool")->empty());

	// A name twice in one list is in it once.
	BatchOverlay twice = batchOf("plugins_game", "a,a,,b");
	settings::settleBatch(twice, refused);
	CHECK(*valueOf(twice, "plugins_game") == "a,b");

	// Two lists written together naming one plugin have no answer, so neither is taken.
	refused.clear();
	BatchOverlay clash = batchOf("plugins_game", "a,x", "plugins_script", "x,y", "plugins_lua", "z");
	settings::settleBatch(clash, refused);
	CHECK(refusedAs(refused, "plugins_game", ErrorCode::SettingConditionNotMet));
	CHECK(refusedAs(refused, "plugins_script", ErrorCode::SettingConditionNotMet));
	CHECK(refused.size() == 2);
	CHECK(valueOf(clash, "plugins_game") == NULL);
	CHECK(valueOf(clash, "plugins_script") == NULL);
	REQUIRE(valueOf(clash, "plugins_lua") != NULL);
	// The refused lists leave the others alone: nothing was moved on their account.
	CHECK(valueOf(clash, "plugins_tool") == NULL);
}

TEST_CASE("a reset writes the declared defaults and reports what it could not", "[couple]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	FakeSystemSource box;
	InstalledSystemSource installed_box(&box);
	box.caps.board_revision = 0;
	box.caps.display_can_set_brightness = 1;
	s.ints["lcd_dim_brightness"] = 9;
	s.strings["lcd_dim_time"] = "42";
	s.ints["epg_save"] = 0;
	s.ints["epg_read"] = 0;

	std::vector<std::string> keys;
	keys.push_back("lcd_dim_brightness");
	keys.push_back("lcd_dim_time");
	// Not allowed while the guide is neither saved nor read. A row that names a place is
	// no example: its default is refused for the place before the condition is looked at.
	keys.push_back("epg_read_frequently");
	// A credential has no default to give back.
	keys.push_back("personalize_pincode");

	Refusals refused;
	REQUIRE(settings::resetDefaults(keys, refused).ok());

	/* The declared default, which is the one the program loads a fresh box with: the
	   defaults action of the front panel menu used 3 and the row has always said 0. */
	CHECK(s.ints["lcd_dim_brightness"] == 0);
	CHECK(s.strings["lcd_dim_time"] == "0");
	CHECK(s.ints.count("epg_read_frequently") == 0);
	CHECK(s.strings.count("personalize_pincode") == 0);
	CHECK(refused.size() == 2);
	CHECK(refusedAs(refused, "epg_read_frequently", ErrorCode::SettingConditionNotMet));
	CHECK(refusedAs(refused, "personalize_pincode", ErrorCode::EmptyCredential));
	CHECK(s.persisted > 0);

	// A panel that is not wired up has none of the rows, and is told so for each.
	refused.clear();
	box.caps.board_revision = 10;
	s.ints["lcd_dim_brightness"] = 9;
	std::vector<std::string> panel(keys.begin(), keys.begin() + 2);
	REQUIRE(settings::resetDefaults(panel, refused).ok());
	CHECK(s.ints["lcd_dim_brightness"] == 9);
	CHECK(refusedAs(refused, "lcd_dim_brightness", ErrorCode::SettingNotOnThisBox));
	CHECK(refusedAs(refused, "lcd_dim_time", ErrorCode::SettingNotOnThisBox));
}

TEST_CASE("a reset runs the couplings on the values it writes", "[couple]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.ints["show_ecm_pos"] = 2;
	s.ints["show_ecm"] = 1;

	std::vector<std::string> keys(1, "show_ecm_pos");
	Refusals refused;
	REQUIRE(settings::resetDefaults(keys, refused).ok());
	CHECK(refused.empty());
	CHECK(s.ints["show_ecm_pos"] == 0);
	CHECK(s.ints["show_ecm"] == 0);
}

TEST_CASE("a reset of a key nobody declared writes nothing", "[couple]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.ints["show_ecm_pos"] = 2;

	std::vector<std::string> keys;
	keys.push_back("show_ecm_pos");
	keys.push_back("no-such-key-4711");
	Refusals refused;
	Result<void> r = settings::resetDefaults(keys, refused);
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().code == ErrorCode::UnknownSetting);
	CHECK(s.ints["show_ecm_pos"] == 2);
	CHECK(s.persisted == 0);
}

TEST_CASE("a reset of one half of a pair resets both", "[couple]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.ints["uselastchannel"] = 0;
	s.strings["startchanneltv"] = "ZDF";
	s.strings["startchanneltv_id"] = "abcd";
	s.strings["startchannelradio"] = "SWR";

	std::vector<std::string> keys(1, "startchanneltv_id");
	Refusals refused;
	REQUIRE(settings::resetDefaults(keys, refused).ok());
	CHECK(refused.empty());
	CHECK(s.strings["startchanneltv"].empty());
	CHECK(s.strings["startchanneltv_id"] == "0");
	// The other pair is not part of it.
	CHECK(s.strings["startchannelradio"] == "SWR");
}

namespace
{

const Condition kGateOpen[] = { when("fx_gate").is(1) };

/* The module line rows with a gate on one of them, or a bound the implied value breaks,
   so that the cases below can make a coupling's trigger or its addition fail on its own. */
const Descriptor kTriggerGated[] =
{
	intRow("show_ecm_pos").section("fx").range(0, 3).defaultValue(0).changeableWhen(kGateOpen).field(COREAPI_NO_FIELD),
	intRow("show_ecm").section("fx").range(0, 1).defaultValue(0).field(COREAPI_NO_FIELD),
	intRow("fx_gate").section("fx").range(0, 1).defaultValue(0).field(COREAPI_NO_FIELD)
};

const Descriptor kAdditionGated[] =
{
	intRow("show_ecm_pos").section("fx").range(0, 3).defaultValue(0).field(COREAPI_NO_FIELD),
	intRow("show_ecm").section("fx").range(0, 1).defaultValue(0).changeableWhen(kGateOpen).field(COREAPI_NO_FIELD),
	intRow("fx_gate").section("fx").range(0, 1).defaultValue(0).field(COREAPI_NO_FIELD)
};

const Descriptor kAdditionOutOfBounds[] =
{
	intRow("show_ecm_pos").section("fx").range(0, 3).defaultValue(0).field(COREAPI_NO_FIELD),
	intRow("show_ecm").section("fx").range(0, 0).defaultValue(0).field(COREAPI_NO_FIELD),
	intRow("fx_gate").section("fx").range(0, 1).defaultValue(0).field(COREAPI_NO_FIELD)
};

const Descriptor kPluginsGated[] =
{
	textRow("plugins_disabled").section("fx").defaultValue("").field(COREAPI_NO_FIELD),
	textRow("plugins_game").section("fx").defaultValue("").field(COREAPI_NO_FIELD),
	textRow("plugins_tool").section("fx").defaultValue("").changeableWhen(kGateOpen).field(COREAPI_NO_FIELD),
	textRow("plugins_script").section("fx").defaultValue("").field(COREAPI_NO_FIELD),
	textRow("plugins_lua").section("fx").defaultValue("").field(COREAPI_NO_FIELD),
	intRow("fx_gate").section("fx").range(0, 1).defaultValue(0).field(COREAPI_NO_FIELD)
};

} // anonymous namespace

TEST_CASE("a trigger the conditions refuse leaves no addition behind", "[couple]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	InstalledSettingsTable table(kTriggerGated, sizeof(kTriggerGated) / sizeof(kTriggerGated[0]));
	s.ints["fx_gate"] = 0;
	Refusals refused;

	BatchOverlay b = batchOf("show_ecm_pos", "2");
	settings::settleBatch(b, refused);
	CHECK(refusedAs(refused, "show_ecm_pos", ErrorCode::SettingConditionNotMet));
	CHECK(refused.size() == 1);
	CHECK(b.values.empty());

	// With the gate open the pair lands whole.
	refused.clear();
	s.ints["fx_gate"] = 1;
	BatchOverlay open = batchOf("show_ecm_pos", "2");
	settings::settleBatch(open, refused);
	CHECK(refused.empty());
	CHECK(open.values.size() == 2);
}

TEST_CASE("an addition the conditions refuse is reported and refuses what asked for it", "[couple]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	InstalledSettingsTable table(kAdditionGated, sizeof(kAdditionGated) / sizeof(kAdditionGated[0]));
	s.ints["fx_gate"] = 0;
	Refusals refused;

	BatchOverlay b = batchOf("show_ecm_pos", "2");
	settings::settleBatch(b, refused);
	CHECK(refusedAs(refused, "show_ecm", ErrorCode::SettingConditionNotMet));
	CHECK(refusedAs(refused, "show_ecm_pos", ErrorCode::SettingConditionNotMet));
	CHECK(b.values.empty());
}

TEST_CASE("an addition the row cannot take refuses what asked for it with the row's answer", "[couple]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	InstalledSettingsTable table(kAdditionOutOfBounds,
	                             sizeof(kAdditionOutOfBounds) / sizeof(kAdditionOutOfBounds[0]));
	Refusals refused;

	BatchOverlay b = batchOf("show_ecm_pos", "2");
	settings::settleBatch(b, refused);
	CHECK(refusedAs(refused, "show_ecm_pos", ErrorCode::OutOfRange));
	CHECK(refused.size() == 1);
	CHECK(b.values.empty());
}

/* Two lists written, and a third that has to lose a name to each of them but cannot be
   written. Only the list whose name is the one the refused list lost goes down with it. */
TEST_CASE("a plugin move that cannot land refuses the list that asked for it and no other", "[couple]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	InstalledSettingsTable table(kPluginsGated, sizeof(kPluginsGated) / sizeof(kPluginsGated[0]));
	s.ints["fx_gate"] = 0;
	s.strings["plugins_tool"] = "b";
	Refusals refused;

	BatchOverlay b = batchOf("plugins_game", "x", "plugins_lua", "b");
	settings::settleBatch(b, refused);
	CHECK(refusedAs(refused, "plugins_tool", ErrorCode::SettingConditionNotMet));
	CHECK(refusedAs(refused, "plugins_lua", ErrorCode::SettingConditionNotMet));
	CHECK_FALSE(refusedAs(refused, "plugins_game", ErrorCode::SettingConditionNotMet));
	REQUIRE(b.values.size() == 1);
	CHECK(b.values[0].first == "plugins_game");
}
