/*
 * test_apply.cpp - tests for the apply registry
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
#include "coreapi/box/apply_video.h"
#include "coreapi/box/mode43.h"
#include "coreapi/osd.h"
#include "coreapi/settings/settings.h"
#include "coreapi/settings/videomodes.h"
#include "gui/widget/settingactive.h"

#include <system/settings.h>

#include <hardware/video.h>

#include <pthread.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

extern SNeutrinoSettings g_settings;

using namespace coreapi;

namespace
{

int g_runs_a = 0;
int g_runs_b = 0;
std::vector<int> g_order;

Status runA() { ++g_runs_a; g_order.push_back(1); return Status::Ok; }
Status runB() { ++g_runs_b; g_order.push_back(2); return Status::Ok; }
Status runFails() { return Status::Internal; }
Status runFailsLate() { ++g_runs_b; return Status::Conflict; }

const char *const kKeysA[] = { "a", "b" };
const char *const kKeysB[] = { "c" };
const char *const kKeysClash[] = { "x", "c" };
const char *const kKeysTwice[] = { "t", "t" };

// Every case starts from nothing: the registry is process wide.
struct Fresh
{
	Fresh()
	{
		resetApplyRegistry();
		g_runs_a = 0;
		g_runs_b = 0;
		g_order.clear();
	}
	~Fresh() { resetApplyRegistry(); }
};

} // namespace

namespace
{
// What a call wrote to stderr, read back from a file stderr was pointed at for its length.
template <class Call>
std::string stderrOf(Call call)
{
	char path[] = "/tmp/coreapi-apply-XXXXXX";
	const int fd = mkstemp(path);
	if (fd < 0)
		return "(no file)";
	unlink(path);
	std::fflush(stderr);
	const int saved = dup(STDERR_FILENO);
	dup2(fd, STDERR_FILENO);
	call();
	std::fflush(stderr);
	dup2(saved, STDERR_FILENO);
	close(saved);
	std::string text;
	char buf[512];
	lseek(fd, 0, SEEK_SET);
	ssize_t n;
	while ((n = read(fd, buf, sizeof(buf))) > 0)
		text.append(buf, (size_t) n);
	close(fd);
	return text;
}
} // namespace

/* The groups are registered from a fixed list whose answers nobody reads, so a refused one
   says so itself: otherwise every key of it lands and nothing puts it in force, silently. */
TEST_CASE("a refused group registration is reported by the group's name", "[apply]")
{
	Fresh fresh;
	const ApplyGroup first = { "first", ApplyPhase::Zapit, COREAPI_KEYS(kKeysB), runA };
	const ApplyGroup second = { "second", ApplyPhase::Zapit, COREAPI_KEYS(kKeysClash), runB };
	REQUIRE(registerApplyGroup(&first) == Status::Ok);
	const std::string said = stderrOf([&]() { registerApplyGroup(&second); });
	CHECK(said.find("second") != std::string::npos);
	CHECK(stderrOf([&]() { registerApplyGroup(NULL); }).find("refused") != std::string::npos);
}

TEST_CASE("a key that is in two groups is refused and the second group is not kept", "[apply]")
{
	Fresh fresh;
	const ApplyGroup first = { "first", ApplyPhase::Zapit, COREAPI_KEYS(kKeysB), runA };
	const ApplyGroup second = { "second", ApplyPhase::Zapit, COREAPI_KEYS(kKeysClash), runB };
	REQUIRE(registerApplyGroup(&first) == Status::Ok);
	REQUIRE(registerApplyGroup(&second) == Status::Conflict);
	// All or nothing: the key of the refused group that was free stays free.
	REQUIRE(groupOf("x") == NULL);
	REQUIRE(groupOf("c") == &first);
}

