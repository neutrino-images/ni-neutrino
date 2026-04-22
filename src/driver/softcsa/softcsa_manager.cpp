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

#include "softcsa_manager.h"
#include "softcsa_engine.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <dmx_hal.h>
#include <neutrino.h>

CSoftCSAManager *CSoftCSAManager::getInstance()
{
	static CSoftCSAManager instance;
	return &instance;
}

CSoftCSAManager::CSoftCSAManager() : next_session_id(1) {}
CSoftCSAManager::~CSoftCSAManager() { stopAll(); }

int CSoftCSAManager::allocateDemux(int frontend_num)
{
	/* All sessions on one frontend share one unit; the key server
	 * separates them by program_number, not by demux_index. */
	for (auto &alloc : demux_allocs) {
		if (alloc.frontend_num == frontend_num) {
			alloc.refcount++;
			printf("[softcsa] allocateDemux: reuse unit %d for fe%d (refcount %d)\n",
			       alloc.demux_unit, frontend_num, alloc.refcount);
			return alloc.demux_unit;
		}
	}

	/* Highest-free-unit policy avoids the low units that zapit pre-binds
	 * to frontend<N> for hardware record/stream/pip. */
	for (int unit = MAX_DMX_UNITS - 1; unit >= 0; unit--) {
		bool in_use = false;
		for (auto &alloc : demux_allocs) {
			if (alloc.demux_unit == unit) {
				in_use = true;
				break;
			}
		}
		if (!in_use) {
			cDemux::SetSource(unit, frontend_num);
			DemuxAlloc alloc;
			alloc.demux_unit = unit;
			alloc.frontend_num = frontend_num;
			alloc.refcount = 1;
			demux_allocs.push_back(alloc);
			printf("[softcsa] allocateDemux: assigned unit %d for fe%d\n",
			       unit, frontend_num);
			return unit;
		}
	}

	printf("[softcsa] allocateDemux: no free unit for fe%d!\n", frontend_num);
	return -1;
}

void CSoftCSAManager::releaseDemux(int frontend_num)
{
	for (auto it = demux_allocs.begin(); it != demux_allocs.end(); ++it) {
		if (it->frontend_num == frontend_num) {
			it->refcount--;
			printf("[softcsa] releaseDemux: unit %d fe%d refcount %d\n",
			       it->demux_unit, frontend_num, it->refcount);
			if (it->refcount <= 0)
				demux_allocs.erase(it);
			return;
		}
	}
}

uint32_t CSoftCSAManager::registerSession(t_channel_id channel_id, SoftCSASessionType type,
                                           int adapter, int frontend_num,
                                           uint8_t capmt_ca_mask)
{
	CSoftCSASession *old_session = NULL;
	uint32_t session_id;

	{
		std::lock_guard<std::mutex> lock(mtx);

		/* PMT update: reuse existing id, drop old session */
		auto existing_key = std::make_pair(channel_id, type);
		auto ch_it = channel_to_session.find(existing_key);
		if (ch_it != channel_to_session.end()) {
			session_id = ch_it->second;
			auto sess_it = sessions.find(session_id);
			if (sess_it != sessions.end()) {
				if (sess_it->second.session != NULL
				    && sess_it->second.frontend_num == frontend_num
				    && sess_it->second.capmt_ca_mask == capmt_ca_mask) {
					printf("[softcsa] registerSession: reuse id %u for channel %llx type %d (no state change)\n",
					       session_id, (unsigned long long)channel_id, type);
					return session_id;
				}
				old_session = sess_it->second.session;
				sess_it->second.session = NULL;
				releaseDemux(sess_it->second.frontend_num);
			}
		} else {
			session_id = next_session_id++;
		}

		SessionState state;
		state.channel_id = channel_id;
		state.type = type;
		state.adapter = adapter;
		state.demux_unit = allocateDemux(frontend_num);
		state.capmt_demux = (frontend_num >= 0) ? frontend_num : 0;
		state.capmt_ca_mask = capmt_ca_mask;
		state.frontend_num = frontend_num;
		state.csa_alt_active = false;
		state.retained = false;
		state.ecm_mode = 0;
		state.session = NULL;
		state.video_pid = 0;
		state.audio_pid = 0;
		state.pcr_pid = 0;
		state.video_type = 0;
		state.audio_type = 0;
		state.pip_dev = -1;
		state.record_fd = -1;
		state.stream_callback = nullptr;

		sessions[session_id] = state;
		channel_to_session[existing_key] = session_id;

		printf("[softcsa] registerSession: id %u for channel %llx type %d (fe %d, dmx %d)\n",
		       session_id, (unsigned long long)channel_id, type, frontend_num,
		       state.demux_unit);
	}

	if (old_session) {
		old_session->stop();
		delete old_session;
	}

	return session_id;
}

void CSoftCSAManager::addPid(uint32_t session_id, unsigned short pid)
{
	std::lock_guard<std::mutex> lock(mtx);

	auto it = sessions.find(session_id);
	if (it == sessions.end())
		return;

	/* dedup — pathological multiplexes may list the same PID under
	 * multiple roles (audio == video, audio == pcr, two identical
	 * audio channel entries). cDemux has no internal dedup and
	 * would emit DMX_ADD_PID errors + stale pesfds entries. */
	auto &pid_list = it->second.pids;
	if (std::find(pid_list.begin(), pid_list.end(), pid) != pid_list.end())
		return;

	pid_list.push_back(pid);

	if (it->second.session)
		it->second.session->addPid(pid);
}

