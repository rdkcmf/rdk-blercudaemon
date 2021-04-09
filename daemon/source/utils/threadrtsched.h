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
//  threadrtsched.h
//  BleRcuDaemon
//

#ifndef THREADRTSCHED_H
#define THREADRTSCHED_H

#include <QObject>
#include <QThread>
#include <QSet>

class ThreadRtSched : public QObject
{
	Q_OBJECT

public:
	enum Policy {
		SchedUnknown,
		SchedFifo,
		SchedRoundRobin,
		SchedOther
	};
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
	Q_ENUM(Policy);
#else
	Q_ENUMS(Policy);
#endif

	static void apply(QThread *thread, Policy policy, int priority,
	                  const QSet<int> &cpuSet = QSet<int>());

	static Policy currentThreadPolicy();
	static int currentThreadPriority();
	static QSet<uint> currentThreadCpuAffinity();


protected:
	ThreadRtSched(Policy policy, int priority, const QSet<int> &cpuSet);

public:
	~ThreadRtSched() final;

private slots:
	void applySettings();

private:
	const Policy m_policy;
	const int m_priority;
	const QSet<int> m_cpuSet;
};

#endif // !defined(THREADRTSCHED_H)
