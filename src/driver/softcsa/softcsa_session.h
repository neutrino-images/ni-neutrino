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

#ifndef __SOFTCSA_SESSION_H__
#define __SOFTCSA_SESSION_H__

#include <cstdint>
#include <functional>
#include <vector>
#include <thread>
#include <atomic>

class CSoftCSAEngine;
class CTsDemuxer;
class CPesRingbuffer;
class cDemux;

enum SoftCSASessionType {
	SOFTCSA_SESSION_LIVE,
	SOFTCSA_SESSION_PIP,
	SOFTCSA_SESSION_RECORD,
	SOFTCSA_SESSION_STREAM
};

typedef std::function<void(const uint8_t *data, int len)> SoftCSAStreamCallback;

class CSoftCSASession
{
public:
	CSoftCSASession(SoftCSASessionType type, int adapter, int demux_unit, int frontend_num);
	~CSoftCSASession();

	CSoftCSAEngine *getEngine() { return engine; }

	void addPid(unsigned short pid);
	void setDecoderPids(unsigned short vpid, unsigned short apid, unsigned short pcrpid);

	/* Switch which audio PID is routed to the memory decoder at runtime
	 * (audio language change). Assumes the new PID is already in the
	 * SoftCSA cDemux TAP — all audio PIDs of the channel are added by
	 * capmt at session setup. */
	void setAudioPidRouting(unsigned short new_apid);

	/* LIVE: start memory-source injection (3 threads) */
	bool start(int video_fd, int audio_fd);
	/* RECORD: start descramble-to-file (standalone, without LIVE) */
	bool startRecord(int fd);
	/* STREAM: start descramble-to-callback (standalone, without LIVE) */
	bool startStream(SoftCSAStreamCallback cb);
	void stop();

	bool isRunning() const { return running.load(); }
	SoftCSASessionType getType() const { return session_type; }
	int getAdapterNum() const { return adapter_num; }

private:
	void readerThread();
	void videoWriterThread();
	void audioWriterThread();
	void recordThread();
	void streamThread();

	SoftCSASessionType session_type;
	CSoftCSAEngine *engine;
	CTsDemuxer *ts_demuxer;
	cDemux *demux;

	CPesRingbuffer *video_rb;
	CPesRingbuffer *audio_rb;

	std::atomic<int> video_fd;    /* duped from HAL fd — owned, closed in stop() */
	std::atomic<int> audio_fd;    /* duped from HAL fd — owned, closed in stop() */
	int record_fd;   /* recording output (RECORD only, not owned) */
	SoftCSAStreamCallback stream_callback; /* STREAM only */

	int adapter_num;
	int demux_unit;
	int frontend_num;

	/* dec_vpid / dec_apid / dec_pcrpid are only touched by the reader
	 * thread after start() returns. Cross-thread APID changes are queued
	 * via pending_apid and applied by the reader thread — never written
	 * directly from the caller side. -1 = no pending change. */
	unsigned short dec_vpid;
	unsigned short dec_apid;
	unsigned short dec_pcrpid;
	std::atomic<int32_t> pending_apid;

	uint8_t *buffer;
	static const int READ_BATCH_SIZE = 256 * 188;          /* TS read batch — 256 TS packets (~47KB). Small batch catches CW key rotations quickly */
	static const int KERNEL_BUFFER_SIZE = 8192 * 188;      /* kernel demux buffer — 8K TS packets (~1.5MB). Tracks Enigma2's eDVBTSRecorder sizing; the 3MB predecessor burdened the kernel allocator without bitrate benefit */
	static const size_t RINGBUFFER_SIZE = 4 * 1024 * 1024; /* PES ringbuffer per stream — must fit several 2160p I-frames */
	static const int MAX_PES_SIZE = 2 * 1024 * 1024;   /* max single PES packet — 2160p H.265 I-frames can reach ~1.5MB */

	std::vector<unsigned short> pids;

	std::thread reader_worker;
	std::thread video_worker;
	std::thread audio_worker;
	std::atomic<bool> running;
};

#endif
