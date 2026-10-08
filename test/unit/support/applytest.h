/*
 * applytest.h - what the apply group cases of the areas share
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

#ifndef __support_applytest_h__
#define __support_applytest_h__

#include "coreapi/base/apply.h"

#include <cstdio>
#include <string>

// Every case starts from nothing: the registry is process wide.
struct ApplyFresh
{
	ApplyFresh() { coreapi::resetApplyRegistry(); }
	~ApplyFresh() { coreapi::resetApplyRegistry(); }
};

// The save of a source whose settings nobody keeps.
inline bool applyNothingToSave() { return true; }

inline std::string applyNumber(int v)
{
	char text[16];
	snprintf(text, sizeof(text), "%d", v);
	return text;
}

#endif
