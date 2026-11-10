/*
	(C)2026 TangoCash

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the
	Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* What a Python script started from outside the box reaches, the way
 * luaserver.cpp serves a Lua one: a socket at /tmp/pyclient.sock, a client
 * that hands it a script and its arguments, and the box's own main loop
 * letting the script run. */

#include <config.h>

#ifdef ENABLE_PYTHON

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <unistd.h>
#include <poll.h>
#include <fcntl.h>

#include <global.h>
#include <neutrino.h>
#include <system/helpers.h>
#include <system/set_threadname.h>
#include <pyclient/pyclient.h>
#include <gui/python/pyinstance.h>

#include "pyserver.h"

static CPythonServer *instance = NULL;
static pthread_mutex_t mutex;

CPythonServer *CPythonServer::getInstance()
{
	if (!instance)
		instance = new CPythonServer();
	return instance;
}

void CPythonServer::destroyInstance()
{
	Lock();
	if (instance) {
		delete instance;
		instance = NULL;
	}
	UnLock();
}

CPythonServer::CPythonServer()
{
	count = 0;
	did_run = false;

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
	pthread_mutex_init(&mutex, &attr);
	pthread_mutexattr_destroy(&attr);

	sem_init(&may_run, 0, 0);

	pthread_create (&thr, NULL, pyserver_main_thread, (void *) NULL);
}

CPythonServer::~CPythonServer()
{
	pthread_cancel(thr);
	pthread_join(thr, NULL);
	sem_destroy(&may_run);
	pthread_mutex_destroy(&mutex);
	instance = NULL;
}

void CPythonServer::UnBlock()
{
	sem_post(&may_run);
}

bool CPythonServer::Block(const neutrino_msg_t msg, const neutrino_msg_data_t data)
{
	sem_wait(&may_run);
	if (did_run) {
		if (msg != CRCInput::RC_timeout)
			g_RCInput->postMsg(msg, data);
		did_run = false;
		return true;
	}
	return false;
}

void CPythonServer::Lock(void)
{
	pthread_mutex_lock(&mutex);
}

void CPythonServer::UnLock(void)
{
	pthread_mutex_unlock(&mutex);
}

class pyserver_data
{
	public:
		CPythonInstance *python;
		bool died;
		int fd;

		std::vector<std::string> argv;
		std::string script;

		pyserver_data(int _fd, std::string &_script) {
			fd = dup(_fd);
			fcntl(fd, F_SETFD, FD_CLOEXEC);
			script = _script;
			died = false;
			python = NULL;
		}
		~pyserver_data(void) {
			close(fd);
		}
};

/* The client that asked for the script is waited on: when it goes away the
 * script is asked to stop, the way the Lua one is. */
void *CPythonServer::pyclient_watchdog(void *arg) {
	set_threadname(__func__);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	pyserver_data *psd = (class pyserver_data *)arg;

	struct pollfd pfd;
	pfd.fd = psd->fd;
	pfd.events = pfd.revents = 0;
	poll(&pfd, 1, -1);
	psd->python->abortScript();
	psd->died = true;
	pthread_exit(NULL);
}

void *CPythonServer::pyserver_thread(void *arg) {
	set_threadname(__func__);
	Lock();
	if (instance) {
		instance->count++;
		instance->did_run = true;
	}
	UnLock();

	pyserver_data *psd = (class pyserver_data *)arg;

	/* The watchdog aborts this instance as soon as the client is gone, which
	   may already be the case when a request had to wait for may_run. */
	CPythonInstance *python = new CPythonInstance();
	psd->python = python;

	pthread_t wdthr;
	pthread_create (&wdthr, NULL, pyclient_watchdog, (void *) psd);

	std::string result_code;
	std::string result_string;
	std::string error_string;
	python->runScript(psd->script.c_str(), &psd->argv, &result_code, &result_string, &error_string);
	pthread_cancel(wdthr);
	pthread_join(wdthr, NULL);
	if (!psd->died) {
		size_t result_code_len = result_code.length() + 1;
		size_t result_string_len = result_string.length() + 1;
		size_t error_string_len = error_string.length() + 1;
		size_t size = result_code_len + result_string_len + error_string_len;
		char result[size + sizeof(size)];
		char *rp = result;
		memcpy(rp, &size, sizeof(size));
		rp += sizeof(size);
		size += sizeof(size);
		memcpy(rp, result_code.c_str(), result_code_len);
		rp += result_code_len;
		memcpy(rp, result_string.c_str(), result_string_len);
		rp += result_string_len;
		memcpy(rp, error_string.c_str(), error_string_len);
		rp += error_string_len;
		CBasicServer::send_data(psd->fd, result, size);
	}
	delete python;
	delete psd;

	Lock();
	if (instance) {
		instance->count--;
		if (!instance->count)
			sem_post(&instance->may_run);
	}
	UnLock();
	pthread_exit(NULL);
}

