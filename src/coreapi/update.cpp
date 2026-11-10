/*
 * update.cpp - the image updates lying on the box, and installing one of them
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

#include <config.h>

#include "update.h"

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/types.h"
#include "coreapi/recordings.h"

#include <neutrinoMessages.h>
#include <system/helpers.h>

#include <cstdio>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <algorithm>
#include <utility>

#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace coreapi
{
namespace update
{

namespace
{

OpenThreads::Mutex &stateMutex()
{
	static OpenThreads::Mutex m;
	return m;
}

/* /tmp and not the update directory of the settings. It is where a file sent
   to the box lands and it is what that setting is on a box nobody changed it
   on; a second directory would be a second place a file can be and not be
   found. */
std::string &rootStore()
{
	static std::string s("/tmp");
	return s;
}

/* The menu's local update runs the script the same way: the directory to
   unpack in first, the file second (src/gui/update.cpp). */
int runInstallScript(const std::string &dir, const std::string &file)
{
	const std::string script = find_executable("install.sh");
	if (script.empty())
	{
		printf("[update] install.sh not found\n");
		return -1;
	}
	printf("[update] calling %s %s %s\n", script.c_str(), dir.c_str(), file.c_str());
	return my_system(3, script.c_str(), dir.c_str(), file.c_str());
}

Runner &runnerStore()
{
	static Runner r = &runInstallScript;
	return r;
}

bool endsWith(const std::string &name, const char *ending)
{
	const std::string e(ending);
	return name.size() > e.size() && name.compare(name.size() - e.size(), e.size(), e) == 0;
}

/* A name that is one entry of the directory and nothing else. No separator, so
   it cannot leave the directory; no leading dot, so it is neither of the two
   names every directory has nor a file somebody hid; no leading hyphen and no
   control character, because the name ends up as an argument of a script. */
bool bareName(const std::string &name)
{
	if (name.empty() || name.size() > NAME_MAX)
		return false;
	if (name[0] == '.' || name[0] == '-')
		return false;
	for (size_t i = 0; i < name.size(); i++)
	{
		const unsigned char c = (unsigned char) name[i];
		if (c == '/' || c < 0x20 || c == 0x7f)
			return false;
	}
	return true;
}

// Only a plain file. A directory called update.bin is not an update.
bool plainFile(const std::string &path, struct stat &st)
{
	return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool byName(const File &a, const File &b)
{
	return a.name < b.name;
}

} // namespace

void setUpdateRoot(const std::string &dir)
{
	std::string trimmed = dir;
	while (trimmed.size() > 1 && trimmed[trimmed.size() - 1] == '/')
		trimmed.erase(trimmed.size() - 1);

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(stateMutex());
	rootStore() = trimmed;
}

std::string updateRoot()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(stateMutex());
	return rootStore();
}

void setRunner(Runner r)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(stateMutex());
	runnerStore() = r ? r : &runInstallScript;
}

/* The ending as the menu reads it once a file is chosen: compared as it is
   written, so Update.BIN is not one. */
bool isUpdateName(const std::string &name)
{
	return bareName(name) && endsWith(name, ".bin");
}

Result<Local> local()
{
	Local out;
	out.dir = updateRoot();

	DIR *d = opendir(out.dir.c_str());
	if (d == NULL)
		return ok(std::move(out));

	while (struct dirent *e = readdir(d))
	{
		File f;
		f.name = e->d_name;
		if (!isUpdateName(f.name))
			continue;
		struct stat st;
		if (!plainFile(out.dir + "/" + f.name, st))
			continue;
		f.size = (uint64_t) st.st_size;
		f.modified = st.st_mtime;
		out.files.push_back(f);
	}
	closedir(d);

	std::sort(out.files.begin(), out.files.end(), byName);
	return ok(std::move(out));
}

