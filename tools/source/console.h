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
//  commandhandler.h
//  SkyBluetoothRcu
//

#ifndef CONSOLE_H
#define CONSOLE_H

#include "readline/readline.h"
#include "base_cmdhandler.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSharedPointer>
#include <QDBusConnection>
#include <QDBusObjectPath>




class Console : public QObject
{
	Q_OBJECT

public:
	explicit Console(const QSharedPointer<BaseCmdHandler> &cmdHandler,
	                 QObject *parent = nullptr);
	~Console() final = default;

public:
	void start();

private:
	void initReadLine();

	bool parseOnOffString(const QString &str, bool *on) const;
	BaseCmdHandler::IrLookupType parseIrType(const QString &type) const;
	bool isValidIrSignalName(const QString &signal) const;

private slots:
	void onShowCommand(const QStringList &args);
	void onListDevicesCommand(const QStringList &args);
	void onListConnectedDevicesCommand(const QStringList &args);
	void onStartPairingCommand(const QStringList &args);
	void onStartScanningCommand(const QStringList &args);
	void onInfoCommand(const QStringList &args);
	void onUnpairCommand(const QStringList &args);
	void onFindMeCommand(const QStringList &args);
	void onAudioStreamingCommand(const QStringList &args);
	void onSetAudioGainCommand(const QStringList &args);
	void onTrackpadCommand(const QStringList &args);
	void onSliderCommand(const QStringList &args);

	void onIrProgramCommand(const QStringList &args);
	void onIrEraseCommand(const QStringList &args);
	void onIrSendCommand(const QStringList &args);
	void onParseEDIDCommand(const QStringList &args);
	void onIrGetManufCommand(const QStringList &args);
	void onIrGetModelCommand(const QStringList &args);
	void onIrGetCodesCommand(const QStringList &args);

	void onFwUpgradeCommand(const QStringList &args);

	void onLogLevelCommand(const QStringList &args);
	void onLogSyslogCommand(const QStringList &args);
	void onLogEthanlogCommand(const QStringList &args);

	void onHciCaptureCommand(const QStringList &args);

private:
	const QSharedPointer<BaseCmdHandler> m_cmdHandler;

private:
	ReadLine m_readLine;
};

#endif // !defined(CONSOLE_H)
