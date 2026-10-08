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
#include "support/boundedrows.h"

#include "gui/widget/numberstep.h"
#include "gui/widget/settingformat.h"

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

TEST_CASE("an unknown key is refused", "[menuspec]")
{
	REQUIRE(menuItem("no_such_setting").error().code == ErrorCode::UnknownSetting);
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
	{ 0, "options.off", NULL, NULL, NULL, 0 },
	{ 1, NULL, "ext4", NULL, NULL, 0 },
	{ 2, NULL, "xfs", never, NULL, 0 },
};
const EnumValue kNoneOffered[] =
{
	{ 2, NULL, "xfs", never, NULL, 0 },
};
const EnumValue kNoYes[] =
{
	{ 0, "messagebox.no", NULL, NULL, NULL, 0 },
	{ 1, "messagebox.yes", NULL, NULL, NULL, 0 },
};
const EnumValue kOffBelow[] =
{
	{ 0, "options.off", NULL, NULL, NULL, 0 },
};
const EnumValue kTwoWords[] =
{
	{ -1, "options.auto", NULL, NULL, NULL, 0 },
	{ 0, "options.off", NULL, NULL, NULL, 0 },
};
// What the box answers for the rows below that ask it.
bool g_box_has = false;
bool boxHas() { return g_box_has; }
const Shape kFlag = shape(ValueType::Bool, "flag_label").range(0, 1);
const Shape kFlagHinted = shape(ValueType::Bool, "flag_label").range(0, 1).hint("shape_hint");
const Descriptor kRows[] =
{
	{
		"t_choice", ValueType::Enum, "fixture", "label", NULL,
		0, 0, kEntries, sizeof(kEntries) / sizeof(kEntries[0]), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker),
		NULL, NULL, NULL, NULL, NULL, NULL, false, NULL
	},
	{
		"t_none", ValueType::Enum, "fixture", "label", NULL,
		0, 0, kNoneOffered, 1, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker),
		NULL, NULL, NULL, NULL, NULL, NULL, false, NULL
	},
	{
		"t_bool", ValueType::Bool, "fixture", "label", NULL,
		0, 0, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker),
		NULL, NULL, NULL, NULL, NULL, NULL, false, NULL
	},
	{
		"t_noyes", ValueType::Bool, "fixture", "label", NULL,
		0, 0, kNoYes, 2, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker),
		NULL, NULL, NULL, NULL, NULL, NULL, false, NULL
	},
	{
		"t_named", ValueType::Int, "fixture", "label", NULL,
		1, 14, kOffBelow, 1, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker),
		NULL, NULL, NULL, NULL, NULL, NULL, false, NULL
	},
	{
		"t_words", ValueType::Int, "fixture", "label", NULL,
		1, 14, kTwoWords, 2, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker),
		NULL, NULL, NULL, NULL, NULL, NULL, false, NULL
	},
	{
		"t_fan", ValueType::Int, "fixture", "label", NULL,
		1, 14, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(repeat_blocker, boxHas, NULL),
		NULL, NULL, NULL, NULL, NULL, NULL, false, NULL
	},
	{
		"t_hinted", ValueType::Int, "fixture", "label", "row_hint",
		0, 999, kOffBelow, 1, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(repeat_blocker, boxHas, &kFlagHinted),
		NULL, NULL, NULL, NULL, NULL, NULL, false, NULL
	},
	{
		"t_scroll", ValueType::Int, "fixture", "label", NULL,
		0, 999, kOffBelow, 1, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(repeat_blocker, boxHas, &kFlag),
		NULL, NULL, NULL, NULL, NULL, NULL, false, NULL
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

TEST_CASE("a text row carries its rule and is read and written whole", "[menuspec]")
{
	Result<MenuItemSpec> r = menuItem("network_nfs_recordingdir");
	REQUIRE(r.ok());
	REQUIRE(r.value().text != NULL);
	CHECK(r.value().text->kind == TextKind::Directory);
	CHECK(r.value().text->must_exist == MustExist::YesNotTmpfs);
	CHECK_FALSE(r.value().secret);

	SNeutrinoSettings s;
	REQUIRE(menuTextWrite(r.value(), s, "/usr"));
	std::string back;
	REQUIRE(menuTextRead(r.value(), s, back));
	CHECK(back == "/usr");
}

TEST_CASE("a text write answers false for a text the row cannot take and writes nothing", "[menuspec]")
{
	SNeutrinoSettings s;

	Result<MenuItemSpec> id = menuItem("startchanneltv_id");
	REQUIRE(id.ok());
	REQUIRE(menuTextWrite(id.value(), s, "1a2b"));
	CHECK_FALSE(menuTextWrite(id.value(), s, "xyz"));
	CHECK_FALSE(menuTextWrite(id.value(), s, ""));
	CHECK_FALSE(menuTextWrite(id.value(), s, "12345678901234567"));
	std::string back;
	REQUIRE(menuTextRead(id.value(), s, back));
	CHECK(back == "1a2b");
	CHECK(menuTextWrite(id.value(), s, "ABCDEF"));
	REQUIRE(menuTextRead(id.value(), s, back));
	CHECK(back == "abcdef");

#if ENABLE_QUADPIP
	// An element of an array has no origin of its own to say it is an identifier.
	MenuItemSpec element;
	element.field.origin = FieldOrigin::Element;
	element.field.read_text = &ElementChannelId<decltype(SNeutrinoSettings::quadpip_channel_id_window),
						    &SNeutrinoSettings::quadpip_channel_id_window, 1>::read;
	element.field.write_text = &ElementChannelId<decltype(SNeutrinoSettings::quadpip_channel_id_window),
						     &SNeutrinoSettings::quadpip_channel_id_window, 1>::write;
	element.field.extra = &ElementChannelId<decltype(SNeutrinoSettings::quadpip_channel_id_window),
						&SNeutrinoSettings::quadpip_channel_id_window, 1>::extra;
	REQUIRE(menuTextWrite(element, s, "77"));
	CHECK_FALSE(menuTextWrite(element, s, "not hex"));
	REQUIRE(menuTextRead(element, s, back));
	CHECK(back == "77");
#endif

	// A path that breaks its row's rule: the directory the row names does not exist.
	Result<MenuItemSpec> dir = menuItem("network_nfs_recordingdir");
	REQUIRE(dir.ok());
	REQUIRE(menuTextWrite(dir.value(), s, "/usr"));
	CHECK_FALSE(menuTextWrite(dir.value(), s, "/no/such/place/for/a/recording"));
	CHECK_FALSE(menuTextWrite(dir.value(), s, ""));
	REQUIRE(menuTextRead(dir.value(), s, back));
	CHECK(back == "/usr");
}

TEST_CASE("a text the field already holds passes the rule again, whatever the place is now", "[menuspec]")
{
	SNeutrinoSettings s;
	Result<MenuItemSpec> dir = menuItem("network_nfs_recordingdir");
	REQUIRE(dir.ok());

	// A disk that is not mounted this time: the stored value was good when it was chosen.
	dir.value().field.write_text(s, "/no/such/stored/place");
	CHECK(menuTextWrite(dir.value(), s, "/no/such/stored/place"));
	CHECK_FALSE(menuTextWrite(dir.value(), s, "/no/such/other/place"));
	std::string back;
	REQUIRE(menuTextRead(dir.value(), s, back));
	CHECK(back == "/no/such/stored/place");
}

TEST_CASE("a pin item carries its four digit rule and is a credential", "[menuspec]")
{
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	Result<MenuItemSpec> r = menuItem("parentallock_pincode");
	REQUIRE(r.ok());
	REQUIRE(r.value().text != NULL);
	CHECK(r.value().text->kind == TextKind::Pin);
	CHECK(r.value().text->min_length == 4);
	CHECK(r.value().text->max_length == 4);
	CHECK(r.value().secret);
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

TEST_CASE("a number's unit reaches the menu item as a name", "[menuspec][units]")
{
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);
	Result<MenuItemSpec> hours = menuItem("record_hours");
	REQUIRE(hours.ok());
	CHECK(hours.value().unit_key == "unit.short.hour");

	// Seconds, and nought shown as off in words.
	Result<MenuItemSpec> after = menuItem("timeshift_auto");
	REQUIRE(after.ok());
	CHECK(after.value().unit_key == "unit.short.second");
	REQUIRE(after.value().choices.size() == 1);
	CHECK(after.value().choices[0].value == 0);
	CHECK(after.value().choices[0].label_key == "options.off");

	Result<MenuItemSpec> bare = menuItem("start_volume");
	REQUIRE(bare.ok());
	CHECK(bare.value().unit_key.empty());

	// A choice has no number to put a unit after.
	Result<MenuItemSpec> choice = menuItem("timeshift_pause");
	REQUIRE(choice.ok());
	CHECK(choice.value().unit_key.empty());
}

TEST_CASE("the number format a row's texts make is the one the screens wrote", "[menuspec][units]")
{
	CHECK(settingNumberFormat("") == "%d");
	CHECK(settingNumberFormat("h") == "%d h");
	CHECK(settingNumberFormat("min") == "%d min");
	CHECK(settingNumberFormat("MB") == "%d MB");
	// Against the number, and doubled for printf.
	CHECK(settingNumberFormat("%") == "%d%%");
	// A letter outside ASCII is a letter, not a sign.
	CHECK(settingNumberFormat("\xc3\xa9") == "%d \xc3\xa9");
}

namespace
{
std::string fakeText(const std::string &key)
{
	if (key == "unit.short.hour")
		return "h";
	if (key == "unit.short.percent")
		return "%";
	if (key == "unit.short.second")
		return "s";
	return "";
}
} // namespace

TEST_CASE("a row's number is printed with its own unit and no other", "[menuspec][units]")
{
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);

	CHECK(settingNumberFormat(menuItem("record_hours").value(), fakeText) == "%d h");
	CHECK(settingNumberFormat(menuItem("recording_fill_warning").value(), fakeText) == "%d%%");
	CHECK(settingNumberFormat(menuItem("timeshift_auto").value(), fakeText) == "%d s");
	// A row that names none leaves the chooser as it is.
	CHECK(settingNumberFormat(menuItem("start_volume").value(), fakeText).empty());
}

