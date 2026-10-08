/*
 * test_settings.cpp - tests for reading and writing settings
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
#include "coreapi/base/apply.h"
#include "coreapi/base/deps.h"
#include "coreapi/base/eventbus.h"
#include "coreapi/settings/settingsfield.h"
#include "coreapi/settings/settingstable.h"
#include "support/fakes.h"
#include "support/counts.h"

#include <pthread.h>
#include <cstring>
#include <unistd.h>
#include <sys/statfs.h>

#include <neutrinoMessages.h>
#include <hardware/video.h>

/* The object the program saves its settings through, compiled in rather than
   linked: the archive it lives in is not one this binary links, and what a text
   value may hold is decided by what this reads back. A copy of the format
   written here would be a copy checking itself. */
#include <configfile.cpp>

using namespace coreapi;

/* The one audio setting the program reads nowhere but in the pass that loads
   its settings, so nothing running can be told about a change to it. */
static const char *kRestartOnlyAudioKey = "start_volume";

TEST_CASE("the schema is not empty and every entry is sane", "[settings]")
{
	// A row may ask the box whether it has what the row controls.
	FakeSystemSource box;
	InstalledSystemSource installed_box(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);
	Result<std::vector<Descriptor> > r = settings::schema();
	REQUIRE(r.ok());
	REQUIRE(r.value().size() > 0);
	for (size_t i = 0; i < r.value().size(); ++i)
		REQUIRE(descriptorIsSane(r.value()[i]));
}

TEST_CASE("a key nobody declared is NotFound and not an empty descriptor", "[settings]")
{
	Result<Descriptor> r = settings::describe("no-such-key-4711");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
	REQUIRE(r.error().code == ErrorCode::UnknownSetting);
}

TEST_CASE("reading a value asks the source and answers what it holds", "[settings]")
{
	FakeSettingsSource f;
	f.ints["audio_AnalogMode"] = 1;
	setSettingsSource(&f);

	Result<std::string> r = settings::get("audio_AnalogMode");
	REQUIRE(r.ok());
	REQUIRE(r.value() == "1");
	setSettingsSource(NULL);
}

TEST_CASE("a value the store has never held reads as the declared default", "[settings]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);

	Result<Descriptor> d = settings::describe("audio_AnalogMode");
	REQUIRE(d.ok());

	Result<std::string> r = settings::get("audio_AnalogMode");
	REQUIRE(r.ok());
	// Rendered the way get() renders it, so the case pins the rendering too.
	char want[32];
	snprintf(want, sizeof(want), "%ld", d.value().default_int);
	REQUIRE(r.value() == std::string(want));
	setSettingsSource(NULL);
}

/* The one shipped row whose number depends on the box and differs from its
   constant on a build with no exceptions: the numeric panel makes the play time
   on by default. A read that took the constant would say off on that box. */
TEST_CASE("a value the store has never held reads as the default of this box", "[settings]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	box.caps.display_xres = 8;

	box.caps.display_type = HW_DISPLAY_LED_NUM;
	Result<std::string> numeric = settings::get("movieplayer_display_playtime");
	REQUIRE(numeric.ok());
	CHECK(numeric.value() == "1");

	box.caps.display_type = HW_DISPLAY_LINE_TEXT;
	Result<std::string> text = settings::get("movieplayer_display_playtime");
	REQUIRE(text.ok());
	CHECK(text.value() == "0");
	setSettingsSource(NULL);
}

namespace
{
long threeOnThisBox() { return 3; }

const Condition kBasisIsThree[] =
{
	{ "fixture_basis", CompareOp::Eq, 3, NULL, 0, NULL, NULL, 0 }
};

/* A row whose number is the box's and not its constant, and a row that is
   settable only while the first holds three. */
const Descriptor kBoxDefaultThenCondition[] =
{
	{
		"fixture_basis", ValueType::Int, "fixture", "label", NULL,
		0, 9, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NO_FIELD,
		NULL, NULL, NULL, NULL, NULL, NULL, threeOnThisBox
	},
	{
		"fixture_dependent", ValueType::Bool, "fixture", "label", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kBasisIsThree),
		COREAPI_NO_FIELD,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
};
} // anonymous namespace

/* The condition reads a number nobody stored, which is what the box falls back
   to and not what the row's constant says. */
TEST_CASE("a condition on a setting nobody stored reads the default of this box", "[settings]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);
	InstalledSettingsTable table(kBoxDefaultThenCondition,
	                             sizeof(kBoxDefaultThenCondition) / sizeof(kBoxDefaultThenCondition[0]));

	Result<void> r = settings::set("fixture_dependent", "1");
	CHECK(r.ok());
	setSettingsSource(NULL);
}

TEST_CASE("sections are distinct and every declared key names one of them", "[settings]")
{
	// A row may ask the box whether it has what the row controls.
	FakeSystemSource row_box;
	InstalledSystemSource installed_row_box(&row_box);
	Result<std::vector<std::string> > s = settings::sections();
	REQUIRE(s.ok());
	REQUIRE(s.value().size() > 0);

	for (size_t i = 0; i < s.value().size(); ++i)
		for (size_t j = i + 1; j < s.value().size(); ++j)
			REQUIRE(s.value()[i] != s.value()[j]);

	Result<std::vector<Descriptor> > sch = settings::schema();
	REQUIRE(sch.ok());
	REQUIRE(sch.value().size() > 0);
	for (size_t i = 0; i < sch.value().size(); ++i)
	{
		bool found = false;
		for (size_t j = 0; j < s.value().size() && !found; ++j)
			found = (s.value()[j] == sch.value()[i].section);
		REQUIRE(found);
	}
}

/* The other direction of the case above, which on its own passes for a list
   that names a section no row is in. */
TEST_CASE("every section named is one some row is in", "[settings]")
{
	// A row may ask the box whether it has what the row controls.
	FakeSystemSource row_box;
	InstalledSystemSource installed_row_box(&row_box);
	Result<std::vector<std::string> > s = settings::sections();
	REQUIRE(s.ok());
	Result<std::vector<Descriptor> > sch = settings::schema();
	REQUIRE(sch.ok());

	size_t checked = 0;
	for (size_t i = 0; i < s.value().size(); ++i)
	{
		bool used = false;
		for (size_t j = 0; j < sch.value().size() && !used; ++j)
			used = (s.value()[i] == sch.value()[j].section);
		++checked;
		INFO("section " << s.value()[i]);
		CHECK(used);
	}
	REQUIRE(checked > 0);
}

/* A key declared twice is a row nothing can reach: every lookup here and in the
   store answers with the first of them. */
TEST_CASE("no key is declared twice", "[settings]")
{
	// A row may ask the box whether it has what the row controls.
	FakeSystemSource row_box;
	InstalledSystemSource installed_row_box(&row_box);
	Result<std::vector<Descriptor> > sch = settings::schema();
	REQUIRE(sch.ok());
	REQUIRE(sch.value().size() > 1);

	for (size_t i = 0; i < sch.value().size(); ++i)
	{
		for (size_t j = i + 1; j < sch.value().size(); ++j)
		{
			INFO("rows " << i << " and " << j << " both declare " << sch.value()[i].key);
			CHECK(std::string(sch.value()[i].key) != std::string(sch.value()[j].key));
		}
	}
}

/* get() reads a String out of one call and everything else out of another, so a
   case set that only ever asks for a number never enters half of it. */
TEST_CASE("a text setting answers the text the source holds", "[settings]")
{
	FakeSettingsSource f;
	f.strings["language"] = "deutsch";
	// A number under the same key, so a read that took the wrong branch would
	// answer this instead of failing to find anything at all.
	f.ints["language"] = 7;
	setSettingsSource(&f);

	Result<std::string> r = settings::get("language");
	REQUIRE(r.ok());
	REQUIRE(r.value() == "deutsch");
	setSettingsSource(NULL);
}

TEST_CASE("a text setting the store never held reads as its declared default", "[settings]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);

	Result<Descriptor> d = settings::describe("language");
	REQUIRE(d.ok());
	REQUIRE(d.value().type == ValueType::String);
	REQUIRE(d.value().default_string != NULL);

	Result<std::string> r = settings::get("language");
	REQUIRE(r.ok());
	REQUIRE(r.value() == std::string(d.value().default_string));
	setSettingsSource(NULL);
}

/* A store that answered and a store that could not be read are two outcomes and
   only one of them is the declared default. Answering the default for both
   would report a box's settings as untouched while its store was broken. */
TEST_CASE("a store that cannot be read is an error and not the default", "[settings]")
{
	FakeSettingsSource f;
	f.ints["audio_AnalogMode"] = 2;
	f.fail_next = true;
	setSettingsSource(&f);

	Result<std::string> r = settings::get("audio_AnalogMode");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(r.error().code == ErrorCode::SettingUnreadable);
	setSettingsSource(NULL);
}

TEST_CASE("with no source installed a value is not answered as its default", "[settings]")
{
	setSettingsSource(NULL);

	Result<std::string> r = settings::get("audio_AnalogMode");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotSupported);
	REQUIRE(r.error().code == ErrorCode::SettingUnreadable);
}

// get() carries its own answer for a key nothing declares rather than reaching
// describe() for one, so the case above it does not cover this.
TEST_CASE("reading a key nobody declared is NotFound", "[settings]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);

	Result<std::string> r = settings::get("no-such-key-4711");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
	REQUIRE(r.error().code == ErrorCode::UnknownSetting);
	setSettingsSource(NULL);
}

/* A descriptor built from zeroes passes every case that only asks whether an
   answer came back, so one row is compared against what it declares. */
TEST_CASE("describing a key answers that row and not another", "[settings]")
{
	Result<Descriptor> d = settings::describe("audio_volume_percent_ac3");
	REQUIRE(d.ok());
	REQUIRE(std::string(d.value().key) == "audio_volume_percent_ac3");
	REQUIRE(std::string(d.value().section) == "audio");
	REQUIRE(d.value().type == ValueType::Int);
	REQUIRE(d.value().min == 0);
	REQUIRE(d.value().max == 100);
	REQUIRE(d.value().default_int == 100);
	REQUIRE(std::string(d.value().label_key) == "audiomenu.volume_adjustment_ac3");

	Result<Descriptor> other = settings::describe("audio_volume_percent_pcm");
	REQUIRE(other.ok());
	REQUIRE(std::string(other.value().key) == "audio_volume_percent_pcm");
	REQUIRE(std::string(other.value().label_key) != std::string(d.value().label_key));
}

/* The hardware library numbers the HDMI link's modes per family, and the box
   is told the stored number as one of them. So each entry is held to the
   library's own name for it, which a build for another family resolves to
   that family's number. The generic library numbers them as the literals the
   row once carried, so this build cannot tell a name from a number written
   out; scan-cecmodes holds the row to the names. */
TEST_CASE("the HDMI link modes are the numbers the hardware library gives them", "[settings]")
{
	// A row may ask the box whether it has what the row controls.
	FakeSystemSource row_box;
	InstalledSystemSource installed_row_box(&row_box);
	Result<Descriptor> d = settings::describe("hdmi_cec_mode");
	REQUIRE(d.ok());
	REQUIRE(d.value().type == ValueType::Enum);
	REQUIRE(d.value().value_count == 3);

	struct
	{
		const char *label_key;
		int         value;
	} const want[] =
	{
		{ "videomenu.hdmi_cec_mode_off", VIDEO_HDMI_CEC_MODE_OFF },
		{ "videomenu.hdmi_cec_mode_tuner", VIDEO_HDMI_CEC_MODE_TUNER },
		{ "videomenu.hdmi_cec_mode_recorder", VIDEO_HDMI_CEC_MODE_RECORDER }
	};
	for (size_t w = 0; w < sizeof(want) / sizeof(want[0]); ++w)
	{
		const EnumValue *found = NULL;
		for (size_t i = 0; i < d.value().value_count; ++i)
			if (d.value().values[i].label_key
			    && std::string(d.value().values[i].label_key) == want[w].label_key)
				found = &d.value().values[i];
		INFO(want[w].label_key);
		REQUIRE(found != NULL);
		REQUIRE(found->value == want[w].value);
	}
}

/* The three kinds that render as a number are rendered by one line, and a case
   set that only asks for one of them would not notice the day they stop being
   the same line. */
TEST_CASE("a bool an int and an enum all render as decimal", "[settings]")
{
	FakeSettingsSource f;
	f.ints["audio_DolbyDigital"] = 0;
	f.ints["current_volume_step"] = 12;
	f.ints["avsync"] = 2;
	f.ints["start_volume"] = -1;
	setSettingsSource(&f);

	REQUIRE(settings::describe("audio_DolbyDigital").value().type == ValueType::Bool);
	REQUIRE(settings::describe("current_volume_step").value().type == ValueType::Int);
	REQUIRE(settings::describe("avsync").value().type == ValueType::Enum);

	REQUIRE(settings::get("audio_DolbyDigital").value() == "0");
	REQUIRE(settings::get("current_volume_step").value() == "12");
	REQUIRE(settings::get("avsync").value() == "2");
	// The one bound below zero the section has, which a renderer written for
	// unsigned numbers would answer differently.
	REQUIRE(settings::get("start_volume").value() == "-1");
	setSettingsSource(NULL);
}

