/*
 * scopes.cpp - what an OAuth client may be granted on this box
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

#include "httpd/oauth/scopes.h"

#include <cstddef>

namespace httpd
{
namespace oauth
{

namespace
{

struct Named
{
	const char *name;
	unsigned    bit;
};

const Named kScopes[] = {
	{ "read", ScopeRead },
	{ "write", ScopeWrite },
	{ "system", ScopeSystem },
	{ "offline_access", ScopeOffline },
};

const size_t kScopeCount = sizeof(kScopes) / sizeof(kScopes[0]);

} // namespace

bool parseScopes(const std::string &text, unsigned *bits)
{
	unsigned out = 0;
	size_t i = 0;
	while (i <= text.size())
	{
		size_t end = text.find(' ', i);
		if (end == std::string::npos)
			end = text.size();
		const std::string word = text.substr(i, end - i);
		if (!word.empty())
		{
			bool known = false;
			for (size_t k = 0; k < kScopeCount && !known; ++k)
			{
				if (word == kScopes[k].name)
				{
					out |= kScopes[k].bit;
					known = true;
				}
			}
			if (!known)
				return false;
		}
		i = end + 1;
	}
	if (bits != NULL)
		*bits = out;
	return true;
}

unsigned withImplied(unsigned bits)
{
	if ((bits & ScopeSystem) != 0)
		bits |= ScopeWrite;
	if ((bits & ScopeWrite) != 0)
		bits |= ScopeRead;
	return bits;
}

std::vector<std::string> scopeNames(unsigned bits)
{
	std::vector<std::string> out;
	for (size_t k = 0; k < kScopeCount; ++k)
	{
		if ((bits & kScopes[k].bit) != 0)
			out.push_back(kScopes[k].name);
	}
	return out;
}

std::string scopeString(unsigned bits)
{
	const std::vector<std::string> names = scopeNames(bits);
	std::string out;
	for (size_t i = 0; i < names.size(); ++i)
	{
		if (i != 0)
			out += ' ';
		out += names[i];
	}
	return out;
}

AuthLevel levelFor(unsigned bits)
{
	if ((bits & ScopeSystem) != 0)
		return AuthLevel::System;
	if ((bits & ScopeWrite) != 0)
		return AuthLevel::Write;
	if ((bits & ScopeRead) != 0)
		return AuthLevel::Read;
	return AuthLevel::Public;
}

std::vector<std::string> resourceScopes()
{
	return scopeNames(ScopeLevels);
}

std::vector<std::string> serverScopes()
{
	return scopeNames(ScopeLevels | ScopeOffline);
}

} // namespace oauth
} // namespace httpd
