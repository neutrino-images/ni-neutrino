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
 
// Multicast write target for the softcsa tap reader. One instance per
// (frontend, service); session output FDs are added/removed as
// LIVE/RECORD/STREAM sessions come and go.
//
// All registered FDs must be O_NONBLOCK (enforced at add()): writeAll()
// holds the lock across every ::write so remove() and the reader
// thread cannot collide on a half-deleted entry.

#ifndef __SOFTCSA_OUTPUT_FD_SET_H__
#define __SOFTCSA_OUTPUT_FD_SET_H__

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

class COutputFdSet
{
public:
	enum FdRole {
		ROLE_LIVE,
		ROLE_RECORD,
		ROLE_STREAM
	};

	COutputFdSet();
	~COutputFdSet();

	// FD must be O_NONBLOCK; rejects (returns -1) otherwise. Caller
	// retains fd ownership and must close it after remove().
	int add(int fd, FdRole role);

	// Synchronises with any in-flight writeAll, then removes. Caller
	// is free to close fd after remove() returns.
	void remove(int id);

	bool empty() const;
	std::size_t size() const;

	// Returns successful-write count; non-EAGAIN errors mark the entry
	// stale (kept in the list to preserve ids, skipped on subsequent
	// iterations).
	int writeAll(const unsigned char *buf, std::size_t len);

	// Running successful-write counter for the given id, used by
	// the manager's wait_for_data_timeout_ms gate.
	uint64_t writeCountFor(int id);

private:
	struct Entry {
		int id;
		int fd;
		FdRole role;
		bool stale;
		std::atomic<uint64_t> write_count;
	};

	mutable std::mutex mtx;
	std::vector<Entry *> entries;
	int next_id;
};

#endif
