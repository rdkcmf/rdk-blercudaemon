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
//  cmdlineoptions.cpp
//  SkyBluetoothRcu
//

#include "cmdlineoptions.h"
#include "utils/logging.h"

#include <QDebug>
#include <QFileInfo>

#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>



CmdLineOptions::CmdLineOptions()
	: m_busType(SystemBus)
	, m_serviceName(QStringLiteral("com.sky.blercu"))
	, m_debugBusType(NoBus)
	, m_netNsFd(-1)
	, m_hciSocketFd(-1)
	, m_hciDeviceId(0)
	, m_audioFifoPath("/tmp")
	, m_irDatabasePath(":irdb.sqlite")
	, m_enableScanMonitor(true)
	, m_enablePairingWebServer(false)
{

	m_parser.setApplicationDescription("Bluetooth RCU Daemon");
	m_parser.addHelpOption();

	QList< QPair<QCommandLineOption, OptionHandler> > options = {

		{ QCommandLineOption( { "V", "version" },   "Displays version information." ),
			std::bind(&CmdLineOptions::showVersion, this, std::placeholders::_1) },

		{ QCommandLineOption( { "v", "verbose" },   "Enables verbose output <false>." ),
			std::bind(&CmdLineOptions::increaseVerbosity, this, std::placeholders::_1) },

		{ QCommandLineOption( { "k", "noconsole" }, "Disable console output on stdout / stderr <false>." ),
			std::bind(&CmdLineOptions::closeConsole, this, std::placeholders::_1) },
		{ QCommandLineOption( { "l", "syslog" },    "Enables logging to syslog along with standard logging <false>." ),
			std::bind(&CmdLineOptions::enableSysLog, this, std::placeholders::_1) },

#if defined(RDK)
		{ QCommandLineOption( { "j", "journald" },  "Enables logging to journald along with standard logging <false>." ),
		  std::bind(&CmdLineOptions::enableJournald, this, std::placeholders::_1) },
#endif

		{ QCommandLineOption( { "g", "group" },     "The group id to run the process as <root>.", "id" ),
			std::bind(&CmdLineOptions::setGroupId, this, std::placeholders::_1) },
		{ QCommandLineOption( { "u", "user" },      "The user id to run the process as <root>.", "id" ),
			std::bind(&CmdLineOptions::setUserId, this, std::placeholders::_1) },

		{ QCommandLineOption( { "p", "priority" },  "The realtime priority to run the daemon as <6>.", "priority" ),
			std::bind(&CmdLineOptions::setRtPriority, this, std::placeholders::_1) },

		{ QCommandLineOption(        "service",     "The dbus service name to use <com.sky.blercu>.", "name" ),
			std::bind(&CmdLineOptions::setDBusService, this, std::placeholders::_1) },
		{ QCommandLineOption(        "system",      "Use the system dbus <default>." ),
			std::bind(&CmdLineOptions::setDBusSystem, this, std::placeholders::_1) },
		{ QCommandLineOption(        "session",     "Use the session dbus." ),
			std::bind(&CmdLineOptions::setDBusSession, this, std::placeholders::_1) },
		{ QCommandLineOption( { "a", "address" },   "The address of the dbus to use.", "address" ),
			std::bind(&CmdLineOptions::setDBusAddress, this, std::placeholders::_1) },

		{ QCommandLineOption( { "b", "debug-dbus-address" }, "The address of a dbus to proxy debug information on.", "address" ),
			std::bind(&CmdLineOptions::setDebugDBusAddress, this, std::placeholders::_1) },

		{ QCommandLineOption( { "n", "netns" },     "The host network namespace <-1>", "fd" ),
			std::bind(&CmdLineOptions::setNetworkNamespace, this, std::placeholders::_1) },

		{ QCommandLineOption( { "d", "hci" },       "The bt hci device to use for monitoring <0>", "devid" ),
			std::bind(&CmdLineOptions::setHciDevice, this, std::placeholders::_1) },
		{ QCommandLineOption( { "s", "hcisocket" }, "The bt hci socket descriptor to use for monitoring <-1>", "fd" ),
			std::bind(&CmdLineOptions::setHciSocket, this, std::placeholders::_1) },

		{ QCommandLineOption( { "f", "audio-fifo-dir" }, "Directory to use for audio fifos </tmp>", "path" ),
			std::bind(&CmdLineOptions::setAudioFifoDirectory, this, std::placeholders::_1) },

		{ QCommandLineOption( { "i", "ir-database" }, "Path to the sqlite IR database to use <:irdb.sqlite>", "path" ),
			std::bind(&CmdLineOptions::setIrDatabaseFile, this, std::placeholders::_1) },

		{ QCommandLineOption( { "m", "disable-scan-monitor" }, "Disables the LE scan monitoring for production logging." ),
			std::bind(&CmdLineOptions::setDisableScanMonitor, this, std::placeholders::_1) },

		{ QCommandLineOption( { "w", "enable-pairing-webserver" }, "Enables a webserver (on port 8280) to trigger pairing." ),
			std::bind(&CmdLineOptions::setEnablePairingWebServer, this, std::placeholders::_1) },
	};

	m_options.swap(options);

	QList< QPair<QCommandLineOption, OptionHandler> >::const_iterator it = m_options.cbegin();
	for (; it != m_options.end(); ++it)
		m_parser.addOption(it->first);

}

