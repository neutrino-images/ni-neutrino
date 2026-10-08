/*
 * test_apply_audio.cpp - the audio apply groups
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

#include "support/catch.hpp"
#include "support/fakes.h"
#include "support/phaseenv.h"

#include <neutrinoMessages.h>

#include "coreapi/base/apply.h"
#include "coreapi/base/deps.h"
#include "coreapi/box/apply_audio.h"
#include "coreapi/settings/settings.h"

#include <system/settings.h>

#include <hardware/audio.h>

#include <cstdio>
#include <string>

extern SNeutrinoSettings g_settings;

using namespace coreapi;

namespace
{

// The members the cases write, put back so the rest of the suite finds them as they were.
struct KeptAudioSettings
{
	int srs_enable, srs_nmgr, srs_algo, srs_ref, analog_out, spdif_dd, avsync, mode, ac3, pcm;
	KeptAudioSettings()
		: srs_enable(g_settings.srs_enable), srs_nmgr(g_settings.srs_nmgr_enable), srs_algo(g_settings.srs_algo),
		  srs_ref(g_settings.srs_ref_volume), analog_out(g_settings.analog_out), spdif_dd(g_settings.spdif_dd),
		  avsync(g_settings.avsync), mode(g_settings.audio_AnalogMode),
		  ac3(g_settings.audio_volume_percent_ac3), pcm(g_settings.audio_volume_percent_pcm) {}
	~KeptAudioSettings()
	{
		g_settings.srs_enable = srs_enable;
		g_settings.srs_nmgr_enable = srs_nmgr;
		g_settings.srs_algo = srs_algo;
		g_settings.srs_ref_volume = srs_ref;
		g_settings.analog_out = analog_out;
		g_settings.spdif_dd = spdif_dd;
		g_settings.avsync = avsync;
		g_settings.audio_AnalogMode = mode;
		g_settings.audio_volume_percent_ac3 = ac3;
		g_settings.audio_volume_percent_pcm = pcm;
	}
};

/* What an audio case runs against: the seams a startup phase has, its fake audio
   output among them, nothing sent yet, and the members it writes put back. */
struct AudioBox
{
	KeptAudioSettings kept;
	PhaseEnvironment  env;
	FakeAudioOutput  &out;

	explicit AudioBox(ApplyPhase phase)
		: env(phase), out(env.fake<FakeAudioOutput>("audio"))
	{
		resetApplyRegistry();
		resetSentAudio();
		registerApplyGroups();
	}
	~AudioBox()
	{
		resetSentAudio();
		resetApplyRegistry();
	}
};

bool audioSave() { return true; }

std::string audioNumber(int v)
{
	char text[16];
	snprintf(text, sizeof(text), "%d", v);
	return text;
}

} // namespace

TEST_CASE("the audio groups are registered by the one hook and keep their keys apart", "[apply][audio]")
{
	AudioBox box(ApplyPhase::Decoders);

	const char *const srs[] = { "srs_enable", "srs_algo", "srs_nmgr_enable", "srs_ref_volume" };
	for (size_t i = 0; i < sizeof(srs) / sizeof(srs[0]); ++i)
	{
		INFO(srs[i]);
		REQUIRE(groupOf(srs[i]) == &kSrsApplyGroup);
	}
	REQUIRE(groupOf("audio_volume_percent_ac3") == &kVolumePercentApplyGroup);
	REQUIRE(groupOf("audio_volume_percent_pcm") == &kVolumePercentApplyGroup);

	const char *const audio[] = { "analog_out", "ac3_pass", "dts_pass", "hdmi_dd", "spdif_dd", "avsync" };
	for (size_t i = 0; i < sizeof(audio) / sizeof(audio[0]); ++i)
	{
		INFO(audio[i]);
		REQUIRE(groupOf(audio[i]) == &kAudioApplyGroup);
	}
	REQUIRE(groupOf("audio_AnalogMode") == &kAudioModeApplyGroup);

	REQUIRE(kSrsApplyGroup.phase == ApplyPhase::Decoders);
	REQUIRE(kVolumePercentApplyGroup.phase == ApplyPhase::Decoders);
	REQUIRE(kAudioApplyGroup.phase == ApplyPhase::Decoders);
	// The channel daemon's client is made after the decoders phase.
	REQUIRE(kAudioModeApplyGroup.phase == ApplyPhase::Zapit);

	// Read where it is used, or only at the next start.
	REQUIRE(groupOf("current_volume_step") == NULL);
	REQUIRE(groupOf("audio_DolbyDigital") == NULL);
}

