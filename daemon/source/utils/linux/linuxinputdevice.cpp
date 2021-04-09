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
//  linuxinoutdevice.cpp
//  SkyBluetoothRcu
//

#include "linuxinputdevice.h"
#include "linux/linuxinputdeviceinfo.h"

#include "logging.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/stat.h>

#if defined(Q_OS_LINUX)
#  include <linux/types.h>
#  include <linux/input.h>
#endif


LinuxInputDevice::LinuxInputDevice(QObject *parent)
	: InputDevice(parent)
	, m_fd(-1)
	, m_notifier(nullptr)
	, m_scanCode(0)
{
}

LinuxInputDevice::LinuxInputDevice(const QString &name, QObject *parent)
	: InputDevice(parent)
	, m_fd(-1)
	, m_notifier(nullptr)
	, m_scanCode(0)
{
	const QList<LinuxInputDeviceInfo> devices = LinuxInputDeviceInfo::availableDevices();
	for (const LinuxInputDeviceInfo &deviceInfo : devices) {
		if (deviceInfo.name() == name) {
			openInputDevNode(deviceInfo.path());
			break;
		}
	}


}

// -----------------------------------------------------------------------------
/*!



 */
LinuxInputDevice::LinuxInputDevice(int fd, QObject *parent)
	: InputDevice(parent)
	, m_fd(-1)
	, m_notifier(nullptr)
	, m_scanCode(0)
{
	// dup the input device fd
	m_fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
	if (m_fd < 0) {
		qErrnoWarning(errno, "failed to dup input device fd");
		return;
	}

	// create a notifier for events
	m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read);
	QObject::connect(m_notifier, &QSocketNotifier::activated,
	                 this, &LinuxInputDevice::onNotification);

	m_notifier->setEnabled(true);
}

// -----------------------------------------------------------------------------
/*!



 */
LinuxInputDevice::LinuxInputDevice(const LinuxInputDeviceInfo &inputDeviceInfo,
                                   QObject *parent)
	: InputDevice(parent)
	, m_fd(-1)
	, m_notifier(nullptr)
	, m_scanCode(0)
{
	openInputDevNode(inputDeviceInfo.path());
}

// -----------------------------------------------------------------------------
/*!



 */
LinuxInputDevice::~LinuxInputDevice()
{
	if (m_notifier) {
		m_notifier->setEnabled(false);
		delete m_notifier;
	}

	if ((m_fd >= 0) && (::close(m_fd) != 0)) {
		qErrnoWarning(errno, "failed to close input device node");
	}

}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the device was created successifully and connected to
	an actual device node.

 */
bool LinuxInputDevice::isValid() const
{
	return (m_fd >= 0);
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
bool LinuxInputDevice::openInputDevNode(const QString &path)
{
	// open the device node
	int devFd = ::open(qPrintable(path), O_CLOEXEC | O_NONBLOCK | O_RDONLY);
	if (devFd < 0) {
		qErrnoWarning(errno, "failed to open '%s'", qPrintable(path));
		return false;
	}

	// sanity check the device we opened is an input device type
	struct stat buf;
	if ((fstat(devFd, &buf) != 0) ||
	    !S_ISCHR(buf.st_mode) ||
	    !LinuxInputDeviceInfo::isInputEventDeviceNumber(buf.st_rdev)) {
		qErrnoWarning(errno, "failed verify device number of '%s'",
		              qPrintable(path));
		close(devFd);
		return false;
	}

	// store the fd and the path
	m_fd = devFd;

	// create a notifier for events
	m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read);
	QObject::connect(m_notifier, &QSocketNotifier::activated,
	                 this, &LinuxInputDevice::onNotification);

	m_notifier->setEnabled(true);

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
void LinuxInputDevice::onNotification(int fd)
{
	// sanity check
	if (Q_UNLIKELY(fd != m_fd)) {
		qError("mismatch file descriptor");
		return;
	}

#if defined(Q_OS_LINUX)

	// populate the vector
	const unsigned maxInputEvents = 16;
	struct input_event inputEvents[maxInputEvents];
	struct iovec iov[maxInputEvents];
	for (size_t i = 0; i < maxInputEvents; i++) {
		iov[i].iov_base = &inputEvents[i];
		iov[i].iov_len = sizeof(struct input_event);
	}

	// read as many events as we can from the dev node
	ssize_t amount = TEMP_FAILURE_RETRY(readv(m_fd, iov, maxInputEvents));
	if (amount < 0) {
		switch (errno) {
			case EAGAIN:
				// we may get EAGAIN as we opened the node in non-blocking mode,
				// ignore this 'error'
				break;
			case ENODEV:
			case ENXIO:
				// we'll get these errors if the device node is closed while
				// reading, handle it by shutting down and removing the node
				qWarning("suspected input device node has been removed");

				// mark this object as invalid
				m_notifier->setEnabled(false);
				::close(m_fd);
				m_fd = -1;

				// emit the signal saying the device node has been removed
				emit deviceRemoved();
				break;
			default:
				qErrnoWarning(errno, "failed to read event dev node");
				break;
		}

	} else if (amount == 0) {
		qWarning("failed to read anything from dev node");

	} else {
		// sanity check the number of bytes read is a multiple of the input
		// event size
		if (Q_UNLIKELY((amount % sizeof(struct input_event)) != 0))
			qWarning("the size of the events read is not a multiple of event size");

		// get the number of events
		const size_t nEvents = (amount / sizeof(struct input_event));
		if (nEvents > 0) {
			// process the events based on input type
			processEvents(inputEvents, nEvents);
		}
	}

#endif // defined(Q_OS_LINUX)
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
void LinuxInputDevice::processEvents(const struct input_event *events, size_t nevents)
{
#if defined(Q_OS_LINUX)

	for (size_t i = 0; i < nevents; i++) {

		const struct input_event *event = &events[i];

		/*
		qDebug("input event { type=%s, code=%hu, value=%d }",
		       (event->type == EV_SYN) ? "EV_SYN" :
		       (event->type == EV_KEY) ? "EV_KEY" :
		       (event->type == EV_REL) ? "EV_REL" :
		       (event->type == EV_ABS) ? "EV_ABS" :
		       (event->type == EV_MSC) ? "EV_MSC" :
		       (event->type == EV_REP) ? "EV_REP" : "UNKNOWN",
		       event->code, event->value);
		*/

		switch (event->type) {
			case EV_SYN:
				m_scanCode = 0;
				break;
			case EV_KEY:
				if (event->value)
					emit keyPress(event->code, m_scanCode);
				else
					emit keyRelease(event->code, m_scanCode);
				m_scanCode = 0;
				break;
			case EV_MSC:
				if (event->code == MSC_SCAN)
					m_scanCode = event->value;
				break;

			default:
				break;
		}
	}

#endif // defined(Q_OS_LINUX)
}
