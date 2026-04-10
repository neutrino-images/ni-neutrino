/*
 * CSoftCSAManager - manages SoftCSA descrambling sessions
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

#ifndef __SOFTCSA_MANAGER_H__
#define __SOFTCSA_MANAGER_H__

#include <cstdint>
#include <map>
#include <zapit/client/zapitclient.h>
#include <condition_variable>
#include <mutex>
#include <utility>
#include <vector>

#include <zapit/channel.h>
#include "softcsa_session.h"

#define CW_ALGO_CSA_ALT 3 // Not in ca_descr_algo enum (0-2); from OSCam globals.h

class CSoftCSAManager
{
public:
	static CSoftCSAManager *getInstance();

	// Called by CDvbApiClient reader thread
	void onDescrMode(uint32_t demux_index, uint32_t algo, uint32_t cipher_mode);
	void onCW(uint32_t demux_index, uint32_t parity, const uint8_t *cw);
	void addReaderPid(uint32_t demux_index, unsigned short pid);

	// Called by zapit
	void registerDemux(uint32_t demux_index, t_channel_id channel_id,
	                   SoftCSASessionType type, int adapter, int demux_unit, int frontend_num);
	void addPid(uint32_t demux_index, unsigned short pid);
	void setDecoderPids(uint32_t demux_index, unsigned short vpid, unsigned short apid, unsigned short pcrpid);
	void setDecoderTypes(uint32_t demux_index, int video_type, int audio_type);
	bool stopSession(t_channel_id channel_id, SoftCSASessionType type);
	void stopAll();

	// Audio language change: update routed apid on the LIVE session for channel_id.
	// Returns true if an active session was found and updated.
	bool notifyAudioPidChange(t_channel_id channel_id, unsigned short new_apid);

	bool isActive(t_channel_id channel_id);

	// LIVE session exists and has a running worker
	bool hasRunningLiveSession(t_channel_id channel_id);

	// Recording: register fd and wait for OSCam to confirm CSA-ALT.
	// Returns true if recordThread started, false on timeout.
	bool waitForRecordStart(t_channel_id channel_id, int fd, int timeout_ms);

private:
	CSoftCSAManager();
	~CSoftCSAManager();

	struct DemuxState {
		t_channel_id channel_id;
		SoftCSASessionType type;
		int adapter;
		int demux_unit;
		int frontend_num;
		bool csa_alt_active;   // CA_SET_DESCR_MODE with algo==3 received
		uint8_t ecm_mode;      // stored per demux for setKey()
		CSoftCSASession *session;
		std::vector<unsigned short> pids; // stored before session creation
		std::vector<unsigned short> pending_reader_pids; // ECM PIDs queued before session start
		unsigned short video_pid;
		unsigned short audio_pid;
		unsigned short pcr_pid;
		int video_type;        // VIDEO_FORMAT stream type
		int audio_type;        // audio channel type for AUDIO_SET_BYPASS_MODE
		int record_fd;         // deferred fd for RECORD sessions (-1 = none)
	};

	// Primary: demux_index -> state (for CW routing from CDvbApiClient)
	std::map<uint32_t, DemuxState> demux_states;

	// Secondary: (channel_id, type) -> demux_index (for lifecycle from zapit)
	std::map<std::pair<t_channel_id, SoftCSASessionType>, uint32_t> channel_to_demux;

	std::mutex mtx;

	std::condition_variable record_cv;
	std::mutex record_cv_mtx;

};

#endif