TEST_CASE("startup runs each audio group once and sends the settings", "[apply][audio]")
{
	AudioBox box(ApplyPhase::Decoders);
	g_settings.srs_enable = 1;
	g_settings.srs_nmgr_enable = 0;
	g_settings.srs_algo = 2;
	g_settings.srs_ref_volume = 60;
	g_settings.analog_out = 0;
	g_settings.spdif_dd = 1;
	g_settings.avsync = AVSYNC_AUDIO_IS_MASTER;
	g_settings.audio_volume_percent_ac3 = 90;
	g_settings.audio_volume_percent_pcm = 80;

	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);

	REQUIRE(box.out.count("srs") == 1);
	REQUIRE(box.out.count("percent") == 1);
	REQUIRE(box.out.percents[0] == std::make_pair(90, 80));
	REQUIRE(box.out.count("hdmi") == 1);
	REQUIRE(box.out.count("spdif") == 1);
	REQUIRE(box.out.count("analog") == 1);
	REQUIRE(box.out.count("sync") == 1);
	// The channel daemon is not there yet.
	REQUIRE(box.out.count("mode") == 0);
	// The surround enhancer, in the order the program always gave it.
	REQUIRE(box.out.values[0] == 1);
	REQUIRE(box.out.values[1] == 0);
	REQUIRE(box.out.values[2] == 2);
	REQUIRE(box.out.values[3] == 60);
}

// The decoders start in AVSYNC_ENABLED, and the startup before the group wrote the mode only when it differed.
TEST_CASE("startup sends the sync mode only where it is not the one the decoders start in", "[apply][audio]")
{
	AudioBox box(ApplyPhase::Decoders);
	g_settings.avsync = AVSYNC_ENABLED;
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);
	REQUIRE(box.out.count("sync") == 0);

	resetSentAudio();
	box.out.forget();
	g_settings.avsync = AVSYNC_DISABLED;
	REQUIRE(applyKey("avsync") == Status::Ok);
	REQUIRE(box.out.count("sync") == 1);
	REQUIRE(box.out.values.back() == AVSYNC_DISABLED);
}

TEST_CASE("startup sends the stereo mode in its own phase", "[apply][audio]")
{
	AudioBox box(ApplyPhase::Zapit);
	g_settings.audio_AnalogMode = 2;
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);
	REQUIRE(box.out.calls.size() == 1);
	REQUIRE(box.out.calls[0] == "mode");
	REQUIRE(box.out.values[0] == 2);
}

/* A change of one key sends that state and nothing else: the clock is not
   written again with the value it holds, and the other group does not run. */
TEST_CASE("a key of the audio group sends only what differs from what was sent", "[apply][audio]")
{
	AudioBox box(ApplyPhase::Decoders);
	g_settings.avsync = AVSYNC_AUDIO_IS_MASTER;
	g_settings.analog_out = 1;
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);
	box.out.forget();

	g_settings.analog_out = 0;
	REQUIRE(applyKey("analog_out") == Status::Ok);
	REQUIRE(box.out.calls.size() == 1);
	REQUIRE(box.out.calls[0] == "analog");
	REQUIRE(box.out.values[0] == 0);

	// Nothing changed, nothing sent.
	box.out.forget();
	REQUIRE(applyKey("analog_out") == Status::Ok);
	REQUIRE(box.out.calls.empty());
	// A key of another group runs that group alone.
	g_settings.srs_algo = (g_settings.srs_algo + 1) % 2;
	REQUIRE(applyKey("srs_algo") == Status::Ok);
	REQUIRE(box.out.calls.size() == 1);
	REQUIRE(box.out.calls[0] == "srs");
}

