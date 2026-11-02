/*
 * test_tsaudiotype.cpp - tests for reading the audio type of a recording
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

#include <driver/tsaudiotype.h>
#include <zapit/channel.h>

#include <cstdio>
#include <string>
#include <unistd.h>
#include <vector>

namespace
{
	/* Frame starts as each format begins them: 48 kHz, a usual bitrate. */
	const uint8_t kAc3[]    = { 0x0b, 0x77, 0x12, 0x34, 0x14, 0x40, 0x00, 0x00 };	// bsid 8
	const uint8_t kEac3[]   = { 0x0b, 0x77, 0x02, 0x00, 0x3f, 0x80, 0x00, 0x00 };	// bsid 16
	const uint8_t kMpeg[]   = { 0xff, 0xfd, 0xa4, 0x00, 0x00, 0x00, 0x00, 0x00 };	// layer II, 192 kbit/s
	const uint8_t kAdts[]   = { 0xff, 0xf1, 0x4c, 0x80, 0x00, 0x00, 0x00, 0x00 };	// AAC LC, 48 kHz
	const uint8_t kLatm[]   = { 0x56, 0xe1, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00 };
	const uint8_t kDts[]    = { 0x7f, 0xfe, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00 };
	const uint8_t kNoise[]  = { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0 };

	/* One TS packet that starts a PES packet on pid, with frame right after
	 * the PES header. */
	void addPes(std::vector<uint8_t> &ts, uint16_t pid, uint8_t sid, const uint8_t *frame, size_t frame_len, bool tei = false)
	{
		size_t at = ts.size();
		ts.resize(at + 188, 0xff);
		uint8_t *p = &ts[at];
		p[0] = 0x47;
		p[1] = (tei ? 0x80 : 0) | 0x40 | (pid >> 8);
		p[2] = pid & 0xff;
		p[3] = 0x10;
		uint8_t pes[] = { 0x00, 0x00, 0x01, sid, 0x00, 0x00, 0x80, 0x80, 0x05, 0x21, 0x00, 0x01, 0x00, 0x01 };
		size_t o = 4;
		for (size_t i = 0; i < sizeof(pes); i++)
			p[o++] = pes[i];
		for (size_t i = 0; i < frame_len; i++)
			p[o++] = frame[i];
	}

	/* A packet on pid that only continues a PES packet. */
	void addMore(std::vector<uint8_t> &ts, uint16_t pid)
	{
		size_t at = ts.size();
		ts.resize(at + 188, 0x0b);
		ts[at] = 0x47;
		ts[at + 1] = pid >> 8;
		ts[at + 2] = pid & 0xff;
		ts[at + 3] = 0x10;
	}

	void addFrames(std::vector<uint8_t> &ts, uint16_t pid, uint8_t sid, const uint8_t *frame, int count)
	{
		for (int i = 0; i < count; i++) {
			addPes(ts, pid, sid, frame, 8);
			addMore(ts, pid);
		}
	}

	std::map<uint16_t, int> detectIn(const std::vector<uint8_t> &ts, const std::vector<uint16_t> &pids)
	{
		std::map<uint16_t, int> types;
		tsaudiotype::detect(ts.empty() ? NULL : &ts[0], ts.size(), pids, types);
		return types;
	}
}

TEST_CASE("each frame start names the type of its format", "[tsaudiotype]")
{
	REQUIRE(tsaudiotype::frameType(kAc3, sizeof(kAc3)) == CZapitAudioChannel::AC3);
	REQUIRE(tsaudiotype::frameType(kEac3, sizeof(kEac3)) == CZapitAudioChannel::EAC3);
	REQUIRE(tsaudiotype::frameType(kMpeg, sizeof(kMpeg)) == CZapitAudioChannel::MPEG);
	REQUIRE(tsaudiotype::frameType(kAdts, sizeof(kAdts)) == CZapitAudioChannel::AAC);
	REQUIRE(tsaudiotype::frameType(kLatm, sizeof(kLatm)) == CZapitAudioChannel::AACPLUS);
	REQUIRE(tsaudiotype::frameType(kDts, sizeof(kDts)) == CZapitAudioChannel::DTS);
	REQUIRE(tsaudiotype::frameType(kNoise, sizeof(kNoise)) == -1);
}

TEST_CASE("a frame start with a reserved field is no frame start", "[tsaudiotype]")
{
	const uint8_t ac3_bad_rate[]  = { 0x0b, 0x77, 0x12, 0x34, 0xd4, 0x40 };
	const uint8_t ac3_bad_bsid[]  = { 0x0b, 0x77, 0x12, 0x34, 0x14, 0x50 };	// bsid 10
	const uint8_t mpeg_bad_rate[] = { 0xff, 0xfd, 0xf4, 0x00 };
	const uint8_t mpeg_bad_freq[] = { 0xff, 0xfd, 0xac, 0x00 };
	const uint8_t mpeg_bad_ver[]  = { 0xff, 0xed, 0xa4, 0x00 };
	REQUIRE(tsaudiotype::frameType(ac3_bad_rate, sizeof(ac3_bad_rate)) == -1);
	REQUIRE(tsaudiotype::frameType(ac3_bad_bsid, sizeof(ac3_bad_bsid)) == -1);
	REQUIRE(tsaudiotype::frameType(mpeg_bad_rate, sizeof(mpeg_bad_rate)) == -1);
	REQUIRE(tsaudiotype::frameType(mpeg_bad_freq, sizeof(mpeg_bad_freq)) == -1);
	REQUIRE(tsaudiotype::frameType(mpeg_bad_ver, sizeof(mpeg_bad_ver)) == -1);
	REQUIRE(tsaudiotype::frameType(kAc3, 5) == -1);
}

