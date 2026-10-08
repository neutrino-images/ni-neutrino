/*
 * settingsfield.h - one settings field: its type, range, and where it lives
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

#ifndef __coreapi_settingsfield_h__
#define __coreapi_settingsfield_h__

#include "coreapi/base/schema.h"

#include <stdint.h>
#include <stdio.h>

#include <list>
#include <string>
#include <type_traits>
#include <vector>

#include <system/settings.h>

/* What a table row writes to say where its setting lives. Separate from the
   header that declares the row, because this one carries the program's whole
   settings struct with it.

   The pair of functions a row gets is generated for the type of the field it
   names, which makes a wrong pairing a compiler error rather than a value read
   as nonsense. The build refuses one of each, see
   test/unit/scan/check-fieldtypes.sh. */

namespace coreapi
{

// An int member hands out its address, any other type none.
template <typename T, T SNeutrinoSettings::*M>
struct IntPointer
{
	static constexpr int *(*get)(SNeutrinoSettings &) = NULL;
};
template <int SNeutrinoSettings::*M>
struct IntPointer<int, M>
{
	static int *at(SNeutrinoSettings &s) { return &(s.*M); }
	static constexpr int *(*get)(SNeutrinoSettings &) = &at;
};
template <typename T, T SNeutrinoSettings::*M>
constexpr int *(*IntPointer<T, M>::get)(SNeutrinoSettings &);
template <int SNeutrinoSettings::*M>
constexpr int *(*IntPointer<int, M>::get)(SNeutrinoSettings &);

// The struct keeps its numbers in seven different types, so a row says whether
// a value fits the one it names before anything stores it.
template <typename T, T SNeutrinoSettings::*M>
struct NumberField
{
	/* A value travels through this layer as a long, four bytes wide on the box,
	   so a field wider than that is one no table compiled for both can carry.
	   The struct has three of those and they are channel ids; two are carried
	   as text instead, see ChannelIdField below, and the third is an array no
	   single key names. */
	static_assert(sizeof(T) <= sizeof(int32_t),
		      "a settings field wider than a long on the box cannot be carried");

	static long read(const SNeutrinoSettings &s) { return (long) (s.*M); }
	static void write(SNeutrinoSettings &s, long v) { s.*M = (T) v; }

	static bool fits(long v)
	{
		T narrowed = (T) v;
		return (long) narrowed == v;
	}

	static constexpr int *(*pointer)(SNeutrinoSettings &) = IntPointer<T, M>::get;
};
template <typename T, T SNeutrinoSettings::*M>
constexpr int *(*NumberField<T, M>::pointer)(SNeutrinoSettings &);

/* One bit of a field beside it. The screen offers the three bits of one mask as
   three questions and folds them back as it leaves.

   The bits the mask does not name are left where they are: two rows over one
   field are two settings, and a write of one that cleared the other would be a
   setting changing a setting nobody asked about. */
template <typename T, T SNeutrinoSettings::*M, unsigned long Mask>
struct MaskBitField
{
	static_assert(sizeof(T) <= sizeof(int32_t),
		      "a settings field wider than a long on the box cannot be carried");
	static_assert(Mask != 0, "a bit of a mask is not the absence of one");

	static long read(const SNeutrinoSettings &s)
	{
		return (((unsigned long) (s.*M) & Mask) != 0) ? 1 : 0;
	}

	static void write(SNeutrinoSettings &s, long v)
	{
		const unsigned long held = (unsigned long) (s.*M);
		s.*M = (T) (v != 0 ? (held | Mask) : (held & ~Mask));
	}