TEST_CASE("a group whose phase was reached is refused and not kept", "[apply]")
{
	Fresh fresh;
	const ApplyGroup late = { "late", ApplyPhase::Zapit, COREAPI_KEYS(kKeysB), runA };
	runPhase(ApplyPhase::Zapit);
	REQUIRE(registerApplyGroup(&late) == Status::InvalidArgument);
	REQUIRE(groupOf("c") == NULL);

	// Another phase is still open to it.
	const ApplyGroup other = { "other", ApplyPhase::Network, COREAPI_KEYS(kKeysB), runB };
	REQUIRE(registerApplyGroup(&other) == Status::Ok);
}

namespace
{
Status g_foreign = Status::Ok;

void *batchElsewhere(void *)
{
	std::vector<std::string> keys;
	keys.push_back("a");
	g_foreign = applyBatch(keys);
	return 0;
}
} // namespace

TEST_CASE("a batch asked on a thread that is not the bound loop is refused", "[apply]")
{
	Fresh fresh;
	const ApplyGroup g = { "a", ApplyPhase::Zapit, COREAPI_KEYS(kKeysA), runA };
	REQUIRE(registerApplyGroup(&g) == Status::Ok);
	runPhase(ApplyPhase::Zapit);
	g_runs_a = 0;

	// Nothing is refused while no thread is named.
	std::vector<std::string> keys;
	keys.push_back("a");
	REQUIRE(applyBatch(keys) == Status::Ok);
	REQUIRE(g_runs_a == 1);

	bindApplyLoop();
	REQUIRE(onApplyLoop());
	REQUIRE(applyBatch(keys) == Status::Ok);
	REQUIRE(g_runs_a == 2);

	pthread_t other;
	REQUIRE(pthread_create(&other, 0, batchElsewhere, 0) == 0);
	REQUIRE(pthread_join(other, 0) == 0);
	REQUIRE(g_foreign == Status::Denied);
	REQUIRE(g_runs_a == 2);
}

TEST_CASE("a group that lists a key twice or lacks a name or a run is refused", "[apply]")
{
	Fresh fresh;
	const ApplyGroup twice = { "twice", ApplyPhase::Zapit, COREAPI_KEYS(kKeysTwice), runA };
	REQUIRE(registerApplyGroup(&twice) == Status::Conflict);
	const ApplyGroup unnamed = { NULL, ApplyPhase::Zapit, COREAPI_KEYS(kKeysB), runA };
	REQUIRE(registerApplyGroup(&unnamed) == Status::InvalidArgument);
	const ApplyGroup norun = { "norun", ApplyPhase::Zapit, COREAPI_KEYS(kKeysB), NULL };
	REQUIRE(registerApplyGroup(&norun) == Status::InvalidArgument);
	REQUIRE(registerApplyGroup(NULL) == Status::InvalidArgument);
	REQUIRE(groupOf("t") == NULL);
}

TEST_CASE("applyKey before its phase is reached does not run and says so", "[apply]")
{
	Fresh fresh;
	const ApplyGroup g = { "a", ApplyPhase::Zapit, COREAPI_KEYS(kKeysA), runA };
	REQUIRE(registerApplyGroup(&g) == Status::Ok);
	REQUIRE(applyKey("a") == Status::Busy);
	REQUIRE(g_runs_a == 0);

	// Another phase being reached does not stand in for this one.
	runPhase(ApplyPhase::Decoders);
	REQUIRE(g_runs_a == 0);
	REQUIRE(applyKey("a") == Status::Busy);
	REQUIRE(g_runs_a == 0);
}

TEST_CASE("after its phase a key runs its group once per call and the phase runs it too", "[apply]")
{
	Fresh fresh;
	const ApplyGroup g = { "a", ApplyPhase::Zapit, COREAPI_KEYS(kKeysA), runA };
	REQUIRE(registerApplyGroup(&g) == Status::Ok);
	runPhase(ApplyPhase::Zapit);
	REQUIRE(g_runs_a == 1);
	REQUIRE(applyKey("a") == Status::Ok);
	REQUIRE(g_runs_a == 2);
	REQUIRE(applyKey("b") == Status::Ok);
	REQUIRE(g_runs_a == 3);
}