// A send the driver refused is not taken as made, so the next run tries again.
TEST_CASE("a refused sync send is sent again by the next run", "[apply][audio]")
{
	AudioBox box(ApplyPhase::Decoders);
	g_settings.avsync = AVSYNC_ENABLED;
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);
	box.out.forget();

	box.out.sync_answer = Status::Internal;
	g_settings.avsync = AVSYNC_DISABLED;
	REQUIRE(applyKey("avsync") == Status::Internal);
	REQUIRE(box.out.count("sync") == 1);

	box.out.sync_answer = Status::Ok;
	box.out.forget();
	REQUIRE(applyKey("avsync") == Status::Ok);
	REQUIRE(box.out.count("sync") == 1);
	box.out.forget();
	REQUIRE(applyKey("avsync") == Status::Ok);
	REQUIRE(box.out.count("sync") == 0);
}

TEST_CASE("the percent a channel starts at is sent whatever else was sent", "[apply][audio]")
{
	AudioBox box(ApplyPhase::Decoders);
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);
	box.out.forget();

	g_settings.audio_volume_percent_pcm = 55;
	REQUIRE(applyKey("audio_volume_percent_pcm") == Status::Ok);
	REQUIRE(box.out.calls.size() == 1);
	REQUIRE(box.out.percents[0].second == 55);
}

TEST_CASE("a group run before anything drives the audio fails rather than doing nothing quietly", "[apply][audio]")
{
	PhaseEnvironment env(ApplyPhase::Decoders);
	resetApplyRegistry();
	resetSentAudio();
	setAudioOutput(NULL);
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::NotSupported);
	REQUIRE(applyKey("srs_algo") == Status::NotSupported);
	REQUIRE(applyKey("analog_out") == Status::NotSupported);
	resetApplyRegistry();
	resetSentAudio();
}

/* A web write, a menu item and startup reach the same group through applyKey and
   the drain; this is the drain's side: two keys of one group written together
   run it once, and nothing is asked of anybody. */
TEST_CASE("a web batch writing two audio keys runs each group once and asks nothing", "[apply][audio]")
{
	AudioBox box(ApplyPhase::Decoders);
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, audioSave);

	g_settings.srs_enable = 1;
	g_settings.avsync = AVSYNC_ENABLED;
	g_settings.analog_out = 1;
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);
	box.out.forget();

	REQUIRE(settings::set("srs_algo", audioNumber(g_settings.srs_algo == 0 ? 1 : 0)).ok());
	REQUIRE(settings::set("srs_ref_volume", audioNumber(33)).ok());
	REQUIRE(settings::set("avsync", audioNumber(AVSYNC_AUDIO_IS_MASTER)).ok());
	REQUIRE(settings::set("analog_out", audioNumber(0)).ok());
	REQUIRE(box.out.calls.empty());
	const size_t posted = sink.posted.size();
	applyPendingSettings();

	REQUIRE(box.out.count("srs") == 1);
	REQUIRE(box.out.count("sync") == 1);
	REQUIRE(box.out.count("analog") == 1);
	REQUIRE(box.out.calls.size() == 3);
	REQUIRE(events.sent.empty());
	// What the drain posts besides is its word that the settings landed.
	for (size_t i = posted; i < sink.posted.size(); ++i)
		CHECK(sink.posted[i].first == NeutrinoMessages::EVT_SETTINGS_WRITTEN);

	installRealSettingsSource(NULL, NULL);
}

TEST_CASE("a web write of the percents and of the stereo mode reaches their groups", "[apply][audio]")
{
	AudioBox box(ApplyPhase::Zapit);
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, audioSave);

	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);
	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Ok);
	box.out.forget();

	REQUIRE(settings::set("audio_volume_percent_ac3", audioNumber(70)).ok());
	REQUIRE(settings::set("audio_AnalogMode", audioNumber(1)).ok());
	applyPendingSettings();

	REQUIRE(box.out.count("percent") == 1);
	REQUIRE(box.out.percents[0].first == 70);
	REQUIRE(box.out.count("mode") == 1);
	REQUIRE(box.out.values.back() == 1);

	installRealSettingsSource(NULL, NULL);
}
