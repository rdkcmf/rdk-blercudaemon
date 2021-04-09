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
//  console.cpp
//  SkyBluetoothRcu
//

#include "console.h"
#include "utils/bleaddress.h"

#include <QDebug>
#include <QFile>


Console::Console(const QSharedPointer<BaseCmdHandler> &cmdHandler,
                 QObject *parent)
	: QObject(parent)
	, m_cmdHandler(cmdHandler)
{
	// initialise the inactive console
	initReadLine();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Initialises the readline interface.


 */
void Console::initReadLine()
{
	m_readLine.setPrompt(m_cmdHandler->prompt());

	m_readLine.addCommand("show", { }, "Shows info on the controller object",
	                      this, &Console::onShowCommand);

	m_readLine.addCommand("devices", { }, "List available devices",
	                      this, &Console::onListDevicesCommand);
	m_readLine.addCommand("connected-devices", { }, "List connected devices",
	                      this, &Console::onListConnectedDevicesCommand);

	m_readLine.addCommand("pair", { "<on/off>", "<code>" }, "Start/stop pairing using pairing byte code",
	                      this, &Console::onStartPairingCommand);

	m_readLine.addCommand("scan", { "<timeout in ms>" }, "Start scanning",
	                      this, &Console::onStartScanningCommand);

	m_readLine.addCommand("unpair", { "<dev>" }, "Unpair device",
	                      this, &Console::onUnpairCommand);
	m_readLine.addCommand("info", { "<dev>" }, "Device information",
	                      this, &Console::onInfoCommand);
	m_readLine.addCommand("findme", { "<dev>", "<off/mid/high>" }, "Turn on/off find me for device",
	                      this, &Console::onFindMeCommand);

	m_readLine.addCommand("audio", { "<dev>", "<start/stop>", "[filepath]" }, "Turn on/off audio streaming",
	                      this, &Console::onAudioStreamingCommand);
	m_readLine.addCommand("set-audio-gain-level", { "<dev>", "<level>" }, "Set the audio gain level",
	                      this, &Console::onSetAudioGainCommand);

	m_readLine.addCommand("trackpad", { "<dev>", "<on/off>" }, "Enable/disable the trackpad",
	                      this, &Console::onTrackpadCommand);
	m_readLine.addCommand("slider", { "<dev>", "<on/off>" }, "Enable/disable the slider",
	                      this, &Console::onSliderCommand);

	m_readLine.addCommand("ir-program", { "<dev>", "<code>", "[signals...]" }, "Programs the IR code(s) into the rcu",
	                      this, &Console::onIrProgramCommand);
	m_readLine.addCommand("ir-erase", { "<dev>", }, "Erases IR code(s) programmed into the rcu",
	                      this, &Console::onIrEraseCommand);
	m_readLine.addCommand("ir-send", { "<dev>", "<signal>" }, "Asks the RCU to send a given IR signal",
	                      this, &Console::onIrSendCommand);

	m_readLine.addCommand("ir-parse-edid", { "<dev>", "[async]", "[edid]" }, "Parses the EDID sending back code list. If [edid] is supplied it should be hex string of 128 bytes",
	                      this, &Console::onParseEDIDCommand);

	m_readLine.addCommand("ir-get-manuf", { "<dev>", "<any/amp/tv>", "<search>", "[sort]" }, "Retreives a list of manufacturers",
	                      this, &Console::onIrGetManufCommand);
	m_readLine.addCommand("ir-get-model", { "<dev>", "<any/amp/tv>", "<manuf>", "<search>" }, "Retreives a list of models",
	                      this, &Console::onIrGetModelCommand);
	m_readLine.addCommand("ir-get-codes", { "<dev>", "<any/amp/tv>", "<manuf>", "<model>" }, "Retreives a list of IR codes",
	                      this, &Console::onIrGetCodesCommand);

	m_readLine.addCommand("fw-upgrade", { "<dev>", "<start/cancel>", "[filepath]" }, "Starts / stops upgrade the firmware",
	                      this, &Console::onFwUpgradeCommand);

	m_readLine.addCommand("log-level", { "[fatal/error/warning/milestone/info/debug]" }, "Gets / sets the daemon log level",
	                      this, &Console::onLogLevelCommand);
	m_readLine.addCommand("log-syslog", { "[<on/off>]" }, "Gets / sets logging to syslog",
	                      this, &Console::onLogSyslogCommand);
	m_readLine.addCommand("log-ethanlog", { "[<on/off>]" }, "Gets / sets logging to ethan DIAG log",
	                      this, &Console::onLogEthanlogCommand);

	m_readLine.addCommand("hci-capture", { "[<on/off/clear/dump>] [file]" }, "Enables / disables / clear or dumps the HCI packet capture",
	                      this, &Console::onHciCaptureCommand);


}

// -----------------------------------------------------------------------------
/*!
	Starts the readline interactive console.

 */
void Console::start()
{
	m_readLine.start();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Parses the supplied \a str to determine if it is an 'on' or 'off' string.
	If the supplied string is neither \c false is returned.
 */
bool Console::parseOnOffString(const QString &str, bool *on) const
{
	if (str.compare("on", Qt::CaseInsensitive) == 0) {
		*on = true;
		return true;
	}

	if (str.compare("off", Qt::CaseInsensitive) == 0) {
		*on = false;
		return true;
	}

	return false;
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'show', this command takes no arguments.

	This command just shows the current state of the controller.

 */
void Console::onShowCommand(const QStringList &args)
{
	Q_UNUSED(args);
	m_cmdHandler->show();
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'devices', this command takes no arguments.

	This command lists all the devices managed by the daemon.

 */
void Console::onListDevicesCommand(const QStringList &args)
{
	Q_UNUSED(args);
	m_cmdHandler->listDevices();
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'connected-devices'.

	This just sends out a request to the daemon to get the list of connected
	devices.

 */
void Console::onListConnectedDevicesCommand(const QStringList &args)
{
	Q_UNUSED(args);
	m_cmdHandler->listConnectedDevices();
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'pair <on/off> <code>'.

	This command sends out the request to start / stop pairing.

 */
void Console::onStartPairingCommand(const QStringList &args)
{
	// the first argument should be an 'on' or 'off' string
	if (args.length() < 1) {
		qWarning("Missing <on/off> argument");
		return;
	}

	bool on;
	if (!parseOnOffString(args[0], &on)) {
		qWarning("First argument must either be 'on' or 'off'");
		return;
	}

	// if 'on' then the second argument must be a pairing code
	quint8 pairingCode = 0;
	if (on) {
		if (args.length() < 2) {
			qWarning("Missing pairing code argument");
			return;
		}

		bool parsedOk = false;
		uint pairingCode_ = args[1].toUInt(&parsedOk);
		if (!parsedOk || (pairingCode_ > 255)) {
			qWarning("Invalid pairing code argument");
			return;
		}

		pairingCode = quint8(pairingCode_);
	}

	//
	if (on)
		m_cmdHandler->startPairing(pairingCode);
	else
		m_cmdHandler->cancelPairing();
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'scan <timeout>'.

	This command sends out the request to start scanning.

 */
void Console::onStartScanningCommand(const QStringList &args)
{
	// the first argument should be an 'timeout value'
	if (args.length() < 1) {
		qWarning("Missing <timeout> argument");
		return;
	}

	bool parsedOk = false;
	uint32_t timeout = args.first().toUInt(&parsedOk);
	if (!parsedOk) {
		qWarning("Invalid timeout argument");
		return;
	}

	m_cmdHandler->startScanning(timeout);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'unpair <dev>'.

	This command unpairs the device and removes it from the daemon

 */
void Console::onUnpairCommand(const QStringList &args)
{
	// the first (and only) argument should be the bdaddr of the device
	if (args.length() < 1) {
		qWarning("Missing device address argument");
		return;
	}

	BleAddress address(args.first());
	if (address.isNull()) {
		qWarning("Device address string is not a valid BDADDR");
		return;
	}

	m_cmdHandler->unPairDevice(address);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'info [dev]'.

	This command requests a bunch of stuff from the daemon

 */
void Console::onInfoCommand(const QStringList &args)
{
	// the first (and only) argument should be the bdaddr of the device
	if (args.length() < 1) {
		qWarning("Missing device address argument");
		return;
	}

	BleAddress address(args.first());
	if (address.isNull()) {
		qWarning("Device address string is not a valid BDADDR");
		return;
	}

	m_cmdHandler->deviceInfo(address);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'findme [dev] <off/mid/high>'.

	This sends a dbus request to the daemon to start / stop triggering findme.

 */
void Console::onFindMeCommand(const QStringList &args)
{
	// the first argument should be the bdaddr of the device and the second an
	// findme mode string
	if (args.length() < 2) {
		qWarning("Requires two arguments; [dev] <off/mid/high>");
		return;
	}

	BleAddress address(args[0]);
	if (address.isNull()) {
		qWarning("Device address string is not a valid BDADDR");
		return;
	}

	BaseCmdHandler::FindMeLevel level;

	const QString mode = args[1].toLower();
	if (mode == "off")
		level = BaseCmdHandler::Off;
	else if (mode == "mid")
		level = BaseCmdHandler::Mid;
	else if (mode == "high")
		level = BaseCmdHandler::High;
	else {
		qWarning("Second argument must either be 'off', 'mid' or 'high'");
		return;
	}

	m_cmdHandler->findMe(address, level);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'audio <dev> <start/stop> [filepath]'.

	This sends a dbus request to the daemon to start / stop audio streaming.

 */
void Console::onAudioStreamingCommand(const QStringList &args)
{
	// the first argument should be the bdaddr of the device and the second an
	// off or encoding string
	if (args.length() < 2) {
		qWarning("Requires two arguments; <dev> <start/stop>");
		return;
	}

	BleAddress address(args[0]);
	if (address.isNull()) {
		qWarning("Device address string is not a valid BDADDR");
		return;
	}

	bool start = true;
	QString mode = args[1].toLower();
	if (mode == "start") {
		start = true;
	} else if (mode == "stop") {
		start = false;
	} else {
		qWarning("Invalid control argument '%s', it must be either 'start' or 'stop'",
		         qPrintable(mode));
		return;
	}

	if (!start) {
		m_cmdHandler->stopAudioStreaming(address);
	} else {

		// if starting audio streaming a file path must be supplied
		if (args.length() < 3) {
			qWarning("Must supply a [filepath] argument if starting streaming");
			return;
		}

		const QString filePath = args[2];
		m_cmdHandler->startAudioStreaming(address, BaseCmdHandler::PCM, filePath);
	}
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'set-audio-gain-level [dev] [level]'.

	This sends a dbus request to the daemon to set the audio gain level for the
	given device.

 */
void Console::onSetAudioGainCommand(const QStringList &args)
{
	// the first argument should be the bdaddr of the device and the second an
	// absolute path
	if (args.length() < 2) {
		qWarning("Requires two arguments; [dev] [level]");
		return;
	}

	BleAddress address(args[0]);
	if (address.isNull()) {
		qWarning("Device address string is not a valid BDADDR");
		return;
	}

	bool parsedOk = false;
	int gainLevel = args[1].toInt(&parsedOk);
	if (!parsedOk) {
		qWarning("Audio Gain argument is not a signed integer");
		return;
	}

	m_cmdHandler->setAudioStreamingGain(address, gainLevel);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'trackpad [dev] <on/off>'.

	This sends a dbus request to the daemon to toggle the touch mode bits

 */
void Console::onTrackpadCommand(const QStringList &args)
{
	// the first argument should be the bdaddr of the device and the second an
	// on / off string
	if (args.length() < 2) {
		qWarning("Requires two arguments; [dev] <on/off>");
		return;
	}

	BleAddress address(args[0]);
	if (address.isNull()) {
		qWarning("Device address string is not a valid BDADDR");
		return;
	}

	bool on;
	if (!parseOnOffString(args[1], &on)) {
		qWarning("Second argument must either be 'on' or 'off'");
		return;
	}

	m_cmdHandler->setTrackpad(address, on);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'slider [dev] <on/off>'.

	This sends a dbus request to the daemon to toggle the touch mode bits

 */
void Console::onSliderCommand(const QStringList &args)
{
	// the first argument should be the bdaddr of the device and the second an
	// on / off string
	if (args.length() < 2) {
		qWarning("Requires two arguments; [dev] <on/off>");
		return;
	}

	BleAddress address(args[0]);
	if (address.isNull()) {
		qWarning("Device address string is not a valid BDADDR");
		return;
	}

	bool on;
	if (!parseOnOffString(args[1], &on)) {
		qWarning("Second argument must either be 'on' or 'off'");
		return;
	}

	m_cmdHandler->setSlider(address, on);
}

// -----------------------------------------------------------------------------
/*!
	Utility function to sanity check IR signal strings

 */
bool Console::isValidIrSignalName(const QString &signal) const
{
	return (signal.compare(QLatin1String("power"), Qt::CaseInsensitive) == 0) ||
	       (signal.compare(QLatin1String("volume_up"), Qt::CaseInsensitive) == 0) ||
	       (signal.compare(QLatin1String("volume_down"), Qt::CaseInsensitive) == 0) ||
	       (signal.compare(QLatin1String("mute"), Qt::CaseInsensitive) == 0) ||
	       (signal.compare(QLatin1String("input"), Qt::CaseInsensitive) == 0);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Parse the type string for the IR APIs
 */
BaseCmdHandler::IrLookupType Console::parseIrType(const QString &type) const
{
	if ((type.compare("amp", Qt::CaseInsensitive) == 0) ||
	    (type.compare("AV Amp", Qt::CaseInsensitive) == 0))
		return BaseCmdHandler::AVAmp;

	if (type.compare("tv", Qt::CaseInsensitive) == 0)
		return BaseCmdHandler::TV;

	if (type.compare("any", Qt::CaseInsensitive) == 0)
		return BaseCmdHandler::Any;

	qWarning("Invalid type argument, should be either 'tv', 'amp' or 'any'");
	return BaseCmdHandler::Invalid;
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'ir-program <dev> <code> [signals...]'.

 */
void Console::onIrProgramCommand(const QStringList &args)
{
	if (args.length() < 2) {
		qWarning("Requires at least two arguments; <dev> <code>");
		return;
	}

	BleAddress address(args[0]);
	if (address.isNull()) {
		qWarning("Device address string is not a valid BDADDR");
		return;
	}

	QString code = args[1];
	if (code.isEmpty()) {
		qWarning("IR code string cannot be empty");
		return;
	}
	bool ok = false;
	quint32 codeId = code.toUInt(&ok, 0);
	if (!ok) {
		qWarning("Failed to convert IR code string to unsigned int");
		return;
	}

	QStringList signals_;
	for (int i = 2; i < args.length(); i++) {
		if (!isValidIrSignalName(args[i])) {
			qWarning() << "Invalid signal name" << args[i]
			           << "possible signal names are "
			           << "'power','volume_up','volume_down','mute','input'";
			return;
		}

		signals_ += args[i].toLower();
	}

	m_cmdHandler->programIrSignals(address, codeId, signals_);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'ir-erase <dev>'.

 */
void Console::onIrEraseCommand(const QStringList &args)
{
	if (args.length() != 1) {
		qWarning("Requires one argument; <dev>");
		return;
	}

	BleAddress address(args[0]);
	if (address.isNull()) {
		qWarning("Device address string is not a valid BDADDR");
		return;
	}

	m_cmdHandler->eraseIrSignals(address);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'ir-send <dev> <signal>'.

 */
void Console::onIrSendCommand(const QStringList &args)
{
	if (args.length() != 2) {
		qWarning("Requires two arguments; <dev> <signal>");
		return;
	}

	BleAddress address(args[0]);
	if (address.isNull()) {
		qWarning("Device address string is not a valid BDADDR");
		return;
	}

	QString signal = args[1];
	if (!isValidIrSignalName(signal)) {
		qWarning() << "Invalid signal name" << signal;
		return;
	}

	m_cmdHandler->sendIrSignal(address, signal);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'ir-parse-edid <dev> [async] [edid]'.

 */
void Console::onParseEDIDCommand(const QStringList &args)
{
	if (args.length() < 1) {
		qWarning("Requires at least one argument; <dev> [async] [edid]");
		return;
	}

	BleAddress address(args[0]);
	if (address.isNull()) {
		qWarning("Device address string is not a valid BDADDR");
		return;
	}

	bool async = false;
	QByteArray edid;

	for (int i = 1; i < qMin<int>(3, args.length()); i++) {

		//
		if (args[i].compare("async", Qt::CaseInsensitive)) {
			async = true;
			continue;
		}

		//
		edid = QByteArray::fromHex(args[i].toLatin1());
		if (edid.length() != 128) {
			qWarning("Invalid EDID hex string, must be 128 bytes in size");
			return;
		}

	}

	// an empty edid is allowed, it means the daemon will try and read the edid
	// from proc

	m_cmdHandler->parseEDID(address, async, edid);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'ir-get-manuf <dev> <any/amp/tv> <search> [sort]'.

 */
void Console::onIrGetManufCommand(const QStringList &args)
{
	if (args.length() < 3) {
		qWarning("Requires at least three arguments; <dev> <any/amp/tv> <search>");
		return;
	}

	BleAddress address(args[0]);
	if (address.isNull()) {
		qWarning("Device address string is not a valid BDADDR");
		return;
	}

	BaseCmdHandler::IrLookupType type = parseIrType(args[1]);
	if (type == BaseCmdHandler::Invalid)
		return;

	QString search = args[2];
	search.remove(QChar('\"'));
	search.remove(QChar('\''));

	bool sort = false;

	for (int i = 3; i < args.length(); i++) {
		if (args[i].compare("sort", Qt::CaseInsensitive) == 0)
			sort = true;
	}

	m_cmdHandler->getIrManufacturers(address, search, type, sort);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'ir-get-model <dev> <any/amp/tv> <manuf> <search>'.

 */
void Console::onIrGetModelCommand(const QStringList &args)
{
	if (args.length() < 3) {
		qWarning("Requires at least three arguments; <dev> <any/amp/tv> <manuf>");
		return;
	}

	BleAddress address(args[0]);
	if (address.isNull()) {
		qWarning("Device address string is not a valid BDADDR");
		return;
	}

	BaseCmdHandler::IrLookupType type = parseIrType(args[1]);
	if (type == BaseCmdHandler::Invalid)
		return;

	QString manuf = args[2];
	manuf.remove(QChar('\"'));
	manuf.remove(QChar('\''));
	if (manuf.isEmpty()) {
		qWarning("Manufacturer argument is not allowed to be empty");
		return;
	}

	QString search("");
	if (args.length() >= 4) {
		search = args[3];
		search.remove(QChar('\"'));
		search.remove(QChar('\''));
	}

	m_cmdHandler->getIrModels(address, manuf, search, type);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'ir-get-codes <dev> <any/amp/tv> <manuf> <model>'.

 */
void Console::onIrGetCodesCommand(const QStringList &args)
{
	if (args.length() < 3) {
		qWarning("Requires at least three arguments; <dev> <any/amp/tv> <manuf>");
		return;
	}

	BleAddress address(args[0]);
	if (address.isNull()) {
		qWarning("Device address string is not a valid BDADDR");
		return;
	}

	BaseCmdHandler::IrLookupType type = parseIrType(args[1]);
	if (type == BaseCmdHandler::Invalid)
		return;

	QString manuf = args[2];
	manuf.remove(QChar('\"'));
	manuf.remove(QChar('\''));
	if (manuf.isEmpty()) {
		qWarning("Manufacturer argument is not allowed to be empty");
		return;
	}

	QString model("");
	if (args.length() >= 4) {
		model = args[3];
		model.remove(QChar('\"'));
		model.remove(QChar('\''));
	}

	m_cmdHandler->getIrCodes(address, manuf, model, type);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'fw-upgrade <dev> <start/cancel> [filepath]'.

 */
void Console::onFwUpgradeCommand(const QStringList &args)
{
	if (args.length() < 2) {
		qWarning("Requires at least two arguments; <dev> <file>");
		return;
	}

	BleAddress address(args[0]);
	if (address.isNull()) {
		qWarning("Device address string is not a valid BDADDR");
		return;
	}

	if (args[1].compare(QStringLiteral("start"), Qt::CaseInsensitive) == 0) {

		if (args.length() < 3) {
			qWarning("Start requires a <filepath> argument");
			return;
		}

		if (!QFile::exists(args[2])) {
			qWarning("Failed to open f/w file @ '%s'", qPrintable(args[2]));
			return;
		}

		m_cmdHandler->fwStartUpgrade(address, args[2]);

	} else if (args[1].compare(QStringLiteral("cancel"), Qt::CaseInsensitive) == 0) {

		m_cmdHandler->fwCancelUpgrade(address);

	} else {

		qWarning("Unknown argument '%s'", qPrintable(args[1]));

	}
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'log-level [fatal/error/...]'.

 */
void Console::onLogLevelCommand(const QStringList &args)
{
	if (args.empty()) {
		m_cmdHandler->getLogLevel();
		return;
	}

	static QSet<QString> validLevels = {
		QStringLiteral("fatal"),
		QStringLiteral("error"),
		QStringLiteral("warning"),
		QStringLiteral("milestone"),
		QStringLiteral("info"),
		QStringLiteral("debug")
	};

	const QString level = args[0].toLower();
	if (!validLevels.contains(level)) {
		qWarning("Invalid log level '%s'", level.toLatin1().constData());
		return;
	}

	m_cmdHandler->setLogLevel(level);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'log-syslog [on/off]'.

 */
void Console::onLogSyslogCommand(const QStringList &args)
{
	if (args.empty()) {
		m_cmdHandler->getLogSyslog();
		return;
	}

	bool on;
	if (!parseOnOffString(args[0], &on)) {
		qWarning("If argument supplied it must either be 'on' or 'off'");
		return;
	}

	m_cmdHandler->setLogSyslog(on);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'log-ethanlog [on/off]'.

 */
void Console::onLogEthanlogCommand(const QStringList &args)
{
	if (args.empty()) {
		m_cmdHandler->getLogEthanlog();
		return;
	}

	bool on;
	if (!parseOnOffString(args[0], &on)) {
		qWarning("If argument supplied it must either be 'on' or 'off'");
		return;
	}

	m_cmdHandler->setLogEthanlog(on);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types one of the following commands
 		hci-capture
 		hci-capture on
 		hci-capture off
 		hci-capture clear
 		hci-capture dump <filepath>

 */
void Console::onHciCaptureCommand(const QStringList &args)
{
	if (args.empty()) {
		m_cmdHandler->getHciCaptureState();
		return;
	}

	if (args[0].compare("on", Qt::CaseInsensitive) == 0) {
		m_cmdHandler->enableHciCapture();

	} else if (args[0].compare("off", Qt::CaseInsensitive) == 0) {
		m_cmdHandler->disableHciCapture();

	} else if (args[0].compare("clear", Qt::CaseInsensitive) == 0) {
		m_cmdHandler->clearHciCapture();

	} else if (args[0].compare("dump", Qt::CaseInsensitive) == 0) {

		if (args.length() < 2) {
			qWarning("An output file path must be supplied with the 'dump' command");
			return;
		}

		m_cmdHandler->dumpHciCapture(args[1]);

	}
}
