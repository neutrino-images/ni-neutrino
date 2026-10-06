/*
 * twofactor.h - the two-factor secret in effect, its last used step and a pending setup
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

#ifndef __httpd_oauth_twofactor_h__
#define __httpd_oauth_twofactor_h__

#include <ctime>
#include <string>

namespace httpd
{
namespace oauth
{

const time_t kPendingTotpSeconds = 600;

// What ni-web.conf holds; an empty secret is off. Called by the configuration loader.
void installTotp(const std::string &secret_base32, long long last_step);

bool totpActive();

enum class CodeUse
{
	Accepted,
	Wrong,
	ClockUnknown,
	NotSetUp
};

// record false checks without using the code up, for a sign-in refused on its password.
CodeUse useTotpCode(const std::string &typed, bool record);

struct TotpSetup
{
	std::string secret;
	std::string uri;
};

// A fresh secret held as pending, replacing one already pending; false without randomness.
bool startTotpSetup(const std::string &account, TotpSetup *out);

enum class SetupConfirm
{
	Activated,
	NoPending,
	Wrong,
	ClockUnknown,
	NotSaved
};

SetupConfirm confirmTotpSetup(const std::string &typed);

// False when the file could not be written; then nothing changed.
bool removeTotp();

time_t totpNow();

// Nought puts the real clock back.
void setTotpClockForTest(time_t now);
// The device cannot be made to run out, so a case makes the draw fail here.
void setTotpDrawFailsForTest(bool fails);
void forgetPendingTotpForTest();
long long totpLastStepForTest();

} // namespace oauth
} // namespace httpd

#endif