TEST_CASE("a number that names several values offers every one of them", "[menuspec][units]")
{
	InstalledSettingsTable table(kRows, sizeof(kRows) / sizeof(kRows[0]));
	Result<MenuItemSpec> r = menuItem("t_words");
	REQUIRE(r.ok());
	REQUIRE(r.value().choices.size() == 2);
	CHECK(r.value().choices[0].value == -1);
	CHECK(r.value().choices[0].label_key == "options.auto");
	CHECK(r.value().choices[1].value == 0);
	CHECK(r.value().choices[1].label_key == "options.off");
	// The one-word row keeps its single entry.
	CHECK(menuItem("t_named").value().choices.size() == 1);
}

/* A percent sign attaches to its number as the screens wrote it, and these
   are the five rows that wrote it so. A change to the attach rule that spaced
   them would show on each, so each is named. */
TEST_CASE("every row that prints a percent keeps it against the number", "[menuspec][units]")
{
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);

	const char *const rows[] = {
		"audio_volume_percent_ac3", "audio_volume_percent_pcm", "recording_fill_warning",
		"font_scaling_x", "font_scaling_y"
	};
	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
	{
		INFO(rows[i]);
		Result<MenuItemSpec> r = menuItem(rows[i]);
		REQUIRE(r.ok());
		CHECK(r.value().unit_key == "unit.short.percent");
		CHECK(settingNumberFormat(r.value(), fakeText) == "%d%%");
	}
}

