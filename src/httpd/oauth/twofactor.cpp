/*
 * twofactor.cpp - the two-factor secret in effect, its last used step and a pending setup
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

#include "httpd/oauth/twofactor.h"

#include "httpd/oauth/totp.h"
#include "httpd/randomsource.h"
#include "httpd/webconfig.h"

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <openssl/crypto.h>

namespace httpd
{
namespace oauth
{

namespace
{

struct State
{
	std::string secret;
	long long   last_step;
	std::string pending;
	time_t      pending_since;
	time_t      test_clock;
	bool        test_draw_fails;

	State() : last_step(0), pending_since(0), test_clock(0), test_draw_fails(false)
	{
	}
};

typedef OpenThreads::ScopedLock<OpenThreads::Mutex> Held;

OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

State &state()
{
	static State s;
	return s;
}

time_t nowLocked()
{
	return state().test_clock != 0 ? state().test_clock : time(NULL);
}

void wipe(std::string &s)
{
	if (!s.empty())
		OPENSSL_cleanse(&s[0], s.size());
	s.clear();
}

void dropPendingLocked()
{
	wipe(state().pending);
	state().pending_since = 0;
}

bool pendingLiveLocked(time_t now)
{
	if (state().pending.empty())
		return false;
	if (now < state().pending_since || now - state().pending_since > kPendingTotpSeconds)
	{
		dropPendingLocked();
		return false;
	}
	return true;
}

std::string keyOf(const std::string &secret)
{
	std::string key;
	return base32Decode(secret, &key) ? key : std::string();
}

} // namespace

void installTotp(const std::string &secret_base32, long long last_step)
{
	Held h(lock());
	wipe(state().secret);
	state().secret = secret_base32;
	state().last_step = last_step;
}

bool totpActive()
{
	Held h(lock());
	return !state().secret.empty();
}

CodeUse useTotpCode(const std::string &typed, bool record)
{
	Held h(lock());
	if (state().secret.empty())
		return CodeUse::NotSetUp;
	long long step = 0;
	switch (checkTotp(keyOf(state().secret), typed, nowLocked(), state().last_step, &step))
	{
		case TotpCheck::ClockUnknown:
			return CodeUse::ClockUnknown;
		case TotpCheck::Wrong:
			return CodeUse::Wrong;
		case TotpCheck::Accepted:
			break;
	}
	if (record)
	{
		state().last_step = step;
		// Kept in memory even if this fails; a restart could take this step once more.
		saveAiTotp(configPath(), state().secret, step);
	}
	return CodeUse::Accepted;
}

bool startTotpSetup(const std::string &account, TotpSetup *out)
{
	{
		Held h(lock());
		if (state().test_draw_fails)
			return false;
	}
	unsigned char raw[kTotpSecretBytes];
	if (!randomBytes(raw, sizeof(raw)))
		return false;
	const std::string secret = base32Encode(std::string((const char *) raw, sizeof(raw)));
	OPENSSL_cleanse(raw, sizeof(raw));

	Held h(lock());
	dropPendingLocked();
	state().pending = secret;
	state().pending_since = nowLocked();
	out->secret = secret;
	out->uri = otpauthUri(secret, account);
	return true;
}

SetupConfirm confirmTotpSetup(const std::string &typed)
{
	Held h(lock());
	const time_t now = nowLocked();
	if (!pendingLiveLocked(now))
		return SetupConfirm::NoPending;
	long long step = 0;
	switch (checkTotp(keyOf(state().pending), typed, now, 0, &step))
	{
		case TotpCheck::ClockUnknown:
			return SetupConfirm::ClockUnknown;
		case TotpCheck::Wrong:
			return SetupConfirm::Wrong;
		case TotpCheck::Accepted:
			break;
	}
	if (!saveAiTotp(configPath(), state().pending, step))
		return SetupConfirm::NotSaved;
	wipe(state().secret);
	state().secret = state().pending;
	state().last_step = step;
	dropPendingLocked();
	return SetupConfirm::Activated;
}

bool removeTotp()
{
	Held h(lock());
	if (!saveAiTotp(configPath(), std::string(), 0))
		return false;
	wipe(state().secret);
	state().last_step = 0;
	dropPendingLocked();
	return true;
}

time_t totpNow()
{
	Held h(lock());
	return nowLocked();
}

void setTotpClockForTest(time_t now)
{
	Held h(lock());
	state().test_clock = now;
}

void setTotpDrawFailsForTest(bool fails)
{
	Held h(lock());
	state().test_draw_fails = fails;
}

void forgetPendingTotpForTest()
{
	Held h(lock());
	dropPendingLocked();
}

long long totpLastStepForTest()
{
	Held h(lock());
	return state().last_step;
}

} // namespace oauth
} // namespace httpd
