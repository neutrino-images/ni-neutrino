/*
 * test_fbc.cpp - tests for the FBC tuner manager
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

#include <zapit/fbc.h>

#include <cstdio>
#include <cstdlib>
#include <string>

/* The driver of an FBC module is only reachable through the files it keeps in
 * /proc/stb/frontend, so the cases build that tree in a directory of their own
 * and point the manager there: what it reads of a module, and what it writes
 * back for each way a slot is tied, are both visible there and nowhere else.
 * How zapit picks a slot and when it ties it lives in femanager.cpp, which
 * needs the hardware layer, and is not reached from here.
 */

namespace {

struct FakeProc
{
	std::string dir;
	CFBCTunerManager *fbc;

	FakeProc() : fbc(CFBCTunerManager::getInstance())
	{
		char tmpl[] = "/tmp/zapit_fbc_XXXXXX";
		if (mkdtemp(tmpl) != NULL)
			dir = tmpl;
		REQUIRE(!dir.empty());
		fbc->SetProcDir(dir);
		fbc->Clear();
	}

	~FakeProc()
	{
		fbc->Clear();
		fbc->SetProcDir("/proc/stb/frontend");
		if (!dir.empty())
			run("rm -rf " + dir);
	}

	static void run(const std::string &cmd)
	{
		const int rc = system(cmd.c_str());
		REQUIRE(rc == 0);
	}

	std::string path(int slot, const char *entry) const
	{
		return dir + "/" + std::to_string(slot) + "/" + entry;
	}

	void write(int slot, const char *entry, const std::string &body) const
	{
		run("mkdir -p " + dir + "/" + std::to_string(slot));
		FILE *f = fopen(path(slot, entry).c_str(), "w");
		REQUIRE(f != NULL);
		fputs(body.c_str(), f);
		fclose(f);
	}

	std::string read(int slot, const char *entry) const
	{
		FILE *f = fopen(path(slot, entry).c_str(), "r");
		if (!f)
			return "<absent>";
		char buf[64] = "";
		if (!fgets(buf, sizeof(buf), f))
			buf[0] = '\0';
		fclose(f);
		return buf;
	}

	/* One module as a VU+ driver shows it: eight slots with the same set
	 * id, the roots named on each, and the two files that get written. */
	void module(int first, int set_id, const char *choices = "0 1\n") const
	{
		for (int i = first; i < first + 8; i++) {
			write(i, "fbc_set_id", std::to_string(set_id) + "\n");
			write(i, "fbc_connect_choices", choices);
			write(i, "fbc_connect", "7\n");
			write(i, "fbc_link", "1\n");
		}
	}

	/* What linkFrontends() does: every slot, in order, from an empty map. */
	void probe(int first, int last) const
	{
		fbc->Clear();
		for (int i = first; i <= last; i++)
			fbc->Init(i, true);
	}
};

}

TEST_CASE("fbc: a slot the driver keeps no set id for is no FBC tuner", "[fbc]")
{
	FakeProc proc;
	proc.write(0, "fbc_connect", "0\n");

	CHECK_FALSE(proc.fbc->Init(0, true));
	CHECK_FALSE(proc.fbc->IsFBCTuner(0));
	CHECK(proc.fbc->GetRootOfSet(0) == -1);
	CHECK(proc.read(0, "fbc_connect") == "0\n");
}

TEST_CASE("fbc: a cable or terrestrial FBC tuner is left alone", "[fbc]")
{
	FakeProc proc;
	proc.module(0, 0);

	CHECK_FALSE(proc.fbc->Init(0, false));
	CHECK_FALSE(proc.fbc->IsFBCTuner(0));
	CHECK(proc.read(0, "fbc_connect") == "7\n");
}

TEST_CASE("fbc: two modules are told apart and each counts its own slots", "[fbc]")
{
	FakeProc proc;
	proc.module(0, 0);
	proc.module(8, 1);
	proc.probe(0, 15);

	for (int i = 0; i < 16; i++) {
		INFO("slot " << i);
		REQUIRE(proc.fbc->IsFBCTuner(i));
		CHECK(proc.fbc->GetSetID(i) == i / 8);
		CHECK(proc.fbc->GetPosition(i) == i % 8);
		CHECK(proc.fbc->IsRoot(i) == (i % 8 < 2));
		CHECK(proc.fbc->IsFBCLink(i) == (i % 8 >= 2));
		/* where the driver is left after the probe: own input or the first */
		CHECK(proc.read(i, "fbc_connect") == std::to_string(i % 8 < 2 ? i % 8 : 0));
		CHECK(proc.read(i, "fbc_link") == "0");
	}
	CHECK(proc.fbc->GetRootOfSet(5) == 0);
	CHECK(proc.fbc->GetRootOfSet(13) == 8);
	CHECK_FALSE(proc.fbc->IsSameSet(7, 8));
}

