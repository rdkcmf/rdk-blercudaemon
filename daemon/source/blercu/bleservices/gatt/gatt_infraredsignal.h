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
//  gatt_infraredsignal.h
//  SkyBluetoothRcu
//

#ifndef GATT_INFRAREDSIGNAL_H
#define GATT_INFRAREDSIGNAL_H

#include "../blercuinfraredservice.h"

#include "utils/statemachine.h"

#include <QObject>
#include <QEvent>
#include <QByteArray>
#include <QSharedPointer>



class BleGattCharacteristic;
class BleGattDescriptor;


class GattInfraredSignal : public QObject
{
	Q_OBJECT

public:
	GattInfraredSignal(const QSharedPointer<BleGattCharacteristic> &gattCharacteristic,
	                   QObject *parent = nullptr);
	~GattInfraredSignal() final;

public:
	bool isValid() const;
	bool isReady() const;

	int instanceId() const;

	Qt::Key keyCode() const;

public:
	void start();
	void stop();

signals:
	void ready();

public:
	Future<> program(const QByteArray &data);

private:
	enum State {
		IdleState,
		InitialisingState,
		ReadyState,
		ProgrammingSuperState,
			DisablingState,
			WritingState,
			EnablingState,
	};

	void initStateMachine();

private slots:
	void onEnteredState(int state);
	void onExitedState(int state);

private:
	void onEnteredInitialisingState();
	void onEnteredDisablingState();
	void onEnteredWritingState();
	void onEnteredEnablingState();

	void onExitedProgrammingState();

private:
	QSharedPointer<BleGattCharacteristic> m_signalCharacteristic;
	QSharedPointer<BleGattDescriptor> m_signalReferenceDescriptor;
	QSharedPointer<BleGattDescriptor> m_signalConfigurationDescriptor;

	Qt::Key m_keyCode;

	StateMachine m_stateMachine;

	QByteArray m_infraredData;
	QSharedPointer< Promise<> > m_programmingPromise;

private:
	static const QEvent::Type StartRequestEvent = QEvent::Type(QEvent::User + 1);
	static const QEvent::Type StopRequestEvent = QEvent::Type(QEvent::User + 2);
	static const QEvent::Type ProgramRequestEvent = QEvent::Type(QEvent::User + 3);

	static const QEvent::Type AckEvent = QEvent::Type(QEvent::User + 4);
	static const QEvent::Type ErrorEvent = QEvent::Type(QEvent::User + 5);


};

#endif // !defined(GATT_INFRAREDSERVICE_H)