	// The two the bit has, and not what the field behind it holds: the field is
	// the whole mask and would take every value a byte has.
	static bool fits(long v) { return v == 0 || v == 1; }
};

/* Read and written under the lock the struct declares for its text, because
   neither side of this pair is on the box's own loop with the screens that
   write the same member: the read is served on one of the web server's threads
   and the write runs from the loop with a request already answered. */
template <std::string SNeutrinoSettings::*M>
struct TextField
{
	static void read(const SNeutrinoSettings &s, std::string &out) { out = settingsText(s.*M); }
	static void write(SNeutrinoSettings &s, const std::string &v) { setSettingsText(s.*M, v); }
};

/* A sixty four bit identifier the struct holds, carried as the text a channel is
   named by everywhere else in this layer. Text and not a number, because the
   long a number travels in is four bytes wide on the box.

   A spelling the reader refuses leaves the field standing. What keeps one from
   ever arriving here is the rule the write is held to before the store is
   touched; this runs later, on another thread, and has nobody left to answer.*/
template <typename T, T SNeutrinoSettings::*M>
struct ChannelIdField
{
	static_assert(sizeof(T) == 8, "a channel identifier is sixty four bits wide");

	static void read(const SNeutrinoSettings &s, std::string &out)
	{
		char buf[24];
		snprintf(buf, sizeof(buf), "%llx", (unsigned long long) (s.*M));
		out = buf;
	}

	static void write(SNeutrinoSettings &s, const std::string &v)
	{
		unsigned long long id = 0;
		if (!readChannelIdText(v, id))
			return;
		s.*M = (T) id;
	}
};


/* The members of the settings struct that are structs themselves, and the arrays
   and lists in it. Each is reached through the member that holds it, named in
   the template so that a row cannot name one and read another. */

// A number member of a struct the settings hold, reached through the member that holds it.
template <typename O, O SNeutrinoSettings::*Outer, typename T, T O::*M>
struct NestedNumberField
{
	static_assert(sizeof(T) <= sizeof(int32_t),
		      "a settings field wider than a long on the box cannot be carried");

	static long read(const SNeutrinoSettings &s) { return (long) ((s.*Outer).*M); }
	static void write(SNeutrinoSettings &s, long v) { (s.*Outer).*M = (T) v; }

	static bool fits(long v)
	{
		T narrowed = (T) v;
		return (long) narrowed == v;
	}
};

// Handed out for an int member only, as IntPointer does for a member of the struct itself.
template <typename O, O SNeutrinoSettings::*Outer, typename T, T O::*M>
struct NestedIntPointer
{
	static constexpr int *(*get)(SNeutrinoSettings &) = NULL;
};
template <typename O, O SNeutrinoSettings::*Outer, int O::*M>
struct NestedIntPointer<O, Outer, int, M>
{
	static int *at(SNeutrinoSettings &s) { return &((s.*Outer).*M); }
	static constexpr int *(*get)(SNeutrinoSettings &) = &at;
};
template <typename O, O SNeutrinoSettings::*Outer, typename T, T O::*M>
constexpr int *(*NestedIntPointer<O, Outer, T, M>::get)(SNeutrinoSettings &);
template <typename O, O SNeutrinoSettings::*Outer, int O::*M>
constexpr int *(*NestedIntPointer<O, Outer, int, M>::get)(SNeutrinoSettings &);

// A text member of such a struct, under the lock the struct declares for its text.
template <typename O, O SNeutrinoSettings::*Outer, std::string O::*M>
struct NestedTextField
{
	static void read(const SNeutrinoSettings &s, std::string &out) { out = settingsText((s.*Outer).*M); }
	static void write(SNeutrinoSettings &s, const std::string &v) { setSettingsText((s.*Outer).*M, v); }
};

/* One element of an array member. The index is a template argument so that one
   past the end does not compile: a row for an element nothing has would read
   whatever follows the array. */
template <typename A, A SNeutrinoSettings::*M, size_t I>
struct ElementNumber
{
	typedef typename std::remove_extent<A>::type T;
	static_assert(std::is_array<A>::value, "an element row names an array");
	static_assert(I < std::extent<A>::value, "the element is past the end of the array");
	static_assert(sizeof(T) <= sizeof(int32_t),
		      "a settings field wider than a long on the box cannot be carried");

	static long read(const SNeutrinoSettings &s) { return (long) (s.*M)[I]; }
	static void write(SNeutrinoSettings &s, long v) { (s.*M)[I] = (T) v; }

	static bool fits(long v)
	{
		T narrowed = (T) v;
		return (long) narrowed == v;
	}