Result<void> start(const Request &r)
{
	if (!isUpdateName(r.name))
		return fail(Status::InvalidArgument, ErrorCode::BadName,
			    "that is not the name of an image update");

	const std::string path = updateRoot() + "/" + r.name;
	struct stat st;
	if (!plainFile(path, st))
		return fail(Status::NotFound, ErrorCode::NoSuchUpdateFile,
			    "the update directory holds no file of that name");

	/* Asked last, so a request that could never have gone through is told so
	   whatever the box is doing. A list that cannot be read refuses too: a box
	   that cannot say whether it is recording is not one to update on a guess. */
	Result<RecordingList> recording = recordings::list();
	if (!recording.ok())
		return fail(recording.error());
	if (!recording.value().empty())
		return fail(Status::Conflict, ErrorCode::RecordingRunning,
			    "the box is recording, and an update would end that");

	// The path and the byte after it: what reads this on the loop reads up to the end of a string.
	return postPayload(NeutrinoMessages::EVT_LOCAL_UPDATE, path.c_str(), path.size() + 1);
}

Asked asked(const char *payload)
{
	Asked a;
	if (payload == NULL)
		return a;

	/* An absolute path whose last part is an update's name, or nothing: a
	   relative one would be read against whatever directory the program
	   happens to stand in. */
	const std::string path(payload);
	const size_t slash = path.rfind('/');
	if (path.empty() || path[0] != '/' || slash == std::string::npos)
		return a;
	if (!isUpdateName(path.substr(slash + 1)))
		return a;

	a.path = path;
	a.valid = true;
	return a;
}

int install(const Asked &a)
{
	if (!a.valid)
		return -1;

	const size_t slash = a.path.rfind('/');
	const std::string dir = slash == 0 ? std::string("/") : a.path.substr(0, slash);
	return install(dir, a.path);
}

int install(const std::string &dir, const std::string &file)
{
	Runner run;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> lock(stateMutex());
		run = runnerStore();
	}
	return run(dir, file);
}

namespace
{

const time_t kOnlineKept = 15 * 60;

struct OnlineState
{
	OpenThreads::Mutex  lock;    // the fields below
	OpenThreads::Mutex  running; // held for as long as a check runs
	OnlineSource       *source;
	Online              kept;
	bool                have;
	// Counts the checks that ended, so a waiter can tell one ended while it waited.
	unsigned long       done;

	OnlineState() : source(NULL), have(false), done(0) {}
};

OnlineState &onlineState()
{
	static OnlineState s;
	return s;
}

/* The kept answer if there is one young enough. A clock set back since makes
   it look older than a quarter of an hour ahead, never younger. */
bool keptAnswer(Online &out)
{
	OnlineState &st = onlineState();
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(st.lock);
	if (!st.have)
		return false;
	const time_t now = time(NULL);
	if (now < st.kept.checked || now - st.kept.checked >= kOnlineKept)
		return false;
	out = st.kept;
	return true;
}

} // namespace

void setOnlineSource(OnlineSource *s)
{
	OnlineState &st = onlineState();
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(st.lock);
	st.source = s;
	st.have = false;
}

void forgetOnline()
{
	OnlineState &st = onlineState();
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(st.lock);
	st.have = false;
}

Result<Online> online(bool fresh)
{
	OnlineState &st = onlineState();
	Online out;
	if (!fresh && keptAnswer(out))
		return ok(std::move(out));

	/* One check at a time. Whoever waited here for another one takes its
	   answer, fresh or not: it is seconds old. Told apart by a count and not
	   by the clock, which within one second cannot say which came first. */
	unsigned long before;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> lock(st.lock);
		before = st.done;
	}
	OpenThreads::ScopedLock<OpenThreads::Mutex> one(st.running);
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> lock(st.lock);
		if (st.have && st.done != before)
		{
			out = st.kept;
			return ok(std::move(out));
		}
	}
	if (!fresh && keptAnswer(out))
		return ok(std::move(out));

	OnlineSource *source;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> lock(st.lock);
		source = st.source;
	}
	if (source == NULL)
		return fail(Status::NotSupported, ErrorCode::NoUpdateCheck,
			    "nothing on this box can ask the update server");

	out = source->check();
	out.checked = time(NULL);
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> lock(st.lock);
		st.kept = out;
		st.have = true;
		st.done++;
	}
	return ok(std::move(out));
}

} // namespace update
} // namespace coreapi
