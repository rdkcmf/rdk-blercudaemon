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
//  readline.h
//  SkyBluetoothRcu
//

#ifndef READLINE_H
#define READLINE_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QPointer>
#include <QAtomicInteger>


class QSocketNotifier;
class ReadLinePrivate;


class ReadLine : public QObject
{
	Q_OBJECT

public:
	explicit ReadLine(QObject *parent = nullptr);
	~ReadLine() final;

public:
	bool isValid() const;

	void setPrompt(const QString &prompt);
	QString prompt() const;

public slots:
	void start();
	void stop();

	void runCommand(const QString &command, const QStringList &arguments);

signals:


public:
#ifdef Q_QDOC
	template<typename PointerToMemberFunction>
	bool addCommand(const QString &name, const QStringList &args, const QString &description,
	                const QObject *receiver, PointerToMemberFunction method);
	template<typename Functor>
	bool addCommand(const QString &name, const QStringList &args, const QString &description,
	                Functor functor);
	template<typename Functor>
	bool addCommand(const QString &name, const QStringList &args, const QString &description,
	                QObject *context, Functor functor);
#else
	template <typename Func1>
	inline bool addCommand(const QString &name, const QStringList &args, const QString &description,
	                       const typename QtPrivate::FunctionPointer<Func1>::Object *receiver, Func1 slot)
	{
		typedef QtPrivate::FunctionPointer<Func1> SlotType;

		Q_STATIC_ASSERT_X(int(SlotType::ArgumentCount) == 1,
		                  "The slot must have one argument.");
		Q_STATIC_ASSERT_X((QtPrivate::AreArgumentsCompatible< typename SlotType::Arguments, typename QtPrivate::List<const QStringList&> >::value),
		                  "The one and only argument to the slot must be 'const QCommandLineParser&'");

		return addCommandImpl(name, args, description, receiver,
		                      new QtPrivate::QSlotObject<Func1, typename SlotType::Arguments, void>(slot));
	}

	template <typename Func1>
	inline typename QtPrivate::QEnableIf<!QtPrivate::FunctionPointer<Func1>::IsPointerToMemberFunction &&
	                                     !std::is_same<const char*, Func1>::value, bool>::Type
			addCommand(const QString &name, const QStringList &args, const QString &description, Func1 slot)
	{
		return addCommand(name, args, description, nullptr, slot);
	}

	template <typename Func1>
	inline typename QtPrivate::QEnableIf<!QtPrivate::FunctionPointer<Func1>::IsPointerToMemberFunction &&
	                                     !std::is_same<const char*, Func1>::value, bool>::Type
			addCommand(const QString &name, const QStringList &args, const QString &description, QObject *context, Func1 slot)
	{
		typedef QtPrivate::FunctionPointer<Func1> SlotType;
		Q_STATIC_ASSERT_X(int(SlotType::ArgumentCount) == 1,
		                  "The slot must have one argument.");
		Q_STATIC_ASSERT_X((QtPrivate::AreArgumentsCompatible< typename SlotType::Arguments, typename QtPrivate::List<const QStringList&> >::value),
		                  "The one and only argument to the slot must be 'const QCommandLineParser&'");

		return addCommandImpl(name, args, description, context,
		                      new QtPrivate::QFunctorSlotObject<Func1, 0,
		                          typename QtPrivate::List_Left<void, 0>::Value, void>(slot));
	}
#endif

private:
	bool addCommandImpl(const QString &name, const QStringList &args, const QString &description,
	                    const QObject *receiver, QtPrivate::QSlotObjectBase *slotObj);

private:
	QPointer<ReadLinePrivate> m_private;
	QString m_prompt;

};


#endif // DBUSABSTRACTINTRFACE_H