	static constexpr FieldExtra extra = { (long) I, NULL, NULL, NULL, NULL, NULL, 0, std::extent<A>::value, false };
};
template <typename A, A SNeutrinoSettings::*M, size_t I>
constexpr FieldExtra ElementNumber<A, M, I>::extra;

// The element's address for an int array only, as IntPointer does for a member.
template <typename A, A SNeutrinoSettings::*M, size_t I,
	  typename T = typename std::remove_extent<A>::type>
struct ElementInt
{
	static constexpr int *(*get)(SNeutrinoSettings &) = NULL;
};
template <typename A, A SNeutrinoSettings::*M, size_t I>
struct ElementInt<A, M, I, int>
{
	static int *at(SNeutrinoSettings &s) { return &(s.*M)[I]; }
	static constexpr int *(*get)(SNeutrinoSettings &) = &at;
};
template <typename A, A SNeutrinoSettings::*M, size_t I, typename T>
constexpr int *(*ElementInt<A, M, I, T>::get)(SNeutrinoSettings &);
template <typename A, A SNeutrinoSettings::*M, size_t I>
constexpr int *(*ElementInt<A, M, I, int>::get)(SNeutrinoSettings &);

template <typename A, A SNeutrinoSettings::*M, size_t I>
struct ElementText
{
	static_assert(std::is_array<A>::value, "an element row names an array");
	static_assert(I < std::extent<A>::value, "the element is past the end of the array");

	static void read(const SNeutrinoSettings &s, std::string &out) { out = settingsText((s.*M)[I]); }
	static void write(SNeutrinoSettings &s, const std::string &v) { setSettingsText((s.*M)[I], v); }

	static constexpr FieldExtra extra = { (long) I, NULL, NULL, NULL, NULL, NULL, 0, std::extent<A>::value, false };
};
template <typename A, A SNeutrinoSettings::*M, size_t I>
constexpr FieldExtra ElementText<A, M, I>::extra;

template <typename A, A SNeutrinoSettings::*M, size_t I>
struct ElementChannelId
{
	typedef typename std::remove_extent<A>::type T;
	static_assert(std::is_array<A>::value, "an element row names an array");
	static_assert(I < std::extent<A>::value, "the element is past the end of the array");
	static_assert(sizeof(T) == 8, "a channel identifier is sixty four bits wide");

	static void read(const SNeutrinoSettings &s, std::string &out)
	{
		char buf[24];
		snprintf(buf, sizeof(buf), "%llx", (unsigned long long) (s.*M)[I]);
		out = buf;
	}

	static void write(SNeutrinoSettings &s, const std::string &v)
	{
		unsigned long long id = 0;
		if (!readChannelIdText(v, id))
			return;
		(s.*M)[I] = (T) id;
	}

	static constexpr FieldExtra extra = { (long) I, NULL, NULL, NULL, NULL, NULL, 0, std::extent<A>::value, false };
};
template <typename A, A SNeutrinoSettings::*M, size_t I>
constexpr FieldExtra ElementChannelId<A, M, I>::extra;

/* A list of texts the struct keeps. Copied out and in whole, under the lock the
   struct's texts are read under, because a reference into the list would outlive
   it. */
template <std::list<std::string> SNeutrinoSettings::*M>
struct ListField
{
	static void read(const SNeutrinoSettings &s, std::vector<std::string> &out)
	{
		CSettingsTextGuard lock;
		out.assign((s.*M).begin(), (s.*M).end());
	}

	static void write(SNeutrinoSettings &s, const std::vector<std::string> &in)
	{
		CSettingsTextGuard lock;
		(s.*M).assign(in.begin(), in.end());
	}

