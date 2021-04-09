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
//  threadrtsched.cpp
//  BleRcuDaemon
//

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include "threadrtsched.h"

#include <QTimer>

#include <pthread.h>


// -----------------------------------------------------------------------------
/*!
	\class ThreadRtSched
	\brief Utility class for setting the thread priority and optionally the
	thread affinity of a QThread.

	\ingroup utils

	Although Qt does supply a \l{QThread::setPriority()} method, if doesn't
	work for RT threads, which is what everything runs as on the Sky STB.  This
	class gets around that by using the unix pthread_setXXX calls directly
	from inside the thread.

	To use you only need to call the static function \l{ThreadRtSched::apply()},
	supplying the QThread object you want to change, for example:

	\code
		QThread someThread;
		ThreadRtSched::apply(someThread, ThreadRtSched::SchedRoundRobin, 6);
		someThread.run();
	\endcode

	The \l{apply()} function can be called on a running or stopped thread. If
	the thread is not running it hooks the \l{QThread::started()} signal to
	apply the settings when it is started, if the thread is already running it
	adds a single shot timer to the thread's event loop and applies the settings
	there.

 */


// -----------------------------------------------------------------------------
/*!
	Returns the current threading policy of the current thread.
 */
ThreadRtSched::Policy ThreadRtSched::currentThreadPolicy()
{
	int policy;
	struct sched_param param;

	int ret = pthread_getschedparam(pthread_self(), &policy, &param);
	if (ret != 0) {
		qErrnoWarning(ret, "failed to get thread scheduling param");
		return SchedUnknown;
	}

	switch (policy) {
		case SCHED_RR:
			return SchedRoundRobin;
		case SCHED_FIFO:
			return SchedFifo;
		case SCHED_OTHER:
			return SchedOther;
		default:
			return SchedUnknown;
	}
}

// -----------------------------------------------------------------------------
/*!
	Returns the priority of the current thread.
 */
int ThreadRtSched::currentThreadPriority()
{
	int policy;
	struct sched_param param;

	int ret = pthread_getschedparam(pthread_self(), &policy, &param);
	if (ret != 0) {
		qErrnoWarning(ret, "failed to get thread scheduling param");
		return -1;
	}

	return param.sched_priority;
}

// -----------------------------------------------------------------------------
/*!
	Returns the set of CPUs that the current thread is allowed to run on.
 
	\note This doesn't take into account the cgroup settings that the thread
	may be running under.
 */
QSet<uint> ThreadRtSched::currentThreadCpuAffinity()
{
	QSet<uint> cpuSet;

#if defined(Q_OS_LINUX)
	cpu_set_t cores;
	CPU_ZERO(&cores);

	int ret = pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cores);
	if (ret != 0) {
		qErrnoWarning(ret, "failed to get thread affinity");
		return cpuSet;
	}

	for (unsigned i = 0; i < CPU_SETSIZE; i++) {
		if (CPU_ISSET(i, &cores))
			cpuSet.insert(i);
	}
#endif

	return cpuSet;
}


// -----------------------------------------------------------------------------
/*!
	Applies the threading \a policy, \a priority and \a cpuSet to the supplied
	\a thread object.
 
	If the thread is not running then the settings will be applied when
	the thread is started.
 
	If the thread is running AND it's running an event loop then a request to
	set the settings will be added to the event loop.  If the thread is not
	running an event loop then this function will silently fail.
 */
void ThreadRtSched::apply(QThread *thread, Policy policy, int priority,
                          const QSet<int> &cpuSet)
{
	ThreadRtSched *rtSched = new ThreadRtSched(policy, priority, cpuSet);
	rtSched->moveToThread(thread);

	// if the thread is running use a single shot timer to apply the settings
	// within the context of the thread's event loop, otherwise just connect
	// to the started signal of the thread to apply the settings at startup
	if (thread->isRunning())
	{
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
		QTimer::singleShot(0, rtSched, &ThreadRtSched::applySettings);
#else
		QTimer* timer = new QTimer(rtSched);
		QObject::connect(timer, &QTimer::timeout, rtSched, &ThreadRtSched::applySettings);
		QObject::connect(timer, &QTimer::timeout, timer, &QObject::deleteLater);
		timer->setSingleShot(true);
		timer->start(0);
#endif
	}
	else
		QObject::connect(thread, &QThread::started, rtSched, &ThreadRtSched::applySettings);

	// destruct the newly created object if the thread is destroyed, this is
	// a catch-all in case our applySettings function is never executed
	QObject::connect(thread, &QObject::destroyed, rtSched, &QObject::deleteLater);
}


ThreadRtSched::ThreadRtSched(Policy policy, int priority, const QSet<int> &cpuSet)
	: QObject(nullptr)
	, m_policy(policy)
	, m_priority(priority)
	, m_cpuSet(cpuSet)
{
}

ThreadRtSched::~ThreadRtSched()
{
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Private slot called from inside the thread we want to modify, this applies
	the settings stored in the class and then calls \l{QObject::deleteLater()}
	to destruct this object.
 */
void ThreadRtSched::applySettings()
{
	int policy;
	switch (m_policy) {
		case SchedRoundRobin:
			policy = SCHED_RR;
			break;
		case SchedFifo:
			policy = SCHED_FIFO;
			break;
		case SchedOther:
		default:
			policy = SCHED_OTHER;
			break;
	}

	struct sched_param param;
	param.sched_priority = m_priority;

	// set the new policy and prioirty for the thread we are running in
	int ret = pthread_setschedparam(pthread_self(), policy, &param);
	if (ret != 0)
		qErrnoWarning(ret, "failed to set thread scheduling policy / priority");

	// set the cpu affinity on linux if a cpue set was supplied
#if defined(Q_OS_LINUX)
	if (!m_cpuSet.isEmpty()) {

		cpu_set_t cores;
		CPU_ZERO(&cores);

		for (int cpu : m_cpuSet)
			CPU_SET(cpu, &cores);

		ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cores);
		if (ret != 0)
			qErrnoWarning(ret, "failed to set thread affinity");
	}
#endif // defined(Q_OS_LINUX)

	// schedule ourselves for deletion
	deleteLater();
}

