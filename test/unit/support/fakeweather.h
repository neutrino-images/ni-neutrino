/*
 * fakeweather.h - the weather group's weather service as a case sees it
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

#ifndef __support_fakeweather_h__
#define __support_fakeweather_h__

#include "coreapi/box/apply_weather.h"

#include <string>
#include <utility>
#include <vector>

// Every call the weather group makes, in order, with what it carried.
struct FakeWeatherService : public coreapi::WeatherService
{
	std::vector<std::string> calls;
	std::vector<std::pair<std::string, std::string> > places;
	std::vector<std::pair<std::string, std::string> > accesses;
	coreapi::Status place_answer;

	FakeWeatherService() : place_answer(coreapi::Status::Ok) {}

	coreapi::Status setPlace(const std::string &coords, const std::string &city)
	{
		calls.push_back("place");
		places.push_back(std::make_pair(coords, city));
		return place_answer;
	}

	coreapi::Status refreshApi(const std::string &key, const std::string &version)
	{
		calls.push_back("api");
		accesses.push_back(std::make_pair(key, version));
		return coreapi::Status::Ok;
	}

	void forget()
	{
		calls.clear();
		places.clear();
		accesses.clear();
	}
};

#endif