namespace
{
const Descriptor kProvidedRows[] =
{
	{
		"t_provided_text", ValueType::String, "fixture", "label", NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(language),
		NULL, NULL, providedChoices, NULL, NULL, NULL, false, NULL
	},
	{
		"t_provided_number", ValueType::Int, "fixture", "label", NULL,
		0, 100, kOffBelow, 1, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker),
		NULL, NULL, providedChoices, NULL, NULL, NULL, false, NULL
	},
};
} // anonymous namespace

TEST_CASE("a row with a provider becomes a menu item with the provider's entries", "[menuspec][provided]")
{
	InstalledSettingsTable table(kProvidedRows, 2);
	ProvidedChoices box;
	box.text("de", "options.off");
	box.text("fr", "", "Francais");
	box.text("it");

	Result<MenuItemSpec> r = menuItem("t_provided_text");
	REQUIRE(r.ok());
	REQUIRE(r.value().type == ValueType::String);
	REQUIRE(r.value().choices.size() == 3);
	CHECK(r.value().choices[0].text == "de");
	CHECK(r.value().choices[0].label_key == "options.off");
	CHECK(r.value().choices[1].text == "fr");
	CHECK(r.value().choices[1].label_text == "Francais");
	CHECK(r.value().choices[1].label_key.empty());
	// An entry with no words of its own is shown as its text.
	CHECK(r.value().choices[2].label_text == "it");

	ProvidedChoices numbers;
	numbers.number(3, "three");
	Result<MenuItemSpec> n = menuItem("t_provided_number");
	REQUIRE(n.ok());
	// The provider's list replaces the row's own named values.
	REQUIRE(n.value().choices.size() == 1);
	CHECK(n.value().choices[0].value == 3);
	CHECK(n.value().choices[0].label_text == "three");
	CHECK(n.value().choices[0].text.empty());
}

