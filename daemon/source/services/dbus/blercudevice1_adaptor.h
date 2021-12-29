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
//  blercudevice1_adaptor.h
//  SkyBluetoothRcu
//

#ifndef BLERCUDEVICE1_ADAPTOR_H
#define BLERCUDEVICE1_ADAPTOR_H

#include "dbus/dbusabstractadaptor.h"

#include "blercu/bleservices/blercuinfraredservice.h"
#include "blercu/bleservices/blercuaudioservice.h"
#include "blercu/blercuerror.h"

#include <QObject>
#include <QString>
#include <QList>

#include <QtDBus>


class BleRcuDevice;


typedef QMap<quint16, QByteArray> IrKeyWaveforms;
Q_DECLARE_METATYPE(IrKeyWaveforms)

typedef QList<quint16> CdiKeyCodeList;
Q_DECLARE_METATYPE(CdiKeyCodeList)

QDBusArgument &operator<<(QDBusArgument &argument, const CdiKeyCodeList& cdiKeyCodes);
const QDBusArgument &operator>>(const QDBusArgument &argument, CdiKeyCodeList &cdiKeyCodes);


/*
 * Adaptor class for interface com.sky.BleRcuDevice1
 */
class BleRcuDevice1Adaptor : public DBusAbstractAdaptor
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "com.sky.blercu.Device1")
	Q_CLASSINFO("D-Bus Introspection", ""
	            "  <interface name=\"com.sky.blercu.Device1\">\n"
	            "    <property access=\"read\" type=\"s\" name=\"Address\">\n"
	            "      <annotation value=\"const\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"s\" name=\"Name\">\n"
	            "      <annotation value=\"true\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"b\" name=\"Connected\">\n"
	            "      <annotation value=\"true\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"s\" name=\"Manufacturer\">\n"
	            "      <annotation value=\"true\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"s\" name=\"Model\">\n"
	            "      <annotation value=\"true\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"s\" name=\"SerialNumber\">\n"
	            "      <annotation value=\"true\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"s\" name=\"HardwareRevision\">\n"
	            "      <annotation value=\"true\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"s\" name=\"FirmwareRevision\">\n"
	            "      <annotation value=\"true\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"s\" name=\"SoftwareRevision\">\n"
	            "      <annotation value=\"true\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"i\" name=\"IrCode\">\n"
	            "      <annotation value=\"true\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"u\" name=\"TouchMode\">\n"
	            "      <annotation value=\"true\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"b\" name=\"TouchModeSettable\">\n"
	            "      <annotation value=\"true\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"y\" name=\"BatteryLevel\">\n"
	            "      <annotation value=\"true\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"b\" name=\"AudioStreaming\">\n"
	            "      <annotation value=\"true\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"readwrite\" type=\"y\" name=\"AudioGainLevel\">\n"
	            "      <annotation value=\"true\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"readwrite\" type=\"u\" name=\"AudioCodecs\">\n"
	            "      <annotation value=\"true\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"o\" name=\"Controller\">\n"
	            "      <annotation value=\"const\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"y\" name=\"UnpairReason\">\n"
	            "      <annotation value=\"const\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"y\" name=\"RebootReason\">\n"
	            "      <annotation value=\"const\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <property access=\"read\" type=\"y\" name=\"LastKeypress\">\n"
	            "      <annotation value=\"const\" name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"/>\n"
	            "    </property>\n"
	            "    <method name=\"SendRcuAction\">\n"
	            "      <arg direction=\"in\" type=\"y\" name=\"action\"/>\n"
	            "    </method>\n"
	            "    <method name=\"WriteAdvertisingConfig\">\n"
	            "      <arg direction=\"in\" type=\"y\" name=\"config\"/>\n"
	            "      <arg direction=\"in\" type=\"ay\" name=\"customList\"/>\n"
	            "    </method>\n"
	            "    <method name=\"FindMe\">\n"
	            "      <arg direction=\"in\" type=\"y\" name=\"level\"/>\n"
	            "      <arg direction=\"in\" type=\"i\" name=\"duration\"/>\n"
	            "    </method>\n"
	            "    <method name=\"SendIrSignal\">\n"
	            "      <arg direction=\"in\" type=\"q\" name=\"id\"/>\n"
	            "    </method>\n"
	            "    <method name=\"ProgramIrSignals\">\n"
	            "      <arg direction=\"in\" type=\"i\" name=\"code\"/>\n"
	            "      <arg direction=\"in\" type=\"aq\" name=\"signals\"/>\n"
	            "      <annotation value=\"CdiKeyCodeList\" name=\"org.qtproject.QtDBus.QtTypeName.In1\"/>\n"
	            "    </method>\n"
	            "    <method name=\"ProgramIrSignalWaveforms\">\n"
	            "      <arg direction=\"in\" type=\"a{qay}\" name=\"waveforms\"/>\n"
	            "    </method>\n"
	            "    <method name=\"EraseIrSignals\"/>\n"
	            "    <method name=\"StartAudioStreaming\">\n"
	            "      <arg direction=\"in\" type=\"u\" name=\"encoding\"/>\n"
	            "      <arg direction=\"out\" type=\"h\" name=\"stream\"/>\n"
	            "    </method>\n"
	            "    <method name=\"StartAudioStreamingTo\">\n"
	            "      <arg direction=\"in\" type=\"u\" name=\"encoding\"/>\n"
	            "      <arg direction=\"in\" type=\"s\" name=\"file_path\"/>\n"
	            "    </method>\n"
	            "    <method name=\"StopAudioStreaming\">\n"
	            "    </method>\n"
	            "    <method name=\"GetAudioStatus\">\n"
	            "      <arg direction=\"out\" type=\"u\" name=\"error_status\"/>\n"
	            "      <arg direction=\"out\" type=\"u\" name=\"packets_received\"/>\n"
	            "      <arg direction=\"out\" type=\"u\" name=\"packets_expected\"/>\n"
	            "    </method>\n"
	            "    <method name=\"SetTouchMode\">\n"
	            "      <arg direction=\"in\" type=\"u\" name=\"flags\"/>\n"
	            "    </method>\n"
	            "  </interface>\n"
	            "")

