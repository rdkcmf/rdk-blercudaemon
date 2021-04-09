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
//  hidrawdevice.h
//  SkyBluetoothRcu
//

#ifndef HIDRAWDEVICE_H
#define HIDRAWDEVICE_H

#include "pnpid.h"

#include <QObject>
#include <QDebug>
#include <QByteArray>
#include <QSharedPointer>


class QSocketNotifier;


class HidRawDevice : public QObject
{
	Q_OBJECT

protected:
	HidRawDevice(QObject *parent)
		: QObject(parent)
	{ }

public:
	virtual ~HidRawDevice() = default;

public:
	enum BusType {
		USB,
		HIL,
		Bluetooth,
		Virtual,
		Other
	};
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
	Q_ENUM(BusType);
#else
	Q_ENUMS(BusType);
#endif

	enum OpenMode {
		ReadOnly = 0x01,
		WriteOnly = 0x02,
		ReadWrite = (ReadOnly | WriteOnly),
	};
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
	Q_ENUM(OpenMode);
#else
	Q_ENUMS(OpenMode);
#endif

public:
	virtual bool isValid() const = 0;

	virtual int minorNumber() const = 0;

	virtual BusType busType() const = 0;
	virtual quint16 vendor() const = 0;
	virtual quint16 product() const = 0;
	virtual PnPId pnpId() const = 0;

	virtual QByteArray physicalAddress() const = 0;

	virtual void enableReport(uint id) = 0;
	virtual void disableReport(uint id) = 0;
	virtual bool reportEnabled(uint id) const = 0;

	virtual bool write(uint number, const QByteArray &data) = 0;
	virtual bool write(uint number, const quint8* data, int dataLen) = 0;

signals:
	void report(uint number, const QByteArray &data);
	void closed();
};

QDebug operator<<(QDebug dbg, const HidRawDevice &device);


#endif // !defined(HIDRAWDEVICE_H)