TEST_CASE("a run that fails is what applyKey answers", "[apply]")
{
	Fresh fresh;
	const ApplyGroup g = { "f", ApplyPhase::Network, COREAPI_KEYS(kKeysB), runFails };
	REQUIRE(registerApplyGroup(&g) == Status::Ok);
	runPhase(ApplyPhase::Network);
	REQUIRE(applyKey("c") == Status::Internal);
}

TEST_CASE("a batch of keys of one group runs it once", "[apply]")
{
	Fresh fresh;
	const ApplyGroup g = { "a", ApplyPhase::Zapit, COREAPI_KEYS(kKeysA), runA };
	const ApplyGroup h = { "b", ApplyPhase::Zapit, COREAPI_KEYS(kKeysB), runB };
	REQUIRE(registerApplyGroup(&g) == Status::Ok);
	REQUIRE(registerApplyGroup(&h) == Status::Ok);
	runPhase(ApplyPhase::Zapit);
	g_runs_a = 0;
	g_runs_b = 0;

	std::vector<std::string> keys;
	keys.push_back("a");
	keys.push_back("b");
	applyBatch(keys);
	REQUIRE(g_runs_a == 1);
	REQUIRE(g_runs_b == 0);

	keys.push_back("c");
	keys.push_back("a");
	applyBatch(keys);
	REQUIRE(g_runs_a == 2);
	REQUIRE(g_runs_b == 1);
}

TEST_CASE("a batch skips a group whose phase is not reached", "[apply]")
{
	Fresh fresh;
	const ApplyGroup g = { "a", ApplyPhase::Network, COREAPI_KEYS(kKeysA), runA };
	REQUIRE(registerApplyGroup(&g) == Status::Ok);
	std::vector<std::string> keys;
	keys.push_back("a");
	applyBatch(keys);
	REQUIRE(g_runs_a == 0);
}

TEST_CASE("a key with no group is fine and runs nothing", "[apply]")
{
	Fresh fresh;
	const ApplyGroup g = { "a", ApplyPhase::Zapit, COREAPI_KEYS(kKeysA), runA };
	REQUIRE(registerApplyGroup(&g) == Status::Ok);
	runPhase(ApplyPhase::Zapit);
	g_runs_a = 0;
	REQUIRE(applyKey("nobody") == Status::Ok);
	REQUIRE(groupOf("nobody") == NULL);
	REQUIRE(g_runs_a == 0);
}

TEST_CASE("a phase runs only its own groups, in the order they were registered", "[apply]")
{
	Fresh fresh;
	const ApplyGroup late = { "late", ApplyPhase::Network, COREAPI_KEYS(kKeysB), runB };
	const ApplyGroup early = { "early", ApplyPhase::Zapit, COREAPI_KEYS(kKeysA), runA };
	const ApplyGroup again = { "again", ApplyPhase::Zapit, NULL, 0, runB };
	REQUIRE(registerApplyGroup(&late) == Status::Ok);
	REQUIRE(registerApplyGroup(&early) == Status::Ok);
	REQUIRE(registerApplyGroup(&again) == Status::Ok);

	runPhase(ApplyPhase::Zapit);
	REQUIRE(g_order.size() == 2);
	REQUIRE(g_order[0] == 1);
	REQUIRE(g_order[1] == 2);
	REQUIRE(g_runs_b == 1);

	runPhase(ApplyPhase::Network);
	REQUIRE(g_runs_b == 2);
}

TEST_CASE("a phase and a batch run every group, and answer the first failure", "[apply]")
{
	Fresh fresh;
	const ApplyGroup bad = { "bad", ApplyPhase::Zapit, COREAPI_KEYS(kKeysA), runFails };
	const ApplyGroup worse = { "worse", ApplyPhase::Zapit, COREAPI_KEYS(kKeysB), runFailsLate };
	const ApplyGroup fine = { "fine", ApplyPhase::Zapit, NULL, 0, runA };
	REQUIRE(registerApplyGroup(&bad) == Status::Ok);
	REQUIRE(registerApplyGroup(&worse) == Status::Ok);
	REQUIRE(registerApplyGroup(&fine) == Status::Ok);

	REQUIRE(runPhase(ApplyPhase::Zapit) == Status::Internal);
	// The failure of the first did not stop the ones after it.
	REQUIRE(g_runs_b == 1);
	REQUIRE(g_runs_a == 1);

	std::vector<std::string> keys;
	keys.push_back("a");
	keys.push_back("c");
	REQUIRE(applyBatch(keys) == Status::Internal);
	REQUIRE(g_runs_b == 2);
}

