/*
 * tsaudiotype.h - read the audio type of a recording's pids from its frames
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

#ifndef __TSAUDIOTYPE_H__
#define __TSAUDIOTYPE_H__

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

/* The .xml of a recording stores each audio pid's type as a number of
 * CZapitAudioChannel::ZapitAudioChannelType, and that enum was renumbered in
 * January 2021 (MPEG and AC3 swapped, AAC and DTS moved). The xml does not say
 * which numbering it was written with, and the PMT a recording carries was
 * wrong for AC3 for a while too. The audio frames themselves are not: each
 * format starts its frames with a sync word of its own. These functions read
 * that word at the start of the first PES packets of each pid. */
namespace tsaudiotype
{
	/* The type the frame at p starts as, a ZapitAudioChannelType value, or -1
	 * when the bytes are no frame start this knows. */
	int frameType(const uint8_t *p, size_t len);

	/* Reads the transport stream in buf and puts a type for every pid of pids
	 * whose PES packets agree on one into types. A pid left out was not
	 * found, or its frames did not agree often enough to be sure. */
	void detect(const uint8_t *buf, size_t len, const std::vector<uint16_t> &pids, std::map<uint16_t, int> &types);

	/* The same for the first bytes of the file; false when it can't be read. */
	bool detectFile(const std::string &file, const std::vector<uint16_t> &pids, std::map<uint16_t, int> &types);
}

#endif
