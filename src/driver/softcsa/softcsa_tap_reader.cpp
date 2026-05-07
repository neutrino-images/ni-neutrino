/*
 * CTapReader - scrambled TS frontend tap with inline descrambling
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
 
#include "softcsa_tap_reader.h"
#include "softcsa_output_fd_set.h"
#include "softcsa_engine.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <vector>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/dvb/dmx.h>

#define TAG "[softcsa tap_reader] "

CTapReader::CTapReader(int adapter, int demux_unit, int frontend_num,
	CSoftCSAEngine *engine, COutputFdSet *outputs)
	: m_adapter(adapter), m_demux_unit(demux_unit), m_frontend_num(frontend_num),
	  m_engine(engine), m_outputs(outputs),
	  m_demux_fd(-1), m_running(false)
{
}

CTapReader::~CTapReader()
{
	stop();
}

void CTapReader::addPid(uint16_t pid)
{
	std::lock_guard<std::mutex> lk(m_pid_mtx);
	auto ins = m_pids.insert(pid);
	if (!ins.second)
		return;
	if (m_demux_fd >= 0) {
		/* DMX_SET_PES_FILTER allows only one filter per fd; subsequent
		 * PIDs in the same TSDEMUX_TAP output mux are added via
		 * DMX_ADD_PID. Retry on EAGAIN/EINTR (transient on some
		 * vendor drivers). */
		while (true) {
			uint16_t p = pid;
			if (ioctl(m_demux_fd, DMX_ADD_PID, &p) < 0) {
				if (errno == EAGAIN || errno == EINTR) continue;
				printf(TAG "DMX_ADD_PID 0x%x failed: %s\n", pid, strerror(errno));
			}
			break;
		}
	}
}

void CTapReader::removePid(uint16_t pid)
{
	std::lock_guard<std::mutex> lk(m_pid_mtx);
	m_pids.erase(pid);
	// The DVB API has no per-filter remove ioctl; hard removal would
	// need close+reopen of the demux fd. Current callers are idempotent
	// (addPid dedups in m_pids) so this is effectively only a bookkeeping
	// drop; surplus TS lands in the broadcast and consumers discard it.
	// Use stop()+start() if hard removal is ever needed.
}

int CTapReader::openTapAndConfigure()
{
	char path[64];
	snprintf(path, sizeof(path),
		"/dev/dvb/adapter%d/demux%d", m_adapter, m_demux_unit);
	/* O_RDONLY: tap only reads. O_RDWR triggers different filter-slot
	 * validation on some vendor drivers. O_NONBLOCK prevents read() from
	 * blocking when poll() races or returns POLLIN without payload, which
	 * would otherwise stall the loop in iter 1 before the diag heartbeat
	 * ever fires. */
	m_demux_fd = ::open(path, O_RDONLY | O_NONBLOCK);
	if (m_demux_fd < 0) {
		printf(TAG "open %s failed: %s\n", path, strerror(errno));
		return -1;
	}

	/* 1.54 MB (8192 TS packets) absorbs ~5 s of disk-write stalls. */
	unsigned long bufsz = 8192UL * 188UL;
	if (ioctl(m_demux_fd, DMX_SET_BUFFER_SIZE, bufsz) < 0) {
		bufsz = 1024UL * 1024UL;
		if (ioctl(m_demux_fd, DMX_SET_BUFFER_SIZE, bufsz) < 0)
			printf(TAG "DMX_SET_BUFFER_SIZE failed: %s\n", strerror(errno));
	}

	/* No DMX_SET_SOURCE: device-source is already correct from the
	 * live channel allocation. Re-issuing it on a demux with active
	 * filters (live DECODER, pmtDemux, sectionsd) can stop them on
	 * some vendor drivers even when the value is identical. */

	std::lock_guard<std::mutex> lk(m_pid_mtx);
	auto it = m_pids.begin();
	if (it != m_pids.end()) {
		struct dmx_pes_filter_params p;
		memset(&p, 0, sizeof(p));
		p.pid = *it;
		p.input = DMX_IN_FRONTEND;
		p.output = DMX_OUT_TSDEMUX_TAP;
		p.pes_type = DMX_PES_OTHER;
		p.flags = 0;
		if (ioctl(m_demux_fd, DMX_SET_PES_FILTER, &p) < 0)
			printf(TAG "PES filter pid=0x%x failed: %s\n",
				*it, strerror(errno));
		else if (ioctl(m_demux_fd, DMX_START) < 0)
			printf(TAG "DMX_START failed: %s\n", strerror(errno));
		++it;
	}
	for (; it != m_pids.end(); ++it) {
		uint16_t pid = *it;
		while (true) {
			uint16_t p = pid;
			if (ioctl(m_demux_fd, DMX_ADD_PID, &p) < 0) {
				if (errno == EAGAIN || errno == EINTR) continue;
				printf(TAG "DMX_ADD_PID 0x%x failed: %s\n", pid, strerror(errno));
			}
			break;
		}
	}
	return 0;
}

int CTapReader::start()
{
	if (m_running.load()) return 0;
	if (openTapAndConfigure() < 0) return -1;
	m_running = true;
	m_thread = std::thread(&CTapReader::readerLoop, this);
	printf(TAG "started on adapter%d demux%d frontend%d\n",
		m_adapter, m_demux_unit, m_frontend_num);
	return 0;
}

void CTapReader::stop()
{
	if (!m_running.exchange(false)) return;
	printf(TAG "stopping adapter%d demux%d frontend%d\n",
		m_adapter, m_demux_unit, m_frontend_num);
	if (m_thread.joinable()) m_thread.join();
	if (m_demux_fd >= 0) {
		::close(m_demux_fd);
		m_demux_fd = -1;
	}
}

void CTapReader::readerLoop()
{
	// 47k buffer, aligned to TS packet size.
	const std::size_t kChunk = 188 * 256;
	std::vector<unsigned char> buf(kChunk);

	while (m_running.load()) {
		struct pollfd pfd = { .fd = m_demux_fd, .events = POLLIN, .revents = 0 };
		int pr = ::poll(&pfd, 1, 200);
		if (pr <= 0) continue;

		ssize_t n = ::read(m_demux_fd, buf.data(), buf.size());
		if (n < 0) {
			if (errno == EINTR || errno == EAGAIN) continue;
			printf(TAG "read failed: %s\n", strerror(errno));
			break;
		}
		if (n == 0) continue;
		ssize_t aligned = n - (n % 188);
		if (aligned <= 0) continue;
		m_engine->descramble(buf.data(), (int)aligned);
		m_outputs->writeAll(buf.data(), (size_t)aligned);
	}
}
