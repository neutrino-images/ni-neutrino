/*
 * test_applyservices.cpp - tests for the services apply group
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
#include "support/nothreads.h"
#include "support/phaseenv.h"

#include "coreapi/base/apply.h"
#include "coreapi/base/eventbus.h"
#include "coreapi/box/applyworker.h"
#include "coreapi/box/apply_services.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace coreapi;

namespace
{

/* The group hands the slow part to the apply worker and returns; a case looks at
   what was sent once the worker is done. */
Status waited(Status s)
{
	applyWorker().wait();
	return s;
}

struct ServicesBox
{
	PhaseEnvironment    env;
	FakeServiceControl &services;

	ServicesBox() : env(ApplyPhase::Network), services(env.fake<FakeServiceControl>("services"))
	{
		resetApplyRegistry();
		resetSentServices();
		registerApplyGroups();
	}
	~ServicesBox()
	{
		resetSentServices();
		resetApplyRegistry();
	}

	// The startup run, which only notes which flags are there.
	void boot() { REQUIRE(waited(runPhase(ApplyPhase::Network)) == Status::Ok); }
};

} // namespace

TEST_CASE("the services group answers for every daemon and softcam flag and for no other flag", "[apply][services]")
{
	ServicesBox box;
	const char *const keys[] = { "flag_daemon_fritzcallmonitor", "flag_daemon_samba", "flag_daemon_crond",
				     "flag_camd_mgcamd", "flag_camd_oscam", "flag_camd_gbox" };
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		INFO(keys[i]);
		REQUIRE(groupOf(keys[i]) == &kServicesApplyGroup);
	}
	REQUIRE(groupOf("flag_hddpower") == NULL);
	REQUIRE(groupOf("flag_scart_osd_fix") == NULL);
}

TEST_CASE("startup notes the flags that are there and starts and stops nothing", "[apply][services]")
{
	ServicesBox box;
	box.services.set("samba", true);
	box.services.set("oscam", true);
	box.boot();
	REQUIRE(box.services.calls.empty());
}

TEST_CASE("a flag that appears starts its daemon once and a flag that goes stops it once", "[apply][services]")
{
	ServicesBox box;
	box.boot();

	box.services.set("nfsd", true);
	REQUIRE(waited(applyKey("flag_daemon_nfsd")) == Status::Ok);
	REQUIRE(box.services.calls.size() == 1);
	REQUIRE(box.services.calls[0] == "start:nfsd");

	// A sibling key runs the group again and the daemon, which runs, is left alone.
	REQUIRE(waited(applyKey("flag_daemon_samba")) == Status::Ok);
	REQUIRE(box.services.calls.size() == 1);

	box.services.set("nfsd", false);
	REQUIRE(waited(applyKey("flag_daemon_nfsd")) == Status::Ok);
	REQUIRE(box.services.calls.size() == 2);
	REQUIRE(box.services.calls[1] == "stop:nfsd");
}

TEST_CASE("a softcam is started and stopped through the camd script", "[apply][services]")
{
	ServicesBox box;
	box.boot();

	box.services.set("oscam", true);
	REQUIRE(waited(applyKey("flag_camd_oscam")) == Status::Ok);
	box.services.set("oscam", false);
	REQUIRE(waited(applyKey("flag_camd_oscam")) == Status::Ok);

	REQUIRE(box.services.calls.size() == 2);
	REQUIRE(box.services.calls[0] == "start:camd:oscam");
	REQUIRE(box.services.calls[1] == "stop:camd:oscam");
}

TEST_CASE("a batch of two flags runs the group once and touches each daemon once", "[apply][services]")
{
	ServicesBox box;
	box.boot();

	box.services.set("samba", true);
	box.services.set("dropbear", true);
	std::vector<std::string> keys;
	keys.push_back("flag_daemon_samba");
	keys.push_back("flag_daemon_dropbear");
	REQUIRE(waited(applyBatch(keys)) == Status::Ok);

	REQUIRE(box.services.calls.size() == 2);
	REQUIRE(box.services.calls[0] == "start:samba");
	REQUIRE(box.services.calls[1] == "start:dropbear");
}

TEST_CASE("a start that failed is tried again by the next run", "[apply][services]")
{
	ServicesBox box;
	box.boot();

	box.services.set("crond", true);
	box.services.answer = Status::Internal;
	// The worker meets the failure after the run has answered.
	REQUIRE(waited(applyKey("flag_daemon_crond")) == Status::Ok);
	REQUIRE(box.services.calls.size() == 1);

	box.services.answer = Status::Ok;
	REQUIRE(waited(applyKey("flag_daemon_crond")) == Status::Ok);
	REQUIRE(box.services.calls.size() == 2);
	REQUIRE(waited(applyKey("flag_daemon_crond")) == Status::Ok);
	REQUIRE(box.services.calls.size() == 2);
}

