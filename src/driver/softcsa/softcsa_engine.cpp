/*
 * CSoftCSAEngine - DVB-CSA descrambling engine using libdvbcsa
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

#include "softcsa_engine.h"
#include <cstdio>
#include <cstring>

// TS packet constants
#define TS_PACKET_SIZE     188
#define TS_SYNC_BYTE       0x47
#define TS_HDR_PAYLOAD_OFF 4
#define TS_TSC_MASK        0xC0
#define TS_TSC_EVEN        0x80
#define TS_TSC_ODD         0xC0
#define TS_ADAPT_FIELD     0x20

CSoftCSAEngine::CSoftCSAEngine()
	: key_even_idx(0)
	, key_odd_idx(0)
	, stored_ecm_mode(0)
{
	memset(key_even, 0, sizeof(key_even));
	memset(key_odd, 0, sizeof(key_odd));
	memset(stored_cw, 0, sizeof(stored_cw));
	for (int i = 0; i < 2; i++)
	{
		key_even[i] = dvbcsa_bs_key_alloc();
		key_odd[i] = dvbcsa_bs_key_alloc();
		if (!key_even[i] || !key_odd[i]) {
			printf("[softcsa] dvbcsa_bs_key_alloc failed\n");
			/* Clean up any already-allocated keys */
			for (int j = 0; j <= i; j++) {
				dvbcsa_bs_key_free(key_even[j]);
				dvbcsa_bs_key_free(key_odd[j]);
				key_even[j] = NULL;
				key_odd[j] = NULL;
			}
			even_batch = NULL;
			odd_batch = NULL;
			batch_size = 0;
			return;
		}
	}
	batch_size = dvbcsa_bs_batch_size();
	even_batch = new struct dvbcsa_bs_batch_s[batch_size + 1];
	odd_batch = new struct dvbcsa_bs_batch_s[batch_size + 1];
}

CSoftCSAEngine::~CSoftCSAEngine()
{
	for (int i = 0; i < 2; i++)
	{
		dvbcsa_bs_key_free(key_even[i]);
		dvbcsa_bs_key_free(key_odd[i]);
	}
	delete[] even_batch;  /* safe if NULL (constructor error path sets to NULL) */
	delete[] odd_batch;
}

void CSoftCSAEngine::setKey(int parity, uint8_t ecm_mode, const uint8_t *cw)
{
	/* OSCam's cipher_mode (from CA_SET_DESCR_MODE) does NOT correspond to
	 * libdvbcsa's ecm parameter. Enigma2 ignores cipher_mode entirely and
	 * extracts ecm_mode from the ECM section body (last byte & 0x0F).
	 * Enigma2's default is 0x04, which triggers the block cipher key
	 * permutation in dvbcsa_bs_key_set_ecm(). Use 0x04 unconditionally
	 * until we implement an ECM section monitor. */
	const uint8_t libdvbcsa_ecm = 4;

	if (parity == 0 || parity == 1) {
		std::lock_guard<std::mutex> lock(stored_cw_mtx);
		memcpy(stored_cw[parity], cw, 8);
		stored_ecm_mode = ecm_mode;
	}

	if (parity == 0) // even
	{
		int active = key_even_idx.load(std::memory_order_relaxed);
		int inactive = 1 - active;
		if (!key_even[inactive])
			return;
		dvbcsa_bs_key_set_ecm(libdvbcsa_ecm, cw, key_even[inactive]);
		key_even_idx.store(inactive, std::memory_order_release);
		key_even_set.store(true, std::memory_order_release);
	}
	else // odd
	{
		int active = key_odd_idx.load(std::memory_order_relaxed);
		int inactive = 1 - active;
		if (!key_odd[inactive])
			return;
		dvbcsa_bs_key_set_ecm(libdvbcsa_ecm, cw, key_odd[inactive]);
		key_odd_idx.store(inactive, std::memory_order_release);
		key_odd_set.store(true, std::memory_order_release);
	}
}

void CSoftCSAEngine::copyKeysTo(CSoftCSAEngine *dst)
{
	uint8_t cw_copy[2][8];
	uint8_t ecm_copy;
	{
		std::lock_guard<std::mutex> lock(stored_cw_mtx);
		memcpy(cw_copy, stored_cw, sizeof(cw_copy));
		ecm_copy = stored_ecm_mode;
	}
	if (key_even_set.load(std::memory_order_acquire))
		dst->setKey(0, ecm_copy, cw_copy[0]);
	if (key_odd_set.load(std::memory_order_acquire))
		dst->setKey(1, ecm_copy, cw_copy[1]);
}