TEST_CASE("every audio pid of a recording gets the type its frames have", "[tsaudiotype]")
{
	std::vector<uint8_t> ts;
	for (int i = 0; i < 6; i++) {
		addMore(ts, 0x100);			// video in between
		addPes(ts, 0x101, 0xc0, kMpeg, 8);
		addPes(ts, 0x102, 0xbd, kAc3, 8);
		addPes(ts, 0x103, 0xc1, kAdts, 8);
	}
	std::vector<uint16_t> pids;
	pids.push_back(0x101);
	pids.push_back(0x102);
	pids.push_back(0x103);
	std::map<uint16_t, int> types = detectIn(ts, pids);
	REQUIRE(types.size() == 3);
	REQUIRE(types[0x101] == CZapitAudioChannel::MPEG);
	REQUIRE(types[0x102] == CZapitAudioChannel::AC3);
	REQUIRE(types[0x103] == CZapitAudioChannel::AAC);
}

TEST_CASE("a pid with too few frame starts stays unknown", "[tsaudiotype]")
{
	std::vector<uint8_t> ts;
	addFrames(ts, 0x101, 0xc0, kMpeg, 2);
	addFrames(ts, 0x102, 0xbd, kNoise, 6);	// PES that start mid frame
	std::vector<uint16_t> pids;
	pids.push_back(0x101);
	pids.push_back(0x102);
	pids.push_back(0x104);			// not in the file at all
	REQUIRE(detectIn(ts, pids).empty());
}

TEST_CASE("frame starts that disagree too often leave the pid unknown", "[tsaudiotype]")
{
	std::vector<uint8_t> ts;
	addFrames(ts, 0x101, 0xbd, kAc3, 3);
	addFrames(ts, 0x101, 0xbd, kMpeg, 2);
	std::vector<uint16_t> pids(1, 0x101);
	REQUIRE(detectIn(ts, pids).empty());

	/* one stray among many still lets the rest decide */
	std::vector<uint8_t> ts2;
	addFrames(ts2, 0x101, 0xbd, kAc3, 6);
	addFrames(ts2, 0x101, 0xbd, kMpeg, 1);
	std::map<uint16_t, int> types = detectIn(ts2, pids);
	REQUIRE(types.size() == 1);
	REQUIRE(types[0x101] == CZapitAudioChannel::AC3);
}

TEST_CASE("only PES packets of audio streams and intact packets are read", "[tsaudiotype]")
{
	std::vector<uint8_t> ts;
	for (int i = 0; i < 6; i++)
		addPes(ts, 0x101, 0xe0, kMpeg, 8);	// a video stream id
	for (int i = 0; i < 6; i++)
		addPes(ts, 0x102, 0xc0, kMpeg, 8, true);	// transport error set
	std::vector<uint16_t> pids;
	pids.push_back(0x101);
	pids.push_back(0x102);
	REQUIRE(detectIn(ts, pids).empty());
}

TEST_CASE("a recording cut off the packet grid is found again", "[tsaudiotype]")
{
	std::vector<uint8_t> ts(77, 0x47);		// the tail of a cut packet
	addFrames(ts, 0x101, 0xbd, kAc3, 4);
	ts.insert(ts.begin() + 77 + 3 * 188, 5, 0x00);	// five bytes lost later on
	addFrames(ts, 0x101, 0xbd, kAc3, 2);
	std::vector<uint16_t> pids(1, 0x101);
	std::map<uint16_t, int> types = detectIn(ts, pids);
	REQUIRE(types.size() == 1);
	REQUIRE(types[0x101] == CZapitAudioChannel::AC3);
}

TEST_CASE("an empty buffer or no pids find nothing", "[tsaudiotype]")
{
	std::vector<uint8_t> ts;
	std::vector<uint16_t> pids(1, 0x101);
	REQUIRE(detectIn(ts, pids).empty());
	addFrames(ts, 0x101, 0xbd, kAc3, 4);
	REQUIRE(detectIn(ts, std::vector<uint16_t>()).empty());
}

TEST_CASE("the file is read from its start", "[tsaudiotype]")
{
	char name[] = "/tmp/ni-tsaudiotype-XXXXXX";
	int fd = mkstemp(name);
	REQUIRE(fd >= 0);
	std::vector<uint8_t> ts;
	addFrames(ts, 0x101, 0xc0, kMpeg, 4);
	REQUIRE(write(fd, &ts[0], ts.size()) == (ssize_t)ts.size());
	close(fd);

	std::vector<uint16_t> pids(1, 0x101);
	std::map<uint16_t, int> types;
	REQUIRE(tsaudiotype::detectFile(name, pids, types));
	REQUIRE(types[0x101] == CZapitAudioChannel::MPEG);
	unlink(name);

	types.clear();
	REQUIRE_FALSE(tsaudiotype::detectFile(name, pids, types));
	REQUIRE(types.empty());
}
