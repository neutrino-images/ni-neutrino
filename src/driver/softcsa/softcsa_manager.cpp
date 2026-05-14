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
#include "softcsa_config.h"
#include "softcsa_dvr_demux_slot.h"
#include <zapit/zapit.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <dmx_hal.h>
#include <fcntl.h>
#include <set>
#include <thread>
#include <unistd.h>

#ifndef F_SETPIPE_SZ
#define F_SETPIPE_SZ 1031
#endif

/* 1 MB absorbs disk-write latency spikes (FS journal commit, HDD seek)
 * that would otherwise back-pressure tap_reader and let the kernel demux
 * ringbuffer overflow, dropping TS packets at the source and producing
 * discontinuities in the recording. Default 64 KB pipe holds <350 ms of
 * payload on a typical Sky stream. Silent fallback: kernel rejects sizes
 * over /proc/sys/fs/pipe-max-size with EPERM, behavior degrades to the
 * default 64 KB which still works (just less robust). */
static inline void enlargePipeBuffer(int write_fd)
{
	::fcntl(write_fd, F_SETPIPE_SZ, 1 << 20);
}

CSoftCSAManager *CSoftCSAManager::getInstance()
{
	static CSoftCSAManager instance;
	return &instance;
}

CSoftCSAManager::CSoftCSAManager() : next_session_id(1), m_standby_in_progress(false) {}
CSoftCSAManager::~CSoftCSAManager() { stopAll(); }

int CSoftCSAManager::tapDemuxForFrontend(int frontend_num)
{
	/* Tap on the data demux of the session's frontend (= physical
	 * demux<frontend_num> per cDemux::SetSource convention). A fresh
	 * high-unit demux is not on the vendor TS bus and never delivers
	 * TSDEMUX_TAP bytes. */
	printf("[softcsa] tapDemuxForFrontend: unit %d for fe%d\n", frontend_num, frontend_num);
	return frontend_num;
}

CSoftCSAManager::PendingDetach CSoftCSAManager::extractPendingDetach(SessionState &ds)
{
	PendingDetach pd;
	pd.outputs = nullptr;
	auto tap_it = m_taps.find(ds.tap_key);
	if (tap_it != m_taps.end())
		pd.outputs = tap_it->second->outputs.get();
	pd.output_token = ds.output_token;
	pd.output_fd = ds.output_fd;
	pd.stream_pipe_read = ds.stream_pipe_read;
	pd.stream_consumer = std::move(ds.stream_consumer);
	ds.output_token = -1;
	ds.output_fd = -1;
	ds.stream_pipe_read = -1;
	return pd;
}


uint32_t CSoftCSAManager::registerSession(t_channel_id channel_id, SoftCSASessionType type,
                                           int adapter, int frontend_num,
                                           uint8_t capmt_ca_mask, bool passive)
{
	if (passive)
		printf("[softcsa] registerSession: passive for channel %llx type %d\n",
		       (unsigned long long)channel_id, type);

	struct StreamThread {
		int output_fd;
		int stream_pipe_read;
		std::thread stream_consumer;
	};
	std::vector<StreamThread> detach_list;
	std::vector<std::unique_ptr<SoftCSAServiceTap>> dead_taps;
	uint32_t session_id = 0;

	{
		std::lock_guard<std::mutex> lock(mtx);

		auto existing_key = std::make_pair(channel_id, type);
		auto ch_it = channel_to_session.find(existing_key);

		if (ch_it != channel_to_session.end()) {
			session_id = ch_it->second;
			auto sess_it = sessions.find(session_id);
			if (sess_it != sessions.end()) {
				SessionState &existing = sess_it->second;
				bool alive = (existing.output_token > 0)
				             || (existing.csa_alt_active && !existing.passive);
				if (alive && existing.tap_key.frontend_num == frontend_num) {
					/* Same tap: update mask in place. */
					existing.capmt_ca_mask = capmt_ca_mask;
					existing.stopping = false;
					printf("[softcsa] registerSession: reuse id %u for channel %llx type %d\n",
					       session_id, (unsigned long long)channel_id, type);
					return session_id;
				}
				/* Frontend changed: remove output binding, drop tap ref. */
				auto old_tap_it = m_taps.find(existing.tap_key);
				COutputFdSet *old_outputs = (old_tap_it != m_taps.end())
				    ? old_tap_it->second->outputs.get() : nullptr;
				if (old_outputs && existing.output_token > 0)
					old_outputs->remove(existing.output_token);
				existing.output_token = -1;
				if (existing.stream_pipe_read >= 0) {
					/* Defer pipe close + thread join until after lock drop. */
					StreamThread st;
					st.output_fd = existing.output_fd;
					st.stream_pipe_read = existing.stream_pipe_read;
					st.stream_consumer = std::move(existing.stream_consumer);
					detach_list.push_back(std::move(st));
					existing.output_fd = -1;
					existing.stream_pipe_read = -1;
				} else if (existing.output_fd >= 0) {
					::close(existing.output_fd);
					existing.output_fd = -1;
				}
				if (old_tap_it != m_taps.end()) {
					old_tap_it->second->refcount--;
					if (old_tap_it->second->refcount <= 0) {
						dead_taps.push_back(std::move(old_tap_it->second));
						m_taps.erase(old_tap_it);
					}
				}
				sessions.erase(sess_it);
				/* channel_to_session entry gets overwritten below. */
			}
		} else {
			session_id = next_session_id++;
		}

		/* Find-or-create ServiceTap for (frontend_num, channel_id). */
		SoftCSAServiceTapKey tap_key;
		tap_key.frontend_num = frontend_num;
		tap_key.channel_id = channel_id;

		auto tap_it = m_taps.find(tap_key);
		if (tap_it == m_taps.end()) {
			int unit = tapDemuxForFrontend(frontend_num);
			std::unique_ptr<SoftCSAServiceTap> tap(new SoftCSAServiceTap());
			tap->adapter = adapter;
			tap->frontend_num = frontend_num;
			tap->tap_demux_unit = unit;
			tap->engine = std::unique_ptr<CSoftCSAEngine>(new CSoftCSAEngine());
			tap->outputs = std::unique_ptr<COutputFdSet>(new COutputFdSet());
			tap->reader = std::unique_ptr<CTapReader>(new CTapReader(
				adapter, unit, frontend_num,
				tap->engine.get(), tap->outputs.get()));
			tap->refcount = 0;
			tap->reader_started = false;
			m_taps[tap_key] = std::move(tap);
		}
		m_taps[tap_key]->refcount++;

		SessionState state;
		state.channel_id = channel_id;
		state.type = type;
		state.tap_key = tap_key;
		state.csa_alt_active = false;
		state.retained = false;
		state.passive = passive;
		state.stopping = false;
		state.ecm_mode = 0;
		state.capmt_demux = (frontend_num >= 0) ? frontend_num : 0;
		state.capmt_ca_mask = capmt_ca_mask;
		state.video_pid = 0;
		state.audio_pid = 0;
		state.pcr_pid = 0;
		state.video_type = 0;
		state.audio_type = 0;
		state.pip_dev = -1;
		state.output_token = -1;
		state.output_fd = -1;
		state.record_fd = -1;
		state.stream_callback = nullptr;
		state.stream_pipe_read = -1;

		sessions[session_id] = std::move(state);
		channel_to_session[existing_key] = session_id;

		printf("[softcsa] registerSession: id %u channel %llx type %d fe %d dmx %d\n",
		       session_id, (unsigned long long)channel_id, type, frontend_num,
		       m_taps[tap_key]->tap_demux_unit);

	}

	/* Close pipe write ends, join consumers outside the lock. */
	for (auto &st : detach_list) {
		if (st.output_fd >= 0)
			::close(st.output_fd);
		if (st.stream_consumer.joinable())
			st.stream_consumer.join();
		if (st.stream_pipe_read >= 0)
			::close(st.stream_pipe_read);
	}
	dead_taps.clear();

	return session_id;
}

