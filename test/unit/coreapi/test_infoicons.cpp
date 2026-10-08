/*
 * test_infoicons.cpp - tests for the info icons
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
#include "support/fakes.h"

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/router.h"
#include "httpd/status.h"

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/osd.h"
#include "coreapi/base/result.h"
#include "coreapi/base/schema.h"
#include "coreapi/settings/settings.h"
#include "coreapi/settings/settingsfield.h"

#include <string>
#include <vector>

using namespace httpd;

namespace
{

/* The three the box writes in mode_icons_skin, as the drawing code names them
   (src/gui/infoicons.h). Written out here for the same reason the layer under
   test writes them out, and held to the row that declares the skin by a case
   below, so the two cannot part without something saying so. */
const long kSkinStatic     = 0;
const long kSkinInfoviewer = 1;
const long kSkinPopup      = 2;

struct ShippedRoutes
{
	ShippedRoutes() { setRoutesForTest(NULL); }
	~ShippedRoutes() { setRoutesForTest(NULL); }

	private:
		ShippedRoutes(const ShippedRoutes &);
		ShippedRoutes &operator=(const ShippedRoutes &);
};

bool fixtureSave() { return true; }

/* The store the box runs on, because what applies a write is whatever carries
   the write into the program's own settings, and a fake carries none.

   The sink stands in for the message loop, so nothing is carried until a case
   says so and the two halves can be seen apart. */
struct RealStore
{
	SNeutrinoSettings     values;
	FakeCommandSink       sink;
	InstalledSink         installed;
	ClearedSettingsSource cleared;

	RealStore() : values(SNeutrinoSettings()), installed(&sink)
	{
		coreapi::installRealSettingsSource(&values, fixtureSave);
	}

	~RealStore() { coreapi::installRealSettingsSource(NULL, NULL); }

	private:
		RealStore(const RealStore &);
		RealStore &operator=(const RealStore &);
};

std::string stateOf(const Response &r)
{
	const std::string key = "\"state\":";
	const size_t at = r.body.find(key);
	if (at == std::string::npos)
		return "<no state>";
	const size_t open = r.body.find('"', at + key.size());
	if (open == std::string::npos)
		return "<no state>";
	const size_t close = r.body.find('"', open + 1);
	if (close == std::string::npos)
		return "<no state>";
	return r.body.substr(open + 1, close - open - 1);
}

Response putState(const std::string &state)
{
	return dispatch(Put, "/api/v1/osd/infoicons", "",
			"{\"state\":\"" + state + "\"}", "127.0.0.1", AuthLevel::Write);
}

} // namespace

TEST_CASE("the four states are the two settings", "[infoicons]")
{
	InstalledDependencies deps;
	FakeSettingsSource s;
	InstalledSettingsSource in(&s);

	s.ints["mode_icons"] = 1; s.ints["mode_icons_skin"] = kSkinStatic;
	REQUIRE(coreapi::osd::infoIcons().value() == coreapi::osd::InfoIcons::Static);

	s.ints["mode_icons"] = 1; s.ints["mode_icons_skin"] = kSkinPopup;
	REQUIRE(coreapi::osd::infoIcons().value() == coreapi::osd::InfoIcons::Popup);

	s.ints["mode_icons"] = 0; s.ints["mode_icons_skin"] = kSkinInfoviewer;
	REQUIRE(coreapi::osd::infoIcons().value() == coreapi::osd::InfoIcons::Infoviewer);

	s.ints["mode_icons"] = 0; s.ints["mode_icons_skin"] = kSkinStatic;
	REQUIRE(coreapi::osd::infoIcons().value() == coreapi::osd::InfoIcons::Off);
}

TEST_CASE("the numbers the state is written as are the ones the box declares",
          "[infoicons]")
{
	/* The skin row lists its three values and the screen is what that list was
	   read off, so a state written under a different number than the row
	   declares is one the box would draw as something else. */
	coreapi::Result<coreapi::Descriptor> d = coreapi::settings::describe("mode_icons_skin");
	REQUIRE(d.ok());
	REQUIRE(d.value().values != NULL);
	REQUIRE(d.value().value_count == 3);

	bool saw_static = false, saw_infoviewer = false, saw_popup = false;
	for (size_t i = 0; i < d.value().value_count; ++i)
	{
		const long v = d.value().values[i].value;
		saw_static = saw_static || (v == kSkinStatic);
		saw_infoviewer = saw_infoviewer || (v == kSkinInfoviewer);
		saw_popup = saw_popup || (v == kSkinPopup);
	}
	REQUIRE(saw_static);
	REQUIRE(saw_infoviewer);
	REQUIRE(saw_popup);
}

TEST_CASE("turning them off leaves a state that can be turned on again",
          "[infoicons]")
{
	/* The rule that lives in the handler and nowhere in the settings table:
	   from infoviewer, off has to move the skin as well, or the box can never
	   leave infoviewer. */
	InstalledDependencies deps;
	FakeSettingsSource s;
	InstalledSettingsSource in(&s);
	s.ints["mode_icons"] = 0; s.ints["mode_icons_skin"] = kSkinInfoviewer;

	REQUIRE(coreapi::osd::setInfoIcons(coreapi::osd::InfoIcons::Off).ok());
	REQUIRE(s.ints["mode_icons"] == 0);
	REQUIRE(s.ints["mode_icons_skin"] == kSkinStatic);
	REQUIRE(coreapi::osd::infoIcons().value() == coreapi::osd::InfoIcons::Off);
}