TEST_CASE("a number whose provider lists its values is offered as that list, and a bare number where it cannot say", "[menuspec][provided]")
{
	InstalledSettingsTable table(kProvidedRows, 2);
	ProvidedChoices numbers;
	numbers.number(-1, "Off");
	numbers.number(0, "1: tuner");

	Result<MenuItemSpec> n = menuItem("t_provided_number");
	REQUIRE(n.ok());
	CHECK(offeredAsList(n.value()));

	providedCanSay() = false;
	Result<MenuItemSpec> silent = menuItem("t_provided_number");
	REQUIRE(silent.ok());
	CHECK_FALSE(offeredAsList(silent.value()));
}

TEST_CASE("a number that names a value in words is a number and a choice is a list", "[menuspec]")
{
	Result<MenuItemSpec> named = menuItem("start_volume");
	REQUIRE(named.ok());
	REQUIRE_FALSE(named.value().choices.empty());
	CHECK_FALSE(offeredAsList(named.value()));

	Result<MenuItemSpec> choice = menuItem("audio_AnalogMode");
	REQUIRE(choice.ok());
	CHECK(offeredAsList(choice.value()));
}

TEST_CASE("a row whose provider cannot say is still a menu item, without entries", "[menuspec][provided]")
{
	InstalledSettingsTable table(kProvidedRows, 2);
	ProvidedChoices box;
	providedCanSay() = false;

	Result<MenuItemSpec> r = menuItem("t_provided_text");
	REQUIRE(r.ok());
	CHECK(r.value().choices.empty());
	Result<MenuItemSpec> n = menuItem("t_provided_number");
	REQUIRE(n.ok());
	CHECK(n.value().choices.empty());
}

