/*
 * tsaudiotype.cpp - read the audio type of a recording's pids from its frames
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

#include <driver/tsaudiotype.h>
#include <zapit/channel.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define TS_SIZE		188
#define TS_SYNC		0x47

/* Enough of an SD or HD recording to hold dozens of audio PES packets of every
 * pid, and little enough to read over a network mount before playback starts. */
#define READ_SIZE	(2 * 1024 * 1024)

/* A PES packet need not start with a frame, so one that doesn't is not a vote.
 * Of the ones that do, this many have to name one type, and no fewer than three
 * in four of them, before the type counts as known. */
#define VOTES_NEEDED	3
#define VOTES_ENOUGH	8

int tsaudiotype::frameType(const uint8_t *p, size_t len)
{
	if (len >= 6 && p[0] == 0x0b && p[1] == 0x77) {
		if ((p[4] >> 6) == 3)
			return -1;		// reserved sample rate
		int bsid = p[5] >> 3;
		if (bsid <= 8)
			return CZapitAudioChannel::AC3;
		if (bsid >= 11 && bsid <= 16)
			return CZapitAudioChannel::EAC3;
		return -1;
	}
	if (len >= 4 && p[0] == 0x7f && p[1] == 0xfe && p[2] == 0x80 && p[3] == 0x01)
		return CZapitAudioChannel::DTS;
	if (len >= 3 && p[0] == 0xff && (p[1] & 0xe0) == 0xe0) {
		int layer = (p[1] >> 1) & 3;
		if (layer == 0) {
			/* ADTS: twelve bits of sync and a layer of 0 */
			if ((p[1] & 0xf6) == 0xf0 && ((p[2] >> 2) & 0x0f) < 13)
				return CZapitAudioChannel::AAC;
			return -1;
		}
		if ((p[1] & 0x18) == 0x08)
			return -1;		// reserved MPEG version
		if ((p[2] >> 4) == 0x0f || ((p[2] >> 2) & 3) == 3)
			return -1;		// bad bitrate or sample rate
		return CZapitAudioChannel::MPEG;
	}
	/* LATM/LOAS, which DVB carries HE-AAC in */
	if (len >= 3 && p[0] == 0x56 && (p[1] & 0xe0) == 0xe0)
		return CZapitAudioChannel::AACPLUS;
	return -1;
}

namespace
{
	struct Tally
	{
		std::map<int, int> votes;
		int total;
		Tally() : total(0) {}
	};

	/* The type of the frame a PES packet starts with, if the packet header
	 * and the frame start both lie in this one TS payload. */
	int pesFrameType(const uint8_t *p, size_t len)
	{
		if (len < 9 || p[0] != 0x00 || p[1] != 0x00 || p[2] != 0x01)
			return -1;
		uint8_t sid = p[3];
		/* MPEG audio streams, and private stream 1 for AC3, E-AC3, DTS */
		if (!((sid >= 0xc0 && sid <= 0xdf) || sid == 0xbd))
			return -1;
		size_t start = 9 + p[8];
		if (start >= len)
			return -1;
		return tsaudiotype::frameType(p + start, len - start);
	}
}

void tsaudiotype::detect(const uint8_t *buf, size_t len, const std::vector<uint16_t> &pids, std::map<uint16_t, int> &types)
{
	std::map<uint16_t, Tally> tally;
	for (std::vector<uint16_t>::const_iterator it = pids.begin(); it != pids.end(); ++it)
		tally[*it];
	if (tally.empty())
		return;

	/* a recording starts on a packet, but a cut one might not */
	size_t off = 0;
	while (off + 2 * TS_SIZE <= len && !(buf[off] == TS_SYNC && buf[off + TS_SIZE] == TS_SYNC))
		off++;

	size_t open = tally.size();
	for (; off + TS_SIZE <= len && open; off += TS_SIZE) {
		const uint8_t *ts = buf + off;
		if (ts[0] != TS_SYNC) {
			/* lost the packet grid: look for it again */
			size_t next = off + 1;
			while (next + 2 * TS_SIZE <= len && !(buf[next] == TS_SYNC && buf[next + TS_SIZE] == TS_SYNC))
				next++;
			if (next + 2 * TS_SIZE > len)
				break;
			off = next - TS_SIZE;
			continue;
		}
		if (ts[1] & 0x80)
			continue;		// transport error
		if (!(ts[1] & 0x40))
			continue;		// no PES starts here
		uint16_t pid = ((ts[1] & 0x1f) << 8) | ts[2];
		std::map<uint16_t, Tally>::iterator t = tally.find(pid);
		if (t == tally.end() || t->second.total >= VOTES_ENOUGH)
			continue;
		int afc = (ts[3] >> 4) & 3;
		if (!(afc & 1))
			continue;		// no payload
		size_t pay = 4;
		if (afc & 2)
			pay += 1 + ts[4];
		if (pay >= TS_SIZE)
			continue;
		int type = pesFrameType(ts + pay, TS_SIZE - pay);
		if (type < 0)
			continue;
		t->second.votes[type]++;
		if (++t->second.total == VOTES_ENOUGH)
			open--;
	}

	for (std::map<uint16_t, Tally>::const_iterator t = tally.begin(); t != tally.end(); ++t) {
		const Tally &v = t->second;
		for (std::map<int, int>::const_iterator b = v.votes.begin(); b != v.votes.end(); ++b) {
			if (b->second >= VOTES_NEEDED && 4 * b->second >= 3 * v.total) {
				types[t->first] = b->first;
				break;
			}
		}
	}
}

bool tsaudiotype::detectFile(const std::string &file, const std::vector<uint16_t> &pids, std::map<uint16_t, int> &types)
{
	int fd = open(file.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;
	std::vector<uint8_t> buf(READ_SIZE);
	size_t len = 0;
	while (len < buf.size()) {
		ssize_t r = read(fd, &buf[len], buf.size() - len);
		if (r < 0 && errno == EINTR)
			continue;
		if (r <= 0)
			break;
		len += r;
	}
	close(fd);
	if (len == 0)
		return false;
	detect(&buf[0], len, pids, types);
	return true;
}
