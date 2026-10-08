/*
 * weather_real.cpp - the weather group's seam bound to the program's weather service
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

namespace coreapi
{

/* The service is made once and kept: its worker may be in a fetch when the program
   ends, and a destructor would wait for it. */
void installRealWeatherService()
{
	setWeatherService(new QueuedWeatherService(&applicationRunWeatherJob, &applicationPrepareWeather));
}

} // namespace coreapi