TEST_CASE("the menu writes of a row with a provider hold to what is offered or already held", "[menuspec][provided]")
{
	InstalledSettingsTable table(kProvidedRows, 2);
	ProvidedChoices box;
	box.text("de");
	box.number(3, "three");
	SNeutrinoSettings s;
	std::string back;
	long number = 0;

	Result<MenuItemSpec> text = menuItem("t_provided_text");
	REQUIRE(text.ok());
	REQUIRE(menuTextWrite(text.value(), s, "de"));
	CHECK_FALSE(menuTextWrite(text.value(), s, "es"));
	REQUIRE(menuTextRead(text.value(), s, back));
	CHECK(back == "de");
	// What is held passes again after the entry has gone, and nothing else does.
	providedList().clear();
	box.text("fr");
	box.number(3, "three");
	CHECK(menuTextWrite(text.value(), s, "de"));
	CHECK_FALSE(menuTextWrite(text.value(), s, "es"));
	CHECK(menuTextWrite(text.value(), s, "fr"));

	Result<MenuItemSpec> num = menuItem("t_provided_number");
	REQUIRE(num.ok());
	REQUIRE(menuValueWrite(num.value(), s, 3));
	CHECK_FALSE(menuValueWrite(num.value(), s, 4));
	REQUIRE(menuValueRead(num.value(), s, number));
	CHECK(number == 3);
	providedList().clear();
	box.number(5, "five");
	CHECK(menuValueWrite(num.value(), s, 3));
	CHECK_FALSE(menuValueWrite(num.value(), s, 4));

	// A provider that cannot say holds the write to nothing.
	providedCanSay() = false;
	CHECK(menuTextWrite(text.value(), s, "es"));
	CHECK(menuValueWrite(num.value(), s, 4));
}

TEST_CASE("the menu writes of a row with a provider take its default and, on an empty list, anything", "[menuspec][provided]")
{
	InstalledSettingsTable table(kProvidedRows, 2);
	ProvidedChoices box;
	box.text("de");
	box.number(3, "three");
	SNeutrinoSettings s;

	Result<MenuItemSpec> text = menuItem("t_provided_text");
	REQUIRE(text.ok());
	CHECK(text.value().default_text.empty());
	// The default is no entry of the list and is still the way back.
	CHECK(menuTextWrite(text.value(), s, "de"));
	CHECK(menuTextWrite(text.value(), s, ""));
	Result<MenuItemSpec> num = menuItem("t_provided_number");
	REQUIRE(num.ok());
	CHECK(menuValueWrite(num.value(), s, 3));
	CHECK(menuValueWrite(num.value(), s, 0));
	CHECK_FALSE(menuValueWrite(num.value(), s, 4));

	// A list of nothing offers nothing to hold the row to.
	providedList().clear();
	CHECK(menuTextWrite(text.value(), s, "es"));
	CHECK(menuValueWrite(num.value(), s, 4));
}

TEST_CASE("a number's menu item carries the range the box states now", "[menuspec][bounds]")
{
	InstalledSettingsTable table(kBoundedRows, 1);
	BoundedProvider box;
	boundedLow() = 20;
	boundedHigh() = 200;

	Result<MenuItemSpec> r = menuItem("t_bounded");
	REQUIRE(r.ok());
	CHECK(r.value().min == 20);
	CHECK(r.value().max == 200);

	// Outside the constants the envelope wins.
	boundedHigh() = 5000;
	boundedLow() = -4;
	Result<MenuItemSpec> wide = menuItem("t_bounded");
	REQUIRE(wide.ok());
	CHECK(wide.value().min == 0);
	CHECK(wide.value().max == 1000);
}

TEST_CASE("the menu write of a bounded number holds to the range and passes what is held", "[menuspec][bounds]")
{
	InstalledSettingsTable table(kBoundedRows, 1);
	BoundedProvider box;
	boundedLow() = 20;
	boundedHigh() = 200;
	SNeutrinoSettings s;

	Result<MenuItemSpec> r = menuItem("t_bounded");
	REQUIRE(r.ok());
	CHECK(menuValueWrite(r.value(), s, 200));
	CHECK_FALSE(menuValueWrite(r.value(), s, 201));
	CHECK_FALSE(menuValueWrite(r.value(), s, 19));
	long back = 0;
	REQUIRE(menuValueRead(r.value(), s, back));
	CHECK(back == 200);

	// A stored value outside is left as it is and writing it again passes; the default is the way back.
	s.repeat_blocker = 700;
	REQUIRE(menuValueRead(r.value(), s, back));
	CHECK(back == 700);
	CHECK(menuValueWrite(r.value(), s, 700));
	CHECK_FALSE(menuValueWrite(r.value(), s, 701));
	CHECK(menuValueWrite(r.value(), s, 10));
}

