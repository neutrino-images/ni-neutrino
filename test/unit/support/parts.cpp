/*
 * parts.cpp - one suite run as several processes and judged as one
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

// The listener base is only declared to a file that asks for it.
#define CATCH_CONFIG_EXTERNAL_INTERFACES
#include "catch.hpp"

#include "answers.h"
#include "counts.h"
#include "parts.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <sys/stat.h>

namespace
{

struct Ran
{
	std::vector<std::string>              names;
	std::map<std::string, double>         seconds;
	std::chrono::steady_clock::time_point started;
};

Ran &ran()
{
	static Ran r;
	return r;
}

// The same file reads the same from an in-tree and an out-of-tree build.
std::string fileKey(const std::string &file)
{
	const std::string mark = "test/unit/";
	const size_t at = file.rfind(mark);
	return at == std::string::npos ? file : file.substr(at + mark.size());
}

struct PartListener : Catch::TestEventListenerBase
{
	using TestEventListenerBase::TestEventListenerBase;

	void testCaseStarting(Catch::TestCaseInfo const &info) override
	{
		TestEventListenerBase::testCaseStarting(info);
		ran().names.push_back(info.name);
		ran().started = std::chrono::steady_clock::now();
	}

	void testCaseEnded(Catch::TestCaseStats const &stats) override
	{
		const std::chrono::duration<double> took = std::chrono::steady_clock::now() - ran().started;
		ran().seconds[fileKey(stats.testInfo.lineInfo.file)] += took.count();
		TestEventListenerBase::testCaseEnded(stats);
	}
};

std::string escape(const std::string &s)
{
	std::string out;
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] == '\\')
			out += "\\\\";
		else if (s[i] == '\t')
			out += "\\t";
		else if (s[i] == '\n')
			out += "\\n";
		else
			out += s[i];
	}
	return out;
}

std::string unescape(const std::string &s)
{
	std::string out;
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] != '\\' || i + 1 == s.size())
		{
			out += s[i];
			continue;
		}
		++i;
		out += s[i] == 't' ? '\t' : s[i] == 'n' ? '\n' : s[i];
	}
	return out;
}

std::vector<std::string> fields(const std::string &line)
{
	std::vector<std::string> out;
	size_t from = 0;
	for (;;)
	{
		const size_t tab = line.find('\t', from);
		out.push_back(unescape(line.substr(from, tab == std::string::npos ? std::string::npos : tab - from)));
		if (tab == std::string::npos)
			return out;
		from = tab + 1;
	}
}

bool readLine(FILE *f, std::string &line)
{
	line.clear();
	int c;
	while ((c = std::fgetc(f)) != EOF && c != '\n')
		line += (char) c;
	return c != EOF || !line.empty();
}

// Strictly decimal and in range, so "3x" or "0" is refused rather than read as something.
bool number(const std::string &s, size_t &out)
{
	if (s.empty() || s.size() > 9)
		return false;
	out = 0;
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] < '0' || s[i] > '9')
			return false;
		out = out * 10 + (size_t)(s[i] - '0');
	}
	return true;
}

std::string statePath(size_t k)
{
	char name[32];
	std::snprintf(name, sizeof(name), "/part-%02lu.state", (unsigned long) k);
	return std::string(COREAPI_PART_DIR) + name;
}

/* Which build of the program wrote a record, so a merge never judges a part
   left over from an earlier binary beside parts of this one. */
std::string binaryIdentity()
{
	struct stat st;
	if (::stat("/proc/self/exe", &st) != 0)
		return "unknown";
	char id[96];
	std::snprintf(id, sizeof(id), "%lu:%lu:%ld:%ld", (unsigned long) st.st_ino, (unsigned long) st.st_size,
		      (long) st.st_mtim.tv_sec, (long) st.st_mtim.tv_nsec);
	return id;
}

struct SourceFile
{
	std::string              key;
	std::string              tag;
	std::vector<std::string> names;
	double                   weight;
};

// Catch's own spelling of the tag -# gives a file: its name without directory or extension.
std::string fileTag(const std::string &file)
{
	std::string base = file.substr(file.find_last_of("\\/") == std::string::npos ? 0 : file.find_last_of("\\/") + 1);
	const size_t dot = base.find_last_of('.');
	if (dot != std::string::npos)
		base.erase(dot);
	return "#" + base;
}

/* The seconds each file took last time it was measured. Only a hint for how the
   files are dealt out: a file it does not name still runs, it only counts as
   short. */
