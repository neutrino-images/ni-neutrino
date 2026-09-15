/*
 * livestream.cpp - an answer whose body is a running program's output
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

/* POLLRDHUP, which says that the peer has closed its end. It is Linux's and
   glibc only offers it under this name, which has to be asked for before
   anything pulls in the header. This program runs on Linux and nowhere else. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include "livestream.h"

#include "http.h"
#include "status.h"

#include "coreapi/base/errors.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <microhttpd.h>

namespace httpd
{

namespace livestream
{

namespace
{

/* HOW MANY MAY RUN AT ONCE, and why it is this small.

   Every listener costs a converter for as long as they listen, because almost all
   German radio on a satellite is Layer II and no browser decodes it. So this is not
   a burst that passes: it is a steady load one person adds by opening a page.

   The number is two. What it buys is that a box which is recording and showing a
   picture stays one: the weakest boxes this program runs on have two cores, and a
   third converter is the one that starts costing the television. A refusal that says
   so is a page that tells somebody to wait; a box that accepts everybody is a picture
   that stutters and nothing anywhere saying why.

   Not a setting, because a setting is a number somebody raises once and never looks
   at again on the box where it was already too high. */
const size_t kMaxSessions = 2;

/* The most one read hands the library. A transport stream at eight megabits is
   a megabyte a second, so this is read from the pipe a few dozen times a
   second and never sits in this process. */
const size_t kBlockSize = 32768;

/* How long the watcher waits before it looks again even though nothing woke
   it. Only reached while every session is being read as fast as it is written,
   so it costs one wakeup a second on an idle box. */
const int kWatchIdleMs = 1000;

// How long a program is given to end on its own after it has been asked to,
// before it is ended for it. It is being asked because nobody is reading it
// any more, so there is nothing to lose by being quick.
const unsigned kEndWaitMs = 200;
const unsigned kEndStepMs = 10;

struct Session
{
	struct MHD_Connection *conn;
	// The reading end of the program's output, non blocking, this object's own.
	int   fd;
	/* The socket the answer goes out on. Never read from and never written to
	   here: it is watched, and the one thing it is watched for is the peer
	   going away while this session is suspended. See watchOnce. */
	int   sock;
	pid_t child;
	/* The library is holding this connection out of its event loop and will
	   not touch it again until it is resumed. Set only by the reader and
	   cleared only by whoever is about to resume, so that one suspension is
	   resumed exactly once. */
	bool  suspended;
	/* This server is stopping and is not going to wake anybody again, so the
	   reader is to end rather than wait. Set once and never cleared: a session
	   past this point is going, and clearing it would put it back to waiting
	   for a watcher that is no longer there. */
	bool  closing;

	Session() : conn(NULL), fd(-1), sock(-1), child(-1), suspended(false), closing(false) {}
};

OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

std::vector<Session *> &sessions()
{
	static std::vector<Session *> v;
	return v;
}

bool serving_ = false;
bool watching_ = false;
pthread_t watcher_;

/* The watcher sleeps in poll and has to be told that the set of things it is
   polling has changed. A pipe of this server's own is the one thing poll can
   be told with that does not depend on a signal reaching a particular thread. */
int wake_read_ = -1;
int wake_write_ = -1;

void nudge()
{
	if (wake_write_ < 0)
		return;
	const char one = 'x';
	// The pipe is only ever a flag. A full one is a watcher that has already
	// been told and has not looked yet, which is the same answer.
	if (::write(wake_write_, &one, 1) < 0)
		(void) 0;
}

void drainWake()
{
	char buf[64];
	while (::read(wake_read_, buf, sizeof(buf)) > 0)
		;
}

void sleepMs(unsigned ms)
{
	struct timespec ts;
	ts.tv_sec = (time_t)(ms / 1000u);
	ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
	while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
		;
}

/* Stops a program and collects it, so a box running this for a month does not fill
   its process table with programs nobody reads.

   Asked first and killed after, because a converter asked to stop closes what it was
   reading and leaves the box's own streaming server one client lighter, and one
   killed outright leaves that server waiting for a socket to time out.

   waitpid may find nothing to collect: something else in this program sets SIGCHLD to
   be ignored when it starts a command in the background, and a process whose parent
   ignores that signal is collected by the system. */
