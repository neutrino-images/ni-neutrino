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

CSoftCSAManager *CSoftCSAManager::getInstance()
{
	static CSoftCSAManager instance;
	return &instance;
}

CSoftCSAManager::CSoftCSAManager() {}
CSoftCSAManager::~CSoftCSAManager() { stopAll(); }

void CSoftCSAManager::registerDemux(uint32_t demux_index, t_channel_id channel_id,
                                     SoftCSASessionType type, int adapter, int demux_unit, int frontend_num)
{
	CSoftCSASession *old_session = NULL;

	{
		std::lock_guard<std::mutex> lock(mtx);

		/* Clean up any existing state for this demux (e.g. PMT update) */
		auto existing = demux_states.find(demux_index);
		if (existing != demux_states.end())
		{
			old_session = existing->second.session;
			existing->second.session = NULL;
			auto old_key = std::make_pair(existing->second.channel_id, existing->second.type);
			channel_to_demux.erase(old_key);
		}

		DemuxState state;
		state.channel_id = channel_id;
		state.type = type;
		state.adapter = adapter;
		state.demux_unit = demux_unit;
		state.frontend_num = frontend_num;
		state.csa_alt_active = false;
		state.ecm_mode = 0;
		state.session = NULL;
		state.video_pid = 0;
		state.audio_pid = 0;
		state.pcr_pid = 0;
		state.video_type = 0;
		state.audio_type = 0;
		state.record_fd = -1;
		state.stream_callback = nullptr;

		demux_states[demux_index] = state;
		channel_to_demux[std::make_pair(channel_id, type)] = demux_index;

		printf("[softcsa] registerDemux: demux %u for channel %llx type %d\n",
		       demux_index, (unsigned long long)channel_id, type);
	}

	if (old_session) {
		old_session->stop();
		delete old_session;
	}
}

