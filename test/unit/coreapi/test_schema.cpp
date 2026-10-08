/*
 * test_schema.cpp - tests for the schema descriptions
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
#include "coreapi/base/schema.h"
#include "support/conditions.h"

#include <cstring>

using namespace coreapi;

static const EnumValue kTwo[] = { { 0, "off", NULL, NULL, NULL, 0 }, { 1, "on", NULL, NULL, NULL, 0 } };
static const long kModes[] = { 2, 5, 9 };

static const Condition kOneCondition[] = {
	{ "other", CompareOp::Eq, 1, NULL, 0, NULL, NULL, 0 }
};
static const Condition kTwoConditions[] = {
	{ "other", CompareOp::Ne, 0, NULL, 0, NULL, NULL, 0 },
	{ "mode", CompareOp::In, 0, COREAPI_VALUES(kModes), NULL, NULL, 0 }
};

namespace
{

struct FakeSetting
{
	const char *key;
	long        value;
};

int reads = 0;

// The table ends at the entry with no key. A key that is not in it is one the
// caller cannot answer for, which is not the same as one whose value is zero.
bool readFake(const char *key, long *value, void *context)
{
	++reads;
	const FakeSetting *t = static_cast<const FakeSetting *>(context);
	for (size_t i = 0; t[i].key != NULL; ++i)
	{
		if (std::strcmp(t[i].key, key) == 0)
		{
			*value = t[i].value;
			return true;
		}
	}
	return false;
}

} // namespace

/* Stand ins for what the macros generate. A row's field is two functions, and
   the rules below are about which of them a row carries rather than about what
   they do, so these do nothing. The struct they take is never touched and never
   defined here. */
namespace
{
long readNumber(const SNeutrinoSettings &) { return 0; }
void writeNumber(SNeutrinoSettings &, long) {}
bool fitsNumber(long) { return true; }
void readText(const SNeutrinoSettings &, std::string &) {}
void writeText(SNeutrinoSettings &, const std::string &) {}

const FieldRef kNumber = { readNumber, writeNumber, NULL, fitsNumber, NULL, NULL, NULL, NULL,
			   "number", FieldOrigin::Member, NULL, NULL, NULL };
const FieldRef kText = { NULL, NULL, NULL, NULL, readText, writeText, NULL, NULL,
			 "text", FieldOrigin::Member, NULL, NULL, NULL };
} // namespace

TEST_CASE("a field that can be read and not written is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 0, 9, NULL, 0, 3, NULL, false, false,
			 COREAPI_ALWAYS, kNumber, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));

	d.field.write_number = NULL;
	REQUIRE_FALSE(descriptorIsSane(d));

	d.field.read_number = NULL;
	d.field.write_number = writeNumber;
	REQUIRE_FALSE(descriptorIsSane(d));
}

// The third of the set is what refuses a value the field cannot hold, so a row
// that carries the other two would take one and store something else.
TEST_CASE("a number field that cannot say what fits is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 0, 9, NULL, 0, 3, NULL, false, false,
			 COREAPI_ALWAYS, kNumber, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));

	d.field.fits_number = NULL;
	REQUIRE_FALSE(descriptorIsSane(d));

	d.field.read_number = NULL;
	d.field.write_number = NULL;
	d.field.fits_number = fitsNumber;
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("a text field that can be read and not written is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::String, "s", "l", "h", 0, 0, NULL, 0, 0, "", false, false,
			 COREAPI_ALWAYS, kText, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));

	d.field.write_text = NULL;
	REQUIRE_FALSE(descriptorIsSane(d));

	d.field.read_text = NULL;
	d.field.write_text = writeText;
	REQUIRE_FALSE(descriptorIsSane(d));
}

/* The name is what a check outside the compiler reads, and it is the only part
   of a field a table can write by hand. A field reached without one cannot be
   held to the key it belongs to, and a name over no field holds to nothing. */
TEST_CASE("a field reached but not named is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 0, 9, NULL, 0, 3, NULL, false, false,
			 COREAPI_ALWAYS, kNumber, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));

	d.field.name = NULL;
	REQUIRE_FALSE(descriptorIsSane(d));

	d.field.name = "";
	REQUIRE_FALSE(descriptorIsSane(d));

	Descriptor e = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(e));

	e.field.name = "menu_left_exit";
	REQUIRE_FALSE(descriptorIsSane(e));
}

/* A String is carried by text and every other kind by a number, so a row whose
   field is of the other sort declares a value no read of it could answer. Both
   directions, because a rule written for one of them leaves the other free. */
