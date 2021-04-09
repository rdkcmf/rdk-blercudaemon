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
//  unixsignalnotifier.cpp
//  BleRcuDaemon
//

#include "unixsignalnotifier.h"
#include "unixsignalnotifier_p.h"

#include <QCoreApplication>
#include <QAbstractEventDispatcher>
#include <QPointer>
#include <QSocketNotifier>
#include <QMap>
#include <QAtomicInteger>
#include <QReadWriteLock>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(__APPLE__)
#  define pipe2(a, b)  pipe(a)
#endif


static int g_signalPipeWriteFd = -1;


void UnixSignalNotifierDispatcher::signalHandler(int signalNumber)
{
	if (g_signalPipeWriteFd >= 0)
		::write(g_signalPipeWriteFd, &signalNumber, sizeof(int));
}

UnixSignalNotifierDispatcher *UnixSignalNotifierDispatcher::instance(bool create)
{
	static QReadWriteLock lock_;
	static QPointer<UnixSignalNotifierDispatcher> instance_;

	lock_.lockForRead();
	if ((instance_.isNull()) && (create == true)) {

		lock_.unlock();
		lock_.lockForWrite();

		if (instance_.isNull()) {

			instance_ = new UnixSignalNotifierDispatcher(QAbstractEventDispatcher::instance());

			QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
			                 instance_.data(), &QObject::deleteLater);

		}
	}

	lock_.unlock();
	return instance_.data();
}

UnixSignalNotifierDispatcher::UnixSignalNotifierDispatcher(QObject *parent)
	: QObject(parent)
	, m_pipeReadFd(-1)
	, m_pipeNotifier(nullptr)
{
	// create pipe, write end goes to the installed signal handler, read end
	// is given to the socket notifier
	int fds[2] = { -1, -1 };

	// create the pipe
	if (pipe2(fds, O_CLOEXEC) != 0) {
		qErrnoWarning(errno, "failed to create pipe");
		return;
	}

	m_pipeReadFd = fds[0];
	g_signalPipeWriteFd = fds[1];

	//
	m_pipeNotifier = new QSocketNotifier(m_pipeReadFd, QSocketNotifier::Read);
	QObject::connect(m_pipeNotifier, &QSocketNotifier::activated,
	                 this, &UnixSignalNotifierDispatcher::onPipeActivated);

}

UnixSignalNotifierDispatcher::~UnixSignalNotifierDispatcher()
{
	// stop the notifier
	if (m_pipeNotifier) {
		m_pipeNotifier->setEnabled(false);
		delete m_pipeNotifier;
	}

	// restore all the default signal handlers
	{
		QMutexLocker locker(&m_lock);

		const QList<int> enabledSignals = m_signalRefCount.keys();
		for (int unixSignal : enabledSignals) {

			struct sigaction action;
			bzero(&action, sizeof(action));

			action.sa_handler = SIG_DFL;
			sigemptyset(&action.sa_mask);
			action.sa_flags |= SA_RESTART;

			if (sigaction(unixSignal, &action, 0) != 0)
				qErrnoWarning(errno, "failed to install a unix signal handler "
				              "for signal %d", unixSignal);
		}
	}

	// close the read side of the pipe
	if ((m_pipeReadFd >= 0) && (::close(m_pipeReadFd) != 0))
		qErrnoWarning(errno, "failed to close signal pipe");

	// close the write side of the pipe
	int pipeWriteFd = g_signalPipeWriteFd;
	g_signalPipeWriteFd = -1;

	if ((pipeWriteFd >= 0) && (::close(pipeWriteFd) != 0))
		qErrnoWarning(errno, "failed to close signal pipe");
}

void UnixSignalNotifierDispatcher::enableSignal(int unixSignal)
{
	QMutexLocker locker(&m_lock);

	// if already in the map it means it must already be enabled, increment
	// the ref count and return
	if (m_signalRefCount.contains(unixSignal)) {
		m_signalRefCount[unixSignal] += 1;
		return;
	}

	// not in the map so add it and then enable the signal
	m_signalRefCount.insert(unixSignal, 1);

	// install a handler for the signal
	struct sigaction action;
	bzero(&action, sizeof(action));

	action.sa_handler = UnixSignalNotifierDispatcher::signalHandler;
	sigemptyset(&action.sa_mask);
	action.sa_flags |= SA_RESTART;

	if (sigaction(unixSignal, &action, 0) != 0) {
		qErrnoWarning(errno, "failed to install a unix signal handler for "
		              "signal %d", unixSignal);
	}
}

void UnixSignalNotifierDispatcher::disableSignal(int unixSignal)
{
	QMutexLocker locker(&m_lock);

	// if not already in the map then it was never enabled ?
	if (Q_UNLIKELY(!m_signalRefCount.contains(unixSignal))) {
		qWarning("unix signal %d not enabled", unixSignal);
		return;
	}

	// decrement the count and if zero remove it and remove signal handler
	m_signalRefCount[unixSignal] -= 1;
	if (m_signalRefCount[unixSignal] == 0) {
		m_signalRefCount.remove(unixSignal);

		struct sigaction action;
		bzero(&action, sizeof(action));

		action.sa_handler = SIG_DFL;
		sigemptyset(&action.sa_mask);
		action.sa_flags |= SA_RESTART;

		if (sigaction(unixSignal, &action, 0) != 0) {
			qErrnoWarning(errno, "failed to set default unix signal handler for "
			              "signal %d", unixSignal);
		}
	}

}

