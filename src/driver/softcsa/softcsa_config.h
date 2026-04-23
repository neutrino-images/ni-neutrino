/*
 * CSoftCSAConfig - runtime config for SoftCSA
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

#ifndef __SOFTCSA_CONFIG_H__
#define __SOFTCSA_CONFIG_H__

#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <sys/types.h>
#include <vector>

#include <zapit/channel.h>

class CSoftCSAConfig
{
public:
	enum Decision {
		DECISION_ALLOW,
		DECISION_DENY,
		DECISION_FALLTHROUGH
	};

	static CSoftCSAConfig *getInstance();

	Decision decide(CZapitChannel *channel);
	int startTimeoutMs();

private:
	CSoftCSAConfig();
	CSoftCSAConfig(const CSoftCSAConfig &) = delete;
	CSoftCSAConfig &operator=(const CSoftCSAConfig &) = delete;

	struct DvbEntry {
		uint16_t sid;
		uint16_t tsid;
		uint16_t onid;
		int32_t satpos;
		uint16_t freq;
		bool has_sid;
		bool has_tsid;
		bool has_onid;
		bool has_satpos;
		bool has_freq;
		bool include;
	};

	struct BouquetEntry {
		std::string name;
		bool include;
		/* Snapshot of channel ids resolved from the live bouquet manager at
		 * load time. decide() avoids walking the bouquet vectors under mtx,
		 * which dodges a latent race with runtime bouquet reloads. */
		std::set<t_channel_id> resolved_cids;
	};

	/* Caller must hold mtx. */
	void resetToNoFileStateLocked();
	void maybeReloadLocked();
	void loadFromFileLocked();
	void parseLineLocked(const std::string &key, const std::string &val, int lineno);
	void parseEntryLocked(const std::string &val, int lineno);
	void parseDvbEntryLocked(const std::string &body, bool include, int lineno);
	void resolveBouquetEntriesLocked();

	bool dvbMatches(const DvbEntry &e, uint16_t sid, uint16_t tsid, uint16_t onid,
	                int32_t satpos, uint16_t freq) const;
	int dvbRank(const DvbEntry &e) const;

	std::mutex mtx;

	bool has_file;
	time_t last_mtime;

	bool cfg_enabled;
	bool cfg_auto;
	int cfg_start_timeout_ms;

	std::vector<DvbEntry> dvb_entries;
	std::vector<BouquetEntry> bouquet_entries;
};

#endif
