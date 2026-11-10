/*
 * update.h - the image updates lying on the box, and installing one of them
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

#ifndef __coreapi_update_h__
#define __coreapi_update_h__

#include "coreapi/base/result.h"

#include <string>
#include <vector>

#include <stdint.h>
#include <time.h>

namespace coreapi
{
namespace update
{

/* AN IMAGE UPDATE, INSTALLED FROM OUTSIDE.

   An image update is a .bin: a tarball with the files it replaces and two
   scripts around them, which the box's own install.sh unpacks and puts in
   place. That is what the menu's local update does with a .bin
   (CFlashUpdate in src/gui/update.cpp), and the same script with the same two
   arguments is what this layer runs.

   NOT A WHOLE IMAGE. Writing a complete system is for somebody sitting in
   front of the television, who answers the menu's questions and sees the
   progress, and is left to the menu.

   NOTHING ON THE SCREEN. The script runs without the menu's progress window,
   so a box in standby stays in standby. Whether the box restarts afterwards
   is the package's own decision. */

struct File
{
	// The bare name inside the directory, which is what start() takes.
	std::string name;
	uint64_t    size;
	time_t      modified;

	File() : size(0), modified(0) {}
};

struct Local
{
	// Where the files were looked for.
	std::string       dir;
	// By name. Only what start() would take.
	std::vector<File> files;
};

/* What is lying in the update directory right now. A directory that cannot be
   read is an empty list and not a refusal: a box with nothing to install and a
   box whose /tmp is gone offer the caller the same thing to do. */
Result<Local> local();

struct Request
{
	// A bare name out of local(), never a path.
	std::string name;
};

/* Asks the box to install one file out of local().

   Refused before anything is sent, in this order: BadName for a name that is
   not a bare file name ending in .bin, NoSuchUpdateFile for a name the
   directory holds no plain file under, and RecordingRunning while the box is
   recording, because an update ends the program that records.

   Ok says the request reached the loop's socket and no more. */
Result<void> start(const Request &r);

/* What the message carries, read back on the loop's side: the absolute path
   of the file. valid is false for a block this layer did not write, and such
   a block is dropped rather than installed. */
struct Asked
{
	bool        valid;
	std::string path;

	Asked() : valid(false) {}
};
Asked asked(const char *payload);

/* Runs install.sh with the file of a valid Asked, unpacking in the directory
   the file lies in, and answers the script's exit code; -1 where there was
   nothing to run. Blocks until the script is done. Meant for the loop's
   thread, which is where the menu runs the same script. */
int install(const Asked &a);

/* The script itself, for the menu's local update as much as for the one
   above: install.sh unpacks in dir and puts in place what file carries.
   Nothing is checked here. The menu hands it what its file browser or its
   download chose, which on some boxes is a .tgz too, and unpacks in the
   update directory of the settings. Answers the exit code, -1 where the
   script is not there. Blocks until the script is done. */
int install(const std::string &dir, const std::string &file);

/* Whether a bare name is an image update. Apart from local() so that the
   rules can be driven without a directory. */
bool isUpdateName(const std::string &name);

/* Where the files are looked for, /tmp on a box; a trailing separator is
   dropped. Settable so that the suite can hand it a directory it filled. */
void setUpdateRoot(const std::string &dir);
std::string updateRoot();

/* What both install() hand the work to: the directory to unpack in and the
   file, and back the exit code. The box's runs install.sh; settable so that the
   suite sees what would be run without running it. NULL puts the box's back. */
typedef int (*Runner)(const std::string &dir, const std::string &file);
void setRunner(Runner r);

} // namespace update
} // namespace coreapi

#endif
