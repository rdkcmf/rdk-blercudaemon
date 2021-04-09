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
//  unixpipenotifier.cpp
//  SkyBluetoothRcu
//

#include "unixpipenotifier.h"
#include "logging.h"

#include <cstdint>
#include <cinttypes>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#if defined(__linux__)
#  include <sys/epoll.h>

#elif defined(__APPLE__)
#  include <sys/types.h>
#  include <sys/event.h>
#  include <sys/time.h>

#endif


// -----------------------------------------------------------------------------
/*!
	\class UnixPipeNotifier
	\brief Better version of QSocketNotifier to use for unix pipes & fifos.

	The problem with the \l{QSocketNotifier} object is that it has different
	behaviour on different platforms and different event loops in regards to
	the \l{QSocketNotifier::Exception} listener, in addition QT recommends you
	don't use it.  However without a correctly working detection of 'Exceptions'
	we can't detect when the opposite side of the pipe is closed, and this is
	generally quite important for handling pipes correctly.
 
	So this class is an attempt to fix the deficiency, it provides a similar
	API to \l{QSocketNotifier} but allows you to enable all event types on
	a single object, rather than having one object per event type.  It correctly
	implements 'Exception' events by using epoll on Linux and kqueue on OSX.
 
	Unlike \l{QSocketNotifier} all events are disabled at construction time,
	clients have to explicitly enable the event source before the event signals
	will be emitted.

 */

// -----------------------------------------------------------------------------
/*!
	\fn void UnixPipeNotifier::readActivated(int pipeFd)

	Signal emitted if read events are enabled and the wrapped pipe has data
	to be read.

 */

// -----------------------------------------------------------------------------
/*!
	\fn void UnixPipeNotifier::writeActivated(int pipeFd)

	Signal emitted if write events are enabled and the pipe can be written to,
	i.e. it is not full.

 */

// -----------------------------------------------------------------------------
/*!
	\fn void UnixPipeNotifier::exceptionActivated(int pipeFd)

	Signal emitted if exception events are enabled and one of the following
	conditions has occured:
	- The read end of a write pipe has been closed.
	- The write end of a read pipe has been closed.
	- The pipe fd has been closed.

 */



// -----------------------------------------------------------------------------
/*!
	Constructs a unix pipe notifier with the given \a parent. All events on
	the pipe are initially disabled.

	This does not take ownship of the pipe or dups it, meaning the supplied
	\a pipeFd must remain valid for the lifetime of the constructed object.

	\see setReadEnabled(), setWriteEnabled() and setExceptionEnabled()
 */
UnixPipeNotifier::UnixPipeNotifier(int pipeFd, QObject *parent)
	: QObject(parent)
	, m_pipeFd(pipeFd)
	, m_eventFlags(0)
#if defined(__linux__)
	, m_monitorFd(-1)
	, m_notifier(nullptr)
#else
	, m_readNotifier(nullptr)
	, m_writeNotifier(nullptr)
	, m_exceptionNotifier(nullptr)
#endif
{
	// check the fd is valid
	if ((fcntl(m_pipeFd, F_GETFL) == -1) && (errno == EBADF)) {
		qError("invalid pipe fd");
		return;
	}

#if defined(__linux__)
	// create the epoll fd
	m_monitorFd = epoll_create1(EPOLL_CLOEXEC);
	if (m_monitorFd < 0) {
		qErrnoWarning(errno, "failed to create epoll");
		return;
	}

	// add the pipe fd with no event flags
	struct epoll_event event;
	bzero(&event, sizeof(event));
	event.events = 0;

	if (epoll_ctl(m_monitorFd, EPOLL_CTL_ADD, m_pipeFd, &event) < 0) {
		qErrnoWarning(errno, "failed to add fd to epoll");
		return;
	}

	// create a socket notifier attached to epoll
	m_notifier = new QSocketNotifier(m_monitorFd, QSocketNotifier::Read, this);

	// and attach to a slot
	QObject::connect(m_notifier, &QSocketNotifier::activated,
	                 this, &UnixPipeNotifier::onMonitorActivated);

#else
	// create a socket notifier attached to epoll
	m_readNotifier = new QSocketNotifier(m_pipeFd, QSocketNotifier::Read, this);
	m_readNotifier->setEnabled(false);

	m_writeNotifier = new QSocketNotifier(m_pipeFd, QSocketNotifier::Write, this);
	m_writeNotifier->setEnabled(false);

	m_exceptionNotifier = new QSocketNotifier(m_pipeFd, QSocketNotifier::Exception, this);
	m_exceptionNotifier->setEnabled(false);

	// chain the notifiers on to this objects signals
	QObject::connect(m_readNotifier, &QSocketNotifier::activated,
	                 this, &UnixPipeNotifier::readActivated);
	QObject::connect(m_writeNotifier, &QSocketNotifier::activated,
	                 this, &UnixPipeNotifier::writeActivated);
	QObject::connect(m_exceptionNotifier, &QSocketNotifier::activated,
	                 this, &UnixPipeNotifier::exceptionActivated);

#endif

}

