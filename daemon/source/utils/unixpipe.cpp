/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2017-2020 Sky UK
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

//
//  unixpipe.cpp
//  BleRcuDaemon
//

#include "unixpipe.h"
#include "logging.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


#if defined(__APPLE__)
#  define pipe2(a, b)  pipe(a)
#endif


UnixPipe::UnixPipe()
	: m_pipeWriteFd(-1)
	, m_pipeReadFd(-1)
	, m_lastError(0)
{
	int fds[2] = { -1, -1 };

	// create the pipe
	if (pipe2(fds, O_CLOEXEC) != 0) {
		m_lastError = errno;
		qErrnoWarning(m_lastError, "failed to create pipe");
		return;
	}

	// store by the read and write fds
	m_pipeReadFd = fds[0];
	m_pipeWriteFd = fds[1];
}

UnixPipe::UnixPipe(const QString &pathName)
	: m_pipeWriteFd(-1)
	, m_pipeReadFd(-1)
	, m_lastError(0)
{
	const QByteArray path_ = pathName.toLatin1();

	// create the fifo
	if (::mkfifo(path_.constData(), 0666) != 0) {

		// if the fifo already exists then just proceed (assuming the object
		// at the path is a fifo)
		if (errno != EEXIST) {
			m_lastError = errno;
			qErrnoWarning(m_lastError, "failed to create fifo @ '%s'",
			              path_.constData());
			return;
		}
	}

	// try and open the write side (only) of the fifo
	m_pipeWriteFd = ::open(path_.constData(), O_WRONLY | O_CLOEXEC);
	if (m_pipeWriteFd < 0) {
		m_lastError = errno;
		qErrnoWarning(m_lastError, "failed to open write side of fifo @ '%s'",
		              path_.constData());
		return;
	}
}

UnixPipe::~UnixPipe()
{
	close();
}

bool UnixPipe::isValid() const
{
	return (m_pipeWriteFd >= 0);
}

bool UnixPipe::isClosed() const
{
	return (m_pipeWriteFd >= 0);
}

int UnixPipe::lastError() const
{
	return m_lastError;
}

int UnixPipe::takeReadDescriptor()
{
	int readFd = m_pipeReadFd;
	m_pipeReadFd = -1;

	return readFd;
}

void UnixPipe::close()
{
	if ((m_pipeWriteFd >= 0) && (::close(m_pipeWriteFd) != 0))
		qErrnoWarning(errno, "failed to close write side of pipe/fifo");

	if ((m_pipeReadFd >= 0) && (::close(m_pipeReadFd) != 0))
		qErrnoWarning(errno, "failed to close read side of pipe");

	m_pipeWriteFd = m_pipeReadFd = -1;
}

ssize_t UnixPipe::write(const quint8 *data, size_t maxSize)
{
	if (Q_UNLIKELY(m_pipeWriteFd < 0)) {
		m_lastError = EBADF;
		return -1;
	}

	ssize_t written = TEMP_FAILURE_RETRY(::write(m_pipeWriteFd, data, maxSize));
	if (written < 0) {
		m_lastError = errno;
		qErrnoWarning(m_lastError, "failed to write to pipe");
		return -1;
	}

	return written;
}

ssize_t UnixPipe::write(const QByteArray &byteArray)
{
	return write(reinterpret_cast<const quint8*>(byteArray.constData()),
	             static_cast<size_t>(byteArray.length()));
}