	static constexpr FieldExtra extra = { 0, &read, &write, NULL, NULL, NULL, 0, 0, false };
};
template <std::list<std::string> SNeutrinoSettings::*M>
constexpr FieldExtra ListField<M>::extra;

/* The fourth channel of a colour, or none. A row of three channels names the
   red member in its place, so one template serves both and the member of a
   colour without an alpha is never named. */
template <bool Has, typename G, unsigned char G::*A>
struct AlphaChannel
{
	static const size_t count = 3;
	static unsigned char get(const G &) { return 0; }
	static void put(G &, unsigned char) {}
};
template <typename G, unsigned char G::*A>
struct AlphaChannel<true, G, A>
{
	static const size_t count = 4;
	static unsigned char get(const G &g) { return g.*A; }
	static void put(G &g, unsigned char v) { g.*A = v; }
};

/* One colour of a struct the settings hold, as the text of its channels. The
   members are the screens' own bytes, each a step from 0 to 100, so a value
   that does not read as a colour of this many channels leaves them as they
   were: the rule that refuses one runs before the write, and this has nobody
   left to answer. A row of three channels never touches the alpha member the
   struct may have beside it. */
template <typename G, G SNeutrinoSettings::*Group,
          unsigned char G::*R, unsigned char G::*Gr, unsigned char G::*B,
          bool Has, unsigned char G::*A>
struct ColorField
{
	typedef AlphaChannel<Has, G, A> Alpha;

	static void read(const SNeutrinoSettings &s, std::string &out)
	{
		const G &g = s.*Group;
		const unsigned char steps[4] = { g.*R, g.*Gr, g.*B, Alpha::get(g) };
		out = colorText(steps, Alpha::count);
	}

	static void write(SNeutrinoSettings &s, const std::string &v)
	{
		unsigned char steps[4] = { 0, 0, 0, 0 };
		if (!readColorText(v, Alpha::count, steps))
			return;
		G &g = s.*Group;
		g.*R = steps[0];
		g.*Gr = steps[1];
		g.*B = steps[2];
		Alpha::put(g, steps[3]);
	}
};

} // namespace coreapi

// decltype rather than a type the row spells out, so a row cannot name a field
// and then claim it is of a type it is not. The name comes from the same
// argument as the functions, so a row cannot point at one field and be checked
// against another.
#define COREAPI_NUMBER_FIELD(f) COREAPI_NUMBER_FIELD_ON(f, NULL, NULL)

/* The same field for a setting not every box has in one shape: a is the test
   for whether the box has what the setting controls, o the shape the setting
   takes where a says no, NULL where it is then not on the box at all. */
