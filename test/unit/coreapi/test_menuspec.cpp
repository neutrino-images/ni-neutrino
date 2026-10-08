/*
 * test_menuspec.cpp - tests for a declared setting as a menu item
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

#include "coreapi/base/errors.h"
#include "coreapi/settings/menuspec.h"
#include "coreapi/settings/settings.h"
#include "coreapi/settings/settingsfield.h"
#include "coreapi/settings/settingstable.h"
#include "support/fakes.h"

#include <timerdclient/timerdtypes.h>

#include <cstring>
#include <vector>

using namespace coreapi;

TEST_CASE("a declared choice becomes a menu item with keys, text and its member", "[menuspec]")
{
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);
	Result<MenuItemSpec> r = menuItem("parentallock_lockage");
	REQUIRE(r.ok());
	REQUIRE(r.value().label_key == "parentallock.lockage");
	REQUIRE(r.value().hint_key == "menu.hint_parentallock_lockage");
	REQUIRE(r.value().choices.size() == 3);
	REQUIRE(r.value().choices[0].value == 12);
	REQUIRE(r.value().choices[0].label_key == "parentallock.lockage12");
	SNeutrinoSettings s;
	REQUIRE(r.value().int_pointer(s) == &s.parentallock_lockage);
}

TEST_CASE("a number becomes a menu item with its bounds", "[menuspec]")
{
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);
	Result<MenuItemSpec> r = menuItem("parentallock_zaptime");
	REQUIRE(r.ok());
	REQUIRE(r.value().min == 0);
	REQUIRE(r.value().max == 10000);
	REQUIRE(r.value().hint_key.empty());
	REQUIRE(r.value().choices.empty());
}

TEST_CASE("the prompt offers the three the screen shows", "[menuspec]")
{
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);
	Result<MenuItemSpec> r = menuItem("parentallock_prompt");
	REQUIRE(r.ok());
	REQUIRE(r.value().choices.size() == 3);
	REQUIRE(r.value().choices[1].value == 2);
}

TEST_CASE("an unknown key and a member no widget can edit are refused", "[menuspec]")
{
	REQUIRE(menuItem("no_such_setting").error().code == ErrorCode::UnknownSetting);
	REQUIRE(menuItem("parentallock_pincode").error().code == ErrorCode::BadTable);
}

TEST_CASE("a row the parental lock holds is a locked item only while the box is locked", "[menuspec]")
{
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);

	REQUIRE_FALSE(menuItem("parentallock_lockage").value().locked);
	box.parental_locked = true;
	REQUIRE(menuItem("parentallock_lockage").value().locked);
	// The box answering nothing locks too.
	box.parental_locked = false;
	box.parental_status = Status::Internal;
	REQUIRE(menuItem("parentallock_lockage").value().locked);
}

namespace
{
bool never() { return false; }

const EnumValue kEntries[] =
{
	{ 0, "options.off", NULL, NULL },
	{ 1, NULL, "ext4", NULL },
	{ 2, NULL, "xfs", never },
};
const EnumValue kNoneOffered[] =
{
	{ 2, NULL, "xfs", never },
};
const EnumValue kNoYes[] =
{
	{ 0, "messagebox.no", NULL, NULL },
	{ 1, "messagebox.yes", NULL, NULL },
};
const EnumValue kOffBelow[] =
{
	{ 0, "options.off", NULL, NULL },
};
// What the box answers for the rows below that ask it.
bool g_box_has = false;
bool boxHas() { return g_box_has; }
const Shape kFlag = { ValueType::Bool, "flag_label", 0, 1, NULL, 0, NULL };
const Shape kFlagHinted = { ValueType::Bool, "flag_label", 0, 1, NULL, 0, "shape_hint" };
const Descriptor kRows[] =
{
	{
		"t_choice", ValueType::Enum, "fixture", "label", NULL,
		0, 0, kEntries, sizeof(kEntries) / sizeof(kEntries[0]), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker)
	},
	{
		"t_none", ValueType::Enum, "fixture", "label", NULL,
		0, 0, kNoneOffered, 1, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker)
	},
	{
		"t_bool", ValueType::Bool, "fixture", "label", NULL,
		0, 0, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker)
	},
	{
		"t_noyes", ValueType::Bool, "fixture", "label", NULL,
		0, 0, kNoYes, 2, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker)
	},
	{
		"t_named", ValueType::Int, "fixture", "label", NULL,
		1, 14, kOffBelow, 1, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker)
	},
	{
		"t_fan", ValueType::Int, "fixture", "label", NULL,
		1, 14, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(repeat_blocker, boxHas, NULL)
	},
	{
		"t_hinted", ValueType::Int, "fixture", "label", "row_hint",
		0, 999, kOffBelow, 1, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(repeat_blocker, boxHas, &kFlagHinted)
	},
	{
		"t_scroll", ValueType::Int, "fixture", "label", NULL,
		0, 999, kOffBelow, 1, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(repeat_blocker, boxHas, &kFlag)
	},
};
} // anonymous namespace

TEST_CASE("a setting the box lacks is no item and one in two shapes takes the shape the box offers", "[menuspec]")
{
	InstalledSettingsTable table(kRows, sizeof(kRows) / sizeof(kRows[0]));

	g_box_has = true;
	REQUIRE(menuItem("t_fan").ok());
	Result<MenuItemSpec> counted = menuItem("t_scroll");
	REQUIRE(counted.ok());
	REQUIRE(counted.value().type == ValueType::Int);
	REQUIRE(counted.value().label_key == "label");
	REQUIRE(counted.value().max == 999);
	REQUIRE(counted.value().choices.size() == 1);

	g_box_has = false;
	Result<MenuItemSpec> fan = menuItem("t_fan");
	REQUIRE_FALSE(fan.ok());
	REQUIRE(fan.error().code == ErrorCode::SettingNotOnThisBox);
	Result<MenuItemSpec> flag = menuItem("t_scroll");
	REQUIRE(flag.ok());
	REQUIRE(flag.value().type == ValueType::Bool);
	REQUIRE(flag.value().label_key == "flag_label");
	REQUIRE(flag.value().choices.size() == 2);
	REQUIRE(flag.value().choices[1].label_key == "options.on");
	// The rest of the row is the same in both shapes.
	SNeutrinoSettings s;
	REQUIRE(flag.value().int_pointer(s) == &s.repeat_blocker);
}

TEST_CASE("a shape that names a hint takes it and one that names none keeps the row's", "[menuspec]")
{
	InstalledSettingsTable table(kRows, sizeof(kRows) / sizeof(kRows[0]));

	g_box_has = true;
	REQUIRE(menuItem("t_hinted").ok());
	CHECK(menuItem("t_hinted").value().hint_key == "row_hint");

	g_box_has = false;
	Result<MenuItemSpec> other = menuItem("t_hinted");
	REQUIRE(other.ok());
	CHECK(other.value().hint_key == "shape_hint");
	CHECK(other.value().label_key == "flag_label");
}

TEST_CASE("an entry keeps its key or its text and one the box lacks is left out", "[menuspec]")
{
	InstalledSettingsTable table(kRows, sizeof(kRows) / sizeof(kRows[0]));
	Result<MenuItemSpec> r = menuItem("t_choice");
	REQUIRE(r.ok());
	REQUIRE(r.value().choices.size() == 2);
	REQUIRE(r.value().choices[0].label_key == "options.off");
	REQUIRE(r.value().choices[0].label_text.empty());
	REQUIRE(r.value().choices[1].label_key.empty());
	REQUIRE(r.value().choices[1].label_text == "ext4");
	REQUIRE(r.value().label_key == "label");
}

TEST_CASE("an enum with nothing offered is refused", "[menuspec]")
{
	InstalledSettingsTable table(kRows, sizeof(kRows) / sizeof(kRows[0]));
	REQUIRE(menuItem("t_none").error().code == ErrorCode::ChoicesUnavailable);
}

TEST_CASE("a bool offers off and on", "[menuspec]")
{
	InstalledSettingsTable table(kRows, sizeof(kRows) / sizeof(kRows[0]));
	Result<MenuItemSpec> r = menuItem("t_bool");
	REQUIRE(r.ok());
	REQUIRE(r.value().choices.size() == 2);
	REQUIRE(r.value().choices[0].value == 0);
	REQUIRE(r.value().choices[0].label_key == "options.off");
	REQUIRE(r.value().choices[1].value == 1);
	REQUIRE(r.value().choices[1].label_key == "options.on");
}

TEST_CASE("a bool that names its own words offers those", "[menuspec]")
{
	InstalledSettingsTable table(kRows, sizeof(kRows) / sizeof(kRows[0]));
	Result<MenuItemSpec> r = menuItem("t_noyes");
	REQUIRE(r.ok());
	REQUIRE(r.value().type == ValueType::Bool);
	REQUIRE(r.value().choices.size() == 2);
	REQUIRE(r.value().choices[0].value == 0);
	REQUIRE(r.value().choices[0].label_key == "messagebox.no");
	REQUIRE(r.value().choices[1].value == 1);
	REQUIRE(r.value().choices[1].label_key == "messagebox.yes");
}

TEST_CASE("a number names the value it shows in words", "[menuspec]")
{
	InstalledSettingsTable table(kRows, sizeof(kRows) / sizeof(kRows[0]));
	Result<MenuItemSpec> r = menuItem("t_named");
	REQUIRE(r.ok());
	REQUIRE(r.value().type == ValueType::Int);
	REQUIRE(r.value().min == 1);
	REQUIRE(r.value().max == 14);
	REQUIRE(r.value().choices.size() == 1);
	REQUIRE(r.value().choices[0].value == 0);
	REQUIRE(r.value().choices[0].label_key == "options.off");
	REQUIRE(r.value().choices[0].label_text.empty());
}

/* Rows with no int member: the frontend keeps the int and goes through the
   spec, which reads and writes the row the way the row itself does. */
