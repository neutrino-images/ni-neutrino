/*
 * CDvbApiClient - persistent DVBAPI protocol v3 client for OSCam
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SOFTCSA

#include "dvbapi_client.h"
#include <driver/softcsa/softcsa_manager.h>
#include <zapit/capmt.h>
#include <zapit/getservices.h>
#include <zapit/settings.h>

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <poll.h>

/* DVBAPI protocol version */
#define DVBAPI_PROTOCOL_VERSION   3

/* DVBAPI opcodes — from OSCam module-dvbapi.h */
#define DVBAPI_CA_SET_DESCR       0x40106F86
#define DVBAPI_CA_SET_DESCR_MODE  0x400C6F88
#define DVBAPI_CA_SET_DESCR_DATA  0x40186F89
#define DVBAPI_DMX_SET_FILTER     0x403C6F2B
#define DVBAPI_DMX_STOP           0x00006F2A
#define DVBAPI_AOT_CA_PMT        0x9F803200
#define DVBAPI_FILTER_DATA        0xFFFF0000
#define DVBAPI_CLIENT_INFO        0xFFFF0001
#define DVBAPI_SERVER_INFO        0xFFFF0002
#define DVBAPI_ECM_INFO           0xFFFF0003

/* Protocol v3 marker byte */
#define DVBAPI_PROTO3_MARKER      0xa5

/* Client name sent during handshake */
#define DVBAPI_CLIENT_NAME        "Neutrino"

/* ca_descr_mode algo values */
#define CW_ALGO_CSA               0
#define CW_ALGO_DES               1
#define CW_ALGO_AES128            2
#define CW_ALGO_CSA_ALT           3  /* not in kernel enum, signals SoftCSA */

#define TAG "[dvbapi] "

/* Helper: read exactly 'count' bytes from fd, returns true on success */
static bool readAll(int fd, uint8_t *buf, int count)
{
	int total = 0;
	while (total < count) {
		int n = ::read(fd, buf + total, count - total);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return false;
		}
		if (n == 0)
			return false; // EOF
		total += n;
	}
	return true;
}

/* Helper: write exactly 'count' bytes to fd, returns true on success */
static bool writeAll(int fd, const uint8_t *buf, int count)
{
	int total = 0;
	while (total < count) {
		int n = ::write(fd, buf + total, count - total);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return false;
		}
		if (n == 0)
			return false;
		total += n;
	}
	return true;
}

/* Helper: read big-endian uint32 from buffer */
static inline uint32_t readU32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/* Helper: read big-endian uint16 from buffer */
static inline uint16_t readU16(const uint8_t *p)
{
	return ((uint16_t)p[0] << 8) | p[1];
}

/* Helper: write big-endian uint32 to buffer */
static inline void writeU32(uint8_t *p, uint32_t v)
{
	p[0] = (v >> 24) & 0xFF;
	p[1] = (v >> 16) & 0xFF;
	p[2] = (v >> 8) & 0xFF;
	p[3] = v & 0xFF;
}

/* Helper: write big-endian uint16 to buffer */
static inline void writeU16(uint8_t *p, uint16_t v)
{
	p[0] = (v >> 8) & 0xFF;
	p[1] = v & 0xFF;
}

CDvbApiClient::CDvbApiClient()
	: sock_fd(-1)
	, running(false)
	, manager(NULL)
	, server_proto_version(0)
{
}

CDvbApiClient::~CDvbApiClient()
{
	stop();
	disconnect();
}

void CDvbApiClient::setManager(CSoftCSAManager *mgr)
{
	manager = mgr;
}