// -----------------------------------------------------------------------------
/*!
	Destroys this unix pipe notifier.

 */
UnixPipeNotifier::~UnixPipeNotifier()
{
#if defined(__linux__)
	// the notifier is owned by us so will be deleted when we're destructed
	if (m_notifier)
		m_notifier->setEnabled(false);

	// close down epoll / kqueue
	if ((m_monitorFd >= 0) && (::close(m_monitorFd) != 0))
		qErrnoWarning(errno, "failed to close monitor fd");

#else
	// the notifiers are owned by us so will be deleted when we're destructed
	if (m_readNotifier)
		m_readNotifier->setEnabled(false);
	if (m_writeNotifier)
		m_writeNotifier->setEnabled(false);
	if (m_exceptionNotifier)
		m_exceptionNotifier->setEnabled(false);

#endif
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the read notifier is enabled; otherwise returns \c false.

	\see setReadEnabled().
 */
bool UnixPipeNotifier::isReadEnabled() const
{
	return ((m_eventFlags & POLLIN) == POLLIN) ? true : false;
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if read notifier is enabled; otherwise returns \c false.

 */
void UnixPipeNotifier::setReadEnabled(bool enable)
{
	const quint32 flag = enable ? POLLIN : 0;
	if ((m_eventFlags ^ flag) & POLLIN) {

		m_eventFlags &= ~POLLIN;
		m_eventFlags |= flag;

#if defined(__linux__)
		struct epoll_event event;
		bzero(&event, sizeof(event));
		if (m_eventFlags & POLLIN)
			event.events |= EPOLLIN;
		if (m_eventFlags & POLLOUT)
			event.events |= EPOLLOUT;

		if (epoll_ctl(m_monitorFd, EPOLL_CTL_MOD, m_pipeFd, &event) < 0)
			qErrnoWarning(errno, "failed to modify epoll");

#else
		m_readNotifier->setEnabled(enable);

#endif
	}
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the write notifier is enabled; otherwise returns \c false.

	\see setWriteEnabled().
 */
bool UnixPipeNotifier::isWriteEnabled() const
{
	return ((m_eventFlags & POLLOUT) == POLLOUT) ? true : false;
}

void UnixPipeNotifier::setWriteEnabled(bool enable)
{
	const quint32 flag = enable ? POLLOUT : 0;
	if ((m_eventFlags ^ flag) & POLLOUT) {

		m_eventFlags &= ~POLLOUT;
		m_eventFlags |= flag;

#if defined(__linux__)
		struct epoll_event event;
		bzero(&event, sizeof(event));
		if (m_eventFlags & POLLIN)
			event.events |= EPOLLIN;
		if (m_eventFlags & POLLOUT)
			event.events |= EPOLLOUT;

		if (epoll_ctl(m_monitorFd, EPOLL_CTL_MOD, m_pipeFd, &event) < 0)
			qErrnoWarning(errno, "failed to modify epoll");

#else
		m_writeNotifier->setEnabled(enable);

#endif
	}
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the exception notifier is enabled; otherwise returns
	\c false.

	\see setExceptionEnabled().
 */
bool UnixPipeNotifier::isExceptionEnabled() const
{
	return ((m_eventFlags & POLLERR) == POLLERR) ? true : false;
}

void UnixPipeNotifier::setExceptionEnabled(bool enable)
{
	if (enable)
		m_eventFlags |= POLLERR;
	else
		m_eventFlags &= ~POLLERR;

#if !defined(__linux__)
	m_exceptionNotifier->setEnabled(enable);
#endif
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when epoll is woken by an event.
 */
void UnixPipeNotifier::onMonitorActivated(int fd)
{
#if !defined(__linux__)

	Q_UNUSED(fd);

#else // defined(__linux__)

	// sanity check the supplied fd
	if (Q_UNLIKELY(fd != m_monitorFd)) {
		qWarning("odd, invalid fd supplied in slot (expected=%d, actual=%d)",
		         m_monitorFd, fd);
		return;
	}

	// read a single event from epoll
	struct epoll_event event;
	int n = TEMP_FAILURE_RETRY(epoll_wait(m_monitorFd, &event, 1, 0));
	if (n < 0) {
		qErrnoWarning(errno, "epoll_wait failed");
		return;
	} else if (n == 0) {
		qWarning("epoll woken but no events?");
		return;
	}

	// emit signals for the various events if they enabled
	if (event.events & EPOLLIN) {
		if (m_eventFlags & POLLIN)
			emit readActivated(m_pipeFd);
		else
			qInfo("received a read event whilst disabled");
	}

	if (event.events & EPOLLOUT) {
		if (m_eventFlags & POLLOUT)
			emit writeActivated(m_pipeFd);
		else
			qInfo("received a write event whilst disabled");
	}

	if (event.events & (EPOLLERR | EPOLLHUP)) {
		if (m_eventFlags & POLLERR)
			emit exceptionActivated(m_pipeFd);
		else
			qInfo("received an unhandled exception event");
	}
#endif // defined(__linux__)
}