void endChild(pid_t child)
{
	if (child <= 0)
		return;

	/* THE WHOLE GROUP AND NOT ONLY THE ONE THAT WAS STARTED. spawn puts the child in
	   a session of its own, so it leads a process group holding it and anything it
	   starts; a converter that is one program in front of another would otherwise
	   leave the second one running with nobody left to end it. The negative number is
	   that group, and the plain one after it is for the case where the session could
	   not be made. */
	if (::kill(-child, SIGTERM) != 0)
		::kill(child, SIGTERM);

	for (unsigned waited = 0; waited < kEndWaitMs; waited += kEndStepMs)
	{
		const pid_t seen = ::waitpid(child, NULL, WNOHANG);
		/* Collected, or never this process's to collect. Either way the number
		   is free for the system to hand to somebody else from here on, so
		   nothing is signalled with it again. */
		if (seen == child || (seen < 0 && errno == ECHILD))
			return;
		sleepMs(kEndStepMs);
	}

	if (::kill(-child, SIGKILL) != 0)
		::kill(child, SIGKILL);
	::waitpid(child, NULL, 0);
}

/* Starts one program with its output on a pipe. Nothing between the fork and
   the exec allocates or takes a lock, because this process has other threads
   and a child of a threaded process may only do what a signal handler may do
   until it has execed.

   Answers the reading end, or below zero. */
int spawn(const std::vector<std::string> &argv, pid_t &child)
{
	if (argv.empty())
		return -1;

	/* Built before the fork, because building it in the child would be the
	   child allocating. The vector is this call's own and outlives the exec by
	   definition: an exec that worked never comes back to read it. */
	std::vector<char *> raw;
	raw.reserve(argv.size() + 1);
	for (size_t i = 0; i < argv.size(); ++i)
		raw.push_back(const_cast<char *>(argv[i].c_str()));
	raw.push_back(NULL);

	int pipefd[2] = { -1, -1 };
	if (::pipe(pipefd) != 0)
		return -1;

	const pid_t pid = ::fork();
	if (pid < 0)
	{
		::close(pipefd[0]);
		::close(pipefd[1]);
		return -1;
	}

	if (pid == 0)
	{
		::dup2(pipefd[1], STDOUT_FILENO);
		::close(pipefd[0]);
		::close(pipefd[1]);
		/* Everything else this process has open, shut. A converter that
		   inherited the listening socket would hold the port after this
		   program has stopped, and one that inherited another caller's
		   connection would hold that caller open too. */
		for (int i = STDERR_FILENO + 1; i < 1024; ++i)
			::close(i);
		/* What the program says about itself goes where this program's own
		   output goes, so that a converter that refused a channel says why in
		   the same log as everything else. */
		::setsid();
		::execv(raw[0], &raw[0]);
		_exit(127);
	}

	::close(pipefd[1]);

	/* Non blocking, because the reader below runs on a thread the library owns
	   and shares with every other connection that thread is polling. A read
	   that waited there would stop answering all of them. */
	const int flags = ::fcntl(pipefd[0], F_GETFL, 0);
	if (flags < 0 || ::fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK) != 0)
	{
		::close(pipefd[0]);
		endChild(pid);
		return -1;
	}
	::fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);

	child = pid;
	return pipefd[0];
}

/* The library wants more of the body. Runs on a thread the library owns, is
   reached from C, and may not throw.

   Nothing waits in here. Either the program has written something, or it has
   finished, or there is nothing yet and the connection goes out of the event
   loop until the watcher says there is. */
ssize_t readSession(void *cls, uint64_t pos, char *buf, size_t max)
{
	(void) pos;

	Session *s = (Session *) cls;
	if (s == NULL || buf == NULL)
		return MHD_CONTENT_READER_END_WITH_ERROR;

	try
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());

		// Said before the pipe is looked at, because a session being taken
		// down has nothing left to say and nothing left to wake it.
		if (s->closing || s->fd < 0)
			return MHD_CONTENT_READER_END_OF_STREAM;

		const size_t want = (max < kBlockSize) ? max : kBlockSize;
		for (;;)
		{
			const ssize_t n = ::read(s->fd, buf, want);
			if (n > 0)
				return n;
			// The program closed its output, which is the program having
			// finished. An answer that ends is what a player then sees.
			if (n == 0)
				return MHD_CONTENT_READER_END_OF_STREAM;
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN && errno != EWOULDBLOCK)
				return MHD_CONTENT_READER_END_WITH_ERROR;
			break;
		}

		/* Under the same lock as the emptiness that led to it. With the lock
		   given up in between, the watcher could see a readable pipe on a
		   session that is not suspended yet and nothing would wake it again. */
		s->suspended = true;
		MHD_suspend_connection(s->conn);
		nudge();
		return 0;
	}
	catch (...)
	{
	}
	return MHD_CONTENT_READER_END_WITH_ERROR;
}

