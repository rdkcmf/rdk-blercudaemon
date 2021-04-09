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
//  promise.cpp
//  SkyBluetoothRcu
//

#include "promise.h"
#include "future.h"



PromisePrivateBase::PromisePrivateBase(QObject *parent)
	: QObject(parent)
	, m_finished(0)
{
}

PromisePrivateBase::~PromisePrivateBase()
{
	// Q_ASSERT(m_finished);

	// we should never be destroyed before signalling either finished or error,
	// but just in case we emit an error sigal here to stop code waiting
	// forever for a signal that never comes
	if (isRunning()) {
		qWarning("promise destroyed without finishing");
		emit error(QStringLiteral("com.sky.Error.Failed"),
		           QStringLiteral("promise destroyed without finishing"));
	}
}

bool PromisePrivateBase::isFinished() const
{
	return (m_finished.loadAcquire() == 1);
}

bool PromisePrivateBase::isRunning() const
{
	return (m_finished.loadAcquire() == 0);
}

bool PromisePrivateBase::isError() const
{
	QReadLocker locker(&m_rwLock);
	return !m_errorName.isNull();
}

QString PromisePrivateBase::errorName() const
{
	QReadLocker locker(&m_rwLock);
	return m_errorName;
}

QString PromisePrivateBase::errorMessage() const
{
	QReadLocker locker(&m_rwLock);
	return m_errorMessage;
}

void PromisePrivateBase::setError(const QString &errorName, const QString &errorMessage)
{
	if (Q_UNLIKELY(isFinished())) {
		qWarning("already finished");
	} else {

		m_rwLock.lockForWrite();

		if (Q_UNLIKELY(errorName.isEmpty()))
			qWarning("error name in result notifier is empty?");
		if (Q_UNLIKELY(errorMessage.isEmpty()))
			qWarning("error message in result notifier is empty?");

		m_errorName = errorName;
		m_errorMessage = errorMessage;

		m_rwLock.unlock();

		m_finished.storeRelease(1);

		emit error(m_errorName, m_errorMessage);
	}
}

void PromisePrivateBase::setError(const QString &name, const char *format, va_list ap)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
	setError(name, QString::vasprintf(format, ap));
#else
    int len = 0;
    len = vsnprintf(NULL, 0, format, ap);
    char* tmpStr = new char[len + 1];
    vsnprintf(tmpStr, len, format, ap);

	setError(name, QString(tmpStr));
    delete[] tmpStr;
#endif

}