bool CDvbApiClient::connect()
{
	if (sock_fd >= 0)
		return true;

	sock_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		printf(TAG "socket() failed: %s\n", strerror(errno));
		return false;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, CAMD_UDS_NAME, sizeof(addr.sun_path) - 1);

	if (::connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		printf(TAG "connect(%s) failed: %s\n", CAMD_UDS_NAME, strerror(errno));
		::close(sock_fd);
		sock_fd = -1;
		return false;
	}

	printf(TAG "connected to %s\n", CAMD_UDS_NAME);

	/* Perform handshake: send CLIENT_INFO, receive SERVER_INFO */
	if (!sendClientInfo()) {
		printf(TAG "failed to send CLIENT_INFO\n");
		disconnect();
		return false;
	}

	if (!recvServerInfo()) {
		printf(TAG "failed to receive SERVER_INFO\n");
		disconnect();
		return false;
	}

	printf(TAG "handshake complete: server='%s' proto=%d\n",
		server_name.c_str(), server_proto_version);

	return true;
}

void CDvbApiClient::disconnect()
{
	if (sock_fd >= 0) {
		::close(sock_fd);
		sock_fd = -1;
		printf(TAG "disconnected\n");
	}
	server_proto_version = 0;
	server_name.clear();
}

bool CDvbApiClient::isConnected() const
{
	return (sock_fd >= 0);
}

bool CDvbApiClient::ensureConnected()
{
	if (sock_fd >= 0)
		return true;

	if (running.load())
		return false;

	if (!connect())
		return false;

	start();
	return true;
}

bool CDvbApiClient::sendClientInfo()
{
	/*
	 * CLIENT_INFO is sent WITHOUT 0xa5 prefix (OSCam proto_version is 0 at this point).
	 * Format: [opcode:4 BE][proto_version:2 BE][name_len:1][name:N]
	 */
	const char *name = DVBAPI_CLIENT_NAME;
	uint8_t name_len = (uint8_t)strlen(name);
	int total = 4 + 2 + 1 + name_len;

	uint8_t buf[256];
	writeU32(buf, DVBAPI_CLIENT_INFO);
	writeU16(buf + 4, DVBAPI_PROTOCOL_VERSION);
	buf[6] = name_len;
	memcpy(buf + 7, name, name_len);

	std::lock_guard<std::mutex> lock(send_mutex);
	return writeAll(sock_fd, buf, total);
}

bool CDvbApiClient::recvServerInfo()
{
	/*
	 * SERVER_INFO is received WITH 0xa5+msgid prefix (OSCam uses proto v3 after handshake).
	 * Format: [0xa5][msgid:4][opcode:4 BE][proto_version:2 BE][name_len:1][name:N]
	 */
	uint8_t marker;
	if (!readAll(sock_fd, &marker, 1))
		return false;

	if (marker != DVBAPI_PROTO3_MARKER) {
		printf(TAG "expected 0xa5 marker, got 0x%02x\n", marker);
		return false;
	}

	uint8_t hdr[8]; /* msgid(4) + opcode(4) */
	if (!readAll(sock_fd, hdr, 8))
		return false;

	/* uint32_t msgid = readU32(hdr); */ /* not used for SERVER_INFO */
	uint32_t opcode = readU32(hdr + 4);

	if (opcode != DVBAPI_SERVER_INFO) {
		printf(TAG "expected SERVER_INFO (0x%08x), got 0x%08x\n", DVBAPI_SERVER_INFO, opcode);
		return false;
	}

	uint8_t info[3]; /* proto_version(2) + name_len(1) */
	if (!readAll(sock_fd, info, 3))
		return false;

	server_proto_version = readU16(info);
	uint8_t name_len = info[2];

	if (name_len > 0) {
		char name_buf[256];
		/* name_len is uint8_t (max 255), buf is 256 — truncation cannot happen,
		 * but keep the guard for defensive coding */
		if (name_len > sizeof(name_buf) - 1)
			name_len = sizeof(name_buf) - 1;
		if (!readAll(sock_fd, (uint8_t *)name_buf, name_len))
			return false;
		name_buf[name_len] = '\0';
		server_name = name_buf;
	} else {
		server_name = "(unknown)";
	}

	return true;
}