public:
	Q_PROPERTY(QString Address READ address)
	Q_PROPERTY(quint8 AudioGainLevel READ audioGainLevel WRITE setAudioGainLevel)
	Q_PROPERTY(quint32 AudioCodecs READ audioCodecs)
	Q_PROPERTY(bool AudioStreaming READ audioStreaming)
	Q_PROPERTY(quint8 BatteryLevel READ batteryLevel)
	Q_PROPERTY(bool Connected READ connected)
	Q_PROPERTY(QDBusObjectPath Controller READ controller)
	Q_PROPERTY(QString FirmwareRevision READ firmwareRevision)
	Q_PROPERTY(QString HardwareRevision READ hardwareRevision)
	Q_PROPERTY(qint32 IrCode READ irCode)
	Q_PROPERTY(QString Manufacturer READ manufacturer)
	Q_PROPERTY(QString Model READ model)
	Q_PROPERTY(QString Name READ name)
	Q_PROPERTY(QString SerialNumber READ serialNumber)
	Q_PROPERTY(QString SoftwareRevision READ softwareRevision)
	Q_PROPERTY(quint32 TouchMode READ touchMode)
	Q_PROPERTY(bool TouchModeSettable READ touchModeSettable)
	Q_PROPERTY(quint8 UnpairReason READ unpairReason)
	Q_PROPERTY(quint8 RebootReason READ rebootReason)
	Q_PROPERTY(quint8 LastKeypress READ lastKeypress)
	Q_PROPERTY(quint8 AdvertisingConfig READ advConfig)
	Q_PROPERTY(QByteArray AdvertisingConfigCustomList READ advConfigCustomList)