TEST_CASE("a bit of a mask is an item that writes only its own bit", "[menuspec]")
{
	Result<MenuItemSpec> r = menuItem("recording_audio_pids_alt");
	REQUIRE(r.ok());
	const MenuItemSpec &spec = r.value();
	REQUIRE(spec.int_pointer == NULL);
	REQUIRE(spec.type == ValueType::Bool);
	REQUIRE(spec.choices.size() == 2);

	SNeutrinoSettings s;
	s.recording_audio_pids_default = TIMERD_APIDS_STD | TIMERD_APIDS_AC3;
	long v = -1;
	REQUIRE(menuValueRead(spec, s, v));
	CHECK(v == 0);

	REQUIRE(menuValueWrite(spec, s, 1));
	CHECK((int) s.recording_audio_pids_default == (TIMERD_APIDS_STD | TIMERD_APIDS_ALT | TIMERD_APIDS_AC3));
	REQUIRE(menuValueRead(spec, s, v));
	CHECK(v == 1);

	REQUIRE(menuValueWrite(spec, s, 0));
	CHECK((int) s.recording_audio_pids_default == (TIMERD_APIDS_STD | TIMERD_APIDS_AC3));

	// Two is no bit value and leaves the mask alone.
	REQUIRE_FALSE(menuValueWrite(spec, s, 2));
	CHECK((int) s.recording_audio_pids_default == (TIMERD_APIDS_STD | TIMERD_APIDS_AC3));
}