void CSoftCSAManager::addPid(uint32_t session_id, unsigned short pid)
{
	std::lock_guard<std::mutex> lock(mtx);

	auto it = sessions.find(session_id);
	if (it == sessions.end())
		return;

	/* Dedup: some multiplexes list the same PID under multiple roles. */
	auto &pid_list = it->second.pids;
	if (std::find(pid_list.begin(), pid_list.end(), pid) != pid_list.end())
		return;
	pid_list.push_back(pid);

	auto tap_it = m_taps.find(it->second.tap_key);
	if (tap_it != m_taps.end())
		tap_it->second->reader->addPid(pid);
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

	auto tap_it = m_taps.find(it->second.tap_key);
	if (tap_it != m_taps.end())
		tap_it->second->reader->addPid(pid);
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
	bool notify = false;
	bool start_attach = false;
	int decoder_index = 0;
	{
		std::lock_guard<std::mutex> lock(mtx);
		auto it = sessions.find(session_id);
		if (it == sessions.end())
			return;
		SessionState &ds = it->second;
		if (ds.passive) {
			printf("[softcsa] onDescrMode: ignored for passive session %u\n", session_id);
			return;
		}
		if (ds.stopping) {
			printf("[softcsa] onDescrMode: ignored for stopping session %u\n", session_id);
			return;
		}
		if (m_standby_in_progress) {
			printf("[softcsa] onDescrMode: ignored during standby teardown (session %u)\n", session_id);
			return;
		}
		if (algo != CW_ALGO_CSA_ALT)
			return;

		printf("[softcsa] onDescrMode: id %u algo %u cipher_mode %u\n",
		       session_id, algo, cipher_mode);

		ds.csa_alt_active = true;
		ds.ecm_mode = (uint8_t)cipher_mode;

		bool is_live = (ds.type == SOFTCSA_SESSION_LIVE);
		bool is_pip  = (ds.type == SOFTCSA_SESSION_PIP);
		if (is_live || is_pip) {
			/* m_live keyed by session_id dedups across CSA-ALT transitions. */
			if (m_live.find(session_id) == m_live.end()) {
				int target_index = is_pip ? (ds.pip_dev + 1) : 0;
				/* OSCam keeps a subscription alive as long as any
				 * sibling holds it (e.g. PIP on the same channel after
				 * the LIVE has zapped away). Late CWs for the now-stale
				 * LIVE/PIP session would otherwise re-trigger startLive
				 * and clobber the decoder slot owned by the new live
				 * attachment. Skip if another m_live entry already
				 * holds the target decoder slot. */
				bool slot_taken = false;
				for (auto &lp : m_live) {
					if (lp.second && lp.second->decoder_index == target_index) {
						slot_taken = true;
						break;
					}
				}
				/* Also check m_live_starting: another startLive that
				 * has passed dedup but not yet inserted into m_live
				 * already owns this decoder_index. */
				if (!slot_taken) {
					for (auto &kv : m_live_starting) {
						if (kv.first != session_id && kv.second == target_index) {
							slot_taken = true;
							break;
						}
					}
				}
				if (slot_taken) {
					printf("[softcsa] onDescrMode: ignored late CW for session %u, decoder slot %d already taken\n",
					       session_id, target_index);
				} else {
					start_attach = true;
					decoder_index = target_index;
				}
			}

			/* Same-mask sibling propagation: a RECORD/STREAM session
			 * registered with (oldmask == newmask) does not get its own
			 * onDescrMode from OSCam. Mark any keyed sibling active so
			 * its waitForXStart predicate succeeds. csa_alt_active needs
			 * the manager mutex (held here); record_cv.notify_all is
			 * triggered after the lock drops via the existing notify
			 * flag. */
			t_channel_id ch = ds.channel_id;
			for (auto &kv : sessions) {
				if (kv.first == session_id) continue;
				if (kv.second.channel_id != ch) continue;
				if (kv.second.passive) continue;
				if (kv.second.stopping) continue;
				if (kv.second.type != SOFTCSA_SESSION_RECORD &&
				    kv.second.type != SOFTCSA_SESSION_STREAM) continue;
				if (kv.second.output_token <= 0) continue;
				if (kv.second.csa_alt_active) continue;
				kv.second.csa_alt_active = true;
				kv.second.ecm_mode = (uint8_t)cipher_mode;
				notify = true;
				printf("[softcsa] onDescrMode: same-mask sibling %u (%s) adopted from session %u\n",
				       kv.first,
				       (kv.second.type == SOFTCSA_SESSION_RECORD) ? "RECORD" : "STREAM",
				       session_id);
			}
		} else {
			/* RECORD/STREAM: pipe bridge survives OSCam disconnect.
			 * If stopSessions detached the output_token, re-attach
			 * now so the recording/stream resumes transparently. The
			 * fresh waitForXStart path leaves output_fd cached on the
			 * SessionState, so it hits this branch too. */
			if (ds.output_token <= 0 && ds.output_fd >= 0) {
				auto tap_it = m_taps.find(ds.tap_key);
				if (tap_it != m_taps.end() && tap_it->second) {
					COutputFdSet::FdRole role = (ds.type == SOFTCSA_SESSION_RECORD)
						? COutputFdSet::ROLE_RECORD
						: COutputFdSet::ROLE_STREAM;
					int token = tap_it->second->outputs->add(ds.output_fd, role);
					if (token > 0) {
						ds.output_token = token;
						printf("[softcsa] onDescrMode: re-attached %s session %u output_fd=%d as token=%d\n",
						       (ds.type == SOFTCSA_SESSION_RECORD) ? "RECORD" : "STREAM",
						       session_id, ds.output_fd, token);
					} else {
						printf("[softcsa] onDescrMode: re-attach for session %u failed (outputs->add)\n",
						       session_id);
					}
				}
			}
			/* Wake any waitForXStart waiters; predicate rechecks
			 * (csa_alt_active && output_token > 0). */
			notify = true;
		}
	}

	if (start_attach) {
		int rc = startLive(session_id, decoder_index);
		if (rc != 0)
			printf("[softcsa] onDescrMode: startLive(decoder_index=%d) failed rc=%d for session %u\n",
			       decoder_index, rc, session_id);
	}

	if (notify)
		record_cv.notify_all();
}

void CSoftCSAManager::onCW(uint32_t session_id, uint32_t parity, const uint8_t *cw)
{
	std::lock_guard<std::mutex> lock(mtx);

	auto it = sessions.find(session_id);
	if (it == sessions.end())
		return;

	SessionState &ds = it->second;
	if (ds.passive)
		return;
	if (!ds.csa_alt_active)
		return;

	auto tap_it = m_taps.find(ds.tap_key);
	if (tap_it != m_taps.end())
		tap_it->second->engine->setKey(parity, ds.ecm_mode, cw);
	/* All sessions on the same tap share one engine. */
}

SoftCSAStopResult CSoftCSAManager::stopSession(t_channel_id channel_id, SoftCSASessionType type)
{
	SoftCSAStopResult result;

	/* LIVE/PIP sessions: reverse the decoder rebind first so zapit
	 * reconstructs demuxes back to the main frontend before the tap
	 * and DVR slot tear down. Without this for PIP, m_live[pip_id]
	 * leaks across StopPip and the slot-taken check in onDescrMode
	 * locks out the next PiP attach. softcsaRebind* acquires
	 * zapit_mutex, so no manager mutex may be held here. */
	if (type == SOFTCSA_SESSION_LIVE || type == SOFTCSA_SESSION_PIP) {
		uint32_t attached_id = 0;
		{
			std::lock_guard<std::mutex> lock(mtx);
			auto ch_it = channel_to_session.find(std::make_pair(channel_id, type));
			if (ch_it != channel_to_session.end()) {
				attached_id = ch_it->second;
				/* Mark stopping before dropping the lock so a late
				 * onDescrMode hitting the stopLive race window cannot
				 * resurrect m_live[attached_id] as a zombie. */
				auto sess_it = sessions.find(attached_id);
				if (sess_it != sessions.end())
					sess_it->second.stopping = true;
			}
		}
		if (attached_id != 0)
			stopLive(attached_id);
	}

	struct StreamThread {
		int output_fd;
		int stream_pipe_read;
		std::thread stream_consumer;
	};
	std::vector<StreamThread> threads;
	std::vector<std::unique_ptr<SoftCSAServiceTap>> dead_taps;

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

		SessionState &ds = sess_it->second;

		/* Count non-passive siblings with an active output binding. */
		int live_siblings = 0;
		std::vector<uint32_t> retained_ids;
		for (auto &pair : sessions) {
			if (pair.first == session_id || pair.second.channel_id != channel_id)
				continue;
			if (pair.second.passive)
				continue;

			/* LIVE/PIP hold their token on SoftCSALiveAttachment in
			 * m_live, not on SessionState. m_live presence == engine-keyed,
			 * mirroring findRunningSibling. */
			bool not_running = (pair.second.output_token <= 0)
			                   && (m_live.find(pair.first) == m_live.end());
			bool retained_shape = not_running
			                      && (pair.second.csa_alt_active || pair.second.retained);

			if (retained_shape)
				retained_ids.push_back(pair.first);
			else
				live_siblings++;
		}

		auto detach_session = [&](SessionState &s) {
			auto tap_it = m_taps.find(s.tap_key);
			if (tap_it != m_taps.end() && s.output_token > 0)
				tap_it->second->outputs->remove(s.output_token);
			s.output_token = -1;
			if (s.stream_pipe_read >= 0) {
				StreamThread st;
				st.output_fd = s.output_fd;
				st.stream_pipe_read = s.stream_pipe_read;
				st.stream_consumer = std::move(s.stream_consumer);
				threads.push_back(std::move(st));
				s.output_fd = -1;
				s.stream_pipe_read = -1;
			} else if (s.output_fd >= 0) {
				::close(s.output_fd);
				s.output_fd = -1;
			}
			s.record_fd = -1;
			s.stream_callback = nullptr;
		};

		if (live_siblings > 0) {
			/* Siblings still depend on this tap; keep entry as the
			 * subscription anchor. */
			detach_session(ds);
			ds.retained = true;
		} else {
			SoftCSAStopNotify notify;
			notify.session_id = session_id;
			notify.capmt_demux = ds.capmt_demux;
			notify.capmt_ca_mask = ds.capmt_ca_mask;
			result.dvbapi_stops.push_back(notify);

			detach_session(ds);

			auto tap_it = m_taps.find(ds.tap_key);
			if (tap_it != m_taps.end())
				tap_it->second->refcount--;

			sessions.erase(sess_it);
			channel_to_session.erase(ch_it);

			for (uint32_t rid : retained_ids) {
				auto rit = sessions.find(rid);
				if (rit == sessions.end())
					continue;

				SoftCSAStopNotify rnotify;
				rnotify.session_id = rid;
				rnotify.capmt_demux = rit->second.capmt_demux;
				rnotify.capmt_ca_mask = rit->second.capmt_ca_mask;
				result.dvbapi_stops.push_back(rnotify);

				detach_session(rit->second);

				SoftCSASessionType rtype = rit->second.type;
				auto rtap_it = m_taps.find(rit->second.tap_key);
				if (rtap_it != m_taps.end())
					rtap_it->second->refcount--;
				sessions.erase(rit);
				channel_to_session.erase(std::make_pair(channel_id, rtype));
			}

			/* Move zero-refcount taps out of the map; destroy outside lock. */
			for (auto it2 = m_taps.begin(); it2 != m_taps.end(); ) {
				if (it2->second->refcount <= 0) {
					dead_taps.push_back(std::move(it2->second));
					it2 = m_taps.erase(it2);
				} else {
					++it2;
				}
			}
		}

	}

	/* Outside the lock: close pipe write ends, join consumers, drop taps. */
	for (auto &st : threads) {
		if (st.output_fd >= 0)
			::close(st.output_fd);
		if (st.stream_consumer.joinable())
			st.stream_consumer.join();
		if (st.stream_pipe_read >= 0)
			::close(st.stream_pipe_read);
	}
	dead_taps.clear();

	return result;
}

