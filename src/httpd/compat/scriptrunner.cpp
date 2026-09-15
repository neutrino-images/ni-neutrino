//=============================================================================
// yExecuteScript, as it was in yhttpd_core/helper.cpp. See scriptrunner.h.
//=============================================================================

#include <cstdio>
#include <cerrno>
#include <vector>

#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#include "httpd/compat/scriptrunner.h"
#include "httpd/compat/controlapi.h"

namespace httpd
{
namespace compat
{

//-----------------------------------------------------------------------------
// Script Helpers
//-----------------------------------------------------------------------------
// Start file with the prepared argv in dir and collect its stdout.
// argv and shargv are built by the caller so that the child only makes
// async-signal-safe calls between fork() and execv() (getdtablesize() is a
// getrlimit() wrapper and safe in practice).
// Returns false when fork()/pipe() failed, i.e. no child ran at all. A child
// that could not exec reports itself through an empty result, exactly as the
// previous popen() based version did.
static bool yRunNoShell(const std::string &file, const std::string &dir,
		char *const argv[], char *const shargv[], std::string &out) {
	int fd[2];
	// O_CLOEXEC: without it a fork() in any other thread inherits our write
	// end and keeps it open, so the read loop below would not see EOF until
	// that unrelated child exits. popen() gave the same guarantee for its
	// own pipes, so plain pipe() here would be a regression.
	if (pipe2(fd, O_CLOEXEC) != 0)
		return false;

	pid_t pid = fork();
	if (pid < 0) {
		int err = errno; // keep it: close() may set errno on success
		close(fd[0]);
		close(fd[1]);
		errno = err;
		return false;
	}

	if (pid == 0) { // child
		close(fd[0]); // the child only writes
		if (fd[1] == STDOUT_FILENO) {
			// already in place, but must survive execv()
			int flags = fcntl(fd[1], F_GETFD);
			if (flags < 0 || fcntl(fd[1], F_SETFD, flags & ~FD_CLOEXEC) < 0)
				_exit(127);
		} else {
			if (dup2(fd[1], STDOUT_FILENO) < 0) // dup2 clears FD_CLOEXEC
				_exit(127);
			// close_range() below starts at 3 and would leave a write end
			// behind that happened to land on fd 2
			close(fd[1]);
		}
		// Do not leak the listening socket, open client connections or
		// driver handles into the script; ysocket.cpp creates them without
		// SOCK_CLOEXEC, so O_CLOEXEC on our own pipe does not cover them.
		// close_range() does this in one call - the fallback loop costs a
		// syscall per possible descriptor, which is tens of milliseconds
		// per script once RLIMIT_NOFILE is large (524288 on a PC build).
#if defined(SYS_close_range)
		if (syscall(SYS_close_range, 3U, ~0U, 0U) != 0)
#endif
		{
			int maxfd = getdtablesize();
			for (int i = 3; i < maxfd; i++)
				close(i);
		}
		if (chdir(dir.c_str()) != 0)
			_exit(127);
		execv(file.c_str(), argv);
		// popen() handed everything to /bin/sh, so a script without a
		// shebang used to run. Keep that working, but through argv rather
		// than a shell command line, so nothing is re-interpreted.
		if (errno == ENOEXEC)
			execv("/bin/sh", shargv);
		_exit(127);
	}

	close(fd[1]);
	const size_t readblocklen = 1024; // chunk size, not an output limit
	char buf[readblocklen];
	for (;;) {
		ssize_t n = read(fd[0], buf, readblocklen);
		if (n > 0)
			out.append(buf, (size_t) n);
		else if (n == 0 || errno != EINTR) // EINTR: keep reading, do not truncate
			break;
	}
	close(fd[0]);

	while (waitpid(pid, NULL, 0) < 0 && errno == EINTR)
		;
	return true;
}

std::string yExecuteScript(std::string cmd) {
	std::string script, para, result;
	bool found = false;
	bool launch_failed = false;

	//aprintf("%s: %s\n", __func__, cmd.c_str());

	// split script and parameters
	int pos;
	if ((pos = cmd.find_first_of(" ")) > 0) {
		script = cmd.substr(0, pos);
		para = cmd.substr(pos + 1, cmd.length() - (pos + 1)); // snip
	} else
		script = cmd;
	// get file
	std::string fullfilename;
	script += ".sh"; //add script extension

	// The script name arrives from the same query string as the parameters.
	// Keep it a bare name so it cannot walk out of the plugin directories.
	if (script.find('/') != std::string::npos) {
		printf("%s: refused script name with a path separator: %s\n",
			__func__, script.c_str());
		return "error";
	}

	// Build the argument vector here instead of handing the whole command
	// line to a shell. The parameters come straight from the HTTP query
	// string; going through popen() meant /bin/sh split the words but also
	// interpreted metacharacters in them. Splitting on whitespace only means
	// ';', '|', '$(...)' and quotes all reach the script as plain text, so an
	// apostrophe in a file name survives instead of being eaten as a quote.
	// Callers encode their parameters, so a space still separates arguments.
	std::vector<std::string> args;
	size_t a = 0;
	while (a < para.length()) {
		a = para.find_first_not_of(" \t", a);
		if (a == std::string::npos)
			break;
		size_t e = para.find_first_of(" \t", a);
		if (e == std::string::npos)
			e = para.length();
		args.push_back(para.substr(a, e - a));
		a = e;
	}

	for (unsigned int i = 0; i < CControlAPI::PLUGIN_DIR_COUNT && !found; i++) {
		fullfilename = CControlAPI::PLUGIN_DIRS[i] + "/" + script;
		// X_OK instead of readability: a script without the execute bit was
		// counted as found and then returned silently empty output
		if (access(fullfilename.c_str(), X_OK) != 0)
			continue;

		// build argv before fork() so the child allocates nothing
		std::vector<char *> argv;
		argv.reserve(args.size() + 2);
		argv.push_back(const_cast<char *>(fullfilename.c_str()));
		for (size_t k = 0; k < args.size(); k++)
			argv.push_back(const_cast<char *>(args[k].c_str()));
		argv.push_back(NULL);

		// same list for the ENOEXEC fallback, prefixed with the shell
		char shname[] = "sh";
		std::vector<char *> shargv;
		shargv.reserve(argv.size() + 1);
		shargv.push_back(shname);
		shargv.insert(shargv.end(), argv.begin(), argv.end());

		// the child chdir()s itself; the old code changed the working
		// directory of the whole threaded process for the call's duration
		// A failed fork()/pipe() means resource exhaustion, not "wrong
		// directory": stop instead of running a same-named script from a
		// later one.
		if (!yRunNoShell(fullfilename, CControlAPI::PLUGIN_DIRS[i], &argv[0], &shargv[0], result)) {
			// say what really happened; the script was there, we just
			// could not start it
			// errno number rather than strerror(): nhttpd serves requests in
			// threads, and strerror() hands out a shared static buffer
			printf("%s: cannot start %s: errno %d\n", __func__,
				fullfilename.c_str(), errno);
			launch_failed = true;
			break;
		}
		found = true;
	}

	if (!found) {
		// only a real lookup miss gets the search-path dump - printing it
		// after a resource failure would send an operator hunting for a
		// file that is not missing at all
		if (!launch_failed) {
			printf("%s: script %s not found in:\n", __func__, script.c_str());
			for (unsigned int i = 0; i < CControlAPI::PLUGIN_DIR_COUNT; i++) {
				printf("\t%s\n", CControlAPI::PLUGIN_DIRS[i].c_str());
			}
		}
		result = "error";
	}
	return result;
}

} // namespace compat
} // namespace httpd
