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
//  bleuuid.h
//  SkyBluetoothRcu
//

#ifndef BLEUUID_H
#define BLEUUID_H

#include <QUuid>
#include <QString>
#include <QDebug>

#ifdef Q_OS_ANDROID
#  include <QtAndroid>
#  include <QAndroidJniEnvironment>
#endif


class BleUuid : public QUuid
{
public:
	enum ServiceType {
		GenericAccess = 0x1800,
		GenericAttribute = 0x1801,
		ImmediateAlert = 0x1802,
		LinkLoss = 0x1803,
		TxPower = 0x1804,
		DeviceInformation = 0x180a,
		BatteryService = 0x180f,
		HumanInterfaceDevice = 0x1812,
		ScanParameters = 0x1813,
	};

	enum SkyServiceType {
		SkyQVoice = 0xf800,
		SkyQInfrared = 0xf801,
		SkyQFirmwareUpgrade = 0xf802,
		ComcastRemoteControl = 0xf803,
	};


	enum CharacteristicType {
		DeviceName = 0x2a00,
		Appearance = 0x2a01,
		PeripheralPreferredConnectionParameters = 0x2a04,
		ServiceChanged = 0x2a05,
		AlertLevel = 0x2a06,
		BatteryLevel = 0x2a19,
		SystemID = 0x2a23,
		ModelNumberString = 0x2a24,
		SerialNumberString = 0x2a25,
		FirmwareRevisionString = 0x2a26,
		HardwareRevisionString = 0x2a27,
		SoftwareRevisionString = 0x2a28,
		ManufacturerNameString = 0x2a29,
		IEEERegulatatoryCertificationDataList = 0x2a2a,
		ScanRefresh = 0x2a31,
		BootKeyboardOutputReport = 0x2a32,
		BootMouseInputReport = 0x2a33,
		HIDInformation = 0x2a4a,
		ReportMap = 0x2a4b,
		HIDControlPoint = 0x2a4c,
		Report = 0x2a4d,
		ProtocolMode = 0x2a4e,
		ScanIntervalWindow = 0x2a4f,
		PnPID = 0x2a50,
	};

	enum SkyCharacteristicType {
		AudioCodecs = 0xea00,
		AudioGain = 0xea01,
		AudioControl = 0xea02,
		AudioData = 0xea03,
		InfraredStandby = 0xeb01,
		InfraredCodeId = 0xeb02,
		InfraredSignal = 0xeb03,
		EmitInfraredSignal = 0xeb06,
		FirmwareControlPoint = 0xec01,
		FirmwarePacket = 0xec02,
		UnpairReason = 0xed01,
		RebootReason = 0xed02,
		RcuAction = 0xed03,
		LastKeypress = 0xed04,
		AdvertisingConfig = 0xed05,
		AdvertisingConfigCustomList = 0xed06,
	};

	enum DescriptorType {
		ClientCharacteristicConfiguration = 0x2902,
		ReportReference = 0x2908,
	};

	enum SkyDescriptorType {
		InfraredSignalReference = 0xeb04,
		InfraredSignalConfiguration = 0xeb05,
		FirmwarePacketWindowSize = 0xec03,
	};

private:
	static void registerType();

public:
	BleUuid();
	BleUuid(ServiceType uuid);
	BleUuid(CharacteristicType uuid);
	BleUuid(DescriptorType uuid);
	BleUuid(SkyServiceType uuid);
	BleUuid(SkyCharacteristicType uuid);
	BleUuid(SkyDescriptorType uuid);
	explicit BleUuid(quint16 uuid);
	explicit BleUuid(quint32 uuid);
	explicit BleUuid(const QString &uuid);
	BleUuid(const BleUuid &uuid);
	BleUuid(const QUuid &uuid);
	~BleUuid();

	bool operator==(const BleUuid &other) const;
	bool operator==(const QUuid &other) const;

	QString name() const;
	QString longName() const;

	enum UuidFormat { WithCurlyBraces, WithoutCurlyBraces };
	QString toString(UuidFormat format = WithCurlyBraces) const;

	bool isStandard() const;
	bool isSkyDefined() const;

#ifdef Q_OS_ANDROID
	static BleUuid fromJavaUUID(const QAndroidJniObject &uuid);
	static BleUuid fromJavaUUID(jobject object);
#endif

};

Q_DECLARE_METATYPE(BleUuid)

QDebug operator<<(QDebug debug, const BleUuid &uuid);

#endif // !defined(BLEUUID_H)