TEST_CASE("a group whose phase is not reached answers Busy, which means deferred", "[apply]")
{
	Fresh fresh;
	const ApplyGroup g = { "a", ApplyPhase::Network, COREAPI_KEYS(kKeysA), runA };
	REQUIRE(registerApplyGroup(&g) == Status::Ok);
	REQUIRE(applyKey("a") == Status::Busy);
	std::vector<std::string> keys(1, "a");
	// A deferred group is not a failure of the batch.
	REQUIRE(applyBatch(keys) == Status::Ok);
	REQUIRE(runPhase(ApplyPhase::Network) == Status::Ok);
	REQUIRE(g_runs_a == 1);
}

namespace
{

// The members the cases write, put back so the rest of the suite finds them as they were.
struct KeptVideoSettings
{
	int mode, format, mode43, dbdr;
	KeptVideoSettings()
		: mode(g_settings.video_Mode), format(g_settings.video_Format), mode43(g_settings.video_43mode),
		  dbdr(g_settings.video_dbdr) {}
	~KeptVideoSettings()
	{
		g_settings.video_Mode = mode;
		g_settings.video_Format = format;
		g_settings.video_43mode = mode43;
		g_settings.video_dbdr = dbdr;
	}
};

/* What a video case runs against: the seams the decoders phase has, its fake
   decoders among them, nothing sent yet and no hold, and the members it writes
   put back afterwards. */
struct VideoBox
{
	KeptVideoSettings kept;
	PhaseEnvironment  env;
	FakeVideoOutput  &out;
	FakeSystemSource &system;

	VideoBox()
		: env(ApplyPhase::Decoders), out(env.fake<FakeVideoOutput>("video")),
		  system(env.fake<FakeSystemSource>("system")) { resetSentVideo(); }
	~VideoBox() { resetSentVideo(); }

	void forget() { out.forget(); }
};

bool videoSave() { return true; }

std::string number(int v)
{
	char text[16];
	snprintf(text, sizeof(text), "%d", v);
	return text;
}

bool answerNo() { return false; }

void applyRowKey(const std::string &key) { applyKey(key); }

} // namespace

TEST_CASE("the video and picture groups are registered by the one hook and run after the decoders", "[apply][video]")
{
	Fresh fresh;
	registerApplyGroups();

	const char *const video[] = { "video_Mode", "video_Format", "video_43mode", "video_dbdr", "analog_mode1",
				      "analog_mode2", "zappingmode", "hdmi_colorimetry", "brightness", "contrast",
				      "saturation", "enable_sd_osd", "enabled_video_mode_0", "enabled_auto_mode_19" };
	for (size_t i = 0; i < sizeof(video) / sizeof(video[0]); ++i)
	{
		INFO(video[i]);
		REQUIRE(groupOf(video[i]) == &kVideoApplyGroup);
	}
	REQUIRE(kVideoApplyGroup.phase == ApplyPhase::Decoders);

	const char *const psi[] = { "video_psi_contrast", "video_psi_saturation", "video_psi_brightness", "video_psi_tint" };
	for (size_t i = 0; i < sizeof(psi) / sizeof(psi[0]); ++i)
	{
		INFO(psi[i]);
		REQUIRE(groupOf(psi[i]) == &kPsiApplyGroup);
	}
	REQUIRE(kPsiApplyGroup.phase == ApplyPhase::Decoders);

	// How far a slider moves is read where it is used.
	REQUIRE(groupOf("video_psi_step") == NULL);
}

