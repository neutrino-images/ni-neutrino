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

CSoftCSAManager *CSoftCSAManager::getInstance()
{
	static CSoftCSAManager instance;
	return &instance;
}

CSoftCSAManager::CSoftCSAManager() : next_session_id(1) {}
CSoftCSAManager::~CSoftCSAManager() { stopAll(); }

int CSoftCSAManager::allocateDemux(int frontend_num)
{
	/* Reuse existing allocation for the same frontend */
	for (auto &alloc : demux_allocs) {
		if (alloc.frontend_num == frontend_num) {
			alloc.refcount++;
			printf("[softcsa] allocateDemux: reuse unit %d for fe%d (refcount %d)\n",
			       alloc.demux_unit, frontend_num, alloc.refcount);
			return alloc.demux_unit;
		}
	}

	/* Find highest free unit — avoids zapit's low units (0, 1) */
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
                                           int adapter, int capmt_demux, int frontend_num)
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
		state.capmt_demux = capmt_demux;
		state.frontend_num = frontend_num;
		state.csa_alt_active = false;
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

		printf("[softcsa] registerSession: id %u for channel %llx type %d (fe %d, capmt_dmx %d, dmx %d)\n",
		       session_id, (unsigned long long)channel_id, type, frontend_num, capmt_demux, state.demux_unit);
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
		/* A retained LIVE entry (session stopped but kept for CW routing
		 * to siblings) has session==NULL AND csa_alt_active==true.  We
		 * must NOT create a new session for it — that would switch the
		 * main decoder to MEMORY and start feeding stale data.  The
		 * !needs_session path below just updates ecm_mode so onCW can
		 * keep routing CWs to RECORD/PIP/STREAM siblings. */
		if (it != sessions.end() && algo == CW_ALGO_CSA_ALT && !it->second.session && !it->second.csa_alt_active) {
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

	int vfd = -1, afd = -1;
	if (is_live) {
		CZapitClient zapit;
		zapit.switchSoftCSASource(true, video_type, audio_type, &vfd, &afd);
		printf("[softcsa] switchSoftCSASource returned video_fd=%d audio_fd=%d\n", vfd, afd);
	} else if (is_pip) {
		CZapitClient zapit;
		zapit.switchSoftCSAPipSource(pip_index, video_type, audio_type, &vfd, &afd);
		printf("[softcsa] switchSoftCSAPipSource returned video_fd=%d audio_fd=%d\n", vfd, afd);
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
		uint32_t retained_live_id = 0;
		int retained_live_capmt_demux = 0;
		bool retained_live_present = false;
		for (auto &pair : sessions) {
			if (pair.first == session_id || pair.second.channel_id != channel_id)
				continue;
			if (pair.second.type == SOFTCSA_SESSION_LIVE && pair.second.session == NULL) {
				retained_live_present = true;
				retained_live_id = pair.first;
				retained_live_capmt_demux = pair.second.capmt_demux;
			} else {
				live_siblings++;
			}
		}

		if (type == SOFTCSA_SESSION_LIVE && live_siblings > 0) {
			/* Retain: OSCam subscription must persist for sibling CW routing.
			 * Do NOT add to dvbapi_stops — no NOT_SELECTED for this session. */
		} else {
			SoftCSAStopNotify notify;
			notify.session_id = session_id;
			notify.capmt_demux = sess_it->second.capmt_demux;
			result.dvbapi_stops.push_back(notify);

			int fe_num = sess_it->second.frontend_num;
			sessions.erase(sess_it);
			channel_to_session.erase(ch_it);
			releaseDemux(fe_num);

			if (type != SOFTCSA_SESSION_LIVE && live_siblings == 0 && retained_live_present) {
				SoftCSAStopNotify live_notify;
				live_notify.session_id = retained_live_id;
				live_notify.capmt_demux = retained_live_capmt_demux;
				result.dvbapi_stops.push_back(live_notify);

				auto retained_it = sessions.find(retained_live_id);
				int retained_fe = (retained_it != sessions.end()) ? retained_it->second.frontend_num : -1;
				sessions.erase(retained_live_id);
				channel_to_session.erase(std::make_pair(channel_id, SOFTCSA_SESSION_LIVE));
				if (retained_fe >= 0)
					releaseDemux(retained_fe);
			}
		}
	}

	if (session_to_stop) {
		session_to_stop->stop();
		delete session_to_stop;
		result.had_running_session = true;
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
		result.push_back(info);
	}
	return result;
}