public:
	BleRcuDevice1Adaptor(const QSharedPointer<BleRcuDevice> &device,
	                     const QDBusObjectPath &objPath,
	                     QObject *parent);
	virtual ~BleRcuDevice1Adaptor();

public:
	QDBusObjectPath controller() const;

	QString address() const;
	QString name() const;

	bool connected() const;

	quint8 audioGainLevel() const;
	void setAudioGainLevel(quint8 value);

	quint32 audioCodecs() const;

	bool audioStreaming() const;

	quint8 batteryLevel() const;

	QString firmwareRevision() const;
	QString hardwareRevision() const;
	QString softwareRevision() const;

	QString manufacturer() const;
	QString model() const;
	QString serialNumber() const;

	qint32 irCode() const;

	quint32 touchMode() const;
	bool touchModeSettable() const;

	quint8 unpairReason() const;
	quint8 rebootReason() const;
	quint8 lastKeypress() const;
	quint8 advConfig() const;
	QByteArray advConfigCustomList() const;

public slots:
	void ProgramIrSignals(qint32 codeId, const CdiKeyCodeList &keyCode,
	                      const QDBusMessage &message);
	void ProgramIrSignalWaveforms(const IrKeyWaveforms &irWaveforms,
	                      const QDBusMessage &message);
	void SendIrSignal(quint16 keyCode, const QDBusMessage &message);
	void EraseIrSignals(const QDBusMessage &message);

	void FindMe(quint8 level, qint32 duration, const QDBusMessage &message);

	void StartAudioStreaming(quint32 encoding, const QDBusMessage &message);
	void StartAudioStreamingTo(quint32 encoding, const QString &filePath,
	                           const QDBusMessage &message);
	void StopAudioStreaming(const QDBusMessage &message);

	void GetAudioStatus(const QDBusMessage &message);

	void SetTouchMode(quint32 flags, const QDBusMessage &message);

	void SendRcuAction(quint8 action, const QDBusMessage &message);
	void WriteAdvertisingConfig(quint8 config, const QByteArray &customList, const QDBusMessage &message);

signals:

private:
	void onReadyChanged(bool ready);
	void onNameChanged(const QString &name);

	void onBatteryLevelChanged(int batteryLevel);

	void onAudioStreamingChanged(bool streaming);
	void onAudioGainLevelChanged(quint8 gainLevel);
	void onAudioCodecsChanged(quint32 codecs);

	void onTouchModeChanged(quint8 touchMode);
	void onTouchModeSettabilityChanged(bool settable);

	void onCodeIdChanged(qint32 codeId);

	void onFirmwareVersionChanged(const QString &fwVersion);
	void onHardwareRevisionChanged(const QString &hwVersion);
	void onManufacturerNameChanged(const QString &name);
	void onModelNumberChanged(const QString &model);
	void onSerialNumberChanged(const QString &serial);
	void onSoftwareVersionChanged(const QString &swVersion);

	void onUnpairReasonChanged(quint8 unpairReason);
	void onRebootReasonChanged(quint8 rebootReason);
	void onLastKeypressChanged(quint8 lastKeypress);
	void onAdvConfigChanged(quint8 config);
	void onAdvConfigCustomListChanged(QByteArray &customList);

	Qt::Key convertCDIKeyCode(quint16 cdiKeyCode) const;

	template <typename T>
	void emitPropertyChanged(const QString &propName, const T &propValue) const;

	void sendError(const QDBusMessage &request, BleRcuError::ErrorType errorType,
	               const QString &errorMessage) const;
	void sendError(const QDBusMessage &request, QDBusError::ErrorType errorType,
	               const QString &errorMessage) const;

	static QList<QVariant> convertStatusInfo(const BleRcuAudioService::StatusInfo &info);
	static QList<QVariant> convertFileDescriptor(const FileDescriptor &fd);

private:
	const QSharedPointer<BleRcuDevice> m_device;
	const QDBusObjectPath m_dbusObjPath;
};

#endif // !defined(BLERCUDEVICE1_ADAPTOR_H)
