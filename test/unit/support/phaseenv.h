/*
 * phaseenv.h - the seams an apply group finds installed in a startup phase
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

#ifndef __support_phaseenv_h__
#define __support_phaseenv_h__

#include "support/phasefakes.h"

#include "coreapi/base/apply.h"
#include "coreapi/base/deps.h"
#include "coreapi/box/applyworker.h"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

/* A key source that knows no key, for a phase that has one installed. */
struct PhaseKeySource : public coreapi::KeySource
{
	bool known(long) const { return false; }
	std::string name(long) const { return std::string(); }
	std::vector<coreapi::KeyName> all() const { return std::vector<coreapi::KeyName>(); }
};

/* One address per type, since the suite builds without run-time type
   information and a fake handed out as the wrong type would be a silent cast. */
template <class T>
const void *phaseTypeTag()
{
	static const char tag = 0;
	return &tag;
}

// One seam the helper can stand in for: how its fake is made, dropped and installed.
struct PhaseSeam
{
	const char *name;
	const void *type;
	void *(*make)();
	void (*drop)(void *);
	void (*install)(void *);
	void (*clear)();
};

#define PHASE_SEAM(n, Fake, setter) \
	{ n, phaseTypeTag<Fake>(), []() -> void * { return new Fake(); }, [](void *p) { delete static_cast<Fake *>(p); }, \
	  [](void *p) { setter(static_cast<Fake *>(p)); }, []() { setter(0); } },

inline const std::vector<PhaseSeam> &phaseSeams()
{
	static const PhaseSeam kSeams[] = {
#include "support/phaseseams.inc"
	};
	static const std::vector<PhaseSeam> seams(kSeams, kSeams + sizeof(kSeams) / sizeof(kSeams[0]));
	return seams;
}

#undef PHASE_SEAM

/* Every seam as a startup phase finds it: a fake for each one
   test/unit/scan/apply-phase-seams.txt says is installed by then, and nothing for
   the others, which then end the process or answer their default exactly as the
   program would. A group's startup case runs inside one, so a group that reaches a
   seam its phase does not have fails here and not on the box. Every seam is cleared
   again when it goes. The fakes are made whether installed or not, so a case can
   set one up before it asks; fake<T>(name) hands one out. */
class PhaseEnvironment
{
	public:
		// Seams the table names that this helper cannot stand in for.
		std::vector<std::string> unknown;
		// How many lines of the table were read, whatever their phase.
		size_t read;

		explicit PhaseEnvironment(coreapi::ApplyPhase phase) : read(0)
		{
			const std::vector<PhaseSeam> &seams = phaseSeams();
			for (size_t i = 0; i < seams.size(); ++i)
				fakes.push_back(seams[i].make());
			clear();
			FILE *f = fopen(COREAPI_PHASE_SEAMS_FILE, "r");
			if (f == NULL)
				return;
			char line[256];
			while (fgets(line, sizeof(line), f) != NULL)
			{
				std::string l(line);
				while (!l.empty() && (l[l.size() - 1] == '\n' || l[l.size() - 1] == '\r'))
					l.erase(l.size() - 1);
				if (l.empty() || l[0] == '#')
					continue;
				const size_t a = l.find('\t');
				const size_t b = a == std::string::npos ? a : l.find('\t', a + 1);
				if (b == std::string::npos)
				{
					unknown.push_back(l);
					continue;
				}
				++read;
				const std::string seam = l.substr(0, a);
				const int first = phaseNumber(l.substr(b + 1));
				const int at = indexOf(seam);
				if (first < 0 || at < 0)
					unknown.push_back(seam);
				else if (first <= (int) phase)
					seams[at].install(fakes[at]);
			}
			fclose(f);
		}

		~PhaseEnvironment()
		{
			// A job a case left on the worker holds a fake that goes below.
			coreapi::applyWorker().wait();
			clear();
			const std::vector<PhaseSeam> &seams = phaseSeams();
			for (size_t i = 0; i < seams.size(); ++i)
				seams[i].drop(fakes[i]);
		}

		/* The fake standing in for a seam, by its name in the table. A name the
		   table lacks or a type other than the seam's fake throws, so a typo in
		   a case fails at once with the name and not as damage further on. */
		template <class T>
		T &fake(const char *name)
		{
			const int at = indexOf(name);
			if (at < 0)
				throw std::logic_error(std::string("no seam named ") + name + " in phaseseams.inc");
			if (phaseSeams()[at].type != phaseTypeTag<T>())
				throw std::logic_error(std::string("the fake of seam ") + name + " is not of the type asked for");
			return *static_cast<T *>(fakes[at]);
		}

	private:
		std::vector<void *> fakes;

		static int phaseNumber(const std::string &name)
		{
			static const char *const kNames[] = { "Framebuffer", "Decoders", "Zapit", "Sectionsd", "Network" };
			for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); ++i)
				if (name == kNames[i])
					return (int) i;
			return -1;
		}

		static int indexOf(const std::string &name)
		{
			const std::vector<PhaseSeam> &seams = phaseSeams();
			for (size_t i = 0; i < seams.size(); ++i)
				if (name == seams[i].name)
					return (int) i;
			return -1;
		}

		static void clear()
		{
			const std::vector<PhaseSeam> &seams = phaseSeams();
			for (size_t i = 0; i < seams.size(); ++i)
				seams[i].clear();
		}

		PhaseEnvironment(const PhaseEnvironment &);
		PhaseEnvironment &operator=(const PhaseEnvironment &);
};

#endif
