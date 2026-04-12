/*
 * CSoftCSASession - single SoftCSA descrambling session (live/pip/record/stream)
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

#include "softcsa_session.h"
#include "softcsa_engine.h"
#include "ts_demuxer.h"
#include "pes_ringbuffer.h"
#include <dmx_hal.h>
#include <system/set_threadname.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>

CSoftCSASession::CSoftCSASession(SoftCSASessionType type, int adapter, int demux_unit_num, int frontend_num_val)
	: session_type(type)
	, engine(new CSoftCSAEngine())
	, ts_demuxer(NULL)
	, demux(NULL)
	, video_rb(NULL)
	, audio_rb(NULL)
	, video_fd(-1)
	, audio_fd(-1)
	, record_fd(-1)
	, adapter_num(adapter)
	, demux_unit(demux_unit_num)
	, frontend_num(frontend_num_val)
	, dec_vpid(0)
	, dec_apid(0)
	, dec_pcrpid(0)
	, pending_apid(-1)
	, buffer(NULL)
	, running(false)
{
	if (posix_memalign((void **)&buffer, 16, BUFFER_SIZE) != 0)
		buffer = NULL;

	demux = new cDemux(demux_unit);
	demux->Open(DMX_TP_CHANNEL, NULL, BUFFER_SIZE);
}

CSoftCSASession::~CSoftCSASession()
{
	if (running.load())
		stop();
	delete engine;
	delete ts_demuxer;
	delete demux;
	delete video_rb;
	delete audio_rb;
	free(buffer);
}

void CSoftCSASession::setDecoderPids(unsigned short vpid, unsigned short apid, unsigned short pcrpid)
{
	dec_vpid = vpid;
	dec_apid = apid;
	dec_pcrpid = pcrpid;
}

void CSoftCSASession::setAudioPidRouting(unsigned short new_apid)
{
	/* Called from the zapit server thread. The reader thread is the sole
	 * owner of dec_apid and ts_demuxer state once start() returns — we
	 * queue the change here and let the reader apply it between reads. */
	pending_apid.store((int32_t)new_apid, std::memory_order_release);
}

void CSoftCSASession::addPid(unsigned short pid)
{
	pids.push_back(pid);
	if (demux) {
		bool ok;
		if (pids.size() == 1)
			ok = demux->pesFilter(pid);
		else
			ok = demux->addPid(pid);
		if (!ok)
			printf("[softcsa] addPid 0x%04x failed (pids.size=%zu) — "
			       "kernel filter limit reached?\n", pid, pids.size());
	}
}

bool CSoftCSASession::start(int vfd, int afd)
{
	if (running.load())
		return false;

	if (!buffer || !demux || !dec_vpid) {
		printf("[softcsa] start: missing buffer, demux, or video PID\n");
		return false;
	}

	/* dup() so we own the fds and can close them in stop() to unblock
	 * writer threads without invalidating the HAL's original fd. */
	video_fd = (vfd >= 0) ? dup(vfd) : -1;
	audio_fd = (afd >= 0) ? dup(afd) : -1;
	if (vfd >= 0 && video_fd < 0) {
		printf("[softcsa] start: dup(video_fd) failed: %s\n", strerror(errno));
		if (audio_fd >= 0) { close(audio_fd); audio_fd = -1; }
		return false;
	}

	ts_demuxer = new CTsDemuxer(dec_vpid, dec_apid);

	if (!demux->Start()) {
		printf("[softcsa] start: demux->Start() failed\n");
		if (video_fd >= 0) { close(video_fd); video_fd = -1; }
		if (audio_fd >= 0) { close(audio_fd); audio_fd = -1; }
		delete ts_demuxer;
		ts_demuxer = NULL;
		return false;
	}
	running = true;

	reader_worker = std::thread(&CSoftCSASession::readerThread, this);
	if (video_fd >= 0) {
		video_rb = new CPesRingbuffer(RINGBUFFER_SIZE);
		if (!video_rb->isValid()) {
			printf("[softcsa] start: video ringbuffer allocation failed\n");
			delete video_rb;
			video_rb = NULL;
		} else {
			video_worker = std::thread(&CSoftCSASession::videoWriterThread, this);
		}
	} else if (dec_vpid) {
		printf("[softcsa] WARNING: video_fd invalid (%d) but vpid=%04x set\n", video_fd.load(), dec_vpid);
	}
	if (audio_fd >= 0) {
		audio_rb = new CPesRingbuffer(RINGBUFFER_SIZE);
		if (!audio_rb->isValid()) {
			printf("[softcsa] start: audio ringbuffer allocation failed\n");
			delete audio_rb;
			audio_rb = NULL;
		} else {
			audio_worker = std::thread(&CSoftCSASession::audioWriterThread, this);
		}
	} else if (dec_apid) {
		printf("[softcsa] WARNING: audio_fd invalid (%d) but apid=%04x set\n", audio_fd.load(), dec_apid);
	}

	printf("[softcsa] start: demux%d, vpid=%04x apid=%04x, video_fd=%d audio_fd=%d\n",
		demux_unit, dec_vpid, dec_apid, video_fd.load(), audio_fd.load());
	return true;
}

