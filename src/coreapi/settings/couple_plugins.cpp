/*
 * couple_plugins.cpp - the plugin lists as one partition
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

#include "couple.h"

#include <set>

namespace coreapi
{
namespace settings
{

namespace
{

const size_t kLists = 5;

const char *const kListKeys[kLists] =
{
	"plugins_disabled", "plugins_game", "plugins_tool", "plugins_script", "plugins_lua"
};

// The names of a list in the order written, each once and none empty.
std::vector<std::string> namesOf(const std::string &list)
{
	std::vector<std::string> out;
	std::set<std::string> seen;
	size_t start = 0;
	while (start <= list.size())
	{
		size_t end = list.find(',', start);
		if (end == std::string::npos)
			end = list.size();
		const std::string name = list.substr(start, end - start);
		if (!name.empty() && seen.insert(name).second)
			out.push_back(name);
		start = end + 1;
	}
	return out;
}

std::string joined(const std::vector<std::string> &names)
{
	std::string out;
	for (size_t i = 0; i < names.size(); ++i)
		out += (i == 0 ? "" : ",") + names[i];
	return out;
}

} // anonymous namespace

/* A plugin has one type, so it is named by one of the five lists and by that list once.
   A write of some of the lists puts each name it carries in that list alone: the other
   lists lose it, the way the personalisation menu rebuilds all five from one answer per
   plugin. Two lists of one write naming the same plugin say two types for it, and
   nothing says which was meant, so both are refused. */
void couplePlugins(CoupledBatch &b)
{
	std::vector<std::string> names[kLists];
	bool named[kLists];
	bool any = false;
	for (size_t i = 0; i < kLists; ++i)
	{
		const std::string *text = b.written(kListKeys[i]);
		named[i] = text != NULL;
		if (text != NULL)
		{
			names[i] = namesOf(*text);
			any = true;
		}
	}
	if (!any)
		return;

	bool clash[kLists] = { false, false, false, false, false };
	for (size_t i = 0; i < kLists; ++i)
	{
		for (size_t j = i + 1; j < kLists; ++j)
		{
			if (!named[i] || !named[j])
				continue;
			for (size_t n = 0; n < names[i].size(); ++n)
			{
				for (size_t m = 0; m < names[j].size(); ++m)
				{
					if (names[i][n] == names[j][m])
						clash[i] = clash[j] = true;
				}
			}
		}
	}
	for (size_t i = 0; i < kLists; ++i)
	{
		if (!clash[i])
			continue;
		b.refuse(kListKeys[i], "a plugin is in one list only and two lists written together name the same one");
		named[i] = false;
	}

	// A list written with a repeated or an empty name is written without them.
	for (size_t i = 0; i < kLists; ++i)
	{
		if (named[i])
		{
			const std::string *text = b.written(kListKeys[i]);
			if (text != NULL && *text != joined(names[i]))
				b.put(kListKeys[i], joined(names[i]), kListKeys[i]);
		}
	}

	for (size_t i = 0; i < kLists; ++i)
	{
		if (named[i] || clash[i])
			continue;
		std::string stored;
		if (!b.current(kListKeys[i], stored))
			continue;

		std::vector<std::string> keep;
		bool took[kLists] = { false, false, false, false, false };
		const std::vector<std::string> had = namesOf(stored);
		for (size_t n = 0; n < had.size(); ++n)
		{
			bool gone = false;
			for (size_t t = 0; t < kLists; ++t)
			{
				if (!named[t])
					continue;
				for (size_t m = 0; m < names[t].size(); ++m)
				{
					if (names[t][m] == had[n])
					{
						gone = true;
						took[t] = true;
					}
				}
			}
			if (!gone)
				keep.push_back(had[n]);
		}
		if (keep.size() == had.size())
			continue;

		/* The list is due to the lists whose names it lost, so that a list which cannot be
		   written leaves those names where they are, and a name that cannot be taken from
		   it leaves the list that wanted it unwritten. */
		bool first = true;
		for (size_t t = 0; t < kLists; ++t)
		{
			if (!took[t])
				continue;
			if (first)
				b.put(kListKeys[i], joined(keep), kListKeys[t]);
			else
				b.link(kListKeys[i], kListKeys[t]);
			first = false;
		}
	}
}

} // namespace settings
} // namespace coreapi