TEST_CASE("off leaves a skin that is not infoviewer where it stands", "[infoicons]")
{
	// The other half of the rule, and the half a push to static for every skin
	// would break: turning the icons off is not choosing a skin.
	InstalledDependencies deps;
	FakeSettingsSource s;
	InstalledSettingsSource in(&s);
	s.ints["mode_icons"] = 1; s.ints["mode_icons_skin"] = kSkinPopup;

	REQUIRE(coreapi::osd::setInfoIcons(coreapi::osd::InfoIcons::Off).ok());
	REQUIRE(s.ints["mode_icons"] == 0);
	REQUIRE(s.ints["mode_icons_skin"] == kSkinPopup);
	REQUIRE(coreapi::osd::infoIcons().value() == coreapi::osd::InfoIcons::Off);
}

TEST_CASE("both settings land in one save", "[infoicons]")
{
	// Two keys and one persist, because a box that saved between them would
	// have a moment in which it is in neither state.
	InstalledDependencies deps;
	FakeSettingsSource s;
	InstalledSettingsSource in(&s);
	s.ints["mode_icons"] = 0; s.ints["mode_icons_skin"] = kSkinStatic;

	const unsigned before = s.persisted;
	REQUIRE(coreapi::osd::setInfoIcons(coreapi::osd::InfoIcons::Popup).ok());
	REQUIRE(s.persisted == before + 1);
	REQUIRE(s.ints["mode_icons"] == 1);
	REQUIRE(s.ints["mode_icons_skin"] == kSkinPopup);
}

TEST_CASE("every move between the four states lands", "[infoicons]")
{
	/* Through the settings layer and its conditions, which lock the two rows against each other
	   in the menu: a state change that passes only as raw writes would be refused here. */
	InstalledDependencies deps;
	FakeSettingsSource s;
	InstalledSettingsSource in(&s);
	s.ints["mode_icons"] = 0; s.ints["mode_icons_skin"] = kSkinStatic;

	static const coreapi::osd::InfoIcons kAll[] =
	{
		coreapi::osd::InfoIcons::Static, coreapi::osd::InfoIcons::Popup,
		coreapi::osd::InfoIcons::Infoviewer, coreapi::osd::InfoIcons::Off
	};
	for (size_t from = 0; from < 4; ++from)
	{
		for (size_t to = 0; to < 4; ++to)
		{
			INFO(from << " to " << to);
			REQUIRE(coreapi::osd::setInfoIcons(kAll[from]).ok());
			REQUIRE(coreapi::osd::setInfoIcons(kAll[to]).ok());
			REQUIRE(coreapi::osd::infoIcons().value() == kAll[to]);
		}
	}
}

TEST_CASE("the state is written with no group behind it", "[infoicons]")
{
	// Nothing here depends on a group being there, so a box that has none
	// writes the state all the same.
	RealStore store;

	REQUIRE(coreapi::osd::setInfoIcons(coreapi::osd::InfoIcons::Infoviewer).ok());
	coreapi::applyPendingSettings();

	REQUIRE(store.values.mode_icons == 0);
	REQUIRE(store.values.mode_icons_skin == (int) kSkinInfoviewer);
}

TEST_CASE("the four names go out and come back as the same four", "[infoicons]")
{
	ShippedRoutes shipped;
	InstalledDependencies deps;
	FakeSettingsSource s;
	InstalledSettingsSource in(&s);
	s.ints["mode_icons"] = 0; s.ints["mode_icons_skin"] = kSkinStatic;

	static const char *const kNames[] = { "static", "popup", "infoviewer", "off" };
	for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); ++i)
	{
		INFO(kNames[i]);
		const Response written = putState(kNames[i]);
		REQUIRE(written.code == StatusNoContent);
		REQUIRE(written.body.empty());

		const Response read = dispatch(Get, "/api/v1/osd/infoicons", "", "",
					       "127.0.0.1", AuthLevel::Read);
		REQUIRE(read.code == StatusOk);
		REQUIRE(stateOf(read) == kNames[i]);
	}
}

TEST_CASE("a state the four do not name never reaches the box", "[infoicons]")
{
	ShippedRoutes shipped;
	InstalledDependencies deps;
	FakeSettingsSource s;
	InstalledSettingsSource in(&s);
	s.ints["mode_icons"] = 1; s.ints["mode_icons_skin"] = kSkinPopup;
	const unsigned before = s.persisted;

	const Response r = putState("unknown");
	REQUIRE(r.code == StatusBadRequest);
	// Refused where the parameter is checked, which is before a handler is
	// entered: nothing was written and nothing was saved.
	REQUIRE(r.body.find("bad-enum") != std::string::npos);
	REQUIRE(s.persisted == before);
	REQUIRE(s.ints["mode_icons"] == 1);
	REQUIRE(s.ints["mode_icons_skin"] == kSkinPopup);
}

TEST_CASE("a store that cannot be read is said so rather than guessed at",
          "[infoicons]")
{
	InstalledDependencies deps;
	FakeSettingsSource s;
	InstalledSettingsSource in(&s);
	s.ints["mode_icons"] = 1; s.ints["mode_icons_skin"] = kSkinPopup;

	s.fail_next = true;
	const coreapi::Result<coreapi::osd::InfoIcons> r = coreapi::osd::infoIcons();
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().code == coreapi::ErrorCode::SettingUnreadable);
}