TEST_CASE("startup runs the video group once and sends everything", "[apply][video]")
{
	Fresh fresh;
	VideoBox box;
	FakeVideoOutput &out = box.out;
	registerApplyGroups();

	g_settings.video_Mode = VIDEO_STD_720P50;
	g_settings.video_Format = DISPLAY_AR_16_9;
	g_settings.video_43mode = DISPLAY_AR_MODE_LETTERBOX;
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);

	REQUIRE(out.systems.size() == 1);
	REQUIRE(out.systems[0] == VIDEO_STD_720P50);
	REQUIRE(out.aspects.size() == 1);
	REQUIRE(out.aspects[0] == std::make_pair((int) DISPLAY_AR_16_9, (int) DISPLAY_AR_MODE_LETTERBOX));
	REQUIRE(out.count("dbdr") == 1);
	REQUIRE(out.count("automodes") == 1);
	REQUIRE(out.count("analog") >= 1);
	// The standard first, since the rest is drawn in it.
	REQUIRE(out.calls[0] == "system");
}

/* A change of one key sends that state and nothing else: the screen is not
   redrawn and the analog, aspect and zapping state is not written again with
   the value it already has. */
TEST_CASE("a key of the video group sends only what differs from what was sent", "[apply][video]")
{
	Fresh fresh;
	VideoBox box;
	registerApplyGroups();
	g_settings.video_dbdr = 0;
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);
	box.forget();

	g_settings.video_dbdr = 2;
	REQUIRE(applyKey("video_dbdr") == Status::Ok);
	REQUIRE(box.out.calls.size() == 1);
	REQUIRE(box.out.calls[0] == "dbdr");

	// Nothing changed, nothing sent.
	box.forget();
	REQUIRE(applyKey("video_dbdr") == Status::Ok);
	REQUIRE(box.out.calls.empty());
}

// A send the driver refused is not taken as made, so the next run tries again.
TEST_CASE("a refused send is sent again by the next run", "[apply][video]")
{
	Fresh fresh;
	VideoBox box;
	registerApplyGroups();
	g_settings.video_dbdr = 0;
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);
	box.forget();

	box.out.dbdr_answer = Status::Internal;
	g_settings.video_dbdr = 1;
	REQUIRE(applyKey("video_dbdr") == Status::Internal);
	box.out.dbdr_answer = Status::Ok;
	box.forget();
	REQUIRE(applyKey("video_dbdr") == Status::Ok);
	REQUIRE(box.out.count("dbdr") == 1);
}

/* The channel daemon's own command sets the standard, and the setting with it,
   past the group. Marked, the next run sends the setting again although it
   equals what the group last sent, so the box returns to what the setting says. */
TEST_CASE("a state another writer changed is sent again by the next run", "[apply][video]")
{
	Fresh fresh;
	VideoBox box;
	registerApplyGroups();
	g_settings.video_Mode = VIDEO_STD_720P50;
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);
	box.forget();

	forgetSentVideo(VideoSent::System);
	REQUIRE(applyKey("video_Mode") == Status::Ok);
	REQUIRE(box.out.systems.size() == 1);
	REQUIRE(box.out.systems[0] == VIDEO_STD_720P50);
	REQUIRE(box.out.calls.size() == 1);

	// Marked once, sent once.
	box.forget();
	REQUIRE(applyKey("video_Mode") == Status::Ok);
	REQUIRE(box.out.calls.empty());
}

/* Standby keeps the zapping mode for itself on one box. Held, a state is left
   alone by every run, whatever is written; let go, the next run sends the
   setting. Shown with the aspect, which every build has. */