TEST_CASE("a String whose field holds a number is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::String, "s", "l", "h", 0, 0, NULL, 0, 0, "", false, false,
			 COREAPI_ALWAYS, kNumber, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE_FALSE(descriptorIsSane(d));

	d.field = kText;
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("a setting that is not a String whose field holds text is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 1, NULL, false, false,
			 COREAPI_ALWAYS, kText, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE_FALSE(descriptorIsSane(d));

	d.field = kNumber;
	REQUIRE(descriptorIsSane(d));

	// And an Enum, because the rule is written against the one kind that is
	// text rather than against the one kind that is not.
	Descriptor e = { "k", ValueType::Enum, "s", "l", "h", 0, 0, COREAPI_ENUM(kTwo), 1, NULL, false, false, COREAPI_ALWAYS, kText, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE_FALSE(descriptorIsSane(e));
	e.field = kNumber;
	REQUIRE(descriptorIsSane(e));
}

// A row that reaches nothing is the ordinary case for a setting the program
// keeps out of its settings struct, so it stays sane.
TEST_CASE("a row that names no field at all is sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 0, 9, NULL, 0, 3, NULL, false, false,
			 COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("an Int whose default sits outside its bounds is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 1, 5, NULL, 0, 7, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE_FALSE(descriptorIsSane(d));
	d.default_int = 3;
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("an Enum whose default names no listed value is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Enum, "s", "l", "h", 0, 0, kTwo, 2, 2, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE_FALSE(descriptorIsSane(d));
	d.default_int = 1;
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("an enum entry carries a key or a fixed text, never both or neither", "[schema]")
{
	const EnumValue both[] = { { 0, "options.off", "Off", NULL, NULL, 0 } };
	const EnumValue neither[] = { { 0, NULL, NULL, NULL, NULL, 0 } };
	const EnumValue key[] = { { 0, "options.off", NULL, NULL, NULL, 0 } };
	const EnumValue text[] = { { 0, NULL, "ext4", NULL, NULL, 0 } };
	Descriptor d = { "k", ValueType::Enum, "s", "l", "h", 0, 0, both, 1, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = neither;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = key;
	REQUIRE(descriptorIsSane(d));
	d.values = text;
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("an Enum with no values is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Enum, "s", "l", "h", 0, 0, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("a String with no default string is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::String, "s", "l", "h", 0, 0, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE_FALSE(descriptorIsSane(d));
	d.default_string = "";
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("an absent key or section is not sane and an absent label is", "[schema]")
{
	Descriptor ok = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(ok));

	Descriptor no_key = ok;    no_key.key = NULL;         REQUIRE_FALSE(descriptorIsSane(no_key));
	Descriptor no_sect = ok;   no_sect.section = NULL;    REQUIRE_FALSE(descriptorIsSane(no_sect));
	Descriptor empty_key = ok; empty_key.key = "";        REQUIRE_FALSE(descriptorIsSane(empty_key));

	// The program has no name for most of what its settings file holds, and a
	// row saying so is what keeps it from borrowing the name of another item.
	Descriptor no_label = ok;  no_label.label_key = NULL; REQUIRE(descriptorIsSane(no_label));
}

TEST_CASE("an Int whose bounds are inverted is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 5, 1, NULL, 0, 3, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("a section or a label that is present but empty is not sane", "[schema]")
{
	Descriptor ok = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(ok));

	Descriptor empty_sect = ok;  empty_sect.section = "";    REQUIRE_FALSE(descriptorIsSane(empty_sect));
	Descriptor empty_label = ok; empty_label.label_key = ""; REQUIRE_FALSE(descriptorIsSane(empty_label));
}

TEST_CASE("an Int whose default sits below its floor is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 1, 5, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE_FALSE(descriptorIsSane(d));
	d.default_int = 1;
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("an Enum that counts values it does not carry is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Enum, "s", "l", "h", 0, 0, NULL, 2, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("the enum macro writes the values and the count as one pair", "[schema]")
{
	Descriptor d = { "k", ValueType::Enum, "s", "l", "h", 0, 0, COREAPI_ENUM(kTwo), 1, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(d.values == kTwo);
	REQUIRE(d.value_count == 2u);
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("a Bool whose default is neither of its two values is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 7, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE_FALSE(descriptorIsSane(d));
	d.default_int = 1;
	REQUIRE(descriptorIsSane(d));
	d.default_int = 0;
	REQUIRE(descriptorIsSane(d));
}

namespace
{
bool notOffered() { return false; }
} // namespace

TEST_CASE("a Bool that names its words names exactly nought and one", "[schema]")
{
	const EnumValue noYes[] = { { 0, "messagebox.no", NULL, NULL, NULL, 0 }, { 1, "messagebox.yes", NULL, NULL, NULL, 0 } };
	const EnumValue yesNo[] = { { 1, "messagebox.yes", NULL, NULL, NULL, 0 }, { 0, "messagebox.no", NULL, NULL, NULL, 0 } };
	const EnumValue twice[] = { { 0, "messagebox.no", NULL, NULL, NULL, 0 }, { 0, "messagebox.yes", NULL, NULL, NULL, 0 } };
	const EnumValue third[] = { { 0, "messagebox.no", NULL, NULL, NULL, 0 }, { 2, "messagebox.yes", NULL, NULL, NULL, 0 } };
	const EnumValue unlabelled[] = { { 0, NULL, NULL, NULL, NULL, 0 }, { 1, "messagebox.yes", NULL, NULL, NULL, 0 } };
	const EnumValue withheld[] = { { 0, "messagebox.no", NULL, NULL, NULL, 0 }, { 1, "messagebox.yes", NULL, notOffered, NULL, 0 } };
	const EnumValue three[] = { { 0, "messagebox.no", NULL, NULL, NULL, 0 }, { 1, "messagebox.yes", NULL, NULL, NULL, 0 }, { 2, "x", NULL, NULL, NULL, 0 } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, noYes, 2, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));
	d.values = yesNo;
	REQUIRE(descriptorIsSane(d));
	d.values = twice;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = third;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = unlabelled;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = withheld;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = three;
	d.value_count = 3;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = noYes;
	d.value_count = 1;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = NULL;
	d.value_count = 2;
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("a number names a value in words, at its floor or outside its bounds", "[schema]")
{
	const EnumValue below[] = { { 0, "options.off", NULL, NULL, NULL, 0 } };
	const EnumValue floor[] = { { 1, "options.off", NULL, NULL, NULL, 0 } };
	const EnumValue above[] = { { 15, "options.off", NULL, NULL, NULL, 0 } };
	const EnumValue inside[] = { { 5, "options.off", NULL, NULL, NULL, 0 } };
	const EnumValue ceiling[] = { { 14, "options.off", NULL, NULL, NULL, 0 } };
	const EnumValue unworded[] = { { 0, NULL, NULL, NULL, NULL, 0 } };
	const EnumValue emptyWord[] = { { 0, "", NULL, NULL, NULL, 0 } };
	const EnumValue fixedText[] = { { 0, NULL, "off", NULL, NULL, 0 } };
	const EnumValue withheld[] = { { 0, "options.off", NULL, notOffered, NULL, 0 } };
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 1, 14, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));
	REQUIRE(namedNumber(d) == NULL);

	d.values = below;
	d.value_count = 1;
	REQUIRE(descriptorIsSane(d));
	REQUIRE(namedNumber(d) == &below[0]);
	d.values = floor;
	REQUIRE(descriptorIsSane(d));
	d.values = above;
	REQUIRE(descriptorIsSane(d));

	d.values = inside;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = ceiling;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = unworded;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = emptyWord;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = fixedText;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = withheld;
	REQUIRE_FALSE(descriptorIsSane(d));

	// Words with no value to stand for, and a count with no list.
	d.values = below;
	d.value_count = 0;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = NULL;
	d.value_count = 1;
	REQUIRE_FALSE(descriptorIsSane(d));
	REQUIRE(namedNumber(d) == NULL);

	// Only a number names a value this way.
	d.values = below;
	d.value_count = 1;
	d.type = ValueType::Enum;
	REQUIRE(namedNumber(d) == NULL);
}

TEST_CASE("a number may name several values, each outside the range and each once", "[schema]")
{
	const EnumValue two[] = { { -1, "options.auto", NULL, NULL, NULL, 0 }, { 0, "options.off", NULL, NULL, NULL, 0 } };
	const EnumValue twice[] = { { 0, "options.off", NULL, NULL, NULL, 0 }, { 0, "options.auto", NULL, NULL, NULL, 0 } };
	const EnumValue oneInside[] = { { 0, "options.off", NULL, NULL, NULL, 0 }, { 5, "options.auto", NULL, NULL, NULL, 0 } };
	const EnumValue unworded[] = { { 0, "options.off", NULL, NULL, NULL, 0 }, { -1, NULL, NULL, NULL, NULL, 0 } };
	const EnumValue five[] =
	{
		{ -4, "a", NULL, NULL, NULL, 0 }, { -3, "b", NULL, NULL, NULL, 0 }, { -2, "c", NULL, NULL, NULL, 0 },
		{ -1, "d", NULL, NULL, NULL, 0 }, { 0, "e", NULL, NULL, NULL, 0 }
	};
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 1, 14, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };

	d.values = two;
	d.value_count = 2;
	REQUIRE(descriptorIsSane(d));
	// Only the single-word case is answered by namedNumber, so nobody mistakes
	// the first of several for the only one.
	REQUIRE(namedNumber(d) == NULL);

	d.values = twice;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = oneInside;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = unworded;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = five;
	d.value_count = 5;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.value_count = 4;
	REQUIRE(descriptorIsSane(d));
}

namespace
{
long boundNow(const ValueLookup *) { return 3; }
bool noChoices(std::vector<SettingChoice> &) { return false; }
} // namespace

TEST_CASE("the members that shape how a value is shown belong to one kind each", "[schema]")
{
	const TextRule rule = { TextKind::Plain, 0, 0, NULL, MustExist::No, NULL, false };
	const Condition ok = { "other", CompareOp::Eq, 1, NULL, 0, NULL, NULL, 0 };
	const Condition group = { NULL, CompareOp::Eq, 0, NULL, 0, NULL, &ok, 1 };
	const Condition empty = { "", CompareOp::Eq, 1, NULL, 0, NULL, NULL, 0 };
	const EnumValue offered[] = { { 0, "a", NULL, NULL, &ok, 1 }, { 1, "b", NULL, NULL, NULL, 0 } };
	const EnumValue lost[] = { { 0, "a", NULL, NULL, NULL, 1 }, { 1, "b", NULL, NULL, NULL, 0 } };
	const EnumValue nested[] = { { 0, "a", NULL, NULL, &group, 1 }, { 1, "b", NULL, NULL, NULL, 0 } };
	const EnumValue unsane[] = { { 0, "a", NULL, NULL, &empty, 1 }, { 1, "b", NULL, NULL, NULL, 0 } };
	Descriptor num = { "k", ValueType::Int, "s", "l", "h", 0, 9, NULL, 0, 3, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	Descriptor str = { "k", ValueType::String, "s", "l", "h", 0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	Descriptor flag = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	Descriptor en = { "k", ValueType::Enum, "s", "l", "h", 0, 0, offered, 2, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };

	Descriptor d = num;
	d.unit_key = "unit.s";
	REQUIRE(descriptorIsSane(d));
	d = num;
	d.unit_key = "";
	REQUIRE_FALSE(descriptorIsSane(d));
	d = str;
	d.unit_key = "unit.s";
	REQUIRE_FALSE(descriptorIsSane(d));
	d = flag;
	d.unit_key = "unit.s";
	REQUIRE_FALSE(descriptorIsSane(d));

	d = str;
	d.text = &rule;
	REQUIRE(descriptorIsSane(d));
	d = num;
	d.text = &rule;
	REQUIRE_FALSE(descriptorIsSane(d));

	d = num;
	d.min_now = boundNow;
	d.max_now = boundNow;
	REQUIRE(descriptorIsSane(d));
	d = flag;
	d.max_now = boundNow;
	REQUIRE_FALSE(descriptorIsSane(d));
	d = flag;
	d.choices_from = noChoices;
	REQUIRE_FALSE(descriptorIsSane(d));
	d = num;
	d.choices_from = noChoices;
	REQUIRE(descriptorIsSane(d));

	REQUIRE(descriptorIsSane(en));
	// A word of a flag or of a number is not offered conditionally.
	const EnumValue wordWhen[] = { { -1, "a", NULL, NULL, &ok, 1 } };
	d = num;
	d.values = wordWhen;
	d.value_count = 1;
	d.min = 0;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = NULL;
	d.value_count = 0;
	REQUIRE(descriptorIsSane(d));
	d = en;
	d.values = lost;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = nested;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.values = unsane;
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("a row of a kind nothing offers yet is refused", "[schema]")
{
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 0, 9, NULL, 0, 3, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));
	// A list and a list of records are kinds, but a row of one without the pair that
	// carries it has nothing to read.
	d.type = ValueType::List;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.type = ValueType::Records;
	REQUIRE_FALSE(descriptorIsSane(d));
}

namespace
{
void readList(const SNeutrinoSettings &, std::vector<std::string> &) {}
void writeList(SNeutrinoSettings &, const std::vector<std::string> &) {}
void readRecords(const SNeutrinoSettings &, std::vector<RecordValues> &) {}
void writeRecords(SNeutrinoSettings &, const std::vector<RecordValues> &) {}

const RecordField kFields[] =
{
	{ "name", ValueType::String, 0, 0, false, NULL },
	{ "port", ValueType::Int, 1, 65535, false, NULL },
	{ "on", ValueType::Bool, 0, 0, false, NULL }
};

const FieldExtra kListExtra = { 0, readList, writeList, NULL, NULL, NULL, 0, 0, 0, false };
const FieldExtra kRecordsExtra = { 0, NULL, NULL, readRecords, writeRecords, kFields, 3, 0, 0, false };

const FieldRef kListField = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
			      "list", FieldOrigin::Member, NULL, NULL, &kListExtra };
const FieldRef kRecordsField = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
				 "records", FieldOrigin::Member, NULL, NULL, &kRecordsExtra };
} // namespace

// A list is a row of its own kind, carried by the pair of functions that copy it.
TEST_CASE("a list row is sane with the pair that carries it and not otherwise", "[schema]")
{
	Descriptor d = { "k", ValueType::List, "s", "l", "h", 0, 0, NULL, 0, 0, "", false, false,
			 COREAPI_ALWAYS, kListField, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));

	// A default is a text, of no entries where it is empty.
	d.default_string = NULL;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.default_string = "";

	// A row is a list or a list of records and not both.
	FieldExtra both = kListExtra;
	both.read_records = readRecords;
	both.write_records = writeRecords;
	d.field.extra = &both;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.field.extra = &kListExtra;

	// Half of the pair reads and cannot write.
	FieldExtra half = kListExtra;
	half.write_list = NULL;
	d.field.extra = &half;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.field.extra = &kListExtra;

	// A list is not a number nor a text, and is no member of an array.
	d.field.read_number = readNumber;
	d.field.write_number = writeNumber;
	d.field.fits_number = fitsNumber;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.field = kListField;
	d.field.origin = FieldOrigin::Element;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.field = kListField;

	// A row of another kind over a list's field reads no list.
	d.type = ValueType::String;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.type = ValueType::Int;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.type = ValueType::List;
	REQUIRE(descriptorIsSane(d));

	// A list's rule is the one its texts are held to, a String's own is a String's.
	static const TextRule rule = { TextKind::Plain, 0, 0, NULL, MustExist::No, NULL, false };
	d.text = &rule;
	REQUIRE(descriptorIsSane(d));
	d.type = ValueType::Records;
	REQUIRE_FALSE(descriptorIsSane(d));
}

// A record is made of members, each with a name, a kind of its own and bounds that hold.
TEST_CASE("a records row names what a record is made of", "[schema]")
{
	Descriptor d = { "k", ValueType::Records, "s", "l", "h", 0, 0, NULL, 0, 0, "", false, false,
			 COREAPI_ALWAYS, kRecordsField, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));

	RecordField fields[3] = { kFields[0], kFields[1], kFields[2] };
	FieldExtra extra = kRecordsExtra;
	extra.record_fields = fields;
	d.field.extra = &extra;
	REQUIRE(descriptorIsSane(d));

	// No members, a member without a name, two with the same name.
	extra.record_field_count = 0;
	REQUIRE_FALSE(descriptorIsSane(d));
	extra.record_field_count = 3;
	fields[1].name = "";
	REQUIRE_FALSE(descriptorIsSane(d));
	fields[1].name = "name";
	REQUIRE_FALSE(descriptorIsSane(d));
	fields[1].name = "port";

	// A member is a flag, a number or a text and nothing else, and a number has bounds that fit it.
	fields[2].type = ValueType::Enum;
	REQUIRE_FALSE(descriptorIsSane(d));
	fields[2].type = ValueType::Bool;
	fields[1].min = 70000;
	REQUIRE_FALSE(descriptorIsSane(d));
	fields[1].min = 1;
	REQUIRE(descriptorIsSane(d));

	// A credential among the members makes the whole list one, because the list is what is read.
	fields[0].secret = true;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.secret = true;
	REQUIRE(descriptorIsSane(d));

	// And it is a list of records and not a list of texts.
	d.field = kListField;
	REQUIRE_FALSE(descriptorIsSane(d));
}

// A flag file has no functions: its name is its path, and it is a flag.
TEST_CASE("a flag file row is a flag named by an absolute path", "[schema]")
{
	const FieldRef flag = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
				"/var/tmp/.flag", FieldOrigin::FlagFile, NULL, NULL, NULL };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, flag, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));

	d.field.name = "var/tmp/.flag";
	REQUIRE_FALSE(descriptorIsSane(d));
	d.field.name = "";
	REQUIRE_FALSE(descriptorIsSane(d));
	d.field.name = NULL;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.field = flag;

	// It is on or off and no number, and it has no member to read.
	d.type = ValueType::Int;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.type = ValueType::Bool;
	d.field.read_number = readNumber;
	d.field.write_number = writeNumber;
	d.field.fits_number = fitsNumber;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.field = flag;
	d.field.extra = &kListExtra;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.field = flag;
	REQUIRE(descriptorIsSane(d));
}

// An element names the array it is of and carries a number or a text, and its place.
TEST_CASE("an element row says which element it is", "[schema]")
{
	const FieldExtra place = { 2, NULL, NULL, NULL, NULL, NULL, 0, 5, 0, false };
	FieldRef element = kNumber;
	element.origin = FieldOrigin::Element;
	element.extra = &place;
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 0, 9, NULL, 0, 3, NULL, false, false,
			 COREAPI_ALWAYS, element, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));

	// Without its place it is no element, and no place is before the first.
	d.field.extra = NULL;
	REQUIRE_FALSE(descriptorIsSane(d));
	const FieldExtra before = { -1, NULL, NULL, NULL, NULL, NULL, 0, 5, 0, false };
	d.field.extra = &before;
	REQUIRE_FALSE(descriptorIsSane(d));

	// And no place is past the end of the array it is of.
	const FieldExtra past = { 5, NULL, NULL, NULL, NULL, NULL, 0, 5, 0, false };
	d.field.extra = &past;
	REQUIRE_FALSE(descriptorIsSane(d));
	const FieldExtra none = { 0, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, false };
	d.field.extra = &none;
	REQUIRE_FALSE(descriptorIsSane(d));

	// An element is a number or a text and not a list, and a text row carries a text.
	d.field.extra = &kListExtra;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.field.extra = &place;
	d.field.ask = NULL;
	REQUIRE(descriptorIsSane(d));
	REQUIRE(valueIsInNamedMember(d.field));
}