TEST_CASE("a number that names values outside its range carries them with its range", "[menuspec][named]")
{
	static const EnumValue names[] =
	{
		{ 0, "options.off", NULL, NULL, NULL, 0 },
		{ 90, "options.on", NULL, NULL, NULL, 0 },
	};
	static const Descriptor rows[] =
	{
		{
			"t_timeout", ValueType::Int, "fixture", "label", NULL,
			5, 60, names, 2, 0, NULL, false, false, COREAPI_ALWAYS,
			COREAPI_NUMBER_FIELD(repeat_blocker),
			NULL, NULL, NULL, NULL, NULL, NULL, false, NULL
		},
	};
	InstalledSettingsTable table(rows, 1);

	Result<MenuItemSpec> r = menuItem("t_timeout");
	REQUIRE(r.ok());
	CHECK(r.value().min == 5);
	CHECK(r.value().max == 60);
	REQUIRE(r.value().choices.size() == 2);
	CHECK(r.value().choices[0].value == 0);
	CHECK(r.value().choices[0].label_key == "options.off");
	CHECK(r.value().choices[1].value == 90);

	// The menu writes what the web takes: a name outside the range stays one.
	SNeutrinoSettings s;
	CHECK(menuValueWrite(r.value(), s, 0));
	CHECK(menuValueWrite(r.value(), s, 90));
}

TEST_CASE("a number chooser steps through the names outside its range and wraps at both ends", "[menuspec][named]")
{
	std::vector<int> named;
	named.push_back(0);
	named.push_back(90);

	// Floor 5, ceiling 60, off below and a word above.
	CHECK(numberStep(5, 60, named, 5, false) == 0);
	CHECK(numberStep(5, 60, named, 0, true) == 5);
	CHECK(numberStep(5, 60, named, 60, true) == 90);
	CHECK(numberStep(5, 60, named, 90, false) == 60);
	CHECK(numberStep(5, 60, named, 90, true) == 0);
	CHECK(numberStep(5, 60, named, 0, false) == 90);
	CHECK(numberStep(5, 60, named, 30, true) == 31);
	CHECK(numberStep(5, 60, named, 30, false) == 29);

	// A stored value that is out of range and unnamed goes to the nearest element on its way.
	CHECK(numberStep(5, 60, named, 3, true) == 5);
	CHECK(numberStep(5, 60, named, 3, false) == 0);
	CHECK(numberStep(5, 60, named, 70, true) == 90);
	CHECK(numberStep(5, 60, named, 70, false) == 60);
	CHECK(numberStep(5, 60, named, 100, true) == 0);
	CHECK(numberStep(5, 60, named, 100, false) == 90);

	// With no names the range alone wraps.
	std::vector<int> none;
	CHECK(numberStep(5, 60, none, 60, true) == 5);
	CHECK(numberStep(5, 60, none, 5, false) == 60);
	CHECK(numberStep(5, 60, none, 3, true) == 5);
	CHECK(numberStep(5, 60, none, 70, false) == 60);

	// The order stays one ascending list: the names below, the range, the names above.
	const std::vector<int> order = numberStepOrder(5, 60, named);
	REQUIRE(order.size() == 58);
	CHECK(order.front() == 0);
	CHECK(order[1] == 5);
	CHECK(order[order.size() - 2] == 60);
	CHECK(order.back() == 90);
}

/* The menus word these rows by the short word of their group, as origin/master builds them
   (screensetup.cpp, osd_setup.cpp, videosettings.cpp and vfd_setup.cpp); the label that
   tells them apart is the schema's. The menu item must keep the old key. */
