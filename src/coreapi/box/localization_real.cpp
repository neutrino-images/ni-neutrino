/*
 * localization_real.cpp - the catalog, clock and guide behind the language groups
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

#include <eitd/sectionsd.h>
#include <system/localize.h>

namespace coreapi
{

namespace
{

/* Its own translation unit for the reason every other adapter here has one: a
   binary that only wants the groups must not have to link the catalog, the
   guide or the application. */
class RealLocalization : public Localization
{
	public:
		Status loadLanguage(const std::string &name)
		{
			return applicationLoadLanguage(name);
		}

		Status linkTimezone()
		{
			return applicationLinkTimezone();
		}

		/* The guide knows a language by its three letter code and the settings by
		   its name. */
		Status setGuideLanguages(const std::vector<std::string> &names)
		{
			CEitManager::getInstance()->setLanguages(guideLanguageCodes(names, iso639));
			return Status::Ok;
		}
};

RealLocalization g_real_localization;

} // anonymous namespace

void installRealLocalization() { setLocalization(&g_real_localization); }

} // namespace coreapi
