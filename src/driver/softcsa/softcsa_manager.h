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
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <utility>
#include <vector>

#include <zapit/channel.h>
#include <zapit/client/zapitclient.h>
#include "softcsa_engine.h"
#include "softcsa_output_fd_set.h"
#include "softcsa_tap_reader.h"

class CDvrDemuxSlot;

#define CW_ALGO_CSA_ALT 3 // Not in ca_descr_algo enum (0-2); OSCam-specific marker.

enum SoftCSASessionType {
	SOFTCSA_SESSION_LIVE,
	SOFTCSA_SESSION_PIP,
	SOFTCSA_SESSION_RECORD,
	SOFTCSA_SESSION_STREAM
};

typedef std::function<void(const uint8_t *data, int len)> SoftCSAStreamCallback;

struct SoftCSAStopNotify {
	uint32_t session_id;
	int capmt_demux;
	uint8_t capmt_ca_mask;
};

struct SoftCSAStopResult {
	std::vector<SoftCSAStopNotify> dvbapi_stops;
};

class CSoftCSAManager
{
public:
	static CSoftCSAManager *getInstance();

	void onDescrMode(uint32_t session_id, uint32_t algo, uint32_t cipher_mode);
	void onCW(uint32_t session_id, uint32_t parity, const uint8_t *cw);

	/* passive=true marks the session as dormant: capmt is still sent
	 * so oscam's slot bookkeeping stays clean, but descrmode is ignored,
	 * cw is dropped, engine never starts, and sibling lookups skip
	 * this entry. */
	uint32_t registerSession(t_channel_id channel_id, SoftCSASessionType type,
	                         int adapter, int frontend_num,
	                         uint8_t capmt_ca_mask, bool passive);
	void addPid(uint32_t session_id, unsigned short pid);
	void addPidByChannel(t_channel_id channel_id, SoftCSASessionType type, unsigned short pid);
	void setDecoderPids(uint32_t session_id, unsigned short vpid, unsigned short apid, unsigned short pcrpid);
	void setDecoderTypes(uint32_t session_id, int video_type, int audio_type);
	void setPipDevIndex(uint32_t session_id, int pip_dev);
	/* record_fd / stream_handle scope the stop to a single RECORD or
	 * STREAM output; session-wide teardown only runs once the last
	 * output goes. The sentinels (-1, 0) tear the whole session down
	 * and are used by all non-overlapping callers and the dvbapi drain
	 * in stopSessions. */
	SoftCSAStopResult stopSession(t_channel_id channel_id, SoftCSASessionType type,
	                              int record_fd = -1,
	                              uint32_t stream_handle = 0);

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

	uint32_t getSessionId(t_channel_id channel_id, SoftCSASessionType type);
	int getCapmtDemux(uint32_t session_id);
	uint8_t getCapmtCaMask(uint32_t session_id);
	/* Mirrors registerSession's internal reuse condition: true when a
	 * session for (channel_id, type) already exists on the same frontend
	 * and is still alive. */
	bool willReuseSession(t_channel_id channel_id, SoftCSASessionType type,
	                      int frontend_num);
	bool isPassiveSession(uint32_t session_id);
	bool hasAnyRunningSession(t_channel_id channel_id);

	/* Returns the session type registered via registerSession.
	 * Used by the PMT-update handler to dispatch per session-type
	 * (LIVE -> softcsa_*Demux pesFilter, RECORD/STREAM -> tap-reader,
	 * PIP -> pipVideoDemux/pipAudioDemux pesFilter). */
	SoftCSASessionType getSessionType(uint32_t session_id);

	/* Returns the pip_dev index set by setPipDevIndex, or -1 if the
	 * session is not a PIP or pip_dev was never assigned. */
	int getPipDevIndex(uint32_t session_id);

	/* Snapshot of all currently-registered session IDs (LIVE, RECORD,
	 * STREAM, PIP -- active and passive) on the given channel. The
	 * caller iterates without holding the manager mutex; if a session
	 * disappears between snapshot and use, the per-session lookups
	 * inside the manager handle it gracefully. */
	std::vector<uint32_t> getActiveSessionsForChannel(t_channel_id channel_id);

	/* Replace the per-session pid bookkeeping with new_pids:
	 *  - PIDs in new_pids that are not in the existing list are added
	 *    via tap_reader->addPid (vendor DMX_ADD_PID).
	 *  - PIDs in the existing list that are not in new_pids are dropped
	 *    from bookkeeping; the vendor filter cannot per-pid-remove, so
	 *    surplus TS keeps flowing until the tap restarts at session-stop.
	 *  - tap-reader-null-safe: if the session is passive or has no
	 *    bound tap, only the bookkeeping list is replaced. */
	void replaceSessionPids(uint32_t session_id, const std::set<uint16_t> &new_pids);

	// Register the recording fd and wait for OSCam's CSA-ALT confirm.
	// Returns true if descrambling started, false on timeout.
	bool waitForRecordStart(t_channel_id channel_id, int fd, int timeout_ms);

