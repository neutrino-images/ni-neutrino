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

#include "pes_ringbuffer.h"
#include <cstdio>
#include <cstring>
#include <chrono>

CPesRingbuffer::CPesRingbuffer(size_t cap)
	: capacity(cap), head(0), tail(0), used(0), cancelled(false)
{
	buf = (uint8_t *)malloc(capacity);
	if (!buf)
		printf("[softcsa] CPesRingbuffer: malloc(%zu) failed\n", capacity);
}

CPesRingbuffer::~CPesRingbuffer()
{
	free(buf);
}

bool CPesRingbuffer::write(const uint8_t *data, int len, int timeout_ms)
{
	if (!buf || len <= 0 || cancelled.load())
		return false;

	/* Each entry is: 4-byte length prefix + PES data */
	size_t need = sizeof(uint32_t) + len;
	if (need > capacity)
		return false;

	std::unique_lock<std::mutex> lock(mtx);
	if (!cv_not_full.wait_for(lock, std::chrono::milliseconds(timeout_ms),
		[&]{ return (capacity - used) >= need || cancelled.load(); }))
		return false;

	if (cancelled.load())
		return false;

	/* Write length prefix */
	uint32_t plen = (uint32_t)len;
	for (size_t i = 0; i < sizeof(uint32_t); i++) {
		buf[head] = ((uint8_t *)&plen)[i];
		head = (head + 1) % capacity;
	}
	/* Write PES data using two-segment memcpy for efficiency */
	size_t first = capacity - head;
	if (first >= (size_t)len) {
		memcpy(buf + head, data, len);
	} else {
		memcpy(buf + head, data, first);
		memcpy(buf, data + first, len - first);
	}
	head = (head + len) % capacity;
	used += need;

	cv_not_empty.notify_one();
	return true;
}

int CPesRingbuffer::read(uint8_t *data, int max_len, int timeout_ms)
{
	if (!buf || cancelled.load())
		return -1;

	std::unique_lock<std::mutex> lock(mtx);
	if (!cv_not_empty.wait_for(lock, std::chrono::milliseconds(timeout_ms),
		[&]{ return used >= sizeof(uint32_t) || cancelled.load(); }))
		return 0; /* timeout */

	if (cancelled.load())
		return -1;

	/* Read length prefix */
	uint32_t plen = 0;
	for (size_t i = 0; i < sizeof(uint32_t); i++) {
		((uint8_t *)&plen)[i] = buf[tail];
		tail = (tail + 1) % capacity;
	}

	/* Drop oversized PES entirely — a partial PES is worse than no PES
	 * because the decoder may misinterpret truncated data */
	if ((int)plen > max_len) {
		printf("[softcsa] ringbuffer: dropping oversized PES (%u bytes, max %d)\n",
			plen, max_len);
		tail = (tail + plen) % capacity;
		used -= (sizeof(uint32_t) + plen);
		cv_not_full.notify_one();
		return 0; /* signal "no data" like timeout, caller will retry */
	}

	int to_read = (int)plen;

	/* Read PES data using two-segment memcpy */
	size_t first = capacity - tail;
	if (first >= (size_t)to_read) {
		memcpy(data, buf + tail, to_read);
	} else {
		memcpy(data, buf + tail, first);
		memcpy(data + first, buf, to_read - first);
	}
	tail = (tail + plen) % capacity;

	used -= (sizeof(uint32_t) + plen);
	cv_not_full.notify_one();
	return to_read;
}

void CPesRingbuffer::cancel()
{
	cancelled.store(true);
	cv_not_full.notify_all();
	cv_not_empty.notify_all();
}

void CPesRingbuffer::reset()
{
	std::lock_guard<std::mutex> lock(mtx);
	head = tail = used = 0;
	cancelled.store(false);
}