namespace
{

struct FailureWatcher : public Subscriber
{
	std::mutex m;
	std::vector<Event> seen;
	FailureWatcher() { EventBus::instance().subscribe(this); }
	void onEvent(const Event &e)
	{
		if (e.type != EventType::SettingApplyFailed)
			return;
		std::lock_guard<std::mutex> lock(m);
		seen.push_back(e);
	}
};

} // namespace

/* The write that queued the start was answered ok before the script ran, so the failure is
   said on the bus, under the key that was written. */
TEST_CASE("a start that failed on the worker is published with its key", "[apply][services]")
{
	ServicesBox box;
	box.boot();
	FailureWatcher watch;

	box.services.set("crond", true);
	box.services.answer = Status::Internal;
	REQUIRE(waited(applyKey("flag_daemon_crond")) == Status::Ok);
	REQUIRE(watch.seen.size() == 1);
	CHECK(watch.seen[0].text == "flag_daemon_crond");
	CHECK(watch.seen[0].value == 500);
	CHECK(watch.seen[0].initiator == "box");

	box.services.answer = Status::Ok;
	REQUIRE(waited(applyKey("flag_daemon_crond")) == Status::Ok);
	CHECK(watch.seen.size() == 1);
}

TEST_CASE("a stop that failed is not asked again by the runs after it", "[apply][services]")
{
	ServicesBox box;
	// A flag left behind by a program that is gone, seen at startup and removed by the screen.
	box.services.set("emmrd", true);
	box.boot();
	box.services.set("emmrd", false);

	box.services.answer = Status::Internal;
	REQUIRE(waited(applyKey("flag_daemon_samba")) == Status::Ok);
	REQUIRE(box.services.calls.size() == 1);
	REQUIRE(box.services.calls[0] == "stop:emmrd");

	box.services.answer = Status::Ok;
	REQUIRE(waited(applyKey("flag_daemon_samba")) == Status::Ok);
	REQUIRE(box.services.calls.size() == 1);
}

TEST_CASE("a stop that failed for an installed daemon is tried again by the next run", "[apply][services]")
{
	ServicesBox box;
	box.services.set("emmrd", true);
	box.services.present.push_back("emmrd");
	box.boot();
	box.services.set("emmrd", false);

	box.services.answer = Status::Internal;
	REQUIRE(waited(applyKey("flag_daemon_samba")) == Status::Ok);
	REQUIRE(waited(applyKey("flag_daemon_samba")) == Status::Ok);
	REQUIRE(box.services.calls.size() == 2);
	REQUIRE(box.services.calls[1] == "stop:emmrd");
}

namespace
{

/* Holds the first call that enters it until the case lets it go. A call that waits
   longer than a run of the group should take gives up and says so, so a group that
   runs the script itself fails the case rather than hanging it. */
struct Gate
{
	std::mutex m;
	std::condition_variable cv;
	bool used;
	bool inside;
	bool open;
	bool gave_up;
	Gate() : used(false), inside(false), open(false), gave_up(false) {}

	void enter()
	{
		std::unique_lock<std::mutex> lock(m);
		if (used)
			return;
		used = true;
		inside = true;
		cv.notify_all();
		if (!cv.wait_for(lock, std::chrono::seconds(2), [this]() { return open; }))
			gave_up = true;
		inside = false;
	}

	void waitInside()
	{
		std::unique_lock<std::mutex> lock(m);
		cv.wait_for(lock, std::chrono::seconds(5), [this]() { return inside || gave_up; });
	}

	bool isInside()
	{
		std::lock_guard<std::mutex> lock(m);
		return inside;
	}

	void letGo()
	{
		std::lock_guard<std::mutex> lock(m);
		open = true;
		cv.notify_all();
	}
};

} // namespace

/* A web write runs the group on the program's loop, and a service script runs for
   seconds, a softcam's one more. */
TEST_CASE("a run hands the script to the worker and returns while it runs", "[apply][services]")
{
	ServicesBox box;
	box.boot();
	Gate gate;
	box.services.before = [&gate]() { gate.enter(); };

	box.services.set("nfsd", true);
	REQUIRE(applyKey("flag_daemon_nfsd") == Status::Ok);
	gate.waitInside();
	CHECK(gate.isInside());
	gate.letGo();
	applyWorker().wait();
	REQUIRE_FALSE(gate.gave_up);
	REQUIRE(box.services.calls.size() == 1);
	REQUIRE(box.services.calls[0] == "start:nfsd");
}

