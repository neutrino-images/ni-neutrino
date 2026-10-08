/*
 * numberstep.h - the order a number chooser steps through
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

#ifndef __numberstep_h__
#define __numberstep_h__

#include <algorithm>
#include <vector>

/* Header only and free of widgets so a test can hold the order without a screen.

   A number that is named in words may lie outside the range, such as off below a
   floor. The chooser has to be able to land on it, so it steps through one
   ascending order: the names below the range, the range, the names above it. */
inline std::vector<int> numberStepOrder(int lower, int upper, const std::vector<int> &named)
{
	std::vector<int> order;
	for (size_t i = 0; i < named.size(); i++)
	{
		if (named[i] < lower)
			order.push_back(named[i]);
	}
	std::sort(order.begin(), order.end());
	order.erase(std::unique(order.begin(), order.end()), order.end());

	for (int v = lower; v <= upper; v++)
		order.push_back(v);

	std::vector<int> above;
	for (size_t i = 0; i < named.size(); i++)
	{
		if (named[i] > upper)
			above.push_back(named[i]);
	}
	std::sort(above.begin(), above.end());
	above.erase(std::unique(above.begin(), above.end()), above.end());
	order.insert(order.end(), above.begin(), above.end());
	return order;
}

/* The element after (up) or before the current value in that order, the ends
   wrapping to each other. A value that is in the order only by being stored,
   outside the range and unnamed, goes to the nearest element in the direction
   of travel. Worked out from the range and the names, without building the
   range: a chooser may span a very wide one. */
inline int numberStep(int lower, int upper, const std::vector<int> &named, int current, bool up)
{
	std::vector<int> below;
	std::vector<int> above;
	for (size_t i = 0; i < named.size(); i++)
	{
		if (named[i] < lower)
			below.push_back(named[i]);
		else if (named[i] > upper)
			above.push_back(named[i]);
	}
	std::sort(below.begin(), below.end());
	std::sort(above.begin(), above.end());

	const int first = below.empty() ? lower : below.front();
	const int last = above.empty() ? upper : above.back();

	if (up)
	{
		if (current < lower)
		{
			std::vector<int>::const_iterator it = std::upper_bound(below.begin(), below.end(), current);
			return it == below.end() ? lower : *it;
		}
		if (current < upper)
			return current + 1;
		std::vector<int>::const_iterator it = std::upper_bound(above.begin(), above.end(), current);
		return it == above.end() ? first : *it;
	}

	if (current > upper)
	{
		std::vector<int>::const_iterator it = std::lower_bound(above.begin(), above.end(), current);
		return it == above.begin() ? upper : *(it - 1);
	}
	if (current > lower)
		return current - 1;
	std::vector<int>::const_iterator it = std::lower_bound(below.begin(), below.end(), current);
	return it == below.begin() ? last : *(it - 1);
}

#endif
