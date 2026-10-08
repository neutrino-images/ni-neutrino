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

#include "support/catch.hpp"

#include "coreapi/base/apply.h"

#include <pthread.h>

#include <string>
#include <vector>

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
