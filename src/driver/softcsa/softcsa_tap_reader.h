/*
 * CTapReader - scrambled TS frontend tap with inline descrambling
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
 
// Reads scrambled TS from a frontend tap demux, calls
// engine->descramble() in place, and broadcasts the result to a
// COutputFdSet. One CTapReader per (frontend, service).

#ifndef __SOFTCSA_TAP_READER_H__
#define __SOFTCSA_TAP_READER_H__

#include <atomic>
#include <cstdint>
#include <mutex>
#include <set>
#include <thread>

class CSoftCSAEngine;
class COutputFdSet;

class CTapReader
{
public:
	CTapReader(int adapter, int demux_unit, int frontend_num,
	           CSoftCSAEngine *engine, COutputFdSet *outputs);
	~CTapReader();

	void addPid(uint16_t pid);
	void removePid(uint16_t pid);

	int start();   // open tap demux, install PES filters, spawn thread
	void stop();   // signal thread exit and close tap demux

private:
	int m_adapter;
	int m_demux_unit;
	int m_frontend_num;
	CSoftCSAEngine *m_engine;
	COutputFdSet *m_outputs;

	int m_demux_fd;
	std::set<uint16_t> m_pids;
	std::mutex m_pid_mtx;

	std::atomic<bool> m_running;
	std::thread m_thread;

	int openTapAndConfigure();
	void readerLoop();
};

#endif