void CSoftCSAManager::addPidByChannel(t_channel_id channel_id, SoftCSASessionType type, unsigned short pid)
{
	std::lock_guard<std::mutex> lock(mtx);
	auto ch_it = channel_to_session.find(std::make_pair(channel_id, type));
	if (ch_it == channel_to_session.end())
		return;
	auto it = sessions.find(ch_it->second);
	if (it == sessions.end())
		return;

	auto &pid_list = it->second.pids;
	if (std::find(pid_list.begin(), pid_list.end(), pid) != pid_list.end())
		return;
	pid_list.push_back(pid);

	if (it->second.session)
		it->second.session->addPid(pid);
}

void CSoftCSAManager::setDecoderPids(uint32_t session_id, unsigned short vpid, unsigned short apid, unsigned short pcrpid)
{
	std::lock_guard<std::mutex> lock(mtx);
	auto it = sessions.find(session_id);
	if (it == sessions.end())
		return;
	it->second.video_pid = vpid;
	it->second.audio_pid = apid;
	it->second.pcr_pid = pcrpid;
}

void CSoftCSAManager::setDecoderTypes(uint32_t session_id, int video_type, int audio_type)
{
	std::lock_guard<std::mutex> lock(mtx);
	auto it = sessions.find(session_id);
	if (it == sessions.end())
		return;
	it->second.video_type = video_type;
	it->second.audio_type = audio_type;
}

void CSoftCSAManager::setPipDevIndex(uint32_t session_id, int pip_dev)
{
	std::lock_guard<std::mutex> lock(mtx);
	auto it = sessions.find(session_id);
	if (it == sessions.end())
		return;
	it->second.pip_dev = pip_dev;
}