CmdLineOptions::~CmdLineOptions()
{
	if ((m_hciSocketFd >= 0) && (close(m_hciSocketFd) != 0))
		qErrnoWarning(errno, "failed to close hci socket");

	if ((m_netNsFd >= 0) && (close(m_netNsFd) != 0))
		qErrnoWarning(errno, "failed to close hci socket");

}

// -----------------------------------------------------------------------------
/*!
	Process the command line for the current app.

	After this call you can use the various getters to get the value of any
	command line option.

 */
void CmdLineOptions::process(const QCoreApplication &app)
{
	m_parser.process(app);

	// iterate through the options supplied
	const QStringList options = m_parser.optionNames();
	for (const QString &option : options) {

		// find the handler
		QList< QPair<QCommandLineOption, OptionHandler> >::const_iterator it = m_options.cbegin();
		for (; it != m_options.end(); ++it) {

			const QCommandLineOption &cmdLineOpt = it->first;
			const OptionHandler &handler = it->second;

			if (handler && cmdLineOpt.names().contains(option)) {
				handler(m_parser.value(option));
				break;
			}
		}
	}
}

// -----------------------------------------------------------------------------
/*!
	Returns the type of dbus selected by the user via the command line options.

	\note Calling this before CmdLineOptions::process() will just return the
	default value which is CmdLineOptions::SystemBus.
 */
CmdLineOptions::DBusType CmdLineOptions::dbusType() const
{
	return m_busType;
}

// -----------------------------------------------------------------------------
/*!
	Returns the dbus address string, the address is only valid if dbusType()
	returns CmdLineOptions::CustomBus.

	\note Calling this before CmdLineOptions::process() will just return the
	default value which is an empty string.
 */
QString CmdLineOptions::dbusAddress() const
{
	return m_busAddress;
}

// -----------------------------------------------------------------------------
/*!
	Returns the name of the service to register this app as on dbus.

	\note Calling this before CmdLineOptions::process() will just return the
	default value which \c "com.sky.blercu".
 */
QString CmdLineOptions::dbusServiceName() const
{
	return m_serviceName;
}

// -----------------------------------------------------------------------------
/*!
	Returns the type of dbus selected by the user via the command line options
	for debug output.

	This option is only enabled on debug builds.

	\note Calling this before CmdLineOptions::process() will just return the
	default value which is CmdLineOptions::NoBus.
 */
CmdLineOptions::DBusType CmdLineOptions::debugDBusType() const
{
	return m_debugBusType;
}

// -----------------------------------------------------------------------------
/*!
	Returns the dbus address string for the debug bus, the address is only valid
	if debugDBusType() returns CmdLineOptions::CustomBus.

	This option is only enabled on debug builds.

	\note Calling this before CmdLineOptions::process() will just return the
	default value which is an empty string.
 */
