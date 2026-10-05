/*
 * sourcelimit.h - how often one address may ask the anonymous OAuth endpoints
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

#ifndef __httpd_oauth_sourcelimit_h__
#define __httpd_oauth_sourcelimit_h__

#include <cstddef>
#include <string>

#include <time.h>

namespace httpd
{
namespace oauth
{

// Per source address and minute; the box-wide registration limit stays the outer bound.
const unsigned kRegisterPerSource  = 5;
const unsigned kAuthorizePerSource = 20;
const unsigned kConsentPerSource   = 10;
const time_t   kSourceWindow       = 60;
// Full, the source whose window began first makes room.
const size_t   kMaxSources         = 256;

enum class Limited
{
	Register,
	Authorize,
	Consent
};

// retry_after gets the seconds until the window ends, 0 on a pass.
bool sourceAllows(Limited what, const std::string &source, unsigned *retry_after);

// Nought puts the real clock back.
void setSourceClockForTest(time_t now);
void forgetSourceLimitsForTest();
size_t sourceCountForTest();

} // namespace oauth
} // namespace httpd

#endif