void CSoftCSAManager::onDescrMode(uint32_t session_id, uint32_t algo, uint32_t cipher_mode)
{
	bool needs_session = false;
	bool is_live = false;
	bool is_pip = false;
	t_channel_id expected_channel = 0;

	{
		std::lock_guard<std::mutex> lock(mtx);
		auto it = sessions.find(session_id);
		/* A retained LIVE entry — either the csa-alt cw-routing shape
		 * (session==NULL && csa_alt_active==true) or the hw subscription-
		 * holder shape (session==NULL && retained==true) — must not
		 * create a new session. For csa-alt the fall-through updates
		 * ecm_mode so onCW keeps routing; for hw there is no cw routing
		 * and the retained entry is purely an oscam bookkeeping anchor. */
		if (it != sessions.end() && algo == CW_ALGO_CSA_ALT && !it->second.session
		    && !it->second.csa_alt_active && !it->second.retained) {
			needs_session = true;
			is_live = (it->second.type == SOFTCSA_SESSION_LIVE);
			is_pip = (it->second.type == SOFTCSA_SESSION_PIP);
			expected_channel = it->second.channel_id;
		}
	}

	if (!needs_session) {
		std::lock_guard<std::mutex> lock(mtx);
		auto it = sessions.find(session_id);
		if (it == sessions.end())
			return;
		if (algo == CW_ALGO_CSA_ALT) {
			it->second.csa_alt_active = true;
			it->second.ecm_mode = (uint8_t)cipher_mode;
		}
		return;
	}

	/* Phase 2: Create session + configure cDemux (like recording) */
	bool record_started = false;
	{
		std::lock_guard<std::mutex> lock(mtx);
		auto it = sessions.find(session_id);

		if (it == sessions.end() || it->second.channel_id != expected_channel) {
			return;
		}
		if (it->second.session) {
			printf("[softcsa] onDescrMode: session already exists for id %u\n", session_id);
			return;
		}

		printf("[softcsa] onDescrMode: id %u algo %u cipher_mode %u\n",
		       session_id, algo, cipher_mode);

		SessionState &ds = it->second;
		ds.csa_alt_active = true;
		ds.ecm_mode = (uint8_t)cipher_mode;

		ds.session = new CSoftCSASession(ds.type, ds.adapter, ds.demux_unit, ds.frontend_num);

		printf("[softcsa] createSession: id %u type %d, %zu stored PIDs\n",
		       session_id, ds.type, ds.pids.size());

		/* addPid calls cDemux::pesFilter/addPid which do kernel ioctls.
		 * Holding mtx during ioctls briefly blocks CW delivery — acceptable
		 * because ioctls are fast and this only runs once per session. */
		for (auto pid : ds.pids)
			ds.session->addPid(pid);

		/* PiP has no audio output — set apid=0 so the reader skips
		 * audio PES extraction (avoids ringbuffer stall). */
		ds.session->setDecoderPids(ds.video_pid,
			is_pip ? 0 : ds.audio_pid, ds.pcr_pid);

		if (!is_live && !is_pip) {
			if (ds.type == SOFTCSA_SESSION_RECORD && ds.record_fd >= 0) {
				printf("[softcsa] createSession: auto-starting record for id %u fd %d\n",
				       session_id, ds.record_fd);
				if (ds.session->startRecord(ds.record_fd)) {
					ds.record_fd = -1;
					record_started = true;
				} else {
					printf("[softcsa] createSession: failed to start record\n");
				}
			} else if (ds.type == SOFTCSA_SESSION_STREAM && ds.stream_callback) {
				printf("[softcsa] createSession: auto-starting stream for id %u\n",
				       session_id);
				if (ds.session->startStream(ds.stream_callback)) {
					ds.stream_callback = nullptr;
					record_started = true;
				} else {
					printf("[softcsa] createSession: failed to start stream\n");
				}
			}
		}
	}

	if (!is_live && !is_pip) {
		if (record_started)
			record_cv.notify_all();
		return;
	}

	/* Phase 3: Switch decoder to MEMORY source — mtx NOT held during IPC
	 * to avoid deadlock (IPC targets zapit server thread which may be
	 * waiting on mtx for a concurrent stopSession/registerSession). */
	int video_type = 0, audio_type = 0;
	int pip_index = 0;
	{
		std::lock_guard<std::mutex> lock(mtx);
		auto it = sessions.find(session_id);
		if (it != sessions.end()) {
			video_type = it->second.video_type;
			audio_type = it->second.audio_type;
			if (is_pip)
				pip_index = it->second.pip_dev >= 0 ? it->second.pip_dev : 0;
		}
	}

	/* In standby the main decoder does not consume from memory —
	 * attaching would saturate the reader ring and starve siblings. */
	bool live_standby = is_live
	    && CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_standby;

	int vfd = -1, afd = -1;
	if (is_live && !live_standby) {
		CZapitClient zapit;
		zapit.switchSoftCSASource(true, video_type, audio_type, &vfd, &afd);
		printf("[softcsa] switchSoftCSASource returned video_fd=%d audio_fd=%d\n", vfd, afd);
	} else if (is_pip) {
		CZapitClient zapit;
		zapit.switchSoftCSAPipSource(pip_index, video_type, audio_type, &vfd, &afd);
		printf("[softcsa] switchSoftCSAPipSource returned video_fd=%d audio_fd=%d\n", vfd, afd);
	}

	if (live_standby) {
		/* Hold the session without starting any thread: oscam emits one
		 * filter per sid so stream/record siblings rely on this entry
		 * for key clone and onCW fan-out. */
		{
			std::lock_guard<std::mutex> lock(mtx);
			auto it = sessions.find(session_id);
			if (it == sessions.end() || it->second.channel_id != expected_channel) {
				return;
			}
		}
		printf("[softcsa] onDescrMode: LIVE in standby, holding session for id %u\n", session_id);
		int n = tryCloneSiblingsFromLive(session_id);
		if (n > 0)
			printf("[softcsa] onDescrMode: cloned %d sibling(s) from held LIVE\n", n);
		return;
	}

	/* Phase 4: Start session with decoder fds */
	if (vfd < 0) {
		printf("[softcsa] createSession: decoder switch failed, aborting\n");
		CSoftCSASession *to_delete = NULL;
		{
			std::lock_guard<std::mutex> lock(mtx);
			auto it = sessions.find(session_id);
			if (it != sessions.end() && it->second.session) {
				to_delete = it->second.session;
				it->second.session = NULL;
			}
		}
		if (to_delete) {
			to_delete->stop();
			delete to_delete;
		}
		if (is_live) {
			CZapitClient zapit_restore;
			zapit_restore.switchSoftCSASource(false, 0, 0);
		} else if (is_pip) {
			CZapitClient zapit_restore;
			zapit_restore.restoreSoftCSAPipSource(pip_index);
		}
		return;
	}

	{
		bool started = false;
		{
			std::lock_guard<std::mutex> lock(mtx);
			auto it = sessions.find(session_id);
			if (it != sessions.end() && it->second.session) {
				if (it->second.session->start(vfd, is_pip ? -1 : afd)) {
					started = true;
				} else {
					printf("[softcsa] createSession: start() failed\n");
				}
			}
		}

		if (started && is_live) {
			/* A sibling stream/record may already be parked on the CV
			 * because OSCam does not emit a duplicate filter for the
			 * same SID — clone keys now so they don't time out. */
			int n = tryCloneSiblingsFromLive(session_id);
			if (n > 0)
				printf("[softcsa] onDescrMode: cloned %d sibling(s) from LIVE\n", n);
		}

		if (!started) {
			CSoftCSASession *to_delete = NULL;
			{
				std::lock_guard<std::mutex> lock(mtx);
				auto it = sessions.find(session_id);
				if (it != sessions.end() && it->second.session) {
					to_delete = it->second.session;
					it->second.session = NULL;
				}
			}
			if (to_delete) {
				to_delete->stop();
				delete to_delete;
			}
			if (is_live) {
				CZapitClient zapit;
				zapit.switchSoftCSASource(false, 0, 0);
			} else if (is_pip) {
				CZapitClient zapit;
				zapit.restoreSoftCSAPipSource(pip_index);
			}
		}
	}
}

