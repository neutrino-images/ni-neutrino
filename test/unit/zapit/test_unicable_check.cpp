/*
 * test_unicable_check.cpp - tests for the unicable user band check
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

#include <zapit/unicable_check.h>

#include <vector>

/* What the frontend setup warns about when a tuner is given a user band: the
 * check is header only, and which tuners share a cable is worked out by the
 * setup and handed in, so the cases build that list themselves. */

namespace {

unicable_use_t use(int tuner, int cable, int scr, int freq)
{
	unicable_use_t u;
	u.tuner = tuner;
	u.cable = cable;
	u.scr = scr;
	u.freq = freq;
	return u;
}

/* A VU+ Uno 4K SE as a user set it up: the first input and six links of
 * its FBC set on one JESS cable, on the bands 3 to 7 of an LNB that puts
 * band n on 975 + 50 * n MHz. */
std::vector<unicable_use_t> box()
{
	std::vector<unicable_use_t> uses;
	uses.push_back(use(0, 1000, 2, 1075));
	uses.push_back(use(2, 1000, 3, 1125));
	uses.push_back(use(3, 1000, 4, 1175));
	uses.push_back(use(4, 1000, 5, 1225));
	uses.push_back(use(5, 1000, 6, 1275));
	return uses;
}

}

TEST_CASE("unicable: a band of its own on the cable is no clash", "[unicable]")
{
	int other = 99;
	CHECK(unicableClash(box(), use(6, 1000, 7, 1325), other) == UNICABLE_CLASH_NONE);
	CHECK(other == -1);
}

TEST_CASE("unicable: the address another tuner on the cable has is a clash", "[unicable]")
{
	int other = -1;
	CHECK(unicableClash(box(), use(6, 1000, 4, 1325), other) == UNICABLE_CLASH_SCR);
	REQUIRE(other == 2);
	CHECK(box()[other].tuner == 3);
}

TEST_CASE("unicable: the frequency another tuner on the cable has is a clash", "[unicable]")
{
	/* the address of one band with the frequency of another, as band 7
	 * set to 1375 MHz, which is band 8's, beside a tuner on band 8 */
	std::vector<unicable_use_t> uses = box();
	uses.push_back(use(7, 1000, 8, 1375));

	int other = -1;
	CHECK(unicableClash(uses, use(6, 1000, 7, 1375), other) == UNICABLE_CLASH_FREQ);
	REQUIRE(other == 5);
	CHECK(uses[other].tuner == 7);
}

TEST_CASE("unicable: a clash of address is told before one of frequency", "[unicable]")
{
	int other = -1;
	CHECK(unicableClash(box(), use(6, 1000, 5, 1125), other) == UNICABLE_CLASH_SCR);
	CHECK(box()[other].tuner == 4);
}

TEST_CASE("unicable: tuners on another cable may have the same band", "[unicable]")
{
	int other = -1;
	CHECK(unicableClash(box(), use(1, 1001, 3, 1125), other) == UNICABLE_CLASH_NONE);
	CHECK(other == -1);
}

TEST_CASE("unicable: a frequency nobody set clashes with nothing", "[unicable]")
{
	std::vector<unicable_use_t> uses = box();
	uses.push_back(use(7, 1000, 9, 0));

	int other = -1;
	CHECK(unicableClash(uses, use(6, 1000, 10, 0), other) == UNICABLE_CLASH_NONE);
}

TEST_CASE("unicable: the tuner being set up is not its own clash", "[unicable]")
{
	int other = -1;
	CHECK(unicableClash(box(), use(3, 1000, 4, 1175), other) == UNICABLE_CLASH_NONE);
	CHECK(other == -1);
}

TEST_CASE("unicable: a cable with nothing else on it has no clash", "[unicable]")
{
	std::vector<unicable_use_t> none;
	int other = -1;
	CHECK(unicableClash(none, use(0, 0, 0, 1210), other) == UNICABLE_CLASH_NONE);
}