TEST_CASE("a newer request for a service whose script has not started takes its place", "[apply][services]")
{
	ServicesBox box;
	box.boot();
	Gate gate;
	box.services.before = [&gate]() { gate.enter(); };

	// The worker is held inside the first script.
	box.services.set("samba", true);
	REQUIRE(applyKey("flag_daemon_samba") == Status::Ok);
	gate.waitInside();

	box.services.set("nfsd", true);
	REQUIRE(applyKey("flag_daemon_nfsd") == Status::Ok);
	box.services.set("nfsd", false);
	REQUIRE(applyKey("flag_daemon_nfsd") == Status::Ok);
	gate.letGo();
	applyWorker().wait();

	REQUIRE_FALSE(gate.gave_up);
	REQUIRE(box.services.calls.size() == 2);
	REQUIRE(box.services.calls[0] == "start:samba");
	REQUIRE(box.services.calls[1] == "stop:nfsd");

	// What the group holds as sent is the newer request.
	REQUIRE(waited(applyKey("flag_daemon_nfsd")) == Status::Ok);
	REQUIRE(box.services.calls.size() == 2);
	box.services.set("nfsd", true);
	REQUIRE(waited(applyKey("flag_daemon_nfsd")) == Status::Ok);
	REQUIRE(box.services.calls.size() == 3);
	REQUIRE(box.services.calls[2] == "start:nfsd");
}

TEST_CASE("a script that gets no thread is a failed start and is tried again", "[apply][services]")
{
	ServicesBox box;
	box.boot();

	box.services.set("crond", true);
	{
		NoNewThreads none;
		REQUIRE_FALSE(NoNewThreads::canMake());
		REQUIRE(applyKey("flag_daemon_crond") == Status::Internal);
	}
	applyWorker().wait();
	REQUIRE(box.services.calls.empty());

	REQUIRE(waited(applyKey("flag_daemon_crond")) == Status::Ok);
	REQUIRE(box.services.calls.size() == 1);
	REQUIRE(box.services.calls[0] == "start:crond");
}

TEST_CASE("a closed worker drops what waits, lets what runs finish and takes nothing more", "[apply][worker]")
{
	ApplyWorker worker;
	Gate gate;
	std::vector<std::string> ran;
	std::mutex ran_m;
	REQUIRE(worker.post("a", [&]() {
		gate.enter();
		std::lock_guard<std::mutex> lock(ran_m);
		ran.push_back("a");
	}));
	gate.waitInside();
	REQUIRE(worker.post("b", [&]() {
		std::lock_guard<std::mutex> lock(ran_m);
		ran.push_back("b");
	}));

	// Lets the first job go only once the worker refuses, which is once close() has dropped what waits.
	std::atomic<bool> probe_ran(false);
	std::thread opener([&]() {
		while (worker.post("probe", [&probe_ran]() { probe_ran = true; }))
			std::this_thread::yield();
		gate.letGo();
	});
	worker.close();
	opener.join();
	REQUIRE_FALSE(gate.gave_up);
	REQUIRE_FALSE(probe_ran);
	REQUIRE(ran.size() == 1);
	REQUIRE(ran[0] == "a");
	REQUIRE_FALSE(worker.post("c", [&]() { ran.push_back("c"); }));

	worker.reopen();
	REQUIRE(worker.post("d", [&]() { ran.push_back("d"); }));
	worker.wait();
	REQUIRE(ran.size() == 2);
	REQUIRE(ran[1] == "d");
}

/* The restart of a display service followed by the force of its next pass: a newer
   force must not run ahead of a restart asked for after the force it replaced. */
TEST_CASE("a newer request replaces a waiting one at the back of the queue", "[apply][worker]")
{
	ApplyWorker worker;
	Gate gate;
	std::vector<std::string> ran;
	std::mutex ran_m;
	auto job = [&ran, &ran_m](const std::string &name) {
		return [&ran, &ran_m, name]() {
			std::lock_guard<std::mutex> lock(ran_m);
			ran.push_back(name);
		};
	};
	REQUIRE(worker.post("long", [&]() { gate.enter(); }));
	gate.waitInside();
	REQUIRE(worker.post("lcd4l.force", job("force 1")));
	REQUIRE(worker.post("lcd4l.mode", job("restart")));
	REQUIRE(worker.post("lcd4l.force", job("force 2")));
	gate.letGo();
	worker.wait();
	REQUIRE_FALSE(gate.gave_up);
	REQUIRE(ran.size() == 2);
	REQUIRE(ran[0] == "restart");
	REQUIRE(ran[1] == "force 2");
}