void CSoftCSAManager::onCW(uint32_t session_id, uint32_t parity, const uint8_t *cw)
{
	std::lock_guard<std::mutex> lock(mtx);

	auto it = sessions.find(session_id);
	if (it == sessions.end())
		return;

	SessionState &ds = it->second;
	if (!ds.csa_alt_active)
		return;

	/* Primary session may be NULL for a retained LIVE (stopped but kept
	 * around so OSCam's msgid still routes CWs to the remaining siblings). */
	if (ds.session)
		ds.session->getEngine()->setKey(parity, ds.ecm_mode, cw);

	t_channel_id cid = ds.channel_id;
	uint8_t ecm_mode = ds.ecm_mode;
	for (auto &pair : sessions) {
		if (pair.first != session_id
		    && pair.second.channel_id == cid
		    && pair.second.csa_alt_active
		    && pair.second.session) {
			pair.second.session->getEngine()->setKey(parity, ecm_mode, cw);
		}
	}
}

SoftCSAStopResult CSoftCSAManager::stopSession(t_channel_id channel_id, SoftCSASessionType type)
{
	SoftCSAStopResult result;
	result.had_running_session = false;

	CSoftCSASession *session_to_stop = NULL;
	std::vector<CSoftCSASession *> retained_sessions_to_stop;

	{
		std::lock_guard<std::mutex> lock(mtx);

		auto key = std::make_pair(channel_id, type);
		auto ch_it = channel_to_session.find(key);
		if (ch_it == channel_to_session.end())
			return result;

		uint32_t session_id = ch_it->second;
		auto sess_it = sessions.find(session_id);
		if (sess_it == sessions.end())
			return result;

		sess_it->second.record_fd = -1;
		sess_it->second.stream_callback = nullptr;
		if (sess_it->second.session) {
			session_to_stop = sess_it->second.session;
			sess_it->second.session = NULL;
		}

		int live_siblings = 0;
		std::vector<uint32_t> retained_ids;
		for (auto &pair : sessions) {
			if (pair.first == session_id || pair.second.channel_id != channel_id)
				continue;

			/* Retained-shape: not driving a writer and carrying cw keys
			 * or flagged retained. Actively-decoding hw sessions have
			 * session==NULL with both flags false and count as siblings. */
			bool not_running = (pair.second.session == NULL
			                    || !pair.second.session->isRunning());
			bool retained_shape = not_running
			                      && (pair.second.csa_alt_active || pair.second.retained);

			if (retained_shape)
				retained_ids.push_back(pair.first);
			else
				live_siblings++;
		}

		if (live_siblings > 0) {
			/* Keep the entry so siblings still have a subscription
			 * anchor; sibling teardown will clear it later. */
			sess_it->second.retained = true;
		} else {
			SoftCSAStopNotify notify;
			notify.session_id = session_id;
			notify.capmt_demux = sess_it->second.capmt_demux;
			notify.capmt_ca_mask = sess_it->second.capmt_ca_mask;
			result.dvbapi_stops.push_back(notify);

			int fe_num = sess_it->second.frontend_num;
			sessions.erase(sess_it);
			channel_to_session.erase(ch_it);
			releaseDemux(fe_num);

			if (!retained_ids.empty()) {
				/* Last active session stopped: tear down orphaned retained siblings. */
				for (uint32_t rid : retained_ids) {
					auto rit = sessions.find(rid);
					if (rit == sessions.end())
						continue;

					SoftCSAStopNotify rnotify;
					rnotify.session_id = rid;
					rnotify.capmt_demux = rit->second.capmt_demux;
					rnotify.capmt_ca_mask = rit->second.capmt_ca_mask;
					result.dvbapi_stops.push_back(rnotify);

					int rfe = rit->second.frontend_num;
					SoftCSASessionType rtype = rit->second.type;
					if (rit->second.session) {
						retained_sessions_to_stop.push_back(rit->second.session);
						rit->second.session = NULL;
					}
					sessions.erase(rit);
					channel_to_session.erase(std::make_pair(channel_id, rtype));
					releaseDemux(rfe);
				}
			}
		}
	}

	if (session_to_stop) {
		session_to_stop->stop();
		delete session_to_stop;
		result.had_running_session = true;
	}
	for (auto *s : retained_sessions_to_stop) {
		/* Idempotent for the held shape: no workers to join, fds are -1. */
		s->stop();
		delete s;
	}
	return result;
}

void CSoftCSAManager::stopAll()
{
	std::vector<CSoftCSASession *> sessions_to_stop;

	{
		std::lock_guard<std::mutex> lock(mtx);
		for (auto &pair : sessions)
		{
			if (pair.second.session)
			{
				sessions_to_stop.push_back(pair.second.session);
				pair.second.session = NULL;
			}
		}
		sessions.clear();
		channel_to_session.clear();
		demux_allocs.clear();
	}

	for (auto *s : sessions_to_stop) {
		s->stop();
		delete s;
	}
}

void CSoftCSAManager::stopSessions()
{
	std::vector<CSoftCSASession *> sessions_to_stop;

	{
		std::lock_guard<std::mutex> lock(mtx);
		for (auto &pair : sessions) {
			if (pair.second.session) {
				sessions_to_stop.push_back(pair.second.session);
				pair.second.session = NULL;
			}
			pair.second.csa_alt_active = false;
			pair.second.retained = false;
		}
		demux_allocs.clear();
	}

	for (auto *s : sessions_to_stop) {
		s->stop();
		delete s;
	}
}

