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

#ifndef __TS_DEMUXER_H__
#define __TS_DEMUXER_H__

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

class CTsDemuxer
{
public:
	CTsDemuxer(unsigned short video_pid, unsigned short audio_pid);

	/* Process a buffer of descrambled TS packets (188-byte aligned).
	 * Calls on_pes for each complete PES packet found.
	 * The PES data pointer is valid only during the callback. */
	void process(const uint8_t *ts_data, int len,
	             const std::function<void(unsigned short pid,
	                                      const uint8_t *pes, int pes_len)> &on_pes);

	/* Reset internal state (e.g., on channel change) */
	void reset();

	static const size_t MAX_PES_BUF_SIZE = 2 * 1024 * 1024; /* safety limit — matches MAX_PES_SIZE */

private:
	struct PidState {
		unsigned short pid;
		std::vector<uint8_t> pes_buf;
		bool has_data;
	};

	PidState video;
	PidState audio;

	void flushPid(PidState &ps,
	              const std::function<void(unsigned short, const uint8_t *, int)> &on_pes);
	void handlePacket(const uint8_t *pkt,
	                  const std::function<void(unsigned short, const uint8_t *, int)> &on_pes);
};

#endif
