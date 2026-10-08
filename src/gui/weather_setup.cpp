/*
	Copyright (C) 2020 TangoCash

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation;

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "weather_setup.h"

#include <global.h>
#include <neutrino.h>

#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/settingitem.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/keyboard_input.h>

#include <gui/weather.h>

#include <coreapi/base/apply.h>
#include <coreapi/box/apply_weather.h>
#include <coreapi/settings/settings.h>

#include <driver/screen_max.h>

#include <system/debug.h>

#include <utility>

void coreapi::applicationPrepareWeather()
{
	CWeather::getInstance();
}

/* Runs on the weather worker, which reads no setting that holds text: the job carries
   them. The service is read by the loop and by the two display threads while this
   fetches, as it is when they fetch themselves. */
void coreapi::applicationRunWeatherJob(const WeatherJob &job)
{
	CWeather *weather = CWeather::getInstance();
	if (job.place)
		weather->setCoords(job.coords, job.city);
	if (job.api)
		weather->updateApi(job.api_key, job.api_version);
}

CWeatherSetup::CWeatherSetup()
{
	width = 40;
	selected = -1;
	location_item = NULL;
	locations.clear();
	loadLocations(CONFIGDIR "/weather-favorites.xml");
	loadLocations(WEATHERDIR "/weather-locations.xml");
}

CWeatherSetup::~CWeatherSetup()
{
}

int CWeatherSetup::exec(CMenuTarget *parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init weather setup menu\n");

	if (parent)
		parent->hide();

	if (actionKey == "select_location")
	{
		return selectLocation();
	}

	return showWeatherSetup();
}

int CWeatherSetup::showWeatherSetup()
{
	CMenuWidget *ms_oservices = new CMenuWidget(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP_ONLINESERVICES);
	ms_oservices->addIntroItems(LOCALE_MISCSETTINGS_ONLINESERVICES);

	CMenuItem *onoff = addSetting(ms_oservices, "weather_enabled");
	if (onoff)
		onoff->hintIcon = NEUTRINO_ICON_HINT_SETTINGS;

	// Not there where the build keeps the key itself.
	CMenuItem *key = addSetting(ms_oservices, "weather_api_key");
	if (key)
		key->hintIcon = NEUTRINO_ICON_HINT_SETTINGS;

	// The list of places is the screen's own dialog, so this item is not one the declaration builds.
	// A forwarder keeps a pointer to a text handed to it; setOption keeps a copy.
	CFollowForwarder *place = new CFollowForwarder(LOCALE_WEATHER_LOCATION, coreapi::settings::conditionsHoldNow("weather_city"), NULL, this, "select_location");
	place->setOption(settingsText(g_settings.weather_city));
	location_item = place;
	// A copy of the place and no row: a write from elsewhere moves the place and the switch that allows it.
	std::vector<std::string> moves;
	moves.push_back("weather_location");
	moves.push_back("weather_enabled");
	place->follow(ms_oservices, "weather_city", moves, true, [place]() { place->setOption(settingsText(g_settings.weather_city)); });
	location_item->setHint(NEUTRINO_ICON_HINT_SETTINGS, LOCALE_MENU_HINT_WEATHER_LOCATION);
	ms_oservices->addItem(location_item);

	CMenuItem *zip = addSetting(ms_oservices, "weather_postalcode", true, this);
	if (zip)
		zip->hintIcon = NEUTRINO_ICON_HINT_SETTINGS;

	int res = ms_oservices->exec(NULL, "");
	selected = ms_oservices->getSelected();
	delete ms_oservices;
	location_item = NULL;
	return res;
}

