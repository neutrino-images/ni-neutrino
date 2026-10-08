#!/bin/sh
# A row that names a field of the wrong sort has to stop the build, and the only
# way to know that it does is to write one and watch it fail. Two are written
# below, one of each direction, and this refuses a compiler that accepts either.
#
# The guarantee is the whole reason a row carries functions rather than an
# offset, so it is checked here rather than described in a comment.
set -e
CXX="$1"
shift

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/good.cpp" <<'PROBE'
#include "coreapi/settings/settingsfield.h"
using namespace coreapi;
// What a row of the kind whose value a daemon holds is written with. Nothing
// calls them; the probe is about what compiles.
bool probeAsk(long &) { return false; }
bool probeTell(long) { return false; }
const Descriptor row[] = {
	{ "k", ValueType::Int, "s", "l", NULL, 0, 2000, NULL, 0, 450, NULL, false, false,
	  COREAPI_ALWAYS, COREAPI_NUMBER_FIELD(repeat_blocker),
	  NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "t", ValueType::String, "s", "l", NULL, 0, 0, NULL, 0, 0, "", false, false,
	  COREAPI_ALWAYS, COREAPI_TEXT_FIELD(language),
	  NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "i", ValueType::String, "s", "l", NULL, 0, 0, NULL, 0, 0, "0", false, false,
	  COREAPI_ALWAYS, COREAPI_CHANNEL_ID_FIELD(startchanneltv_id),
	  NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "b", ValueType::Bool, "s", "l", NULL, 0, 1, NULL, 0, 0, NULL, false, false,
	  COREAPI_ALWAYS, COREAPI_MASK_BIT_FIELD(recording_audio_pids_std,
						 recording_audio_pids_default, 1),
	  NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "d", ValueType::Int, "s", "l", NULL, 0, 99, NULL, 0, 0, NULL, false, false,
	  COREAPI_ALWAYS, COREAPI_SERVICE_FIELD(record_safety_time_before, probeAsk, probeTell),
	  NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "c", ValueType::Color, "s", "l", NULL, 3, 4, NULL, 0, 0, "#00000000", false, false,
	  COREAPI_ALWAYS, COREAPI_COLOR_FIELD(theme, menu_Head, true),
	  NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "n", ValueType::Color, "s", "l", NULL, 3, 3, NULL, 0, 0, "#000000", false, false,
	  COREAPI_ALWAYS, COREAPI_COLOR_FIELD(theme, menu_Head_Text, false),
	  NULL, NULL, NULL, NULL, NULL, NULL, NULL },
};
// The members of the structs inside the settings, the elements of an array and a
// list, each written the way a row of its kind is.
const Descriptor nested[] = {
	{ "n", ValueType::Int, "s", "l", NULL, 0, 100, NULL, 0, 0, NULL, false, false,
	  COREAPI_ALWAYS, COREAPI_THEME_FIELD(menu_Head_alpha),
	  NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "g", ValueType::Int, "s", "l", NULL, 0, 100, NULL, 0, 0, NULL, false, false,
	  COREAPI_ALWAYS, COREAPI_GLCD_THEME_FIELD(glcd_channel_percent),
	  NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "gt", ValueType::String, "s", "l", NULL, 0, 0, NULL, 0, 0, "", false, false,
	  COREAPI_ALWAYS, COREAPI_GLCD_THEME_TEXT_FIELD(glcd_font),
	  NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "e", ValueType::Int, "s", "l", NULL, 0, 2, NULL, 0, 0, NULL, false, false,
	  COREAPI_ALWAYS, COREAPI_ELEMENT_FIELD(personalize, 0),
	  NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "et", ValueType::String, "s", "l", NULL, 0, 0, NULL, 0, 0, "", false, false,
	  COREAPI_ALWAYS, COREAPI_ELEMENT_TEXT_FIELD(pref_lang, 0),
	  NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "ei", ValueType::String, "s", "l", NULL, 0, 0, NULL, 0, 0, "0", false, false,
	  COREAPI_ALWAYS, COREAPI_ELEMENT_CHANNEL_ID_FIELD(quadpip_channel_id_window, 0),
	  NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "li", ValueType::List, "s", "l", NULL, 0, 0, NULL, 0, 0, "", false, false,
	  COREAPI_ALWAYS, COREAPI_LIST_FIELD(webtv_xml),
	  NULL, NULL, NULL, NULL, NULL, NULL, NULL },
};
const Descriptor *first() { return row; }
const Descriptor *second() { return nested; }
PROBE