/* The key the brief names is not one the program has; this is the volume step
   it means, read off the row that declares it. */
TEST_CASE("a value outside the declared bounds is refused and nothing is written", "[settings]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);

	Result<Descriptor> d = settings::describe("current_volume_step");
	REQUIRE(d.ok());
	REQUIRE(d.value().type == ValueType::Int);

	char buf[32];
	snprintf(buf, sizeof(buf), "%ld", d.value().max + 1);
	std::string too_big(buf);
	Result<void> r = settings::set("current_volume_step", too_big);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::OutOfRange);
	REQUIRE(f.ints.find("current_volume_step") == f.ints.end());
	setSettingsSource(NULL);
}

TEST_CASE("an enum value the declaration does not list is refused", "[settings]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);

	Result<void> r = settings::set("audio_AnalogMode", "99");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().code == ErrorCode::NotAListedValue);
	REQUIRE(f.ints.find("audio_AnalogMode") == f.ints.end());
	setSettingsSource(NULL);
}

TEST_CASE("a value that is not a number where a number is declared is refused", "[settings]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);

	Result<void> r = settings::set("audio_AnalogMode", "loud");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().code == ErrorCode::NotANumber);

	Result<void> trailing = settings::set("audio_AnalogMode", "1x");
	REQUIRE_FALSE(trailing.ok());
	REQUIRE(trailing.error().code == ErrorCode::NotANumber);
	setSettingsSource(NULL);
}

TEST_CASE("an accepted value is written and persisted", "[settings]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);

	REQUIRE(settings::set("audio_AnalogMode", "1").ok());
	REQUIRE(f.ints["audio_AnalogMode"] == 1);
	REQUIRE(f.persisted == 1);
	setSettingsSource(NULL);
}

TEST_CASE("a row the parental lock holds is refused on a locked box and nothing is written", "[settings]")
{
	FakeSettingsSource f;
	InstalledSettingsSource installed(&f);
	FakeSystemSource box;
	InstalledSystemSource installed_box(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);
	box.parental_locked = true;

	Result<void> r = settings::set("parentallock_lockage", "16");
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().code == ErrorCode::SettingLocked);
	CHECK(r.error().status == Status::Conflict);
	CHECK(f.ints.empty());
	CHECK(f.persisted == 0);

	// The pin is not held, and neither is a row outside the parental section.
	REQUIRE(settings::set("parentallock_pincode", "4711").ok());
	REQUIRE(settings::set("audio_AnalogMode", "1").ok());
	CHECK(f.strings["parentallock_pincode"] == "4711");
	CHECK(f.ints["audio_AnalogMode"] == 1);

	box.parental_locked = false;
	REQUIRE(settings::set("parentallock_lockage", "16").ok());
	CHECK(f.ints["parentallock_lockage"] == 16);
}

TEST_CASE("a lock state the box cannot read refuses a held row", "[settings]")
{
	FakeSettingsSource f;
	InstalledSettingsSource installed(&f);
	FakeSystemSource box;
	InstalledSystemSource installed_box(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);
	box.parental_status = Status::Internal;

	Result<void> r = settings::set("parentallock_zaptime", "30");
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().code == ErrorCode::SettingLocked);
	CHECK(f.ints.empty());
	REQUIRE(settings::set("audio_AnalogMode", "1").ok());
}

TEST_CASE("a store that refuses the write is reported rather than swallowed", "[settings]")
{
	FakeSettingsSource f;
	f.fail_next = true;
	setSettingsSource(&f);

	Result<void> r = settings::set("audio_AnalogMode", "1");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	setSettingsSource(NULL);
}

TEST_CASE("writing a key nobody declared is refused before the store is touched", "[settings]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);

	Result<void> r = settings::set("no-such-key-4711", "1");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().code == ErrorCode::UnknownSetting);
	REQUIRE(f.ints.empty());
	REQUIRE(f.persisted == 0);
	setSettingsSource(NULL);
}

/* set() branches on the kind of the setting, so a case set written entirely of
   one kind would pass over the branches for the others. The four below are the
   three the brief's cases do not reach: a Bool, a String and the floor of an
   Int. */

// A Bool has two values and its row is not required to bound them, so what
// refuses a third is the type and not the row.
TEST_CASE("a bool takes its two values and refuses a third", "[settings]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);

	REQUIRE(settings::describe("audio_DolbyDigital").value().type == ValueType::Bool);

	REQUIRE(settings::set("audio_DolbyDigital", "0").ok());
	REQUIRE(f.ints["audio_DolbyDigital"] == 0);
	REQUIRE(settings::set("audio_DolbyDigital", "1").ok());
	REQUIRE(f.ints["audio_DolbyDigital"] == 1);

	/* The field behind this row is an int and holds 2 without complaint, so
	   the store cannot be what turns it down. */
	Result<void> r = settings::set("audio_DolbyDigital", "2");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::OutOfRange);
	REQUIRE(f.ints["audio_DolbyDigital"] == 1);
	setSettingsSource(NULL);
}

/* Text goes to the other call of the source and is held to nothing the row
   declares. A write that took the number branch would refuse this as not a
   number, and one that took the wrong call would leave it where no read of it
   looks. */
TEST_CASE("a text setting is written as text", "[settings]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);

	REQUIRE(settings::describe("language").value().type == ValueType::String);

	REQUIRE(settings::set("language", "deutsch").ok());
	REQUIRE(f.strings["language"] == "deutsch");
	REQUIRE(f.ints.empty());
	REQUIRE(f.persisted == 1);
	setSettingsSource(NULL);
}

// A range is two conditions. A check written as one of them takes everything
// below the floor.
TEST_CASE("a value below the declared floor is refused as one above the ceiling is", "[settings]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);

	Result<Descriptor> d = settings::describe("current_volume_step");
	REQUIRE(d.ok());
	REQUIRE(d.value().min == 1);

	char buf[32];
	snprintf(buf, sizeof(buf), "%ld", d.value().min - 1);
	Result<void> r = settings::set("current_volume_step", std::string(buf));
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::OutOfRange);
	REQUIRE(f.ints.find("current_volume_step") == f.ints.end());

	// The floor itself is a value the setting takes.
	REQUIRE(settings::set("current_volume_step", "1").ok());
	REQUIRE(f.ints["current_volume_step"] == 1);
	setSettingsSource(NULL);
}

// The one row whose floor is below zero, which a parse written for unsigned
// numbers would refuse and a check written for them would place above the
// ceiling.
TEST_CASE("a negative value a row declares is taken", "[settings]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);

	REQUIRE(settings::describe("start_volume").value().min == -1);

	REQUIRE(settings::set("start_volume", "-1").ok());
	REQUIRE(f.ints["start_volume"] == -1);
	REQUIRE(f.persisted == 1);

	Result<void> r = settings::set("start_volume", "-2");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().code == ErrorCode::OutOfRange);
	setSettingsSource(NULL);
}

// The store answers for the text write as for the number one, and a status
// read on one branch and dropped on the other is two branches to check.
TEST_CASE("a store that refuses a text write is reported too", "[settings]")
{
	FakeSettingsSource f;
	f.fail_next = true;
	setSettingsSource(&f);

	Result<void> r = settings::set("language", "deutsch");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(r.error().code == ErrorCode::SettingNotWritten);
	REQUIRE(f.strings.empty());
	REQUIRE(f.persisted == 0);
	setSettingsSource(NULL);
}

namespace
{
/* A store that takes the write and refuses the save. The shared fake cannot say
   that: its one failure lands on the write, which is the call before. */
struct SaveRefusingSource : public FakeSettingsSource
{
	coreapi::Status persist() { return coreapi::Status::Internal; }
};
} // anonymous namespace

/* The save is a second call and fails on its own, so a value the store took and
   nobody saved is not an accepted write. */
TEST_CASE("a save that fails is not an accepted write", "[settings]")
{
	SaveRefusingSource f;
	setSettingsSource(&f);

	Result<void> r = settings::set("audio_AnalogMode", "1");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(r.error().code == ErrorCode::SettingNotWritten);
	// The write itself went through, so the refusal is the save's and not the
	// write's.
	REQUIRE(f.ints["audio_AnalogMode"] == 1);
	REQUIRE(f.persisted == 0);
	setSettingsSource(NULL);
}

// A box with no store behind it answers what it cannot do rather than taking
// the value into nothing.
TEST_CASE("with no source installed a value is not quietly taken", "[settings]")
{
	setSettingsSource(NULL);

	Result<void> r = settings::set("audio_AnalogMode", "1");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotSupported);
	REQUIRE(r.error().code == ErrorCode::SettingNotWritten);

	Result<void> text = settings::set("language", "deutsch");
	REQUIRE_FALSE(text.ok());
	REQUIRE(text.error().status == Status::NotSupported);
}

namespace
{
bool fixtureSave() { return true; }

/* A row whose bounds outrun the field it names, which is the only place the two
   refusals can be told apart: 300 is inside what this row declares and outside
   what the byte behind it holds. No shipped row is written this way. */
const Descriptor kWiderThanItsField[] =
{
	{
		"fixture_byte", ValueType::Int, "fixture", "label", NULL,
		0, 1000, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_audio_pids_default),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
};
const size_t kWiderThanItsFieldCount =
	sizeof(kWiderThanItsField) / sizeof(kWiderThanItsField[0]);

/* The store the box runs on, because what asks an applier is whatever carries the
   write into the program's own settings, and a fake carries none: a case driving one
   would be checking its own bookkeeping. The sink stands in for the message loop, so
   nothing is carried until this file says so. Unbound from a destructor, or a write no
   case drained would be carried into a struct that is gone. */
struct RealStore
{
	SNeutrinoSettings     values;
	FakeCommandSink       sink;
	InstalledSink         installed;
	ClearedSettingsSource cleared;

	RealStore() : values(SNeutrinoSettings()), installed(&sink)
	{
		installRealSettingsSource(&values, fixtureSave);
	}

	~RealStore() { installRealSettingsSource(NULL, NULL); }
};
} // anonymous namespace

TEST_CASE("an accepted write asks the applier of its own section once", "[settings]")
{
	RealStore store;
	FakeApplier audio, network;
	REQUIRE(registerSettingsApplier("audio", &audio) == Status::Ok);
	REQUIRE(registerSettingsApplier("network", &network) == Status::Ok);

	REQUIRE(settings::set("audio_AnalogMode", "1").ok());
	applyPendingSettings();
	REQUIRE(audio.calls == 1);
	REQUIRE(audio.last_key == "audio_AnalogMode");
	REQUIRE(network.calls == 0);

	registerSettingsApplier("audio", NULL);
	registerSettingsApplier("network", NULL);
}

TEST_CASE("a section with no applier still writes and says so", "[settings]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);

	Result<void> r = settings::set("audio_AnalogMode", "1");
	REQUIRE(r.ok());
	REQUIRE(f.ints["audio_AnalogMode"] == 1);
	setSettingsSource(NULL);
}

/* What an applier answers reaches no caller: the value was stored and saved
   before anything was asked to apply it. What a refusal must not do is put the
   value back, or the store and the program would disagree the other way round
   with nobody able to see which. */
TEST_CASE("an applier that refuses leaves the written value standing", "[settings]")
{
	RealStore store;
	FakeApplier audio;
	audio.answer = false;
	InstalledApplier a("audio", &audio);

	REQUIRE(settings::set("audio_AnalogMode", "1").ok());
	applyPendingSettings();
	REQUIRE(audio.calls == 1);
	REQUIRE(store.values.audio_AnalogMode == 1);
}

TEST_CASE("a restart-only setting is written and never handed to an applier", "[settings]")
{
	RealStore store;
	FakeApplier audio;
	InstalledApplier a("audio", &audio);

	Result<Descriptor> d = settings::describe(kRestartOnlyAudioKey);
	REQUIRE(d.ok());
	REQUIRE(d.value().needs_restart);

	REQUIRE(settings::set(kRestartOnlyAudioKey, "1").ok());
	applyPendingSettings();
	// Written, and nobody told: the value is in the program's settings and the
	// applier was never asked.
	REQUIRE(store.values.start_volume == 1);
	REQUIRE(audio.calls == 0);
}

/* Two sections that both have rows the program ships, so the lookup is driven
   over a section a write really lands in rather than over one no row names.
   A call to every applier answers the same as a call to the right one while
   only one of them stands for a row. */
