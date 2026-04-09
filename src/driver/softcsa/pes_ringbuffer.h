/*
 * CPesRingbuffer - lock-free PES packet ringbuffer for SoftCSA
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

#ifndef __PES_RINGBUFFER_H__
#define __PES_RINGBUFFER_H__

#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <atomic>

class CPesRingbuffer
{
public:
	CPesRingbuffer(size_t capacity);
	~CPesRingbuffer();

	/* Producer: write a complete PES packet. Returns false on timeout or cancel. */
	bool write(const uint8_t *data, int len, int timeout_ms);

	/* Consumer: read next PES packet. Returns bytes read, 0 on timeout, -1 on cancel. */
	int read(uint8_t *data, int max_len, int timeout_ms);

	/* Signal shutdown — unblocks any waiting read/write */
	void cancel();

	/* Reset for reuse */
	void reset();

	bool isValid() const { return buf != NULL; }

private:
	uint8_t *buf;
	size_t capacity;
	size_t head;
	size_t tail;
	size_t used;

	std::mutex mtx;
	std::condition_variable cv_not_full;
	std::condition_variable cv_not_empty;
	std::atomic<bool> cancelled;
};

#endif
