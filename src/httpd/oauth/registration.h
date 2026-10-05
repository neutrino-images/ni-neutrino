/*
 * registration.h - dynamic client registration (RFC 7591)
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

#ifndef __httpd_oauth_registration_h__
#define __httpd_oauth_registration_h__

#include "httpd/endpoint.h"

#include <cstddef>
#include <string>

namespace httpd
{
namespace oauth
{

const size_t   kMaxRegisterBytes       = 16384;
const unsigned kRegistrationsPerMinute = 10;

Response answerRegister(const std::string &body, const std::string &content_type);
void resetRegistrationLimitForTest();

} // namespace oauth
} // namespace httpd

#endif