std::vector<CSoftCSAManager::ResubscribeInfo> CSoftCSAManager::getResubscribeInfo()
{
	std::lock_guard<std::mutex> lock(mtx);
	std::vector<ResubscribeInfo> result;
	for (auto &pair : sessions) {
		ResubscribeInfo info;
		info.channel_id = pair.second.channel_id;
		info.session_id = pair.first;
		info.type = pair.second.type;
		info.capmt_demux = pair.second.capmt_demux;
		info.frontend_num = pair.second.frontend_num;
		info.capmt_ca_mask = pair.second.capmt_ca_mask;
		result.push_back(info);
	}
	return result;
}

bool CSoftCSAManager::waitForRecordStart(t_channel_id channel_id, int fd, int timeout_ms)
{
	uint32_t session_id = 0;
	/* Store fd in RECORD session for deferred start by onDescrMode */
	{
		std::lock_guard<std::mutex> lock(mtx);
		auto key = std::make_pair(channel_id, SOFTCSA_SESSION_RECORD);
		auto ch_it = channel_to_session.find(key);
		if (ch_it == channel_to_session.end()) {
			printf("[softcsa] waitForRecordStart: no RECORD session for channel %llx\n",
			       (unsigned long long)channel_id);
			return false;
		}
		auto sess_it = sessions.find(ch_it->second);
		if (sess_it == sessions.end())
			return false;

		SessionState &rec_ds = sess_it->second;
		rec_ds.record_fd = fd;
		session_id = ch_it->second;

		/* Check if onDescrMode already created the session (fast OSCam response) */
		if (rec_ds.session && !rec_ds.session->isRunning()) {
			printf("[softcsa] waitForRecordStart: session exists, starting record immediately\n");
			if (rec_ds.session->startRecord(fd)) {
				rec_ds.record_fd = -1;
				return true;
			}
			/* startRecord failed — drop the orphan session so the CV
			 * predicate below doesn't fire a false positive. */
			delete rec_ds.session;
			rec_ds.session = NULL;
		}
	}

	/* Same-channel sibling-attach. Helper takes mtx internally. */
	if (cloneAndStartRecord(session_id, fd))
		return true;

	printf("[softcsa] waitForRecordStart: fd %d stored, waiting %dms\n", fd, timeout_ms);

	/* Wait for onDescrMode to create session and start recordThread */
	std::unique_lock<std::mutex> cv_lock(record_cv_mtx);
	bool started = record_cv.wait_for(cv_lock, std::chrono::milliseconds(timeout_ms), [&]() {
		std::lock_guard<std::mutex> lock(mtx);
		auto key = std::make_pair(channel_id, SOFTCSA_SESSION_RECORD);
		auto ch_it = channel_to_session.find(key);
		if (ch_it == channel_to_session.end())
			return false;
		auto sess_it = sessions.find(ch_it->second);
		if (sess_it == sessions.end())
			return false;
		return sess_it->second.session != NULL && sess_it->second.session->isRunning();
	});

	if (!started) {
		/* Symmetric with waitForStreamStart: a late onDescrMode would
		 * otherwise auto-start a second writer on the fd the caller has
		 * already handed to cRecord — corrupts the output .ts file. */
		std::lock_guard<std::mutex> lock(mtx);
		auto ch_it = channel_to_session.find(std::make_pair(channel_id, SOFTCSA_SESSION_RECORD));
		if (ch_it != channel_to_session.end()) {
			auto sess_it = sessions.find(ch_it->second);
			if (sess_it != sessions.end())
				sess_it->second.record_fd = -1;
		}
	}

	printf("[softcsa] waitForRecordStart: %s\n", started ? "started" : "timeout");
	return started;
}

bool CSoftCSAManager::waitForStreamStart(t_channel_id channel_id, SoftCSAStreamCallback cb, int timeout_ms)
{
	uint32_t session_id = 0;
	/* Store callback in STREAM session for deferred start by onDescrMode */
	{
		std::lock_guard<std::mutex> lock(mtx);
		auto key = std::make_pair(channel_id, SOFTCSA_SESSION_STREAM);
		auto ch_it = channel_to_session.find(key);
		if (ch_it == channel_to_session.end()) {
			printf("[softcsa] waitForStreamStart: no STREAM session for channel %llx\n",
			       (unsigned long long)channel_id);
			return false;
		}
		auto sess_it = sessions.find(ch_it->second);
		if (sess_it == sessions.end())
			return false;

		SessionState &str_ds = sess_it->second;
		str_ds.stream_callback = cb;
		session_id = ch_it->second;

		/* Fast path: onDescrMode already created the session */
		if (str_ds.session && !str_ds.session->isRunning()) {
			printf("[softcsa] waitForStreamStart: session exists, starting stream immediately\n");
			if (str_ds.session->startStream(cb)) {
				str_ds.stream_callback = nullptr;
				return true;
			}
			/* startStream failed — drop the orphan session so the CV
			 * predicate below doesn't fire a false positive. */
			delete str_ds.session;
			str_ds.session = NULL;
		}
	}

	/* Same-channel sibling-attach. Helper takes mtx internally. */
	if (cloneAndStartStream(session_id, cb))
		return true;

	printf("[softcsa] waitForStreamStart: callback stored, waiting %dms\n", timeout_ms);

	/* Wait for onDescrMode to create session and start streamThread */
	std::unique_lock<std::mutex> cv_lock(record_cv_mtx);
	bool started = record_cv.wait_for(cv_lock, std::chrono::milliseconds(timeout_ms), [&]() {
		std::lock_guard<std::mutex> lock(mtx);
		auto key = std::make_pair(channel_id, SOFTCSA_SESSION_STREAM);
		auto ch_it = channel_to_session.find(key);
		if (ch_it == channel_to_session.end())
			return false;
		auto sess_it = sessions.find(ch_it->second);
		if (sess_it == sessions.end())
			return false;
		return sess_it->second.session != NULL && sess_it->second.session->isRunning();
	});

	if (!started) {
		/* Late onDescrMode after our 3s deadline would otherwise auto-start
		 * a second reader on top of the dmx->Read fallback the caller has
		 * already committed to — two writers on the same HTTP socket. */
		std::lock_guard<std::mutex> lock(mtx);
		auto ch_it = channel_to_session.find(std::make_pair(channel_id, SOFTCSA_SESSION_STREAM));
		if (ch_it != channel_to_session.end()) {
			auto sess_it = sessions.find(ch_it->second);
			if (sess_it != sessions.end())
				sess_it->second.stream_callback = nullptr;
		}
	}

	printf("[softcsa] waitForStreamStart: %s\n", started ? "started" : "timeout");
	return started;
}

