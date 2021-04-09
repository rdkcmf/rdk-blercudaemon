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
//  unixpipesplicer.cpp
//  BleRcuDaemon
//

#include "unixpipesplicer.h"
#include "unixpipenotifier.h"

#include <QVarLengthArray>
#include <QThread>
#include <QTimer>

#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>


// -----------------------------------------------------------------------------
/*!
	\class UnixPipeSplicer
	\brief Object that splices data from one pipe / file to another pipe / file.

	This class is essentially just a state machine that splices data from one
	pipe or file to another.  The object can splice from a pipe to a pipe or
	from a pipe to a file, or from a file to a pipe, but not from a file to
	file.  Note that the data is spliced rather than copied, which means the
	data doesn't pass through userspace, it is copied / moved in the kernel.

	This class uses \l{QSocketNotifier} meaning it needs an event loop running
	to trigger events to request the kernel to splice the data.

	This object can be moved into a thread providing it is running an event
	loop.

 */




long UnixPipeSplicer::m_pageSize = -1;


// -----------------------------------------------------------------------------
/*!
	\internal

	Utility function to just set the \c O_NONBLOCK flag on the suppled \a fd.
 */
static bool _setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL);
	if (flags < 0) {
		qErrnoWarning(errno, "failed to get flags of fd");
		return false;
	}

	int rc = fcntl(fd, F_SETFL, (flags | O_NONBLOCK));
	if (rc < 0) {
		qErrnoWarning(errno, "failed to set non-blocking flags of fd");
		return false;
	}

	return true;
}


// -----------------------------------------------------------------------------
/*!

 */
UnixPipeSplicer::UnixPipeSplicer(int readFd, int writeFd, Mode mode)
	: m_mode(mode)
	, m_readException(false)
	, m_writeException(false)
	, m_inThrowAwayMode(false)
	, m_bytesRx(0)
	, m_bytesTx(0)
{
	// get the page size if we haven't already
	if (m_pageSize <= 0) {
		long pageSize = sysconf(_SC_PAGESIZE);
		__sync_bool_compare_and_swap(&m_pageSize, -1, pageSize);
	}

	// dup the fd's and put both in non-blocking mode
	m_readFd = fcntl(readFd, F_DUPFD_CLOEXEC, 3);
	if (Q_UNLIKELY(m_readFd < 0)) {
		qErrnoWarning(errno, "failed to dup fifo read fd");
		return;
	}
	m_writeFd = fcntl(writeFd, F_DUPFD_CLOEXEC, 3);
	if (Q_UNLIKELY(m_writeFd < 0)) {
		qErrnoWarning(errno, "failed to dup pipe write fd");
		return;
	}

	_setNonBlocking(m_readFd);
	_setNonBlocking(m_writeFd);


	// create socket notifiers for them
	m_readNotifier = QSharedPointer<QSocketNotifier>(
		new QSocketNotifier(m_readFd, QSocketNotifier::Read, this),
		&QObject::deleteLater);

	m_writeNotifier = QSharedPointer<UnixPipeNotifier>(
		new UnixPipeNotifier(m_writeFd, this),
		&QObject::deleteLater);

	// deactivate the notifiers until start() is called
	m_readNotifier->setEnabled(false);
	m_writeNotifier->setWriteEnabled(false);
	m_writeNotifier->setExceptionEnabled(false);

	// connect the fd signals
	QObject::connect(m_readNotifier.data(), &QSocketNotifier::activated,
	                 this, &UnixPipeSplicer::onReadActivated);
	QObject::connect(m_writeNotifier.data(), &UnixPipeNotifier::writeActivated,
	                 this, &UnixPipeSplicer::onWriteActivated);
	QObject::connect(m_writeNotifier.data(), &UnixPipeNotifier::exceptionActivated,
	                 this, &UnixPipeSplicer::onWriteException);
}

// -----------------------------------------------------------------------------
/*!
	Destructor that closes the fifo and pipe descriptors.

 */