#define COREAPI_NUMBER_FIELD_ON(f, a, o) \
	{ &coreapi::NumberField<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f>::read, \
	  &coreapi::NumberField<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f>::write, \
	  coreapi::NumberField<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f>::pointer, \
	  &coreapi::NumberField<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f>::fits, \
	  NULL, NULL, NULL, NULL, #f, coreapi::FieldOrigin::Member, (a), (o), NULL }

#define COREAPI_TEXT_FIELD(f) COREAPI_TEXT_FIELD_ON(f, NULL, NULL)

// The same for text not every box has, as COREAPI_NUMBER_FIELD_ON.
#define COREAPI_TEXT_FIELD_ON(f, a, o) \
	{ NULL, NULL, NULL, NULL, \
	  &coreapi::TextField<&SNeutrinoSettings::f>::read, \
	  &coreapi::TextField<&SNeutrinoSettings::f>::write, \
	  NULL, NULL, #f, coreapi::FieldOrigin::Member, (a), (o), NULL }

/* f is the member the screen binds the question to and the name this row is
   found under, m is the mask the value really lives in, and b is the bit of it.
   The row needs both: the value is stored in the mask, and what the screens say
   about this question they say beside f. */
#define COREAPI_MASK_BIT_FIELD(f, m, b) \
	{ &coreapi::MaskBitField<decltype(SNeutrinoSettings::m), &SNeutrinoSettings::m, (b)>::read, \
	  &coreapi::MaskBitField<decltype(SNeutrinoSettings::m), &SNeutrinoSettings::m, (b)>::write, \
	  NULL, \
	  &coreapi::MaskBitField<decltype(SNeutrinoSettings::m), &SNeutrinoSettings::m, (b)>::fits, \
	  NULL, NULL, NULL, NULL, \
	  (sizeof(&SNeutrinoSettings::f) > 0 ? #f : #f), coreapi::FieldOrigin::MaskBit, NULL, NULL, NULL }

#define COREAPI_CHANNEL_ID_FIELD(f) \
	{ NULL, NULL, NULL, NULL, \
	  &coreapi::ChannelIdField<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f>::read, \
	  &coreapi::ChannelIdField<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f>::write, \
	  NULL, NULL, #f, coreapi::FieldOrigin::ChannelIdField, NULL, NULL, NULL }

/* A value a daemon holds. a asks it and t tells it, and f is the member the
   program keeps as the screen's buffer for that value, named so the checks
   outside the compiler can find what the screens say about it.

   The member is named in an unevaluated sizeof, which holds the name to a real
   member without reading one: a name that is not a member forms no pointer to
   member and does not compile, and nothing here is called, so a table of these
   stays a constant. */
#define COREAPI_SERVICE_FIELD(f, a, t) \
	{ NULL, NULL, NULL, NULL, NULL, NULL, (a), (t), \
	  (sizeof(&SNeutrinoSettings::f) > 0 ? #f : #f), coreapi::FieldOrigin::Service, NULL, NULL, NULL }

/* A number of one of the structs the settings hold, written under the key the
   settings file gives it. The field is named theme.m or glcd_theme.m, which is
   what a check outside the compiler finds it under. */
#define COREAPI_NESTED_NUMBER_FIELD(outer, type, prefix, m) \
	{ &coreapi::NestedNumberField<type, &SNeutrinoSettings::outer, decltype(type::m), &type::m>::read, \
	  &coreapi::NestedNumberField<type, &SNeutrinoSettings::outer, decltype(type::m), &type::m>::write, \
	  coreapi::NestedIntPointer<type, &SNeutrinoSettings::outer, decltype(type::m), &type::m>::get, \
	  &coreapi::NestedNumberField<type, &SNeutrinoSettings::outer, decltype(type::m), &type::m>::fits, \
	  NULL, NULL, NULL, NULL, prefix #m, coreapi::FieldOrigin::Member, NULL, NULL, NULL }

#define COREAPI_THEME_FIELD(m) COREAPI_NESTED_NUMBER_FIELD(theme, SNeutrinoTheme, "theme.", m)
#define COREAPI_GLCD_THEME_FIELD(m) \
	COREAPI_NESTED_NUMBER_FIELD(glcd_theme, SNeutrinoGlcdTheme, "glcd_theme.", m)

#define COREAPI_NESTED_TEXT_FIELD(outer, type, prefix, m) \
	{ NULL, NULL, NULL, NULL, \
	  &coreapi::NestedTextField<type, &SNeutrinoSettings::outer, &type::m>::read, \
	  &coreapi::NestedTextField<type, &SNeutrinoSettings::outer, &type::m>::write, \
	  NULL, NULL, prefix #m, coreapi::FieldOrigin::Member, NULL, NULL, NULL }

#define COREAPI_GLCD_THEME_TEXT_FIELD(m) \
	COREAPI_NESTED_TEXT_FIELD(glcd_theme, SNeutrinoGlcdTheme, "glcd_theme.", m)

/* One row to each element of an array member, f the array and i the element. The
   element is a setting of its own, so it is read and written as one. */
#define COREAPI_ELEMENT_FIELD(f, i) \
	{ &coreapi::ElementNumber<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f, (i)>::read, \
	  &coreapi::ElementNumber<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f, (i)>::write, \
	  coreapi::ElementInt<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f, (i)>::get, \
	  &coreapi::ElementNumber<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f, (i)>::fits, \
	  NULL, NULL, NULL, NULL, #f, coreapi::FieldOrigin::Element, NULL, NULL, \
	  &coreapi::ElementNumber<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f, (i)>::extra }

#define COREAPI_ELEMENT_TEXT_FIELD(f, i) \
	{ NULL, NULL, NULL, NULL, \
	  &coreapi::ElementText<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f, (i)>::read, \
	  &coreapi::ElementText<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f, (i)>::write, \
	  NULL, NULL, #f, coreapi::FieldOrigin::Element, NULL, NULL, \
	  &coreapi::ElementText<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f, (i)>::extra }

#define COREAPI_ELEMENT_CHANNEL_ID_FIELD(f, i) \
	{ NULL, NULL, NULL, NULL, \
	  &coreapi::ElementChannelId<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f, (i)>::read, \
	  &coreapi::ElementChannelId<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f, (i)>::write, \
	  NULL, NULL, #f, coreapi::FieldOrigin::Element, NULL, NULL, \
	  &coreapi::ElementChannelId<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f, (i)>::extra }

// A list of texts the struct keeps, f the member.
#define COREAPI_LIST_FIELD(f) \
	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, #f, coreapi::FieldOrigin::Member, NULL, NULL, \
	  &coreapi::ListField<&SNeutrinoSettings::f>::extra }

/* A list of records the struct keeps in a container of its own kind. extra is a
   constant the table states beside the row, carrying the pair of functions that
   copy the records out and in, one pair to each kind of record because no two
   are held alike, and what a record is made of, in the order those functions
   carry the members. The member is named in an unevaluated sizeof for the reason
   COREAPI_SERVICE_FIELD does. */
#define COREAPI_RECORDS_FIELD(f, extra) \
	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, \
	  (sizeof(&SNeutrinoSettings::f) > 0 ? #f : #f), coreapi::FieldOrigin::Member, NULL, NULL, \
	  &(extra) }

/* A member of the settings struct that is a list, an array of structs or the like,
   named for the list of what the tables leave out and for nothing else: it reads
   and writes nothing. The member is named in an unevaluated sizeof, which holds
   the name to a real member without reading one. */
#define COREAPI_AGGREGATE_FIELD(f) \
	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, \
	  (sizeof(&SNeutrinoSettings::f) > 0 ? #f : #f), coreapi::FieldOrigin::Nowhere, NULL, NULL, NULL }

/* A flag that is a file's existence. The path stands in the name, which is what
   the layer that holds the store reads it from. */
#define COREAPI_FLAG_FILE_FIELD(path) \
	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, path, coreapi::FieldOrigin::FlagFile, NULL, NULL, NULL }

/* A colour of the struct member named g, whose members are p_red, p_green,
   p_blue and, where has_alpha is the word true, p_alpha. The row is found under
   g.p, which is no member of the settings struct and is no key the settings
   file holds.

   The type of the group is named here and not taken from the member with
   decltype: a pointer to a member named through decltype inside a template
   argument list is not read the way the same name spelled out is. A group
   that names the wrong type does not compile, because the member pointer to it
   is of the other. */
#define COREAPI_COLOR_GROUP_theme SNeutrinoTheme
#define COREAPI_COLOR_GROUP_glcd_theme SNeutrinoGlcdTheme

#define COREAPI_COLOR_ALPHA_true(g, p) true, &COREAPI_COLOR_GROUP_##g::p##_alpha
#define COREAPI_COLOR_ALPHA_false(g, p) false, &COREAPI_COLOR_GROUP_##g::p##_red

#define COREAPI_COLOR_FIELD(g, p, has_alpha) \
	{ NULL, NULL, NULL, NULL, \
	  &coreapi::ColorField<COREAPI_COLOR_GROUP_##g, &SNeutrinoSettings::g, \
	                       &COREAPI_COLOR_GROUP_##g::p##_red, \
	                       &COREAPI_COLOR_GROUP_##g::p##_green, \
	                       &COREAPI_COLOR_GROUP_##g::p##_blue, \
	                       COREAPI_COLOR_ALPHA_##has_alpha(g, p)>::read, \
	  &coreapi::ColorField<COREAPI_COLOR_GROUP_##g, &SNeutrinoSettings::g, \
	                       &COREAPI_COLOR_GROUP_##g::p##_red, \
	                       &COREAPI_COLOR_GROUP_##g::p##_green, \
	                       &COREAPI_COLOR_GROUP_##g::p##_blue, \
	                       COREAPI_COLOR_ALPHA_##has_alpha(g, p)>::write, \
	  NULL, NULL, #g "." #p, coreapi::FieldOrigin::ColorBytes, NULL, NULL, NULL }

#endif