TEST_CASE("a write to one shipped section leaves the other section's applier alone", "[settings]")
{
	RealStore store;
	FakeApplier audio, general;
	InstalledApplier a("audio", &audio);
	InstalledApplier g("general", &general);

	REQUIRE(std::string(settings::describe("language").value().section) == "general");
	REQUIRE(std::string(settings::describe("audio_AnalogMode").value().section) == "audio");

	REQUIRE(settings::set("language", "deutsch").ok());
	applyPendingSettings();
	REQUIRE(general.calls == 1);
	REQUIRE(general.last_key == "language");
	REQUIRE(audio.calls == 0);

	REQUIRE(settings::set("audio_AnalogMode", "1").ok());
	applyPendingSettings();
	REQUIRE(audio.calls == 1);
	REQUIRE(audio.last_key == "audio_AnalogMode");
	REQUIRE(general.calls == 1);
}

// A value that never reached the store is not a change, so nothing is told
// about it. The applier runs after the write and not before it.
TEST_CASE("a write the store refuses never reaches the applier", "[settings]")
{
	RealStore store;
	InstalledSettingsTable table(kWiderThanItsField, kWiderThanItsFieldCount);
	FakeApplier applier;
	InstalledApplier a("fixture", &applier);

	// Inside what the row declares and outside what its field holds, so the
	// store is what turns it down.
	Result<void> r = settings::set("fixture_byte", "300");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().code == ErrorCode::SettingNotWritten);
	applyPendingSettings();
	REQUIRE(applier.calls == 0);
}

// The value the declaration turns down never reaches the store either, and the
// applier is behind both.
TEST_CASE("a value the declaration refuses never reaches the applier", "[settings]")
{
	RealStore store;
	FakeApplier audio;
	InstalledApplier a("audio", &audio);

	Result<void> r = settings::set("audio_AnalogMode", "99");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().code == ErrorCode::NotAListedValue);
	applyPendingSettings();
	REQUIRE(audio.calls == 0);
}

/* A write the loop was not told about is taken back, because that is what the
   answer said: the error is the caller's word that the value is not in the box
   and will not get there. Left held it would read back, and the next write of
   any other setting would be the message that carried it in. */
TEST_CASE("a write whose message the loop refused is not carried by a later one", "[settings]")
{
	RealStore store;
	FakeApplier audio;
	InstalledApplier a("audio", &audio);

	store.sink.answer = Status::Busy;
	Result<void> refused = settings::set("audio_AnalogMode", "1");
	REQUIRE_FALSE(refused.ok());
	REQUIRE(refused.error().code == ErrorCode::SettingNotWritten);
	REQUIRE(audio.calls == 0);
	REQUIRE(store.values.audio_AnalogMode == 0);

	// A read after the error answers what the box is running on and not what
	// the refusal said was not written.
	REQUIRE(settings::get("audio_AnalogMode").value() == "0");

	// Some other setting of the same section, written and saved. The refused
	// value must not ride in on it.
	store.sink.answer = Status::Ok;
	REQUIRE(settings::set("current_volume_step", "5").ok());
	applyPendingSettings();

	REQUIRE(store.values.current_volume_step == 5);
	REQUIRE(store.values.audio_AnalogMode == 0);
	REQUIRE(audio.calls == 1);
	REQUIRE(audio.last_key == "current_volume_step");
}

/* Registering nothing is what lets a case put the registry back, so a removal
   that left the entry standing would leave every case after it calling into an
   object that is gone. */
TEST_CASE("an applier taken away is not asked again", "[settings]")
{
	RealStore store;
	FakeApplier audio;

	REQUIRE(registerSettingsApplier("audio", &audio) == Status::Ok);
	REQUIRE(settings::set("audio_AnalogMode", "1").ok());
	applyPendingSettings();
	REQUIRE(audio.calls == 1);

	REQUIRE(registerSettingsApplier("audio", NULL) == Status::Ok);
	REQUIRE(settings::set("audio_AnalogMode", "2").ok());
	applyPendingSettings();
	REQUIRE(audio.calls == 1);

	// Removing what nobody registered leaves what the caller asked for.
	REQUIRE(registerSettingsApplier("audio", NULL) == Status::Ok);
}

// Registering twice replaces rather than adds, or the section would hold two
// appliers and the room for it would run out one screen at a time.
TEST_CASE("registering a section again replaces what was there", "[settings]")
{
	RealStore store;
	FakeApplier first, second;

	InstalledApplier a("audio", &first);
	REQUIRE(registerSettingsApplier("audio", &second) == Status::Ok);

	REQUIRE(settings::set("audio_AnalogMode", "1").ok());
	applyPendingSettings();
	REQUIRE(second.calls == 1);
	REQUIRE(first.calls == 0);
}

namespace
{
/* Records the thread it ran on and what the program's settings held while it
   ran. Both are the point: a notifier on the caller's thread touches what the
   loop owns, and one that runs before the write reads the value the box was
   running on rather than the one that was written. */
struct LoopApplier : public coreapi::SettingsApplier
{
	const SNeutrinoSettings *values;
	pthread_t                where;
	unsigned                 calls;
	long                     seen;
	std::string              last_key;

	explicit LoopApplier(const SNeutrinoSettings *v)
		: values(v), where(pthread_self()), calls(0), seen(-1) {}

	bool apply(const char *key)
	{
		where = pthread_self();
		calls++;
		seen = values->audio_AnalogMode;
		last_key = (key != 0) ? key : "";
		return true;
	}
};

/* What the loop does when the message the write posted reaches it. This thread is the
   loop for the time it runs, so it names itself as such, as the program does, and
   gives the name up again so a later case is not left with a loop that is gone. */
void *drainOnOwnThread(void *)
{
	bindApplyLoop();
	applyPendingSettings();
	resetApplyRegistry();
	return 0;
}

// A thread that is not the loop, which the bound registry refuses.
void *drainElsewhere(void *)
{
	applyPendingSettings();
	return 0;
}
} // anonymous namespace

/* The seam end to end, as far as it reaches without an endpoint in front of it:
   a write through the facade, the store the box runs on, the registry the
   screens register in, and the applier asked on the thread that drains rather
   than on the one that asked for the write. */
TEST_CASE("a written setting reaches its applier on the loop thread and not on the caller's", "[settings]")
{
	RealStore store;
	LoopApplier applier(&store.values);
	InstalledApplier a("audio", &applier);

	REQUIRE(settings::set("audio_AnalogMode", "1").ok());

	// Answered and not yet applied: the write is held and the message is what
	// carries it.
	REQUIRE(applier.calls == 0);
	REQUIRE(store.values.audio_AnalogMode == 0);
	REQUIRE(store.sink.posted.size() == 1);
	REQUIRE(store.sink.posted[0].first == (neutrino_msg_t) NeutrinoMessages::APPLY_SETTINGS);

	pthread_t loop;
	REQUIRE(pthread_create(&loop, 0, drainOnOwnThread, 0) == 0);
	REQUIRE(pthread_join(loop, 0) == 0);

	REQUIRE(applier.calls == 1);
	REQUIRE(applier.last_key == "audio_AnalogMode");
	REQUIRE_FALSE(pthread_equal(applier.where, pthread_self()));
	// The value was in the program's settings before the applier was asked,
	// which is what a notifier reads.
	REQUIRE(applier.seen == 1);
	REQUIRE(store.values.audio_AnalogMode == 1);
}

namespace
{
/* What a group saw when it ran. A group is a bare function, so what it records
   lives here, and Fresh puts it back for every case. */
struct GroupLog
{
	unsigned   runs;
	long       seen;
	long       seen_volume;
	unsigned   saves_before;
	pthread_t  where;
};

GroupLog        g_group;
unsigned        g_saves = 0;
const SNeutrinoSettings *g_group_values = 0;

bool countingSave() { ++g_saves; return true; }

Status runAudioGroup()
{
	++g_group.runs;
	g_group.seen = g_group_values->audio_AnalogMode;
	g_group.seen_volume = g_group_values->audio_volume_percent_ac3;
	g_group.saves_before = g_saves;
	g_group.where = pthread_self();
	return Status::Ok;
}

const char *const kAudioGroupKeys[] = { "audio_AnalogMode", "audio_volume_percent_ac3" };

struct FreshGroup
{
	FreshGroup(const SNeutrinoSettings *values)
	{
		resetApplyRegistry();
		g_group = GroupLog();
		g_group.runs = 0;
		g_group.seen = -1;
		g_group.seen_volume = -1;
		g_group.saves_before = 0;
		g_group.where = pthread_self();
		g_saves = 0;
		g_group_values = values;
	}
	~FreshGroup() { resetApplyRegistry(); }
};
} // anonymous namespace

/* One drain takes everything written since the last, so two keys of one group
   are one run of it, after the values landed and the save was made: what a
   group reads is what the box now holds. */
TEST_CASE("a drained batch of keys of one group runs the group once after the save", "[settings][apply]")
{
	RealStore store;
	installRealSettingsSource(&store.values, countingSave);
	FreshGroup fresh(&store.values);
	const ApplyGroup group = { "audio", ApplyPhase::Decoders, COREAPI_KEYS(kAudioGroupKeys), runAudioGroup };
	REQUIRE(registerApplyGroup(&group) == Status::Ok);
	runPhase(ApplyPhase::Decoders);
	g_group.runs = 0;
	FakeApplier audio;
	InstalledApplier a("audio", &audio);

	REQUIRE(settings::set("audio_AnalogMode", "1").ok());
	REQUIRE(settings::set("audio_volume_percent_ac3", "50").ok());
	REQUIRE(g_group.runs == 0);
	applyPendingSettings();

	REQUIRE(g_group.runs == 1);
	REQUIRE(g_group.seen == 1);
	REQUIRE(g_group.saves_before == 1);
	// The group stands in for the section's applier, so the key is applied once.
	REQUIRE(audio.calls == 0);
}

/* The transition: a key with no group is still applied by its section's
   applier, in the same drain that runs another key's group. */
TEST_CASE("a key without a group still reaches its section applier", "[settings][apply]")
{
	RealStore store;
	FreshGroup fresh(&store.values);
	const ApplyGroup group = { "audio", ApplyPhase::Decoders, COREAPI_KEYS(kAudioGroupKeys), runAudioGroup };
	REQUIRE(registerApplyGroup(&group) == Status::Ok);
	runPhase(ApplyPhase::Decoders);
	g_group.runs = 0;
	FakeApplier general;
	InstalledApplier g("general", &general);

	REQUIRE(settings::set("audio_AnalogMode", "1").ok());
	REQUIRE(settings::set("language", "deutsch").ok());
	applyPendingSettings();

	REQUIRE(g_group.runs == 1);
	REQUIRE(general.calls == 1);
	REQUIRE(general.last_key == "language");
}

/* A write that is drained before its group's phase is kept and not applied:
   the phase runs the group later with the value then in the box, and the
   section's applier is not asked in its place. This is what makes a write that
   arrives before the daemon is up harmless. */
TEST_CASE("a write drained before its phase is applied when the phase is reached", "[settings][apply]")
{
	RealStore store;
	FreshGroup fresh(&store.values);
	const ApplyGroup group = { "audio", ApplyPhase::Decoders, COREAPI_KEYS(kAudioGroupKeys), runAudioGroup };
	REQUIRE(registerApplyGroup(&group) == Status::Ok);
	FakeApplier audio;
	InstalledApplier a("audio", &audio);

	REQUIRE(settings::set("audio_AnalogMode", "1").ok());
	applyPendingSettings();

	REQUIRE(store.values.audio_AnalogMode == 1);
	REQUIRE(g_group.runs == 0);
	REQUIRE(audio.calls == 0);

	runPhase(ApplyPhase::Decoders);
	REQUIRE(g_group.runs == 1);
	REQUIRE(g_group.seen == 1);
}

/* The registry has no lock and belongs to the loop, so the group has to run on
   the thread that drains and not on the one that wrote. */
TEST_CASE("a group runs on the draining thread and not on the writer's", "[settings][apply]")
{
	RealStore store;
	FreshGroup fresh(&store.values);
	const ApplyGroup group = { "audio", ApplyPhase::Decoders, COREAPI_KEYS(kAudioGroupKeys), runAudioGroup };
	REQUIRE(registerApplyGroup(&group) == Status::Ok);
	runPhase(ApplyPhase::Decoders);
	g_group.runs = 0;

	REQUIRE(settings::set("audio_AnalogMode", "1").ok());
	REQUIRE(g_group.runs == 0);

	pthread_t loop;
	REQUIRE(pthread_create(&loop, 0, drainOnOwnThread, 0) == 0);
	REQUIRE(pthread_join(loop, 0) == 0);

	REQUIRE(g_group.runs == 1);
	REQUIRE_FALSE(pthread_equal(g_group.where, pthread_self()));
}

