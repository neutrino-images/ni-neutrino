/*
 * CDvrDemuxSlot - DVR loopback demux slot allocator for SoftCSA
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
 
#include "softcsa_dvr_demux_slot.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <set>
#include <unistd.h>
#include <sys/ioctl.h>
#include <dmx_hal.h>

#define TAG "[softcsa dvr_demux_slot] "

static bool is_demux_reserved(int adapter, int unit)
{
	/* Stub: high-index scan keeps low-index CI/CAM reservations
	 * untouched on current platforms. Refine per platform if a small
	 * MAX_DMX_UNITS box collides on the way down. */
	(void)adapter;
	(void)unit;
	return false;
}

CDvrDemuxSlot::CDvrDemuxSlot(int adapter, int demux_unit, int dvr_fd)
	: m_adapter(adapter), m_demux_unit(demux_unit), m_dvr_fd(dvr_fd)
{
}

CDvrDemuxSlot::~CDvrDemuxSlot()
{
	if (m_dvr_fd >= 0) ::close(m_dvr_fd);
}

CDvrDemuxSlot *CDvrDemuxSlot::alloc(int adapter, const std::set<int> &excluded_units)
{
	/* dmx_hal.h carries the BOXMODEL-aware DVR source base. The vendor
	 * driver routes dvr<M> writes to demux<M> when demux<M> source is
	 * DVR0+M. */
	int offset = (int)DMX_SOURCE_DVR0;

	for (int unit = MAX_DMX_UNITS - 1; unit >= 0; unit--) {
		if (is_demux_reserved(adapter, unit))
			continue;
		if (excluded_units.count(unit))
			continue;

		char path[64];
		snprintf(path, sizeof(path),
			"/dev/dvb/adapter%d/demux%d", adapter, unit);
		int dmx_fd = ::open(path, O_RDWR | O_NONBLOCK);
		if (dmx_fd < 0) continue;

		int src = offset + unit;
		int rc = ioctl(dmx_fd, DMX_SET_SOURCE, &src);
		if (rc < 0) {
			printf(TAG "demux%d DMX_SET_SOURCE DVR(%d) failed: %s\n",
				unit, src, strerror(errno));
			::close(dmx_fd);
			continue;
		}
		::close(dmx_fd);

		char dvr_path[64];
		snprintf(dvr_path, sizeof(dvr_path),
			"/dev/dvb/adapter%d/dvr%d", adapter, unit);
		int dvr_fd = ::open(dvr_path, O_WRONLY | O_NONBLOCK);
		if (dvr_fd < 0) {
			printf(TAG "open %s O_WRONLY failed: %s\n",
				dvr_path, strerror(errno));
			continue;
		}

		printf(TAG "allocated demux%d -> dvr%d (DVR source %d)\n",
			unit, unit, src);
		return new CDvrDemuxSlot(adapter, unit, dvr_fd);
	}

	printf(TAG "no free demux slot on adapter%d\n", adapter);
	return nullptr;
}