TEST_CASE("a row relabelled for the schema keeps its old text in the menu", "[menuspec][label]")
{
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);
	static const struct { const char *row; const char *menu; } kKept[] =
	{
		{ "window_width", "window_size" },
		{ "window_height", "window_size" },
		{ "screen_StartX_a_0", "screensetup.upperleft" },
		{ "screen_StartY_a_0", "screensetup.upperleft" },
		{ "screen_EndX_a_0", "screensetup.lowerright" },
		{ "screen_EndY_a_0", "screensetup.lowerright" },
		{ "screen_StartX_a_1", "screensetup.upperleft" },
		{ "screen_StartY_a_1", "screensetup.upperleft" },
		{ "screen_EndX_a_1", "screensetup.lowerright" },
		{ "screen_EndY_a_1", "screensetup.lowerright" },
		{ "screen_StartX_b_0", "screensetup.upperleft" },
		{ "screen_StartY_b_0", "screensetup.upperleft" },
		{ "screen_EndX_b_0", "screensetup.lowerright" },
		{ "screen_EndY_b_0", "screensetup.lowerright" },
		{ "screen_StartX_b_1", "screensetup.upperleft" },
		{ "screen_StartY_b_1", "screensetup.upperleft" },
		{ "screen_EndX_b_1", "screensetup.lowerright" },
		{ "screen_EndY_b_1", "screensetup.lowerright" },
		{ "pip_x", "videomenu.pip" },
		{ "pip_y", "videomenu.pip" },
		{ "pip_width", "videomenu.pip" },
		{ "pip_height", "videomenu.pip" },
		{ "pip_radio_x", "videomenu.pip" },
		{ "pip_radio_y", "videomenu.pip" },
		{ "pip_radio_width", "videomenu.pip" },
		{ "pip_radio_height", "videomenu.pip" },
		{ "pip_rotate_lastpos", "videomenu.pip" },
		{ "backlight_standby", "ledcontroler.mode.standby" },
		{ "backlight_deepstandby", "ledcontroler.mode.deepstandby" },
		{ "theme.menu_Head", "colormenu.background" },
		{ "theme.menu_Head_Text", "colormenu.textcolor" },
		{ "theme.menu_Content", "colormenu.background" },
		{ "theme.menu_Content_Text", "colormenu.textcolor" },
		{ "theme.menu_Content_Selected", "colormenu.background" },
		{ "theme.menu_Content_Selected_Text", "colormenu.textcolor" },
		{ "theme.menu_Content_inactive", "colormenu.background" },
		{ "theme.menu_Content_inactive_Text", "colormenu.textcolor" },
		{ "theme.menu_Foot", "colormenu.background" },
		{ "theme.menu_Foot_Text", "colormenu.textcolor" },
		{ "theme.infobar", "colormenu.background" },
		{ "theme.infobar_Text", "colormenu.textcolor" },
		{ "theme.infobar_casystem", "miscsettings.infobar_casystem_display" },
		{ "theme.colored_events", "colormenu.textcolor" },
		{ "menu_Head_gradient", "color.gradient" },
		{ "menu_Head_gradient_direction", "color.gradient_mode_direction" },
		{ "menu_SubHead_gradient", "color.gradient" },
		{ "menu_SubHead_gradient_direction", "color.gradient_mode_direction" },
		{ "menu_Hint_gradient", "color.gradient" },
		{ "menu_Hint_gradient_direction", "color.gradient_mode_direction" },
		{ "infobar_gradient_top_direction", "color.gradient_mode_direction" },
		{ "infobar_gradient_body_direction", "color.gradient_mode_direction" },
		{ "infobar_gradient_bottom_direction", "color.gradient_mode_direction" }
	};
	size_t seen = 0;
	for (size_t i = 0; i < sizeof(kKept) / sizeof(kKept[0]); ++i)
	{
		const Descriptor *d = settings::findRow(kKept[i].row);
		if (d == NULL)
			continue;
		INFO(kKept[i].row);
		CHECK(std::string(d->label_key) != kKept[i].menu);
		Result<MenuItemSpec> r = menuItem(kKept[i].row);
		if (!r.ok())
			continue;
		CHECK(r.value().label_key == kKept[i].menu);
		++seen;
	}
	CHECK(seen > 30);
}