UnixPipeSplicer::~UnixPipeSplicer()
{
	if (m_readNotifier)
		m_readNotifier->setEnabled(false);
	if (m_writeNotifier) {
		m_writeNotifier->setWriteEnabled(false);
		m_writeNotifier->setExceptionEnabled(false);
	}

	if ((m_readFd >= 0) && (::close(m_readFd) != 0))
		qErrnoWarning(errno, "failed to close read fd");

	if ((m_writeFd >= 0) && (::close(m_writeFd) != 0))
		qErrnoWarning(errno, "failed to close write fd");
}

// -----------------------------------------------------------------------------
/*!
	\threadsafe

	Returns the number of bytes read from the input pipe.  This count is cleared
	when start() is called.

 */
quint64 UnixPipeSplicer::bytesRx() const
{
	return m_bytesRx.load();
}

// -----------------------------------------------------------------------------
/*!
	\threadsafe

	Returns the number of bytes written to the output pipe.  This count is
	cleared when start() is called.

	The number of bytes written may be less than the number of bytes read if
	the splicer is in \l{UnixPipeSplicer::FreeFlow} and the output pipe has
	been closed.
 */
quint64 UnixPipeSplicer::bytesTx() const
{
	return m_bytesTx.load();
}

// -----------------------------------------------------------------------------
/*!
	\threadsafe

	Starts the splicing process by enabling the read side socket notifier.

 */
void UnixPipeSplicer::start()
{
	if (QObject::thread() == QThread::currentThread())
		doStart();
	else
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
		QTimer::singleShot(0, this, &UnixPipeSplicer::doStart);
#else
	{
		QTimer* timer = new QTimer(this);
		QObject::connect(timer, &QTimer::timeout, this, &UnixPipeSplicer::doStart);
		QObject::connect(timer, &QTimer::timeout, timer, &QObject::deleteLater);
		timer->setSingleShot(true);
		timer->start(0);
	}
#endif
}

void UnixPipeSplicer::doStart()
{
	if (m_readException || m_writeException) {
		qWarning("ignoring start request as read/write exception raised");
		return;
	}

	if (m_readNotifier)
		m_readNotifier->setEnabled(true);
	if (m_writeNotifier)
		m_writeNotifier->setExceptionEnabled(true);

	m_inThrowAwayMode = false;
	m_bytesRx = 0;
	m_bytesTx = 0;

	emit started();
}

// -----------------------------------------------------------------------------
/*!
	\threadsafe

	Stops splicing by disabling both socket notifiers.

 */
void UnixPipeSplicer::stop()
{
	if (QObject::thread() == QThread::currentThread())
		doStop();
	else
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
		QTimer::singleShot(0, this, &UnixPipeSplicer::doStop);
#else
	{
		QTimer* timer = new QTimer(this);
		QObject::connect(timer, &QTimer::timeout, this, &UnixPipeSplicer::doStop);
		QObject::connect(timer, &QTimer::timeout, timer, &QObject::deleteLater);
		timer->setSingleShot(true);
		timer->start(0);
	}
#endif
}

void UnixPipeSplicer::doStop()
{
	if (m_readNotifier)
		m_readNotifier->setEnabled(false);

	if (m_writeNotifier) {
		m_writeNotifier->setWriteEnabled(false);
		m_writeNotifier->setExceptionEnabled(false);
	}

	emit stopped();
}

// -----------------------------------------------------------------------------
/*!
	\threadsafe

	Closes the output pipe of the splicer.

	The behavor of the splicer when the output pipe is closed depends on the
	mode setting; if in the default \l{UnixPipeSplicer::Block} mode then no more
	data is read from the input pipe but it is not closed. 
	In \l{UnixPipeSplicer::FreeFlow} mode data is still read from the input pipe
	but it's thrown away, data is continously read until this object is
	destroyed.

 */
