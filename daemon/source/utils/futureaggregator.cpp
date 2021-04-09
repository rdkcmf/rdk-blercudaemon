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
//  futureaggregator.cpp
//  SkyBluetoothRcu
//

#include "futureaggregator.h"

#include <QTimer>


// -----------------------------------------------------------------------------
/*!
	\class FutureAggregator
	\brief Object that wraps one or more Future objects and returns another
	Future that will only be signalled when all internal Futures complete.

	This is useful if you have a bunch of parallel operations and you only want
	to know when all of them have completed.

	If an error is signal on any of the Futures then the error is stored
	internally and will be signalled once all the futures have completed.
	Only the first error is stored and returned, other errors are discarded.
	If an error occured on any of the internal Futures then the aggregator
	is also considered to be errored.

 */


#if (QT_VERSION < QT_VERSION_CHECK(5, 7, 0))
	// this adds const to non-const objects (like std::as_const)
	template <typename T>
	Q_DECL_CONSTEXPR typename std::add_const<T>::type &qAsConst(T &t) Q_DECL_NOTHROW { return t; }
#endif


// -----------------------------------------------------------------------------
/*!
	Constructs the FutureAggregator watching the supplied \a futures list or
	Future objects.

  */
FutureAggregator::FutureAggregator(const QList< Future<> > &futures,
                                   QObject *parent)
	: QObject(parent)
	, m_futures(futures)
	, m_signalledFinished(false)
{
	connectSignals();
}

// -----------------------------------------------------------------------------
/*!
	Constructs the FutureAggregator using move sematics.

 */
FutureAggregator::FutureAggregator(const QList< Future<> > &&futures,
                                   QObject *parent)
	: QObject(parent)
	, m_futures(std::move(futures))
	, m_signalledFinished(false)
{
	connectSignals();
}

// -----------------------------------------------------------------------------
/*!
	Destructs the FutureAggregator, this does NOT wait for internal futures to
	finish before returning.

	If any internal Future is still running and the aggregator was holding the
	last handle to it then an error will be logged.

 */
FutureAggregator::~FutureAggregator()
{
	// debugging - expect if a promise was created then we have signalled it's
	// completion
	if (Q_UNLIKELY(!m_promise.isNull()))
		qInfo("destroying incomplete promise");
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Connects to the signals of all the futures aggregated by this object.

 */
void FutureAggregator::connectSignals()
{
	for (const Future<> &future : qAsConst(m_futures)) {
		future.connectFinished(this, &FutureAggregator::onFutureFinished);
		future.connectErrored(this, &FutureAggregator::onFutureErrored);
	}
}

// -----------------------------------------------------------------------------
/*!
	Returns the list of futures the aggregator is monitoring.

 */
QList< Future<> > FutureAggregator::futures() const
{
	return m_futures;
}

// -----------------------------------------------------------------------------
/*!
	Returns a future that is an aggregation of all the added futures, this will
	only trigger when all the futures stored within this object finish

 */
Future<> FutureAggregator::future()
{
	// check if they've all already finished, in which case we should also be
	// set the finished state on the promise
	if (Q_UNLIKELY(isFinished())) {
		if (!m_errorName.isEmpty())
			return Future<>::createErrored(m_errorName, m_errorName);
		else
			return Future<>::createFinished();
	}

	if (!m_promise)
		m_promise = QSharedPointer< Promise<> >::create();

	return m_promise->future();
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if no futures have been added to the aggregator.

 */
bool FutureAggregator::isEmpty() const
{
	return m_futures.isEmpty();
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if all the futures have finished.

 */
bool FutureAggregator::isFinished() const
{
	// iterate through the list of futures, if they've all finished then
	// emit our finished signal
	int finishedCount = 0;

	for (const Future<> &future : m_futures) {
		if (future.isFinished())
			finishedCount++;
	}

	return (finishedCount == m_futures.length());
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if one or more of the futures is still running.

 */
bool FutureAggregator::isRunning() const
{
	return !isFinished();
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if one or more of the futures has finished with an error.

 */
bool FutureAggregator::isError() const
{
	return !m_errorName.isEmpty();
}

// -----------------------------------------------------------------------------
/*!
	Returns the name of the first error that occured on a future.  If no error
	occurred then an empty string is returned.

 */
QString FutureAggregator::errorName() const
{
	return m_errorName;
}

// -----------------------------------------------------------------------------
/*!
	Returns the message of the first error that occured on a future.  If no
	error occurred then an empty string is returned.

 */
QString FutureAggregator::errorMessage() const
{
	return m_errorMessage;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when one of our futures has finished, when this happens we check if
	all the futures have no finished and if so emit our finished signal.

 */
void FutureAggregator::onFutureFinished()
{
	// check if all the futures have now finished, if so emit a finished signal
	// from the event queue
	if (isFinished())
		onAllFuturesFinished();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when one of our futures has errored, we store the error if we don't
	already have an error.

 */
void FutureAggregator::onFutureErrored(const QString &errorName,
                                       const QString &errorMessage)
{
	// store and emit the error signal
	if (m_errorName.isEmpty()) {
		m_errorName = errorName;
		m_errorMessage = errorMessage;
	}

	// check if now eveything has finished
	if (isFinished())
		onAllFuturesFinished();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when one of our futures has finished, when this happens we check if
	all the futures have no finished and if so emit our finished signal.

 */
void FutureAggregator::onAllFuturesFinished()
{
	qDebug("all futures finished");

	// send a errored or finished signal depending on whether any future
	// triggered an error or not
	if (!m_errorName.isEmpty()) {

		if (m_promise) {
			m_promise->setError(m_errorName, m_errorMessage);
			m_promise.reset();
		}

		if (!m_signalledFinished)
			emit errored(m_errorName, m_errorMessage);

	} else {

		if (m_promise) {
			m_promise->setFinished();
			m_promise.reset();
		}

		if (!m_signalledFinished)
			emit finished();
	}

	m_signalledFinished = true;
}


