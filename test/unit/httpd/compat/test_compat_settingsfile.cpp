/*
 * test_compat_settingsfile.cpp - what the legacy calls read out of a settings file
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

#include "support/catch.hpp"

// compat/ is not compiled with --disable-legacy-api (src/httpd/Makefile.am).
#include <config.h>

#ifndef DISABLE_LEGACY_API

#include "httpd/compat/settingsfile.h"

#include "coreapi/settings/settings.h"
#include "coreapi/settings/settingstable.h"

#include <string>
#include <vector>

using httpd::compat::CStringList;
using httpd::compat::legacyConfigReadable;

namespace
{

/* The names a secret row's value has in the settings file. A list files each
   entry with an index and the count beside them. A records row files each member
   under the row's stem with an index, and of those only the secret members are
   a credential. */
std::vector<std::string> fileKeysOf(const coreapi::Descriptor &d)
{
	std::vector<std::string> out;
	const std::string key(d.key);
	const size_t cut = key.rfind('_');
	if (d.type == coreapi::ValueType::Records && cut != std::string::npos)
	{
		const std::string stem = key.substr(0, cut + 1);
		const coreapi::FieldExtra *x = d.field.extra;
		for (size_t m = 0; x != NULL && m < x->record_field_count; ++m)
			if (x->record_fields[m].secret)
			{
				out.push_back(stem + x->record_fields[m].name + "_0");
				out.push_back(stem + x->record_fields[m].name + "_7");
			}
		return out;
	}
	out.push_back(key);
	if (d.type != coreapi::ValueType::List)
		return out;
	out.push_back(key + "_0");
	out.push_back(key + "_7");
	out.push_back(key + "_count");
	return out;
}

std::vector<std::string> secretFileKeys()
{
	std::vector<std::string> out;
	const coreapi::Descriptor *t = coreapi::settingsTable();
	const size_t n = coreapi::settingsTableCount();
	for (size_t i = 0; i < n; ++i)
	{
		if (t[i].key == NULL || !t[i].secret)
			continue;
		const std::vector<std::string> keys = fileKeysOf(t[i]);
		out.insert(out.end(), keys.begin(), keys.end());
	}
	return out;
}

// Credentials no row declares, each a spelling the name rule has to catch.
const char *const kUndeclaredSecrets[] =
{
	"network_nfs_password_0", "network_nfs_password_3", "youtube_dev_id",
	"plugin_passwd", "cloud_token", "cloud_secret", "box_pin", "service_apikey",
	"service_api_key_2"
};

// Entries that stay, among them names that only look like a credential.
const char *const kReadable[] =
{
	"ci_save_pincode_1", "ac3_pass", "dts_pass", "keyboard_layout", "zappingmode",
	"network_nfs_username_0", "network_nfs_ip_0", "softupdate_proxyusername",
	"softupdate_proxyserver", "webtv_xml_0", "webtv_xml_count", "usermenu_key_red",
	"tmdb_enabled", "timer_remotebox_ip_count_unrelated", "language"
};

CStringList everything()
{
	CStringList all;
	const std::vector<std::string> secrets = secretFileKeys();
	for (size_t i = 0; i < secrets.size(); ++i)
		all[secrets[i]] = "s3cr3t";
	for (size_t i = 0; i < sizeof(kUndeclaredSecrets) / sizeof(kUndeclaredSecrets[0]); ++i)
		all[kUndeclaredSecrets[i]] = "s3cr3t";
	for (size_t i = 0; i < sizeof(kReadable) / sizeof(kReadable[0]); ++i)
		all[kReadable[i]] = "1";
	return all;
}

} // namespace

TEST_CASE("no row the settings declare secret leaves through /control/config", "[compat][settingsfile]")
{
	const std::vector<std::string> secrets = secretFileKeys();
	// A table that lost its secret marks would pass the loop below on nothing.
	REQUIRE(secrets.size() >= 14);

	const CStringList out = legacyConfigReadable("neutrino", everything());
	for (size_t i = 0; i < secrets.size(); ++i)
	{
		INFO("entry " << secrets[i]);
		CHECK(out.find(secrets[i]) == out.end());
	}
}

TEST_CASE("of a remote box only the password is withheld", "[compat][settingsfile]")
{
	// The names neutrino.cpp files the records under, which not every member name spells.
	const char *const kReadableMembers[] =
	{
		"timer_remotebox_ip_0", "timer_remotebox_rbname_0", "timer_remotebox_user_0",
		"timer_remotebox_port_0", "timer_remotebox_enabled_0", "timer_remotebox_ip_count",
		"timer_remotebox_ip_1", "timer_remotebox_user_1"
	};
	CStringList all;
	for (size_t i = 0; i < sizeof(kReadableMembers) / sizeof(kReadableMembers[0]); ++i)
		all[kReadableMembers[i]] = "x";
	all["timer_remotebox_pass_0"] = "s3cr3t";
	all["timer_remotebox_pass_1"] = "s3cr3t";
	const coreapi::Descriptor *row = coreapi::settings::findRow("timer_remotebox_ip");
	REQUIRE(row != NULL);
	REQUIRE(row->secret);

	const CStringList out = legacyConfigReadable("neutrino", all);
	CHECK(out.find("timer_remotebox_pass_0") == out.end());
	CHECK(out.find("timer_remotebox_pass_1") == out.end());
	for (size_t i = 0; i < sizeof(kReadableMembers) / sizeof(kReadableMembers[0]); ++i)
	{
		INFO("entry " << kReadableMembers[i]);
		CHECK(out.find(kReadableMembers[i]) != out.end());
	}
}

TEST_CASE("a credential nothing declares is withheld by its name", "[compat][settingsfile]")
{
	const CStringList out = legacyConfigReadable("neutrino", everything());
	for (size_t i = 0; i < sizeof(kUndeclaredSecrets) / sizeof(kUndeclaredSecrets[0]); ++i)
	{
		INFO("entry " << kUndeclaredSecrets[i]);
		REQUIRE(coreapi::settings::findRow(kUndeclaredSecrets[i]) == NULL);
		CHECK(out.find(kUndeclaredSecrets[i]) == out.end());
	}

	CStringList web;
	web["mod_auth.password"] = "x";
	web["mod_auth.username"] = "root";
	const CStringList shown = legacyConfigReadable("nhttpd", web);
	CHECK(shown.size() == 1);
	CHECK(shown.count("mod_auth.username") == 1);
}

TEST_CASE("every other entry is still handed out as it is", "[compat][settingsfile]")
{
	const CStringList out = legacyConfigReadable("neutrino", everything());
	for (size_t i = 0; i < sizeof(kReadable) / sizeof(kReadable[0]); ++i)
	{
		INFO("entry " << kReadable[i]);
		REQUIRE(out.count(kReadable[i]) == 1);
		CHECK(out.at(kReadable[i]) == "1");
	}
	CHECK(out.size() == sizeof(kReadable) / sizeof(kReadable[0]));
}

#endif // DISABLE_LEGACY_API