std::map<std::string, double> lastSeconds()
{
	std::map<std::string, double> out;
	FILE *f = std::fopen(COREAPI_PART_SECONDS, "r");
	if (f == NULL)
	{
		std::fprintf(stderr, "%s cannot be read, the parts are dealt out by case count\n",
			     COREAPI_PART_SECONDS);
		return out;
	}
	std::string line;
	while (readLine(f, line))
	{
		if (line.empty() || line[0] == '#')
			continue;
		const std::vector<std::string> v = fields(line);
		size_t ms = 0;
		if (v.size() == 2 && number(v[1], ms))
			out[v[0]] = ms / 1000.0;
	}
	std::fclose(f);
	return out;
}

// Every case a plain run would run, by file. Hidden ones are only ever run by name.
bool sourceFiles(std::vector<SourceFile> &out)
{
	const std::map<std::string, double> last = lastSeconds();
	std::map<std::string, size_t> at;
	const std::vector<Catch::TestCase> &all = Catch::getRegistryHub().getTestCaseRegistry().getAllTests();
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].isHidden())
			continue;
		const std::string key = fileKey(all[i].lineInfo.file);
		if (at.find(key) == at.end())
		{
			at[key] = out.size();
			SourceFile s;
			s.key = key;
			s.tag = fileTag(all[i].lineInfo.file);
			s.weight = 0;
			out.push_back(s);
		}
		out[at[key]].names.push_back(all[i].name);
	}

	std::set<std::string> tags;
	for (size_t i = 0; i < out.size(); ++i)
	{
		std::string lower = out[i].tag;
		std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
		if (!tags.insert(lower).second)
		{
			std::fprintf(stderr, "two files are named %s, and a part picks its cases by that name\n",
				     out[i].tag.c_str() + 1);
			return false;
		}
		std::map<std::string, double>::const_iterator l = last.find(out[i].key);
		// A millisecond a case on top, so files measured at nothing still spread out.
		out[i].weight = (l != last.end() ? l->second : 0.01 * out[i].names.size()) + 0.001 * out[i].names.size();
	}
	return true;
}

/* Longest first, each onto the part with the least so far. Ties go by name, so
   the same tree always deals the same parts. */
std::vector<std::vector<size_t> > deal(const std::vector<SourceFile> &files, size_t n)
{
	std::vector<size_t> order;
	for (size_t i = 0; i < files.size(); ++i)
		order.push_back(i);
	for (size_t i = 1; i < order.size(); ++i)
	{
		for (size_t j = i; j > 0; --j)
		{
			const SourceFile &a = files[order[j - 1]];
			const SourceFile &b = files[order[j]];
			if (a.weight > b.weight || (a.weight == b.weight && a.key < b.key))
				break;
			std::swap(order[j - 1], order[j]);
		}
	}

	std::vector<std::vector<size_t> > parts(n);
	std::vector<double> load(n, 0);
	for (size_t i = 0; i < order.size(); ++i)
	{
		size_t least = 0;
		for (size_t p = 1; p < n; ++p)
		{
			if (load[p] < load[least])
				least = p;
		}
		parts[least].push_back(order[i]);
		load[least] += files[order[i]].weight;
	}
	return parts;
}

bool partOf(const std::string &which, size_t &k, size_t &n)
{
	const size_t slash = which.find('/');
	return slash != std::string::npos && number(which.substr(0, slash), k) &&
	       number(which.substr(slash + 1), n) && n > 0 && k > 0 && k <= n;
}

void writeState(FILE *f, size_t k, size_t n, int failed)
{
	std::fprintf(f, "part\t%lu\t%lu\nbinary\t%s\nfailed\t%d\n", (unsigned long) k, (unsigned long) n,
		     escape(binaryIdentity()).c_str(), failed);
	for (size_t i = 0; i < ran().names.size(); ++i)
		std::fprintf(f, "case\t%s\n", escape(ran().names[i]).c_str());

	std::map<std::string, size_t> measured, clashed;
	recordedCounts(measured, clashed);
	for (std::map<std::string, size_t>::const_iterator it = measured.begin(); it != measured.end(); ++it)
		std::fprintf(f, "count\t%s\t%lu\n", escape(it->first).c_str(), (unsigned long) it->second);
	for (std::map<std::string, size_t>::const_iterator it = clashed.begin(); it != clashed.end(); ++it)
		std::fprintf(f, "clash\t%s\t%lu\n", escape(it->first).c_str(), (unsigned long) it->second);

	std::vector<SeenAnswer> seen;
	std::set<std::string> rules, examples;
	answerState(seen, rules, examples);
	for (size_t i = 0; i < seen.size(); ++i)
		std::fprintf(f, "seen\t%lu\t%lu\t%d\t%s\t%s\n", (unsigned long) seen[i].table,
			     (unsigned long) seen[i].index, seen[i].http, escape(seen[i].code).c_str(),
			     escape(seen[i].detail).c_str());
	for (std::set<std::string>::const_iterator it = rules.begin(); it != rules.end(); ++it)
		std::fprintf(f, "rule\t%s\n", escape(*it).c_str());
	for (std::set<std::string>::const_iterator it = examples.begin(); it != examples.end(); ++it)
		std::fprintf(f, "example\t%s\n", escape(*it).c_str());
	for (std::map<std::string, double>::const_iterator it = ran().seconds.begin(); it != ran().seconds.end(); ++it)
		std::fprintf(f, "seconds\t%s\t%lu\n", escape(it->first).c_str(), (unsigned long) (it->second * 1000));
	std::fprintf(f, "end\n");
}

