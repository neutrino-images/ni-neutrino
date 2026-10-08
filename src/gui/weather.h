/*
	Copyright (C) 2017, 2018, 2019, 2020 TangoCash

	“Powered by OpenWeather” https://openweathermap.org/api/one-call-api

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation;

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __WEATHER__
#define __WEATHER__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <mutex>
#include <string>
#include <time.h>
#include <vector>

#include "system/settings.h"
#include "system/helpers.h"

#include <gui/components/cc.h>

struct current_data
{
	time_t timestamp;
	std::string icon;
	std::string icon_only_name;
	float temperature;
	float humidity;
	float pressure;
	float windSpeed;
	int windBearing;

	current_data():
		timestamp(0),
		icon("unknown.png"),
		icon_only_name("unknown"),
		temperature(0),
		humidity(0),
		pressure(0),
		windSpeed(0),
		windBearing(0)
	{}
};

typedef struct
{
	time_t timestamp;
	int weekday; // 0=Sunday, 1=Monday, ...
	std::string icon;
	std::string icon_only_name;
	float temperatureMin;
	float temperatureMax;
	time_t sunriseTime;
	time_t sunsetTime;
	float windSpeed;
	int windBearing;
} forecast_data;

class CWeather
{
	private:
		std::string coords;
		std::string city;
		std::string timezone;
		current_data current;
		std::vector<forecast_data> v_forecast;
		CComponentsForm *form;
		std::string key;
		std::string api;
		bool GetWeatherDetails();
		time_t last_time;
		std::string getDirectionString(int degree);
		/* The worker that fetches, the two display threads and the loop all read and
		   write the members; never held across a fetch. */
		std::mutex mutex;
		// A copy of entry i, clamped to the last one, or an empty one when there is none.
		forecast_data forecastAt(int i);

	public:
		static CWeather *getInstance();
		CWeather();
		~CWeather();
		void updateApi();
		// For a caller that is not the loop and so has read the settings itself.
		void updateApi(const std::string &new_key, const std::string &new_api);
		bool checkUpdate(bool forceUpdate = false);
		void setCoords(std::string new_coords, std::string new_city = "Unknown");
		bool FindCoords(std::string postalcode, std::string country = "DE");

		// globals
		std::string getCity()
		{
			std::lock_guard<std::mutex> g(mutex);
			return city;
		};

		// current conditions
#if 0
		std::string getCurrentTimestamp()
		{
			return to_string((int)(current.timestamp));
		};
#endif
		time_t getCurrentTimestamp()
		{
			std::lock_guard<std::mutex> g(mutex);
			return current.timestamp;
		};
		std::string getCurrentTemperature()
		{
			std::lock_guard<std::mutex> g(mutex);
			return to_string((int)(current.temperature + 0.5));
		};
		std::string getCurrentHumidity()
		{
			std::lock_guard<std::mutex> g(mutex);
			return to_string((int)(current.humidity * 100.0));
		};
		std::string getCurrentPressure()
		{
			std::lock_guard<std::mutex> g(mutex);
			return to_string(current.pressure);
		};
		std::string getCurrentWindSpeed()
		{
			std::lock_guard<std::mutex> g(mutex);
			return to_string(current.windSpeed);
		};
		std::string getCurrentWindBearing()
		{
			std::lock_guard<std::mutex> g(mutex);
			return to_string(current.windBearing);
		};
		std::string getCurrentWindDirection()
		{
			int bearing;
			{
				std::lock_guard<std::mutex> g(mutex);
				bearing = current.windBearing;
			}
			return getDirectionString(bearing);
		};
		std::string getCurrentIcon()
		{
			std::lock_guard<std::mutex> g(mutex);
			return ICONSDIR"/weather/" + current.icon;
		};
		std::string getCurrentIconOnlyName()
		{
			std::lock_guard<std::mutex> g(mutex);
			return current.icon_only_name;
		};

		// forecast conditions
		int getForecastSize()
		{
			std::lock_guard<std::mutex> g(mutex);
			return (int)v_forecast.size();
		};
		int getForecastWeekday(int i = 0)
		{
			return forecastAt(i).weekday;
		};
		std::string getForecastTemperatureMin(int i = 0)
		{
			return to_string((int)(forecastAt(i).temperatureMin + 0.5));
		};
		std::string getForecastTemperatureMax(int i = 0)
		{
			return to_string((int)(forecastAt(i).temperatureMax + 0.5));
		};
		std::string getForecastWindSpeed(int i = 0)
		{
			return to_string(forecastAt(i).windSpeed);
		};
		std::string getForecastWindBearing(int i = 0)
		{
			return to_string(forecastAt(i).windBearing);
		};
		std::string getForecastWindDirection(int i = 0)
		{
			return getDirectionString(forecastAt(i).windBearing);
		};
		std::string getForecastIcon(int i = 0)
		{
			return ICONSDIR"/weather/" + forecastAt(i).icon;
		};
		std::string getForecastIconOnlyNane(int i = 0)
		{
			return forecastAt(i).icon_only_name;
		};

		void show(int x = 50, int y = 50);
		void hide();
};

#endif
