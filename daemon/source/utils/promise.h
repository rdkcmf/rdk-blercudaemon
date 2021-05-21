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
//  promise.h
//  SkyBluetoothRcu
//

#ifndef PROMISE_H
#define PROMISE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QReadWriteLock>
#include <QAtomicInteger>
#include <QSharedPointer>

#if QT_VERSION <= QT_VERSION_CHECK(5, 4, 0)

#ifndef QT_MESSAGELOG_FILE

#define QT_MESSAGELOG_FILE          __FILE__
#define QT_MESSAGELOG_LINE          __LINE__
#define QT_MESSAGELOG_FUNC          __FUNCTION__

#define QtInfoMsg                   QtWarningMsg
#define qInfo                       qWarning

#endif

#endif

template <typename T>
class Future;


class PromisePrivateBase : public QObject
{
	Q_OBJECT

public:
	explicit PromisePrivateBase(QObject *parent = nullptr);
	~PromisePrivateBase() override;

public:
	void setError(const QString &name, const QString &message);
	void setError(const QString &name, const char *format, va_list ap);

public:
	bool isFinished() const;
	bool isRunning() const;
	bool isError() const;

	QString errorName() const;
	QString errorMessage() const;

signals:
	void finished();
	void error(const QString &name, const QString &message);

protected:
	QAtomicInteger<int> m_finished;
	mutable QReadWriteLock m_rwLock;

private:
	QString m_errorName;
	QString m_errorMessage;
};



template <typename T>
class PromisePrivate final : public PromisePrivateBase
{
public:
	explicit PromisePrivate(QObject *parent = nullptr)
		: PromisePrivateBase(parent)
	{ }
	~PromisePrivate() final = default;

public:
	void setFinished(const T &result);
	T result() const;

private:
	T m_result;

private:
	Q_DISABLE_COPY(PromisePrivate<T>)
};

template <typename T>
Q_INLINE_TEMPLATE void PromisePrivate<T>::setFinished(const T &result)
{
	if (Q_UNLIKELY(m_finished == 1)) {
		qWarning("already finished");
	} else {
		m_rwLock.lockForWrite();
		m_result = result;
		m_rwLock.unlock();
		m_finished.storeRelease(1);
		emit finished();
	}
}

template <typename T>
Q_INLINE_TEMPLATE T PromisePrivate<T>::result() const
{
	if (Q_UNLIKELY(m_finished == 0))
		qWarning("not finished");

	QReadLocker locker(&m_rwLock);
	return m_result;
}



template <>
class PromisePrivate<void> final : public PromisePrivateBase
{
public:
	explicit PromisePrivate(QObject *parent = nullptr)
		: PromisePrivateBase(parent)
	{ }
	~PromisePrivate() final = default;


public:
	void setFinished();
	void result() const;

private:
	Q_DISABLE_COPY(PromisePrivate<void>)
};

Q_INLINE_TEMPLATE void PromisePrivate<void>::setFinished()
{
	if (Q_UNLIKELY(m_finished.testAndSetOrdered(0, 1) == false))
		qWarning("already finished");
	else
		emit finished();
}

Q_INLINE_TEMPLATE void PromisePrivate<void>::result() const
{
	if (Q_UNLIKELY(m_finished == 0))
		qWarning("not finished");
}



template <typename T = void>
class Promise
{
public:
	explicit Promise(QObject *parent = nullptr)
		: d(QSharedPointer<PromisePrivate<T>>::create(parent))
	{ }
	Promise(const Promise<T> &other)
		: d(other.d)
	{ }
	Promise(Promise<T> &&other) noexcept
		: d(std::move(other.d))
	{ }
	~Promise() = default;


public:
	void setFinished(const T &result) const
	{
		d->setFinished(result);
	}

public:
	void setError(const QString &name, const QString &message = QString()) const
	{
		d->setError(name, message);
	}

	void setError(const QString &name, const char *format, ...) const
		__attribute__ ((format (printf, 3, 4)))
	{
		va_list ap;
		va_start(ap, format);
		this->setError(name, format, ap);
		va_end(ap);
	}

	void setError(const QString &name, const char *format, va_list ap) const
		__attribute__ ((format (printf, 3, 0)))
	{
		d->setError(name, format, ap);
	}

public:
	Future<T> future() const
	{
		return Future<T>(d);
	}

private:
	QSharedPointer<PromisePrivate<T>> d;

//private:
//	Q_DISABLE_COPY(Promise<T>)
};



template <>
class Promise<void>
{
public:
	explicit Promise(QObject *parent = nullptr)
		: d(QSharedPointer<PromisePrivate<void>>::create(parent))
	{ }
	Promise(const Promise<void> &other)
		: d(other.d)
	{ }
	Promise(Promise<void> &&other) noexcept
		: d(std::move(other.d))
	{ }
	~Promise() = default;

public:
	void setFinished() const
	{
		d->setFinished();
	}

public:
	void setError(const QString &name, const QString &message = QString()) const
	{
		d->setError(name, message);
	}

	void setError(const QString &name, const char *format, ...) const
		__attribute__ ((format (printf, 3, 4)))
	{
		va_list ap;
		va_start(ap, format);
		this->setError(name, format, ap);
		va_end(ap);
	}

	void setError(const QString &name, const char *format, va_list ap) const
		__attribute__ ((format (printf, 3, 0)))
	{
		d->setError(name, format, ap);
	}

public:
	Future<void> future() const;

private:
	QSharedPointer<PromisePrivate<void>> d;

//private:
//	Q_DISABLE_COPY(Promise<void>)
};




#endif // PROMISE_H