void CSoftCSAManager::stopAll()
{
	/* Rebind each live decoder back to its main demux before tearing
	 * down the OutputFdSet entries and dvr fds: otherwise the decoder
	 * keeps pointing at a dvr fd that closes a few lines later. */
	std::vector<int> live_decoders;
	{
		std::lock_guard<std::mutex> lock(mtx);
		for (auto &lp : m_live)
			if (lp.second)
				live_decoders.push_back(lp.second->decoder_index);
	}
	for (int di : live_decoders)
		CZapit::getInstance()->softcsaRebindDecoderToFrontDemux(di);
	{
		std::lock_guard<std::mutex> lock(mtx);
		/* Detach LIVE/PIP output tokens BEFORE clearing m_live.
		 * m_live.clear() destructs CDvrDemuxSlot which closes the
		 * dvr_fd; if a token still referenced that fd, the reader
		 * thread could write to a kernel-recycled fd in the window
		 * before the tap is destroyed. */
		for (auto &lp : m_live) {
			if (!lp.second) continue;
			auto sit = sessions.find(lp.first);
			if (sit == sessions.end()) continue;
			auto tap_it = m_taps.find(sit->second.tap_key);
			if (tap_it != m_taps.end() && tap_it->second && lp.second->output_token > 0)
				tap_it->second->outputs->remove(lp.second->output_token);
		}
		m_live.clear();
	}

	/* Collect fds, threads, and taps for close/join/destroy outside the lock. */
	struct ThreadEntry {
		int output_fd;
		int stream_pipe_read;
		std::thread stream_consumer;
	};
	std::vector<ThreadEntry> threads;
	std::vector<std::unique_ptr<SoftCSAServiceTap>> dead_taps;

	{
		std::lock_guard<std::mutex> lock(mtx);

		/* Remove output tokens and collect owned fds/threads. Must run
		 * before tap ownership transfers so outputs are still alive. */
		for (auto &pair : sessions) {
			SessionState &ds = pair.second;
			if (ds.output_token > 0) {
				auto tap_it = m_taps.find(ds.tap_key);
				if (tap_it != m_taps.end())
					tap_it->second->outputs->remove(ds.output_token);
				ds.output_token = -1;
			}
			if (ds.stream_pipe_read >= 0) {
				ThreadEntry te;
				te.output_fd = ds.output_fd;
				te.stream_pipe_read = ds.stream_pipe_read;
				te.stream_consumer = std::move(ds.stream_consumer);
				threads.push_back(std::move(te));
				ds.output_fd = -1;
				ds.stream_pipe_read = -1;
			} else if (ds.output_fd >= 0) {
				::close(ds.output_fd);
				ds.output_fd = -1;
			}
		}

		sessions.clear();
		channel_to_session.clear();

		/* Transfer tap ownership out of the map so destructors (which
		 * stop and join the reader thread) run outside the lock. */
		for (auto &kv : m_taps)
			dead_taps.push_back(std::move(kv.second));
		m_taps.clear();
	}

	/* Close pipe write ends (signals EOF), join consumers, close read ends. */
	for (auto &te : threads) {
		if (te.output_fd >= 0)
			::close(te.output_fd);
		if (te.stream_consumer.joinable())
			te.stream_consumer.join();
		if (te.stream_pipe_read >= 0)
			::close(te.stream_pipe_read);
	}
	dead_taps.clear();
}

void CSoftCSAManager::stopSessions()
{
	printf("[softcsa] stopSessions: OSCam disconnect, draining state\n");
	/* After an OSCam reconnect a new session re-registers; otherwise
	 * the m_live entry from the old session_id stays orphaned with
	 * encrypted TS flowing while the engine has no key. Rebind decoder
	 * back to main demux first, then drop output tokens and m_live. */
	std::vector<int> live_decoders;
	{
		std::lock_guard<std::mutex> lock(mtx);
		for (auto &lp : m_live)
			if (lp.second)
				live_decoders.push_back(lp.second->decoder_index);
	}
	for (int di : live_decoders)
		CZapit::getInstance()->softcsaRebindDecoderToFrontDemux(di);
	{
		std::lock_guard<std::mutex> lock(mtx);
		/* Drop output tokens before clearing m_live. The tap itself
		 * survives the reconnect; only the live attachment is torn
		 * down here. */
		for (auto &lp : m_live) {
			if (!lp.second) continue;
			auto sit = sessions.find(lp.first);
			if (sit == sessions.end()) continue;
			auto tap_it = m_taps.find(sit->second.tap_key);
			if (tap_it != m_taps.end() && tap_it->second && lp.second->output_token > 0)
				tap_it->second->outputs->remove(lp.second->output_token);
		}
		m_live.clear();
	}

	{
		std::lock_guard<std::mutex> lock(mtx);

		for (auto &pair : sessions) {
			SessionState &ds = pair.second;
			/* Detach from the tap broadcast: descrambling pauses while
			 * OSCam is gone. Pipe bridge, record_fd, and stream_callback
			 * are deliberately preserved so the onDescrMode resume path
			 * can re-add output_fd without restarting the consumer or
			 * asking record.cpp / streamts.cpp to re-register. */
			if (ds.output_token > 0) {
				auto tap_it = m_taps.find(ds.tap_key);
				if (tap_it != m_taps.end())
					tap_it->second->outputs->remove(ds.output_token);
				ds.output_token = -1;
			}
			ds.csa_alt_active = false;
			ds.retained = false;
		}
		/* All output tokens dropped: stop tap readers that no longer
		 * have a consumer. They restart on onDescrMode resume; restart
		 * cost (open + filter setup) is negligible against the OSCam
		 * reconnect handshake. */
		for (auto &kv : m_taps) {
			if (kv.second)
				stopReaderIfIdleLocked(kv.second.get());
		}
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
		info.frontend_num = pair.second.tap_key.frontend_num;
		info.capmt_ca_mask = pair.second.capmt_ca_mask;
		result.push_back(info);
	}
	return result;
}