	/* Returns a nonzero StreamOutput handle on success, 0 on timeout
	 * or any failure. The caller passes the handle back to stopSession
	 * so a per-stream stop never tears down a sibling stream on the
	 * same SessionState. */
	uint32_t waitForStreamStart(t_channel_id channel_id, SoftCSAStreamCallback cb, int timeout_ms);
	bool hasRegisteredSession(t_channel_id channel_id, SoftCSASessionType type);

	/* Returns a nonzero StreamOutput handle when a keyed sibling
	 * absorbs us, 0 when no sibling exists or rollback fired. */
	uint32_t cloneAndStartStream(uint32_t session_id, SoftCSAStreamCallback cb);
	bool cloneAndStartRecord(uint32_t session_id, int fd);

	/* Attach a new LIVE/PIP session to an existing sibling's tap engine
	 * without waiting for onDescrMode. Used when capmt skipped sendCaPmt
	 * because the same-mask sibling already runs: OSCam will not confirm
	 * the new session, but the engine is keyed via the sibling, so we
	 * rebind the decoder to a fresh DVR slot directly. Returns 0 on
	 * success. decoder_index: 0 for main, pip+1 for PiP. */
	int cloneAndStartLive(uint32_t session_id, int decoder_index);
	int cloneAndStartPip(uint32_t session_id, int decoder_index);

	int  startLive(uint32_t session_id, int decoder_index);
	void stopLive(uint32_t session_id, bool skip_decoder_start = false);

	/* Drain all LIVE attachments before zapit's StopPlayBack runs, with
	 * skip_decoder_start so the FRONT-rebind does not briefly start
	 * the decoder during standby entry. */
	void enterStandbyTeardown();

	/* True once the DVR loopback fd for (adapter, demux_unit) has
	 * flushed at least one byte. */
	bool isLiveDataFlowing(int adapter, int demux_unit);

	/* True when the decoder for decoder_index is currently bound through
	 * the dvr-loopback (via a LIVE attachment for index 0 or a PIP
	 * attachment for index >= 1). Callers use this to route operations
	 * on the audio/video demux to softcsa_*Demux instead of the
	 * FRONT-bound cDemux objects, which are paused while the loopback
	 * is active. */
	bool isDecoderDvrBound(int decoder_index);

private:
	CSoftCSAManager();
	~CSoftCSAManager();

	struct SoftCSAServiceTapKey {
		int frontend_num;
		t_channel_id channel_id;
		bool operator<(const SoftCSAServiceTapKey &o) const {
			if (frontend_num != o.frontend_num) return frontend_num < o.frontend_num;
			return channel_id < o.channel_id;
		}
	};

	struct SoftCSAServiceTap {
		int adapter;
		int frontend_num;
		int tap_demux_unit;
		std::unique_ptr<CSoftCSAEngine> engine;
		std::unique_ptr<COutputFdSet> outputs;
		std::unique_ptr<CTapReader> reader;
		int refcount;
		bool reader_started;
	};

	struct SessionState {
		t_channel_id channel_id;
		SoftCSASessionType type;
		SoftCSAServiceTapKey tap_key;
		/* Engine ready to descramble: set by onDescrMode after OSCam's
		 * confirm, or by cloneAndStart* when a sibling already keyed
		 * the shared tap engine. */
		bool csa_alt_active;
		/* Set while a stream/record sibling still depends on this live's
		 * oscam subscription. Distinguishes a parked hw live from an
		 * actively-decoding one. */
		bool retained;
		/* Runtime config denied the channel. capmt is still sent so
		 * oscam's slot bookkeeping stays clean; descrmode is ignored,
		 * cw dropped, engine never starts, sibling lookups skip it. */
		bool passive;
		/* Set while stopSession is tearing the session down. Blocks
		 * onDescrMode auto-attach so a late CW cannot resurrect an
		 * m_live entry that the second crit-section is about to drop. */
		bool stopping;
		uint8_t ecm_mode;
		int capmt_demux;
		uint8_t capmt_ca_mask;
		std::vector<unsigned short> pids;
		unsigned short video_pid;
		unsigned short audio_pid;
		unsigned short pcr_pid;
		int video_type;
		int audio_type;
		int pip_dev;
		/* RECORD only: one entry per overlapping CRecordInstance::Start
		 * (own pipe + consumer + record fd). High concurrency consumes
		 * the user pipe-page budget (pipe-user-pages-soft). */
		struct RecordOutput {
			int output_token;      /* token in tap.outputs */
			int output_fd;         /* pipe write end (owned) */
			int stream_pipe_read;  /* pipe read end (owned) */
			std::thread stream_consumer;
			int record_fd;         /* caller's recording fd (not owned) */
			/* Splitting the stable record_fd identity from the one-shot
			 * wait_acked predicate-fire flag keeps stopSession(fd) able
			 * to exact-match record_fd after the wait has been ack'd. */
			bool wait_acked;
			RecordOutput()
				: output_token(-1), output_fd(-1),
				  stream_pipe_read(-1), record_fd(-1),
				  wait_acked(false) {}
			RecordOutput(RecordOutput &&) = default;
			RecordOutput &operator=(RecordOutput &&) = default;
			RecordOutput(const RecordOutput &) = delete;
			RecordOutput &operator=(const RecordOutput &) = delete;
		};
		std::vector<RecordOutput> record_outputs;
		/* STREAM only: one entry per overlapping CStreamInstance::run
		 * (own pipe + consumer + captured callback). The manager
		 * assigns output_id since STREAM has no natural per-output
		 * handle the way RECORD has its recording fd; wait_acked is
		 * the one-shot predicate flag so a second overlapping wait
		 * cannot steal the first one's predicate-fire. */
		struct StreamOutput {
			int output_token;      /* token in tap.outputs */
			int output_fd;         /* pipe write end (owned) */
			int stream_pipe_read;  /* pipe read end (owned) */
			std::thread stream_consumer;
			SoftCSAStreamCallback stream_callback;
			uint32_t output_id;    /* stable handle returned to caller */
			bool wait_acked;
			StreamOutput()
				: output_token(-1), output_fd(-1),
				  stream_pipe_read(-1), output_id(0),
				  wait_acked(false) {}
			StreamOutput(StreamOutput &&) = default;
			StreamOutput &operator=(StreamOutput &&) = default;
			StreamOutput(const StreamOutput &) = delete;
			StreamOutput &operator=(const StreamOutput &) = delete;
		};
		std::vector<StreamOutput> stream_outputs;
	};