bool CSoftCSAManager::hasRegisteredSession(t_channel_id channel_id, SoftCSASessionType type)
{
	std::lock_guard<std::mutex> lock(mtx);
	return channel_to_session.find(std::make_pair(channel_id, type)) != channel_to_session.end();
}

int CSoftCSAManager::tryCloneSiblingsFromLive(uint32_t live_session_id)
{
	int cloned = 0;
	{
		std::lock_guard<std::mutex> lock(mtx);

		auto live_sess = sessions.find(live_session_id);
		if (live_sess == sessions.end())
			return 0;
		if (live_sess->second.type != SOFTCSA_SESSION_LIVE)
			return 0;
		if (!live_sess->second.csa_alt_active)
			return 0;
		/* Only transient during register; normal live entries, including
		 * the standby key holder, carry a session and engine to clone from. */
		if (!live_sess->second.session)
			return 0;

		t_channel_id channel_id = live_sess->second.channel_id;

		for (auto &kv : sessions) {
			SessionState &ds = kv.second;
			if (ds.channel_id != channel_id)
				continue;
			if (ds.type != SOFTCSA_SESSION_STREAM && ds.type != SOFTCSA_SESSION_RECORD)
				continue;
			if (ds.session && ds.session->isRunning())
				continue;

			bool has_pending = (ds.type == SOFTCSA_SESSION_STREAM)
				? (bool)ds.stream_callback
				: (ds.record_fd >= 0);
			if (!has_pending)
				continue;

			/* Drop any half-built orphan so the CV predicate isn't
			 * tripped by a non-running session pointer. */
			if (ds.session) {
				delete ds.session;
				ds.session = NULL;
			}

			printf("[softcsa] tryCloneSiblingsFromLive: cloning to sibling type %d\n",
			       ds.type);

			ds.csa_alt_active = true;
			ds.ecm_mode = live_sess->second.ecm_mode;
			ds.session = new CSoftCSASession(
				ds.type, ds.adapter, ds.demux_unit, ds.frontend_num);

			for (auto pid : ds.pids)
				ds.session->addPid(pid);

			live_sess->second.session->getEngine()->copyKeysTo(ds.session->getEngine());

			bool started = false;
			if (ds.type == SOFTCSA_SESSION_STREAM) {
				if (ds.session->startStream(ds.stream_callback)) {
					ds.stream_callback = nullptr;
					started = true;
				}
			} else {
				if (ds.session->startRecord(ds.record_fd)) {
					ds.record_fd = -1;
					started = true;
				}
			}

			if (started) {
				cloned++;
			} else {
				printf("[softcsa] tryCloneSiblingsFromLive: start failed, dropping orphan\n");
				delete ds.session;
				ds.session = NULL;
			}
		}
	}

	/* Notify outside mtx to match the existing auto-start pattern and
	 * avoid waking a waiter that then re-acquires mtx immediately. */
	if (cloned > 0)
		record_cv.notify_all();

	return cloned;
}

bool CSoftCSAManager::cloneAndStartStream(uint32_t session_id, SoftCSAStreamCallback cb)
{
	bool started = false;
	{
		std::lock_guard<std::mutex> lock(mtx);

		auto sess_it = sessions.find(session_id);
		if (sess_it == sessions.end())
			return false;
		SessionState &ds = sess_it->second;
		if (ds.session && ds.session->isRunning())
			return false;

		uint32_t sib_id = findRunningSibling(ds.channel_id, session_id);
		if (sib_id == 0)
			return false;
		auto sib_it = sessions.find(sib_id);
		if (sib_it == sessions.end())
			return false;

		printf("[softcsa] cloneAndStartStream: session %u cloning from sibling %u\n",
		       session_id, sib_id);

		if (ds.session) {
			delete ds.session;
			ds.session = NULL;
		}

		ds.csa_alt_active = true;
		ds.ecm_mode = sib_it->second.ecm_mode;
		ds.session = new CSoftCSASession(
			ds.type, ds.adapter, ds.demux_unit, ds.frontend_num);

		for (auto pid : ds.pids)
			ds.session->addPid(pid);
		ds.session->setDecoderPids(ds.video_pid, ds.audio_pid, ds.pcr_pid);
		sib_it->second.session->getEngine()->copyKeysTo(ds.session->getEngine());

		if (ds.session->startStream(cb)) {
			ds.stream_callback = nullptr;
			started = true;
		} else {
			/* Entry must survive so waiters/onDescrMode can still find it. */
			printf("[softcsa] cloneAndStartStream: start failed, dropping orphan\n");
			delete ds.session;
			ds.session = NULL;
		}
	}

	if (started)
		record_cv.notify_all();
	return started;
}

