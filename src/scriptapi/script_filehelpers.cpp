/*
 * the file work both bindings do themselves
 *
 * (C) 2015 M. Liebmann (micha-bbg), and the two bindings this stands in for.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <climits>

#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

#include <system/helpers.h>

#include "script_filehelpers.h"

/* The number a script writes for a mode, read as the octal digits it looks
 * like. */
static mode_t modeOf(int mode)
{
	std::string mode_s = itoa(mode, 10);
	return (mode_t)(strtol(mode_s.c_str(), (char **)NULL, 8) & 0x0FFF);
}

bool ScriptFileHelpers::Chmod(const std::string &file, int mode, std::string &error)
{
	error = "";

	if (chmod(file.c_str(), modeOf(mode)) != 0)
	{
		error = strerror(errno);
		return false;
	}
	return true;
}

/* An error noticed here, at the line that names it. */
#define SCRIPT_FH_ERROR(e, m) \
	do { (e).msg = (m); (e).file = __path_file__; (e).line = __LINE__; } while (0)

/* An error CFileHelpers noticed, at the place it wrote down. */
static void errorOf(CFileHelpers &fh, ScriptFileHelpers::Error &error)
{
	helpersDebugInfo di;
	fh.readDebugInfo(&di);
	error.msg = di.msg;
	error.file = di.file;
	error.line = di.line;
}

bool ScriptFileHelpers::Cp(const char *from, const char *to, const char *flags, Error &error)
{
	CFileHelpers fh;
	fh.setConsoleQuiet(true);
	if (!fh.cp(from, to, flags))
	{
		errorOf(fh, error);
		return false;
	}
	return true;
}

bool ScriptFileHelpers::Touch(const char *file, bool haveTime, time_t newTime, Error &error)
{
	if (!file_exists(file))
	{
		FILE *f = fopen(file, "w");
		if (f == NULL)
		{
			SCRIPT_FH_ERROR(error, strerror(errno));
			return false;
		}
		fclose(f);
		if (!haveTime)
			return true;
	}

	time_t modTime = haveTime ? newTime : time(NULL);
	utimbuf utb;
	utb.actime = modTime;
	utb.modtime = modTime;
	if (utime(file, &utb) != 0)
	{
		SCRIPT_FH_ERROR(error, strerror(errno));
		return false;
	}
	return true;
}

bool ScriptFileHelpers::Rmdir(const char *dir, Error &error)
{
	CFileHelpers *fh = CFileHelpers::getInstance();
	fh->setConsoleQuiet(true);
	if (!fh->removeDir(dir))
	{
		errorOf(*fh, error);
		return false;
	}
	return true;
}

bool ScriptFileHelpers::Mkdir(const char *dir, bool haveMode, int mode, Error &error)
{
	mode_t mode_o = haveMode ? modeOf(mode) : 0755;

	CFileHelpers *fh = CFileHelpers::getInstance();
	fh->setConsoleQuiet(true);
	if (!fh->createDir(dir, mode_o))
	{
		errorOf(*fh, error);
		return false;
	}
	return true;
}

bool ScriptFileHelpers::Readlink(const char *link, std::string &target, Error &error)
{
	char buf[PATH_MAX];
	memset(buf, '\0', sizeof(buf));
	if (readlink(link, buf, sizeof(buf) - 1) == -1)
	{
		SCRIPT_FH_ERROR(error, strerror(errno));
		return false;
	}
	target = buf;
	return true;
}

bool ScriptFileHelpers::Ln(const char *src, const char *link, const char *flags, Error &error)
{
	bool symlnk = (strchr(flags, 's') != NULL);
	bool force = (strchr(flags, 'f') != NULL);

	if (!symlnk)
	{
		SCRIPT_FH_ERROR(error, "Currently only supports symlinks.");
		return false;
	}

	if (symlink(src, link) != 0)
	{
		if (force && (errno == EEXIST))
		{
			if (unlink(link) == 0)
			{
				if (symlink(src, link) == 0)
					return true;
			}
		}
		SCRIPT_FH_ERROR(error, strerror(errno));
		return false;
	}
	return true;
}

bool ScriptFileHelpers::Exist(const char *file, const char *flags, bool &result, Error &error)
{
	result = false;
	if (!file_exists(file))
		return true;

	struct stat FileInfo;
	if (lstat(file, &FileInfo) == -1)
	{
		SCRIPT_FH_ERROR(error, strerror(errno));
		return false;
	}

	if (strchr(flags, 'f') != NULL)
		result = S_ISREG(FileInfo.st_mode);
	else if (strchr(flags, 'l') != NULL)
		result = S_ISLNK(FileInfo.st_mode);
	else if (strchr(flags, 'd') != NULL)
		result = S_ISDIR(FileInfo.st_mode);
	else
	{
		std::string msg = (strlen(flags) == 0) ? "no" : "unknown";
		msg += " flag given.";
		SCRIPT_FH_ERROR(error, msg);
		return false;
	}
	return true;
}

bool ScriptFileHelpers::IsEmpty(const char *file, bool &result, Error &error)
{
	result = false;
	if (!file_exists(file))
	{
		SCRIPT_FH_ERROR(error, std::string(file) + " does not exist.");
		return false;
	}

	struct stat FileInfo;
	if (lstat(file, &FileInfo) == -1)
	{
		SCRIPT_FH_ERROR(error, strerror(errno));
		return false;
	}

	if (S_ISLNK(FileInfo.st_mode))
	{
		printf("Following symlink: %s\n", file);
		if (stat(file, &FileInfo) == -1)
		{
			SCRIPT_FH_ERROR(error, "Symlink target does not exist: " + std::string(strerror(errno)));
			return false;
		}
		if (!S_ISREG(FileInfo.st_mode))
		{
			SCRIPT_FH_ERROR(error, "Target of symlink is not a regular file.");
			return false;
		}
	}
	else if (!S_ISREG(FileInfo.st_mode))
	{
		SCRIPT_FH_ERROR(error, std::string(file) + " is not a regular file.");
		return false;
	}

	result = (FileInfo.st_size == 0);
	return true;
}
