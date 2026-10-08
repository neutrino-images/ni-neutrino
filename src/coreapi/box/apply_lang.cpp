/*
 * apply_lang.cpp - what makes a changed language or time zone setting take effect
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

#include <config.h>

#include "coreapi/box/apply_lang.h"
#include "coreapi/box/sentstate.h"

#include <system/settings.h>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

class NoLocalization : public Localization
{
	public:
		Status loadLanguage(const std::string &) { return Status::NotSupported; }
		Status linkTimezone() { return Status::NotSupported; }
		Status setGuideLanguages(const std::vector<std::string> &) { return Status::NotSupported; }
};

NoLocalization g_no_localization;
Localization *g_localization = 0;

/* What the groups last put on the program (sentstate.h). Loading a catalog
   frees the texts every open menu may still point into, relinking the zone
   rewrites files of the system, so neither is done again for the value it
   already has. */
struct LanguageState
{
	Sent<std::string> language;
	Sent<std::string> timezone;
	Sent<std::vector<std::string> > guide;
};

LanguageState g_state;
// Nobody else writes this state and nobody holds it, so no bit is ever marked.
SentFlags g_flags;

enum LanguageSent
{
	SentLanguage = 1u << 0,
	SentTimezone = 1u << 1,
	SentGuide    = 1u << 2
};

Status runLanguage()
{
	Localization &l = localization();
	Status first = Status::Ok;
	const std::string name = g_settings.language;
	sendChanged(first, g_flags, SentLanguage, g_state.language, name, [&]() { return l.loadLanguage(name); });
	return first;
}

Status runTimezone()
{
	Localization &l = localization();
	Status first = Status::Ok;
	const std::string zone = g_settings.timezone;
	sendChanged(first, g_flags, SentTimezone, g_state.timezone, zone, [&]() { return l.linkTimezone(); });
	return first;
}

Status runGuideLanguage()
{
	Localization &l = localization();
	Status first = Status::Ok;
	std::vector<std::string> names;
	for (int i = 0; i < 3; i++)
		names.push_back(g_settings.pref_lang[i]);
	sendChanged(first, g_flags, SentGuide, g_state.guide, names, [&]() { return l.setGuideLanguages(names); });
	return first;
}

const char *const kLanguageKeys[] =
{
	"language"
};

const char *const kTimezoneKeys[] =
{
	"timezone"
};

const char *const kGuideLanguageKeys[] =
{
	"pref_lang_0",
	"pref_lang_1",
	"pref_lang_2"
};

} // namespace

const ApplyGroup kLanguageApplyGroup = { "language", ApplyPhase::Framebuffer, COREAPI_KEYS(kLanguageKeys), &runLanguage };
const ApplyGroup kTimezoneApplyGroup = { "timezone", ApplyPhase::Framebuffer, COREAPI_KEYS(kTimezoneKeys), &runTimezone };
const ApplyGroup kGuideLanguageApplyGroup = { "guideLanguage", ApplyPhase::Sectionsd, COREAPI_KEYS(kGuideLanguageKeys), &runGuideLanguage };

std::vector<std::string> guideLanguageCodes(const std::vector<std::string> &names,
					    const std::map<std::string, std::string> &codes)
{
	std::vector<std::string> out;
	for (size_t i = 0; i < names.size(); i++)
	{
		if (names[i].empty() || names[i] == "none")
			continue;
		for (std::map<std::string, std::string>::const_iterator it = codes.begin(); it != codes.end(); ++it)
		{
			if (names[i] == it->second)
				out.push_back(it->first);
		}
	}
	return out;
}

Localization &localization()
{
	if (!g_localization)
		return g_no_localization;
	return *g_localization;
}

void setLocalization(Localization *l) { g_localization = l; }

void noteLanguageLoaded(const std::string &name)
{
	g_state.language.known = true;
	g_state.language.value = name;
}

void noteGuideLanguagesLoaded(const std::vector<std::string> &names)
{
	g_state.guide.known = true;
	g_state.guide.value = names;
}

void resetSentLanguage()
{
	g_state = LanguageState();
	g_flags.reset();
}

} // namespace coreapi