bool CSoftCSASession::startRecord(int fd)
{
	if (running.load())
		return false;
	if (!buffer || !demux)
		return false;
	record_fd = fd;
	if (!demux->Start()) {
		printf("[softcsa] startRecord: demux->Start() failed\n");
		return false;
	}
	running = true;
	reader_worker = std::thread(&CSoftCSASession::recordThread, this);
	return true;
}

bool CSoftCSASession::startStream(SoftCSAStreamCallback cb)
{
	if (running.load())
		return false;
	if (!buffer || !demux || !cb)
		return false;
	stream_callback = cb;
	if (!demux->Start()) {
		printf("[softcsa] startStream: demux->Start() failed\n");
		return false;
	}
	running = true;
	reader_worker = std::thread(&CSoftCSASession::streamThread, this);
	return true;
}

void CSoftCSASession::stop()
{
	running = false;

	if (video_rb)
		video_rb->cancel();
	if (audio_rb)
		audio_rb->cancel();

	/* Close decoder fds to unblock writer threads that may be stuck
	 * in a blocking write() on the video/audio device.  Swap to -1
	 * atomically so the writer's next read sees -1 and won't pass
	 * a stale fd to write().  The in-flight write() on the old fd
	 * returns EBADF after close(). */
	int vfd_tmp = video_fd.exchange(-1);
	int afd_tmp = audio_fd.exchange(-1);
	if (vfd_tmp >= 0) close(vfd_tmp);
	if (afd_tmp >= 0) close(afd_tmp);

	if (reader_worker.joinable())
		reader_worker.join();
	if (video_worker.joinable())
		video_worker.join();
	if (audio_worker.joinable())
		audio_worker.join();

	/* Release the callback's captured state (e.g. a CStreamInstance*)
	 * once the reader thread has definitely stopped calling it. */
	stream_callback = nullptr;

	if (demux)
		demux->Stop();
}

void CSoftCSASession::readerThread()
{
	set_threadname("n:softcsa_rd");
	int consecutive_errors = 0;

	uint64_t stat_bytes_read = 0;
	uint32_t stat_vpes_ok = 0, stat_vpes_drop = 0;
	uint32_t stat_apes_ok = 0, stat_apes_drop = 0;
	time_t stat_last = time(NULL);

	while (running) {
		/* Apply pending audio-pid change from setAudioPidRouting().
		 * Runs single-threaded here so ts_demuxer state is safe. */
		int32_t pending = pending_apid.exchange(-1, std::memory_order_acquire);
		if (pending >= 0) {
			dec_apid = (unsigned short)pending;
			if (ts_demuxer)
				ts_demuxer->setAudioPid((unsigned short)pending);
		}

		int len = demux->Read(buffer, BUFFER_SIZE, 100);
		if (len <= 0) {
			if (len < 0) {
				consecutive_errors++;
				if (consecutive_errors >= 100) {
					printf("[softcsa] reader: %d consecutive read errors, stopping\n",
						consecutive_errors);
					break;
				}
				usleep(10000);
			}
			continue;
		}
		consecutive_errors = 0;
		stat_bytes_read += len;

		engine->descramble(buffer, len);

		ts_demuxer->process(buffer, len, [&](unsigned short pid, const uint8_t *pes, int pes_len) {
			if (pid == dec_vpid && video_rb) {
				if (video_rb->write(pes, pes_len, 100))
					stat_vpes_ok++;
				else
					stat_vpes_drop++;
			} else if (pid == dec_apid && audio_rb) {
				if (audio_rb->write(pes, pes_len, 100))
					stat_apes_ok++;
				else
					stat_apes_drop++;
			}
		});

		time_t now = time(NULL);
		if (now - stat_last >= 10) {
			time_t elapsed = now - stat_last;
			if (elapsed < 1) elapsed = 1;
			printf("[softcsa] reader stats: %lluKB/s read, vpes %u ok %u drop, apes %u ok %u drop, vrb %zu/%zuKB arb %zu/%zuKB\n",
			       (unsigned long long)(stat_bytes_read / 1024 / elapsed),
			       stat_vpes_ok, stat_vpes_drop,
			       stat_apes_ok, stat_apes_drop,
			       video_rb ? video_rb->usedBytes()/1024 : 0,
			       video_rb ? video_rb->capacityBytes()/1024 : 0,
			       audio_rb ? audio_rb->usedBytes()/1024 : 0,
			       audio_rb ? audio_rb->capacityBytes()/1024 : 0);
			stat_bytes_read = 0;
			stat_vpes_ok = stat_vpes_drop = 0;
			stat_apes_ok = stat_apes_drop = 0;
			stat_last = now;
		}
	}
}