bool CSoftCSAManager::waitForRecordStart(t_channel_id channel_id, int fd, int timeout_ms)
{
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

		/* Same-channel: LIVE session already has CW keys.
		 * OSCam doesn't create a second filter for the same SID,
		 * so onDescrMode will never fire for the RECORD session.
		 * Create the RECORD session here and copy keys from LIVE. */
		auto live_key = std::make_pair(channel_id, SOFTCSA_SESSION_LIVE);
		auto live_it = channel_to_session.find(live_key);
		if (live_it != channel_to_session.end()) {
			auto live_sess = sessions.find(live_it->second);
			if (live_sess != sessions.end()
			    && live_sess->second.csa_alt_active
			    && live_sess->second.session) {
				printf("[softcsa] waitForRecordStart: same-channel, creating RECORD from LIVE keys\n");

				rec_ds.csa_alt_active = true;
				rec_ds.ecm_mode = live_sess->second.ecm_mode;
				rec_ds.session = new CSoftCSASession(
					rec_ds.type, rec_ds.adapter, rec_ds.demux_unit, rec_ds.frontend_num);

				for (auto pid : rec_ds.pids)
					rec_ds.session->addPid(pid);

				live_sess->second.session->getEngine()->copyKeysTo(rec_ds.session->getEngine());

				if (rec_ds.session->startRecord(fd)) {
					rec_ds.record_fd = -1;
					printf("[softcsa] waitForRecordStart: same-channel recording started\n");
					return true;
				}

				printf("[softcsa] waitForRecordStart: startRecord failed\n");
				delete rec_ds.session;
				rec_ds.session = NULL;
			}
		}

		printf("[softcsa] waitForRecordStart: fd %d stored, waiting %dms\n", fd, timeout_ms);
	}

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

	printf("[softcsa] waitForRecordStart: %s\n", started ? "started" : "timeout");
	return started;
}

bool CSoftCSAManager::waitForStreamStart(t_channel_id channel_id, SoftCSAStreamCallback cb, int timeout_ms)
{
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

		/* Same-channel: LIVE session has CW keys already — clone and start.
		 * OSCam won't issue a second DVBAPI filter for the same SID, so
		 * onDescrMode will never fire for the STREAM session on its own. */
		auto live_key = std::make_pair(channel_id, SOFTCSA_SESSION_LIVE);
		auto live_it = channel_to_session.find(live_key);
		if (live_it != channel_to_session.end()) {
			auto live_sess = sessions.find(live_it->second);
			if (live_sess != sessions.end()
			    && live_sess->second.csa_alt_active
			    && live_sess->second.session) {
				printf("[softcsa] waitForStreamStart: same-channel, creating STREAM from LIVE keys\n");

				str_ds.csa_alt_active = true;
				str_ds.ecm_mode = live_sess->second.ecm_mode;
				str_ds.session = new CSoftCSASession(
					str_ds.type, str_ds.adapter, str_ds.demux_unit, str_ds.frontend_num);

				for (auto pid : str_ds.pids)
					str_ds.session->addPid(pid);

				live_sess->second.session->getEngine()->copyKeysTo(str_ds.session->getEngine());

				if (str_ds.session->startStream(cb)) {
					str_ds.stream_callback = nullptr;
					printf("[softcsa] waitForStreamStart: same-channel streaming started\n");
					return true;
				}

				printf("[softcsa] waitForStreamStart: startStream failed\n");
				delete str_ds.session;
				str_ds.session = NULL;
			}
		}

		printf("[softcsa] waitForStreamStart: callback stored, waiting %dms\n", timeout_ms);
	}

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

	printf("[softcsa] waitForStreamStart: %s\n", started ? "started" : "timeout");
	return started;
}

bool CSoftCSAManager::startPipFromLive(t_channel_id channel_id, int pip_vfd)
{
	std::lock_guard<std::mutex> lock(mtx);

	auto key = std::make_pair(channel_id, SOFTCSA_SESSION_PIP);
	auto ch_it = channel_to_session.find(key);
	if (ch_it == channel_to_session.end()) {
		printf("[softcsa] startPipFromLive: no PIP session for channel %llx\n",
		       (unsigned long long)channel_id);
		return false;
	}
	auto sess_it = sessions.find(ch_it->second);
	if (sess_it == sessions.end())
		return false;

	SessionState &pip_ds = sess_it->second;

	auto live_key = std::make_pair(channel_id, SOFTCSA_SESSION_LIVE);
	auto live_it = channel_to_session.find(live_key);
	if (live_it == channel_to_session.end())
		return false;
	auto live_sess = sessions.find(live_it->second);
	if (live_sess == sessions.end()
	    || !live_sess->second.csa_alt_active
	    || !live_sess->second.session) {
		printf("[softcsa] startPipFromLive: no running LIVE session for channel %llx\n",
		       (unsigned long long)channel_id);
		return false;
	}

	printf("[softcsa] startPipFromLive: cloning LIVE keys into PIP\n");

	pip_ds.csa_alt_active = true;
	pip_ds.ecm_mode = live_sess->second.ecm_mode;
	pip_ds.session = new CSoftCSASession(
		pip_ds.type, pip_ds.adapter, pip_ds.demux_unit, pip_ds.frontend_num);

	for (auto pid : pip_ds.pids)
		pip_ds.session->addPid(pid);

	/* PiP has no audio output — skip audio to avoid the audio ringbuffer
	 * filling up (PiP audio decoder never consumes) which would stall
	 * the reader thread on blocked rb->write() calls. */
	pip_ds.session->setDecoderPids(pip_ds.video_pid, 0, pip_ds.pcr_pid);
	live_sess->second.session->getEngine()->copyKeysTo(pip_ds.session->getEngine());

	if (pip_ds.session->start(pip_vfd, -1)) {
		printf("[softcsa] startPipFromLive: pip started\n");
		return true;
	}

	printf("[softcsa] startPipFromLive: start failed\n");
	delete pip_ds.session;
	pip_ds.session = NULL;
	int fe = pip_ds.frontend_num;
	sessions.erase(sess_it);
	channel_to_session.erase(ch_it);
	releaseDemux(fe);
	return false;
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