bool CDvbApiClient::sendCaPmt(const unsigned char *data, unsigned int len, uint32_t msgid)
{
	/* Note: sock_fd check races with readerThread's disconnect(), but a stale
	 * check only causes a failed writeAll which returns false safely. */
	if (sock_fd < 0)
		return false;

	/*
	 * CA-PMT sending (proto v3 framing):
	 * [0xa5][msgid:4][ca_pmt_apdu...]
	 *
	 * The ca_pmt_apdu from writeToBuffer() already contains the AOT tag
	 * (0x9F 80 32), ASN.1 length, and the full CA_PMT body including
	 * adapter/demux/CA-device info via private descriptors (0x83/0x86/0x87).
	 * OSCam parses the AOT tag and ASN.1 length directly after stripping
	 * the v3 prefix — no separate opcode or adapter_index byte is expected.
	 */
	uint8_t hdr[5];
	hdr[0] = DVBAPI_PROTO3_MARKER;
	writeU32(hdr + 1, msgid);

	std::lock_guard<std::mutex> lock(send_mutex);
	if (!writeAll(sock_fd, hdr, 5))
		return false;
	if (len > 0 && !writeAll(sock_fd, data, len))
		return false;

	return true;
}

void CDvbApiClient::start()
{
	if (running.load())
		return;

	running.store(true);
	reader = std::thread(&CDvbApiClient::readerThread, this);
}

void CDvbApiClient::stop()
{
	running.store(false);
	if (reader.joinable()) {
		/* closing socket will unblock the read in the reader thread */
		if (sock_fd >= 0) {
			::shutdown(sock_fd, SHUT_RDWR);
		}
		reader.join();
	}
}

