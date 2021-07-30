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
//  cmdlineoptions.h
//  SkyBluetoothRcu
//

#ifndef CMDLINEOPTIONS_H
#define CMDLINEOPTIONS_H

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QString>
#include <QVector>
#include <QPair>

#include <functional>

class CmdLineOptions
{
public:
	CmdLineOptions();
	~CmdLineOptions();

	void process(const QCoreApplication &app);

public:
	enum DBusType{
		NoBus,
		SessionBus,
		SystemBus,
		CustomBus
	};

	DBusType dbusType() const;
	QString dbusAddress() const;
	QString dbusServiceName() const;

	DBusType debugDBusType() const;
	QString debugDBusAddress() const;

	int networkNamespace() const;

	int takeHciSocket() const;
	uint hciDeviceId() const;

	QString audioFifoDirectory() const;

	QString irDatabasePluginPath() const;

	bool enableScanMonitor() const;

	bool enablePairingWebServer() const;

private:
	void showVersion(const QString &ignore);

	void increaseVerbosity(const QString &ignore);

	void enableSysLog(const QString &ignore);

#if defined(RDK)
	void enableJournald(const QString &ignore);
#endif

	void setGroupId(const QString &groupName);
	void setUserId(const QString &userName);
	void setRtPriority(const QString &priority);
	void closeConsole(const QString &ignore);

	void setDBusService(const QString &name);
	void setDBusSystem(const QString &ignore);
	void setDBusSession(const QString &ignore);
	void setDBusAddress(const QString &address);

	void setDebugDBusAddress(const QString &address);

	void setNetworkNamespace(const QString &networkNsStr);

	void setHciDevice(const QString &hciDeviceStr);
	void setHciSocket(const QString &hciSocketStr);

	void setAudioFifoDirectory(const QString &audioFifoPath);

	void setIrDatabasePluginFile(const QString &irDatabasePluginPath);

	void setDisableScanMonitor(const QString &ignore);

	void setEnablePairingWebServer(const QString &ignore);

private:
	typedef std::function<void(const QString&)> OptionHandler;
	QList< QPair<QCommandLineOption, OptionHandler> > m_options;

private:
	QCommandLineParser m_parser;

	DBusType m_busType;
	QString m_busAddress;
	QString m_serviceName;

	DBusType m_debugBusType;
	QString m_debugBusAddress;

	int m_netNsFd;

	mutable int m_hciSocketFd;
	uint m_hciDeviceId;

	QString m_audioFifoPath;

	QString m_irDatabasePluginPath;

	bool m_enableScanMonitor;

	bool m_enablePairingWebServer;
};

#endif // !defined(CMDLINEOPTIONS_H)