/* The name the client handed over is looked for as a file, and then in the
 * two directories a Python plugin stands in, with a .py added. */
bool CPythonServer::pyserver_parse_command(CBasicMessage::Header &rmsg __attribute__((unused)), int connfd)
{
	size_t size;

	if (!CBasicServer::receive_data(connfd, &size, sizeof(size))) {
		fprintf(stderr, "%s %s %d: receive_data failed\n", __file__, __func__, __LINE__);
		return true;
	}
	char data[size];
	if (!CBasicServer::receive_data(connfd, data, size)) {
		fprintf(stderr, "%s %s %d: receive_data failed\n", __file__, __func__, __LINE__);
		return true;
	}
	if (data[size - 1]) {
		fprintf(stderr, "%s %s %d: unterminated string\n", __file__, __func__, __LINE__);
		return true;
	}
	std::string pyscript;
	if (data[0] == '/')
		pyscript = data;
	const char * path[2] = { PYTHONPLUGINDIR_VAR, PYTHONPLUGINDIR };
	for (unsigned int i = 0; i < 2; i++)
	{
		std::string filename = path[i];
		filename += "/";
		filename += data;
		filename += ".py";
		if (access(filename, R_OK) == 0)
		{
			pyscript = filename;
			break;
		}
	}
	if (access(pyscript, R_OK) != 0)
	{
		fprintf(stderr, "%s %s %d: %s not found\n", __file__, __func__, __LINE__, data);
		const char *result_code = "-1";
		const char *result_string = "";
		std::string error_string = data;
		error_string += " not found\n";
		size_t result_code_len = strlen(result_code) + 1;
		size_t result_string_len = strlen(result_string) + 1;
		size_t error_string_len = error_string.size() + 1;
		size = result_code_len + result_string_len + error_string_len;
		char result[size + sizeof(size)];
		char *rp = result;
		memcpy(rp, &size, sizeof(size));
		rp += sizeof(size);
		size += sizeof(size);
		memcpy(rp, result_code, result_code_len);
		rp += result_code_len;
		memcpy(rp, result_string, result_string_len);
		rp += result_string_len;
		memcpy(rp, error_string.c_str(), error_string_len);
		rp += error_string_len;
		CBasicServer::send_data(connfd, result, size);
		return true;
	}
	pyserver_data *psd = new pyserver_data(connfd, pyscript);
	char *datap = data;
	while (size > 0) {
		psd->argv.push_back(std::string(datap));
		size_t len = strlen(datap) + 1;
		datap += len;
		size -= len;
	}

	Lock();
	if (!instance->count)
		sem_wait(&instance->may_run);
	UnLock();

	pthread_t thr;
	pthread_create (&thr, NULL, pyserver_thread, (void *) psd);
	pthread_detach(thr);

	return true;
}

void *CPythonServer::pyserver_main_thread(void *) {
	set_threadname(__func__);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	CBasicServer server;
	if (!server.prepare(PYCLIENT_UDS_NAME)) {
		fprintf(stderr, "%s %s %d: prepare failed\n", __file__, __func__, __LINE__);
		pthread_exit(NULL);
	}

	server.run(pyserver_parse_command, PYCLIENT_VERSION);
	pthread_exit(NULL);
}

#endif /* ENABLE_PYTHON */