void UnixPipeSplicer::closeWriteSide()
{
	if (QObject::thread() == QThread::currentThread())
		onOutputClosed();
	else
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
		QTimer::singleShot(0, this, &UnixPipeSplicer::onOutputClosed);
#else
	{
		QTimer* timer = new QTimer(this);
		QObject::connect(timer, &QTimer::timeout, this, &UnixPipeSplicer::onOutputClosed);
		QObject::connect(timer, &QTimer::timeout, timer, &QObject::deleteLater);
		timer->setSingleShot(true);
		timer->start(0);
	}
#endif
}

#if !defined(__linux__)
// -----------------------------------------------------------------------------
/*!
	\internal

	\warning This method is only intended to be used for unit testing, it is
	very inefficent and should not be used in production code.

	Copy implementation of splice for non-linux systems.
 */
ssize_t UnixPipeSplicer::splice(int fd_in, off_t *off_in, int fd_out,
                                off_t *off_out, size_t len, unsigned int flags) const
{
	Q_UNUSED(off_in);
	Q_UNUSED(off_out);
	Q_UNUSED(flags);

	Q_ASSERT(off_in == nullptr);
	Q_ASSERT(off_out == nullptr);

	ssize_t result = -1;
	for (size_t i = 0; i < len; i++) {

		// check if the output fd is writable
		struct pollfd fds;
		fds.fd = fd_out;
		fds.events = POLLOUT;
		fds.revents = 0;
		poll(&fds, 1, 0);

		if (!(fds.revents & POLLOUT)) {
			errno = EWOULDBLOCK;
			return result;
		}

		// read a byte
		quint8 byte;
		ssize_t rd = TEMP_FAILURE_RETRY(::read(fd_in, &byte, 1));
		if (rd <= 0)
			return result;

		// write a byte
		ssize_t wr = TEMP_FAILURE_RETRY(::write(fd_out, &byte, 1));
		if (wr <= 0)
			return result;

		if (result < 0)
			result = 1;
		else
			result++;
	}

	return result;
}
#endif // !defined(__linux__)

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when we detect that the output pipe / fifo has been closed.

	This function depends on the mode setting; if in the default
	\l{UnixPipeSplicer::Block} mode then no more data is read from the
	input pipe but it is not closed.  In \l{UnixPipeSplicer::FreeFlow} mode
	data is still read from the input pipe but it's thrown away, data is
	continously read until this object is destroyed.

 */