struct PartRecord
{
	size_t                        k;
	size_t                        n;
	int                           failed;
	std::string                   binary;
	std::vector<std::string>      cases;
	std::vector<std::pair<std::string, size_t> > counts;
	std::vector<SeenAnswer>       seen;
	std::set<std::string>         rules;
	std::set<std::string>         examples;
	std::map<std::string, size_t> ms;
};

// False, with what is wrong in why, for a record that stops short or carries a line it does not know.
bool readState(const std::string &path, PartRecord &r, std::string &why)
{
	FILE *f = std::fopen(path.c_str(), "r");
	if (f == NULL)
	{
		why = "left no record";
		return false;
	}
	bool ended = false, headed = false, ok = true, failed = false, binary = false;
	std::string line;
	while (ok && !ended && readLine(f, line))
	{
		const std::vector<std::string> v = fields(line);
		size_t a = 0, b = 0;
		if (!headed)
		{
			ok = v.size() == 3 && v[0] == "part" && number(v[1], r.k) && number(v[2], r.n);
			headed = true;
		}
		else if (v[0] == "failed" && v.size() == 2)
		{
			r.failed = std::atoi(v[1].c_str());
			failed = true;
		}
		else if (v[0] == "binary" && v.size() == 2)
		{
			r.binary = v[1];
			binary = true;
		}
		else if (v[0] == "case" && v.size() == 2)
			r.cases.push_back(v[1]);
		else if ((v[0] == "count" || v[0] == "clash") && v.size() == 3 && number(v[2], a))
			r.counts.push_back(std::make_pair(v[1], a));
		else if (v[0] == "seen" && v.size() == 6 && number(v[1], a) && number(v[2], b))
		{
			SeenAnswer s;
			s.table = a;
			s.index = b;
			s.http = std::atoi(v[3].c_str());
			s.code = v[4];
			s.detail = v[5];
			r.seen.push_back(s);
		}
		else if (v[0] == "rule" && v.size() == 2)
			r.rules.insert(v[1]);
		else if (v[0] == "example" && v.size() == 2)
			r.examples.insert(v[1]);
		else if (v[0] == "seconds" && v.size() == 3 && number(v[2], a))
			r.ms[v[1]] = a;
		else if (v[0] == "end" && v.size() == 1)
			ended = true;
		else
			ok = false;
	}
	std::fclose(f);
	if (!ok)
		why = "carries a line it should not: " + line;
	else if (!ended)
		why = "stops before its end";
	else if (!failed || !binary)
		why = "does not say whether it failed and which binary ran it";
	return ok && ended && failed && binary;
}

} // namespace

CATCH_REGISTER_LISTENER(PartListener)

namespace
{

struct Planned
{
	size_t                k;
	size_t                n;
	std::set<std::string> names;
};

Planned &planned()
{
	static Planned p;
	return p;
}

} // namespace

bool planPart(const std::string &which, std::string &spec)
{
	size_t k = 0, n = 0;
	if (!partOf(which, k, n))
	{
		std::fprintf(stderr, "--part wants k/n with 1 <= k <= n, not %s\n", which.c_str());
		return false;
	}

	// Gone before anything runs, so a part that dies leaves no record from an earlier run behind.
	std::remove(statePath(k).c_str());

	std::vector<SourceFile> files;
	if (!sourceFiles(files))
		return false;
	const std::vector<size_t> mine = deal(files, n)[k - 1];

	planned().k = k;
	planned().n = n;
	spec.clear();
	for (size_t i = 0; i < mine.size(); ++i)
	{
		const SourceFile &s = files[mine[i]];
		spec += (spec.empty() ? "[" : ",[") + s.tag + "]~[.]";
		planned().names.insert(s.names.begin(), s.names.end());
	}
	return true;
}

