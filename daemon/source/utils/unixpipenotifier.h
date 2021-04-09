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
//  unixpipenotifier.h
//  SkyBluetoothRcu
//

#ifndef UNIXPIPENOTIFIER_H
#define UNIXPIPENOTIFIER_H

#include <QObject>
#include <QSocketNotifier>


class UnixPipeNotifier : public QObject
{
	Q_OBJECT

public:
	UnixPipeNotifier(int pipeFd, QObject *parent = nullptr);
	~UnixPipeNotifier();

	bool isReadEnabled() const;
	void setReadEnabled(bool enable);

	bool isWriteEnabled() const;
	void setWriteEnabled(bool enable);

	bool isExceptionEnabled() const;
	void setExceptionEnabled(bool enable);

signals:
	void readActivated(int pipeFd);
	void writeActivated(int pipeFd);
	void exceptionActivated(int pipeFd);

private slots:
	void onMonitorActivated(int pipeFd);

private:
	const int m_pipeFd;
	uint m_eventFlags;

#if defined(__linux__)
	int m_monitorFd;
	QSocketNotifier *m_notifier;
#else
	QSocketNotifier *m_readNotifier;
	QSocketNotifier *m_writeNotifier;
	QSocketNotifier *m_exceptionNotifier;
#endif
};

#endif // !defined(UNIXPIPENOTIFIER_H)