void CSoftCSAManager::addPid(uint32_t demux_index, unsigned short pid)
{
	std::lock_guard<std::mutex> lock(mtx);

	auto it = demux_states.find(demux_index);
	if (it == demux_states.end())
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

void CSoftCSAManager::setDecoderPids(uint32_t demux_index, unsigned short vpid, unsigned short apid, unsigned short pcrpid)
{
	std::lock_guard<std::mutex> lock(mtx);
	auto it = demux_states.find(demux_index);
	if (it == demux_states.end())
		return;
	it->second.video_pid = vpid;
	it->second.audio_pid = apid;
	it->second.pcr_pid = pcrpid;
}

void CSoftCSAManager::setDecoderTypes(uint32_t demux_index, int video_type, int audio_type)
{
	std::lock_guard<std::mutex> lock(mtx);
	auto it = demux_states.find(demux_index);
	if (it == demux_states.end())
		return;
	it->second.video_type = video_type;
	it->second.audio_type = audio_type;
}

void CSoftCSAManager::onDescrMode(uint32_t demux_index, uint32_t algo, uint32_t cipher_mode)
{
	bool needs_session = false;
	bool is_live = false;
	t_channel_id expected_channel = 0;

	{
		std::lock_guard<std::mutex> lock(mtx);
		auto it = demux_states.find(demux_index);
		if (it != demux_states.end() && algo == CW_ALGO_CSA_ALT && !it->second.session) {
			needs_session = true;
			is_live = (it->second.type == SOFTCSA_SESSION_LIVE || it->second.type == SOFTCSA_SESSION_PIP);
			expected_channel = it->second.channel_id;
		}
	}

	if (!needs_session) {
		std::lock_guard<std::mutex> lock(mtx);
		auto it = demux_states.find(demux_index);
		if (it == demux_states.end())
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
		auto it = demux_states.find(demux_index);

		if (it == demux_states.end() || it->second.channel_id != expected_channel) {
			return;
		}
		if (it->second.session) {
			printf("[softcsa] onDescrMode: session already exists for demux %u\n", demux_index);
			return;
		}

		printf("[softcsa] onDescrMode: demux %u algo %u cipher_mode %u\n",
		       demux_index, algo, cipher_mode);

		DemuxState &ds = it->second;
		ds.csa_alt_active = true;
		ds.ecm_mode = (uint8_t)cipher_mode;

		ds.session = new CSoftCSASession(ds.type, ds.adapter, ds.demux_unit, ds.frontend_num);

		printf("[softcsa] createSession: demux %u type %d, %zu stored PIDs\n",
		       demux_index, ds.type, ds.pids.size());

		/* addPid calls cDemux::pesFilter/addPid which do kernel ioctls.
		 * Holding mtx during ioctls briefly blocks CW delivery — acceptable
		 * because ioctls are fast and this only runs once per session. */
		for (auto pid : ds.pids)
			ds.session->addPid(pid);

		ds.session->setDecoderPids(ds.video_pid, ds.audio_pid, ds.pcr_pid);

		if (!is_live) {
			if (ds.type == SOFTCSA_SESSION_RECORD && ds.record_fd >= 0) {
				printf("[softcsa] createSession: auto-starting record for demux %u fd %d\n",
				       demux_index, ds.record_fd);
				if (ds.session->startRecord(ds.record_fd)) {
					ds.record_fd = -1;
					record_started = true;
				} else {
					printf("[softcsa] createSession: failed to start record\n");
				}
			} else if (ds.type == SOFTCSA_SESSION_STREAM && ds.stream_callback) {
				printf("[softcsa] createSession: auto-starting stream for demux %u\n",
				       demux_index);
				if (ds.session->startStream(ds.stream_callback)) {
					ds.stream_callback = nullptr;
					record_started = true;
				} else {
					printf("[softcsa] createSession: failed to start stream\n");
				}
			}
		}
	}

	if (!is_live) {
		if (record_started)
			record_cv.notify_all();
		return;
	}

	/* Phase 3: Switch decoder to MEMORY source */
	int video_type = 0, audio_type = 0;
	{
		std::lock_guard<std::mutex> lock(mtx);
		auto it = demux_states.find(demux_index);
		if (it != demux_states.end()) {
			video_type = it->second.video_type;
			audio_type = it->second.audio_type;
		}
	}

	int vfd = -1, afd = -1;
	if (is_live) {
		CZapitClient zapit;
		zapit.switchSoftCSASource(true, video_type, audio_type, &vfd, &afd);
		printf("[softcsa] switchSoftCSASource returned video_fd=%d audio_fd=%d\n", vfd, afd);
	}

	/* Phase 4: Start session with decoder fds */
	if (is_live && vfd < 0) {
		printf("[softcsa] createSession: video0 open failed, aborting\n");
		CSoftCSASession *to_delete = NULL;
		{
			std::lock_guard<std::mutex> lock(mtx);
			auto it = demux_states.find(demux_index);
			if (it != demux_states.end() && it->second.session) {
				to_delete = it->second.session;
				it->second.session = NULL;
			}
		}
		if (to_delete) {
			to_delete->stop();
			delete to_delete;
		}
		CZapitClient zapit_restore;
		zapit_restore.switchSoftCSASource(false, 0, 0);
		return;
	}
	if (is_live) {

		bool started = false;
		{
			std::lock_guard<std::mutex> lock(mtx);
			auto it = demux_states.find(demux_index);
			if (it != demux_states.end() && it->second.session) {
				if (it->second.session->start(vfd, afd)) {
					started = true;
					for (auto pid : it->second.pending_reader_pids)
						it->second.session->addReaderPid(pid);
					it->second.pending_reader_pids.clear();
				} else {
					printf("[softcsa] createSession: start() failed\n");
				}
			}
		}

		if (!started) {
			CSoftCSASession *to_delete = NULL;
			{
				std::lock_guard<std::mutex> lock(mtx);
				auto it = demux_states.find(demux_index);
				if (it != demux_states.end() && it->second.session) {
					to_delete = it->second.session;
					it->second.session = NULL;
				}
			}
			if (to_delete) {
				to_delete->stop();
				delete to_delete;
			}
			CZapitClient zapit;
			zapit.switchSoftCSASource(false, 0, 0);
		}
	}
}

void CSoftCSAManager::addReaderPid(uint32_t demux_index, unsigned short pid)
{
	std::lock_guard<std::mutex> lock(mtx);

	auto it = demux_states.find(demux_index);
	if (it == demux_states.end())
		return;

	DemuxState &ds = it->second;
	if (ds.session && ds.session->isRunning())
		ds.session->addReaderPid(pid);
	else
		ds.pending_reader_pids.push_back(pid);
}

void CSoftCSAManager::onCW(uint32_t demux_index, uint32_t parity, const uint8_t *cw)
{
	std::lock_guard<std::mutex> lock(mtx);

	auto it = demux_states.find(demux_index);
	if (it == demux_states.end())
		return;

	DemuxState &ds = it->second;
	if (!ds.csa_alt_active || !ds.session)
		return;

	ds.session->getEngine()->setKey(parity, ds.ecm_mode, cw);

	/* CW-sharing: deliver to other sessions on the same channel.
	 * Same-channel RECORD gets CW even if OSCam only sends to LIVE demux. */
	t_channel_id cid = ds.channel_id;
	uint8_t ecm_mode = ds.ecm_mode;
	for (auto &pair : demux_states) {
		if (pair.first != demux_index
		    && pair.second.channel_id == cid
		    && pair.second.csa_alt_active
		    && pair.second.session) {
			pair.second.session->getEngine()->setKey(parity, ecm_mode, cw);
		}
	}
}

bool CSoftCSAManager::stopSession(t_channel_id channel_id, SoftCSASessionType type)
{
	CSoftCSASession *session_to_stop = NULL;

	/* Extract session pointer under lock, then stop/delete outside lock.
	 * session->stop() calls worker.join() which blocks — holding the mutex
	 * during join would deadlock if the reader thread or CW handler
	 * tries to acquire it (addReaderPid, onCW). */
	{
		std::lock_guard<std::mutex> lock(mtx);

		auto key = std::make_pair(channel_id, type);
		auto ch_it = channel_to_demux.find(key);
		if (ch_it == channel_to_demux.end())
			return false;

		uint32_t demux_index = ch_it->second;
		auto dm_it = demux_states.find(demux_index);
		if (dm_it != demux_states.end())
		{
			/* Reset deferred fds/callbacks — caller owns the fd, the
			 * callback's std::function captures are released with the
			 * state on erase() below anyway; nulling is for symmetry. */
			dm_it->second.record_fd = -1;
			dm_it->second.stream_callback = nullptr;
			if (dm_it->second.session) {
				session_to_stop = dm_it->second.session;
				dm_it->second.session = NULL;
			}
			demux_states.erase(dm_it);
			channel_to_demux.erase(ch_it);
		}
	}

	if (session_to_stop) {
		session_to_stop->stop();
		delete session_to_stop;
		/* Decoder restore (close/reopen for MEMORY→DEMUX transition) is
		 * handled by the caller (StopPlayBack) based on our return value.
		 * We must NOT call switchSoftCSASource via IPC here because stopSession
		 * runs on the zapit server thread (via StopPlayBack → ParseCommand),
		 * and CZapitClient IPC would deadlock (zapit can't answer its own request). */
		return true;
	}
	return false;
}

void CSoftCSAManager::stopAll()
{
	std::vector<CSoftCSASession *> sessions_to_stop;

	{
		std::lock_guard<std::mutex> lock(mtx);
		for (auto &pair : demux_states)
		{
			if (pair.second.session)
			{
				sessions_to_stop.push_back(pair.second.session);
				pair.second.session = NULL;
			}
		}
		demux_states.clear();
		channel_to_demux.clear();
	}

	for (auto *s : sessions_to_stop) {
		s->stop();
		delete s;
	}
	/* No decoder restore via IPC — see stopSession comment above. */
}

bool CSoftCSAManager::waitForRecordStart(t_channel_id channel_id, int fd, int timeout_ms)
{
	/* Store fd in RECORD DemuxState for deferred start by onDescrMode */
	{
		std::lock_guard<std::mutex> lock(mtx);
		auto key = std::make_pair(channel_id, SOFTCSA_SESSION_RECORD);
		auto ch_it = channel_to_demux.find(key);
		if (ch_it == channel_to_demux.end()) {
			printf("[softcsa] waitForRecordStart: no RECORD demux for channel %llx\n",
			       (unsigned long long)channel_id);
			return false;
		}
		auto dm_it = demux_states.find(ch_it->second);
		if (dm_it == demux_states.end())
			return false;

		DemuxState &rec_ds = dm_it->second;
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
		 * so onDescrMode will never fire for the RECORD demux.
		 * Create the RECORD session here and copy keys from LIVE. */
		auto live_key = std::make_pair(channel_id, SOFTCSA_SESSION_LIVE);
		auto live_it = channel_to_demux.find(live_key);
		if (live_it != channel_to_demux.end()) {
			auto live_dm = demux_states.find(live_it->second);
			if (live_dm != demux_states.end()
			    && live_dm->second.csa_alt_active
			    && live_dm->second.session) {
				printf("[softcsa] waitForRecordStart: same-channel, creating RECORD from LIVE keys\n");

				rec_ds.csa_alt_active = true;
				rec_ds.ecm_mode = live_dm->second.ecm_mode;
				rec_ds.session = new CSoftCSASession(
					rec_ds.type, rec_ds.adapter, rec_ds.demux_unit, rec_ds.frontend_num);

				for (auto pid : rec_ds.pids)
					rec_ds.session->addPid(pid);

				live_dm->second.session->getEngine()->copyKeysTo(rec_ds.session->getEngine());

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
		auto ch_it = channel_to_demux.find(key);
		if (ch_it == channel_to_demux.end())
			return false;
		auto dm_it = demux_states.find(ch_it->second);
		if (dm_it == demux_states.end())
			return false;
		return dm_it->second.session != NULL && dm_it->second.session->isRunning();
	});

	printf("[softcsa] waitForRecordStart: %s\n", started ? "started" : "timeout");
	return started;
}

bool CSoftCSAManager::waitForStreamStart(t_channel_id channel_id, SoftCSAStreamCallback cb, int timeout_ms)
{
	/* Store callback in STREAM DemuxState for deferred start by onDescrMode */
	{
		std::lock_guard<std::mutex> lock(mtx);
		auto key = std::make_pair(channel_id, SOFTCSA_SESSION_STREAM);
		auto ch_it = channel_to_demux.find(key);
		if (ch_it == channel_to_demux.end()) {
			printf("[softcsa] waitForStreamStart: no STREAM demux for channel %llx\n",
			       (unsigned long long)channel_id);
			return false;
		}
		auto dm_it = demux_states.find(ch_it->second);
		if (dm_it == demux_states.end())
			return false;

		DemuxState &str_ds = dm_it->second;
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
		 * onDescrMode will never fire for the STREAM demux on its own. */
		auto live_key = std::make_pair(channel_id, SOFTCSA_SESSION_LIVE);
		auto live_it = channel_to_demux.find(live_key);
		if (live_it != channel_to_demux.end()) {
			auto live_dm = demux_states.find(live_it->second);
			if (live_dm != demux_states.end()
			    && live_dm->second.csa_alt_active
			    && live_dm->second.session) {
				printf("[softcsa] waitForStreamStart: same-channel, creating STREAM from LIVE keys\n");

				str_ds.csa_alt_active = true;
				str_ds.ecm_mode = live_dm->second.ecm_mode;
				str_ds.session = new CSoftCSASession(
					str_ds.type, str_ds.adapter, str_ds.demux_unit, str_ds.frontend_num);

				for (auto pid : str_ds.pids)
					str_ds.session->addPid(pid);

				live_dm->second.session->getEngine()->copyKeysTo(str_ds.session->getEngine());

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
		auto ch_it = channel_to_demux.find(key);
		if (ch_it == channel_to_demux.end())
			return false;
		auto dm_it = demux_states.find(ch_it->second);
		if (dm_it == demux_states.end())
			return false;
		return dm_it->second.session != NULL && dm_it->second.session->isRunning();
	});

	printf("[softcsa] waitForStreamStart: %s\n", started ? "started" : "timeout");
	return started;
}

bool CSoftCSAManager::notifyAudioPidChange(t_channel_id channel_id, unsigned short new_apid)
{
	std::lock_guard<std::mutex> lock(mtx);

	auto key = std::make_pair(channel_id, SOFTCSA_SESSION_LIVE);
	auto ch_it = channel_to_demux.find(key);
	if (ch_it == channel_to_demux.end())
		return false;

	auto dm_it = demux_states.find(ch_it->second);
	if (dm_it == demux_states.end())
		return false;

	DemuxState &ds = dm_it->second;
	ds.audio_pid = new_apid;

	if (ds.session && ds.session->isRunning()) {
		printf("[softcsa] notifyAudioPidChange: demux %u channel %llx new_apid=%04x\n",
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
	auto ch_it = channel_to_demux.find(key);
	if (ch_it == channel_to_demux.end())
		return false;

	auto dm_it = demux_states.find(ch_it->second);
	if (dm_it == demux_states.end())
		return false;

	return dm_it->second.session != NULL
	    && dm_it->second.session->isRunning();
}

bool CSoftCSAManager::isActive(t_channel_id channel_id)
{
	std::lock_guard<std::mutex> lock(mtx);

	for (int t = SOFTCSA_SESSION_LIVE; t <= SOFTCSA_SESSION_STREAM; t++)
	{
		auto key = std::make_pair(channel_id, (SoftCSASessionType)t);
		auto it = channel_to_demux.find(key);
		if (it != channel_to_demux.end())
		{
			auto dm = demux_states.find(it->second);
			if (dm != demux_states.end() && dm->second.csa_alt_active)
				return true;
		}
	}
	return false;
}