# A number where the field is text, and text where the field is a number.
sed 's/COREAPI_NUMBER_FIELD(repeat_blocker)/COREAPI_NUMBER_FIELD(language)/' "$tmp/good.cpp" > "$tmp/number_over_text.cpp"
sed 's/COREAPI_TEXT_FIELD(language)/COREAPI_TEXT_FIELD(repeat_blocker)/' "$tmp/good.cpp" > "$tmp/text_over_number.cpp"
# And a field wider than the long a value travels in, which is the one kind of
# truncation that would show on the box and on nothing else.
sed 's/COREAPI_NUMBER_FIELD(repeat_blocker)/COREAPI_NUMBER_FIELD(startchanneltv_id)/' "$tmp/good.cpp" > "$tmp/wider_than_a_long.cpp"
# An identifier is sixty four bits, and a row naming a narrower field as one
# would answer half of something that is not an identifier at all.
sed 's/COREAPI_CHANNEL_ID_FIELD(startchanneltv_id)/COREAPI_CHANNEL_ID_FIELD(repeat_blocker)/' "$tmp/good.cpp" > "$tmp/id_over_a_narrow_field.cpp"
# The three kinds whose row names a member the value is not in still have to
# name a member: the name is what every check outside the compiler reads, and
# one that is not a member holds to nothing.
sed 's/COREAPI_MASK_BIT_FIELD(recording_audio_pids_std,/COREAPI_MASK_BIT_FIELD(no_such_member_4711,/' "$tmp/good.cpp" > "$tmp/mask_bit_names_nothing.cpp"
sed 's/COREAPI_SERVICE_FIELD(record_safety_time_before,/COREAPI_SERVICE_FIELD(no_such_member_4711,/' "$tmp/good.cpp" > "$tmp/service_names_nothing.cpp"

# A colour is named by the group and the prefix of its members, so a prefix that is
# none, and an alpha claimed for a colour whose struct has none, have to stop the
# build: the name is what every check outside the compiler reads.
sed 's/COREAPI_COLOR_FIELD(theme, menu_Head, true)/COREAPI_COLOR_FIELD(theme, no_such_color_4711, true)/' "$tmp/good.cpp" > "$tmp/color_names_nothing.cpp"
sed 's/COREAPI_COLOR_FIELD(theme, menu_Head_Text, false)/COREAPI_COLOR_FIELD(theme, progressbar_active, true)/' "$tmp/good.cpp" > "$tmp/color_claims_an_alpha_it_lacks.cpp"

# The struct has its two inner members on the builds that have the display and the
# four windows, so the probes are compiled for such a build whatever this one is.
PROBE_FLAGS="-DENABLE_GRAPHLCD=1 -DENABLE_QUADPIP=1"

if ! $CXX "$@" $PROBE_FLAGS -c -o "$tmp/out.o" "$tmp/good.cpp" > "$tmp/log" 2>&1; then
	echo "a row naming its own kind of field does not compile:" >&2
	cat "$tmp/log" >&2
	exit 1
fi

# The same kinds of mistake for the rows that name a part of a member. A number
# over a text and a text over a number, an element past the end of its array, an
# element of what is no array, a list over what is no list, and an identifier over
# an array of narrow numbers.
sed 's/COREAPI_GLCD_THEME_FIELD(glcd_channel_percent)/COREAPI_GLCD_THEME_FIELD(glcd_font)/' "$tmp/good.cpp" > "$tmp/nested_number_over_text.cpp"
sed 's/COREAPI_GLCD_THEME_TEXT_FIELD(glcd_font)/COREAPI_GLCD_THEME_TEXT_FIELD(glcd_channel_percent)/' "$tmp/good.cpp" > "$tmp/nested_text_over_number.cpp"
sed 's/COREAPI_THEME_FIELD(menu_Head_alpha)/COREAPI_THEME_FIELD(no_such_member_4711)/' "$tmp/good.cpp" > "$tmp/nested_names_nothing.cpp"
sed 's/COREAPI_ELEMENT_FIELD(personalize, 0)/COREAPI_ELEMENT_FIELD(personalize, 4711)/' "$tmp/good.cpp" > "$tmp/element_past_the_end.cpp"
sed 's/COREAPI_ELEMENT_FIELD(personalize, 0)/COREAPI_ELEMENT_FIELD(language, 0)/' "$tmp/good.cpp" > "$tmp/element_of_no_array.cpp"
sed 's/COREAPI_ELEMENT_FIELD(personalize, 0)/COREAPI_ELEMENT_FIELD(pref_lang, 0)/' "$tmp/good.cpp" > "$tmp/element_number_over_text.cpp"
sed 's/COREAPI_ELEMENT_TEXT_FIELD(pref_lang, 0)/COREAPI_ELEMENT_TEXT_FIELD(personalize, 0)/' "$tmp/good.cpp" > "$tmp/element_text_over_number.cpp"
sed 's/COREAPI_ELEMENT_CHANNEL_ID_FIELD(quadpip_channel_id_window, 0)/COREAPI_ELEMENT_CHANNEL_ID_FIELD(personalize, 0)/' "$tmp/good.cpp" > "$tmp/element_id_over_a_narrow_array.cpp"
sed 's/COREAPI_LIST_FIELD(webtv_xml)/COREAPI_LIST_FIELD(language)/' "$tmp/good.cpp" > "$tmp/list_over_no_list.cpp"

for bad in number_over_text text_over_number wider_than_a_long \
	   id_over_a_narrow_field mask_bit_names_nothing service_names_nothing \
	   color_names_nothing color_claims_an_alpha_it_lacks \
	   nested_number_over_text nested_text_over_number nested_names_nothing \
	   element_past_the_end element_of_no_array element_number_over_text \
	   element_text_over_number element_id_over_a_narrow_array list_over_no_list; do
	if $CXX "$@" $PROBE_FLAGS -c -o "$tmp/out.o" "$tmp/$bad.cpp" > "$tmp/log" 2>&1; then
		echo "a row naming a field of the wrong sort compiled: $bad" >&2
		exit 1
	fi
done
exit 0