void UnixSignalNotifierDispatcher::onPipeActivated(int fd)
{
	Q_ASSERT(m_pipeReadFd == fd);

	// read the signal numbe from the fd
	int unixSignal = 0;
	if (TEMP_FAILURE_RETRY(::read(m_pipeReadFd, &unixSignal, sizeof(int))) != sizeof(int)) {
		qErrnoWarning(errno, "failed to read signal number from pipe");
		return;
	}

	//
	emit activated(unixSignal);
}


// -----------------------------------------------------------------------------
/*!
	\class UnixSignalNotifier
	\brief The UnixSignalNotifier class provides support for monitoring unix
	signals.

	When a unix signal notifier object is created (and enabled) it will disable
	the default handling of the unix signal and instead emit the activated()
	Qt signal.

	More than one unix signal notifier can be attached to the same signal,
	however it's important to note that if any of the attached notifiers are
	enabled then the default signal handling is disabled.  It's only when all
	notifier's to the signal are destroyed or disabled will the default
	signal handling be restored.

 */


// -----------------------------------------------------------------------------
/*!
	Constructs a unix signal notifier with the given \a parent.  It's
	activated() signal will be emitted when the signal \a unixSignal is raised.

	Unix signal notifiers are created with the notifier enabled, to disable
	call setEnabled(false).

	\sa setEnabled(), isEnabled()
 */
UnixSignalNotifier::UnixSignalNotifier(int unixSignal, QObject *parent)
	: QObject(parent)
	, m_unixSignal(unixSignal)
	, m_enabled(false)
{
	// get a pointer to the dispatcher singleton
	UnixSignalNotifierDispatcher *dispatcher =
		UnixSignalNotifierDispatcher::instance(true);
	if (!dispatcher) {
		qWarning("failed to get unix signal dispatcher instance");
		return;
	}

	// connect to the dispatchers activate signal
	QObject::connect(dispatcher, &UnixSignalNotifierDispatcher::activated,
	                 this, &UnixSignalNotifier::onSignalActivated);

	// enable the signal
	dispatcher->enableSignal(m_unixSignal);
	m_enabled = true;
}

// -----------------------------------------------------------------------------
/*!
	Destroys this unix signal notifier.

 */
UnixSignalNotifier::~UnixSignalNotifier()
{
	if (m_enabled) {
		UnixSignalNotifierDispatcher *dispatcher =
			UnixSignalNotifierDispatcher::instance();
		if (dispatcher)
			dispatcher->disableSignal(m_unixSignal);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called by the \l{UnixSignalNotifierDispatcher} when any enabled signal
	is raised, here we check if we're enabled and the signal matches the one
	we're notifing for. If yes to both then we emit our \a activated() signal.

 */
void UnixSignalNotifier::onSignalActivated(int unixSignal)
{
	qDebug("received signal %d", unixSignal);

	if (m_enabled && (unixSignal == m_unixSignal))
		emit activated(m_unixSignal);
}

// -----------------------------------------------------------------------------
/*!
	\fn void UnixSignalNotifier::activated(int unixSignal)

	This signal is emitted whenever the unix signal notifier is enabled and
	a unix signal corresponding to the number set in the constructor is raised.

	The unix signal number is passed in the \a unixSignal parameter.

	\sa unixSignal()
 */

// -----------------------------------------------------------------------------
/*!
	Returns true if the notifier is \c enabled; otherwise returns \c false.

	\sa setEnabled()
 */
bool UnixSignalNotifier::isEnabled() const
{
	return m_enabled;
}

// -----------------------------------------------------------------------------
/*!
	Returns the unix socket number specified to the constructor.

 */
int UnixSignalNotifier::unixSignal() const
{
	return m_unixSignal;
}

// -----------------------------------------------------------------------------
/*!
	If \a enable is true, the notifier is enabled; otherwise the notifier is
	disabled.

	The notifier is enabled by default, i.e. it emits the activated() signal
	whenever a unix signal corresponding to the number set in the constructor
	is raised. If it is disabled, it ignores the unix signal (the same effect as
	not creating the unix signal notifier).

	\sa isEnabled(), activated()
 */
void UnixSignalNotifier::setEnabled(bool enable)
{
	if (m_enabled != enable) {

		UnixSignalNotifierDispatcher *dispatcher =
			UnixSignalNotifierDispatcher::instance();
		if (!dispatcher && enable) {
			qWarning("can't enable unix signal notifier as dispatcher has disappeared");
			return;
		}

		if (dispatcher) {
			if (enable)
				dispatcher->enableSignal(m_unixSignal);
			else
				dispatcher->disableSignal(m_unixSignal);
		}

		m_enabled = enable;
	}
}