TEST_CASE("a held state is not sent until it is let go", "[apply][video]")
{
	Fresh fresh;
	VideoBox box;
	registerApplyGroups();
	g_settings.video_Format = DISPLAY_AR_16_9;
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);
	box.forget();

	holdVideoState(VideoSent::Aspect, true);
	g_settings.video_Format = DISPLAY_AR_4_3;
	REQUIRE(applyKey("video_Format") == Status::Ok);
	REQUIRE(box.out.aspects.empty());

	/* Back to what was sent before the hold, while the holder put something else
	   on the box: let go, the setting is sent again all the same. */
	g_settings.video_Format = DISPLAY_AR_16_9;
	holdVideoState(VideoSent::Aspect, false);
	REQUIRE(applyKey("video_dbdr") == Status::Ok);
	REQUIRE(box.out.aspects.size() == 1);
	REQUIRE(box.out.aspects[0].first == DISPLAY_AR_16_9);
}

/* Standby holds the state and sets its own value with no run of the group in between, which
   is the usual case. Let go, the setting goes back on the box once although it never changed. */
TEST_CASE("a state held with no run between is sent once when let go", "[apply][video]")
{
	Fresh fresh;
	VideoBox box;
	registerApplyGroups();
	g_settings.video_Format = DISPLAY_AR_16_9;
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);
	box.forget();

	holdVideoState(VideoSent::Aspect, true);
	holdVideoState(VideoSent::Aspect, false);
	REQUIRE(applyKey("video_Format") == Status::Ok);
	REQUIRE(box.out.aspects.size() == 1);
	REQUIRE(box.out.aspects[0].first == DISPLAY_AR_16_9);

	box.forget();
	REQUIRE(applyKey("video_Format") == Status::Ok);
	REQUIRE(box.out.aspects.empty());
}

/* The second analog output is set apart only where the board keeps it apart:
   one list for both on revision 6. */
TEST_CASE("the second analog output is set only where the board keeps it apart", "[apply][video]")
{
	Fresh fresh;
	VideoBox box;
	registerApplyGroups();

	box.system.caps.board_revision = 0x06;
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);
	REQUIRE(box.out.count("analog") == 1);
#ifndef BOXMODEL_CST_HD2
	box.forget();
	box.system.caps.board_revision = 0x08;
	REQUIRE(applyKey("analog_mode2") == Status::Ok);
	// The first output was sent already; the second is its own now.
	REQUIRE(box.out.count("analog") == 1);
#endif
}

TEST_CASE("a group run before anything drives the decoders fails rather than doing nothing quietly", "[apply][video]")
{
	Fresh fresh;
	VideoBox box;
	setVideoOutput(0);
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::NotSupported);
}

/* The web path: nobody may stand at the television, so the run asks nothing.
   The two ways this layer has to put a question on the screen are the event a
   message box travels as and a command to the loop; neither is sent while the
   written batch is applied. */
TEST_CASE("a web batch writing video_Mode and video_Format runs the video group once and asks nothing", "[apply][video]")
{
	Fresh fresh;
	VideoBox box;
	FakeVideoOutput &out = box.out;
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	FakeEventSink events;
	InstalledEventSink evented(&events);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, videoSave);

	g_settings.video_Mode = VIDEO_STD_720P50;
	g_settings.video_Format = DISPLAY_AR_16_9;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);
	box.forget();

	REQUIRE(settings::set("video_Mode", number(VIDEO_STD_1080I50)).ok());
	REQUIRE(settings::set("video_Format", number(DISPLAY_AR_4_3)).ok());
	REQUIRE(out.calls.empty());
	const size_t posted = sink.posted.size();
	applyPendingSettings();

	REQUIRE(out.systems.size() == 1);
	REQUIRE(out.systems[0] == VIDEO_STD_1080I50);
	REQUIRE(out.aspects.size() == 1);
	REQUIRE(out.aspects[0].first == DISPLAY_AR_4_3);
	REQUIRE(events.sent.empty());
	// What the drain posts besides is its word that the settings landed, which no screen asks anything with.
	for (size_t i = posted; i < sink.posted.size(); ++i)
		CHECK(sink.posted[i].first == NeutrinoMessages::EVT_SETTINGS_WRITTEN);

	installRealSettingsSource(NULL, NULL);
}

