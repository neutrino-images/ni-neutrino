/*
 * apply_weather.cpp - what makes a changed weather setting take effect
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

#include "coreapi/box/apply_weather.h"
#include "coreapi/box/sentstate.h"

#include <system/settings.h>

#include <stdio.h>
#include <string.h>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

class NoWeatherService : public WeatherService
{
	public:
		Status setPlace(const std::string &, const std::string &) { return Status::NotSupported; }
		Status refreshApi(const std::string &, const std::string &) { return Status::NotSupported; }
};

NoWeatherService g_no_weather_service;
WeatherService *g_weather_service = 0;

bool g_started = false;

/* The first run is startup, and the first fetch is the application's own, at the
   end of its startup, so this one only learns. */
Status runWeather()
{
	if (!g_started)
	{
		g_started = true;
		return Status::Ok;
	}
	return fetchWeather();
}

const char *const kWeatherKeys[] =
{
	"weather_api_key",
	"weather_api_version",
	"weather_city",
	"weather_location"
};

} // namespace

WeatherWorker::WeatherWorker(Run run, void (*prepare)()) : run_(run), prepare_(prepare), thread_(), joinable_(false), running_(false) {}

WeatherWorker::~WeatherWorker()
{
	wait();
}

void WeatherWorker::post(const WeatherJob &job)
{
	if (prepare_)
		prepare_();

	std::lock_guard<std::mutex> lock(m_);
	if (job.place)
	{
		pending_.place = true;
		pending_.coords = job.coords;
		pending_.city = job.city;
	}
	if (job.api)
	{
		pending_.api = true;
		pending_.api_key = job.api_key;
		pending_.api_version = job.api_version;
	}
	if (running_)
		return;
	// The last worker has left its loop, so this does not wait.
	if (joinable_)
	{
		pthread_join(thread_, NULL);
		joinable_ = false;
	}
	const int err = pthread_create(&thread_, NULL, &WeatherWorker::main, this);
	if (err != 0)
	{
		pending_ = WeatherJob();
		printf("[weather] no thread for the fetch, skipped: %s\n", strerror(err));
		return;
	}
	joinable_ = true;
	running_ = true;
}

void *WeatherWorker::main(void *arg)
{
	static_cast<WeatherWorker *>(arg)->loop();
	return NULL;
}

void WeatherWorker::loop()
{
	for (;;)
	{
		WeatherJob job;
		{
			std::lock_guard<std::mutex> lock(m_);
			if (!pending_.place && !pending_.api)
			{
				running_ = false;
				idle_.notify_all();
				return;
			}
			job = pending_;
			pending_ = WeatherJob();
		}
		run_(job);
	}
}

void WeatherWorker::wait()
{
	std::unique_lock<std::mutex> lock(m_);
	idle_.wait(lock, [this]() { return !running_; });
	if (joinable_)
	{
		pthread_join(thread_, NULL);
		joinable_ = false;
	}
}

Status QueuedWeatherService::setPlace(const std::string &coords, const std::string &city)
{
	WeatherJob job;
	job.place = true;
	job.coords = coords;
	job.city = city;
	worker_.post(job);
	return Status::Ok;
}

Status QueuedWeatherService::refreshApi(const std::string &key, const std::string &version)
{
	WeatherJob job;
	job.api = true;
	job.api_key = key;
	job.api_version = version;
	worker_.post(job);
	return Status::Ok;
}

/* The place first: the service has fetched nothing yet at startup, and a fetch
   for the access before the place would go out with no coordinates and be
   followed at once by the one for the place. The service compares both against
   what it holds, so a run for a sibling key fetches nothing it has. */
Status fetchWeather()
{
	WeatherService &w = weatherService();
	Status first = Status::Ok;
	noteFirst(first, w.setPlace(settingsText(g_settings.weather_location), settingsText(g_settings.weather_city)));
	noteFirst(first, w.refreshApi(settingsText(g_settings.weather_api_key), settingsText(g_settings.weather_api_version)));
	return first;
}

void resetSentWeather() { g_started = false; }

WeatherService &weatherService()
{
	if (!g_weather_service)
		return g_no_weather_service;
	return *g_weather_service;
}

void setWeatherService(WeatherService *w) { g_weather_service = w; }

/* After the network, which the fetches go out over. */
const ApplyGroup kWeatherApplyGroup = { "weather", ApplyPhase::Network, COREAPI_KEYS(kWeatherKeys), &runWeather };

} // namespace coreapi
