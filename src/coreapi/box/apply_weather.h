/*
 * apply_weather.h - what makes a changed weather setting take effect
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

#ifndef __coreapi_apply_weather_h__
#define __coreapi_apply_weather_h__

#include "coreapi/base/apply.h"
#include "coreapi/base/result.h"

#include <condition_variable>
#include <mutex>
#include <string>

#include <pthread.h>

namespace coreapi
{

/* The program's weather service, which keeps the place and the access it last
   fetched with and fetches again when either differs. A seam so the group runs
   where no service exists. Both calls compare against what the service holds
   and are harmless when sent again unchanged. They return at once: a fetch is an
   HTTP request of up to a minute, and the caller is the program's loop, which
   must not stand still for it. */
struct WeatherService
{
	virtual ~WeatherService() {}

	// The coordinates "latitude,longitude" and the name shown for them.
	virtual Status setPlace(const std::string &coords, const std::string &city) = 0;
	// The key and the version of the interface, as the settings held them when this was asked.
	virtual Status refreshApi(const std::string &key, const std::string &version) = 0;
};

/* What is to be fetched: a new place, a new access, or both. */
struct WeatherJob
{
	bool        place;
	std::string coords;
	std::string city;
	bool        api;
	// Read by whoever posts, because the thread that fetches reads no setting that holds text.
	std::string api_key;
	std::string api_version;

	WeatherJob() : place(false), api(false) {}
};

/* Runs jobs one at a time on a thread of its own, never two at once. A job posted
   while one runs is kept and the ones after it are merged into it, the newest place
   winning, so a burst of writes costs one more fetch and not one each. post()
   returns at once; where no thread can be made, the fetch is skipped and logged. */
class WeatherWorker
{
public:
	typedef void (*Run)(const WeatherJob &);
	/* prepare, when given, runs on the posting thread before the job is queued, for
	   whatever the job's thread must not be the first to make. */
	explicit WeatherWorker(Run run, void (*prepare)() = 0);
	~WeatherWorker();

	void post(const WeatherJob &job);
	// Returns once nothing runs and nothing waits. For the end of a case.
	void wait();

private:
	static void *main(void *arg);
	void loop();

	Run                run_;
	void             (*prepare_)();
	std::mutex         m_;
	std::condition_variable idle_;
	pthread_t          thread_;
	bool               joinable_;
	bool               running_;
	WeatherJob         pending_;
};

/* A weather service that hands what it is asked to a worker and returns. The
   result is the service's own state, read when the weather is drawn next. */
class QueuedWeatherService : public WeatherService
{
public:
	QueuedWeatherService(WeatherWorker::Run run, void (*prepare)() = 0) : worker_(run, prepare) {}

	Status setPlace(const std::string &coords, const std::string &city);
	Status refreshApi(const std::string &key, const std::string &version);
	void wait() { worker_.wait(); }

private:
	WeatherWorker worker_;
};

// NotSupported while nothing is installed.
WeatherService &weatherService();
void setWeatherService(WeatherService *w);

// Binds the accessor above to the program's weather service.
void installRealWeatherService();

/* Defined by the application, because the service is an object whose header
   reaches the GUI and this layer must not. The first makes the service where the
   loop does, so that the worker is never the one to; the second is a job as the
   worker runs it, and may take a minute. */
void applicationPrepareWeather();
void applicationRunWeatherJob(const WeatherJob &job);

/* Gives the service the place and the access the settings hold. The application
   calls it once when its startup is done, which is where the first fetch always
   was; the group does it for every change after that. */
Status fetchWeather();

// Forgets that the first run has passed, for a case that needs it again.
void resetSentWeather();

/* The place, the key and the version of the interface the weather is fetched
   with. The first run, which is startup, only learns; the application fetches at
   the end of its startup. Not the switch and not the postal code: the switch is read where the
   weather is drawn, and the code is only what the lookup of a place starts from. */
extern const ApplyGroup kWeatherApplyGroup;

} // namespace coreapi

#endif
