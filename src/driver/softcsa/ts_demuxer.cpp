/*
 * CTsDemuxer - TS packet demuxer for PES extraction
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

#include "ts_demuxer.h"
#include <cstddef>
#include <cstdio>

#define TS_PACKET_SIZE 188
#define TS_SYNC_BYTE   0x47

CTsDemuxer::CTsDemuxer(unsigned short video_pid, unsigned short audio_pid)
{
	video.pid = video_pid;
	video.has_data = false;
	audio.pid = audio_pid;
	audio.has_data = false;
}

void CTsDemuxer::reset()
{
	video.pes_buf.clear();
	video.has_data = false;
	audio.pes_buf.clear();
	audio.has_data = false;
}

void CTsDemuxer::setAudioPid(unsigned short pid)
{
	audio.pid = pid;
	audio.pes_buf.clear();
	audio.has_data = false;
}

void CTsDemuxer::flushPid(PidState &ps,
                           const std::function<void(unsigned short, const uint8_t *, int)> &on_pes)
{
	if (ps.has_data && ps.pes_buf.size() > 0) {
		on_pes(ps.pid, ps.pes_buf.data(), (int)ps.pes_buf.size());
		ps.pes_buf.clear();
		ps.has_data = false;
	}
}

void CTsDemuxer::handlePacket(const uint8_t *pkt,
                               const std::function<void(unsigned short, const uint8_t *, int)> &on_pes)
{
	if (pkt[0] != TS_SYNC_BYTE)
		return;

	unsigned short pid = ((pkt[1] & 0x1F) << 8) | pkt[2];
	bool pusi = (pkt[1] & 0x40) != 0;
	int afc = (pkt[3] >> 4) & 3;

	PidState *ps = NULL;
	if (pid == video.pid)
		ps = &video;
	else if (pid == audio.pid)
		ps = &audio;
	else
		return;

	/* Calculate payload offset */
	int payload_off = 4;
	if (afc & 2) { /* adaptation field present */
		int af_len = pkt[4];
		payload_off = 5 + af_len;
	}
	if (!(afc & 1)) /* no payload */
		return;
	if (payload_off >= TS_PACKET_SIZE)
		return;

	const uint8_t *payload = pkt + payload_off;
	int payload_len = TS_PACKET_SIZE - payload_off;

	if (pusi) {
		flushPid(*ps, on_pes);
		ps->has_data = true;
	}

	if (ps->has_data) {
		if (ps->pes_buf.size() + payload_len > MAX_PES_BUF_SIZE) {
			printf("[softcsa] ts_demuxer: PES buffer overflow for PID %04x (%zu bytes), resetting\n",
				ps->pid, ps->pes_buf.size());
			ps->pes_buf.clear();
			ps->has_data = false;
			return;
		}
		ps->pes_buf.insert(ps->pes_buf.end(), payload, payload + payload_len);
	}
}

void CTsDemuxer::process(const uint8_t *ts_data, int len,
                          const std::function<void(unsigned short pid,
                                                   const uint8_t *pes, int pes_len)> &on_pes)
{
	for (int off = 0; off + TS_PACKET_SIZE <= len; off += TS_PACKET_SIZE) {
		handlePacket(ts_data + off, on_pes);
	}
}
