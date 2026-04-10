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

#ifndef __SOFTCSA_SESSION_H__
#define __SOFTCSA_SESSION_H__

#include <cstdint>
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
	SOFTCSA_SESSION_RECORD
};

class CSoftCSASession
{
public:
	CSoftCSASession(SoftCSASessionType type, int adapter, int demux_unit, int frontend_num);
	~CSoftCSASession();

	CSoftCSAEngine *getEngine() { return engine; }

	void addPid(unsigned short pid);
	void setDecoderPids(unsigned short vpid, unsigned short apid, unsigned short pcrpid);
	bool addReaderPid(unsigned short pid);

	/* Switch which audio PID is routed to the memory decoder at runtime
	 * (audio language change). Assumes the new PID is already in the
	 * SoftCSA cDemux TAP — all audio PIDs of the channel are added by
	 * capmt at session setup. */
	void setAudioPidRouting(unsigned short new_apid);

	/* LIVE: start memory-source injection (3 threads) */
	bool start(int video_fd, int audio_fd);
	/* RECORD: start descramble-to-file (standalone, without LIVE) */
	bool startRecord(int fd);
	void stop();

	bool isRunning() const { return running.load(); }
	SoftCSASessionType getType() const { return session_type; }
	int getAdapterNum() const { return adapter_num; }

private:
	void readerThread();
	void videoWriterThread();
	void audioWriterThread();
	void recordThread();

	SoftCSASessionType session_type;
	CSoftCSAEngine *engine;
	CTsDemuxer *ts_demuxer;
	cDemux *demux;

	CPesRingbuffer *video_rb;
	CPesRingbuffer *audio_rb;

	int video_fd;    /* video0 in MEMORY mode (LIVE only, not owned) */
	int audio_fd;    /* audio0 in MEMORY mode (LIVE only, not owned) */
	int record_fd;   /* recording output (RECORD only, not owned) */

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
	static const int BUFFER_SIZE = 512 * 1024;         /* TS read buffer — 512KB for 2160p bitrates up to 50 Mbit/s */
	static const size_t RINGBUFFER_SIZE = 4 * 1024 * 1024; /* PES ringbuffer per stream — must fit several 2160p I-frames */
	static const int MAX_PES_SIZE = 2 * 1024 * 1024;   /* max single PES packet — 2160p H.265 I-frames can reach ~1.5MB */

	std::vector<unsigned short> pids;

	std::thread reader_worker;
	std::thread video_worker;
	std::thread audio_worker;
	std::atomic<bool> running;
};

#endif