namespace
{
bool g_has = false;
bool boxHas() { return g_has; }
} // namespace

TEST_CASE("a row in two shapes needs its test and its other shape is held to every rule", "[schema]")
{
	const EnumValue off[] = { { 0, "options.off", NULL, NULL, NULL, 0 } };
	const Shape flag = shape(ValueType::Bool, "flag").range(0, 1);
	const Shape text = shape(ValueType::String, "text");
	const Shape emptyLabel = shape(ValueType::Bool, "").range(0, 1);
	const Shape narrow = shape(ValueType::Int, "narrow").range(2, 5);
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 0, 999, off, 1, 1, NULL, false, false, COREAPI_ALWAYS, kNumber, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));

	d.field.available = boxHas;
	REQUIRE(descriptorIsSane(d));
	d.field.otherwise = &flag;
	REQUIRE(descriptorIsSane(d));

	// A shape nothing chooses.
	d.field.available = NULL;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.field.available = boxHas;

	// Text over a number field, a label present and empty, and a default of
	// the row the other shape cannot hold.
	d.field.otherwise = &text;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.field.otherwise = &emptyLabel;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.field.otherwise = &narrow;
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("the row this box offers is the declared one or its other shape or none", "[schema]")
{
	const EnumValue off[] = { { 0, "options.off", NULL, NULL, NULL, 0 } };
	const Shape flag = shape(ValueType::Bool, "flag").range(0, 1);
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 0, 999, off, 1, 1, NULL, false, false, COREAPI_ALWAYS, kNumber, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	Descriptor here;

	REQUIRE(rowOnThisBox(d, here));
	REQUIRE(here.type == ValueType::Int);

	d.field.available = boxHas;
	g_has = true;
	REQUIRE(rowOnThisBox(d, here));
	REQUIRE(here.max == 999);
	g_has = false;
	REQUIRE_FALSE(rowOnThisBox(d, here));
	REQUIRE(here.type == ValueType::Int);

	d.field.otherwise = &flag;
	REQUIRE(rowOnThisBox(d, here));
	REQUIRE(here.type == ValueType::Bool);
	REQUIRE(std::string(here.label_key) == "flag");
	REQUIRE(here.min == 0);
	REQUIRE(here.max == 1);
	REQUIRE(here.values == NULL);
	REQUIRE(here.value_count == 0);
	REQUIRE(std::string(here.hint_key) == "h");
	REQUIRE(here.default_int == 1);
	g_has = true;
	REQUIRE(rowOnThisBox(d, here));
	REQUIRE(here.type == ValueType::Int);
	REQUIRE(here.value_count == 1);
}

