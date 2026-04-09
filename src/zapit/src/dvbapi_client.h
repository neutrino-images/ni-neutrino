/*
 * CDvbApiClient - persistent DVBAPI protocol v3 client for OSCam
 *
 * Communicates with OSCam via /tmp/camd.socket to send CA-PMT requests
 * and receive control word (CW) responses for software descrambling.
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

#ifndef __DVBAPI_CLIENT_H__
#define __DVBAPI_CLIENT_H__

#include <cstdint>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

class CSoftCSAManager; // forward declaration

class CDvbApiClient
{
public:
	CDvbApiClient();
	~CDvbApiClient();

	void setManager(CSoftCSAManager *mgr);
	bool connect();
	void disconnect();
	bool isConnected() const;
	bool ensureConnected();

	bool sendCaPmt(const unsigned char *data, unsigned int len, uint32_t msgid);

	void start(); // start reader thread
	void stop();  // stop reader thread

private:
	void readerThread();
	bool sendClientInfo();
	bool recvServerInfo();

	// Protocol v3 message parsing
	void handleCaSetDescrMode(uint32_t msgid, const uint8_t *payload, int len);
	void handleCaSetDescr(uint32_t msgid, const uint8_t *payload, int len);
	void handleDmxSetFilter(uint32_t msgid, const uint8_t *payload, int len);
	void handleDmxStop(uint32_t msgid, const uint8_t *payload, int len);

	int sock_fd;
	std::atomic<bool> running;
	std::thread reader;
	std::mutex send_mutex;
	CSoftCSAManager *manager;
	std::string server_name;
	int server_proto_version;
};

#endif