void CWeatherSetup::setPlace(const std::string &coords, const std::string &city)
{
	// Written as the pair it is, which also empties the postal code that described the old place.
	std::vector<std::pair<std::string, std::string> > members;
	members.push_back(std::make_pair(std::string("weather_city"), city));
	members.push_back(std::make_pair(std::string("weather_location"), coords));
	coreapi::settings::Refusals failed;
	coreapi::settings::writeBatch(members, failed, true);
	for (size_t i = 0; i < failed.size(); i++)
		dprintf(DEBUG_NORMAL, "[weather] %s not written: %s\n", failed[i].first.c_str(), failed[i].second.message.c_str());

	// The write applied and announced itself; the place shown here is a copy and no row.
	if (location_item)
		location_item->setOption(settingsText(g_settings.weather_city));
}

void CWeatherSetup::placeChanged()
{
	const coreapi::Status s = coreapi::applyKey("weather_location");
	if (s != coreapi::Status::Ok && s != coreapi::Status::Busy)
		dprintf(DEBUG_NORMAL, "[weather] the place was not applied\n");
	if (location_item)
		location_item->setOption(settingsText(g_settings.weather_city));
}

int CWeatherSetup::selectLocation()
{
	int select = 0;
	int res = 0;

	if (locations.empty())
	{
		// TODO: localize hint
		ShowHint("Warning", "Failed to load weather-favorites.xml or weather-locations.xml\nPlease press any key or wait some seconds! ...", 700, 10, NULL, NEUTRINO_ICON_HINT_IMAGEINFO, CComponentsHeader::CC_BTN_EXIT);
		setPlace(WEATHER_DEFAULT_LOCATION, WEATHER_DEFAULT_CITY);
		return menu_return::RETURN_REPAINT;
	}

	CMenuWidget *m = new CMenuWidget(LOCALE_WEATHER_LOCATION, NEUTRINO_ICON_LANGUAGE);
	CMenuSelectorTarget *selector = new CMenuSelectorTarget(&select);

	m->addItem(GenericMenuSeparator);

	CMenuForwarder *mf;
	for (size_t i = 0; i < locations.size(); i++)
	{
		std::string hint = locations[i].country;
		hint += ": ";
		hint += locations[i].coords.c_str();

		mf = new CMenuForwarder(locations[i].city, true, NULL, selector, to_string(i).c_str());
		mf->setHint(NEUTRINO_ICON_HINT_SETTINGS, hint);
		m->addItem(mf);
	}

	m->enableSaveScreen();
	res = m->exec(NULL, "");

	if (!m->gotAction())
		return res;

	delete selector;

	setPlace(locations[select].coords, std::string(locations[select].city));

	return res;
}

void CWeatherSetup::findLocation()
{
	if (CWeather::getInstance()->FindCoords(settingsText(g_settings.weather_postalcode)))
		placeChanged();
}

bool CWeatherSetup::changeNotify(const neutrino_locale_t OptionName, void * /*data*/)
{
	// A switch left on without a key is kept and locked by its row condition, as on the web.
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_WEATHER_POSTALCODE))
	{
		findLocation();
	}
	return false;
}

void CWeatherSetup::loadLocations(std::string filename)
{
	xmlDocPtr parser = parseXmlFile(filename.c_str());

	if (parser == NULL)
	{
		dprintf(DEBUG_INFO, "failed to load %s\n", filename.c_str());
		return;
	}

	xmlNodePtr l0 = xmlDocGetRootElement(parser);
	xmlNodePtr l1 = xmlChildrenNode(l0);

	if (l1)
	{
		while ((xmlGetNextOccurence(l1, "location")))
		{
			const char *country = xmlGetAttribute(l1, "country");
			const char *city = xmlGetAttribute(l1, "city");
			const char *latitude = xmlGetAttribute(l1, "latitude");
			const char *longitude = xmlGetAttribute(l1, "longitude");
			weather_loc loc;
			loc.country = strdup(country);
			loc.city = strdup(city);
			loc.coords = std::string(latitude) + "," + std::string(longitude);
			locations.push_back(loc);
			l1 = xmlNextNode(l1);
		}
	}

	xmlFreeDoc(parser);
}