/* The registry is unlocked, so with the loop named a drain from another thread is
   refused whole: nothing lands, nothing runs, and what was written is still held
   for the loop, which then takes it. */
TEST_CASE("a drain on a thread that is not the loop is refused and the write stays held", "[settings][apply]")
{
	RealStore store;
	FreshGroup fresh(&store.values);
	const ApplyGroup group = { "audio", ApplyPhase::Decoders, COREAPI_KEYS(kAudioGroupKeys), runAudioGroup };
	REQUIRE(registerApplyGroup(&group) == Status::Ok);
	runPhase(ApplyPhase::Decoders);
	g_group.runs = 0;
	bindApplyLoop();

	REQUIRE(settings::set("audio_AnalogMode", "1").ok());

	pthread_t other;
	REQUIRE(pthread_create(&other, 0, drainElsewhere, 0) == 0);
	REQUIRE(pthread_join(other, 0) == 0);
	REQUIRE(g_group.runs == 0);
	REQUIRE(store.values.audio_AnalogMode == 0);

	applyPendingSettings();
	REQUIRE(g_group.runs == 1);
	REQUIRE(store.values.audio_AnalogMode == 1);
}

/* Two writes of one group before its phase are one run of it by the phase, which
   finds the last value of each, and the deferred drain did not run it. */
TEST_CASE("writes to one group before its phase are one phase run with the final values", "[settings][apply]")
{
	RealStore store;
	FreshGroup fresh(&store.values);
	const ApplyGroup group = { "audio", ApplyPhase::Decoders, COREAPI_KEYS(kAudioGroupKeys), runAudioGroup };
	REQUIRE(registerApplyGroup(&group) == Status::Ok);

	REQUIRE(settings::set("audio_volume_percent_ac3", "30").ok());
	REQUIRE(settings::set("audio_volume_percent_ac3", "60").ok());
	REQUIRE(settings::set("audio_AnalogMode", "1").ok());
	applyPendingSettings();
	REQUIRE(g_group.runs == 0);

	runPhase(ApplyPhase::Decoders);
	REQUIRE(g_group.runs == 1);
	REQUIRE(g_group.seen == 1);
	REQUIRE(g_group.seen_volume == 60);
}

// After its phase a drain applies the group again with what that drain wrote.
TEST_CASE("a drain after the phase applies the group", "[settings][apply]")
{
	RealStore store;
	FreshGroup fresh(&store.values);
	const ApplyGroup group = { "audio", ApplyPhase::Decoders, COREAPI_KEYS(kAudioGroupKeys), runAudioGroup };
	REQUIRE(registerApplyGroup(&group) == Status::Ok);

	REQUIRE(settings::set("audio_AnalogMode", "1").ok());
	applyPendingSettings();
	runPhase(ApplyPhase::Decoders);
	REQUIRE(g_group.runs == 1);

	REQUIRE(settings::set("audio_volume_percent_ac3", "40").ok());
	applyPendingSettings();
	REQUIRE(g_group.runs == 2);
	REQUIRE(g_group.seen_volume == 40);
	// The earlier write is still there, so the run saw the whole state.
	REQUIRE(g_group.seen == 1);
}

// A section is what the registry is keyed by, so one without a name names
// nothing and is refused rather than taking a slot nothing can reach.
TEST_CASE("an applier with no section name is refused", "[settings]")
{
	FakeApplier a;
	REQUIRE(registerSettingsApplier(NULL, &a) == Status::InvalidArgument);
	REQUIRE(registerSettingsApplier("", &a) == Status::InvalidArgument);
	REQUIRE(registerSettingsApplier(NULL, NULL) == Status::InvalidArgument);
}

/* The table is fixed so that registering allocates nothing, which means it can
   run out. Running out is answered rather than silently dropped, or a screen
   would come up registered and never be told about a change. */
TEST_CASE("a registry with no room left says so", "[settings]")
{
	FakeApplier a;
	// Distinct names that outlive the registrations, because the registry keeps
	// the pointer it was handed.
	std::vector<std::string> names(SETTINGS_APPLIER_LIMIT + 1);
	for (size_t i = 0; i < names.size(); ++i)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "fixture_section_%u", (unsigned) i);
		names[i] = buf;
	}

	for (size_t i = 0; i < SETTINGS_APPLIER_LIMIT; ++i)
		REQUIRE(registerSettingsApplier(names[i].c_str(), &a) == Status::Ok);

	REQUIRE(registerSettingsApplier(names[SETTINGS_APPLIER_LIMIT].c_str(), &a) == Status::Internal);

	// A slot given back is a slot the next registration takes.
	REQUIRE(registerSettingsApplier(names[0].c_str(), NULL) == Status::Ok);
	REQUIRE(registerSettingsApplier(names[SETTINGS_APPLIER_LIMIT].c_str(), &a) == Status::Ok);

	for (size_t i = 1; i < names.size(); ++i)
		REQUIRE(registerSettingsApplier(names[i].c_str(), NULL) == Status::Ok);
}

/* Everything above runs against the table the program ships, which is what makes those
   cases checks on the product. The rest of this file runs against rows of its own, for
   the rules the shipped rows cannot tell apart: a rule two implementations agree on
   over the rows that happen to exist is a rule no case against those rows can
   separate. */

