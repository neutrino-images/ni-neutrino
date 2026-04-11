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

#ifndef __SOFTCSA_ENGINE_H__
#define __SOFTCSA_ENGINE_H__

#include <cstdint>
#include <atomic>

extern "C" {
#include <dvbcsa/dvbcsa.h>
}

class CSoftCSAEngine
{
public:
	CSoftCSAEngine();
	~CSoftCSAEngine();

	// Set control word. Called from the CDvbApiClient reader thread via
	// CSoftCSAManager::onCW, and via copyKeysTo from the zapit server
	// thread at same-channel session creation.
	// parity: 0=even, 1=odd
	void setKey(int parity, uint8_t ecm_mode, const uint8_t *cw);

	// Descramble TS packets in buffer. Called from the session reader
	// thread (LIVE/PIP) or the dedicated record/stream thread.
	// Returns number of packets descrambled
	int descramble(uint8_t *data, int len);

private:
	struct dvbcsa_bs_key_s *key_even[2];
	struct dvbcsa_bs_key_s *key_odd[2];
	std::atomic<int> key_even_idx;
	std::atomic<int> key_odd_idx;
	std::atomic<bool> key_even_set{false};
	std::atomic<bool> key_odd_set{false};
	unsigned int batch_size;
	struct dvbcsa_bs_batch_s *even_batch;
	struct dvbcsa_bs_batch_s *odd_batch;

	/* Stored raw CW for key transfer to RECORD sessions */
	uint8_t stored_cw[2][8]; /* [0]=even, [1]=odd */
	uint8_t stored_ecm_mode;

public:
	/* Copy current keys to another engine (for same-channel recording) */
	void copyKeysTo(CSoftCSAEngine *dst);
};

#endif