TEST_CASE("a value a daemon holds is an item that asks it and tells it once", "[menuspec]")
{
	FakeRecordingSafety safety;
	InstalledRecordingSafety installed(&safety);
	safety.before = 900;
	safety.after = 1800;

	Result<MenuItemSpec> r = menuItem("record_safety_time_before");
	REQUIRE(r.ok());
	const MenuItemSpec &spec = r.value();
	REQUIRE(spec.int_pointer == NULL);
	REQUIRE(spec.min == 0);
	REQUIRE(spec.max == 99);

	// The member beside it says something else and is not what is read.
	SNeutrinoSettings s;
	s.record_safety_time_before = 7;
	long v = -1;
	REQUIRE(menuValueRead(spec, s, v));
	CHECK(v == 15);

	const unsigned writes = safety.writes;
	REQUIRE(menuValueWrite(spec, s, 3));
	CHECK(safety.writes == writes + 1);
	CHECK(safety.before == 180);
	CHECK(safety.after == 1800);
	CHECK(s.record_safety_time_before == 7);
}

TEST_CASE("a daemon that cannot be asked answers no value", "[menuspec]")
{
	FakeRecordingSafety safety;
	InstalledRecordingSafety installed(&safety);
	safety.before = 900;

	Result<MenuItemSpec> r = menuItem("record_safety_time_before");
	REQUIRE(r.ok());
	SNeutrinoSettings s;
	long v = -1;
	safety.fail_next = true;
	REQUIRE_FALSE(menuValueRead(r.value(), s, v));
	CHECK(v == -1);
}