/* The library is done with the answer, which is the last thing it does with the
   connection. Reached from C and may not throw.

   This is the only place a session stops being counted, so a browser that went
   away and a program that ended both arrive here and both free the place. */
void sessionGone(void *cls)
{
	Session *s = (Session *) cls;
	if (s == NULL)
		return;

	int fd = -1;
	pid_t child = -1;
	try
	{
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
			std::vector<Session *> &list = sessions();
			for (size_t i = 0; i < list.size(); ++i)
			{
				if (list[i] != s)
					continue;
				list.erase(list.begin() + (std::ptrdiff_t) i);
				break;
			}
			fd = s->fd;
			child = s->child;
			s->fd = -1;
			s->child = -1;
		}
		nudge();
	}
	catch (...)
	{
	}

	/* Outside the lock. Ending a program waits for it, and waiting while
	   holding the lock would stop every other session for as long as this one
	   takes to die. */
	if (fd >= 0)
		::close(fd);
	endChild(child);
	delete s;
}

/* One turn of the watcher: everything suspended is polled, and whatever has something
   to read or has ended goes back into the library's event loop.

   Only suspended sessions are polled. A session the library is reading is one it will
   come back to on its own, and polling it here would be a second reader of the same
   pipe.

   AND THE SOCKET IS POLLED BESIDE THE PIPE, which is why a converter used to outlive
   the person watching it. While a connection is suspended the library does not touch
   it: it writes nothing, so it never finds out that nobody is there, and a suspended
   connection is the one thing its own timeout does not reach either. A session
   suspended at the moment somebody closed the tab was therefore held until the box was
   restarted, with its converter running and its place on the ceiling taken. Measured:
   a client that exited cleanly left ffmpeg standing with no reader, and the third
   person to ask was refused on a box doing nothing.

   The pipe running dry for a moment is ordinary and not a fault. A live converter has
   dry moments whenever its source has one, so waiting for the pipe alone is waiting
   for something that may not come for minutes.

   POLLRDHUP is what says the peer has gone. It means no more will be sent from there,
   which for a client waiting on a body it asked for is the same thing as having left.
   It is Linux only, which this program is. */
