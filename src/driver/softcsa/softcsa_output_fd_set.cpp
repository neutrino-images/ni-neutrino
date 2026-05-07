/*
 * COutputFdSet - fan-out fd set for descrambled TS sinks
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
 
#include "softcsa_output_fd_set.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>

#define TAG "[softcsa output_fd_set] "

static inline uint64_t writeAll_now_us()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

COutputFdSet::COutputFdSet() : next_id(1)
{
}

COutputFdSet::~COutputFdSet()
{
	std::lock_guard<std::mutex> lk(mtx);
	for (auto *e : entries) delete e;
	entries.clear();
}

int COutputFdSet::add(int fd, FdRole role)
{
	int flags = ::fcntl(fd, F_GETFL, 0);
	if (flags < 0 || !(flags & O_NONBLOCK)) {
		printf(TAG "fd=%d not O_NONBLOCK; refusing to add\n", fd);
		return -1;
	}
	std::lock_guard<std::mutex> lk(mtx);
	auto *e = new Entry{ next_id++, fd, role, false, {0} };
	entries.push_back(e);
	return e->id;
}

void COutputFdSet::remove(int id)
{
	// Detach under the lock; once erased, no in-flight writeAll can
	// still reference the entry. delete runs unlocked.
	Entry *gone = nullptr;
	{
		std::lock_guard<std::mutex> lk(mtx);
		auto it = std::find_if(entries.begin(), entries.end(),
			[id](Entry *e) { return e->id == id; });
		if (it != entries.end()) {
			gone = *it;
			entries.erase(it);
		}
	}
	delete gone;
}

bool COutputFdSet::empty() const
{
	std::lock_guard<std::mutex> lk(mtx);
	return entries.empty();
}

std::size_t COutputFdSet::size() const
{
	std::lock_guard<std::mutex> lk(mtx);
	return entries.size();
}

int COutputFdSet::writeAll(const unsigned char *buf, std::size_t len)
{
	// Hold the lock across the entire iteration so remove() cannot
	// delete an entry under us. All registered fds are O_NONBLOCK so
	// each ::write is bounded; no slow-consumer stall to design around.
	std::lock_guard<std::mutex> lk(mtx);
	int ok = 0;
	for (auto *e : entries) {
		if (e->stale) continue;
		const unsigned char *ptr = buf;
		std::size_t rem = len;
		uint64_t stall_start_us = 0;
		while (rem > 0) {
			ssize_t n = ::write(e->fd, ptr, rem);
			if (n == (ssize_t)rem) {
				ptr += n;
				rem = 0;
			} else if (n < 0 && errno == EINTR) {
				continue;
			} else if (n < 0 && errno == EAGAIN) {
				// Pipe buffer full. Track stall start so we can
				// emit one diagnostic per stall episode rather
				// than per poll-tick.
				if (!stall_start_us)
					stall_start_us = writeAll_now_us();
				struct pollfd pfd = { e->fd, POLLOUT, 0 };
				::poll(&pfd, 1, 5);
			} else if (n < 0) {
				printf(TAG "fd=%d role=%d write failed: %s; marking stale\n",
					e->fd, (int)e->role, strerror(errno));
				e->stale = true;
				break;
			} else {
				// Partial write: retry remaining bytes.
				ptr += n;
				rem -= (std::size_t)n;
			}
		}
		if (rem == 0) {
			e->write_count.fetch_add(1, std::memory_order_relaxed);
			ok++;
			// Log stalls >= 50 ms so we can correlate disk pauses
			// with TS discontinuities in the final recording.
			if (stall_start_us) {
				uint64_t stall_us = writeAll_now_us() - stall_start_us;
				if (stall_us >= 50000)
					printf(TAG "fd=%d role=%d stalled %llu us\n",
						e->fd, (int)e->role,
						(unsigned long long)stall_us);
			}
		}
	}
	return ok;
}

uint64_t COutputFdSet::writeCountFor(int id)
{
	std::lock_guard<std::mutex> lk(mtx);
	for (auto *e : entries) {
		if (e->id == id)
			return e->write_count.load(std::memory_order_relaxed);
	}
	return 0;
}