int CSoftCSAEngine::descramble(uint8_t *data, int len)
{
	if (!even_batch || !odd_batch || batch_size == 0)
		return 0;

	/* Snapshot key availability — once per call, not per packet.
	 * Before setKey() is called, key buffers contain uninitialized data
	 * from dvbcsa_bs_key_alloc(). Descrambling with garbage keys produces
	 * corrupted output that the decoder misinterprets as valid TS.
	 * Instead, packets without a valid key are converted to null packets. */
	const bool even_set = key_even_set.load(std::memory_order_acquire);
	const bool odd_set = key_odd_set.load(std::memory_order_acquire);
	struct dvbcsa_bs_key_s *k_even = even_set ? key_even[key_even_idx.load(std::memory_order_acquire)] : nullptr;
	struct dvbcsa_bs_key_s *k_odd = odd_set ? key_odd[key_odd_idx.load(std::memory_order_acquire)] : nullptr;

	unsigned int even_count = 0;
	unsigned int odd_count = 0;
	int total = 0;

	for (int offset = 0; offset + TS_PACKET_SIZE <= len; offset += TS_PACKET_SIZE)
	{
		uint8_t *pkt = data + offset;

		if (pkt[0] != TS_SYNC_BYTE)
			continue;

		uint8_t tsc = pkt[3] & TS_TSC_MASK;
		if (tsc == 0)
			continue; // not scrambled — PSI, ECM, clear streams pass through

		// Calculate payload offset (skip adaptation field if present)
		int payload_off = TS_HDR_PAYLOAD_OFF;
		if (pkt[3] & TS_ADAPT_FIELD)
			payload_off += 1 + pkt[4]; // 1 byte adapt length + adapt data

		if (payload_off >= TS_PACKET_SIZE)
			continue; // no payload

		if (tsc == TS_TSC_EVEN)
		{
			if (k_even)
			{
				int payload_len = TS_PACKET_SIZE - payload_off;
				even_batch[even_count].data = pkt + payload_off;
				even_batch[even_count].len = payload_len;
				even_count++;
				total++;

				if (even_count >= batch_size)
				{
					even_batch[even_count].data = NULL;
					dvbcsa_bs_decrypt(k_even, even_batch, TS_PACKET_SIZE - TS_HDR_PAYLOAD_OFF);
					even_count = 0;
				}
				/* Clear TSC bits after adding to batch. Safe because dvbcsa_bs_decrypt
				 * operates on payload data pointers, not TS headers. */
				pkt[3] &= ~TS_TSC_MASK;
			}
			else
			{
				// No even key yet — convert to null packet
				pkt[1] = 0x1F;
				pkt[2] = 0xFF;
				pkt[3] &= ~TS_TSC_MASK;
			}
		}
		else // TS_TSC_ODD
		{
			if (k_odd)
			{
				int payload_len = TS_PACKET_SIZE - payload_off;
				odd_batch[odd_count].data = pkt + payload_off;
				odd_batch[odd_count].len = payload_len;
				odd_count++;
				total++;

				if (odd_count >= batch_size)
				{
					odd_batch[odd_count].data = NULL;
					dvbcsa_bs_decrypt(k_odd, odd_batch, TS_PACKET_SIZE - TS_HDR_PAYLOAD_OFF);
					odd_count = 0;
				}
				/* Clear TSC bits — see even-key comment above */
				pkt[3] &= ~TS_TSC_MASK;
			}
			else
			{
				// No odd key yet — convert to null packet
				pkt[1] = 0x1F;
				pkt[2] = 0xFF;
				pkt[3] &= ~TS_TSC_MASK;
			}
		}
	}

	// Flush remaining packets
	if (even_count > 0)
	{
		even_batch[even_count].data = NULL;
		dvbcsa_bs_decrypt(k_even, even_batch, TS_PACKET_SIZE - TS_HDR_PAYLOAD_OFF);
	}
	if (odd_count > 0)
	{
		odd_batch[odd_count].data = NULL;
		dvbcsa_bs_decrypt(k_odd, odd_batch, TS_PACKET_SIZE - TS_HDR_PAYLOAD_OFF);
	}

	return total;
}