bool CSoftCSAManager::waitForRecordStart(t_channel_id channel_id, int fd, int timeout_ms)
{
	uint32_t session_id = 0;
	bool already_active = false;

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

		if (sess_it->second.passive) {
			printf("[softcsa] waitForRecordStart: passive, skipping for channel %llx\n",
			       (unsigned long long)channel_id);
			return false;
		}

		SessionState &ds = sess_it->second;
		session_id = ch_it->second;
		ds.record_fd = fd;
		already_active = ds.csa_alt_active;
	}

	/* Sibling-attach: if a sibling on the same tap already runs, the
	 * engine has keys and we can attach directly. */
	if (cloneAndStartRecord(session_id, fd))
		return true;

	/* Pipe bridge: O_NONBLOCK write end into COutputFdSet (tap reader
	 * never blocks), blocking read end drained by a consumer thread that
	 * forwards to the caller's recording fd. The caller's fd keeps its
	 * original blocking semantics. */
	{
		/* unique_lock so mtx can be dropped before joining the consumer
		 * thread on the reader-start-failure rollback path. */
		std::unique_lock<std::mutex> lock(mtx);
		auto it = sessions.find(session_id);
		if (it == sessions.end())
			return false;
		SessionState &ds = it->second;

		if (ds.output_token <= 0) {
			int p[2];
			if (::pipe(p) < 0) {
				printf("[softcsa] waitForRecordStart: pipe failed\n");
				return false;
			}
			enlargePipeBuffer(p[1]);
			int wflags = ::fcntl(p[1], F_GETFL, 0);
			if (wflags >= 0)
				::fcntl(p[1], F_SETFL, wflags | O_NONBLOCK);
			int rflags = ::fcntl(p[0], F_GETFL, 0);
			if (rflags >= 0)
				::fcntl(p[0], F_SETFL, rflags & ~O_NONBLOCK);

			auto tap_it = m_taps.find(ds.tap_key);
			if (tap_it == m_taps.end()) {
				::close(p[0]);
				::close(p[1]);
				return false;
			}
			SoftCSAServiceTap &tap = *tap_it->second;
			int token = tap.outputs->add(p[1], COutputFdSet::ROLE_RECORD);
			if (token < 0) {
				::close(p[0]);
				::close(p[1]);
				printf("[softcsa] waitForRecordStart: outputs->add failed\n");
				return false;
			}
			ds.output_token = token;
			ds.output_fd = p[1];
			ds.stream_pipe_read = p[0];

			int read_end = p[0];
			int record_fd_cap = fd;
			ds.stream_consumer = std::thread([read_end, record_fd_cap]() {
				const int kBuf = 1024 * 1024;
				std::vector<uint8_t> buf(kBuf);
				/* Drive writeback in 4 MB chunks so the page cache
				 * never grows enough to trigger a global writeback
				 * burst at dirty_background_ratio that would stall
				 * this thread and overflow the pipe. */
				off_t total_written = 0;
				off_t last_advised = 0;
				const off_t kAdviseInterval = 4 * 1024 * 1024;
				while (true) {
					ssize_t n = ::read(read_end, buf.data(), kBuf);
					if (n <= 0) break;
					const uint8_t *ptr = buf.data();
					ssize_t rem = n;
					while (rem > 0) {
						ssize_t w = ::write(record_fd_cap, ptr, (size_t)rem);
						if (w < 0) {
							if (errno == EINTR) continue;
							return;
						}
						ptr += w;
						rem -= w;
					}
					total_written += n;
					if (total_written - last_advised >= kAdviseInterval) {
						::sync_file_range(record_fd_cap, last_advised,
						                  total_written - last_advised,
						                  SYNC_FILE_RANGE_WRITE);
						if (last_advised >= kAdviseInterval) {
							::posix_fadvise(record_fd_cap,
							                last_advised - kAdviseInterval,
							                kAdviseInterval,
							                POSIX_FADV_DONTNEED);
						}
						last_advised = total_written;
					}
				}
			});

			if (!tap.reader_started) {
				if (tap.reader->start() == 0) {
					tap.reader_started = true;
				} else {
					/* Reader failed to start. Roll back the output
					 * registration and pipe under the lock, then join
					 * the consumer thread outside. */
					printf("[softcsa] waitForRecordStart: reader->start failed, rolling back\n");
					PendingDetach pd = extractPendingDetach(ds);
					ds.record_fd = -1;
					if (pd.outputs && pd.output_token > 0)
						pd.outputs->remove(pd.output_token);
					lock.unlock();
					if (pd.output_fd >= 0)
						::close(pd.output_fd);
					if (pd.stream_consumer.joinable())
						pd.stream_consumer.join();
					if (pd.stream_pipe_read >= 0)
						::close(pd.stream_pipe_read);
					return false;
				}
			}
		}

		/* Same-mask sibling-attach: when the LIVE for this channel is
		 * already keyed but the dvbapi server skipped sendCaPmt because
		 * (oldmask == newmask), no onDescrMode for this RECORD session
		 * will arrive. Adopt the sibling's keying directly so the wait
		 * predicate succeeds without the 3-second fallback timeout. */
		if (!ds.csa_alt_active && ds.output_token > 0) {
			uint32_t sib = findRunningSibling(channel_id, session_id);
			if (sib != 0) {
				ds.csa_alt_active = true;
				already_active = true;
				printf("[softcsa] waitForRecordStart: adopting keyed sibling %u for channel %llx\n",
				       sib, (unsigned long long)channel_id);
			}
		}

		if (already_active && ds.output_token > 0) {
			printf("[softcsa] waitForRecordStart: csa_alt already active, returning immediately\n");
			ds.record_fd = -1;
			return true;
		}
	}

	printf("[softcsa] waitForRecordStart: fd %d stored, waiting %dms\n", fd, timeout_ms);

	/* Predicate: engine keyed (csa_alt_active) AND output attached
	 * (output_token > 0). */
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
		return sess_it->second.csa_alt_active && sess_it->second.output_token > 0;
	});

	if (started) {
		std::lock_guard<std::mutex> lock(mtx);
		auto ch_it = channel_to_session.find(std::make_pair(channel_id, SOFTCSA_SESSION_RECORD));
		if (ch_it != channel_to_session.end()) {
			auto sess_it = sessions.find(ch_it->second);
			if (sess_it != sessions.end())
				sess_it->second.record_fd = -1;
		}
	} else {
		/* Timeout: detach so a late onDescrMode cannot write into a
		 * consumer thread about to be torn down. Order: close write end
		 * (signals EOF), join, close read end. */
		PendingDetach pd;
		pd.outputs = nullptr;
		pd.output_token = -1;
		pd.output_fd = -1;
		pd.stream_pipe_read = -1;
		{
			std::lock_guard<std::mutex> lock(mtx);
			auto ch_it = channel_to_session.find(std::make_pair(channel_id, SOFTCSA_SESSION_RECORD));
			if (ch_it != channel_to_session.end()) {
				auto sess_it = sessions.find(ch_it->second);
				if (sess_it != sessions.end()) {
					pd = extractPendingDetach(sess_it->second);
					sess_it->second.record_fd = -1;
					auto tap_it = m_taps.find(sess_it->second.tap_key);
					if (pd.outputs && pd.output_token > 0)
						pd.outputs->remove(pd.output_token);
					if (tap_it != m_taps.end())
						stopReaderIfIdleLocked(tap_it->second.get());
				}
			}
		}
		if (pd.output_fd >= 0)
			::close(pd.output_fd);
		if (pd.stream_consumer.joinable())
			pd.stream_consumer.join();
		if (pd.stream_pipe_read >= 0)
			::close(pd.stream_pipe_read);
	}

	printf("[softcsa] waitForRecordStart: %s\n", started ? "started" : "timeout");
	return started;
}