void CDvbApiClient::readerThread()
{
	printf(TAG "reader thread started\n");

	while (running.load()) {
		if (sock_fd < 0) {
			printf(TAG "attempting reconnect...\n");
			if (manager)
				manager->stopSessions();
			for (int i = 0; i < 10 && running.load(); i++)
				usleep(500000);
			if (!running.load())
				break;
			if (!connect()) {
				printf(TAG "reconnect failed, retrying in 5s\n");
				continue;
			}
			printf(TAG "reconnected to OSCam\n");
			if (manager) {
				auto resub = manager->getResubscribeInfo();
				for (auto &info : resub) {
					CZapitChannel *channel = CServiceManager::getInstance()->FindChannel(info.channel_id);
					if (!channel)
						continue;
					CCam cam;
					cam.setCaMask(info.capmt_ca_mask);
					/* Match the stop and primary-send paths: a broken
					 * allocation (-1) is floored explicitly instead of
					 * relying on makeCaPmt's implicit 0x00 truncation. */
					int capmt_demux = (info.capmt_demux >= 0) ? info.capmt_demux : 0;
					cam.setSource(capmt_demux);
					cam.makeCaPmt(channel, true, CCam::CAPMT_ADD);
					if (!sendCaPmt(cam.getBuffer(), cam.getLength(), info.session_id))
						printf(TAG "resubscribe failed for channel %llx session %u\n",
						       (unsigned long long)info.channel_id, info.session_id);
					else
						printf(TAG "resubscribed channel %llx session %u\n",
						       (unsigned long long)info.channel_id, info.session_id);
				}
			}
			continue;
		}

		/* Use poll to allow periodic checks of running flag */
		struct pollfd pfd;
		pfd.fd = sock_fd;
		pfd.events = POLLIN;
		int ret = ::poll(&pfd, 1, 500); /* 500ms timeout */

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			printf(TAG "poll error: %s\n", strerror(errno));
			disconnect();
			continue;
		}

		if (ret == 0)
			continue; /* timeout, check running flag */

		if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
			printf(TAG "socket error (revents=0x%x)\n", pfd.revents);
			disconnect();
			continue;
		}

		/* Read the 0xa5 marker */
		uint8_t marker;
		if (!readAll(sock_fd, &marker, 1)) {
			printf(TAG "connection closed by server\n");
			disconnect();
			continue;
		}

		if (marker != DVBAPI_PROTO3_MARKER) {
			printf(TAG "unexpected byte 0x%02x, expected 0xa5\n", marker);
			disconnect();
			continue;
		}

		/* Read msgid (4 bytes) + opcode (4 bytes) */
		uint8_t hdr[8];
		if (!readAll(sock_fd, hdr, 8)) {
			printf(TAG "failed to read message header\n");
			disconnect();
			continue;
		}

		uint32_t msgid = readU32(hdr);
		uint32_t opcode = readU32(hdr + 4);

		/*
		 * Based on opcode, determine how much more to read.
		 * CA_SET_DESCR, CA_SET_DESCR_MODE, DMX_SET_FILTER, DMX_STOP, ECM_INFO
		 * have an adapter_index byte followed by opcode-specific payload.
		 * SERVER_INFO does NOT have adapter_index.
		 */
		switch (opcode) {
		case DVBAPI_CA_SET_DESCR: {
			/* adapter_index(1) + ca_descr_t: index(4)+parity(4)+cw(8) = 17 */
			uint8_t payload[17];
			if (!readAll(sock_fd, payload, 17)) {
				printf(TAG "failed to read CA_SET_DESCR payload\n");
				disconnect();
				break;
			}
			handleCaSetDescr(msgid, payload, 17);
			break;
		}
		case DVBAPI_CA_SET_DESCR_MODE: {
			/* adapter_index(1) + ca_descr_mode_t: index(4)+algo(4)+cipher_mode(4) = 13 */
			uint8_t payload[13];
			if (!readAll(sock_fd, payload, 13)) {
				printf(TAG "failed to read CA_SET_DESCR_MODE payload\n");
				disconnect();
				break;
			}
			handleCaSetDescrMode(msgid, payload, 13);
			break;
		}
		case DVBAPI_CA_SET_DESCR_DATA: {
			/*
			 * adapter_index(1) + ca_descr_data_t: index(4)+parity(4)+data_type(4)+length(4)+data(N)
			 * Read the fixed header first (17 bytes), extract length, then read data.
			 */
			uint8_t ddhdr[17]; /* adapter_index(1)+index(4)+parity(4)+data_type(4)+length(4) */
			if (!readAll(sock_fd, ddhdr, 17)) {
				printf(TAG "failed to read CA_SET_DESCR_DATA header\n");
				disconnect();
				break;
			}
			uint32_t data_length = readU32(ddhdr + 13);
			if (data_length > 0) {
				uint8_t discard[256];
				uint32_t remaining = data_length;
				while (remaining > 0) {
					uint32_t chunk = (remaining > sizeof(discard)) ? sizeof(discard) : remaining;
					if (!readAll(sock_fd, discard, chunk)) {
						printf(TAG "failed to read CA_SET_DESCR_DATA data (%u bytes)\n", data_length);
						disconnect();
						break;
					}
					remaining -= chunk;
				}
				if (remaining > 0)
					break; /* readAll failed above */
			}
			/* AES128 — out of scope, consume and ignore */
			break;
		}
		case DVBAPI_DMX_SET_FILTER:
		case DVBAPI_DMX_STOP:
			/* OSCam's pc_nodmx filter path — never triggered in our
			 * setup. Disconnect rather than guess the payload size
			 * against a potentially mismatched proto version. */
			printf(TAG "unexpected DMX opcode 0x%08x — disconnecting\n", opcode);
			disconnect();
			break;
		case DVBAPI_SERVER_INFO: {
			/* No adapter_index. proto_version(2)+name_len(1)+name(N) */
			uint8_t info[3];
			if (!readAll(sock_fd, info, 3)) {
				printf(TAG "failed to read SERVER_INFO\n");
				disconnect();
				break;
			}
			server_proto_version = readU16(info);
			uint8_t name_len = info[2];
			if (name_len > 0) {
				char name_buf[256]; /* uint8_t name_len <= 255, always fits */
				if (!readAll(sock_fd, (uint8_t *)name_buf, name_len)) {
					disconnect();
					break;
				}
				name_buf[name_len] = '\0';
				server_name = name_buf;
			}
			printf(TAG "SERVER_INFO update: server='%s' proto=%d\n",
				server_name.c_str(), server_proto_version);
			break;
		}
		case DVBAPI_ECM_INFO: {
			/*
			 * Has adapter_index (OSCam sends it for all opcodes except SERVER_INFO).
			 * Consume all fields and discard — we don't use ECM_INFO data.
			 *
			 * Format: adapter_index(1) + service_id(2)+caid(2)+pid(2)+prov_id(4)+ecm_time(4)
			 *         + [cardsystem_len(1)+cardsystem(N)] + [reader_len(1)+reader(N)]
			 *         + [from_len(1)+from(N)] + [protocol_len(1)+protocol(N)]
			 *         + hops(1)
			 */
			uint8_t ecm_fixed[15]; /* adapter_index(1)+service_id(2)+caid(2)+pid(2)+prov_id(4)+ecm_time(4) */
			if (!readAll(sock_fd, ecm_fixed, 15)) {
				printf(TAG "failed to read ECM_INFO fixed fields\n");
				disconnect();
				break;
			}
			bool ecm_ok = true;
			for (int i = 0; i < 4 && ecm_ok; i++) {
				uint8_t slen;
				if (!readAll(sock_fd, &slen, 1)) {
					ecm_ok = false;
					break;
				}
				if (slen > 0) {
					uint8_t discard[256];
					if (!readAll(sock_fd, discard, slen)) {
						ecm_ok = false;
						break;
					}
				}
			}
			if (!ecm_ok) {
				printf(TAG "failed to read ECM_INFO variable fields\n");
				disconnect();
				break;
			}
			uint8_t hops;
			if (!readAll(sock_fd, &hops, 1)) {
				printf(TAG "failed to read ECM_INFO hops\n");
				disconnect();
				break;
			}
			printf(TAG "ECM_INFO: msgid=0x%08x (consumed)\n", msgid);
			break;
		}
		default:
			printf(TAG "unknown opcode 0x%08x, msgid=0x%08x\n", opcode, msgid);
			/*
			 * Unknown opcode with unknown payload length — we cannot safely
			 * continue parsing the stream. Disconnect.
			 */
			disconnect();
			break;
		}
	}

	printf(TAG "reader thread stopped\n");
}

/* msgid = our session_id (OSCam reflects it opaquely). The payload
 * `index` is OSCam's usedidx, not routable across ca_mask-isolated
 * sessions — we ignore it. */
void CDvbApiClient::handleCaSetDescrMode(uint32_t msgid, const uint8_t *payload, int len)
{
	if (len < 13)
		return;

	uint32_t algo = readU32(payload + 5);
	uint32_t cipher_mode = readU32(payload + 9);

	printf(TAG "CA_SET_DESCR_MODE: msgid=0x%08x algo=%u cipher_mode=%u\n",
		msgid, algo, cipher_mode);

	if (manager)
		manager->onDescrMode(msgid, algo, cipher_mode);
}

void CDvbApiClient::handleCaSetDescr(uint32_t msgid, const uint8_t *payload, int len)
{
	if (len < 17)
		return;

	uint32_t parity = readU32(payload + 5);
	const uint8_t *cw = payload + 9;

	printf(TAG "CA_SET_DESCR: msgid=0x%08x parity=%u\n", msgid, parity);

	if (manager)
		manager->onCW(msgid, parity, cw);
}

#endif /* HAVE_SOFTCSA */
