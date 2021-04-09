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
//  base_cmdhandler.h
//  SkyBluetoothRcu
//

#ifndef BASE_CMDHANDLER_H
#define BASE_CMDHANDLER_H

#include "utils/bleaddress.h"

#include <QObject>
#include <QString>


class BaseCmdHandler : public QObject
{
	Q_OBJECT

public:
	explicit BaseCmdHandler(QObject *parent = nullptr);
	~BaseCmdHandler() override = default;

public:
	enum FindMeLevel {
		Off = 0,
		Mid = 1,
		High = 2
	};
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
	Q_ENUM(FindMeLevel);
#else
	Q_ENUMS(FindMeLevel);
#endif

	enum AudioStreamingCodec {
		PCM = 0,
		ADPCM = 1,
	};
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
	Q_ENUM(AudioStreamingCodec);
#else
	Q_ENUMS(AudioStreamingCodec);
#endif

	enum IrLookupType {
		Invalid = 0,
		Any = 1,
		TV = 2,
		AVAmp = 3,
	};
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
	Q_ENUM(IrLookupType);
#else
	Q_ENUMS(IrLookupType);
#endif

public:
	virtual bool isValid() = 0;

	virtual QString prompt() = 0;

public slots:
	virtual void show() = 0;

	virtual void listDevices() = 0;
	virtual void listConnectedDevices() = 0;

	virtual void startPairing(quint8 pairingCode) = 0;
	virtual void cancelPairing() = 0;
	virtual void startScanning(quint32 timeout) = 0;

	virtual void unPairDevice(const BleAddress &device) = 0;

	virtual void deviceInfo(const BleAddress &device) = 0;

	virtual void findMe(const BleAddress &device, FindMeLevel level) = 0;

	virtual void startAudioStreaming(const BleAddress &device, AudioStreamingCodec codec, const QString &filePath) = 0;
	virtual void stopAudioStreaming(const BleAddress &device) = 0;
	virtual void setAudioStreamingGain(const BleAddress &device, int level) = 0;

	virtual void setTrackpad(const BleAddress &device, bool on) = 0;
	virtual void setSlider(const BleAddress &device, bool on) = 0;

	virtual void programIrSignals(const BleAddress &device, quint32 code, const QStringList &irSignals) = 0;
	virtual void eraseIrSignals(const BleAddress &device) = 0;
	virtual void sendIrSignal(const BleAddress &device, const QString &signal) = 0;
	virtual void parseEDID(const BleAddress &device, bool async, const QByteArray &edid) = 0;
	virtual void getIrManufacturers(const BleAddress &device, const QString &search, IrLookupType type, bool sort) = 0;
	virtual void getIrModels(const BleAddress &device, const QString &manuf, const QString &search, IrLookupType type) = 0;
	virtual void getIrCodes(const BleAddress &device, const QString &manuf, const QString &model, IrLookupType type) = 0;

	virtual void fwStartUpgrade(const BleAddress &device, const QString &filePath) = 0;
	virtual void fwCancelUpgrade(const BleAddress &device) = 0;

	virtual void getLogLevel() const = 0;
	virtual void setLogLevel(const QString &level) = 0;

	virtual void getLogSyslog() const = 0;
	virtual void setLogSyslog(bool enable) = 0;
	virtual void getLogEthanlog() const = 0;
	virtual void setLogEthanlog(bool enable) = 0;

	virtual void getHciCaptureState() const = 0;
	virtual void enableHciCapture() = 0;
	virtual void disableHciCapture() = 0;
	virtual void clearHciCapture() = 0;
	virtual void dumpHciCapture(const QString &filePath) = 0;

};

#endif // !defined(BASE_CMDHANDLER_H)