namespace
{
/* A Bool that declares no bounds. Nothing refuses one: descriptorIsSane holds a
   Bool row to its default and to nothing else, and the rows of other kinds
   leave both bounds at nought. Every Bool the program declares today writes
   nought and one, so over the shipped table a check against the row's bounds
   and a check against the type's two values are one check. */
const Descriptor kBoolWithoutBounds[] =
{
	{
		"fixture_bool", ValueType::Bool, "fixture", "label", NULL,
		0, 0, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_descmode),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
};
const size_t kBoolWithoutBoundsCount =
	sizeof(kBoolWithoutBounds) / sizeof(kBoolWithoutBounds[0]);

// A number from one to fourteen that shows nought as off, and one that names
// nothing beside the same bounds.
const EnumValue kOffBelow[] = { { 0, "options.off", NULL, NULL, NULL, 0 } };
const Descriptor kNamedNumber[] =
{
	{
		"fixture_named", ValueType::Int, "fixture", "label", NULL,
		1, 14, COREAPI_VALUES(kOffBelow), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_descmode),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	{
		"fixture_plain", ValueType::Int, "fixture", "label", NULL,
		1, 14, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_descmode),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
};
const size_t kNamedNumberCount = sizeof(kNamedNumber) / sizeof(kNamedNumber[0]);

// A number from one to fourteen that shows two values below the floor in words.
const EnumValue kAutoAndOff[] =
{
	{ -1, "options.auto", NULL, NULL, NULL, 0 },
	{ 0, "options.off", NULL, NULL, NULL, 0 }
};
const Descriptor kSeveralNamed[] =
{
	{
		"fixture_several", ValueType::Int, "fixture", "label", NULL,
		1, 14, COREAPI_VALUES(kAutoAndOff), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_descmode),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
};
const size_t kSeveralNamedCount = sizeof(kSeveralNamed) / sizeof(kSeveralNamed[0]);

// A fan the box may lack, and a count the box may take only as a flag.
bool g_box_has = false;
bool boxHas() { return g_box_has; }
const Shape kFlagShape = shape(ValueType::Bool, "flag_label").range(0, 1);
const EnumValue kOffFloor[] = { { 0, "options.off", NULL, NULL, NULL, 0 } };
const Descriptor kOnBox[] =
{
	{
		"fixture_fan", ValueType::Int, "fixture", "label", NULL,
		1, 14, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(channellist_descmode, boxHas, NULL),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	{
		"fixture_scroll", ValueType::Int, "fixture", "label", NULL,
		0, 999, COREAPI_VALUES(kOffFloor), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(channellist_descmode, boxHas, &kFlagShape),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	{
		"fixture_choice", ValueType::Enum, "fixture", "label", NULL,
		0, 0, COREAPI_VALUES(kOffFloor), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ON(channellist_descmode, boxHas, NULL),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
};
const size_t kOnBoxCount = sizeof(kOnBox) / sizeof(kOnBox[0]);

// Rows descriptorIsSane refuses. set() does not call it, so what it does with
// one is its own answer and not something the table check stands in for.
const Descriptor kWrongRows[] =
{
	// A kind no ValueType names.
	{
		"fixture_unknown_kind", (ValueType) 99, "fixture", "label", NULL,
		0, 100, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	// An Enum whose list is not there, which offers no value at all.
	{
		"fixture_empty_enum", ValueType::Enum, "fixture", "label", NULL,
		0, 0, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	/* An Enum counting three values it does not have. The macro that writes the
	   pair cannot say this and a row written by hand can, which is the reason
	   the count is not trusted on its own. */
	{
		"fixture_lying_enum", ValueType::Enum, "fixture", "label", NULL,
		0, 0, NULL, 3, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
};
const size_t kWrongRowCount = sizeof(kWrongRows) / sizeof(kWrongRows[0]);

} // anonymous namespace

// A Bool has two values whatever its row says about bounds, so a check written
// against the bounds takes the same answers only while every Bool row declares
// nought and one.
TEST_CASE("a bool is held to its two values and not to the bounds of its row", "[settings]")
{
	FakeSettingsSource f;
	InstalledSettingsSource source(&f);
	InstalledSettingsTable table(kBoolWithoutBounds, kBoolWithoutBoundsCount);

	REQUIRE(descriptorIsSane(kBoolWithoutBounds[0]));
	// Outside what the row declares and inside what the type is.
	REQUIRE(kBoolWithoutBounds[0].max == 0);

	REQUIRE(settings::set("fixture_bool", "1").ok());
	REQUIRE(f.ints["fixture_bool"] == 1);
	REQUIRE(settings::set("fixture_bool", "0").ok());
	REQUIRE(f.ints["fixture_bool"] == 0);

	Result<void> r = settings::set("fixture_bool", "2");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::OutOfRange);
	REQUIRE(f.ints["fixture_bool"] == 0);
}

TEST_CASE("a number takes the value it names in words beside its bounds and nothing else", "[settings]")
{
	FakeSettingsSource f;
	InstalledSettingsSource source(&f);
	InstalledSettingsTable table(kNamedNumber, kNamedNumberCount);
	REQUIRE(descriptorIsSane(kNamedNumber[0]));
	REQUIRE(descriptorIsSane(kNamedNumber[1]));

	REQUIRE(settings::set("fixture_named", "0").ok());
	REQUIRE(f.ints["fixture_named"] == 0);
	REQUIRE(settings::set("fixture_named", "14").ok());
	REQUIRE(f.ints["fixture_named"] == 14);

	Result<void> under = settings::set("fixture_named", "-1");
	REQUIRE_FALSE(under.ok());
	REQUIRE(under.error().code == ErrorCode::OutOfRange);
	REQUIRE(under.error().message == "the setting takes 1 to 14 or 0");
	Result<void> over = settings::set("fixture_named", "15");
	REQUIRE_FALSE(over.ok());
	REQUIRE(over.error().code == ErrorCode::OutOfRange);
	REQUIRE(f.ints["fixture_named"] == 14);

	Result<void> plain = settings::set("fixture_plain", "0");
	REQUIRE_FALSE(plain.ok());
	REQUIRE(plain.error().code == ErrorCode::OutOfRange);
	REQUIRE(plain.error().message == "the setting takes 1 to 14");
	REQUIRE(f.ints.count("fixture_plain") == 0);
}

TEST_CASE("a number naming several values in words takes each of them beside its bounds", "[settings]")
{
	FakeSettingsSource f;
	InstalledSettingsSource source(&f);
	InstalledSettingsTable table(kSeveralNamed, kSeveralNamedCount);
	REQUIRE(descriptorIsSane(kSeveralNamed[0]));

	REQUIRE(settings::set("fixture_several", "-1").ok());
	REQUIRE(f.ints["fixture_several"] == -1);
	REQUIRE(settings::set("fixture_several", "0").ok());
	REQUIRE(f.ints["fixture_several"] == 0);
	REQUIRE(settings::set("fixture_several", "7").ok());
	REQUIRE(f.ints["fixture_several"] == 7);

	Result<void> other = settings::set("fixture_several", "-2");
	REQUIRE_FALSE(other.ok());
	REQUIRE(other.error().code == ErrorCode::OutOfRange);
	REQUIRE(other.error().message == "the setting takes 1 to 14 or -1, 0");
	REQUIRE(f.ints["fixture_several"] == 7);
}

TEST_CASE("a setting the box lacks reads and refuses every write", "[settings]")
{
	FakeSettingsSource f;
	InstalledSettingsSource source(&f);
	InstalledSettingsTable table(kOnBox, kOnBoxCount);
	REQUIRE(descriptorIsSane(kOnBox[0]));
	REQUIRE(descriptorIsSane(kOnBox[1]));
	f.ints["fixture_fan"] = 5;

	g_box_has = false;
	Result<void> r = settings::set("fixture_fan", "6");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Conflict);
	REQUIRE(r.error().code == ErrorCode::SettingNotOnThisBox);
	REQUIRE(f.ints["fixture_fan"] == 5);
	REQUIRE(settings::get("fixture_fan").value() == "5");
	REQUIRE(settings::describe("fixture_fan").value().type == ValueType::Int);
	Result<std::vector<SettingChoice> > none = settings::choices("fixture_choice");
	REQUIRE_FALSE(none.ok());
	REQUIRE(none.error().code == ErrorCode::SettingNotOnThisBox);

	g_box_has = true;
	REQUIRE(settings::choices("fixture_choice").ok());
	REQUIRE(settings::set("fixture_fan", "6").ok());
	REQUIRE(f.ints["fixture_fan"] == 6);
}

TEST_CASE("a setting in two shapes is written and described in the one the box offers", "[settings]")
{
	FakeSettingsSource f;
	InstalledSettingsSource source(&f);
	InstalledSettingsTable table(kOnBox, kOnBoxCount);

	g_box_has = true;
	REQUIRE(settings::set("fixture_scroll", "500").ok());
	REQUIRE(f.ints["fixture_scroll"] == 500);
	REQUIRE(settings::describe("fixture_scroll").value().max == 999);

	g_box_has = false;
	Result<void> r = settings::set("fixture_scroll", "2");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().code == ErrorCode::OutOfRange);
	REQUIRE(r.error().message == "the setting takes 0 or 1");
	REQUIRE(settings::set("fixture_scroll", "1").ok());
	REQUIRE(f.ints["fixture_scroll"] == 1);
	Result<Descriptor> flag = settings::describe("fixture_scroll");
	REQUIRE(flag.value().type == ValueType::Bool);
	REQUIRE(std::string(flag.value().label_key) == "flag_label");

	Result<std::vector<Descriptor> > all = settings::schema();
	REQUIRE(all.ok());
	REQUIRE(all.value().size() == kOnBoxCount);
	REQUIRE(all.value()[0].type == ValueType::Int);
	REQUIRE(all.value()[1].type == ValueType::Bool);
}

// A wrong row is refused rather than taken, and an enum list that is not there
// is missed rather than read.
TEST_CASE("a row this layer got wrong refuses the value", "[settings]")
{
	FakeSettingsSource f;
	InstalledSettingsSource source(&f);
	InstalledSettingsTable table(kWrongRows, kWrongRowCount);

	REQUIRE_FALSE(descriptorIsSane(kWrongRows[0]));
	REQUIRE_FALSE(descriptorIsSane(kWrongRows[1]));
	REQUIRE_FALSE(descriptorIsSane(kWrongRows[2]));

	Result<void> kind = settings::set("fixture_unknown_kind", "1");
	REQUIRE_FALSE(kind.ok());
	REQUIRE(kind.error().status == Status::Internal);
	REQUIRE(kind.error().code == ErrorCode::BadTable);

	Result<void> listless = settings::set("fixture_empty_enum", "0");
	REQUIRE_FALSE(listless.ok());
	REQUIRE(listless.error().status == Status::InvalidArgument);
	REQUIRE(listless.error().code == ErrorCode::NotAListedValue);

	// The count says there are three to read and there is nothing to read them
	// from, so the array and not the count is what says whether to look.
	Result<void> lying = settings::set("fixture_lying_enum", "0");
	REQUIRE_FALSE(lying.ok());
	REQUIRE(lying.error().code == ErrorCode::NotAListedValue);

	REQUIRE(f.ints.empty());
	REQUIRE(f.persisted == 0);
}

/* The two refusals are not one and neither stands in for the other. Driving the
   shipped source rather than a fake, because what a field holds is the source's
   answer and a fake would only repeat what this file decided. */
TEST_CASE("a value the row allows and the field cannot hold is refused by the store", "[settings]")
{
	SNeutrinoSettings values = SNeutrinoSettings();
	values.recording_audio_pids_default = 10;

	FakeCommandSink sink;
	InstalledSink installed(&sink);
	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kWiderThanItsField, kWiderThanItsFieldCount);
	installRealSettingsSource(&values, fixtureSave);

	// Inside what the row declares, so the declaration lets it through and the
	// field is what turns it down.
	Result<void> r = settings::set("fixture_byte", "300");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::SettingNotWritten);

	// Outside the row and inside the field, which the other refusal answers.
	Result<void> above = settings::set("fixture_byte", "1001");
	REQUIRE_FALSE(above.ok());
	REQUIRE(above.error().code == ErrorCode::OutOfRange);

	// Inside both, which nothing refuses.
	REQUIRE(settings::set("fixture_byte", "200").ok());
	applyPendingSettings();
	REQUIRE((int) values.recording_audio_pids_default == 200);
}

namespace
{
bool never() { return false; }
bool always() { return true; }

const EnumValue kOffered[] =
{
	{ 0, "options.off", NULL, NULL, NULL, 0 },
	{ 1, NULL, "ext4", NULL, NULL, 0 },
	{ 2, NULL, "xfs", never, NULL, 0 },
	{ 3, NULL, "f2fs", always, NULL, 0 },
};
const Descriptor kOfferedRows[] =
{
	{
		"t_choice", ValueType::Enum, "fixture", "label", NULL,
		0, 0, kOffered, sizeof(kOffered) / sizeof(kOffered[0]), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
};
} // anonymous namespace

TEST_CASE("choices carry the key, fixed text as text, and leave out what the box lacks", "[settings]")
{
	InstalledSettingsTable table(kOfferedRows, 1);
	Result<std::vector<SettingChoice> > r = settings::choices("t_choice");
	REQUIRE(r.ok());
	REQUIRE(r.value().size() == 3);
	REQUIRE(r.value()[0].label_key == "options.off");
	REQUIRE(r.value()[1].label_key.empty());
	REQUIRE(r.value()[1].label == "ext4");
	REQUIRE(r.value()[2].value == 3);
}

TEST_CASE("a write of an entry the box lacks is refused, a stored one still reads", "[settings]")
{
	FakeSettingsSource f;
	InstalledSettingsSource source(&f);
	InstalledSettingsTable table(kOfferedRows, 1);

	Result<void> refused = settings::set("t_choice", "2");
	REQUIRE_FALSE(refused.ok());
	REQUIRE(refused.error().code == ErrorCode::NotAListedValue);
	REQUIRE(f.ints.count("t_choice") == 0);
	REQUIRE(settings::set("t_choice", "3").ok());
	f.ints["t_choice"] = 2;
	REQUIRE(settings::get("t_choice").value() == "2");
}

/* Every other case in this suite and in two others walks the shipped table, so
   what the seam is held to is that it gives it back. A fixture left standing
   would have those cases checking the fixture. */
TEST_CASE("the shipped table comes back when a fixture is taken away", "[settings]")
{
	const size_t shipped = settingsTableCount();
	REQUIRE(shipped > kBoolWithoutBoundsCount);
	REQUIRE(settings::describe("audio_AnalogMode").ok());

	{
		InstalledSettingsTable table(kBoolWithoutBounds, kBoolWithoutBoundsCount);
		// The array and the count move together, or a walk reads a row that is
		// not there.
		REQUIRE(settingsTableCount() == kBoolWithoutBoundsCount);
		REQUIRE(settingsTable() == kBoolWithoutBounds);
		REQUIRE_FALSE(settings::describe("audio_AnalogMode").ok());
	}

	REQUIRE(settingsTableCount() == shipped);
	REQUIRE(settings::describe("audio_AnalogMode").ok());
	REQUIRE_FALSE(settings::describe("fixture_bool").ok());
}

namespace
{
/* Two rows of one section, one of them applied by nothing running. The shipped
   table has a single row marked that way, so over it a guard on needs_restart
   and a guard on that one key answer alike. */
const Descriptor kRestartAndNot[] =
{
	{
		"fixture_at_once", ValueType::Int, "fixture", "label", NULL,
		0, 100, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	{
		"fixture_on_restart", ValueType::Int, "fixture", "label", NULL,
		0, 100, NULL, 0, 0, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(current_volume),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
};
const size_t kRestartAndNotCount =
	sizeof(kRestartAndNot) / sizeof(kRestartAndNot[0]);
} // anonymous namespace

TEST_CASE("of two rows in one section only the one a restart does not gate is applied", "[settings]")
{
	RealStore store;
	InstalledSettingsTable table(kRestartAndNot, kRestartAndNotCount);
	FakeApplier applier;
	InstalledApplier installed("fixture", &applier);

	REQUIRE(settings::set("fixture_on_restart", "7").ok());
	applyPendingSettings();
	REQUIRE((int) store.values.current_volume == 7);
	REQUIRE(applier.calls == 0);

	REQUIRE(settings::set("fixture_at_once", "8").ok());
	applyPendingSettings();
	REQUIRE(store.values.repeat_blocker == 8);
	REQUIRE(applier.calls == 1);
	REQUIRE(applier.last_key == "fixture_at_once");
}

namespace
{
/* Two pairs of rows, each pair one secret and one not, so that a redaction
   written the wrong way round has a row to fail on. A rule exercised only over
   secret rows reads the same whether it hides them or hides everything else. */
const Descriptor kSecretAndNot[] =
{
	{
		"fixture_secret_text", ValueType::String, "fixture", "label", NULL,
		0, 0, NULL, 0, 0, "", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(language),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	{
		"fixture_plain_text", ValueType::String, "fixture", "label", NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(epg_dir),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	{
		"fixture_secret_number", ValueType::Int, "fixture", "label", NULL,
		0, 1000, NULL, 0, 0, NULL, false, true, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	{
		"fixture_plain_number", ValueType::Int, "fixture", "label", NULL,
		0, 1000, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(current_volume),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
};
const size_t kSecretAndNotCount = sizeof(kSecretAndNot) / sizeof(kSecretAndNot[0]);

/* What the program declares as a credential today. The one behind an arm is
   behind the arm its own key is loaded and saved under, so a build without it
   has neither the row nor this name. */
const char *const kShippedSecrets[] =
{
	"tmdb_api_key",
	"omdb_api_key",
	"shoutcast_dev_id",
	"youtube_api_key",
#if ENABLE_WEATHER_KEY_MANAGE
	"weather_api_key",
#endif
	"softupdate_proxyusername",
	"softupdate_proxypassword",
	"parentallock_pincode",
	"personalize_pincode",
	// The pin of each module slot, and the remote boxes, whose list carries a password.
	"ci_pincode_0",
	"ci_pincode_1",
	"ci_pincode_2",
	"ci_pincode_3",
	"timer_remotebox_ip"
};
const size_t kShippedSecretsCount = sizeof(kShippedSecrets) / sizeof(kShippedSecrets[0]);
} // anonymous namespace

TEST_CASE("a secret setting reads as nothing and the row beside it reads its value", "[settings]")
{
	InstalledSettingsTable table(kSecretAndNot, kSecretAndNotCount);
	FakeSettingsSource f;
	f.strings["fixture_secret_text"] = "hunter2";
	f.strings["fixture_plain_text"] = "/media/sda1/epg";
	f.ints["fixture_secret_number"] = 4711;
	f.ints["fixture_plain_number"] = 42;
	setSettingsSource(&f);

	REQUIRE(settings::get("fixture_secret_text").ok());
	REQUIRE(settings::get("fixture_secret_text").value() == "");
	REQUIRE(settings::get("fixture_secret_number").ok());
	REQUIRE(settings::get("fixture_secret_number").value() == "");

	// The other half of the rule: without these the same case would pass for a
	// redaction that answered nothing at all.
	REQUIRE(settings::get("fixture_plain_text").value() == "/media/sda1/epg");
	REQUIRE(settings::get("fixture_plain_number").value() == "42");

	setSettingsSource(NULL);
}

/* Nothing is redacted on the way out of the store, because the store is never
   asked. A source told to fail its next call still holds that failure after a
   secret has been read and spends it on the row after, which is what says the
   read went nowhere near it. */
TEST_CASE("reading a secret setting does not ask the store at all", "[settings]")
{
	InstalledSettingsTable table(kSecretAndNot, kSecretAndNotCount);
	FakeSettingsSource f;
	f.strings["fixture_plain_text"] = "/media/sda1/epg";
	setSettingsSource(&f);

	f.fail_next = true;
	Result<std::string> secret = settings::get("fixture_secret_text");
	REQUIRE(secret.ok());
	REQUIRE(secret.value() == "");

	Result<std::string> plain = settings::get("fixture_plain_text");
	REQUIRE_FALSE(plain.ok());
	REQUIRE(plain.error().code == ErrorCode::SettingUnreadable);

	setSettingsSource(NULL);
}

// Only the read changes. A credential a frontend types in reaches the program's
// own settings the way every other value does.
TEST_CASE("a secret setting is written like any other", "[settings]")
{
	RealStore store;
	InstalledSettingsTable table(kSecretAndNot, kSecretAndNotCount);

	REQUIRE(settings::set("fixture_secret_text", "a-real-key").ok());
	REQUIRE(settings::set("fixture_secret_number", "17").ok());
	applyPendingSettings();

	REQUIRE(store.values.language == "a-real-key");
	REQUIRE(store.values.repeat_blocker == 17);

	// And it is still not readable back, which is the round trip a caller does
	// not get for these.
	REQUIRE(settings::get("fixture_secret_text").value() == "");
}

// The schema keeps the row, or a frontend has no field to offer.
TEST_CASE("a secret setting is described like any other and says it is secret", "[settings]")
{
	InstalledSettingsTable table(kSecretAndNot, kSecretAndNotCount);

	Result<Descriptor> secret = settings::describe("fixture_secret_text");
	REQUIRE(secret.ok());
	REQUIRE(secret.value().secret);
	REQUIRE(std::string(secret.value().key) == "fixture_secret_text");
	REQUIRE(secret.value().type == ValueType::String);

	Result<Descriptor> plain = settings::describe("fixture_plain_text");
	REQUIRE(plain.ok());
	REQUIRE_FALSE(plain.value().secret);
}

/* The shipped table, so that a row losing the flag is caught rather than only
   the mechanism being. The store here is the program's own struct, filled by
   hand, because a fake would be answering for a lookup this is meant to prove
   goes nowhere. */
TEST_CASE("the credentials the program declares are secret and answer nothing", "[settings]")
{
	RealStore store;
	store.values.tmdb_api_key = "tmdb-real";
	store.values.omdb_api_key = "omdb-real";
	store.values.shoutcast_dev_id = "shoutcast-real";
	store.values.youtube_api_key = "youtube-real";
	store.values.weather_api_key = "weather-real";
	store.values.softupdate_proxyusername = "proxy-user";
	store.values.softupdate_proxypassword = "proxy-pass";
	store.values.epg_dir = "/media/sda1/epg";

	for (size_t i = 0; i < kShippedSecretsCount; ++i)
	{
		INFO("row " << kShippedSecrets[i]);
		Result<Descriptor> d = settings::describe(kShippedSecrets[i]);
		REQUIRE(d.ok());
		CHECK(d.value().secret);
		Result<std::string> v = settings::get(kShippedSecrets[i]);
		REQUIRE(v.ok());
		CHECK(v.value() == "");
	}

	/* A text row of the same section that is not a credential, so the case
	   cannot pass by hiding every String the misc screen offers. */
	REQUIRE_FALSE(settings::describe("epg_dir").value().secret);
	REQUIRE(std::string(settings::describe("epg_dir").value().section) == "misc");
	REQUIRE(settings::get("epg_dir").value() == "/media/sda1/epg");
}

/* The write a redaction makes likely. A frontend that reads a form and writes it back
   sends what the read answered, which for a credential is nothing. The row beside each
   secret one takes the same empty value, which is the half that refuses an inverted
   rule: a refusal written the wrong way round would turn every empty write down and
   this case would still read as green without it. */
TEST_CASE("a secret setting refuses an empty value and the row beside it takes one", "[settings]")
{
	RealStore store;
	InstalledSettingsTable table(kSecretAndNot, kSecretAndNotCount);

	REQUIRE(settings::set("fixture_secret_text", "a-real-key").ok());
	applyPendingSettings();
	REQUIRE(store.values.language == "a-real-key");

	Result<void> wiped = settings::set("fixture_secret_text", "");
	REQUIRE_FALSE(wiped.ok());
	CHECK(wiped.error().status == Status::InvalidArgument);
	CHECK(wiped.error().code == ErrorCode::EmptyCredential);

	// Nothing was taken, so nothing is carried and the credential still stands.
	applyPendingSettings();
	REQUIRE(store.values.language == "a-real-key");

	/* The other direction. A String that is not a credential takes an empty
	   value, because empty is a value for most of them: an unset directory and
	   an unset name are both written this way. */
	REQUIRE(settings::set("fixture_plain_text", "/media/sda1/epg").ok());
	applyPendingSettings();
	REQUIRE(store.values.epg_dir == "/media/sda1/epg");

	REQUIRE(settings::set("fixture_plain_text", "").ok());
	applyPendingSettings();
	REQUIRE(store.values.epg_dir == "");
}

/* Every kind, and the answer says why rather than reading as a bad number. A
   secret Int would be turned down by the parse anyway, under a code that says
   the value was not a number, which is not what a caller has to act on. */
TEST_CASE("a secret setting of any kind says why an empty value is refused", "[settings]")
{
	RealStore store;
	InstalledSettingsTable table(kSecretAndNot, kSecretAndNotCount);

	Result<void> number = settings::set("fixture_secret_number", "");
	REQUIRE_FALSE(number.ok());
	CHECK(number.error().code == ErrorCode::EmptyCredential);

	// The same row still takes a value, so the refusal is about the empty one.
	REQUIRE(settings::set("fixture_secret_number", "17").ok());
	applyPendingSettings();
	REQUIRE(store.values.repeat_blocker == 17);

	// And a number row that is not a credential is still held to the parse.
	Result<void> plain = settings::set("fixture_plain_number", "");
	REQUIRE_FALSE(plain.ok());
	CHECK(plain.error().code == ErrorCode::NotANumber);
}

/* The shipped table, so that the rule is checked over the rows the program
   really declares and not only over a fixture. */
TEST_CASE("the credentials the program declares refuse an empty value", "[settings]")
{
	RealStore store;

	for (size_t i = 0; i < kShippedSecretsCount; ++i)
	{
		INFO("row " << kShippedSecrets[i]);
		Result<void> r = settings::set(kShippedSecrets[i], "");
		REQUIRE_FALSE(r.ok());
		CHECK(r.error().code == ErrorCode::EmptyCredential);
	}

	// A text row of the same section that is not a credential, so the case
	// cannot pass by refusing every empty write. It is offered while the guide
	// is read.
	store.values.timeshiftdir = "/media/sda1/ts";
	REQUIRE(settings::set("timeshiftdir", "").ok());
	applyPendingSettings();
	REQUIRE(store.values.timeshiftdir == "");
}

// Reported rather than required: which rows are credentials is a judgement the
// table makes, and the number is what a reader checks it against.
TEST_CASE("the secret rows of the shipped table are counted", "[settings]")
{
	// A row may ask the box whether it has what the row controls.
	FakeSystemSource row_box;
	InstalledSystemSource installed_row_box(&row_box);
	Result<std::vector<Descriptor> > sch = settings::schema();
	REQUIRE(sch.ok());

	std::string names;
	size_t secret = 0;
	for (size_t i = 0; i < sch.value().size(); ++i)
	{
		if (!sch.value()[i].secret)
			continue;
		++secret;
		names += (names.empty() ? "" : ", ");
		names += sch.value()[i].key;
	}

	INFO("secret rows: " << secret << " of " << sch.value().size() << ": " << names);
	CHECK(secret == kShippedSecretsCount);
}

namespace
{
/* One save and one load of the program's own settings file, and everything that
   came back. What saves it on the box is CNeutrinoApp::saveSetup, which this
   binary cannot link; what it saves through is the object driven here. */
ConfigDataMap throughTheFile(const std::string &key, const std::string &value)
{
	// Relative and named with the process, so two runs of this suite writing
	// the same key at once read back what the other one wrote instead of
	// their own.
	const std::string path = "settings-format-case." + std::to_string(getpid()) + ".conf";

	CConfigFile out('=');
	out.setString(key, value);
	REQUIRE(out.saveConfig(path.c_str()));

	CConfigFile back('=');
	REQUIRE(back.loadConfig(path.c_str()));
	unlink(path.c_str());
	return back.getConfigDataMap();
}

// The value the reviewer sent, kept whole rather than built from pieces: the
// key it names is the parental pin and the value is one anybody could guess.
const char *const kInjection = "Berlin\nparentallock_pincode=1234";
} // anonymous namespace

/* The one that matters. A value carrying a line end is written into the file as
   the end of its own line, and everything after it is read back as a setting of
   its own, under any key the program loads. */
TEST_CASE("a text value cannot carry a second setting into the settings file", "[settings]")
{
	/* What the file does with it when nothing stops it. Without this half the
	   case below would pass for a layer that refused the letter B. */
	ConfigDataMap raw = throughTheFile("weather_location", kInjection);
	CHECK(raw.size() == 2);
	CHECK(raw["weather_location"] == "Berlin");
	CHECK(raw["parentallock_pincode"] == "1234");

	FakeSettingsSource f;
	InstalledSettingsSource installed(&f);

	Result<void> r = settings::set("weather_location", kInjection);
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().status == Status::InvalidArgument);
	CHECK(r.error().code == ErrorCode::BadString);
	CHECK(f.strings.find("weather_location") == f.strings.end());
	CHECK(f.persisted == 0);

	// And what the layer does take is one setting in the file and no more.
	REQUIRE(settings::set("weather_location", "Berlin").ok());
	ConfigDataMap saved = throughTheFile("weather_location", f.strings["weather_location"]);
	CHECK(saved.size() == 1);
	CHECK(saved["weather_location"] == "Berlin");
}

// Over every text row the program declares, because the rule belongs to the
// kind and not to one row of it.
TEST_CASE("no text row the program declares takes a value carrying a line end", "[settings]")
{
	// A box that has everything a row may ask about, so every text row is
	// held to the rule rather than refused for want of hardware.
	FakeSystemSource row_box;
	InstalledSystemSource installed_row_box(&row_box);
	memset(&row_box.caps, 0xff, sizeof(row_box.caps));
	FakeSettingsSource f;
	InstalledSettingsSource installed(&f);

	Result<std::vector<Descriptor> > sch = settings::schema();
	REQUIRE(sch.ok());

	size_t rows = 0;
	for (size_t i = 0; i < sch.value().size(); ++i)
	{
		if (sch.value()[i].type != ValueType::String)
			continue;
		++rows;
		INFO("row " << sch.value()[i].key);
		Result<void> r = settings::set(sch.value()[i].key, kInjection);
		REQUIRE_FALSE(r.ok());
		CHECK(r.error().code == ErrorCode::BadString);
	}

	// The rows are the point of the case, so a schema that stopped answering
	// them would otherwise pass it without checking anything.
	REQUIRE(rows > 40);
	CHECK(f.strings.empty());
	CHECK(f.persisted == 0);
}

namespace
{
struct TextSample
{
	const char *value;
	bool        allowed;
};

/* What a value may be, each with what the file would do with it. The four that
   are refused for the round trip are checked against it below; the carriage
   return is refused with the other control bytes rather than for that, because
   it survives a line the program reads and not one anything else does. */
const TextSample kTextSamples[] =
{
	{ "Berlin", true },
	{ "", true },
	{ "52.52,13.40", true },
	// The split is at the first separator, which is the one the program wrote.
	{ "a=b", true },
	{ "/media/sda1/movies", true },
	{ "Berlin\nparentallock_pincode=1234", false },
	{ "Berlin\rWest", false },
	{ "Berlin\tWest", false },
	{ "Berlin#West", false },
	{ " Berlin", false },
	{ "Berlin ", false },
};
const size_t kTextSampleCount = sizeof(kTextSamples) / sizeof(kTextSamples[0]);
} // anonymous namespace

/* The rule, stated as what the file gives back: everything this layer takes is
   one setting in the file and the same value out of it again. A rule kept only
   as a list of refused bytes would say nothing about what the refusals are for.
*/
TEST_CASE("what a text setting takes is what one line of the file gives back", "[settings]")
{
	FakeSettingsSource f;
	InstalledSettingsSource installed(&f);

	for (size_t i = 0; i < kTextSampleCount; ++i)
	{
		const std::string value = kTextSamples[i].value;
		INFO("value [" << value << "]");

		Result<void> r = settings::set("weather_city", value);
		CHECK(r.ok() == kTextSamples[i].allowed);

		/* Branched on what came back and not on what was expected: reading the
		   error of a result that succeeded ends the process, which would take
		   the rest of the suite down with it rather than fail this line. */
		if (!r.ok())
		{
			CHECK(r.error().status == Status::InvalidArgument);
			CHECK(r.error().code == ErrorCode::BadString);
			continue;
		}

		ConfigDataMap m = throughTheFile("weather_city", value);
		CHECK(m.size() == 1);
		CHECK(m["weather_city"] == value);
	}
}

/* The three the file cannot give back, each shown doing it. Without these the
   list above is a list of bytes somebody chose. */
TEST_CASE("the text values this layer refuses are the ones the file loses", "[settings]")
{
	// A line end makes a setting of its own, which the case above pins whole.
	ConfigDataMap split = throughTheFile("weather_city", "Berlin\nweather_postalcode=99999");
	CHECK(split.size() == 2);

	// A number sign takes the rest of the line with it.
	ConfigDataMap cut = throughTheFile("weather_city", "Berlin#West");
	CHECK(cut.size() == 1);
	CHECK(cut["weather_city"] == "Berlin");

	/* A space at either end comes back, and that is the trouble: nothing that
	   shows the value shows it, so a setting that is not the one asked for
	   reads as the one asked for. */
	ConfigDataMap padded = throughTheFile("weather_city", "Berlin ");
	CHECK(padded["weather_city"] == "Berlin ");
	CHECK(padded["weather_city"] != "Berlin");
}

// The two rules that hold whatever kind the row is, because neither the store
// nor the file can carry what they refuse and neither is about text.
TEST_CASE("a zero byte and an overlong value are refused whatever the row is", "[settings]")
{
	FakeSettingsSource f;
	InstalledSettingsSource installed(&f);

	std::string zero("Ber", 3);
	zero += '\0';
	zero += "lin";
	Result<void> text = settings::set("weather_city", zero);
	REQUIRE_FALSE(text.ok());
	CHECK(text.error().status == Status::InvalidArgument);
	CHECK(text.error().code == ErrorCode::ValueHasZeroByte);

	/* A number row says the same rather than answering that the value is not a
	   number: the parse would stop at the zero byte and take what came before
	   it, which is a different number from the one offered. */
	std::string number("1", 1);
	number += '\0';
	number += "9";
	Result<void> num = settings::set("current_volume_step", number);
	REQUIRE_FALSE(num.ok());
	CHECK(num.error().code == ErrorCode::ValueHasZeroByte);
	CHECK(f.ints.find("current_volume_step") == f.ints.end());

	const std::string longest(4096, 'a');
	REQUIRE(settings::set("weather_city", longest).ok());
	CHECK(f.strings["weather_city"] == longest);

	Result<void> over = settings::set("weather_city", longest + "a");
	REQUIRE_FALSE(over.ok());
	CHECK(over.error().status == Status::InvalidArgument);
	CHECK(over.error().code == ErrorCode::ValueTooLong);
	// The value that fit is still what is stored, so the refusal wrote nothing.
	CHECK(f.strings["weather_city"] == longest);
}

/* A read of a credential answers nothing and the schema beside it hands out the
   row's declared default, which is a value of the same setting. The tables
   declare the default the program falls back to because a check holds them to
   it, so what is withheld is withheld here. */
TEST_CASE("a secret row hands out no default and the row beside it keeps its own", "[settings]")
{
	// A row may ask the box whether it has what the row controls.
	FakeSystemSource row_box;
	InstalledSystemSource installed_row_box(&row_box);
	const Descriptor *declared = NULL;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		if (std::string(settingsTable()[i].key) == "parentallock_pincode")
			declared = &settingsTable()[i];
	}
	REQUIRE(declared != NULL);
	// What the table really says, or this case would pass over a row that
	// declares nothing anyway.
	REQUIRE(std::string(declared->default_string) == "0000");

	Result<Descriptor> pin = settings::describe("parentallock_pincode");
	REQUIRE(pin.ok());
	REQUIRE(pin.value().secret);
	CHECK(std::string(pin.value().default_string) == "");

	// The other direction: a row that is not a credential keeps its default.
	Result<Descriptor> plain = settings::describe("weather_city");
	REQUIRE(plain.ok());
	REQUIRE_FALSE(plain.value().secret);
	CHECK(std::string(plain.value().default_string) == "Berlin");

	// And the schema answers as the description does, or one of the two is the
	// way round it.
	Result<std::vector<Descriptor> > sch = settings::schema();
	REQUIRE(sch.ok());
	size_t seen = 0;
	for (size_t i = 0; i < sch.value().size(); ++i)
	{
		if (!sch.value()[i].secret)
			continue;
		++seen;
		INFO("row " << sch.value()[i].key);
		CHECK(std::string(sch.value()[i].default_string) == "");
	}
	REQUIRE(seen > 0);
}

/* resolveLabel is what stands between a Descriptor's label_key and a caller: a
   route that used to print label_key itself now goes through this, so what it
   answers for a name that does and does not resolve is what the API answers,
   not a detail behind it. */

TEST_CASE("resolveLabel is false for a NULL key and asks nothing to find that out", "[settings][locale]")
{
	// No source installed at all: a NULL key must not even reach the seam,
	// which a NoLocale default would answer NotFound for in any case, so this
	// is the one call here that would still pass with the check inside
	// resolveLabel deleted. The point of asking with nothing installed is that
	// nothing here can be mistaken for reading a fake's map.
	std::string out = "unchanged";
	REQUIRE_FALSE(settings::resolveLabel(NULL, out));
	// Left as it was found: a caller checks the bool and never this on a false
	// answer, but a resolveLabel that half wrote out on the way to failing
	// would be a wrong answer nothing here would otherwise catch.
	CHECK(out == "unchanged");
}

TEST_CASE("resolveLabel is false for a real key with no catalog installed", "[settings][locale]")
{
	setLocaleSource(0);
	std::string out;
	REQUIRE_FALSE(settings::resolveLabel("videomenu.videoformat_169", out));
}

TEST_CASE("resolveLabel hands back what the installed catalog resolves the key to", "[settings][locale]")
{
	FakeLocaleSource cat;
	cat.texts["videomenu.videoformat_169"] = "16:9";
	InstalledLocaleSource installed(&cat);

	std::string out;
	REQUIRE(settings::resolveLabel("videomenu.videoformat_169", out));
	CHECK(out == "16:9");
}

TEST_CASE("resolveLabel is false, not the key, for a name the installed catalog does not carry", "[settings][locale]")
{
	FakeLocaleSource cat;
	cat.texts["videomenu.videoformat_169"] = "16:9";
	InstalledLocaleSource installed(&cat);

	std::string out = "unchanged";
	// The exact defect this closes: a key nothing resolves must never come
	// back as itself, only as false with nothing written.
	REQUIRE_FALSE(settings::resolveLabel("videomenu.videoformat_XYZ", out));
	CHECK(out == "unchanged");
}

namespace
{

struct SettingsWatcher : public Subscriber
{
	std::vector<Event> seen;

	SettingsWatcher() { EventBus::instance().subscribe(this); }

	void onEvent(const Event &e) { seen.push_back(e); }
};

} // anonymous namespace

TEST_CASE("announceSettingsChanged puts one SettingsChanged event on the bus", "[settings]")
{
	SettingsWatcher watch;

	settings::announceSettingsChanged();

	REQUIRE(watch.seen.size() == 1);
	REQUIRE(watch.seen[0].type == EventType::SettingsChanged);
}

using settings::BatchOverlay;

// Asked before error(), which a mutation that turns the result ok would abort on.
static bool refusedAs(const Result<void> &r, ErrorCode code)
{
	return !r.ok() && r.error().code == code;
}

TEST_CASE("a write whose condition fails is refused", "[settings]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.ints["epg_save"] = 0;
	s.ints["epg_read"] = 0;

	Result<void> r = settings::set("epg_dir", "/usr");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().code == ErrorCode::SettingConditionNotMet);
	CHECK(r.error().status == Status::Conflict);
	CHECK(s.strings.count("epg_dir") == 0);
	CHECK(s.persisted == 0);

	// Either half of the group is enough.
	s.ints["epg_read"] = 1;
	REQUIRE(settings::set("epg_dir", "/usr").ok());
	CHECK(s.strings["epg_dir"] == "/usr");
}

TEST_CASE("a controlling setting and its dependent in one batch are accepted", "[settings]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.ints["epg_save"] = 0;
	s.ints["epg_read"] = 0;

	BatchOverlay b;
	b.values.push_back(std::make_pair("epg_save", "1"));
	b.values.push_back(std::make_pair("epg_dir", "/usr"));
	REQUIRE(settings::set("epg_save", "1", &b).ok());
	REQUIRE(settings::set("epg_dir", "/usr", &b).ok());
}

TEST_CASE("a dependent written ahead of its controller in one batch is judged on the batch", "[settings]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.ints["epg_save"] = 0;
	s.ints["epg_read"] = 0;

	BatchOverlay b;
	b.values.push_back(std::make_pair("epg_dir", "/usr"));
	b.values.push_back(std::make_pair("epg_save", "1"));
	REQUIRE(settings::set("epg_dir", "/usr", &b).ok());
	CHECK(s.ints["epg_save"] == 0);
	REQUIRE(settings::set("epg_save", "1", &b).ok());
}

TEST_CASE("a batch that turns the controller off refuses the dependent the store would allow", "[settings]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.ints["epg_save"] = 1;
	s.ints["epg_read"] = 0;

	BatchOverlay b;
	b.values.push_back(std::make_pair("epg_save", "0"));
	b.values.push_back(std::make_pair("epg_dir", "/usr"));
	REQUIRE(refusedAs(settings::set("epg_dir", "/usr", &b), ErrorCode::SettingConditionNotMet));
}

TEST_CASE("a condition the store cannot answer lets the write through", "[settings]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.ints["epg_save"] = 0;
	s.ints["epg_read"] = 0;

	// The first read is the condition's look at epg_save.
	s.fail_next = true;
	REQUIRE(settings::set("epg_dir", "/usr").ok());
	CHECK(s.strings["epg_dir"] == "/usr");
}

TEST_CASE("a controller the store never held is judged by its declared default", "[settings]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.ints["epg_read"] = 0;

	Result<Descriptor> save = settings::describe("epg_save");
	REQUIRE(save.ok());
	REQUIRE(save.value().default_int == 0);
	REQUIRE(refusedAs(settings::set("epg_dir", "/usr"), ErrorCode::SettingConditionNotMet));
}

namespace
{
const Condition kFixtureKeyValid[] =
{
	{ "fixture_key", CompareOp::TextValid, 0, NULL, 0, "XXXX", NULL, 0 }
};

/* A credential and a switch that needs it, the shape every online service has. */
const Descriptor kKeyAndService[] =
{
	{
		"fixture_key", ValueType::String, "fixture", "label", NULL,
		0, 0, NULL, 0, 0, "", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(language),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	{
		"fixture_service", ValueType::Bool, "fixture", "label", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kFixtureKeyValid),
		COREAPI_NUMBER_FIELD(current_volume),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
};
} // anonymous namespace

TEST_CASE("a credential a read withholds still allows what depends on it", "[settings]")
{
	InstalledSettingsTable table(kKeyAndService, 2);
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.strings["fixture_key"] = "the key";

	REQUIRE(settings::get("fixture_key").value() == "");
	REQUIRE(settings::set("fixture_service", "1").ok());
}

TEST_CASE("a text condition is judged on the batch first and the store after", "[settings]")
{
	InstalledSettingsTable table(kKeyAndService, 2);
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);

	// Never held: the declared default is empty and does not hold.
	REQUIRE(refusedAs(settings::set("fixture_service", "1"), ErrorCode::SettingConditionNotMet));

	BatchOverlay typed;
	typed.values.push_back(std::make_pair("fixture_key", "a new key"));
	REQUIRE(settings::set("fixture_service", "1", &typed).ok());

	s.strings["fixture_key"] = "the key";
	BatchOverlay placeholder;
	placeholder.values.push_back(std::make_pair("fixture_key", "XXXX"));
	REQUIRE(refusedAs(settings::set("fixture_service", "1", &placeholder),
	                  ErrorCode::SettingConditionNotMet));
}

namespace
{
const Condition kNumberOnText[] =
{
	{ "fixture_key", CompareOp::Ne, 0, NULL, 0, NULL, NULL, 0 }
};

const Condition kTextOnNumber[] =
{
	{ "fixture_number", CompareOp::TextValid, 0, NULL, 0, "XXXX", NULL, 0 }
};

/* Conditions the table check refuses, written here so that a row that slipped past it
   is not judged on a value read as the wrong kind. */
const Descriptor kWrongKind[] =
{
	{
		"fixture_key", ValueType::String, "fixture", "label", NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(language),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	{
		"fixture_number", ValueType::Int, "fixture", "label", NULL,
		0, 1000, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	{
		"fixture_by_number", ValueType::Bool, "fixture", "label", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kNumberOnText),
		COREAPI_NUMBER_FIELD(current_volume),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	{
		"fixture_by_text", ValueType::Bool, "fixture", "label", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kTextOnNumber),
		COREAPI_NUMBER_FIELD(current_volume),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
};
} // anonymous namespace

TEST_CASE("a condition naming a setting of the other kind is not answered", "[settings]")
{
	InstalledSettingsTable table(kWrongKind, 4);
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	// Both would fail if they were read: a number 0 and an empty text.
	s.ints["fixture_key"] = 0;
	s.strings["fixture_number"] = "";

	REQUIRE(settings::set("fixture_by_number", "1").ok());
	REQUIRE(settings::set("fixture_by_text", "1").ok());
}

TEST_CASE("check answers what set would for the value alone and writes nothing", "[settings]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.ints["epg_save"] = 0;
	s.ints["epg_read"] = 0;

	REQUIRE(refusedAs(settings::check("epg_save", "5"), ErrorCode::OutOfRange));
	REQUIRE(refusedAs(settings::check("epg_save", "on"), ErrorCode::NotANumber));
	REQUIRE(refusedAs(settings::check("no-such-key-4711", "1"), ErrorCode::UnknownSetting));
	REQUIRE(refusedAs(settings::check("epg_dir", "a\nb"), ErrorCode::BadString));
	// Its conditions are not asked: they are judged once the batch is known.
	REQUIRE(settings::check("epg_dir", "/usr").ok());
	REQUIRE(settings::check("epg_save", "1").ok());
	CHECK(s.ints.size() == 2);
	CHECK(s.strings.empty());
	CHECK(s.persisted == 0);
}

namespace
{
bool refusedKey(const std::vector<std::pair<std::string, Error> > &refused, const char *key)
{
	for (size_t i = 0; i < refused.size(); ++i)
		if (refused[i].first == key && refused[i].second.code == ErrorCode::SettingConditionNotMet &&
		    refused[i].second.status == Status::Conflict)
			return true;
	return false;
}
} // anonymous namespace

/* The icons are offered only where the skin is not the infobar's, and the skin only while
   the icons are off. Judged once against the whole batch, the skin falls and the icons
   then land on the infobar skin the store keeps, which their own condition forbids. */
TEST_CASE("settling takes out a member whose support was itself taken out", "[settings]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.ints["mode_icons"] = 0;
	s.ints["mode_icons_skin"] = INFOICONS_INFOVIEWER;

	BatchOverlay b;
	b.values.push_back(std::make_pair("mode_icons", "1"));
	b.values.push_back(std::make_pair("mode_icons_skin", "0"));
	std::vector<std::pair<std::string, Error> > refused;
	settings::settleBatch(b, refused);

	REQUIRE(refused.size() == 2);
	CHECK(refusedKey(refused, "mode_icons_skin"));
	CHECK(refusedKey(refused, "mode_icons"));
	CHECK(b.values.empty());
}

TEST_CASE("settling keeps a batch whose members hold together and takes out only what fails", "[settings]")
{
	FakeSettingsSource s;
	InstalledSettingsSource installed(&s);
	s.ints["epg_save"] = 0;
	s.ints["epg_read"] = 0;
	s.ints["mode_icons"] = 0;
	s.ints["mode_icons_skin"] = INFOICONS_STATIC;

	BatchOverlay b;
	b.values.push_back(std::make_pair("epg_dir", "/usr"));
	b.values.push_back(std::make_pair("mode_icons", "1"));
	b.values.push_back(std::make_pair("epg_save", "1"));
	b.values.push_back(std::make_pair("epg_save_standby", "1"));
	std::vector<std::pair<std::string, Error> > refused;
	settings::settleBatch(b, refused);

	REQUIRE(refused.empty());
	// The four named and the read the guide's save brings with it.
	REQUIRE(b.values.size() == 5);

	// Without the controller its two dependents fall, and the rest stays in body order.
	BatchOverlay alone;
	alone.values.push_back(std::make_pair("epg_dir", "/usr"));
	alone.values.push_back(std::make_pair("mode_icons", "1"));
	alone.values.push_back(std::make_pair("epg_save_standby", "1"));
	settings::settleBatch(alone, refused);
	REQUIRE(refused.size() == 2);
	CHECK(refusedKey(refused, "epg_dir"));
	CHECK(refusedKey(refused, "epg_save_standby"));
	REQUIRE(alone.values.size() == 1);
	CHECK(alone.values[0].first == "mode_icons");
}

TEST_CASE("a pin row refuses text the remote cannot type back", "[settings][textrules]")
{
	FakeSettingsSource f;
	InstalledSettingsSource installed(&f);
	FakeSystemSource box;
	InstalledSystemSource installed_box(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);

	const char *const keys[] = { "parentallock_pincode", "personalize_pincode" };
	const char *const refused[] = { "abcd", "12345", "123", "12 4", "-123" };
	for (size_t k = 0; k < 2; ++k)
	{
		for (size_t v = 0; v < sizeof(refused) / sizeof(refused[0]); ++v)
		{
			INFO(keys[k] << " " << refused[v]);
			CHECK(refusedAs(settings::set(keys[k], refused[v]), ErrorCode::BadString));
			CHECK(refusedAs(settings::check(keys[k], refused[v]), ErrorCode::BadString));
		}
		CHECK(f.strings.count(keys[k]) == 0);
		REQUIRE(settings::set(keys[k], "0815").ok());
		CHECK(f.strings[keys[k]] == "0815");
	}
}

TEST_CASE("a text row is held to its length, characters and place", "[settings][textrules]")
{
	FakeSettingsSource f;
	InstalledSettingsSource installed(&f);
	FakeSystemSource box;
	InstalledSystemSource installed_box(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);

	// Longer than the keyboard of the screen takes.
	CHECK(refusedAs(settings::set("omdb_api_key", "123456789"), ErrorCode::BadString));
	REQUIRE(settings::set("omdb_api_key", "12345678").ok());
	// Digits only, three of them.
	CHECK(refusedAs(settings::set("network_ntprefresh", "abc"), ErrorCode::BadString));
	CHECK(refusedAs(settings::set("network_ntprefresh", "1234"), ErrorCode::BadString));
	REQUIRE(settings::set("network_ntprefresh", "30").ok());

	// A folder that is not there, and one that is a file.
	CHECK(refusedAs(settings::set("network_nfs_audioplayerdir", "/no/such/folder-4711"), ErrorCode::BadString));
	CHECK(refusedAs(settings::set("network_nfs_audioplayerdir", "/etc/passwd"), ErrorCode::BadString));
	REQUIRE(settings::set("network_nfs_audioplayerdir", "/usr").ok());
	// A folder row that tests nothing takes any place.
	REQUIRE(settings::set("logo_hdd_dir", "/no/such/folder-4711").ok());
	// Empty means none and is not looked for.
	REQUIRE(settings::set("timeshiftdir", "").ok());

	// A font is a file of the right ending.
	CHECK(refusedAs(settings::set("font_file", "/etc/passwd"), ErrorCode::BadString));
	CHECK(refusedAs(settings::set("font_file", "/no/such/font.ttf"), ErrorCode::BadString));
	CHECK(refusedAs(settings::set("font_file", "/usr"), ErrorCode::BadString));
}

TEST_CASE("a text rule tells memory backed folders from the rest on the real file system", "[settings][textrules]")
{
	// /dev/shm is memory backed wherever the suite runs in the build image. Where it is
	// not, the case says so and records that it did not run, which the counts file fails.
	struct statfs fs;
	if (statfs("/dev/shm", &fs) != 0 || (long) fs.f_type != 0x1021994L)
	{
		WARN("skipped: /dev/shm is not a tmpfs here");
		recordCount("memory backed folders tried", 0);
		return;
	}

	const TextRule durable = { TextKind::Directory, 0, 0, NULL, MustExist::YesNotTmpfs, NULL, false };
	const TextRule update = { TextKind::Directory, 0, 0, NULL, MustExist::YesNotFlash, NULL, false };
	const TextRule any = { TextKind::Directory, 0, 0, NULL, MustExist::Yes, NULL, false };
	CHECK_FALSE(settings::holdsTextRule(durable, "/dev/shm").ok());
	CHECK(settings::holdsTextRule(update, "/dev/shm").ok());
	CHECK(settings::holdsTextRule(any, "/dev/shm").ok());
	CHECK(settings::holdsTextRule(durable, "/usr").ok());
	recordCount("memory backed folders tried", 1);
}

TEST_CASE("a file system is allowed by the level a row asks for", "[settings][textrules]")
{
	const long ramfs = 0x858458f6L, tmpfs = 0x1021994L, jffs2 = 0x72b6L, ext4 = 0xEF53L;
	// Flash is refused by both tested levels and taken by a row that only wants the folder.
	CHECK_FALSE(settings::fileSystemAllowed(MustExist::YesNotFlash, jffs2));
	CHECK_FALSE(settings::fileSystemAllowed(MustExist::YesNotTmpfs, jffs2));
	CHECK(settings::fileSystemAllowed(MustExist::Yes, jffs2));
	// Memory is refused only by the stricter level.
	CHECK(settings::fileSystemAllowed(MustExist::YesNotFlash, tmpfs));
	CHECK(settings::fileSystemAllowed(MustExist::YesNotFlash, ramfs));
	CHECK_FALSE(settings::fileSystemAllowed(MustExist::YesNotTmpfs, tmpfs));
	CHECK_FALSE(settings::fileSystemAllowed(MustExist::YesNotTmpfs, ramfs));
	CHECK(settings::fileSystemAllowed(MustExist::YesNotTmpfs, ext4));
	CHECK(settings::fileSystemAllowed(MustExist::YesNotFlash, ext4));
}

TEST_CASE("a pin cannot be cleared and a credential without a rule can", "[settings][textrules]")
{
	FakeSettingsSource f;
	InstalledSettingsSource installed(&f);
	FakeSystemSource box;
	InstalledSystemSource installed_box(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);

	f.strings["parentallock_pincode"] = "0815";
	f.strings["personalize_pincode"] = "0815";
	CHECK(refusedAs(settings::clearSecret("parentallock_pincode"), ErrorCode::BadString));
	CHECK(refusedAs(settings::clearSecret("personalize_pincode"), ErrorCode::BadString));
	CHECK(f.strings["parentallock_pincode"] == "0815");
	CHECK(f.strings["personalize_pincode"] == "0815");

	f.strings["softupdate_proxypassword"] = "secret";
	REQUIRE(settings::clearSecret("softupdate_proxypassword").ok());
	CHECK(f.strings["softupdate_proxypassword"] == "");
}

TEST_CASE("a row that names a place cannot be emptied unless its screen can", "[settings][textrules]")
{
	FakeSettingsSource f;
	InstalledSettingsSource installed(&f);
	FakeSystemSource box;
	InstalledSystemSource installed_box(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);

	CHECK(refusedAs(settings::set("font_file", ""), ErrorCode::BadString));
	CHECK(refusedAs(settings::set("font_file_monospace", ""), ErrorCode::BadString));
	CHECK(refusedAs(settings::set("epg_dir", ""), ErrorCode::BadString));
	CHECK(refusedAs(settings::set("network_nfs_audioplayerdir", ""), ErrorCode::BadString));
#ifndef USE_SMS_INPUT
	// Picked from existing files; the SMS arm types a name instead and may leave it empty.
	CHECK(refusedAs(settings::set("softupdate_url_file", ""), ErrorCode::BadString));
#endif
	// The timeshift folder empty means the recording folder.
	REQUIRE(settings::set("timeshiftdir", "").ok());
	// A row naming no place is not made stricter.
	REQUIRE(settings::set("keyboard_layout", "").ok());
}

TEST_CASE("a place that is already stored is not looked for again", "[settings][textrules]")
{
	FakeSettingsSource f;
	InstalledSettingsSource installed(&f);
	FakeSystemSource box;
	InstalledSystemSource installed_box(&box);
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);

	// An unmounted disk and a legacy empty font path, as a client sends the whole section back.
	f.strings["network_nfs_audioplayerdir"] = "/media/sda1/music-not-mounted";
	f.strings["font_file"] = "";
	REQUIRE(settings::set("network_nfs_audioplayerdir", "/media/sda1/music-not-mounted").ok());
	REQUIRE(settings::check("network_nfs_audioplayerdir", "/media/sda1/music-not-mounted").ok());
	REQUIRE(settings::set("font_file", "").ok());

	// Any other value has to be there, as the file browser demands.
	CHECK(refusedAs(settings::set("network_nfs_audioplayerdir", "/media/sda1/elsewhere-not-mounted"),
			ErrorCode::BadString));
	CHECK(f.strings["network_nfs_audioplayerdir"] == "/media/sda1/music-not-mounted");
	REQUIRE(settings::set("network_nfs_audioplayerdir", "/usr").ok());
}

TEST_CASE("a text rule matches file endings without regard to case", "[settings][textrules]")
{
	const TextRule fonts = { TextKind::File, 0, 0, NULL, MustExist::No, "ttf,otf", false };
	CHECK(settings::holdsTextRule(fonts, "/a/b.TTF").ok());
	CHECK(settings::holdsTextRule(fonts, "/a/b.otf").ok());
	CHECK_FALSE(settings::holdsTextRule(fonts, "/a/b.ttf.bak").ok());
	CHECK_FALSE(settings::holdsTextRule(fonts, "/a/ttf").ok());
}
