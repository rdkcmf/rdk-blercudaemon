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
//  hidrawdevice_p.h
//  SkyBluetoothRcu
//

#ifndef HIDRAWDEVICE_P_H
#define HIDRAWDEVICE_P_H

#include "hidrawdevice.h"

#include <QString>
#include <QSharedPointer>


class HidRawDeviceImpl : public HidRawDevice
{

public:
	HidRawDeviceImpl(int hidrawDevFd, QObject *parent = nullptr);
	HidRawDeviceImpl(const QString &hidrawDevPath,
	                 OpenMode openMode = OpenMode::ReadWrite,
	                 QObject *parent = nullptr);
	~HidRawDeviceImpl();

	bool isValid() const override;

	int minorNumber() const override;

	BusType busType() const override;
	quint16 vendor() const override;
	quint16 product() const override;
	PnPId pnpId() const override;
	QByteArray physicalAddress() const override;

	void enableReport(uint id) override;
	void disableReport(uint id) override;
	bool reportEnabled(uint id) const override;

public:
	static bool deviceInfo(int hidrawDevFd, BusType *busType,
	                       quint16 *vendor, quint16 *product,
	                       QByteArray *phyAddress);
	static bool deviceInfo(const QString &hidrawDevPath, BusType *busType,
	                       quint16 *vendor, quint16 *product,
	                       QByteArray *phyAddress);

public:
	bool write(uint number, const QByteArray &data) override;
	bool write(uint number, const QVector<quint8> &data);
	bool write(uint number, const quint8* data, int dataLen) override;

private:
	void init();
	void term();
	void deviceRemoved();

	void onReadActivated(int hidrawDevFd);
	void onExceptionActivated(int hidrawDevFd);

private:
	friend class HidRawDeviceManagerImpl;
	static bool getInfo(int hidDevFd, BusType *busType, quint16 *vendor, quint16 *product);
	static bool getPhysicalAddress(int hidDevFd, QByteArray *phyAddress);

private:
	int m_hidrawDevFd;
	QSharedPointer<QSocketNotifier> m_readNotifier;
	QSharedPointer<QSocketNotifier> m_exceptionNotifier;

	quint32 m_reportFilter;

	int m_minorNumber;

	BusType m_busType;
	quint16 m_vendor;
	quint16 m_product;
	QByteArray m_physicalAddress;
};



#endif // !defined(HIDRAWDEVICE_P_H)