bool watchOnce()
{
	std::vector<struct pollfd> fds;
	std::vector<Session *> waiting;
	bool go_on = true;

	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
		go_on = watching_;
		const std::vector<Session *> &list = sessions();
		fds.reserve(list.size() * 2 + 1);
		waiting.reserve(list.size());
		for (size_t i = 0; i < list.size(); ++i)
		{
			if (!list[i]->suspended || list[i]->fd < 0)
				continue;
			struct pollfd one;
			std::memset(&one, 0, sizeof(one));
			one.fd = list[i]->fd;
			one.events = POLLIN;
			fds.push_back(one);
			waiting.push_back(list[i]);
		}
		/* The sockets after the pipes and in the same order, so one index
		   reaches both halves of a session without a second list to keep in
		   step. A session whose connection would not name a socket is watched
		   on its pipe alone, which is what this did for all of them before. */
		for (size_t i = 0; i < waiting.size(); ++i)
		{
			struct pollfd one;
			std::memset(&one, 0, sizeof(one));
			one.fd = waiting[i]->sock;
			one.events = POLLRDHUP | POLLERR | POLLHUP;
			fds.push_back(one);
		}
	}

	if (!go_on)
		return false;

	struct pollfd woken;
	std::memset(&woken, 0, sizeof(woken));
	woken.fd = wake_read_;
	woken.events = POLLIN;
	fds.push_back(woken);

	if (::poll(&fds[0], (nfds_t) fds.size(), kWatchIdleMs) < 0 && errno != EINTR)
		return true;

	if ((fds[fds.size() - 1].revents & POLLIN) != 0)
		drainWake();

	std::vector<struct MHD_Connection *> wake;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
		const std::vector<Session *> &list = sessions();
		wake.reserve(waiting.size());
		for (size_t i = 0; i < waiting.size(); ++i)
		{
			/* The session may have been freed since the poll set was built, so
			   the list is what says whether this pointer is still one. */
			bool alive = false;
			for (size_t j = 0; j < list.size(); ++j)
				alive = alive || list[j] == waiting[i];
			if (!alive || !waiting[i]->suspended)
				continue;

			/* Nobody is listening any more. The session is marked so that the
			   reader ends the answer the moment it is asked again, and it is
			   handed back for exactly that: the library then finishes with the
			   connection, which is what stops the program and gives the place
			   back. Marked and not torn down here, because the descriptor and
			   the child belong to the one path that owns them. */
			const bool gone =
				(fds[waiting.size() + i].revents & (POLLRDHUP | POLLERR | POLLHUP)) != 0;
			if (gone)
				waiting[i]->closing = true;

			// POLLHUP as well as POLLIN: a program that ended has to be read
			// once more so the reader sees the end and says so.
			if (!gone && (fds[i].revents & (POLLIN | POLLHUP | POLLERR)) == 0)
				continue;
			waiting[i]->suspended = false;
			wake.push_back(waiting[i]->conn);
		}
	}

	// Outside the lock, and this is all the watcher does to a connection: hand
	// it back. One of the library's own threads writes what the reader reads.
	for (size_t i = 0; i < wake.size(); ++i)
		MHD_resume_connection(wake[i]);

	return true;
}

void *watch(void *)
{
	for (;;)
	{
		bool go_on = true;
		try
		{
			go_on = watchOnce();
		}
		catch (...)
		{
		}
		if (!go_on)
			return NULL;
	}
}

// Wakes every session so the reader sees a pipe that is gone and ends. Answers
// how many there were, so the caller can wait for the library to give them back.
size_t closeEvery()
{
	std::vector<struct MHD_Connection *> wake;
	size_t left = 0;

	try
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
		std::vector<Session *> &list = sessions();
		left = list.size();
		wake.reserve(left);
		for (size_t i = 0; i < list.size(); ++i)
		{
			/* Marked rather than closed. The descriptor belongs to the session
			   and closing it here would leave the reader holding a number the
			   system may already have given to somebody else. What ends the
			   answer is the flag, and the program is asked to stop beside it so
			   that the box's own streaming server loses its client now rather
			   than on a timeout. */
			list[i]->closing = true;
			if (list[i]->child > 0)
				::kill(list[i]->child, SIGTERM);
			if (list[i]->suspended)
			{
				list[i]->suspended = false;
				wake.push_back(list[i]->conn);
			}
		}
	}
	catch (...)
	{
	}

	for (size_t i = 0; i < wake.size(); ++i)
		MHD_resume_connection(wake[i]);

	return left;
}

} // namespace

size_t openSessions()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	return sessions().size();
}

size_t maxSessions()
{
	return kMaxSessions;
}

bool isStream(const Response &r)
{
	return r.code == StatusOk && !r.stream_argv.empty();
}