bool CSoftCSAManager::cloneAndStartRecord(uint32_t session_id, int fd)
{
	bool started = false;
	{
		std::lock_guard<std::mutex> lock(mtx);

		auto sess_it = sessions.find(session_id);
		if (sess_it == sessions.end())
			return false;
		SessionState &ds = sess_it->second;
		if (ds.session && ds.session->isRunning())
			return false;

		uint32_t sib_id = findRunningSibling(ds.channel_id, session_id);
		if (sib_id == 0)
			return false;
		auto sib_it = sessions.find(sib_id);
		if (sib_it == sessions.end())
			return false;

		printf("[softcsa] cloneAndStartRecord: session %u cloning from sibling %u\n",
		       session_id, sib_id);

		if (ds.session) {
			delete ds.session;
			ds.session = NULL;
		}

		ds.csa_alt_active = true;
		ds.ecm_mode = sib_it->second.ecm_mode;
		ds.session = new CSoftCSASession(
			ds.type, ds.adapter, ds.demux_unit, ds.frontend_num);

		for (auto pid : ds.pids)
			ds.session->addPid(pid);
		ds.session->setDecoderPids(ds.video_pid, ds.audio_pid, ds.pcr_pid);
		sib_it->second.session->getEngine()->copyKeysTo(ds.session->getEngine());

		if (ds.session->startRecord(fd)) {
			ds.record_fd = -1;
			started = true;
		} else {
			printf("[softcsa] cloneAndStartRecord: start failed, dropping orphan\n");
			delete ds.session;
			ds.session = NULL;
		}
	}

	if (started)
		record_cv.notify_all();
	return started;
}

bool CSoftCSAManager::cloneAndStartLive(uint32_t session_id, int vfd, int afd)
{
	bool started = false;
	{
		std::lock_guard<std::mutex> lock(mtx);

		auto sess_it = sessions.find(session_id);
		if (sess_it == sessions.end())
			return false;
		SessionState &ds = sess_it->second;
		if (ds.session && ds.session->isRunning())
			return false;

		uint32_t sib_id = findRunningSibling(ds.channel_id, session_id);
		if (sib_id == 0)
			return false;
		auto sib_it = sessions.find(sib_id);
		if (sib_it == sessions.end())
			return false;

		printf("[softcsa] cloneAndStartLive: session %u cloning from sibling %u\n",
		       session_id, sib_id);

		if (ds.session) {
			delete ds.session;
			ds.session = NULL;
		}

		ds.csa_alt_active = true;
		ds.ecm_mode = sib_it->second.ecm_mode;
		ds.session = new CSoftCSASession(
			ds.type, ds.adapter, ds.demux_unit, ds.frontend_num);

		for (auto pid : ds.pids)
			ds.session->addPid(pid);
		ds.session->setDecoderPids(ds.video_pid, ds.audio_pid, ds.pcr_pid);
		sib_it->second.session->getEngine()->copyKeysTo(ds.session->getEngine());

		if (ds.session->start(vfd, afd)) {
			started = true;
		} else {
			/* Caller handles decoder restoration; leave the entry. */
			printf("[softcsa] cloneAndStartLive: start failed, dropping orphan\n");
			delete ds.session;
			ds.session = NULL;
		}
	}

	if (started)
		record_cv.notify_all();
	return started;
}

bool CSoftCSAManager::cloneAndStartPip(uint32_t session_id, int pip_vfd)
{
	bool started = false;
	{
		std::lock_guard<std::mutex> lock(mtx);

		auto sess_it = sessions.find(session_id);
		if (sess_it == sessions.end())
			return false;
		SessionState &ds = sess_it->second;
		if (ds.session && ds.session->isRunning())
			return false;

		uint32_t sib_id = findRunningSibling(ds.channel_id, session_id);
		if (sib_id == 0)
			return false;
		auto sib_it = sessions.find(sib_id);
		if (sib_it == sessions.end())
			return false;

		printf("[softcsa] cloneAndStartPip: session %u cloning from sibling %u\n",
		       session_id, sib_id);

		if (ds.session) {
			delete ds.session;
			ds.session = NULL;
		}

		ds.csa_alt_active = true;
		ds.ecm_mode = sib_it->second.ecm_mode;
		ds.session = new CSoftCSASession(
			ds.type, ds.adapter, ds.demux_unit, ds.frontend_num);

		for (auto pid : ds.pids)
			ds.session->addPid(pid);
		/* PiP audio zeroed: pip video decoder has no audio output;
		 * feeding audio PES would stall the reader on a full ringbuffer. */
		ds.session->setDecoderPids(ds.video_pid, 0, ds.pcr_pid);
		sib_it->second.session->getEngine()->copyKeysTo(ds.session->getEngine());

		if (ds.session->start(pip_vfd, -1)) {
			started = true;
		} else {
			/* No fallback path: fully erase so the broken entry cannot
			 * leak into stopSession or a subsequent StartPip. */
			printf("[softcsa] cloneAndStartPip: start failed\n");
			delete ds.session;
			ds.session = NULL;
			int fe = ds.frontend_num;
			auto ch_it = channel_to_session.find(
				std::make_pair(ds.channel_id, SOFTCSA_SESSION_PIP));
			sessions.erase(sess_it);
			if (ch_it != channel_to_session.end())
				channel_to_session.erase(ch_it);
			releaseDemux(fe);
		}
	}

	if (started)
		record_cv.notify_all();
	return started;
}