	struct SoftCSALiveAttachment {
		int decoder_index;
		std::unique_ptr<CDvrDemuxSlot> slot;
		int output_token;
		/* Cached so stopLive never has to fall back to sessions[] (which
		 * may already be erased by then). */
		SoftCSAServiceTapKey tap_key;
	};

	std::map<uint32_t, SessionState> sessions;
	std::map<std::pair<t_channel_id, SoftCSASessionType>, uint32_t> channel_to_session;
	std::map<SoftCSAServiceTapKey, std::unique_ptr<SoftCSAServiceTap>> m_taps;
	std::map<uint32_t, std::unique_ptr<SoftCSALiveAttachment>> m_live;
	/* In-progress sentinel set during startLive: closes the window
	 * between the dedup check (first crit-section) and m_live[session_id]
	 * being populated (third crit-section). Without this, two concurrent
	 * calls could both pass dedup, allocate duplicate DVR slots, and
	 * leak an output_token into the tap's OutputFdSet. */
	/* session_id -> decoder_index. Used as in-progress sentinel AND so
	 * the slot-taken check in onDescrMode can see attaches that have
	 * passed dedup but not yet populated m_live. */
	std::map<uint32_t, int> m_live_starting;
	uint32_t next_session_id;
	/* 1-based; 0 is the "invalid handle" sentinel returned by
	 * waitForStreamStart / cloneAndStartStream on failure. */
	uint32_t next_stream_output_id;
	/* Set while enterStandbyTeardown is running. Blocks new LIVE/PIP
	 * attaches via onDescrMode and cloneAndStart* so a CW that arrives
	 * mid-drain cannot reinsert an m_live entry behind the snapshot. */
	bool m_standby_in_progress;

	std::mutex mtx;

	/* Two-mutex pattern: record_cv waits on record_cv_mtx but the
	 * predicate relocks mtx to read state. Wakeups from notify_all
	 * only need to be delivered (record_cv_mtx held), not synchronised
	 * with state mutation under mtx. Do not collapse to a single mutex
	 * without restructuring the wait sites. */
	std::condition_variable record_cv;
	std::mutex record_cv_mtx;

	/* Output-teardown bundle extracted while holding mtx; the actual
	 * close/join/remove runs outside the lock. */
	struct PendingDetach {
		COutputFdSet *outputs;   /* non-owning; tap outlives the detach */
		int output_token;        /* -1 if none */
		int output_fd;           /* write end of owned pipe; -1 if none */
		int stream_pipe_read;    /* -1 if not a stream */
		std::thread stream_consumer;
	};

	void stopAll();

	/* Highest free demux unit not already assigned to any tap. */
	int tapDemuxForFrontend(int frontend_num);

	/* Detach one record_outputs entry for off-lock cleanup. Caller
	 * still owns vector erasure (erase invalidates the reference).
	 * Caller holds mtx. */
	PendingDetach extractRecordOutput(SessionState &ds, size_t index);

	/* Detach one stream_outputs entry for off-lock cleanup. Clears
	 * wait_acked and the callback so the detached entry cannot fire
	 * any more user callbacks. Caller still owns vector erasure.
	 * Caller holds mtx. */
	PendingDetach extractStreamOutput(SessionState &ds, size_t index);

	/* Caller holds mtx. Returns 0 if no qualifying sibling. */
	uint32_t findRunningSibling(t_channel_id channel_id, uint32_t exclude_session_id) const;

	/* Caller holds mtx. Stops the tap reader if no output consumers
	 * remain. Reader.stop() joins the thread (up to ~poll-timeout, ie
	 * ~200ms), so mtx is held during the join; acceptable on the
	 * cleanup paths where this is called (rebind-fail, wait*-timeout,
	 * stopSessions). */
	void stopReaderIfIdleLocked(SoftCSAServiceTap *tap);

};

#endif
