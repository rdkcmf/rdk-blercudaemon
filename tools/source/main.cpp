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
//  main.cpp
//  SkyBluetoothRcu
//

#include "console.h"
#include "utils/unixsignalnotifier.h"

#include "base_cmdhandler.h"
#include "blercu_cmdhandler.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>

#include <QFileInfo>
#include <QSharedPointer>
#include <QDBusConnection>
#include <QDBusError>

#include <signal.h>
#include <unistd.h>




static QDBusConnection guessDBusToUse(const QString &dbusName)
{
	const QString aiPrivateBusSocket("/mnt/nds/tmpfs/APPLICATIONS_WORKSPACE/dbus/socket/private/serverfd");

	// check if we have an 'ai-private' dbus running, if so use that, otherwise
	// drop back to the system bus
    if (QFileInfo(aiPrivateBusSocket).exists())
	{
		return QDBusConnection::connectToBus(QString("unix:path=") +
		                                     aiPrivateBusSocket, dbusName);
	}
	else
	{
		return QDBusConnection::connectToBus(QDBusConnection::SystemBus, dbusName);
	}
}

int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName("BleRcuConsole");
	QCoreApplication::setApplicationVersion(BLUETOOTHRCU_VERSION);

	QCommandLineParser parser;
	parser.setApplicationDescription("Bluetooth RCU Console");
	parser.addHelpOption();
	parser.addVersionOption();

#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
    //QCommandLineOption oOption (QStringList() << QStringLiteral("d") << QStringLiteral("dependencies"));
    //oOption.setDescription(QStringLiteral("Display the dependencies."));
    //parser.addOption(oOption);

    QCommandLineOption vOption(QStringList() << "v" << "verbose", "Enables verbose output <false>.");
    QCommandLineOption aOption(QStringList() << "a" << "address", "The address of the dbus to use.", "address");
    parser.addOption(vOption);
    parser.addOption(aOption);
	QCommandLineOption serOption("service",          "The dbus service name of the RCU daemon <com.sky.blercudaemon>.", "name");
	QCommandLineOption sysOption("system",           "Use the system dbus <default>.");
	QCommandLineOption sesOption("session",          "Use the session dbus.");
    parser.addOption(serOption);
    parser.addOption(sysOption);
    parser.addOption(sesOption);
#else
	const QList<QCommandLineOption> options = {
		{ { "v", "verbose" }, "Enables verbose output <false>." },
		{ "service",          "The dbus service name of the RCU daemon <com.sky.blercudaemon>.", "name" },
		{ "system",           "Use the system dbus <default>." },
		{ "session",          "Use the session dbus." },
		{ { "a", "address" }, "The address of the dbus to use.", "address" },
	};
	parser.addOptions(options);
#endif
	// process the actual command line arguments given by the user
	parser.process(app);

	// disable SIGPIPE early
	signal(SIGPIPE, SIG_IGN);


	// connect to the dbus specified in the args
	const QString dbusName =  QString("com.sky.blercuconsole.pid%1").arg(getpid());

	QDBusConnection dbus("");
	if (parser.isSet("address"))
		dbus = QDBusConnection::connectToBus(parser.value("address"), dbusName);
	else if (parser.isSet("session"))
		dbus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, dbusName);
	else if (parser.isSet("system"))
		dbus = QDBusConnection::connectToBus(QDBusConnection::SystemBus, dbusName);
	else
		dbus = guessDBusToUse(dbusName);

	if (!dbus.isConnected()) {
		qWarning() << "failed to connect to dbus due to" << dbus.lastError();
		exit(EXIT_FAILURE);
	}


	// set the service name of the daemon we're targeting
	QString serviceName = QStringLiteral("com.sky.blercu");
	if (parser.isSet("service"))
		serviceName = parser.value("service");



	QSharedPointer<BaseCmdHandler> cmdHandler;
	cmdHandler = QSharedPointer<BleRcuCmdHandler>::create(dbus, serviceName);

	// create a unix signal handler to capture the ctrl-c signal and do an
	// ordered clean up (needed for readline / console tidy up)
	UnixSignalNotifier unixSignalNotifier(SIGINT, &app);
	QObject::connect(&unixSignalNotifier, &UnixSignalNotifier::activated,
	                 &app, QCoreApplication::quit);


	// create the command handler and start it
	Console *console = new Console(cmdHandler, &app);
	console->start();

	
	return app.exec();
}
