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
//  blegattnotifypipe.cpp
//  SkyBluetoothRcu
//

#include "blegattnotifypipe.h"

#include "utils/unixpipenotifier.h"
#include "utils/logging.h"

#include <QDebug>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>



// -----------------------------------------------------------------------------
/*!
	Constructs a BleGattNotifyPipe object wrapping the supplied \a notifyPipeFd
	descriptor.  The \a mtu value describes the maximum transfer size for
	each notification.

	The class dup's the supplied descriptor so it should / can be closed after
	construction.

 */
BleGattNotifyPipe::BleGattNotifyPipe(const QDBusUnixFileDescriptor &notifyPipeFd,
                                     quint16 mtu, QObject *parent)
	: QObject(parent)
	, m_pipeFd(-1)
	, m_notifier(nullptr)
	, m_bufferSize(23)
	, m_buffer(nullptr)
{
	// sanity check the input notify pipe
	if (!notifyPipeFd.isValid()) {
		qError("invalid notify pipe fd");
		return;
	}

	// dup the supplied fd
	m_pipeFd = fcntl(notifyPipeFd.fileDescriptor(), F_DUPFD_CLOEXEC, 3);
	if (m_pipeFd < 0) {
		qErrnoWarning(errno, "failed to dup bluez notify pipe");
		return;
	}

	// put in non-blocking mode
	int flags = fcntl(m_pipeFd, F_GETFL);
	if (!(flags & O_NONBLOCK))
		fcntl(m_pipeFd, F_SETFL, flags | O_NONBLOCK);


	// allocate a buffer for each individual notification
	m_bufferSize = mtu;
	if (m_bufferSize < 1) {
		qError("invalid mtu size, defaulting to 23");
		m_bufferSize = 23;
	}
	if (m_bufferSize > PIPE_BUF) {
		qError("mtu size is larger than atomic pipe buffer size");
		m_bufferSize = PIPE_BUF;
	}
	m_buffer = new quint8[m_bufferSize];


	// final stage is to create listeners on the pipe
	m_notifier = new UnixPipeNotifier(m_pipeFd);
	QObject::connect(m_notifier, &UnixPipeNotifier::readActivated,
	                 this, &BleGattNotifyPipe::onActivated);
	QObject::connect(m_notifier, &UnixPipeNotifier::exceptionActivated,
	                 this, &BleGattNotifyPipe::onActivated);

	// enable read and exception (pipe closed) events on the input pipe
	m_notifier->setReadEnabled(true);
	m_notifier->setExceptionEnabled(true);
}

// -----------------------------------------------------------------------------
/*!

 */
BleGattNotifyPipe::~BleGattNotifyPipe()
{
	if (m_notifier) {
		delete m_notifier;
		m_notifier = nullptr;
	}

	if ((m_pipeFd >= 0) && (::close(m_pipeFd) != 0))
		qErrnoWarning(errno, "failed to notification pipe fd");

	if (m_buffer)
		delete [] m_buffer;
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the notification pipe is valid.

 */
bool BleGattNotifyPipe::isValid() const
{
	return (m_pipeFd >= 0);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when there is data available to be read from the input pipe.

 */
void BleGattNotifyPipe::onActivated(int pipeFd)
{
	if (Q_UNLIKELY(pipeFd != m_pipeFd))
		return;

	// read as much as we can from the pipe
	while (m_pipeFd >= 0) {

		// note that bluez sensibly uses the O_DIRECT flag for the pipe so that
		// the data in the pipe is packetised, meaning we must read in 20 byte
		// chunks, and we should only get 20 bytes
		ssize_t rd = TEMP_FAILURE_RETRY(::read(m_pipeFd, m_buffer, m_bufferSize));
		if (rd < 0) {

			// check if the pipe is empty, if not the error is valid
			if ((errno != EWOULDBLOCK) && (errno != EAGAIN))
				qErrnoWarning(errno, "failed to read from pipe");

			break;

		} else if (rd == 0) {
			// a read of zero bytes means the remote end of the pipe has been
			// closed, this usually just means that the RCU has disconnected,
			// this will stop the recording,
			qInfo("notification pipe closed");

			if (m_notifier) {
				m_notifier->setReadEnabled(false);
				m_notifier->setExceptionEnabled(false);
				m_notifier->deleteLater();
				m_notifier = nullptr;
			}

			if (::close(m_pipeFd) != 0)
				qErrnoWarning(errno, "failed to close pipe fd");
			m_pipeFd = -1;

			emit closed();

			break;

		} else {
			// emit a notification signal
			emit notification(QByteArray::fromRawData(reinterpret_cast<const char*>(m_buffer), rd));

		}
	}

}
