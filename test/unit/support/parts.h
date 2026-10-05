/*
 * parts.h - one suite run as several processes and judged as one
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

#ifndef COREAPI_TEST_PARTS_H
#define COREAPI_TEST_PARTS_H

#include <string>

/* The suite spends its time waiting on sleeps and timeouts rather than on the
   processor, so it is run as several processes side by side. What the cases
   compared is still only whole once every case has run, so each part writes
   down what it saw and one more run reads all of them back and judges them as
   the single process used to. */

/* which is "k/n". Clears what part k recorded last and gives the Catch test
   spec for the source files dealt to it; false, with the reason on stderr, for
   a which that is not a part. */
bool planPart(const std::string &which, std::string &spec);

/* Records what the part ran and saw; failed is what the run returned. Answers 1
   or 0 rather than the count, which the harness would read as a skip at 77. */
int finishPart(int failed);

/* Reads what parts 1 to n recorded into this process. False, with the reason on
   stderr, unless every part left a complete record, none of them failed, and
   every case of this binary ran in exactly one of them. */
bool mergeParts(const std::string &count);

#endif