/* The mode the box shows is X, the web writes Z, the menu then picks Y and the
   answer to the question is no: the box goes back to Z, the mode it had right
   before, and not to the X the menu was opened with. */
TEST_CASE("a no to the video mode question puts back the mode the box had right before", "[apply][video]")
{
	Fresh fresh;
	VideoBox box;
	FakeCommandSink sink;
	InstalledSink sunk(&sink);
	ClearedSettingsSource cleared;
	installRealSettingsSource(&g_settings, videoSave);

	g_settings.video_Mode = VIDEO_STD_720P50;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);

	REQUIRE(settings::set("video_Mode", number(VIDEO_STD_1080I50)).ok());
	applyPendingSettings();

	g_settings.video_Mode = VIDEO_STD_PAL;
	REQUIRE(applyKey("video_Mode") == Status::Ok);
	box.forget();

	int kept = videoModeBeforeLastChange();
	REQUIRE(keepOrRestore(g_settings.video_Mode, kept, "video_Mode", answerNo, applyRowKey));
	REQUIRE(g_settings.video_Mode == VIDEO_STD_1080I50);
	REQUIRE(box.out.systems.size() == 1);
	REQUIRE(box.out.systems[0] == VIDEO_STD_1080I50);

	installRealSettingsSource(NULL, NULL);
}

/* The channel daemon set the standard past the group, so what the group sent last
   is not on the box. A mode then picked in the menu is still asked about, with
   the mode the decoder ran as the one a no puts back. */
TEST_CASE("after another writer changed the standard the question puts back what the decoder ran", "[apply][video]")
{
	Fresh fresh;
	VideoBox box;
	registerApplyGroups();
	g_settings.video_Mode = VIDEO_STD_720P50;
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);

	forgetSentVideo(VideoSent::System);
	box.out.decoder_system = VIDEO_STD_1080I50;
	g_settings.video_Mode = VIDEO_STD_PAL;
	REQUIRE(applyKey("video_Mode") == Status::Ok);
	REQUIRE(videoModeBeforeLastChange() == VIDEO_STD_1080I50);

	// A decoder that cannot say leaves the setting as the baseline, which asks nothing.
	forgetSentVideo(VideoSent::System);
	box.out.decoder_system = -1;
	REQUIRE(applyKey("video_Mode") == Status::Ok);
	REQUIRE(videoModeBeforeLastChange() == VIDEO_STD_PAL);
}

TEST_CASE("a picture control key runs the picture group and not the video one", "[apply][video]")
{
	Fresh fresh;
	VideoBox box;
	FakeVideoOutput &out = box.out;
	registerApplyGroups();
	REQUIRE(runPhase(ApplyPhase::Decoders) == Status::Ok);
	box.forget();

	REQUIRE(applyKey("video_psi_tint") == Status::Ok);
	REQUIRE(out.systems.empty());
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	// Sent at startup and unchanged since.
	REQUIRE(out.calls.empty());
	g_settings.psi_tint = g_settings.psi_tint + 1;
	REQUIRE(applyKey("video_psi_tint") == Status::Ok);
	REQUIRE(out.count("control") == 1);
	g_settings.psi_tint = g_settings.psi_tint - 1;
#else
	REQUIRE(out.calls.empty());
#endif
}

TEST_CASE("a 4:3 mode other than the setting's was set past the video group", "[apply][video]")
{
	CHECK_FALSE(mode43PastGroup(1, 1));
	CHECK(mode43PastGroup(0, 1));
	CHECK(mode43PastGroup(2, 1));
}

TEST_CASE("the standards that take the larger screen are the family's", "[apply][video]")
{
	REQUIRE(osd::videoSystemNeeds1080(VIDEO_STD_1080I50));
	REQUIRE(osd::videoSystemNeeds1080(VIDEO_STD_1080P24));
	REQUIRE_FALSE(osd::videoSystemNeeds1080(VIDEO_STD_720P50));
	REQUIRE_FALSE(osd::videoSystemNeeds1080(VIDEO_STD_PAL));
#if HAVE_ARM_HARDWARE
	REQUIRE(osd::videoSystemNeeds1080(VIDEO_STD_2160P50));
#else
	REQUIRE_FALSE(osd::videoSystemNeeds1080(VIDEO_STD_2160P50));
#endif
}