bool CSoftCSAManager::waitForStreamStart(t_channel_id channel_id, SoftCSAStreamCallback cb, int timeout_ms)
{
	uint32_t session_id = 0;
	bool already_active = false;

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

		if (sess_it->second.passive) {
			printf("[softcsa] waitForStreamStart: passive, skipping for channel %llx\n",
			       (unsigned long long)channel_id);
			return false;
		}

		SessionState &ds = sess_it->second;
		session_id = ch_it->second;
		ds.stream_callback = cb;
		already_active = ds.csa_alt_active;
	}

	if (cloneAndStartStream(session_id, cb))
		return true;

	/* Pipe bridge: O_NONBLOCK write end into COutputFdSet, blocking
	 * read end drained by the consumer thread. */
	{
		/* unique_lock so mtx can be dropped before joining the consumer
		 * thread on the reader-start-failure rollback path. */
		std::unique_lock<std::mutex> lock(mtx);
		auto it = sessions.find(session_id);
		if (it == sessions.end())
			return false;
		SessionState &ds = it->second;

		if (ds.output_token <= 0) {
			int p[2];
			if (::pipe(p) < 0) {
				printf("[softcsa] waitForStreamStart: pipe failed\n");
				return false;
			}
			enlargePipeBuffer(p[1]);
			int wflags = ::fcntl(p[1], F_GETFL, 0);
			if (wflags >= 0)
				::fcntl(p[1], F_SETFL, wflags | O_NONBLOCK);
			int rflags = ::fcntl(p[0], F_GETFL, 0);
			if (rflags >= 0)
				::fcntl(p[0], F_SETFL, rflags & ~O_NONBLOCK);

			auto tap_it = m_taps.find(ds.tap_key);
			if (tap_it == m_taps.end()) {
				::close(p[0]);
				::close(p[1]);
				return false;
			}
			SoftCSAServiceTap &tap = *tap_it->second;
			int token = tap.outputs->add(p[1], COutputFdSet::ROLE_STREAM);
			if (token < 0) {
				::close(p[0]);
				::close(p[1]);
				printf("[softcsa] waitForStreamStart: outputs->add failed\n");
				return false;
			}
			ds.output_token = token;
			ds.output_fd = p[1];
			ds.stream_pipe_read = p[0];

			SoftCSAStreamCallback captured_cb = cb;
			int read_end = p[0];
			ds.stream_consumer = std::thread([captured_cb, read_end]() {
				const int kBuf = 65536;
				std::vector<uint8_t> buf(kBuf);
				while (true) {
					ssize_t n = ::read(read_end, buf.data(), kBuf);
					if (n <= 0) break;
					captured_cb(buf.data(), (int)n);
				}
			});

			if (!tap.reader_started) {
				if (tap.reader->start() == 0) {
					tap.reader_started = true;
				} else {
					/* Reader failed to start. Roll back the output
					 * registration and pipe under the lock, then join
					 * the consumer thread outside. */
					printf("[softcsa] waitForStreamStart: reader->start failed, rolling back\n");
					PendingDetach pd = extractPendingDetach(ds);
					ds.stream_callback = nullptr;
					if (pd.outputs && pd.output_token > 0)
						pd.outputs->remove(pd.output_token);
					lock.unlock();
					if (pd.output_fd >= 0)
						::close(pd.output_fd);
					if (pd.stream_consumer.joinable())
						pd.stream_consumer.join();
					if (pd.stream_pipe_read >= 0)
						::close(pd.stream_pipe_read);
					return false;
				}
			}
		}

		/* Same-mask sibling-attach: see waitForRecordStart for the
		 * dvbapi rationale. Stream sessions can ride on the sibling's
		 * keying even when the dvbapi server suppressed the per-session
		 * CSA-ALT confirmation. */
		if (!ds.csa_alt_active && ds.output_token > 0) {
			uint32_t sib = findRunningSibling(channel_id, session_id);
			if (sib != 0) {
				ds.csa_alt_active = true;
				already_active = true;
				printf("[softcsa] waitForStreamStart: adopting keyed sibling %u for channel %llx\n",
				       sib, (unsigned long long)channel_id);
			}
		}

		if (already_active && ds.output_token > 0) {
			printf("[softcsa] waitForStreamStart: csa_alt already active, returning immediately\n");
			ds.stream_callback = nullptr;
			return true;
		}
	}

	printf("[softcsa] waitForStreamStart: callback stored, waiting %dms\n", timeout_ms);

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
		return sess_it->second.csa_alt_active && sess_it->second.output_token > 0;
	});

	if (started) {
		std::lock_guard<std::mutex> lock(mtx);
		auto ch_it = channel_to_session.find(std::make_pair(channel_id, SOFTCSA_SESSION_STREAM));
		if (ch_it != channel_to_session.end()) {
			auto sess_it = sessions.find(ch_it->second);
			if (sess_it != sessions.end())
				sess_it->second.stream_callback = nullptr;
		}
	} else {
		/* Order: close write end (signals EOF), join, close read end. */
		PendingDetach pd;
		pd.outputs = nullptr;
		pd.output_token = -1;
		pd.output_fd = -1;
		pd.stream_pipe_read = -1;
		{
			std::lock_guard<std::mutex> lock(mtx);
			auto ch_it = channel_to_session.find(std::make_pair(channel_id, SOFTCSA_SESSION_STREAM));
			if (ch_it != channel_to_session.end()) {
				auto sess_it = sessions.find(ch_it->second);
				if (sess_it != sessions.end()) {
					pd = extractPendingDetach(sess_it->second);
					sess_it->second.stream_callback = nullptr;
					auto tap_it = m_taps.find(sess_it->second.tap_key);
					if (pd.outputs && pd.output_token > 0)
						pd.outputs->remove(pd.output_token);
					if (tap_it != m_taps.end())
						stopReaderIfIdleLocked(tap_it->second.get());
					/* Pipe fds must NOT close under lock. */
				}
			}
		}
		if (pd.output_fd >= 0)
			::close(pd.output_fd);
		if (pd.stream_consumer.joinable())
			pd.stream_consumer.join();
		if (pd.stream_pipe_read >= 0)
			::close(pd.stream_pipe_read);
	}

	printf("[softcsa] waitForStreamStart: %s\n", started ? "started" : "timeout");
	return started;
}

bool CSoftCSAManager::hasRegisteredSession(t_channel_id channel_id, SoftCSASessionType type)
{
	std::lock_guard<std::mutex> lock(mtx);
	return channel_to_session.find(std::make_pair(channel_id, type)) != channel_to_session.end();
}

bool CSoftCSAManager::cloneAndStartRecord(uint32_t session_id, int fd)
{
	bool started = false;
	{
		/* unique_lock so mtx can be dropped before joining the consumer
		 * thread on the reader-start-failure rollback path. */
		std::unique_lock<std::mutex> lock(mtx);

		auto sess_it = sessions.find(session_id);
		if (sess_it == sessions.end())
			return false;
		SessionState &ds = sess_it->second;

		if (ds.output_token > 0)
			return false;

		/* Sibling on the same channel: engine is shared via tap. */
		uint32_t sib_id = findRunningSibling(ds.channel_id, session_id);
		if (sib_id == 0)
			return false;

		printf("[softcsa] cloneAndStartRecord: session %u using shared engine from sibling %u\n",
		       session_id, sib_id);

		auto sib_it = sessions.find(sib_id);
		uint8_t sib_ecm_mode = (sib_it != sessions.end()) ? sib_it->second.ecm_mode : 0;

		int p[2];
		if (::pipe(p) < 0)
			return false;
		enlargePipeBuffer(p[1]);
		int wflags = ::fcntl(p[1], F_GETFL, 0);
		if (wflags >= 0)
			::fcntl(p[1], F_SETFL, wflags | O_NONBLOCK);
		int rflags = ::fcntl(p[0], F_GETFL, 0);
		if (rflags >= 0)
			::fcntl(p[0], F_SETFL, rflags & ~O_NONBLOCK);

		auto tap_it = m_taps.find(ds.tap_key);
		if (tap_it == m_taps.end()) {
			::close(p[0]);
			::close(p[1]);
			return false;
		}
		SoftCSAServiceTap &tap = *tap_it->second;
		int token = tap.outputs->add(p[1], COutputFdSet::ROLE_RECORD);
		if (token < 0) {
			::close(p[0]);
			::close(p[1]);
			return false;
		}

		/* Commit. */
		ds.csa_alt_active = true;
		ds.ecm_mode = sib_ecm_mode;
		ds.output_token = token;
		ds.output_fd = p[1];
		ds.stream_pipe_read = p[0];
		ds.record_fd = -1;

		int read_end = p[0];
		int record_fd_cap = fd;
		ds.stream_consumer = std::thread([read_end, record_fd_cap]() {
			const int kBuf = 1024 * 1024;
			std::vector<uint8_t> buf(kBuf);
			off_t total_written = 0;
			off_t last_advised = 0;
			const off_t kAdviseInterval = 4 * 1024 * 1024;
			while (true) {
				ssize_t n = ::read(read_end, buf.data(), kBuf);
				if (n <= 0) break;
				const uint8_t *ptr = buf.data();
				ssize_t rem = n;
				while (rem > 0) {
					ssize_t w = ::write(record_fd_cap, ptr, (size_t)rem);
					if (w < 0) {
						if (errno == EINTR) continue;
						return;
					}
					ptr += w;
					rem -= w;
				}
				total_written += n;
				if (total_written - last_advised >= kAdviseInterval) {
					::sync_file_range(record_fd_cap, last_advised,
					                  total_written - last_advised,
					                  SYNC_FILE_RANGE_WRITE);
					if (last_advised >= kAdviseInterval) {
						::posix_fadvise(record_fd_cap,
						                last_advised - kAdviseInterval,
						                kAdviseInterval,
						                POSIX_FADV_DONTNEED);
					}
					last_advised = total_written;
				}
			}
		});

		if (!tap.reader_started) {
			if (tap.reader->start() == 0) {
				tap.reader_started = true;
			} else {
				/* Reader failed to start: roll back so the caller does
				 * not see a session marked active with no reader. */
				printf("[softcsa] cloneAndStartRecord: reader->start failed, rolling back\n");
				PendingDetach pd = extractPendingDetach(ds);
				ds.csa_alt_active = false;
				if (pd.outputs && pd.output_token > 0)
					pd.outputs->remove(pd.output_token);
				lock.unlock();
				if (pd.output_fd >= 0)
					::close(pd.output_fd);
				if (pd.stream_consumer.joinable())
					pd.stream_consumer.join();
				if (pd.stream_pipe_read >= 0)
					::close(pd.stream_pipe_read);
				return false;
			}
		}
		started = true;
	}

	if (started)
		record_cv.notify_all();
	return started;
}

