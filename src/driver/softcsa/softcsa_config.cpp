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

#include "softcsa_config.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#include <zapit/bouquets.h>

extern CBouquetManager *g_bouquetManager;

#define CONFIG_PATH "/var/tuxbox/config/softcsa.conf"
#define TAG "[softcsa-cfg] "

namespace {
std::string trim(const std::string &s)
{
	size_t b = 0;
	while (b < s.size() && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n'))
		b++;
	size_t e = s.size();
	while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n'))
		e--;
	return s.substr(b, e - b);
}

bool parseIntStrict(const std::string &s, long &out)
{
	if (s.empty())
		return false;
	errno = 0;
	char *end = NULL;
	long v = strtol(s.c_str(), &end, 0);
	if (errno != 0 || end == s.c_str() || *end != '\0')
		return false;
	out = v;
	return true;
}
} // namespace

CSoftCSAConfig *CSoftCSAConfig::getInstance()
{
	static CSoftCSAConfig instance;
	return &instance;
}

CSoftCSAConfig::CSoftCSAConfig()
	: has_file(false), last_mtime(0),
	  cfg_enabled(true), cfg_auto(true), cfg_start_timeout_ms(3000)
{
	resetToNoFileStateLocked();
}

void CSoftCSAConfig::resetToNoFileStateLocked()
{
	has_file = false;
	last_mtime = 0;
	cfg_enabled = true;
	cfg_auto = true;
	cfg_start_timeout_ms = 3000;
	dvb_entries.clear();
	bouquet_entries.clear();
}

void CSoftCSAConfig::maybeReloadLocked()
{
	struct stat st;
	if (stat(CONFIG_PATH, &st) != 0) {
		if (has_file) {
			printf(TAG "file %s gone, falling back to no-file state\n", CONFIG_PATH);
			resetToNoFileStateLocked();
		}
		return;
	}
	if (has_file && st.st_mtime == last_mtime)
		return;
	resetToNoFileStateLocked();
	has_file = true;
	last_mtime = st.st_mtime;
	loadFromFileLocked();
}

void CSoftCSAConfig::loadFromFileLocked()
{
	FILE *fp = fopen(CONFIG_PATH, "r");
	if (!fp) {
		printf(TAG "cannot open %s: %s; feature off\n", CONFIG_PATH, strerror(errno));
		has_file = false;
		last_mtime = 0;
		return;
	}

	char buf[1024];
	int lineno = 0;
	while (fgets(buf, sizeof(buf), fp)) {
		lineno++;
		std::string line = trim(buf);
		if (line.empty() || line[0] == '#')
			continue;
		size_t eq = line.find('=');
		if (eq == std::string::npos) {
			printf(TAG "line %d: no '=' separator, skipping: %s\n", lineno, line.c_str());
			continue;
		}
		std::string key = trim(line.substr(0, eq));
		std::string val = trim(line.substr(eq + 1));
		parseLineLocked(key, val, lineno);
	}
	fclose(fp);
	resolveBouquetEntriesLocked();
	printf(TAG "loaded %s: enabled=%d auto=%d start_timeout_ms=%d dvb=%zu bouquet=%zu\n",
	       CONFIG_PATH, (int)cfg_enabled, (int)cfg_auto, cfg_start_timeout_ms,
	       dvb_entries.size(), bouquet_entries.size());
}

void CSoftCSAConfig::parseLineLocked(const std::string &key, const std::string &val, int lineno)
{
	if (key == "enabled") {
		long v;
		if (!parseIntStrict(val, v) || (v != 0 && v != 1)) {
			printf(TAG "line %d: enabled must be 0 or 1, got '%s'\n", lineno, val.c_str());
			return;
		}
		cfg_enabled = (v == 1);
		return;
	}
	if (key == "auto") {
		long v;
		if (!parseIntStrict(val, v) || (v != 0 && v != 1)) {
			printf(TAG "line %d: auto must be 0 or 1, got '%s'\n", lineno, val.c_str());
			return;
		}
		cfg_auto = (v == 1);
		return;
	}
	if (key == "start_timeout_ms") {
		long v;
		if (!parseIntStrict(val, v)) {
			printf(TAG "line %d: start_timeout_ms not an integer: '%s'\n", lineno, val.c_str());
			return;
		}
		if (v < 100) {
			printf(TAG "line %d: start_timeout_ms %ld clamped to 100\n", lineno, v);
			v = 100;
		} else if (v > 15000) {
			printf(TAG "line %d: start_timeout_ms %ld clamped to 15000\n", lineno, v);
			v = 15000;
		}
		cfg_start_timeout_ms = (int)v;
		return;
	}
	if (key == "entry") {
		parseEntryLocked(val, lineno);
		return;
	}
	printf(TAG "line %d: unknown key '%s', skipping\n", lineno, key.c_str());
}

void CSoftCSAConfig::parseEntryLocked(const std::string &val, int lineno)
{
	/* Grammar: <type>:<mode>:<body>. Body is taken verbatim after the
	 * second colon so bouquet names can contain further colons. */
	size_t c1 = val.find(':');
	if (c1 == std::string::npos) {
		printf(TAG "line %d: entry missing type, skipping: %s\n", lineno, val.c_str());
		return;
	}
	size_t c2 = val.find(':', c1 + 1);
	if (c2 == std::string::npos) {
		printf(TAG "line %d: entry missing body, skipping: %s\n", lineno, val.c_str());
		return;
	}
	std::string type = trim(val.substr(0, c1));
	std::string mode = trim(val.substr(c1 + 1, c2 - c1 - 1));
	std::string body = val.substr(c2 + 1);

	bool include;
	if (mode == "include")
		include = true;
	else if (mode == "exclude")
		include = false;
	else {
		printf(TAG "line %d: unknown mode '%s', skipping\n", lineno, mode.c_str());
		return;
	}

	if (type == "dvb") {
		parseDvbEntryLocked(body, include, lineno);
	} else if (type == "bouquet") {
		/* Bouquet names keep internal colons and trailing spaces only if
		 * the user meant them. Strip leading whitespace so `bouquet: Name`
		 * is forgiven; preserve trailing so odd names round-trip. */
		size_t b = 0;
		while (b < body.size() && (body[b] == ' ' || body[b] == '\t'))
			b++;
		std::string name = body.substr(b);
		if (name.empty()) {
			printf(TAG "line %d: bouquet name empty, skipping\n", lineno);
			return;
		}
		BouquetEntry e;
		e.name = name;
		e.include = include;
		bouquet_entries.push_back(e);
	} else {
		printf(TAG "line %d: unknown entry type '%s', skipping\n", lineno, type.c_str());
	}
}

void CSoftCSAConfig::parseDvbEntryLocked(const std::string &body, bool include, int lineno)
{
	DvbEntry e;
	e.sid = e.tsid = e.onid = e.freq = 0;
	e.satpos = 0;
	e.has_sid = e.has_tsid = e.has_onid = e.has_satpos = e.has_freq = false;
	e.include = include;

	size_t pos = 0;
	while (pos < body.size()) {
		size_t comma = body.find(',', pos);
		std::string tok = body.substr(pos, (comma == std::string::npos ? body.size() : comma) - pos);
		pos = (comma == std::string::npos ? body.size() : comma + 1);

		std::string pair = trim(tok);
		if (pair.empty())
			continue;
		size_t eq = pair.find('=');
		if (eq == std::string::npos) {
			printf(TAG "line %d: dvb token '%s' has no '=', skipping entry\n", lineno, pair.c_str());
			return;
		}
		std::string key = trim(pair.substr(0, eq));
		std::string val = trim(pair.substr(eq + 1));
		long v;
		if (!parseIntStrict(val, v)) {
			printf(TAG "line %d: dvb field %s='%s' not numeric, skipping entry\n",
			       lineno, key.c_str(), val.c_str());
			return;
		}

		if (key == "sid") {
			e.sid = (uint16_t)v; e.has_sid = true;
		} else if (key == "tsid") {
			e.tsid = (uint16_t)v; e.has_tsid = true;
		} else if (key == "onid") {
			e.onid = (uint16_t)v; e.has_onid = true;
		} else if (key == "satpos") {
			/* Spec allows two forms: signed (e.g. -300 for 30 degrees W)
			 * and the transponder-id encoding 0xF000 + abs(position).
			 * Channels store the signed form in int16_t, so normalise
			 * the transponder-id form back to signed before comparison. */
			if (v >= 0xF000 && v <= 0xFFFF) {
				long signed_v = -(v - 0xF000);
				printf(TAG "line %d: satpos 0x%04lx interpreted as signed %ld via 0xF000+abs() encoding\n",
				       lineno, v, signed_v);
				v = signed_v;
			}
			e.satpos = (int32_t)v; e.has_satpos = true;
		} else if (key == "freq") {
			e.freq = (uint16_t)v; e.has_freq = true;
		} else {
			printf(TAG "line %d: unknown dvb field '%s', skipping entry\n", lineno, key.c_str());
			return;
		}
	}

	if (!e.has_sid && !e.has_tsid && !e.has_onid && !e.has_satpos && !e.has_freq) {
		printf(TAG "line %d: dvb entry has no fields set, skipping\n", lineno);
		return;
	}
	dvb_entries.push_back(e);
}

bool CSoftCSAConfig::dvbMatches(const DvbEntry &e, uint16_t sid, uint16_t tsid,
                                uint16_t onid, int32_t satpos, uint16_t freq) const
{
	if (e.has_sid && e.sid != sid) return false;
	if (e.has_tsid && e.tsid != tsid) return false;
	if (e.has_onid && e.onid != onid) return false;
	if (e.has_satpos && e.satpos != satpos) return false;
	if (e.has_freq && e.freq != freq) return false;
	return true;
}

int CSoftCSAConfig::dvbRank(const DvbEntry &e) const
{
	/* Spec: sid > tsid+onid > freq > satpos. Rank 3 is most specific. */
	if (e.has_sid) return 3;
	if (e.has_tsid && e.has_onid) return 2;
	if (e.has_freq) return 1;
	if (e.has_satpos) return 0;
	return -1;
}

void CSoftCSAConfig::resolveBouquetEntriesLocked()
{
	for (auto &e : bouquet_entries)
		e.resolved_cids.clear();
	if (!g_bouquetManager)
		return;
	for (const CZapitBouquet *bouquet : g_bouquetManager->Bouquets) {
		if (!bouquet)
			continue;
		for (auto &e : bouquet_entries) {
			if (bouquet->Name != e.name)
				continue;
			for (const CZapitChannel *ch : bouquet->tvChannels)
				if (ch)
					e.resolved_cids.insert(ch->getChannelID());
			for (const CZapitChannel *ch : bouquet->radioChannels)
				if (ch)
					e.resolved_cids.insert(ch->getChannelID());
		}
	}
}

CSoftCSAConfig::Decision CSoftCSAConfig::decide(CZapitChannel *channel)
{
	std::lock_guard<std::mutex> lock(mtx);
	maybeReloadLocked();
	if (!has_file || !cfg_enabled || !channel)
		return DECISION_DENY;

	t_channel_id cid = channel->getChannelID();
	uint16_t sid = GET_SERVICE_ID_FROM_CHANNEL_ID(cid);
	uint16_t tsid = GET_TRANSPORT_STREAM_ID_FROM_CHANNEL_ID(cid);
	uint16_t onid = GET_ORIGINAL_NETWORK_ID_FROM_CHANNEL_ID(cid);
	int32_t satpos = (int32_t)channel->getSatellitePosition();
	uint16_t freq = (uint16_t)channel->getFreqId();

	/* DVB level: find highest-ranked matching entry; ties resolve to exclude. */
	int best_rank = -1;
	bool best_include = false;
	bool best_exclude = false;
	for (const auto &e : dvb_entries) {
		if (!dvbMatches(e, sid, tsid, onid, satpos, freq))
			continue;
		int rank = dvbRank(e);
		if (rank < 0 || rank < best_rank)
			continue;
		if (rank > best_rank) {
			best_rank = rank;
			best_include = e.include;
			best_exclude = !e.include;
		} else {
			if (e.include) best_include = true;
			else best_exclude = true;
		}
	}
	if (best_rank >= 0) {
		if (best_exclude)
			return DECISION_DENY;
		if (best_include)
			return DECISION_ALLOW;
	}

	/* Bouquet level, only if no DVB entry matched. Tie-break: exclude wins.
	 * Membership is checked against the snapshot built at config load time,
	 * not the live bouquet manager, so we do not walk bouquet vectors under
	 * mtx and avoid a latent race with runtime bouquet reloads. */
	bool bq_include = false, bq_exclude = false;
	for (const auto &b : bouquet_entries) {
		if (b.resolved_cids.find(cid) == b.resolved_cids.end())
			continue;
		if (b.include) bq_include = true;
		else bq_exclude = true;
	}
	if (bq_exclude)
		return DECISION_DENY;
	if (bq_include)
		return DECISION_ALLOW;

	/* Fallthrough: auto=1 keeps today's behaviour, auto=0 denies. */
	return cfg_auto ? DECISION_FALLTHROUGH : DECISION_DENY;
}

int CSoftCSAConfig::startTimeoutMs()
{
	std::lock_guard<std::mutex> lock(mtx);
	maybeReloadLocked();
	return cfg_start_timeout_ms;
}
