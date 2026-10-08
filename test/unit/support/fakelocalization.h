/*
 * fakelocalization.h - the language groups' catalog, clock and guide as a case sees them
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

#ifndef __support_fakelocalization_h__
#define __support_fakelocalization_h__

#include "coreapi/box/apply_lang.h"

#include <string>
#include <vector>

/* Every call the language groups make, in order, so a case can say what one run
   sent and that it sent nothing else. */
struct FakeLocalization : public coreapi::Localization
{
	std::vector<std::string> calls;
	std::vector<std::string> languages;
	std::vector<std::vector<std::string> > guide;
	// What the catalog call answers, so a case can make one send fail.
	coreapi::Status language_answer;

	FakeLocalization() : language_answer(coreapi::Status::Ok) {}

	coreapi::Status loadLanguage(const std::string &name)
	{
		calls.push_back("language");
		languages.push_back(name);
		return language_answer;
	}

	coreapi::Status linkTimezone()
	{
		calls.push_back("timezone");
		return coreapi::Status::Ok;
	}

	coreapi::Status setGuideLanguages(const std::vector<std::string> &names)
	{
		calls.push_back("guide");
		guide.push_back(names);
		return coreapi::Status::Ok;
	}

	size_t count(const std::string &what) const
	{
		size_t n = 0;
		for (size_t i = 0; i < calls.size(); ++i)
			n += (calls[i] == what) ? 1 : 0;
		return n;
	}

	void forget()
	{
		calls.clear();
		languages.clear();
		guide.clear();
	}
};

#endif
