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
//  blercu_cmdhandler.h
//  SkyBluetoothRcu
//

#ifndef BLERCU_CMDHANDLER_H
#define BLERCU_CMDHANDLER_H

#include "base_cmdhandler.h"
#include "utils/bleaddress.h"

#include <QObject>
#include <QString>
#include <QMap>
#include <QSharedPointer>
#include <QDBusConnection>
#include <QDBusObjectPath>


class ComSkyBleRcuController1Interface;
class ComSkyBleRcuDevice1Interface;
class ComSkyBleRcuUpgrade1Interface;
class ComSkyBleRcuDebug1Interface;
class ComSkyBleRcuHciCapture1Interface;

class AudioWavFile;


class BleRcuCmdHandler : public BaseCmdHandler
{
	Q_OBJECT

public:
	BleRcuCmdHandler(const QDBusConnection &bus,
	                 const QString &service,
	                 QObject *parent = nullptr);
	~BleRcuCmdHandler() final;

private:
	void initBleRcuInterfaces(const QDBusConnection &bus,
	                          const QString &service, const QString &path);

	void showDBusError(const QDBusError &error) const;
	QString getPairingStateName(qint32 status) const;

public:
	bool isValid() override;

	QString prompt() override;

public slots:
	void show() override;

	void listDevices() override;
	void listConnectedDevices() override;

	void startPairing(quint8 pairingCode) override;
	void cancelPairing() override;
	void startScanning(quint32 timeout) override;

	void unPairDevice(const BleAddress &device) override;

	void deviceInfo(const BleAddress &device) override;

	void findMe(const BleAddress &device, FindMeLevel level) override;

	void startAudioStreaming(const BleAddress &device, AudioStreamingCodec codec, const QString &filePath) override;
	void stopAudioStreaming(const BleAddress &device) override;
	void setAudioStreamingGain(const BleAddress &device, int level) override;

	void setTrackpad(const BleAddress &device, bool on) override;
	void setSlider(const BleAddress &device, bool on) override;

	void programIrSignals(const BleAddress &device, quint32 code, const QStringList &irSignals) override;
	void eraseIrSignals(const BleAddress &device) override;
	void sendIrSignal(const BleAddress &device, const QString &signal) override;
	void parseEDID(const BleAddress &device, bool async, const QByteArray &edid) override;
	void getIrManufacturers(const BleAddress &device, const QString &search, IrLookupType type, bool sort) override;
	void getIrModels(const BleAddress &device, const QString &manuf, const QString &search, IrLookupType type) override;
	void getIrCodes(const BleAddress &device, const QString &manuf, const QString &model, IrLookupType type) override;

	void fwStartUpgrade(const BleAddress &device, const QString &filePath) override;
	void fwCancelUpgrade(const BleAddress &device) override;

	void getLogLevel() const override;
	void setLogLevel(const QString &level) override;

	void getLogSyslog() const override;
	void setLogSyslog(bool enable) override;
	void getLogEthanlog() const override;
	void setLogEthanlog(bool enable) override;

	void getHciCaptureState() const override;
	void enableHciCapture() override;
	void disableHciCapture() override;
	void clearHciCapture() override;
	void dumpHciCapture(const QString &filePath) override;

private slots:
	void onDeviceAdded(const QDBusObjectPath &path, const QString &address);
	void onDeviceRemoved(const QDBusObjectPath &path, const QString &address);

	void onPairingStateChanged(bool isPairing);
	void onStateChanged(quint32 status);

	void onBatteryLevelChanged(const BleAddress &device, quint8 level);
	void onConnectedChanged(const BleAddress &device, bool connected);
	void onNameChanged(const BleAddress &device, const QString &name);
	void onIrCodeChanged(const BleAddress &device, quint32 code);
	void onTouchModeChanged(const BleAddress &device, quint32 mode);
	void onAudioStreamingChanged(const BleAddress &device, bool streaming);
	void onAudioGainLevelChanged(const BleAddress &device, qint32 gainLevel);

	void onFwUpgradeStateChanged(const BleAddress &device, bool isUpgrading);
	void onFwUpgradeProgressChanged(const BleAddress &device, qint32 progress);

private:
	void addDevice(const QDBusObjectPath &path);
	void removeDevice(const BleAddress &bdaddr);

	quint16 irSignalNameToKeyCode(const QString &name) const;

private:
	const QDBusConnection m_bus;
	const QString m_serviceName;

	QSharedPointer<ComSkyBleRcuController1Interface> m_blercuController1;
	QSharedPointer<ComSkyBleRcuDebug1Interface> m_blercuDebug1;
	QSharedPointer<ComSkyBleRcuHciCapture1Interface> m_blercuHciCapture1;

	QMap<BleAddress, QSharedPointer<ComSkyBleRcuDevice1Interface>> m_devices;
	QMap<BleAddress, QSharedPointer<ComSkyBleRcuUpgrade1Interface>> m_deviceUpgrades;

	QSharedPointer<AudioWavFile> m_wavFile;
};


#endif // !defined(BLERCU_CMDHANDLER_H)