TEST_CASE("a text row is still refused", "[menuspec]")
{
	REQUIRE(menuItem("network_nfs_recordingdir").error().code == ErrorCode::BadTable);
}

namespace
{
std::vector<long> epgScanModes()
{
	std::vector<long> v;
	Result<MenuItemSpec> r = menuItem("epg_scan_mode");
	REQUIRE(r.ok());
	for (size_t i = 0; i < r.value().choices.size(); i++)
		v.push_back(r.value().choices[i].value);
	return v;
}
}

TEST_CASE("the live scan modes are offered only with several tuners enabled", "[menuspec]")
{
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);

	tuner.enabled = 1;
	const long one[] = { 0, 2 };
	CHECK(epgScanModes() == std::vector<long>(one, one + 2));

	tuner.enabled = 2;
	const long two[] = { 0, 2, 1, 3 };
	CHECK(epgScanModes() == std::vector<long>(two, two + 4));
}

namespace
{
struct BoxRow
{
	const char *key;
	int BoxCapabilities::*capability;
};
} // anonymous namespace

/* The shipped rows a box may lack: each is left out exactly where its own
   capability is missing, whatever the others say. */
TEST_CASE("each shipped row behind a capability is offered only where the box has it", "[menuspec]")
{
	const BoxRow rows[] =
	{
		{ "fan_speed", &BoxCapabilities::has_fan },
		{ "shutdown_block_while_recording", &BoxCapabilities::can_shutdown },
		{ "cpufreq", &BoxCapabilities::can_cpufreq },
		{ "standby_cpufreq", &BoxCapabilities::can_cpufreq },
		{ "hdmi_cec_mode", &BoxCapabilities::can_cec },
		{ "hdmi_cec_view_on", &BoxCapabilities::can_cec },
		{ "hdmi_cec_standby", &BoxCapabilities::can_cec },
		{ "hdmi_cec_volume", &BoxCapabilities::can_cec },
		{ "lcd_dim_brightness", &BoxCapabilities::display_can_set_brightness },
		{ "lcd_dim_time", &BoxCapabilities::display_can_set_brightness },
		{ "hdmi_dd", &BoxCapabilities::has_HDMI },
		{ "key_format_mode_active", &BoxCapabilities::has_button_vformat },
	};
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
	{
		INFO(rows[i].key);
		memset(&box.caps, 0xff, sizeof(box.caps));
		box.caps.*(rows[i].capability) = 0;
		Result<MenuItemSpec> lacking = menuItem(rows[i].key);
		REQUIRE_FALSE(lacking.ok());
		CHECK(lacking.error().code == ErrorCode::SettingNotOnThisBox);

		// Text is no item a widget edits, so a box with the capability only
		// gets past the test of the box.
		box.caps.*(rows[i].capability) = 1;
		Result<MenuItemSpec> having = menuItem(rows[i].key);
		if (having.ok())
			continue;
		CHECK(having.error().code == ErrorCode::BadTable);
		CHECK(settings::findRow(rows[i].key)->type == ValueType::String);
	}
}

/* The energy menu is opened from the power menu on a box that cannot switch
   itself off, and shows its shutdown choices there; only the setting that needs
   deep standby is left out. */
TEST_CASE("the shutdown choices of the energy menu are offered on a box that cannot shut down", "[menuspec]")
{
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);
	memset(&box.caps, 0xff, sizeof(box.caps));
	box.caps.can_shutdown = 0;

	const char *const offered[] = { "shutdown_real", "shutdown_real_rcdelay", "shutdown_count" };
	for (size_t i = 0; i < sizeof(offered) / sizeof(offered[0]); ++i)
	{
		INFO(offered[i]);
		Result<MenuItemSpec> item = menuItem(offered[i]);
		CHECK(item.ok());
	}
	Result<MenuItemSpec> block = menuItem("shutdown_block_while_recording");
	REQUIRE_FALSE(block.ok());
	CHECK(block.error().code == ErrorCode::SettingNotOnThisBox);
}