bool CSoftCSAManager::cloneAndStartStream(uint32_t session_id, SoftCSAStreamCallback cb)
{
	bool started = false;
	{
		/* unique_lock needed so we can drop mtx before joining the consumer
		 * thread on the reader-start-failure rollback path. */
		std::unique_lock<std::mutex> lock(mtx);

		auto sess_it = sessions.find(session_id);
		if (sess_it == sessions.end())
			return false;
		SessionState &ds = sess_it->second;

		if (ds.output_token > 0)
			return false;

		uint32_t sib_id = findRunningSibling(ds.channel_id, session_id);
		if (sib_id == 0)
			return false;

		printf("[softcsa] cloneAndStartStream: session %u using shared engine from sibling %u\n",
		       session_id, sib_id);

		auto sib_it = sessions.find(sib_id);
		uint8_t sib_ecm_mode = (sib_it != sessions.end()) ? sib_it->second.ecm_mode : 0;

		int p[2];
		if (::pipe(p) < 0)
			return false;
		enlargePipeBuffer(p[1]);
		int wflags = ::fcntl(p[1], F_GETFL, 0);
		if (wflags >= 0)
			::fcntl(p[1], F_SETFL, wflags | O_NONBLOCK);
		int rflags = ::fcntl(p[0], F_GETFL, 0);
		if (rflags >= 0)
			::fcntl(p[0], F_SETFL, rflags & ~O_NONBLOCK);

		auto tap_it = m_taps.find(ds.tap_key);
		if (tap_it == m_taps.end()) {
			::close(p[0]);
			::close(p[1]);
			return false;
		}
		SoftCSAServiceTap &tap = *tap_it->second;
		int token = tap.outputs->add(p[1], COutputFdSet::ROLE_STREAM);
		if (token < 0) {
			::close(p[0]);
			::close(p[1]);
			return false;
		}

		/* Commit. */
		ds.csa_alt_active = true;
		ds.ecm_mode = sib_ecm_mode;
		ds.output_token = token;
		ds.output_fd = p[1];
		ds.stream_pipe_read = p[0];
		ds.stream_callback = nullptr;

		SoftCSAStreamCallback captured_cb = cb;
		int read_end = p[0];
		ds.stream_consumer = std::thread([captured_cb, read_end]() {
			const int kBuf = 65536;
			std::vector<uint8_t> buf(kBuf);
			while (true) {
				ssize_t n = ::read(read_end, buf.data(), kBuf);
				if (n <= 0) break;
				captured_cb(buf.data(), (int)n);
			}
		});

		if (!tap.reader_started) {
			if (tap.reader->start() == 0) {
				tap.reader_started = true;
			} else {
				/* Reader failed to start: roll back so the caller does
				 * not see a session marked active with no reader. */
				printf("[softcsa] cloneAndStartStream: reader->start failed, rolling back\n");
				PendingDetach pd = extractPendingDetach(ds);
				ds.csa_alt_active = false;
				if (pd.outputs && pd.output_token > 0)
					pd.outputs->remove(pd.output_token);
				lock.unlock();
				if (pd.output_fd >= 0)
					::close(pd.output_fd);
				if (pd.stream_consumer.joinable())
					pd.stream_consumer.join();
				if (pd.stream_pipe_read >= 0)
					::close(pd.stream_pipe_read);
				return false;
			}
		}
		started = true;
	}

	if (started)
		record_cv.notify_all();
	return started;
}

int CSoftCSAManager::startLive(uint32_t session_id, int decoder_index)
{
	/* Look up session + tap under mtx; capture state for out-of-lock
	 * work. softcsaRebindDecoder* takes zapit_mutex, so manager.mtx
	 * must be dropped first to preserve the zapit_mutex -> manager.mtx
	 * hierarchy isLiveDataFlowing relies on. */
	int adapter;
	std::set<int> excluded_units;
	{
		std::lock_guard<std::mutex> lock(mtx);
		/* cloneAndStartLive and onDescrMode can both reach startLive
		 * concurrently in the same-mask sibling case. Check m_live
		 * (completed) and m_live_starting (in-progress sentinel) so
		 * two parallel calls cannot both pass the dedup and allocate
		 * duplicate DVR slots / output_tokens. */
		if (m_live.find(session_id) != m_live.end()
		    || m_live_starting.count(session_id))
			return 0;
		m_live_starting[session_id] = decoder_index;
		auto sit = sessions.find(session_id);
		if (sit == sessions.end()) {
			m_live_starting.erase(session_id);
			return -1;
		}
		auto tap_it = m_taps.find(sit->second.tap_key);
		if (tap_it == m_taps.end() || !tap_it->second) {
			m_live_starting.erase(session_id);
			return -2;
		}
		adapter = tap_it->second->adapter;
		/* Exclude every tap demux unit and every existing LIVE/PIP slot
		 * unit on the same adapter so the DVR slot allocator does not
		 * collide on boxes with a small MAX_DMX_UNITS. On per-device
		 * vendor drivers, a second open of the same demux<M> would
		 * silently overwrite the prior source. */
		for (auto &kv : m_taps) {
			if (kv.second && kv.second->adapter == adapter)
				excluded_units.insert(kv.second->tap_demux_unit);
		}
		for (auto &kv : m_live) {
			if (kv.second && kv.second->slot &&
			    kv.second->slot->adapter() == adapter)
				excluded_units.insert(kv.second->slot->demuxUnit());
		}
	}

	std::unique_ptr<SoftCSALiveAttachment> live(new SoftCSALiveAttachment);
	live->decoder_index = decoder_index;
	live->output_token = -1;
	live->slot.reset(CDvrDemuxSlot::alloc(adapter, excluded_units));
	if (!live->slot) {
		std::lock_guard<std::mutex> lock(mtx);
		m_live_starting.erase(session_id);
		return -3;
	}

	/* Re-validate the chosen unit under lock: a parallel startLive for a
	 * different session may have allocated the same unit between our
	 * excluded_units snapshot and CDvrDemuxSlot::alloc. */
	{
		std::lock_guard<std::mutex> lock(mtx);
		int chosen_unit = live->slot->demuxUnit();
		bool collision = false;
		for (auto &kv : m_live) {
			if (kv.second && kv.second->slot &&
			    kv.second->slot->adapter() == adapter &&
			    kv.second->slot->demuxUnit() == chosen_unit) {
				collision = true;
				break;
			}
		}
		if (collision) {
			printf("[softcsa] startLive: dvr_demux_unit %d collided post-alloc, aborting session %u\n",
			       chosen_unit, session_id);
			live.reset();
			m_live_starting.erase(session_id);
			return -3;
		}
	}

	/* Register the output token first so the reader, once started, has
	 * a sink ready. */
	int output_token = -1;
	SoftCSAServiceTapKey tap_key{};
	{
		std::lock_guard<std::mutex> lock(mtx);
		auto sit = sessions.find(session_id);
		if (sit == sessions.end()) {
			m_live_starting.erase(session_id);
			return -4;
		}
		auto tap_it = m_taps.find(sit->second.tap_key);
		if (tap_it == m_taps.end() || !tap_it->second) {
			m_live_starting.erase(session_id);
			return -4;
		}
		SoftCSAServiceTap *tap_ptr = tap_it->second.get();
		output_token = tap_ptr->outputs->add(live->slot->writeFd(),
		                                     COutputFdSet::ROLE_LIVE);
		if (output_token < 0) {
			m_live_starting.erase(session_id);
			return -4;
		}
		live->output_token = output_token;
		live->tap_key = sit->second.tap_key;
		tap_key = sit->second.tap_key;
	}

	/* Callback runs inside the rebind, after the live DECODER filters
	 * on the tap pids are stopped (so the tap filter is not pre-empted
	 * by exclusive-output vendor drivers) and before the new softcsa
	 * decoder demuxes are started. zapit_mutex is held throughout. */
	auto ready_for_data = [this, tap_key](int cb_adapter, int cb_dvr_demux_unit) {
		{
			std::lock_guard<std::mutex> lock(mtx);
			auto tap_it = m_taps.find(tap_key);
			if (tap_it != m_taps.end() && tap_it->second
			    && !tap_it->second->reader_started
			    && tap_it->second->reader)
			{
				tap_it->second->reader->start();
				tap_it->second->reader_started = true;
			}
		}
		/* Anti-race for "decoder Start before any data is buffered"
		 * which manifests as PTS=0 stalls on some platforms. Both
		 * waits are clamped to 0 or 100..2000ms by the config parser;
		 * default 0+0 disables both. */
		int wait_ms = CSoftCSAConfig::getInstance()->waitForDataTimeoutMs();
		if (wait_ms > 0) {
			auto deadline = std::chrono::steady_clock::now() +
			                std::chrono::milliseconds(wait_ms);
			while (std::chrono::steady_clock::now() < deadline) {
				if (isLiveDataFlowing(cb_adapter, cb_dvr_demux_unit))
					break;
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
			}
		}
		int buffer_ms = CSoftCSAConfig::getInstance()->bufferTimeMs();
		if (buffer_ms > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(buffer_ms));
	};

	int rc = CZapit::getInstance()->softcsaRebindDecoderToDvrDemux(
		decoder_index, adapter, live->slot->demuxUnit(), ready_for_data);
	if (rc != 0) {
		/* Roll back the output token and stop the reader if no other
		 * consumer remains. RECORD/STREAM siblings sharing this tap
		 * keep their tokens, so stopReaderIfIdleLocked is a no-op in
		 * that case. */
		std::lock_guard<std::mutex> lock(mtx);
		auto sit = sessions.find(session_id);
		if (sit != sessions.end()) {
			auto tap_it = m_taps.find(sit->second.tap_key);
			if (tap_it != m_taps.end() && tap_it->second) {
				tap_it->second->outputs->remove(output_token);
				stopReaderIfIdleLocked(tap_it->second.get());
			}
		}
		live.reset();
		m_live_starting.erase(session_id);
		return -5;
	}

	int dvr_unit;
	int dvr_adapter;
	bool aborted = false;
	{
		std::lock_guard<std::mutex> lock(mtx);
		/* While the rebind held zapit_mutex (and possibly slept in
		 * ready_for_data), a parallel stopSession may have set
		 * stopping=true and erased the session. onDescrMode's pre-flight
		 * check on stopping is not enough on its own. */
		auto sit = sessions.find(session_id);
		if (sit == sessions.end() || sit->second.stopping) {
			aborted = true;
			auto tap_it = m_taps.find(live->tap_key);
			if (tap_it != m_taps.end() && tap_it->second) {
				tap_it->second->outputs->remove(output_token);
				stopReaderIfIdleLocked(tap_it->second.get());
			}
			m_live_starting.erase(session_id);
		} else {
			dvr_unit = live->slot->demuxUnit();
			dvr_adapter = live->slot->adapter();
			m_live[session_id] = std::move(live);
			m_live_starting.erase(session_id);
		}
	}

	if (aborted) {
		/* Decoder is bound to dvr_demux_unit by the rebind we just did.
		 * Revert to FRONT before the slot dtor closes the dvr_fd, so the
		 * next StartPlayBack starts cleanly. zapit_mutex order preserved
		 * (no manager mtx held here). */
		CZapit::getInstance()->softcsaRebindDecoderToFrontDemux(
			decoder_index, /*skip_decoder_start=*/true);
		live.reset();
		printf("[softcsa] startLive: session %u aborted mid-rebind, rolled back\n",
		       session_id);
		return -6;
	}

	printf("[softcsa] startLive: session %u decoder_index=%d adapter=%d dvr_demux=%d ok\n",
	       session_id, decoder_index, dvr_adapter, dvr_unit);
	return 0;
}