bool CSoftCSAManager::notifyAudioPidChange(t_channel_id channel_id, unsigned short new_apid)
{
	std::lock_guard<std::mutex> lock(mtx);

	auto key = std::make_pair(channel_id, SOFTCSA_SESSION_LIVE);
	auto ch_it = channel_to_session.find(key);
	if (ch_it == channel_to_session.end())
		return false;

	auto sess_it = sessions.find(ch_it->second);
	if (sess_it == sessions.end())
		return false;

	SessionState &ds = sess_it->second;
	ds.audio_pid = new_apid;

	if (ds.session && ds.session->isRunning()) {
		printf("[softcsa] notifyAudioPidChange: session %u channel %llx new_apid=%04x\n",
		       ch_it->second, (unsigned long long)channel_id, new_apid);
		ds.session->setAudioPidRouting(new_apid);
		return true;
	}
	return false;
}

bool CSoftCSAManager::hasRunningLiveSession(t_channel_id channel_id)
{
	std::lock_guard<std::mutex> lock(mtx);

	auto key = std::make_pair(channel_id, SOFTCSA_SESSION_LIVE);
	auto ch_it = channel_to_session.find(key);
	if (ch_it == channel_to_session.end())
		return false;

	auto sess_it = sessions.find(ch_it->second);
	if (sess_it == sessions.end())
		return false;

	return sess_it->second.session != NULL
	    && sess_it->second.session->isRunning();
}

uint32_t CSoftCSAManager::findRunningSibling(t_channel_id channel_id, uint32_t exclude_session_id) const
{
	/* Lower rank wins. All qualifying siblings carry identical CW keys. */
	auto rank = [](SoftCSASessionType t) -> int {
		switch (t) {
			case SOFTCSA_SESSION_LIVE:   return 0;
			case SOFTCSA_SESSION_RECORD: return 1;
			case SOFTCSA_SESSION_PIP:    return 2;
			case SOFTCSA_SESSION_STREAM: return 3;
		}
		return 4;
	};

	uint32_t best_id = 0;
	int best_rank = 5;
	for (const auto &pair : sessions) {
		if (pair.first == exclude_session_id)
			continue;
		if (pair.second.channel_id != channel_id)
			continue;
		/* Match running siblings and standby key-holders: a non-running
		 * csa_alt_active entry still receives onCW and has valid keys. */
		if (pair.second.session == NULL || !pair.second.csa_alt_active)
			continue;
		int r = rank(pair.second.type);
		if (r < best_rank) {
			best_rank = r;
			best_id = pair.first;
		}
	}
	return best_id;
}

uint32_t CSoftCSAManager::getLiveSessionId(t_channel_id channel_id)
{
	std::lock_guard<std::mutex> lock(mtx);

	auto ch_it = channel_to_session.find(std::make_pair(channel_id, SOFTCSA_SESSION_LIVE));
	if (ch_it == channel_to_session.end())
		return 0;
	return ch_it->second;
}

uint32_t CSoftCSAManager::getSessionId(t_channel_id channel_id, SoftCSASessionType type)
{
	std::lock_guard<std::mutex> lock(mtx);
	auto it = channel_to_session.find(std::make_pair(channel_id, type));
	return it == channel_to_session.end() ? 0 : it->second;
}

int CSoftCSAManager::getCapmtDemux(uint32_t session_id)
{
	std::lock_guard<std::mutex> lock(mtx);
	auto it = sessions.find(session_id);
	return it == sessions.end() ? -1 : it->second.capmt_demux;
}

bool CSoftCSAManager::hasRunningSibling(t_channel_id channel_id, uint32_t exclude_session_id)
{
	std::lock_guard<std::mutex> lock(mtx);
	return findRunningSibling(channel_id, exclude_session_id) != 0;
}

bool CSoftCSAManager::hasAnyRunningSession(t_channel_id channel_id)
{
	std::lock_guard<std::mutex> lock(mtx);
	/* next_session_id starts at 1, so 0 excludes nothing. */
	return findRunningSibling(channel_id, 0) != 0;
}

bool CSoftCSAManager::isActive(t_channel_id channel_id)
{
	std::lock_guard<std::mutex> lock(mtx);

	for (int t = SOFTCSA_SESSION_LIVE; t <= SOFTCSA_SESSION_STREAM; t++)
	{
		auto key = std::make_pair(channel_id, (SoftCSASessionType)t);
		auto it = channel_to_session.find(key);
		if (it != channel_to_session.end())
		{
			auto sess = sessions.find(it->second);
			if (sess != sessions.end()
			    && sess->second.csa_alt_active
			    && sess->second.session != NULL)
				return true;
		}
	}
	return false;
}
