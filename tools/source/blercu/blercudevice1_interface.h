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

#ifndef BLERCUDEVICE1_INTERFACE_H
#define BLERCUDEVICE1_INTERFACE_H

#include "dbus/dbusabstractinterface.h"

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>

/*
 * Proxy class for interface com.sky.BleRcuDevice1
 */
class ComSkyBleRcuDevice1Interface: public DBusAbstractInterface
{
	Q_OBJECT
public:
	static inline const char *staticInterfaceName()
	{ return "com.sky.blercu.Device1"; }

public:
	ComSkyBleRcuDevice1Interface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = 0);

	~ComSkyBleRcuDevice1Interface() final;

	Q_PROPERTY(QString Address READ address)
	inline QString address() const
	{ return qvariant_cast< QString >(property("Address")); }

	Q_PROPERTY(qint32 AudioGainLevel READ audioGainLevel WRITE setAudioGainLevel NOTIFY audioGainLevelChanged)
	inline qint32 audioGainLevel() const
	{ return qvariant_cast< qint32 >(property("AudioGainLevel")); }
	inline void setAudioGainLevel(qint32 value)
	{ setProperty("AudioGainLevel", QVariant::fromValue(value)); }

	Q_PROPERTY(bool AudioStreaming READ audioStreaming NOTIFY audioStreamingChanged)
	inline bool audioStreaming() const
	{ return qvariant_cast< bool >(property("AudioStreaming")); }

	Q_PROPERTY(quint8 BatteryLevel READ batteryLevel NOTIFY batteryLevelChanged)
	inline quint8 batteryLevel() const
	{ return qvariant_cast< quint8 >(property("BatteryLevel")); }

	Q_PROPERTY(bool Connected READ connected NOTIFY connectedChanged)
	inline bool connected() const
	{ return qvariant_cast< bool >(property("Connected")); }

	Q_PROPERTY(QDBusObjectPath Controller READ controller)
	inline QDBusObjectPath controller() const
	{ return qvariant_cast< QDBusObjectPath >(property("Controller")); }

	Q_PROPERTY(QString FirmwareRevision READ firmwareRevision)
	inline QString firmwareRevision() const
	{ return qvariant_cast< QString >(property("FirmwareRevision")); }

	Q_PROPERTY(QString HardwareRevision READ hardwareRevision)
	inline QString hardwareRevision() const
	{ return qvariant_cast< QString >(property("HardwareRevision")); }

	Q_PROPERTY(QString Manufacturer READ manufacturer)
	inline QString manufacturer() const
	{ return qvariant_cast< QString >(property("Manufacturer")); }

	Q_PROPERTY(QString Model READ model)
	inline QString model() const
	{ return qvariant_cast< QString >(property("Model")); }

	Q_PROPERTY(QString Name READ name NOTIFY nameChanged)
	inline QString name() const
	{ return qvariant_cast< QString >(property("Name")); }

	Q_PROPERTY(QString SerialNumber READ serialNumber)
	inline QString serialNumber() const
	{ return qvariant_cast< QString >(property("SerialNumber")); }

	Q_PROPERTY(QString SoftwareRevision READ softwareRevision)
	inline QString softwareRevision() const
	{ return qvariant_cast< QString >(property("SoftwareRevision")); }

	Q_PROPERTY(qint32 IrCode READ irCode NOTIFY irCodeChanged)
	inline qint32 irCode() const
	{ return qvariant_cast< qint32 >(property("IrCode")); }

	Q_PROPERTY(quint32 TouchMode READ touchMode NOTIFY touchModeChanged)
	inline quint32 touchMode() const
	{ return qvariant_cast< quint32 >(property("TouchMode")); }

	Q_PROPERTY(bool TouchModeSettable READ touchModeSettable NOTIFY touchModeSettableChanged)
	inline bool touchModeSettable() const
	{ return qvariant_cast< bool >(property("TouchModeSettable")); }

public Q_SLOTS: // METHODS
	inline QDBusPendingReply<> EraseIrSignals()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("EraseIrSignals"), argumentList);
	}

	inline QDBusPendingReply<> FindMe(quint8 level, int duration)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(level) << QVariant::fromValue(duration);
		return asyncCallWithArgumentList(QStringLiteral("FindMe"), argumentList);
	}

	inline QDBusPendingReply<quint32, quint32, quint32> GetAudioStatus()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("GetAudioStatus"), argumentList);
	}

	inline QDBusPendingReply<> ProgramIrSignals(qint32 code, QList<quint16> signals_)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(code) << QVariant::fromValue(signals_);
		return asyncCallWithArgumentList(QStringLiteral("ProgramIrSignals"), argumentList);
	}

	inline QDBusPendingReply<> SendIrSignal(quint16 id)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(id);
		return asyncCallWithArgumentList(QStringLiteral("SendIrSignal"), argumentList);
	}

	inline QDBusPendingReply<QDBusUnixFileDescriptor> StartAudioStreaming(quint32 encoding)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(encoding);
		return asyncCallWithArgumentList(QStringLiteral("StartAudioStreaming"), argumentList);
	}

	inline QDBusPendingReply<> StartAudioStreamingTo(quint32 encoding, const QString &file_path)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(encoding) << QVariant::fromValue(file_path);
		return asyncCallWithArgumentList(QStringLiteral("StartAudioStreamingTo"), argumentList);
	}

	inline QDBusPendingReply<> StopAudioStreaming()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("StopAudioStreaming"), argumentList);
	}

Q_SIGNALS: // SIGNALS
	void batteryLevelChanged(quint8 level);
	void connectedChanged(bool connected);
	void nameChanged(const QString &name);
	void irCodeChanged(qint32 code);
	void touchModeChanged(quint32 mode);
	void touchModeSettableChanged(bool settable);
	void audioStreamingChanged(bool streaming);
	void audioGainLevelChanged(qint32 gainLevel);

};

namespace com {
  namespace sky {
	typedef ::ComSkyBleRcuDevice1Interface BleRcuDevice1;
  }
}
#endif