/* The flags are numbered by the position of the mode's name, which is not the driver's number
   of the standard, so a flag set for one position must enable that standard and no other. */
TEST_CASE("the automatic modes are looked up by the driver's number and not indexed by it", "[apply][video]")
{
	const size_t n = VIDEOMENU_VIDEOMODE_OPTION_COUNT;
	size_t tried = 0;
	for (size_t i = 0; i < n; ++i)
	{
		const int standard = videoModeValue(i);
		if (standard < 0)
			continue;
		std::vector<int> flags(n, 0);
		flags[i] = 1;
		CHECK(osd::autoModeEnabled(&flags[0], n, standard));
		for (size_t j = 0; j < n; ++j)
		{
			const int other = videoModeValue(j);
			if (other >= 0 && other != standard)
				CHECK_FALSE(osd::autoModeEnabled(&flags[0], n, other));
		}
		++tried;
	}
	REQUIRE(tried > 0);
	std::vector<int> all(n, 1);
	CHECK_FALSE(osd::autoModeEnabled(&all[0], n, -1));
}

/* Every registered group, run with exactly the seams its own phase has: a group
   that reaches a seam installed later ends the suite here, whatever case its own
   area wrote. A seam that answers a silent default still passes, as apply.h says. */
TEST_CASE("every registered group runs with only the seams its phase has", "[apply][phase]")
{
	Fresh fresh;
	registerApplyGroups();
	const std::vector<const ApplyGroup *> all = applyGroups();
	REQUIRE(!all.empty());
	for (size_t i = 0; i < all.size(); ++i)
	{
		INFO("group " << all[i]->name);
		PhaseEnvironment env(all[i]->phase);
		all[i]->run();
		CHECK(env.unknown.empty());
	}
}

TEST_CASE("the phase table names only seams the test helper can stand in for", "[apply][phase]")
{
	PhaseEnvironment env(ApplyPhase::Network);
	INFO("seams the helper does not know: " << env.unknown.size());
	CHECK(env.unknown.empty());
	CHECK(env.read >= 17);
}

/* A stream's typo in a seam name, or the wrong fake type for it, must stop the
   case with the name; an unchecked lookup would index before the table or cast
   a fake to a type it is not. */
TEST_CASE("a fake asked for by an unknown name or the wrong type is refused loudly", "[apply][phase]")
{
	PhaseEnvironment env(ApplyPhase::Network);
	CHECK_NOTHROW(env.fake<FakeVideoOutput>("video"));
	CHECK_THROWS_WITH(env.fake<FakeVideoOutput>("vidoe"), Catch::Contains("no seam named vidoe"));
	CHECK_THROWS_WITH(env.fake<FakeVideoOutput>("system"), Catch::Contains("seam system is not of the type"));
	CHECK_THROWS_WITH(env.fake<FakeSystemSource>("video"), Catch::Contains("seam video is not of the type"));
}

/* What a group of an early phase finds: the box source, and not the seams the
   program installs later, which then answer as they do on the box. */
TEST_CASE("an early phase has the box source and none of the seams installed later", "[apply][phase]")
{
	std::vector<std::pair<int, int> > sizes;
	{
		PhaseEnvironment env(ApplyPhase::Decoders);
		BoxCapabilities caps;
		CHECK(systemSource().capabilities(caps) == Status::Ok);
		CHECK_FALSE(dependenciesInstalled());
		CHECK(osdResolutionSource().available(sizes) == Status::NotSupported);
	}
	{
		PhaseEnvironment env(ApplyPhase::Sectionsd);
		CHECK(dependenciesInstalled());
		CHECK(osdResolutionSource().available(sizes) == Status::Ok);
	}
	// Nothing is left installed behind it.
	CHECK_FALSE(dependenciesInstalled());
}