TEST_CASE("a type that is none of the four is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));
	d.type = static_cast<ValueType>(99);
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("a hint is the one thing a setting may leave out", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", NULL, 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("a setting with no condition is always shown and is sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(d.conditions == NULL);
	REQUIRE(d.condition_count == 0u);
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("conditions are carried as a list and every one of them is read", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(kTwoConditions), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(d.conditions == kTwoConditions);
	REQUIRE(d.condition_count == 2u);
	REQUIRE(descriptorIsSane(d));

	// The second element is the one an evaluator would miss if the count were
	// ignored and only the first were read.
	Condition broken[] = { kTwoConditions[0], kTwoConditions[1] };
	broken[1].key = "";
	Descriptor with_broken = d;
	with_broken.conditions = broken;
	REQUIRE_FALSE(descriptorIsSane(with_broken));
}

TEST_CASE("a condition count without the conditions is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, NULL, 2, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE_FALSE(descriptorIsSane(d));

	// One is the count a dropped list leaves behind, and it is the boundary the
	// guard is written at.
	d.condition_count = 1;
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("a condition that names no setting is not sane", "[schema]")
{
	Condition c[] = { { NULL, CompareOp::Eq, 1, NULL, 0, NULL, NULL, 0 } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(c), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE_FALSE(descriptorIsSane(d));

	c[0].key = "";
	REQUIRE_FALSE(descriptorIsSane(d));

	c[0].key = "other";
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("an In condition that lists no value is not sane", "[schema]")
{
	Condition c[] = { { "mode", CompareOp::In, 0, NULL, 2, NULL, NULL, 0 } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(c), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE_FALSE(descriptorIsSane(d));

	c[0].values = kModes;
	c[0].value_count = 0;
	REQUIRE_FALSE(descriptorIsSane(d));

	c[0].value_count = 3;
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("only an In condition is asked for a value list", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(kOneCondition), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(kOneCondition[0].values == NULL);
	REQUIRE(kOneCondition[0].value_count == 0u);
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("the value macro writes an In list and its count as one pair", "[schema]")
{
	REQUIRE(kTwoConditions[1].values == kModes);
	REQUIRE(kTwoConditions[1].value_count == 3u);
}

TEST_CASE("a malformed condition is refused whatever the type of the setting is", "[schema]")
{
	Condition bad[] = { { NULL, CompareOp::Eq, 1, NULL, 0, NULL, NULL, 0 } };

	Descriptor as_bool = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			       COREAPI_CONDITIONS(bad), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	Descriptor as_int = { "k", ValueType::Int, "s", "l", "h", 0, 9, NULL, 0, 3, NULL, false, false,
			      COREAPI_CONDITIONS(bad), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	Descriptor as_string = { "k", ValueType::String, "s", "l", "h", 0, 0, NULL, 0, 0, "", false, false,
				 COREAPI_CONDITIONS(bad), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	Descriptor as_enum = { "k", ValueType::Enum, "s", "l", "h", 0, 0, COREAPI_ENUM(kTwo), 1, NULL, false, false,
			       COREAPI_CONDITIONS(bad), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };

	REQUIRE_FALSE(descriptorIsSane(as_bool));
	REQUIRE_FALSE(descriptorIsSane(as_int));
	REQUIRE_FALSE(descriptorIsSane(as_string));
	REQUIRE_FALSE(descriptorIsSane(as_enum));

	// All four are sane once the condition names a setting, so it is the
	// condition being refused and not the descriptor around it.
	bad[0].key = "other";
	REQUIRE(descriptorIsSane(as_bool));
	REQUIRE(descriptorIsSane(as_int));
	REQUIRE(descriptorIsSane(as_string));
	REQUIRE(descriptorIsSane(as_enum));
}

TEST_CASE("a setting that carries no condition is shown", "[schema]")
{
	FakeSetting t[] = { { "other", 0 }, { NULL, 0 } };
	ValueLookup lookup = { readFake, NULL, t };

	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(conditionsHold(d, lookup));
}

TEST_CASE("a list of no conditions is shown though its first entry would not hold", "[schema]")
{
	FakeSetting t[] = { { "other", 0 }, { NULL, 0 } };
	ValueLookup lookup = { readFake, NULL, t };

	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, kOneCondition, 0, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(conditionsHold(d, lookup));

	d.condition_count = 1;
	REQUIRE_FALSE(conditionsHold(d, lookup));
}

TEST_CASE("every condition has to hold and not merely one of them", "[schema]")
{
	Condition both[] = {
		{ "a", CompareOp::Eq, 1, NULL, 0, NULL, NULL, 0 },
		{ "b", CompareOp::Eq, 1, NULL, 0, NULL, NULL, 0 }
	};
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(both), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };

	FakeSetting yes_yes[] = { { "a", 1 }, { "b", 1 }, { NULL, 0 } };
	FakeSetting yes_no[] = { { "a", 1 }, { "b", 0 }, { NULL, 0 } };
	FakeSetting no_yes[] = { { "a", 0 }, { "b", 1 }, { NULL, 0 } };
	FakeSetting no_no[] = { { "a", 0 }, { "b", 0 }, { NULL, 0 } };

	ValueLookup both_hold = { readFake, NULL, yes_yes };
	REQUIRE(conditionsHold(d, both_hold));

	// These two are what tells a conjunction from a disjunction: under an OR
	// both of them would show the setting.
	ValueLookup second_fails = { readFake, NULL, yes_no };
	REQUIRE_FALSE(conditionsHold(d, second_fails));
	ValueLookup first_fails = { readFake, NULL, no_yes };
	REQUIRE_FALSE(conditionsHold(d, first_fails));

	ValueLookup neither = { readFake, NULL, no_no };
	REQUIRE_FALSE(conditionsHold(d, neither));
}

TEST_CASE("each operator answers against the value its own condition carries", "[schema]")
{
	FakeSetting t[] = { { "a", 5 }, { NULL, 0 } };
	ValueLookup lookup = { readFake, NULL, t };

	Condition c[] = { { "a", CompareOp::Eq, 5, NULL, 0, NULL, NULL, 0 } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(c), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };

	REQUIRE(conditionsHold(d, lookup));
	c[0].value = 4;
	REQUIRE_FALSE(conditionsHold(d, lookup));

	c[0].op = CompareOp::Ne;
	REQUIRE(conditionsHold(d, lookup));
	c[0].value = 5;
	REQUIRE_FALSE(conditionsHold(d, lookup));

	// Written the other way round these two would answer the opposite, so they
	// pin the order of the comparison as well as its strictness.
	c[0].op = CompareOp::Lt;
	c[0].value = 6;
	REQUIRE(conditionsHold(d, lookup));
	c[0].value = 5;
	REQUIRE_FALSE(conditionsHold(d, lookup));

	c[0].op = CompareOp::Gt;
	c[0].value = 4;
	REQUIRE(conditionsHold(d, lookup));
	c[0].value = 5;
	REQUIRE_FALSE(conditionsHold(d, lookup));
}

TEST_CASE("In holds for any listed value and for no other", "[schema]")
{
	Condition c[] = { { "mode", CompareOp::In, 0, COREAPI_VALUES(kModes), NULL, NULL, 0 } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(c), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };

	FakeSetting first[] = { { "mode", 2 }, { NULL, 0 } };
	FakeSetting middle[] = { { "mode", 5 }, { NULL, 0 } };
	FakeSetting last[] = { { "mode", 9 }, { NULL, 0 } };
	FakeSetting between[] = { { "mode", 4 }, { NULL, 0 } };

	ValueLookup at_first = { readFake, NULL, first };
	ValueLookup at_middle = { readFake, NULL, middle };
	ValueLookup at_last = { readFake, NULL, last };
	ValueLookup at_none = { readFake, NULL, between };

	REQUIRE(conditionsHold(d, at_first));
	REQUIRE(conditionsHold(d, at_middle));
	REQUIRE(conditionsHold(d, at_last));
	REQUIRE_FALSE(conditionsHold(d, at_none));

	// In reads the list and never the single value, which is set here to the
	// one number the list does not hold.
	c[0].value = 4;
	REQUIRE_FALSE(conditionsHold(d, at_none));
	REQUIRE(conditionsHold(d, at_first));
}

TEST_CASE("a key the lookup does not know leaves the setting shown", "[schema]")
{
	Condition c[] = { { "absent", CompareOp::Eq, 1, NULL, 0, NULL, NULL, 0 } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(c), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };

	FakeSetting without[] = { { "other", 0 }, { NULL, 0 } };
	ValueLookup cannot_answer = { readFake, NULL, without };
	REQUIRE(conditionsHold(d, cannot_answer));

	// The same condition is genuinely false once the key can be read, so the
	// answer above is the unknown key and not a condition that holds anyway.
	FakeSetting with[] = { { "absent", 0 }, { NULL, 0 } };
	ValueLookup can_answer = { readFake, NULL, with };
	REQUIRE_FALSE(conditionsHold(d, can_answer));
}

TEST_CASE("a lookup that reads nothing leaves every setting shown", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(kTwoConditions), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	ValueLookup none = { NULL, NULL, NULL };
	REQUIRE(conditionsHold(d, none));
}

TEST_CASE("a count whose conditions are absent is shown rather than walked", "[schema]")
{
	FakeSetting t[] = { { "other", 0 }, { NULL, 0 } };
	ValueLookup lookup = { readFake, NULL, t };

	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, NULL, 2, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(conditionsHold(d, lookup));
}

TEST_CASE("an In condition whose list is absent is shown rather than walked", "[schema]")
{
	FakeSetting t[] = { { "mode", 4 }, { NULL, 0 } };
	ValueLookup lookup = { readFake, NULL, t };

	Condition c[] = { { "mode", CompareOp::In, 0, NULL, 3, NULL, NULL, 0 } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(c), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(conditionsHold(d, lookup));
}

TEST_CASE("an operator that is none of the five leaves the setting shown", "[schema]")
{
	FakeSetting t[] = { { "a", 5 }, { NULL, 0 } };
	ValueLookup lookup = { readFake, NULL, t };

	Condition c[] = { { "a", static_cast<CompareOp>(99), 1, NULL, 0, NULL, NULL, 0 } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(c), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(conditionsHold(d, lookup));
}

TEST_CASE("the closed comparisons hold at the bound where the strict ones do not", "[schema]")
{
	FakeSetting t[] = { { "a", 5 }, { NULL, 0 } };
	ValueLookup lookup = { readFake, NULL, t };

	Condition c[] = { { "a", CompareOp::Lt, 5, NULL, 0, NULL, NULL, 0 } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(c), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };

	// At the bound itself the strict one refuses and the closed one holds. That
	// difference is the whole reason both exist.
	REQUIRE_FALSE(conditionsHold(d, lookup));
	c[0].op = CompareOp::Le;
	REQUIRE(conditionsHold(d, lookup));

	c[0].op = CompareOp::Gt;
	REQUIRE_FALSE(conditionsHold(d, lookup));
	c[0].op = CompareOp::Ge;
	REQUIRE(conditionsHold(d, lookup));

	// Away from the bound each closed one answers like its strict partner, and
	// in the same direction, which is what these four pin.
	c[0].value = 4;
	c[0].op = CompareOp::Le;
	REQUIRE_FALSE(conditionsHold(d, lookup));
	c[0].op = CompareOp::Ge;
	REQUIRE(conditionsHold(d, lookup));

	c[0].value = 6;
	c[0].op = CompareOp::Le;
	REQUIRE(conditionsHold(d, lookup));
	c[0].op = CompareOp::Ge;
	REQUIRE_FALSE(conditionsHold(d, lookup));
}

TEST_CASE("a closed range is two conditions and needs no arithmetic on a bound", "[schema]")
{
	Condition range[] = {
		{ "mode", CompareOp::Gt, 0, NULL, 0, NULL, NULL, 0 },
		{ "mode", CompareOp::Le, 3, NULL, 0, NULL, NULL, 0 }
	};
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(range), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));

	FakeSetting below[] = { { "mode", 0 }, { NULL, 0 } };
	FakeSetting at_floor[] = { { "mode", 1 }, { NULL, 0 } };
	FakeSetting inside[] = { { "mode", 2 }, { NULL, 0 } };
	FakeSetting at_ceiling[] = { { "mode", 3 }, { NULL, 0 } };
	FakeSetting above[] = { { "mode", 4 }, { NULL, 0 } };

	ValueLookup l_below = { readFake, NULL, below };
	ValueLookup l_at_floor = { readFake, NULL, at_floor };
	ValueLookup l_inside = { readFake, NULL, inside };
	ValueLookup l_at_ceiling = { readFake, NULL, at_ceiling };
	ValueLookup l_above = { readFake, NULL, above };

	REQUIRE_FALSE(conditionsHold(d, l_below));
	REQUIRE(conditionsHold(d, l_at_floor));
	REQUIRE(conditionsHold(d, l_inside));
	REQUIRE(conditionsHold(d, l_at_ceiling));
	REQUIRE_FALSE(conditionsHold(d, l_above));
}

TEST_CASE("a key named by two conditions is read once for each of them", "[schema]")
{
	Condition twice[] = {
		{ "mode", CompareOp::Gt, 0, NULL, 0, NULL, NULL, 0 },
		{ "mode", CompareOp::Le, 3, NULL, 0, NULL, NULL, 0 }
	};
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(twice), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };

	FakeSetting t[] = { { "mode", 2 }, { NULL, 0 } };
	ValueLookup lookup = { readFake, NULL, t };

	reads = 0;
	REQUIRE(conditionsHold(d, lookup));
	REQUIRE(reads == 2);
}

TEST_CASE("a text condition holds for a real key and not for the placeholder", "[schema]")
{
	static const Condition c[] = { { "k_key", CompareOp::TextValid, 0, NULL, 0, "XXXX", NULL, 0 } };
	Descriptor d = rowWithConditions(c, 1);
	REQUIRE(conditionsHold(d, textLookup("k_key", "abcd")));
	REQUIRE_FALSE(conditionsHold(d, textLookup("k_key", "XXXX")));
	REQUIRE_FALSE(conditionsHold(d, textLookup("k_key", "")));
}

TEST_CASE("a text condition nobody can read text for leaves the setting shown", "[schema]")
{
	static const Condition c[] = { { "k_key", CompareOp::TextValid, 0, NULL, 0, "XXXX", NULL, 0 } };
	Descriptor d = rowWithConditions(c, 1);

	// A lookup that knows numbers only, and one that knows the text of another key.
	REQUIRE(conditionsHold(d, numberLookup({{"k_key", 0}})));
	REQUIRE(conditionsHold(d, textLookup("other", "")));
}

TEST_CASE("a text condition without a placeholder asks only for some text", "[schema]")
{
	static const Condition none[] = { { "k_key", CompareOp::TextValid, 0, NULL, 0, NULL, NULL, 0 } };
	Descriptor d = rowWithConditions(none, 1);
	REQUIRE(conditionsHold(d, textLookup("k_key", "abcd")));
	REQUIRE_FALSE(conditionsHold(d, textLookup("k_key", "")));

	static const Condition empty[] = { { "k_key", CompareOp::TextValid, 0, NULL, 0, "", NULL, 0 } };
	Descriptor e = rowWithConditions(empty, 1);
	REQUIRE(conditionsHold(e, textLookup("k_key", "abcd")));
	REQUIRE_FALSE(conditionsHold(e, textLookup("k_key", "")));
}

TEST_CASE("a text condition without its placeholder is not sane", "[schema]")
{
	Condition c[] = { { "k_key", CompareOp::TextValid, 0, NULL, 0, NULL, NULL, 0 } };
	Descriptor d = rowWithConditions(c, 1);
	REQUIRE_FALSE(descriptorIsSane(d));

	c[0].text = "XXXX";
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("an or group holds when one member holds", "[schema]")
{
	static const Condition any[] = {
		{ "a", CompareOp::Ne, 0, NULL, 0, NULL, NULL, 0 },
		{ "b", CompareOp::Ne, 0, NULL, 0, NULL, NULL, 0 } };
	static const Condition c[] = { { NULL, CompareOp::Eq, 0, NULL, 0, NULL, COREAPI_ANY(any) } };
	Descriptor d = rowWithConditions(c, 1);
	REQUIRE(conditionsHold(d, numberLookup({{"a",0},{"b",1}})));
	REQUIRE_FALSE(conditionsHold(d, numberLookup({{"a",0},{"b",0}})));
}

TEST_CASE("an or group is one entry of a list that stays a conjunction", "[schema]")
{
	static const Condition any[] = {
		{ "a", CompareOp::Ne, 0, NULL, 0, NULL, NULL, 0 },
		{ "b", CompareOp::Ne, 0, NULL, 0, NULL, NULL, 0 } };
	static const Condition c[] = {
		{ NULL, CompareOp::Eq, 0, NULL, 0, NULL, COREAPI_ANY(any) },
		{ "on", CompareOp::Ne, 0, NULL, 0, NULL, NULL, 0 } };
	Descriptor d = rowWithConditions(c, 2);
	REQUIRE(descriptorIsSane(d));
	REQUIRE(conditionsHold(d, numberLookup({{"a",1},{"b",0},{"on",1}})));
	REQUIRE_FALSE(conditionsHold(d, numberLookup({{"a",1},{"b",0},{"on",0}})));
	REQUIRE_FALSE(conditionsHold(d, numberLookup({{"a",0},{"b",0},{"on",1}})));
}

TEST_CASE("an or group member nobody can answer for leaves the setting shown", "[schema]")
{
	static const Condition any[] = {
		{ "a", CompareOp::Ne, 0, NULL, 0, NULL, NULL, 0 },
		{ "absent", CompareOp::Ne, 0, NULL, 0, NULL, NULL, 0 } };
	static const Condition c[] = { { NULL, CompareOp::Eq, 0, NULL, 0, NULL, COREAPI_ANY(any) } };
	Descriptor d = rowWithConditions(c, 1);
	REQUIRE(conditionsHold(d, numberLookup({{"a",0}})));
	REQUIRE_FALSE(conditionsHold(d, numberLookup({{"a",0},{"absent",0}})));
}

TEST_CASE("an or group with no members, a key or a group inside it is not sane", "[schema]")
{
	Condition inner[] = { { "x", CompareOp::Ne, 0, NULL, 0, NULL, NULL, 0 } };
	Condition any[] = {
		{ "a", CompareOp::Ne, 0, NULL, 0, NULL, NULL, 0 },
		{ "b", CompareOp::Ne, 0, NULL, 0, NULL, NULL, 0 } };
	Condition c[] = { { NULL, CompareOp::Eq, 0, NULL, 0, NULL, COREAPI_ANY(any) } };
	Descriptor d = rowWithConditions(c, 1);
	REQUIRE(descriptorIsSane(d));

	c[0].any_count = 0;
	REQUIRE_FALSE(descriptorIsSane(d));
	c[0].any_count = 2;

	c[0].any_of = NULL;
	REQUIRE_FALSE(descriptorIsSane(d));
	c[0].any_of = any;

	c[0].key = "a";
	REQUIRE_FALSE(descriptorIsSane(d));
	c[0].key = NULL;

	// A member is held to every rule a comparison is held to.
	any[1].key = NULL;
	REQUIRE_FALSE(descriptorIsSane(d));
	any[1].key = "b";

	any[1].op = CompareOp::TextValid;
	REQUIRE_FALSE(descriptorIsSane(d));
	any[1].op = CompareOp::Ne;

	// Refused as a group even where it would pass as a comparison.
	any[1].any_of = inner;
	any[1].any_count = 1;
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("a group inside a group is passed over by the evaluator rather than walked", "[schema]")
{
	static const Condition inner[] = { { "x", CompareOp::Ne, 0, NULL, 0, NULL, NULL, 0 } };
	static const Condition any[] = {
		{ "a", CompareOp::Ne, 0, NULL, 0, NULL, NULL, 0 },
		{ NULL, CompareOp::Eq, 0, NULL, 0, NULL, COREAPI_ANY(inner) } };
	static const Condition c[] = { { NULL, CompareOp::Eq, 0, NULL, 0, NULL, COREAPI_ANY(any) } };
	Descriptor d = rowWithConditions(c, 1);
	REQUIRE(conditionsHold(d, numberLookup({{"a",0},{"x",0}})));
}

namespace
{
long sevenOnThisBox() { return 7; }
} // namespace

/* The function is the row's number where the box differs, so a row asks for it
   through one call and the constant is only the answer where there is none. */
TEST_CASE("a default function answers in place of the constant and only for numbers", "[schema]")
{
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 0, 9, NULL, 0, 3, NULL, false, false,
			 COREAPI_ALWAYS, kNumber, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(d));
	REQUIRE(defaultInt(d) == 3);

	d.default_fn = sevenOnThisBox;
	REQUIRE(descriptorIsSane(d));
	REQUIRE(defaultInt(d) == 7);

	// Sanity reads the constant, so a table is checked without a box to ask.
	d.default_int = 12;
	REQUIRE_FALSE(descriptorIsSane(d));

	Descriptor t = { "k", ValueType::String, "s", "l", "h", 0, 0, NULL, 0, 0, "x", false, false,
			 COREAPI_ALWAYS, kText, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };
	REQUIRE(descriptorIsSane(t));
	t.default_fn = sevenOnThisBox;
	REQUIRE_FALSE(descriptorIsSane(t));
}

TEST_CASE("a text rule has to be possible and its row's default has to keep it", "[schema]")
{
	const TextRule backwards = { TextKind::Plain, 5, 3, NULL, MustExist::No, NULL, false };
	const TextRule digits = { TextKind::Pin, 4, 4, "0123456789", MustExist::No, NULL, false };
	Descriptor d = { "k", ValueType::String, "s", "l", "h", 0, 0, NULL, 0, 0, "0000", false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, false, NULL };

	d.text = &digits;
	REQUIRE(descriptorIsSane(d));
	d.default_string = "12";
	REQUIRE_FALSE(descriptorIsSane(d));
	d.default_string = "abcd";
	REQUIRE_FALSE(descriptorIsSane(d));
	// An empty default is no value and stays allowed.
	d.default_string = "";
	REQUIRE(descriptorIsSane(d));

	d.text = &backwards;
	d.default_string = "";
	REQUIRE_FALSE(descriptorIsSane(d));
}