TEST_CASE("fbc: a second probe finds the same roots as the first", "[fbc]")
{
	/* linkFrontends(true) runs again whenever the frontend setup restarts
	 * the tuners. The position of a slot is counted by the slots of its set
	 * already known, so a second round over a map that still holds the first
	 * one put every slot at position 8 and left the module without a root. */
	FakeProc proc;
	proc.module(0, 0);
	proc.probe(0, 7);
	proc.probe(0, 7);

	CHECK(proc.fbc->GetPosition(0) == 0);
	CHECK(proc.fbc->GetPosition(7) == 7);
	CHECK(proc.fbc->IsRoot(0));
	CHECK(proc.fbc->IsRoot(1));
	CHECK(proc.fbc->IsFBCLink(2));
	CHECK(proc.fbc->GetRootOfSet(4) == 0);
}

TEST_CASE("fbc: the roots are the places of the module, not of the slots probed", "[fbc]")
{
	/* A slot left out of the probe, as an unused tuner was, moves every
	 * slot after it one place down: slot 2 then counts as the second root. */
	FakeProc proc;
	proc.module(0, 0);
	proc.fbc->Clear();
	for (int i = 1; i <= 7; i++)
		proc.fbc->Init(i, true);
	CHECK(proc.fbc->IsRoot(2));

	proc.probe(0, 7);
	CHECK_FALSE(proc.fbc->IsRoot(2));
	CHECK(proc.fbc->IsRoot(1));
}

TEST_CASE("fbc: a loop link sees its root's input and leaves the LNB to it", "[fbc]")
{
	FakeProc proc;
	proc.module(0, 0);
	proc.probe(0, 7);

	REQUIRE(proc.fbc->Connect(4, 1, true));
	CHECK(proc.read(4, "fbc_connect") == "1");
	CHECK(proc.read(4, "fbc_link") == "1");
	CHECK(proc.fbc->GetConnectedTo(4) == 1);
	CHECK(proc.fbc->IsLinkedTo(4, 1));
	CHECK_FALSE(proc.fbc->IsLinkedTo(4, 0));
}

TEST_CASE("fbc: a twin link sees its root's input and keeps its own control", "[fbc]")
{
	/* unicable: every demodulator sends its own commands down the cable,
	 * and enigma2 never marks a unicable tuner as linked */
	FakeProc proc;
	proc.module(0, 0);
	proc.probe(0, 7);

	REQUIRE(proc.fbc->Connect(5, 1, false));
	CHECK(proc.read(5, "fbc_connect") == "1");
	CHECK(proc.read(5, "fbc_link") == "0");
	CHECK(proc.fbc->GetConnectedTo(5) == 1);
	CHECK_FALSE(proc.fbc->IsLinkedTo(5, 1));
}

TEST_CASE("fbc: an untied link goes back to the first input, unlinked", "[fbc]")
{
	FakeProc proc;
	proc.module(0, 0);
	proc.probe(0, 7);

	REQUIRE(proc.fbc->Connect(3, 1, true));
	REQUIRE(proc.fbc->Disconnect(3));
	CHECK(proc.read(3, "fbc_connect") == "0");
	CHECK(proc.read(3, "fbc_link") == "0");
	CHECK_FALSE(proc.fbc->IsLinkedTo(3, 1));
	CHECK_FALSE(proc.fbc->IsLinkedTo(3, 0));

	/* a root is never untied */
	CHECK_FALSE(proc.fbc->Disconnect(1));
}

TEST_CASE("fbc: a link is tied only to a root of its own module", "[fbc]")
{
	FakeProc proc;
	proc.module(0, 0);
	proc.module(8, 1);
	proc.probe(0, 15);

	/* a root of the other module, another link, nothing at all */
	CHECK_FALSE(proc.fbc->Connect(3, 9, true));
	CHECK_FALSE(proc.fbc->Connect(3, 4, true));
	CHECK_FALSE(proc.fbc->Connect(3, -1, true));
	CHECK(proc.read(3, "fbc_connect") == "0");
	CHECK(proc.read(3, "fbc_link") == "0");
	CHECK_FALSE(proc.fbc->IsLinkedTo(3, 9));
}

TEST_CASE("fbc: a root stays on its own input whatever it is tied to", "[fbc]")
{
	FakeProc proc;
	proc.module(0, 0);
	proc.probe(0, 7);
	proc.write(1, "fbc_connect", "5\n");

	REQUIRE(proc.fbc->Connect(1, 1));
	CHECK(proc.read(1, "fbc_connect") == "1");
	REQUIRE(proc.fbc->Connect(1, 0));
	CHECK(proc.read(1, "fbc_connect") == "1");
	CHECK_FALSE(proc.fbc->IsLinkedTo(1, 0));
}

TEST_CASE("fbc: a fresh probe forgets how the links were tied", "[fbc]")
{
	FakeProc proc;
	proc.module(0, 0);
	proc.probe(0, 7);
	REQUIRE(proc.fbc->Connect(6, 1, true));

	proc.probe(0, 7);
	CHECK_FALSE(proc.fbc->IsLinkedTo(6, 1));
	CHECK(proc.read(6, "fbc_connect") == "0");
	CHECK(proc.read(6, "fbc_link") == "0");
}