QString CmdLineOptions::debugDBusAddress() const
{
	return m_debugBusAddress;
}

// -----------------------------------------------------------------------------
/*!
	Returns the file descriptor for the network namespace to use if one was
	specified on the command line.

	\warning If a network namespace is supplied by the caller it is dup'd and
	stored within this object, on desctruction of this object it is closed,
	therefore the returned file descriptor should not be used after this
	object is desctructed.

	\note Calling this before CmdLineOptions::process() will just return the
	default value which is -1, i.e. no custom network namespace.
 */
int CmdLineOptions::networkNamespace() const
{
	return m_netNsFd;
}

// -----------------------------------------------------------------------------
/*!
	Takes the file descriptor for the hci socket to use if one was specified
	on the command line. The socket can only be taken once, subsequent attempts
	to call this function will return -1.

	\note Calling this before CmdLineOptions::process() will just return the
	default value which is -1, i.e. no hci socket
 */
int CmdLineOptions::takeHciSocket() const
{
	int socketFd = m_hciSocketFd;
	m_hciSocketFd = -1;
	return socketFd;
}

// -----------------------------------------------------------------------------
/*!
	Returns the id of the hci device as specified on the command line.

	\note Calling this before CmdLineOptions::process() will just return the
	default value which 0 (/dev/hci0)
 */
uint CmdLineOptions::hciDeviceId() const
{
	return m_hciDeviceId;
}

// -----------------------------------------------------------------------------
/*!
	Returns the optional path set for creating the audio fifos.

	\note Calling this before CmdLineOptions::process() will just return the
	default value which is an empty string.
 */
QString CmdLineOptions::audioFifoDirectory() const
{
	return m_audioFifoPath;
}

// -----------------------------------------------------------------------------
/*!
	Returns the path to the IR sqlite database file to use.

	\note Calling this before CmdLineOptions::process() will just return the
	default value.
 */