int CSoftCSAManager::cloneAndStartLive(uint32_t session_id, int decoder_index)
{
	{
		std::lock_guard<std::mutex> lock(mtx);
		if (m_standby_in_progress)
			return -1;
		auto sit = sessions.find(session_id);
		if (sit == sessions.end())
			return -1;
		if (sit->second.passive)
			return -1;
		/* onDescrMode beat us. Not an error. */
		if (m_live.find(session_id) != m_live.end())
			return 0;
		/* Sibling on the same channel: keys are already in the shared engine. */
		uint32_t sib_id = findRunningSibling(sit->second.channel_id, session_id);
		if (sib_id == 0)
			return -1;
		auto sib_it = sessions.find(sib_id);
		if (sib_it != sessions.end())
			sit->second.ecm_mode = sib_it->second.ecm_mode;
		printf("[softcsa] cloneAndStartLive: session %u using shared engine from sibling %u\n",
		       session_id, sib_id);
	}
	int rc = startLive(session_id, decoder_index);
	if (rc == 0) {
		/* Engine is physically shared via the tap. Flip csa_alt_active
		 * only after startLive succeeded so a third thread cannot see
		 * a transient alive state if the attach fails. */
		std::lock_guard<std::mutex> lock(mtx);
		auto sit = sessions.find(session_id);
		if (sit != sessions.end())
			sit->second.csa_alt_active = true;
	}
	return rc;
}

int CSoftCSAManager::cloneAndStartPip(uint32_t session_id, int decoder_index)
{
	{
		std::lock_guard<std::mutex> lock(mtx);
		if (m_standby_in_progress)
			return -1;
		auto sit = sessions.find(session_id);
		if (sit == sessions.end())
			return -1;
		if (sit->second.passive)
			return -1;
		if (m_live.find(session_id) != m_live.end())
			return 0;
		uint32_t sib_id = findRunningSibling(sit->second.channel_id, session_id);
		if (sib_id == 0)
			return -1;
		auto sib_it = sessions.find(sib_id);
		if (sib_it != sessions.end())
			sit->second.ecm_mode = sib_it->second.ecm_mode;
		printf("[softcsa] cloneAndStartPip: session %u using shared engine from sibling %u\n",
		       session_id, sib_id);
	}
	int rc = startLive(session_id, decoder_index);
	if (rc == 0) {
		std::lock_guard<std::mutex> lock(mtx);
		auto sit = sessions.find(session_id);
		if (sit != sessions.end())
			sit->second.csa_alt_active = true;
	}
	return rc;
}

void CSoftCSAManager::stopLive(uint32_t session_id, bool skip_decoder_start)
{
	/* Snapshot decoder_index + output_token + tap_key from the live
	 * record itself. The sessions[] entry may already be gone; falling
	 * back to it would leave tap_key zero-initialised and m_taps.find
	 * could match an unrelated tap on (frontend 0, channel 0). */
	int decoder_index = -1;
	int output_token = -1;
	SoftCSAServiceTapKey tap_key{};
	{
		std::lock_guard<std::mutex> lock(mtx);
		auto it = m_live.find(session_id);
		if (it == m_live.end())
			return;
		decoder_index = it->second->decoder_index;
		output_token = it->second->output_token;
		tap_key = it->second->tap_key;

		/* Auto-skip the decoder restart in front-rebind when an active
		 * RECORD/STREAM sibling on the same channel keeps the engine
		 * keyed via the tap. Restarting the decoder on the FRONT-bound
		 * scrambled stream produces a transient audio glitch and -- if
		 * the caller has its own follow-up StopPlayBack chain -- holds
		 * the dvr-loopback fd in tap.outputs without a consumer until
		 * outputs->remove runs below, which can stall the sibling pipe
		 * via writeAll EAGAIN-spin. The next StartPlayBack/TuneChannel
		 * by the caller reconfigures the decoder cleanly. */
		if (!skip_decoder_start) {
			auto self = sessions.find(session_id);
			t_channel_id ch = (self != sessions.end()) ? self->second.channel_id : 0;
			if (ch != 0) {
				for (const auto &p : sessions) {
					if (p.first == session_id) continue;
					if (p.second.channel_id != ch) continue;
					if (p.second.passive) continue;
					if (p.second.output_token <= 0) continue;
					if (p.second.type == SOFTCSA_SESSION_RECORD ||
					    p.second.type == SOFTCSA_SESSION_STREAM) {
						printf("[softcsa] stopLive: auto-skip decoder restart, sibling %u (%s) on channel %llx still consumes the tap\n",
						       p.first,
						       (p.second.type == SOFTCSA_SESSION_RECORD) ? "RECORD" : "STREAM",
						       (unsigned long long)ch);
						skip_decoder_start = true;
						break;
					}
				}
			}
		}
	}
	printf("[softcsa] stopLive: session %u decoder_index=%d skip_decoder_start=%d\n",
	       session_id, decoder_index, skip_decoder_start ? 1 : 0);

	/* Rebind decoders back to main demux first. softcsaRebind* calls
	 * Stop internally; for skip_decoder_start=false this also restarts
	 * the decoder on FRONT briefly so any in-flight dvr<M> write drains
	 * while the decoder is still consuming, before the outputs->remove
	 * below evicts the dvr fd. For skip=true the drain argument does
	 * not apply -- the decoder stays stopped and the dvr-loopback fd
	 * is removed unconditionally below. softcsaRebind* takes
	 * zapit_mutex, so no manager mutex may be held here. */
	CZapit::getInstance()->softcsaRebindDecoderToFrontDemux(decoder_index, skip_decoder_start);

	std::unique_ptr<SoftCSALiveAttachment> dying;
	{
		std::lock_guard<std::mutex> lock(mtx);
		auto it = m_live.find(session_id);
		if (it == m_live.end())
			return;
		dying = std::move(it->second);
		m_live.erase(it);
		auto tap_it = m_taps.find(tap_key);
		if (tap_it != m_taps.end() && tap_it->second) {
			if (output_token > 0)
				tap_it->second->outputs->remove(output_token);
			/* LIVE-only path: no PIP/RECORD/STREAM sibling holds the
			 * tap, refcount stays > 0 until capmt's stopSession runs,
			 * but in that window the reader has no consumer. Stop it
			 * here; it restarts on the next attach via the normal
			 * waitForXStart / onDescrMode path. */
			stopReaderIfIdleLocked(tap_it->second.get());
		}
	}
	/* dying drops here, outside mtx; CDvrDemuxSlot's dtor closes the
	 * dvr_fd. Neither outputs nor reader thread can still reference
	 * the fd at this point. */
}