Opened open(struct MHD_Connection *connection, const Response &r, Response &refusal)
{
	if (connection == NULL || r.stream_argv.empty())
		return StreamFailed;

	/* Asked before anything is counted or started, because a box without the
	   converter is a box that cannot do this at all and ought to say so rather
	   than hand out an answer with no bytes in it. Not every box carries one:
	   the ffmpeg some of them build has neither an AAC encoder nor a command
	   line program, and browsers cannot play what those boxes broadcast
	   anyway. */
	if (::access(r.stream_argv[0].c_str(), X_OK) != 0)
	{
		refusal = problemResponse(StatusNotImplemented,
		                          coreapi::ErrorCode::NotPlayableInBrowser,
		                          "this box carries nothing that can convert a channel for a browser");
		return StreamRefused;
	}

	bool room = false;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
		// Counted under the lock the place is taken under, so that two
		// requests arriving together cannot both find the last one free.
		room = serving_ && sessions().size() < kMaxSessions;
	}

	if (!room)
	{
		refusal = problemResponse(StatusServiceUnavailable,
		                          coreapi::ErrorCode::TooManyConversions,
		                          "this box is already converting as many streams as it will");
		refusal.headers.push_back(std::make_pair(std::string("Retry-After"),
		                                         std::string("15")));
		return StreamRefused;
	}

	pid_t child = -1;
	const int fd = spawn(r.stream_argv, child);
	if (fd < 0)
		return StreamFailed;

	/* The socket the answer goes out on, learned once here. It is what the
	   watcher looks at to find out that nobody is listening any more; a
	   connection that will not name one is still served, and is still held to
	   the pipe alone. */
	const union MHD_ConnectionInfo *seat =
		MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CONNECTION_FD);

	Session *s = new Session;
	s->conn = connection;
	s->fd = fd;
	s->sock = (seat != NULL) ? (int) seat->connect_fd : -1;
	s->child = child;

	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
		/* Looked at a second time, because the program was started with the
		   lock given up and another request may have taken the last place in
		   between. Giving this one back is cheaper than holding a lock across
		   a fork. */
		if (!serving_ || sessions().size() >= kMaxSessions)
		{
			room = false;
		}
		else
		{
			sessions().push_back(s);
		}
	}

	if (!room)
	{
		::close(fd);
		endChild(child);
		delete s;
		refusal = problemResponse(StatusServiceUnavailable,
		                          coreapi::ErrorCode::TooManyConversions,
		                          "this box is already converting as many streams as it will");
		refusal.headers.push_back(std::make_pair(std::string("Retry-After"),
		                                         std::string("15")));
		return StreamRefused;
	}

	struct MHD_Response *response =
		MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, kBlockSize,
		                                  &readSession, s, &sessionGone);
	if (response == NULL)
	{
		// Nothing was handed over, so nothing will call the free callback and
		// this is the one path that gives the session back itself.
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
			std::vector<Session *> &list = sessions();
			for (size_t i = 0; i < list.size(); ++i)
			{
				if (list[i] != s)
					continue;
				list.erase(list.begin() + (std::ptrdiff_t) i);
				break;
			}
		}
		::close(fd);
		endChild(child);
		delete s;
		return StreamFailed;
	}

	bool added = MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE,
	                                     r.content_type.c_str()) == MHD_YES;
	for (size_t i = 0; added && i < r.headers.size(); ++i)
		added = MHD_add_response_header(response, r.headers[i].first.c_str(),
		                                r.headers[i].second.c_str()) == MHD_YES;

	const MHD_Result queued =
		added ? MHD_queue_response(connection, StatusOk, response) : MHD_NO;

	/* The reference this call holds, given back either way. The connection
	   holds a second one the queue took, and dropping this one where the queue
	   was refused is what runs the free callback. */
	MHD_destroy_response(response);
	return (queued == MHD_YES) ? StreamOpened : StreamFailed;
}

void start()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	serving_ = true;
	if (watching_)
		return;

	int pipefd[2] = { -1, -1 };
	if (::pipe(pipefd) != 0)
	{
		serving_ = false;
		return;
	}
	::fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
	::fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
	::fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
	wake_read_ = pipefd[0];
	wake_write_ = pipefd[1];

	watching_ = true;
	if (pthread_create(&watcher_, NULL, &watch, NULL) != 0)
	{
		/* Without the watcher a session that runs dry is never woken again, so
		   sessions are refused rather than served by nobody. */
		watching_ = false;
		serving_ = false;
		::close(wake_read_);
		::close(wake_write_);
		wake_read_ = -1;
		wake_write_ = -1;
	}
}

void stop()
{
	pthread_t joining;
	bool join = false;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
		serving_ = false;
		join = watching_;
		joining = watcher_;
		watching_ = false;
	}
	nudge();

	/* Every session is asked to end and then waited for. The library gives a
	   connection back on a thread of its own, so what this waits for is that
	   call having run, and until it has there is a suspended connection this
	   daemon may not be stopped under. */
	while (closeEvery() > 0)
		sleepMs(kEndStepMs);

	if (join)
		pthread_join(joining, NULL);

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	if (wake_read_ >= 0)
		::close(wake_read_);
	if (wake_write_ >= 0)
		::close(wake_write_);
	wake_read_ = -1;
	wake_write_ = -1;
}

} // namespace livestream
} // namespace httpd