QString CmdLineOptions::irDatabasePath() const
{
	return m_irDatabasePath;
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the LE scan prod logs monitoring should be enabled. By
	default it is enabled.

	\note Calling this before CmdLineOptions::process() will just return the
	default value.
 */
bool CmdLineOptions::enableScanMonitor() const
{
	return m_enableScanMonitor;
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the pairing webserver should be enabled.  This allows
	users to browse to a WebUI served from the STB to initiate RCU pairing
	without the IR signal.

	\note Calling this before CmdLineOptions::process() will just return the
	default value.
 */
bool CmdLineOptions::enablePairingWebServer() const
{
	return m_enablePairingWebServer;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Sets the current gid of the running process.
 */
void CmdLineOptions::setGroupId(const QString &groupName)
{
	// if the group name can be converted to an int then assume it's a gid
	bool ok = false;
	gid_t gid = groupName.toInt(&ok, 0);

	if (!ok) {
		// couldn't be converted to an int so look it up (not thread safe but
		// only call early at startup so don't care)
		struct group *group = getgrnam(groupName.toLatin1().constData());
		if (!group) {
			qErrnoWarning(errno, "failed to find group with name '%s'",
			              groupName.toLatin1().constData());
			exit(EXIT_FAILURE);
		}

		gid = group->gr_gid;
	}

	// try and set the group
	if (setregid(gid, gid) != 0) {
		qErrnoWarning(errno, "failed to switch to gid %d", gid);
		exit(EXIT_FAILURE);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Sets the current uid of the running process.
 */
void CmdLineOptions::setUserId(const QString &userName)
{
	// if the group name can be converted to an int then assume it's a gid
	bool ok = false;
	uid_t uid = userName.toInt(&ok, 0);

	if (!ok) {
		// couldn't be converted to an int so look it up (not thread safe but
		// only call early at startup so don't care)
		struct passwd *passwd = getpwnam(userName.toLatin1().constData());
		if (!passwd) {
			qErrnoWarning(errno, "failed to find user with name '%s'",
			              userName.toLatin1().constData());
			exit(EXIT_FAILURE);
		}

		uid = passwd->pw_uid;
	}

	// try and set the group
	if (setreuid(uid, uid) != 0) {
		qErrnoWarning(errno, "failed to switch to uid %d", uid);
		exit(EXIT_FAILURE);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Sets the RT priority of the daemon.
 */
void CmdLineOptions::setRtPriority(const QString &priority)
{
	bool ok = false;

	int rtprio = priority.toInt(&ok);
	if (!ok || (rtprio <= 0) || (rtprio > 100)) {
		qError("invalid priority argument");
		exit(EXIT_FAILURE);
	}

#if defined(__linux__)
	struct sched_param param;
	param.sched_priority = rtprio;
	sched_setscheduler(0, SCHED_RR, &param);
#endif
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Takes the string supplied on the command line and verifies it corresponds
	to a valid file descriptor and then returns said file descriptor. If not
	a valid file descriptor -1 is returned.

 */
void CmdLineOptions::setNetworkNamespace(const QString &netNsString)
{
	bool isOk = false;
	const int fd = netNsString.toInt(&isOk);

	// sanity check the fd number is valid
	if (!isOk || (fd < 3) || (fd > 1024)) {
		qWarning("failed to parse 'netns' option, it should be an integer");
		return;
	}

	// try and dup it, this really will verify it is a valid fd and set the
	// the cloexec flag
	int netNsFd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
	if (netNsFd < 0) {
		qErrnoWarning(errno, "failed to dup 'netns' option, it should be a valid fd");
		return;
	}

	// close the original fd (because it doesn't have 'cloexec' flag and
	// return our dup'd version
	if (close(fd) != 0)
		qErrnoWarning(errno, "failed to close netns");

	// close the old one (in case user put two --netns options on the cmdline)
	if ((m_netNsFd >= 0) && (close(m_netNsFd) != 0))
		qErrnoWarning(errno, "failed to close old hci socket");

	m_netNsFd = netNsFd;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Redirects stdout/stderr and stdin to /dev/null

 */
void CmdLineOptions::closeConsole(const QString &ignore)
{
	Q_UNUSED(ignore);

	int devNullFd = open("/dev/null", O_RDWR);
	if (devNullFd < 0) {
		qErrnoWarning(errno, "failed to redirect stdin, stdout and stderr to /dev/null");
		return;
	}

	dup2(devNullFd, STDIN_FILENO);
	dup2(devNullFd, STDOUT_FILENO);
	dup2(devNullFd, STDERR_FILENO);
	if (devNullFd > STDERR_FILENO)
		close(devNullFd);
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
void CmdLineOptions::showVersion(const QString &ignore)
{
	Q_UNUSED(ignore);
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
	m_parser.showVersion();
#endif
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Increases the logging verbosity, if this function is called once then
	the 'info' level is enabled, if called more than once then the 'debug'
	level is also enabled.

 */
void CmdLineOptions::increaseVerbosity(const QString &ignore)
{
	Q_UNUSED(ignore);

	static int verbosity = 0;

	LoggingLevels levels = LoggingLevel::Fatal | LoggingLevel::Error |
	                       LoggingLevel::Warning | LoggingLevel::Milestone |
	                       LoggingLevel::Info;
	if (++verbosity > 1)
		levels |= LoggingLevel::Debug;

	setLogLevels(levels);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Enables output to syslog alongside the standard logging channels.

 */
void CmdLineOptions::enableSysLog(const QString &ignore)
{
	Q_UNUSED(ignore);

	LoggingTargets logTargets = getLogTargets();

	logTargets |= LoggingTarget::SysLog;

	setLogTargets(logTargets);
}

#if defined(RDK)
// -----------------------------------------------------------------------------
/*!
	\internal

	Enables output to journald alongside the standard logging channels.

 */
void CmdLineOptions::enableJournald(const QString &ignore)
{
	Q_UNUSED(ignore);

	LoggingTargets logTargets = getLogTargets();

	logTargets |= LoggingTarget::Journald;

	setLogTargets(logTargets);
}
#endif // defined(RDK)

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void CmdLineOptions::setDBusService(const QString &name)
{
	m_serviceName = name;
}

void CmdLineOptions::setDBusSystem(const QString &ignore)
{
	Q_UNUSED(ignore);
	m_busType = SystemBus;
}

void CmdLineOptions::setDBusSession(const QString &ignore)
{
	Q_UNUSED(ignore);
	m_busType = SessionBus;
}

void CmdLineOptions::setDBusAddress(const QString &address)
{
	m_busType = CustomBus;
	m_busAddress = address;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Sets the address of the debug dbus to use, this is typically set to the
	AI public bus so that apps can talk to this daemon for debugging.

 */
void CmdLineOptions::setDebugDBusAddress(const QString &address)
{
	m_debugBusType = CustomBus;
	m_debugBusAddress = address;
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void CmdLineOptions::setHciDevice(const QString &hciDeviceStr)
{
	bool isOk = false;
	const int id = hciDeviceStr.toInt(&isOk);

	// sanity check the fd number is valid
	if (!isOk || (id < 0) || (id > 100)) {
		qWarning("failed to parse 'hci' option, it should be a positive integer");
		return;
	}

	m_hciDeviceId = static_cast<uint>(id);
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void CmdLineOptions::setHciSocket(const QString &hciSocketStr)
{
	int sock = hciSocketStr.toInt();

	struct stat buf;
	if (sock < 3)
		sock = -1;
	else if (fstat(sock, &buf) != 0)
		sock = -1;
	else if (!S_ISSOCK(buf.st_mode))
		sock = -1;

	if (sock < 0) {
		qWarning("the 'hcisocket' argument is malformed or doesn't "
		         "correspond to a socket");
		return;
	}

	// try and dup the supplied socket
	int duppedFd = fcntl(sock, F_DUPFD_CLOEXEC, 3);
	if (duppedFd < 0) {
		qErrnoWarning(errno, "failed to dup hci socket");
		close(sock);
		return;
	}

	// close the user supplied socket
	if (close(sock) != 0)
		qErrnoWarning(errno, "failed to close user hci socket");

	// close the old one (in case user put two --hcisocket options on the cmdline)
	if ((m_hciSocketFd >= 0) && (close(m_hciSocketFd) != 0))
		qErrnoWarning(errno, "failed to close old hci socket");

	m_hciSocketFd = duppedFd;
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void CmdLineOptions::setAudioFifoDirectory(const QString &audioFifoPath)
{
	const QByteArray _audioFifoPath = audioFifoPath.toLatin1();

	// check if the path exists
	QFileInfo info(audioFifoPath);
	if (!info.exists()) {

		// attempt to make the dir with 'drwxr-x---' perms
		if (mkdir(_audioFifoPath.constData(), 0750) != 0) {
			qErrnoWarning(errno, "failed to create dir '%s'", _audioFifoPath.constData());
			return;
		}

		if (chmod(_audioFifoPath.constData(), 0750) != 0)
			qErrnoWarning(errno, "failed to set perms on '%s'", _audioFifoPath.constData());

	} else {

		// the file / directory already exists, sanity check it is actually a
		// directory and it's writeable
		if (!info.isDir()) {
			qWarning("supplied path for audio fifo(s) is not a directory");
			return;
		}

		if (!info.isWritable())
			qWarning("supplied path for audio fifo(s) is writable");

	}

	m_audioFifoPath = audioFifoPath;
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void CmdLineOptions::setIrDatabaseFile(const QString &irDatabasePath)
{
	QFileInfo info(irDatabasePath);
	if (!info.exists())
		qWarning("failed to find ir database file @ '%s'", qPrintable(irDatabasePath));
	else if (!info.isReadable())
		qWarning("ir database file @ '%s' is not readable", qPrintable(irDatabasePath));

	m_irDatabasePath = irDatabasePath;
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void CmdLineOptions::setDisableScanMonitor(const QString &ignore)
{
	Q_UNUSED(ignore);

	m_enableScanMonitor = false;
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void CmdLineOptions::setEnablePairingWebServer(const QString &ignore)
{
	Q_UNUSED(ignore);

	m_enablePairingWebServer = true;
}