TEST_CASE("waiting for one's own work does not wait for another's", "[apply][worker]")
{
	ApplyWorker worker;
	Gate gate;
	REQUIRE(worker.post("hdIdle.disks", [&]() { gate.enter(); }));
	gate.waitInside();
	REQUIRE(worker.pending("hdIdle."));
	REQUIRE_FALSE(worker.pending("lcd4l."));
	worker.waitFor("lcd4l.");
	CHECK(gate.isInside());

	std::atomic<bool> restarted(false);
	REQUIRE(worker.post("lcd4l.mode", [&restarted]() { restarted = true; }));
	REQUIRE(worker.pending("lcd4l."));
	gate.letGo();
	worker.waitFor("lcd4l.");
	REQUIRE(restarted);
	REQUIRE_FALSE(worker.pending("lcd4l."));
	REQUIRE_FALSE(gate.gave_up);
	worker.wait();
}

TEST_CASE("a close that a hung job outlasts gives up and says so", "[apply][worker]")
{
	ApplyWorker worker;
	Gate gate;
	REQUIRE(worker.post("services.samba", [&]() { gate.enter(); }));
	gate.waitInside();
	REQUIRE_FALSE(worker.close(1));
	CHECK(gate.isInside());
	REQUIRE_FALSE(worker.post("services.nfsd", []() {}));
	gate.letGo();
	worker.wait();
	REQUIRE_FALSE(gate.gave_up);
	REQUIRE(worker.close(1));
}

/* A close drops the jobs that wait: each of those, and only those, is told, so whoever
   recorded the value as sent can send it again. */
TEST_CASE("a close tells each job it drops and no other", "[apply][worker]")
{
	ApplyWorker worker;
	Gate gate;
	std::atomic<int> held_dropped(0), older_dropped(0), newer_dropped(0);
	REQUIRE(worker.post("hold", [&]() { gate.enter(); }, [&]() { ++held_dropped; }));
	gate.waitInside();
	REQUIRE(worker.post("services.nfsd", []() {}, [&]() { ++older_dropped; }));
	REQUIRE(worker.post("services.nfsd", []() {}, [&]() { ++newer_dropped; }));

	std::thread opener([&]() {
		while (worker.post("probe", []() {}))
			std::this_thread::yield();
		gate.letGo();
	});
	REQUIRE(worker.close());
	opener.join();
	REQUIRE_FALSE(gate.gave_up);
	REQUIRE(held_dropped == 0);
	REQUIRE(older_dropped == 0);
	REQUIRE(newer_dropped == 1);
}

/* A value recorded as sent when its job was queued, and then dropped by a close: after the
   worker takes jobs again, as after a flash that failed, the next run must send it. */
TEST_CASE("a send that a close drops is marked to be sent again", "[apply][worker]")
{
	Gate gate;
	REQUIRE(applyWorker().post("hold", [&]() { gate.enter(); }));
	gate.waitInside();

	SentFlags flags;
	Sent<int> sent;
	Status first = Status::Ok;
	std::atomic<int> calls(0);
	postChanged(first, flags, 1u, sent, 5, "fixture.slot", "fixture_key", [&calls]() { ++calls; return Status::Ok; });
	REQUIRE(first == Status::Ok);
	REQUIRE(sent.known);
	REQUIRE(sent.value == 5);

	std::thread opener([&]() {
		while (applyWorker().post("probe", []() {}))
			std::this_thread::yield();
		gate.letGo();
	});
	REQUIRE(applyWorker().close());
	opener.join();
	applyWorker().reopen();
	REQUIRE_FALSE(gate.gave_up);
	REQUIRE(calls == 0);
	REQUIRE(flags.take() == 1u);
}

/* The end of the program closes twice, before its own teardown and again in the stop of
   the daemons: a hung job is waited for once. */
TEST_CASE("a second close of a closed worker does not wait again", "[apply][worker]")
{
	ApplyWorker worker;
	Gate gate;
	REQUIRE(worker.post("services.samba", [&]() { gate.enter(); }));
	gate.waitInside();
	REQUIRE_FALSE(worker.close(1));
	const std::chrono::steady_clock::time_point before = std::chrono::steady_clock::now();
	REQUIRE_FALSE(worker.close(1));
	CHECK(std::chrono::steady_clock::now() - before < std::chrono::milliseconds(500));
	CHECK(gate.isInside());
	gate.letGo();
	worker.wait();
	REQUIRE_FALSE(gate.gave_up);
}
