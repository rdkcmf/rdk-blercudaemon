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
//  future.h
//  SkyBluetoothRcu
//

#ifndef FUTURE_H
#define FUTURE_H

#include "promise.h"

#include <QObject>
#include <QString>
#include <QVariant>
#include <QSharedPointer>

#include <functional>



template<typename T = void>
class Future
{

public:
	template<typename T1 = T>
	static Future<T1> createFinished(const typename std::enable_if<!std::is_void<T1>::value, T1>::type &result)
	{
		Promise<T1> promise;
		promise.setFinished(result);
		return promise.future();
	}

	static Future<void> createFinished()
	{
		Promise<void> promise;
		promise.setFinished();
		return promise.future();
	}

	static Future<T> createErrored(const QString &name, const QString &message = QString())
	{
		Promise<T> promise;
		promise.setError(name, message);
		return promise.future();
	}

private:
	friend class Promise<T>;
	friend class Promise<void>;

	explicit Future(const QSharedPointer<const PromisePrivate<T>> &promise)
		: m_promise(promise)
	{ }

public:
	Future()
	{ }
	Future(const Future &other)
		: m_promise(other.m_promise)
	{ }
	Future(Future &&other) noexcept
		: m_promise(std::move(other.m_promise))
	{ }
	~Future() = default;

	Future& operator=(const Future &other)
	{ m_promise = other.m_promise; return *this; }

public:
	inline bool isValid() const
	{ return !m_promise.isNull(); }

	inline bool isFinished() const
	{ return m_promise.isNull() || m_promise->isFinished(); }

	inline bool isRunning() const
	{ return !m_promise.isNull() && m_promise->isRunning(); }

	inline bool isError() const
	{ return m_promise.isNull() || m_promise->isError(); }

	inline T result() const
	{
		if (!m_promise)
			return T();
		else
			return m_promise->result();
	}

	inline QString errorName() const
	{
		if (!m_promise)
			return QStringLiteral("Invalid Future");
		else
			return m_promise->errorName();
	}

	inline QString errorMessage() const
	{
		if (!m_promise)
			return QString("");
		else
			return m_promise->errorMessage();
	}

private:
	QSharedPointer<const PromisePrivate<T>> m_promise;


public:
	// connect a signal to a pointer to qobject member function
	template <typename Func>
	inline QMetaObject::Connection connectFinished(typename QtPrivate::FunctionPointer<Func>::Object *receiver,
	                                               Func slot, Qt::ConnectionType type = Qt::AutoConnection) const
	{
		// check if the promise is valid and warn if it's already completed
		if (m_promise && m_promise->isFinished())
			qWarning("future is has already finished");

		// lambda used to get the result and put it in the slot
		const PromisePrivate<T> *promise = m_promise.data();
		std::function<void()> lambda = [receiver, slot, promise]() {
			(receiver->*(slot))(promise->result());
		};

		// connect the lambda to the finished signal of the result notifier
		return QObject::connect(m_promise.data(), &PromisePrivateBase::finished,
		                        receiver, lambda, type);
	}

	// connect to a functor
	template <typename Func>
	inline typename QtPrivate::QEnableIf<QtPrivate::FunctionPointer<Func>::ArgumentCount == -1, QMetaObject::Connection>::Type
		connectFinished(Func slot) const
	{
		return connectFinished(m_promise.data(), slot, Qt::DirectConnection);
	}

	// connect to a functor, with a "context" object defining in which event loop is going to be executed
	template <typename Func>
	inline typename QtPrivate::QEnableIf<QtPrivate::FunctionPointer<Func>::ArgumentCount == -1, QMetaObject::Connection>::Type
		connectFinished(const QObject *context, Func slot, Qt::ConnectionType type = Qt::AutoConnection) const
	{
		// check if the promise is valid and warn if it's already completed
		if (m_promise && m_promise->isFinished())
			qWarning("future is has already finished");

		// lambda used to convert the QVariant to the type of the reply
		const PromisePrivate<T> *promise = m_promise.data();
		std::function<void()> lambda = [slot, promise]() {
			slot(promise->result());
		};

		// connect the lambda to the finished signal of the result notifier
		return QObject::connect(m_promise.data(), &PromisePrivateBase::finished,
		                        context, lambda, type);
	}

public:
	// connect a signal to a pointer to qobject member function
	template <typename Func>
	inline QMetaObject::Connection connectErrored(typename QtPrivate::FunctionPointer<Func>::Object *receiver,
	                                              Func slot, Qt::ConnectionType type = Qt::AutoConnection) const
	{
		return QObject::connect(m_promise.data(), &PromisePrivateBase::error, receiver, slot, type);
	}

	template <typename Func>
	inline typename QtPrivate::QEnableIf<QtPrivate::FunctionPointer<Func>::ArgumentCount == -1, QMetaObject::Connection>::Type
		connectErrored(Func slot) const
	{
		return connectErrored(m_promise.data(), slot, Qt::DirectConnection);
	}

	template <typename Func>
	inline typename QtPrivate::QEnableIf<QtPrivate::FunctionPointer<Func>::ArgumentCount == -1, QMetaObject::Connection>::Type
		connectErrored(const QObject *context, Func slot, Qt::ConnectionType type = Qt::AutoConnection) const
	{
		return QObject::connect(m_promise.data(), &PromisePrivateBase::error, context, slot, type);
	}

};


// connect a signal to a pointer to qobject member function (specialisation for empty reply)
template <> template <typename Func>
inline QMetaObject::Connection Future<>::connectFinished(typename QtPrivate::FunctionPointer<Func>::Object *receiver,
                                                         Func slot, Qt::ConnectionType type) const
{
	return QObject::connect(m_promise.data(), &PromisePrivateBase::finished, receiver, slot, type);
}

// connect to a functor (specialisation for empty reply)
template <> template <typename Func>
inline typename QtPrivate::QEnableIf<QtPrivate::FunctionPointer<Func>::ArgumentCount == -1, QMetaObject::Connection>::Type
	Future<>::connectFinished(Func slot) const
{
	return QObject::connect(m_promise.data(), &PromisePrivateBase::finished, slot);
}

// connect to a functor, with a "context" object defining in which event loop is going to be executed (specialisation for empty reply)
template <> template <typename Func>
inline typename QtPrivate::QEnableIf<QtPrivate::FunctionPointer<Func>::ArgumentCount == -1, QMetaObject::Connection>::Type
	Future<>::connectFinished(const QObject *context, Func slot, Qt::ConnectionType type) const
{
	return QObject::connect(m_promise.data(), &PromisePrivateBase::finished, context, slot, type);
}



Q_INLINE_TEMPLATE Future<void> Promise<void>::future() const
{
	return Future<void>(d);
}


#endif // !defined(FUTURE_H)