void CSoftCSAManager::enterStandbyTeardown()
{
	/* RAII guard so an exception out of stopLive (or transitively
	 * softcsaRebindDecoderToFrontDemux) cannot leave the standby flag
	 * stuck at true and lock out every future LIVE/PIP attach. */
	struct StandbyGuard {
		CSoftCSAManager *mgr;
		~StandbyGuard() {
			std::lock_guard<std::mutex> lock(mgr->mtx);
			mgr->m_standby_in_progress = false;
		}
	};
	{
		std::lock_guard<std::mutex> lock(mtx);
		m_standby_in_progress = true;
	}
	StandbyGuard guard{this};

	/* Drain in a loop; a late onDescrMode arriving between snapshot and
	 * stopLive could otherwise insert a fresh m_live entry that the
	 * single-pass loop would miss. The standby flag blocks new attaches
	 * but a startLive already past its first crit-section can still
	 * complete. Bound the loop so a runaway producer cannot spin us. */
	const int kMaxIterations = 5;
	for (int iter = 0; iter < kMaxIterations; iter++) {
		std::vector<uint32_t> live_ids;
		{
			std::lock_guard<std::mutex> lock(mtx);
			for (auto &lp : m_live)
				live_ids.push_back(lp.first);
		}
		if (live_ids.empty())
			break;
		printf("[softcsa] enterStandbyTeardown: draining %zu live attachment(s) (iter %d)\n",
		       live_ids.size(), iter);
		for (uint32_t sid : live_ids)
			stopLive(sid, /*skip_decoder_start=*/true);
	}
}

void CSoftCSAManager::stopReaderIfIdleLocked(SoftCSAServiceTap *tap)
{
	if (!tap || !tap->reader_started || !tap->reader)
		return;
	if (!tap->outputs->empty())
		return;
	tap->reader->stop();
	tap->reader_started = false;
}

bool CSoftCSAManager::isLiveDataFlowing(int adapter, int demux_unit)
{
	std::lock_guard<std::mutex> lock(mtx);
	for (auto &lp : m_live) {
		auto &live = lp.second;
		if (!live || !live->slot)
			continue;
		if (live->slot->adapter() == adapter &&
		    live->slot->demuxUnit() == demux_unit)
		{
			auto sit = sessions.find(lp.first);
			if (sit == sessions.end())
				return false;
			auto tap_it = m_taps.find(sit->second.tap_key);
			if (tap_it == m_taps.end() || !tap_it->second)
				return false;
			return tap_it->second->outputs->writeCountFor(live->output_token) > 0;
		}
	}
	return false;
}

bool CSoftCSAManager::isDecoderDvrBound(int decoder_index)
{
	std::lock_guard<std::mutex> lock(mtx);
	for (const auto &lp : m_live) {
		if (lp.second && lp.second->decoder_index == decoder_index)
			return true;
	}
	return false;
}

uint32_t CSoftCSAManager::findRunningSibling(t_channel_id channel_id, uint32_t exclude_session_id) const
{
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
		if (pair.second.passive)
			continue;
		/* RECORD/STREAM keep their token on SessionState; LIVE/PIP
		 * keep theirs on SoftCSALiveAttachment in m_live. Either form
		 * confirms the engine is keyed and has a live consumer. */
		bool engine_keyed = (pair.second.output_token > 0)
		                    || (m_live.find(pair.first) != m_live.end());
		if (!engine_keyed || !pair.second.csa_alt_active)
			continue;
		int r = rank(pair.second.type);
		if (r < best_rank) {
			best_rank = r;
			best_id = pair.first;
		}
	}
	return best_id;
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

uint8_t CSoftCSAManager::getCapmtCaMask(uint32_t session_id)
{
	std::lock_guard<std::mutex> lock(mtx);
	auto it = sessions.find(session_id);
	return it == sessions.end() ? 0 : it->second.capmt_ca_mask;
}

bool CSoftCSAManager::willReuseSession(t_channel_id channel_id, SoftCSASessionType type,
                                        int frontend_num)
{
	std::lock_guard<std::mutex> lock(mtx);
	auto ch_it = channel_to_session.find(std::make_pair(channel_id, type));
	if (ch_it == channel_to_session.end())
		return false;
	auto sess_it = sessions.find(ch_it->second);
	if (sess_it == sessions.end())
		return false;
	const SessionState &ds = sess_it->second;
	bool alive = (ds.output_token > 0) || (ds.csa_alt_active && !ds.passive);
	return alive
	    && ds.tap_key.frontend_num == frontend_num;
}

bool CSoftCSAManager::isPassiveSession(uint32_t session_id)
{
	std::lock_guard<std::mutex> lock(mtx);
	auto it = sessions.find(session_id);
	return it != sessions.end() && it->second.passive;
}

bool CSoftCSAManager::hasAnyRunningSession(t_channel_id channel_id)
{
	std::lock_guard<std::mutex> lock(mtx);
	/* exclude=0 excludes nothing (session ids start at 1). */
	return findRunningSibling(channel_id, 0) != 0;
}

SoftCSASessionType CSoftCSAManager::getSessionType(uint32_t session_id)
{
	std::lock_guard<std::mutex> lock(mtx);
	auto it = sessions.find(session_id);
	if (it == sessions.end())
		return SOFTCSA_SESSION_LIVE;   /* sentinel; caller must check */
	return it->second.type;
}

int CSoftCSAManager::getPipDevIndex(uint32_t session_id)
{
	std::lock_guard<std::mutex> lock(mtx);
	auto it = sessions.find(session_id);
	if (it == sessions.end())
		return -1;
	if (it->second.type != SOFTCSA_SESSION_PIP)
		return -1;
	return it->second.pip_dev;
}

std::vector<uint32_t> CSoftCSAManager::getActiveSessionsForChannel(t_channel_id channel_id)
{
	std::vector<uint32_t> out;
	std::lock_guard<std::mutex> lock(mtx);
	for (const auto &kv : sessions) {
		if (kv.second.channel_id == channel_id)
			out.push_back(kv.first);
	}
	return out;
}

void CSoftCSAManager::replaceSessionPids(uint32_t session_id, const std::set<uint16_t> &new_pids)
{
	std::lock_guard<std::mutex> lock(mtx);
	auto it = sessions.find(session_id);
	if (it == sessions.end())
		return;

	auto &pid_list = it->second.pids;
	std::set<uint16_t> old_set(pid_list.begin(), pid_list.end());

	auto tap_it = m_taps.find(it->second.tap_key);
	bool tap_alive = (tap_it != m_taps.end()
	                  && tap_it->second
	                  && tap_it->second->reader);

	/* Vendor add-only: forward new PIDs to the tap. Surplus old PIDs
	 * stay in the vendor filter set; consumers drop the unused PID
	 * traffic by demuxing only what they care about. */
	for (uint16_t pid : new_pids) {
		if (old_set.find(pid) == old_set.end() && tap_alive)
			tap_it->second->reader->addPid(pid);
	}

	pid_list.assign(new_pids.begin(), new_pids.end());
}

