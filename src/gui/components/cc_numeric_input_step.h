/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Stepping the digit under the cursor of a numeric input
	Copyright (C) 2026

	License: GPL
*/

#ifndef __CC_NUMERIC_INPUT_STEP_H__
#define __CC_NUMERIC_INPUT_STEP_H__

#include <stddef.h>
#include <string>

/* Header only and free of widgets so a test can hold the stepping without a
   screen. */

enum cc_digit_step_t
{
	///no digit under the cursor, or the text is no number: nothing to step
	CC_DIGIT_STEP_NONE = 0,
	///the step would leave the allowed values; text and cursor stay
	CC_DIGIT_STEP_REFUSED,
	///text and cursor hold the stepped number
	CC_DIGIT_STEP_DONE
};

/**
 * Counts the digit under the cursor one up or down, as the old string
 * input did with up and down - but as a place of the number, not as a
 * character of its own: the tens of "095" go up to "105", where the old
 * input wrapped to "005".
 *
 * text is a whole number of at most 18 digits with an optional minus in
 * front, cursor the index of a character in it. The number keeps its
 * width, leading zeros and all, and grows only by the digit a carry
 * adds; the cursor stays on the place it stepped, whatever happens to
 * the sign and the width in front of it.
 *
 * A step that would pass highest on the way up or lowest on the way
 * down is refused whole. The value is not clamped: stopping at 999 on
 * the hundreds of "950" would change places the cursor is not on. A
 * value outside the limits may still step towards them.
 */
inline cc_digit_step_t ccStepDigit(std::string &text, size_t &cursor, bool up,
	long long lowest, long long highest)
{
	const size_t first = (!text.empty() && text[0] == '-') ? 1 : 0;
	const size_t digits = text.size() - first;

	if (digits == 0 || digits > 18 || cursor < first || cursor >= text.size())
		return CC_DIGIT_STEP_NONE;

	long long value = 0;
	for (size_t i = first; i < text.size(); i++)
	{
		if (text[i] < '0' || text[i] > '9')
			return CC_DIGIT_STEP_NONE;
		value = value * 10 + (text[i] - '0');
	}
	if (first)
		value = -value;

	/* Counted from the right: that is what survives a change of width. */
	const size_t place = text.size() - 1 - cursor;
	long long step = 1;
	for (size_t i = 0; i < place; i++)
		step *= 10;

	/* 18 digits and a step of at most 10^17 stay well inside long long. */
	const long long target = up ? value + step : value - step;
	if (up ? target > highest : target < lowest)
		return CC_DIGIT_STEP_REFUSED;

	const bool negative = target < 0;
	std::string number = std::to_string(negative ? -target : target);
	if (number.size() < digits)
		number.insert(0, digits - number.size(), '0');

	text = (negative ? "-" : "") + number;
	cursor = text.size() - 1 - place;
	return CC_DIGIT_STEP_DONE;
}

#endif