void CSoftCSASession::videoWriterThread()
{
	set_threadname("n:softcsa_vw");

	uint8_t *pes_buf = (uint8_t *)malloc(MAX_PES_SIZE);
	if (!pes_buf)
		return;
	int consecutive_errors = 0;

	uint64_t stat_bytes_written = 0;
	uint32_t stat_stalls = 0;
	time_t stat_last = time(NULL);

	while (running && consecutive_errors < 100) {
		int len = video_rb->read(pes_buf, MAX_PES_SIZE, 100);
		if (len <= 0)
			continue;

		int written = 0;
		while (written < len) {
			int fd = video_fd.load();
			if (fd < 0) break;
			int w = ::write(fd, pes_buf + written, len - written);
			if (w < 0) {
				if (errno == EINTR) continue;
				if (errno == EAGAIN || errno == EWOULDBLOCK) { stat_stalls++; usleep(5000); continue; }
				if (!running) break;
				consecutive_errors++;
				break;
			}
			written += w;
			stat_bytes_written += w;
			consecutive_errors = 0;
		}

		time_t now = time(NULL);
		if (now - stat_last >= 10) {
			time_t elapsed = now - stat_last;
			if (elapsed < 1) elapsed = 1;
			printf("[softcsa] vwriter stats: %lluKB/s written, %u stalls\n",
			       (unsigned long long)(stat_bytes_written / 1024 / elapsed),
			       stat_stalls);
			stat_bytes_written = 0;
			stat_stalls = 0;
			stat_last = now;
		}
	}

	free(pes_buf);
}

void CSoftCSASession::audioWriterThread()
{
	set_threadname("n:softcsa_aw");

	uint8_t *pes_buf = (uint8_t *)malloc(MAX_PES_SIZE);
	if (!pes_buf)
		return;
	int consecutive_errors = 0;

	while (running && consecutive_errors < 100) {
		int len = audio_rb->read(pes_buf, MAX_PES_SIZE, 100);
		if (len <= 0)
			continue;

		int written = 0;
		while (written < len) {
			int fd = audio_fd.load();
			if (fd < 0) break;
			int w = ::write(fd, pes_buf + written, len - written);
			if (w < 0) {
				if (errno == EINTR) continue;
				if (errno == EAGAIN || errno == EWOULDBLOCK) { usleep(5000); continue; }
				if (!running) break;
				consecutive_errors++;
				break;
			}
			written += w;
			consecutive_errors = 0;
		}
	}

	free(pes_buf);
}

void CSoftCSASession::recordThread()
{
	set_threadname("n:softcsa_rec");
	while (running) {
		int len = demux->Read(buffer, BUFFER_SIZE, 100);
		if (len > 0) {
			engine->descramble(buffer, len);
			int written = 0;
			while (written < len && running) {
				int ret = ::write(record_fd, buffer + written, len - written);
				if (ret < 0) {
					if (errno == EINTR)
						continue;
					printf("[softcsa] record write error: %s\n", strerror(errno));
					break;
				}
				written += ret;
			}
		}
	}
}

void CSoftCSASession::streamThread()
{
	set_threadname("n:softcsa_str");
	while (running) {
		int len = demux->Read(buffer, BUFFER_SIZE, 100);
		if (len > 0) {
			engine->descramble(buffer, len);
			if (stream_callback)
				stream_callback(buffer, len);
		}
	}
}
