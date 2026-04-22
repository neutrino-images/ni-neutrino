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

struct SoftCSAStopNotify {
	uint32_t session_id;
	int capmt_demux;
	uint8_t capmt_ca_mask;
};

struct SoftCSAStopResult {
	bool had_running_session;
	std::vector<SoftCSAStopNotify> dvbapi_stops;
};

class CSoftCSAManager
{
public:
	static CSoftCSAManager *getInstance();

	void onDescrMode(uint32_t session_id, uint32_t algo, uint32_t cipher_mode);
	void onCW(uint32_t session_id, uint32_t parity, const uint8_t *cw);

	uint32_t registerSession(t_channel_id channel_id, SoftCSASessionType type,
	                         int adapter, int frontend_num,
	                         uint8_t capmt_ca_mask);
	void addPid(uint32_t session_id, unsigned short pid);
	void addPidByChannel(t_channel_id channel_id, SoftCSASessionType type, unsigned short pid);
	void setDecoderPids(uint32_t session_id, unsigned short vpid, unsigned short apid, unsigned short pcrpid);
	void setDecoderTypes(uint32_t session_id, int video_type, int audio_type);
	void setPipDevIndex(uint32_t session_id, int pip_dev);
	SoftCSAStopResult stopSession(t_channel_id channel_id, SoftCSASessionType type);
	void stopAll();

	struct ResubscribeInfo {
		t_channel_id channel_id;
		uint32_t session_id;
		SoftCSASessionType type;
		int capmt_demux;
		int frontend_num;
		uint8_t capmt_ca_mask;
	};
	void stopSessions();
	std::vector<ResubscribeInfo> getResubscribeInfo();

	// Audio language change: update routed apid on the LIVE session for channel_id.
	// Returns true if an active session was found and updated.
	bool notifyAudioPidChange(t_channel_id channel_id, unsigned short new_apid);

	bool isActive(t_channel_id channel_id);

	// LIVE session exists and has a running worker
	bool hasRunningLiveSession(t_channel_id channel_id);

	uint32_t getLiveSessionId(t_channel_id channel_id);
	uint32_t getSessionId(t_channel_id channel_id, SoftCSASessionType type);
	int getCapmtDemux(uint32_t session_id);
	bool hasRunningSibling(t_channel_id channel_id, uint32_t exclude_session_id);
	bool hasAnyRunningSession(t_channel_id channel_id);

	// Recording: register fd and wait for OSCam to confirm CSA-ALT.
	// Returns true if recordThread started, false on timeout.
	bool waitForRecordStart(t_channel_id channel_id, int fd, int timeout_ms);

	// Streaming: register callback and wait for OSCam to confirm CSA-ALT.
	// Returns true if streamThread started, false on timeout.
	bool waitForStreamStart(t_channel_id channel_id, SoftCSAStreamCallback cb, int timeout_ms);
	bool hasRegisteredSession(t_channel_id channel_id, SoftCSASessionType type);

	bool cloneAndStartStream(uint32_t session_id, SoftCSAStreamCallback cb);
	bool cloneAndStartRecord(uint32_t session_id, int fd);
	bool cloneAndStartLive(uint32_t session_id, int vfd, int afd);
	bool cloneAndStartPip(uint32_t session_id, int pip_vfd);

private:
	CSoftCSAManager();
	~CSoftCSAManager();

	struct SessionState {
		t_channel_id channel_id;
		SoftCSASessionType type;
		int adapter;
		int demux_unit;      // allocated kernel demux (from allocator)
		int capmt_demux;     // value for CAPMT 0x86 descriptor (for OSCam ca_mask)
		uint8_t capmt_ca_mask;
		int frontend_num;
		bool csa_alt_active;
		/* True only while a stream/record sibling still depends on this
		 * live's oscam subscription (capmt path b suppresses the
		 * sibling's own capmt while live exists). Distinguishes a
		 * parked hw live from an actively-decoding one. */
		bool retained;
		uint8_t ecm_mode;
		CSoftCSASession *session;
		std::vector<unsigned short> pids;
		unsigned short video_pid;
		unsigned short audio_pid;
		unsigned short pcr_pid;
		int video_type;
		int audio_type;
		int pip_dev;         // PiP device index (0, 1, 2) — only for PIP sessions
		int record_fd;
		SoftCSAStreamCallback stream_callback;
	};

	std::map<uint32_t, SessionState> sessions;
	std::map<std::pair<t_channel_id, SoftCSASessionType>, uint32_t> channel_to_session;
	uint32_t next_session_id;

	std::mutex mtx;

	std::condition_variable record_cv;
	std::mutex record_cv_mtx;

	struct DemuxAlloc {
		int demux_unit;
		int frontend_num;
		int refcount;
	};
	std::vector<DemuxAlloc> demux_allocs;

	int allocateDemux(int frontend_num);
	void releaseDemux(int frontend_num);

	/* Caller must hold mtx. */
	uint32_t findRunningSibling(t_channel_id channel_id, uint32_t exclude_session_id) const;

	/* When LIVE becomes CSA-ALT active, walk same-channel siblings that
	 * have a pending callback/fd and clone keys so they don't time out
	 * waiting for a duplicate filter that never arrives. */
	int tryCloneSiblingsFromLive(uint32_t live_session_id);

};

#endif