int finishPart(int failed)
{
	const size_t k = planned().k, n = planned().n;
	const std::set<std::string> done(ran().names.begin(), ran().names.end());
	if (done != planned().names || done.size() != ran().names.size())
	{
		std::fprintf(stderr, "part %lu of %lu was to run %lu cases and ran %lu\n", (unsigned long) k,
			     (unsigned long) n, (unsigned long) planned().names.size(),
			     (unsigned long) ran().names.size());
		if (failed == 0)
			failed = 1;
	}

	const std::string path = statePath(k);
	const std::string tmp = path + ".tmp";
	FILE *f = std::fopen(tmp.c_str(), "w");
	if (f == NULL)
	{
		std::fprintf(stderr, "%s cannot be written\n", tmp.c_str());
		return 1;
	}
	writeState(f, k, n, failed);
	if (std::fclose(f) != 0 || std::rename(tmp.c_str(), path.c_str()) != 0)
	{
		std::fprintf(stderr, "%s cannot be written\n", path.c_str());
		return 1;
	}
	return failed != 0 ? 1 : 0;
}

bool mergeParts(const std::string &count)
{
	size_t n = 0;
	if (!number(count, n) || n == 0)
	{
		std::fprintf(stderr, "--merge wants the number of parts, not %s\n", count.c_str());
		return false;
	}

	bool whole = true;
	std::set<std::string> ran_once;
	std::map<std::string, size_t> ms;
	for (size_t k = 1; k <= n; ++k)
	{
		PartRecord r;
		r.failed = 0;
		std::string why;
		if (!readState(statePath(k), r, why))
		{
			std::fprintf(stderr, "part %lu of %lu %s\n", (unsigned long) k, (unsigned long) n, why.c_str());
			whole = false;
			continue;
		}
		if (r.k != k || r.n != n)
		{
			std::fprintf(stderr, "%s is part %lu of %lu and not %lu of %lu\n", statePath(k).c_str(),
				     (unsigned long) r.k, (unsigned long) r.n, (unsigned long) k, (unsigned long) n);
			whole = false;
			continue;
		}
		if (r.binary != binaryIdentity())
		{
			std::fprintf(stderr, "part %lu of %lu was run by another build of this program\n",
				     (unsigned long) k, (unsigned long) n);
			whole = false;
			continue;
		}
		if (r.failed != 0)
		{
			std::fprintf(stderr, "part %lu of %lu failed\n", (unsigned long) k, (unsigned long) n);
			whole = false;
		}
		for (size_t i = 0; i < r.cases.size(); ++i)
		{
			if (!ran_once.insert(r.cases[i]).second)
			{
				std::fprintf(stderr, "%s ran in more than one part\n", r.cases[i].c_str());
				whole = false;
			}
		}
		for (size_t i = 0; i < r.counts.size(); ++i)
			recordCount(r.counts[i].first.c_str(), r.counts[i].second);
		if (!addAnswerState(r.seen, r.rules, r.examples))
		{
			std::fprintf(stderr, "part %lu of %lu names a route this binary does not have\n",
				     (unsigned long) k, (unsigned long) n);
			whole = false;
		}
		for (std::map<std::string, size_t>::const_iterator it = r.ms.begin(); it != r.ms.end(); ++it)
			ms[it->first] += it->second;
	}

	std::vector<SourceFile> files;
	if (!sourceFiles(files))
		whole = false;
	size_t expected = 0;
	for (size_t i = 0; i < files.size(); ++i)
	{
		for (size_t j = 0; j < files[i].names.size(); ++j)
		{
			++expected;
			if (ran_once.count(files[i].names[j]) == 0)
			{
				std::fprintf(stderr, "%s ran in no part\n", files[i].names[j].c_str());
				whole = false;
			}
		}
	}
	if (ran_once.size() != expected)
	{
		std::fprintf(stderr, "the parts ran %lu cases and this binary has %lu\n",
			     (unsigned long) ran_once.size(), (unsigned long) expected);
		whole = false;
	}

	// Measured beside the binary, for whoever wants the dealing refreshed.
	const std::string actual = std::string(COREAPI_PART_DIR) + "/part-seconds.actual";
	FILE *f = std::fopen(actual.c_str(), "w");
	if (f != NULL)
	{
		std::fprintf(f, "# Milliseconds each file took, written by the last merge of the parts.\n");
		for (std::map<std::string, size_t>::const_iterator it = ms.begin(); it != ms.end(); ++it)
			std::fprintf(f, "%s\t%lu\n", it->first.c_str(), (unsigned long) it->second);
		std::fclose(f);
	}

	std::printf("%lu parts ran %lu of %lu cases\n", (unsigned long) n, (unsigned long) ran_once.size(),
		    (unsigned long) expected);
	return whole;
}
