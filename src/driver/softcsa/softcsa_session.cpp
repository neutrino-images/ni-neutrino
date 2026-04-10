/*
 * CSoftCSASession - single SoftCSA descrambling session (live/record)
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

bool CSoftCSASession::addReaderPid(unsigned short pid)
{
	if (!demux)
		return false;
	if (!demux->addPid(pid)) {
		printf("[softcsa] addReaderPid %04x failed\n", pid);
		return false;
	}
	return true;
}

bool CSoftCSASession::start(int vfd, int afd)
{
	if (running.load())
		return false;

	if (!buffer || !demux || !dec_vpid) {
		printf("[softcsa] start: missing buffer, demux, or video PID\n");
		return false;
	}

	video_fd = vfd;
	audio_fd = afd;

	ts_demuxer = new CTsDemuxer(dec_vpid, dec_apid);

	demux->Start();
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
		printf("[softcsa] WARNING: video_fd invalid (%d) but vpid=%04x set\n", video_fd, dec_vpid);
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
		printf("[softcsa] WARNING: audio_fd invalid (%d) but apid=%04x set\n", audio_fd, dec_apid);
	}

	printf("[softcsa] start: demux%d, vpid=%04x apid=%04x, video_fd=%d audio_fd=%d\n",
		demux_unit, dec_vpid, dec_apid, video_fd, audio_fd);
	return true;
}

bool CSoftCSASession::startRecord(int fd)
{
	if (running.load())
		return false;
	if (!buffer || !demux)
		return false;
	record_fd = fd;
	demux->Start();
	running = true;
	reader_worker = std::thread(&CSoftCSASession::recordThread, this);
	return true;
}

void CSoftCSASession::stop()
{
	running = false;

	if (video_rb)
		video_rb->cancel();
	if (audio_rb)
		audio_rb->cancel();

	if (reader_worker.joinable())
		reader_worker.join();
	if (video_worker.joinable())
		video_worker.join();
	if (audio_worker.joinable())
		audio_worker.join();

	if (demux)
		demux->Stop();
}

void CSoftCSASession::readerThread()
{
	set_threadname("n:softcsa_rd");
	int consecutive_errors = 0;

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

		engine->descramble(buffer, len);

		ts_demuxer->process(buffer, len, [&](unsigned short pid, const uint8_t *pes, int pes_len) {
			if (pid == dec_vpid && video_rb)
				video_rb->write(pes, pes_len, 100);
			else if (pid == dec_apid && audio_rb)
				audio_rb->write(pes, pes_len, 100);
		});
	}
}

void CSoftCSASession::videoWriterThread()
{
	set_threadname("n:softcsa_vw");

	uint8_t *pes_buf = (uint8_t *)malloc(MAX_PES_SIZE);
	if (!pes_buf)
		return;
	int consecutive_errors = 0;

	while (running && consecutive_errors < 100) {
		int len = video_rb->read(pes_buf, MAX_PES_SIZE, 100);
		if (len <= 0)
			continue;

		int written = 0;
		while (written < len) {
			int w = ::write(video_fd, pes_buf + written, len - written);
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
			int w = ::write(audio_fd, pes_buf + written, len - written);
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