void UnixPipeSplicer::onOutputClosed()
{
	Q_ASSERT(QObject::thread() == QThread::currentThread());

	// close and destroy the notifiers attached to the write side
	if (m_writeNotifier) {
		m_writeNotifier->setExceptionEnabled(false);
		m_writeNotifier->setWriteEnabled(false);
		m_writeNotifier.clear();
	}

	// close the old write side fd
	if (m_writeFd >= 0) {
		if (::close(m_writeFd) != 0)
			qErrnoWarning(errno, "failed to close output pipe");

		m_writeFd = -1;
	}


	// if the mode is 'free flow' then we swap out the output fd to
	// /dev/null, we can always write to devnull so this means we
	// can continue consuming data until the input pipe is closed
	if (m_mode == FreeFlow) {

		// open /dev/null
		int devNullFd = ::open("/dev/null", O_CLOEXEC | O_WRONLY | O_NONBLOCK);
		if (devNullFd < 0)
			qErrnoWarning(errno, "failed to open /dev/null");

		// and store the new /dev/null fd as the write fd
		m_writeFd = devNullFd;

		// entered free flow mode
		m_inThrowAwayMode = true;

	} else {

		//
		if (m_readNotifier)
			m_readNotifier->setEnabled(false);

	}

	// if not already raised a write exception then do so now
	if (!m_writeException) {
		m_writeException = true;
		emit writeException();
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Private slot called when an 'exception' is detected on the write pipe, an
	exception usually means that the write pipe has been closed 

 */
void UnixPipeSplicer::onWriteException(int fd)
{
	Q_ASSERT(QObject::thread() == QThread::currentThread());

	qWarning("detected output pipe is closed");

	// sanity check the supplied fd
	if (Q_UNLIKELY(fd != m_writeFd)) {
		qWarning("odd, invalid fd supplied in slot (expected=%d, actual=%d)",
		         m_writeFd, fd);
		return;
	}

	// handle the output close based on the mode
	onOutputClosed();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Private slot called when there is data that can be read from the input
	fifo.

 */
void UnixPipeSplicer::onReadActivated(int fd)
{
	Q_ASSERT(QObject::thread() == QThread::currentThread());

	// sanity check the supplied fd
	if (Q_UNLIKELY(fd != m_readFd)) {
		qWarning("odd, invalid fd supplied in slot (expected=%d, actual=%d)",
		         m_readFd, fd);
		return;
	}

	// sanity check we haven't already detected a pipe close
	if (Q_UNLIKELY(m_readException)) {
		qWarning("odd, socket notifier called after detecting pipe closed");
		m_readNotifier->setEnabled(false);
		return;
	}

	// try and splice a page of data from the input fifo to the output pipe
	ssize_t spliced = splice(m_readFd, NULL, m_writeFd, NULL,
	                         (m_pageSize * 4), SPLICE_F_NONBLOCK | SPLICE_F_MOVE);

	if (spliced == 0) {
		qDebug("splice 0 bytes, input file/pipe complete");

		// this typically happens if the write end of the read pipe has been
		// closed and therefore there is no more data coming from the pipe
		if (m_writeNotifier)
			m_writeNotifier->setWriteEnabled(false);
		if (m_readNotifier)
			m_readNotifier->setEnabled(false);

		m_readException = true;
		emit readException();


	} else if (spliced > 0) {
		// qDebug("spliced %zd bytes from fifo to output pipe", spliced);

		// update the byte counts
		m_bytesRx += spliced;
		if (!m_inThrowAwayMode)
			m_bytesTx += spliced;

	} else {

		if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
			// if we get this error it means the splice call would have blocked,
			// since we've been told there is data to read this means the
			// write end is blocked, so enable the pipe write notifier (to
			// tell us when space is available) and disable the read notifier
			qDebug("output pipe blocked");

			if (m_writeNotifier)
				m_writeNotifier->setWriteEnabled(true);
			if (m_readNotifier)
				m_readNotifier->setEnabled(false);

		} else if (errno == EPIPE) {
			// this error means the read side of our output pipe has been closed
			// and therefore we can't write anything more to the pipe.
			qWarning("detected output pipe is closed");

			// handle the output close based on the mode
			onOutputClosed();

		} else {
			// any other error is a bug
			qErrnoWarning(errno, "failed to splice data between fifo and pipe");
		}

	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Private slot called when there is data that can be written to the output
	pipe.

 */
void UnixPipeSplicer::onWriteActivated(int fd)
{
	Q_ASSERT(QObject::thread() == QThread::currentThread());

	qDebug("output pipe un-blocked");

	// sanity check the supplied fd
	if (Q_UNLIKELY(fd != m_writeFd)) {
		qWarning("odd, invalid fd supplied in slot (expected=%d, actual=%d)",
		         m_writeFd, fd);
		return;
	}

	// sanity check we haven't already detected a pipe close
	if (Q_UNLIKELY(m_writeException)) {
		qWarning("odd, socket notifier called after detecting pipe closed");
		m_writeNotifier->setWriteEnabled(false);
		return;
	}

	// all we need to do is re-enable the fifo input notifier which will trigger
	// the slot if there is data that can be read
	if (m_readNotifier)
		m_readNotifier->setEnabled(true);

	// and disable this notifier as the write pipe is unblocked now
	if (m_writeNotifier)
		m_writeNotifier->setWriteEnabled(false);
}

