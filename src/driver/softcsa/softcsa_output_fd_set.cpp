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
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#define TAG "[softcsa output_fd_set] "

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
		ssize_t n = ::write(e->fd, buf, len);
		if (n == (ssize_t)len) {
			e->write_count.fetch_add(1, std::memory_order_relaxed);
			ok++;
		} else if (n < 0 && errno == EAGAIN) {
			/* drop silently; consumer will catch up via kernel buffering */
		} else if (n < 0) {
			printf(TAG "fd=%d role=%d write failed: %s; marking stale\n",
				e->fd, (int)e->role, strerror(errno));
			e->stale = true;
		} else {
			printf(TAG "fd=%d role=%d short write n=%zd of %zu\n",
				e->fd, (int)e->role, n, len);
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
