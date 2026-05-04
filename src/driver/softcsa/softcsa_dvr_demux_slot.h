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
 
// Allocates a /dev/dvb/adapterN/demuxM slot bound as DMX_SOURCE_DVR<M>
// and opens /dev/dvb/adapterN/dvrM O_WRONLY | O_NONBLOCK.
//
// The DMX_SET_SOURCE binding is device-persistent across fd cycles, so
// the demux fd is closed immediately after the ioctl.

#ifndef __SOFTCSA_DVR_DEMUX_SLOT_H__
#define __SOFTCSA_DVR_DEMUX_SLOT_H__

#include <set>

class CDvrDemuxSlot
{
public:
	// Tries demux slots from MAX_DMX_UNITS-1 downward (the tap and the
	// main decoder demux live in the low indices). excluded_units skips
	// units already owned by live taps on the same adapter so the DVR
	// slot cannot collide on boxes with a small MAX_DMX_UNITS.
	// Returns nullptr on failure.
	static CDvrDemuxSlot *alloc(int adapter,
	                            const std::set<int> &excluded_units = std::set<int>());

	~CDvrDemuxSlot();

	int adapter() const { return m_adapter; }
	int demuxUnit() const { return m_demux_unit; }
	int writeFd() const { return m_dvr_fd; }   // O_WRONLY | O_NONBLOCK

private:
	CDvrDemuxSlot(int adapter, int demux_unit, int dvr_fd);

	CDvrDemuxSlot(const CDvrDemuxSlot &) = delete;
	CDvrDemuxSlot &operator=(const CDvrDemuxSlot &) = delete;

	int m_adapter;
	int m_demux_unit;
	int m_dvr_fd;
};

#endif
