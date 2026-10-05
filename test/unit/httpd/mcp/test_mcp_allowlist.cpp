/*
 * test_mcp_allowlist.cpp - what an AI client may start and may change
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

#include "support/answers.h"
#include "support/catch.hpp"
#include "support/fakes.h"

#include "httpd/mcp/allowlist.h"
#include "httpd/mcp/contract.h"
#include "httpd/mcp/jsonrpc.h"
#include "httpd/router.h"
#include "httpd/webconfig.h"

#include "coreapi/settings/settings.h"

#include "toolcaller.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>

#include <unistd.h>

using namespace httpd;

namespace
{

struct Lists
{
	~Lists()
	{
		mcp::installAllowlists(mcp::Allowlists());
		setConfigForTest(defaultWebConfig());
	}
};

std::string tempConf(const std::string &lines)
{
	char tmpl[] = "/tmp/mcp_allow_XXXXXX";
	const int fd = mkstemp(tmpl);
	if (fd < 0)
		return std::string();
	const ssize_t n = ::write(fd, lines.data(), lines.size());
	::close(fd);
	return n == (ssize_t) lines.size() ? std::string(tmpl) : std::string();
}

// A temp ni-web.conf loaded as the file the routes save to.
struct ConfFile
{
	std::string path;

	ConfFile() : path(tempConf(std::string()))
	{
		REQUIRE(load(path));
	}

	~ConfFile()
	{
		::unlink(path.c_str());
	}

	std::string text() const
	{
		std::ifstream in(path.c_str());
		std::ostringstream out;
		out << in.rdbuf();
		return out.str();
	}
};

Response aiAsk(Method m, const std::string &body = std::string())
{
	return dispatch(m, "/api/v1/ai/allowlists", std::string(), body, "192.168.1.9", AuthLevel::System);
}

const mcp::ToolDef *named(const std::vector<mcp::ToolDef> &all, const char *name)
{
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].name == name)
			return &all[i];
	}
	return NULL;
}

} // namespace

TEST_CASE("the deny set names network parental update and every section with a secret", "[allowlist]")
{
	REQUIRE(mcp::sectionDenial("network") == "network");
	REQUIRE(mcp::sectionDenial("parental") == "parental");
	REQUIRE(mcp::sectionDenial("update") == "update");
	REQUIRE(mcp::sectionDenial("misc") == "secret");
	REQUIRE(mcp::sectionDenial("weather") == "secret");
	const char *open[] = { "audio", "cam", "channel", "display", "hdd", "keybindings", "osd", "player",
		"recording", "video" };
	for (size_t i = 0; i < sizeof(open) / sizeof(open[0]); ++i)
	{
		INFO(open[i]);
		REQUIRE(mcp::sectionDenial(open[i]).empty());
	}
}

TEST_CASE("an allowlist is empty until installed and answers at once after", "[allowlist]")
{
	Lists back;
	REQUIRE_FALSE(mcp::pluginAllowed("Tierpark"));
	REQUIRE_FALSE(mcp::sectionAllowed("audio"));
	mcp::Allowlists a;
	a.plugins.push_back("Tierpark");
	a.sections.push_back("audio");
	a.sections.push_back("network");
	mcp::installAllowlists(a);
	REQUIRE(mcp::pluginAllowed("Tierpark"));
	REQUIRE_FALSE(mcp::pluginAllowed("tierpark"));
	REQUIRE(mcp::sectionAllowed("audio"));
	REQUIRE_FALSE(mcp::sectionAllowed("network"));
}

TEST_CASE("the file's lists are read and denied or unknown sections dropped with a word", "[allowlist]")
{
	Lists back;
	const std::string path = tempConf(
		"ai_allowed_plugins=Tierpark,Wetter\n"
		"ai_allowed_sections=audio,network,nonsense,video\n");
	REQUIRE(load(path));
	REQUIRE(config().ai_allowed_plugins.size() == 2);
	REQUIRE(config().ai_allowed_sections.size() == 2);
	REQUIRE(mcp::sectionAllowed("audio"));
	REQUIRE(mcp::sectionAllowed("video"));
	REQUIRE_FALSE(mcp::sectionAllowed("network"));
	REQUIRE(mcp::pluginAllowed("Wetter"));
	std::string problems;
	for (size_t i = 0; i < configProblems().size(); ++i)
		problems += configProblems()[i] + "\n";
	REQUIRE(problems.find("network") != std::string::npos);
	REQUIRE(problems.find("nonsense") != std::string::npos);
	unlink(path.c_str());
}

TEST_CASE("saved lists read back as given and install live", "[allowlist]")
{
	Lists back;
	const std::string path = tempConf("ai_enabled=true\n");
	REQUIRE(load(path));
	std::vector<std::string> plugins(1, "Tierpark");
	std::vector<std::string> sections(1, "osd");
	REQUIRE(saveAiAllowlists(path, plugins, sections));
	REQUIRE(mcp::pluginAllowed("Tierpark"));
	REQUIRE(mcp::sectionAllowed("osd"));
	std::ifstream in(path.c_str());
	std::stringstream all;
	all << in.rdbuf();
	REQUIRE(all.str().find("ai_allowed_plugins=Tierpark") != std::string::npos);
	REQUIRE(all.str().find("ai_allowed_sections=osd") != std::string::npos);
	REQUIRE(all.str().find("ai_enabled=true") != std::string::npos);

	const std::string over(65, 'B');
	REQUIRE_FALSE(saveAiAllowlists(path, std::vector<std::string>(1, over), sections));
	REQUIRE(mcp::pluginAllowed("Tierpark"));
	REQUIRE_FALSE(mcp::pluginAllowed(over));
	std::ifstream still(path.c_str());
	std::stringstream stillAll;
	stillAll << still.rdbuf();
	REQUIRE(stillAll.str() == all.str());

	const std::string atCeiling(64, 'A');
	REQUIRE(saveAiAllowlists(path, std::vector<std::string>(1, atCeiling), sections));
	REQUIRE(mcp::pluginAllowed(atCeiling));

	unlink(path.c_str());
}

TEST_CASE("the allowlists answer every plugin and section with its state", "[allowlist][routes]")
{
	Lists back;
	ConfFile conf;
	FakePluginSource plugins;
	plugins.add("Tierpark");
	InstalledPluginSource in_plugins(&plugins);
	Response r = aiAsk(Get);
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find("{\"name\":\"Tierpark\",\"allowed\":false}") != std::string::npos);
	REQUIRE(r.body.find("{\"id\":\"network\",\"allowed\":false,\"denied\":\"network\"}") != std::string::npos);
	REQUIRE(r.body.find("{\"id\":\"audio\",\"allowed\":false,\"denied\":\"\"}") != std::string::npos);
}

TEST_CASE("a PUT saves the lists and they hold at once", "[allowlist][routes]")
{
	Lists back;
	ConfFile conf;
	FakePluginSource plugins;
	InstalledPluginSource in_plugins(&plugins);
	Response r = aiAsk(Put, "{\"plugins\":\"Tierpark,Wetter\",\"sections\":\"audio,video\"}");
	REQUIRE(r.code == 200);
	REQUIRE(mcp::pluginAllowed("Wetter"));
	REQUIRE(mcp::sectionAllowed("video"));
	REQUIRE(conf.text().find("ai_allowed_sections=audio,video") != std::string::npos);
	REQUIRE(aiAsk(Put, "{\"sections\":\"\"}").code == 200);
	REQUIRE_FALSE(mcp::sectionAllowed("audio"));
	REQUIRE(mcp::pluginAllowed("Tierpark"));
}

TEST_CASE("a PUT finds its members by name and not by a word inside the text", "[allowlist][routes]")
{
	Lists back;
	ConfFile conf;
	FakePluginSource plugins;
	InstalledPluginSource in_plugins(&plugins);
	REQUIRE(aiAsk(Put, "{\"sections\":\"audio\"}").code == 200);
	REQUIRE(mcp::sectionAllowed("audio"));
	Response r = aiAsk(Put, "{\"plugins\":\"sections\"}");
	REQUIRE(r.code == 200);
	REQUIRE(mcp::sectionAllowed("audio"));
	REQUIRE(conf.text().find("ai_allowed_sections=audio") != std::string::npos);
}

TEST_CASE("a PUT refuses what may never be allowed and changes nothing", "[allowlist][routes]")
{
	Lists back;
	ConfFile conf;
	REQUIRE(aiAsk(Put, "{\"sections\":\"audio,parental\"}").code == 400);
	REQUIRE(aiAsk(Put, "{\"sections\":\"nonsense\"}").code == 404);
	REQUIRE(aiAsk(Put, "{\"plugins\":\"Tier\\npark\"}").code == 400);
	REQUIRE(aiAsk(Put, "{\"plugins\":\"" + std::string(300, 'p') + "\"}").code == 400);
	REQUIRE(aiAsk(Put, "{}").code == 400);
	REQUIRE_FALSE(mcp::sectionAllowed("audio"));
	REQUIRE(conf.text().find("ai_allowed_sections=audio") == std::string::npos);
}

TEST_CASE("the body example the document gives for the allowlists is accepted", "[allowlist][routes]")
{
	Lists back;
	ConfFile conf;
	FakePluginSource plugins;
	InstalledPluginSource in_plugins(&plugins);
	CHECK(sendBodyExample("PUT", "/api/v1/ai/allowlists", std::map<std::string, std::string>()) == 200);
}

TEST_CASE("a configuration file that went away is a server fault for the allowlists", "[allowlist][routes]")
{
	Lists back;
	ConfFile conf;
	::unlink(conf.path.c_str());
	FakePluginSource plugins;
	InstalledPluginSource in_plugins(&plugins);
	const Response r = aiAsk(Put, "{\"plugins\":\"Tierpark\"}");
	REQUIRE(r.code == 500);
	REQUIRE(r.body.find("webserver-not-configured") != std::string::npos);
}

TEST_CASE("a gated tool is not offered while its allowlist is empty", "[allowlist][gate]")
{
	Lists back;
	mcp::installAllowlists(mcp::Allowlists());
	const std::vector<mcp::ToolDef> all = mcp::boxTools().list();
	REQUIRE(named(all, "start_plugin") == NULL);
	REQUIRE(named(all, "write_settings") == NULL);
	REQUIRE(mcp::boxTools().call(callerAt(AuthLevel::System), "start_plugin", "{\"name\":\"Tierpark\"}").error().code ==
	        coreapi::ErrorCode::PluginNotAllowed);
}

TEST_CASE("a gated tool keeps its route's system level and takes the allowlist as its enum", "[allowlist][gate]")
{
	Lists back;
	mcp::Allowlists a;
	a.plugins.push_back("Tierpark");
	a.sections.push_back("audio");
	mcp::installAllowlists(a);
	const std::vector<mcp::ToolDef> all = mcp::boxTools().list();
	const mcp::ToolDef *start = named(all, "start_plugin");
	const mcp::ToolDef *write = named(all, "write_settings");
	REQUIRE(start != NULL);
	REQUIRE(write != NULL);
	REQUIRE(start->level == AuthLevel::System);
	REQUIRE(write->level == AuthLevel::System);
	mcp::JsonValue in;
	REQUIRE(mcp::parseJson(start->input, 64, in));
	REQUIRE(in["properties"]["name"]["enum"].size() == 1);
	REQUIRE(in["properties"]["name"]["enum"][0].asString() == "Tierpark");
	REQUIRE(mcp::parseJson(write->input, 64, in));
	REQUIRE(in["properties"]["section"]["enum"].size() == 1);
	REQUIRE(in["properties"]["section"]["enum"][0].asString() == "audio");
}

TEST_CASE("every call is checked against the list as it is now", "[allowlist][gate]")
{
	Lists back;
	FakeEventSink events;
	InstalledEventSink in_events(&events);
	FakePluginSource plugins;
	InstalledPluginSource in_plugins(&plugins);
	mcp::Allowlists a;
	a.plugins.push_back("Tierpark");
	mcp::installAllowlists(a);
	REQUIRE(mcp::boxTools().call(callerAt(AuthLevel::System), "start_plugin", "{\"name\":\"Tierpark\"}").ok());
	REQUIRE(mcp::boxTools().call(callerAt(AuthLevel::System), "start_plugin", "{\"name\":\"Wetter\"}").error().code ==
	        coreapi::ErrorCode::PluginNotAllowed);
	mcp::installAllowlists(mcp::Allowlists());
	const size_t before = events.sent.size();
	REQUIRE(mcp::boxTools().call(callerAt(AuthLevel::System), "start_plugin", "{\"name\":\"Tierpark\"}").error().code ==
	        coreapi::ErrorCode::PluginNotAllowed);
	REQUIRE(events.sent.size() == before);
	REQUIRE(mcp::boxTools().call(callerAt(AuthLevel::Write), "start_plugin", "{\"name\":\"Tierpark\"}").error().code ==
	        coreapi::ErrorCode::NotPermitted);
}

TEST_CASE("write_settings refuses a section off the list or a denied one or a secret key", "[allowlist][gate]")
{
	Lists back;
	FakeSettingsSource store;
	InstalledSettingsSource in_store(&store);
	mcp::Allowlists a;
	a.sections.push_back("audio");
	mcp::installAllowlists(a);
	REQUIRE(mcp::boxTools().call(callerAt(AuthLevel::System), "write_settings",
		"{\"section\":\"osd\",\"settings\":{\"x\":\"1\"}}").error().code == coreapi::ErrorCode::SettingsSectionNotAllowed);
	REQUIRE(mcp::boxTools().call(callerAt(AuthLevel::System), "write_settings",
		"{\"section\":\"network\",\"settings\":{\"x\":\"1\"}}").error().code == coreapi::ErrorCode::SettingsSectionDenied);
	REQUIRE(mcp::deniedKeyIn("{\"tmdb_api_key\":\"abc\",\"auto_subs\":\"1\"}") == "tmdb_api_key");
	REQUIRE(mcp::deniedKeyIn("{\"auto_subs\":\"1\"}").empty());

	// A credential named in the body is refused before the route runs, whatever section
	// it is sent under.
	REQUIRE(mcp::boxTools().call(callerAt(AuthLevel::System), "write_settings",
		"{\"section\":\"audio\",\"settings\":{\"tmdb_api_key\":\"abc\"}}").error().code ==
		coreapi::ErrorCode::SettingsSectionDenied);

	// A key the gate does not refuse but the route does not carry under this section
	// reaches the route, which answers its own refusal.
	REQUIRE(mcp::boxTools().call(callerAt(AuthLevel::System), "write_settings",
		"{\"section\":\"audio\",\"settings\":{\"network_ntprefresh\":\"30\"}}").error().code ==
		coreapi::ErrorCode::UnknownSetting);

	REQUIRE(store.persisted == 0);
}

TEST_CASE("write_settings refuses every setting that names a place on the box's disk", "[allowlist][gate]")
{
	Lists back;
	FakeSettingsSource store;
	InstalledSettingsSource in_store(&store);
	mcp::Allowlists a;
	const char *const open[] = { "recording", "osd", "player", "channel", "misc" };
	for (size_t i = 0; i < sizeof(open) / sizeof(open[0]); ++i)
		a.sections.push_back(open[i]);
	mcp::installAllowlists(a);

	const char *const paths[][2] = {
		{ "recording", "network_nfs_recordingdir" }, { "recording", "timeshiftdir" },
		{ "recording", "network_nfs_moviedir" }, { "recording", "recordingmenu.filename_template" },
		{ "misc", "plugin_hdd_dir" }, { "misc", "epg_dir" },
		{ "misc", "backup_dir" }, { "osd", "screenshot_dir" }, { "osd", "logo_hdd_dir" },
		{ "osd", "font_file" }, { "player", "network_nfs_picturedir" },
		{ "channel", "livestreamScriptPath" }, { "player", "network_nfs_audioplayerdir" },
	};
	for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
	{
		INFO(paths[i][1]);
		const coreapi::Result<mcp::JsonText> r = mcp::boxTools().call(callerAt(AuthLevel::System), "write_settings",
			std::string("{\"section\":\"") + paths[i][0] + "\",\"settings\":{\"" + paths[i][1] + "\":\"/\"}}");
		REQUIRE_FALSE(r.ok());
		REQUIRE(r.error().code == coreapi::ErrorCode::SettingsSectionDenied);
	}

	// One path in the body refuses the whole call, and a plain key beside it is not written.
	REQUIRE(mcp::boxTools().call(callerAt(AuthLevel::System), "write_settings",
		"{\"section\":\"recording\",\"settings\":{\"timeshift_hours\":\"5\",\"timeshiftdir\":\"/\"}}")
		.error().code == coreapi::ErrorCode::SettingsSectionDenied);
	REQUIRE(store.persisted == 0);

	REQUIRE(mcp::boxTools().call(callerAt(AuthLevel::System), "write_settings",
		"{\"section\":\"recording\",\"settings\":{\"timeshift_hours\":\"5\"}}").ok());
	REQUIRE(store.ints["timeshift_hours"] == 5);
}

TEST_CASE("the schema marks every setting that names a place on the box's disk", "[allowlist][gate]")
{
	coreapi::Result<std::vector<coreapi::Descriptor> > schema = coreapi::settings::schema();
	REQUIRE(schema.ok());
	std::set<std::string> marked, declared;
	for (size_t i = 0; i < schema.value().size(); ++i)
	{
		declared.insert(schema.value()[i].key);
		if (coreapi::settings::holdsPath(schema.value()[i]))
			marked.insert(schema.value()[i].key);
	}
	const char *const expected[] = {
		"backup_dir", "epg_dir", "font_file", "font_file_monospace", "glcd_logodir", "last_webradio_dir",
		"last_webtv_dir", "lcd4l_logodir", "livestreamScriptPath", "logo_hdd_dir", "network_nfs_audioplayerdir",
		"network_nfs_moviedir", "network_nfs_picturedir", "network_nfs_recordingdir",
		"network_nfs_streamripperdir", "plugin_hdd_dir", "recordingmenu.filename_template", "screensaver_dir",
		"screenshot_dir",
		"softupdate_url_file", "timeshiftdir", "update_dir", "update_dir_opkg",
	};
	const std::set<std::string> want(expected, expected + sizeof(expected) / sizeof(expected[0]));
	for (std::set<std::string>::const_iterator it = marked.begin(); it != marked.end(); ++it)
	{
		INFO(*it);
		CHECK(want.count(*it) == 1);
	}
	// The display rows are declared only on a box with that display.
	for (std::set<std::string>::const_iterator it = want.begin(); it != want.end(); ++it)
	{
		INFO(*it);
		CHECK(marked.count(*it) == declared.count(*it));
	}
	CHECK(marked.size() >= want.size() - 2);

	FakeSettingsSource store;
	InstalledSettingsSource in_store(&store);
	const coreapi::Result<mcp::JsonText> r = mcp::boxTools().call(callerAt(AuthLevel::Read), "settings_schema",
		"{\"section\":\"recording\"}");
	REQUIRE(r.ok());
	mcp::JsonValue v;
	REQUIRE(mcp::parseJson(r.value(), 64, v));
	unsigned seen = 0;
	for (unsigned i = 0; i < v["items"].size(); ++i)
	{
		const std::string id = v["items"][i]["id"].asString();
		INFO(id);
		REQUIRE(v["items"][i]["path"].isBool());
		REQUIRE(v["items"][i]["path"].asBool() == (want.count(id) == 1));
		seen += want.count(id);
	}
	REQUIRE(seen == 4);
}

TEST_CASE("write_settings answers a mixed outcome as a tool error naming what did not land",
	  "[allowlist][gate]")
{
	Lists back;
	FakeSettingsSource store;
	InstalledSettingsSource in_store(&store);
	mcp::Allowlists a;
	a.sections.push_back("audio");
	mcp::installAllowlists(a);

	const coreapi::Result<mcp::JsonText> r = mcp::boxTools().call(callerAt(AuthLevel::System), "write_settings",
		"{\"section\":\"audio\",\"settings\":{\"audio_volume_percent_ac3\":\"50\",\"nope\":\"1\"}}");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().code == coreapi::ErrorCode::SettingNotWritten);
	REQUIRE(r.error().message.find("not every value was written") != std::string::npos);
	REQUIRE(r.error().message.find("audio_volume_percent_ac3") != std::string::npos);
	REQUIRE(r.error().message.find("no-such-setting") != std::string::npos);

	// The key that did land is not rolled back by the one that did not.
	REQUIRE(store.ints["audio_volume_percent_ac3"] == 50);
}

TEST_CASE("read_settings never answers a secret value in any section", "[allowlist][secrets]")
{
	FakeSettingsSource store;
	InstalledSettingsSource in_store(&store);
	coreapi::Result<std::vector<coreapi::Descriptor> > schema = coreapi::settings::schema();
	REQUIRE(schema.ok());
	for (size_t i = 0; i < schema.value().size(); ++i)
	{
		const coreapi::Descriptor &d = schema.value()[i];
		if (d.secret)
			store.strings[d.key] = "the-secret-value";
	}
	coreapi::Result<std::vector<std::string> > sections = coreapi::settings::sections();
	REQUIRE(sections.ok());
	for (size_t s = 0; s < sections.value().size(); ++s)
	{
		INFO(sections.value()[s]);
		coreapi::Result<mcp::JsonText> r = mcp::boxTools().call(callerAt(AuthLevel::Read), "read_settings",
			"{\"section\":\"" + sections.value()[s] + "\"}");
		REQUIRE(r.ok());
		REQUIRE(r.value().find("the-secret-value") == std::string::npos);
	}
}
