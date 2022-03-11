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
//  logging.cpp
//  SkyBluetoothRcu
//

#include "logging.h"

#include <QString>
#include <QDebug>
#include <QLibrary>
#include <QMutex>
#include <QElapsedTimer>
#include <QAtomicInteger>

#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>

#include <sys/uio.h>
#include <sys/types.h>

#if defined(__APPLE__)
#  include <mach/clock.h>
#  include <mach/mach.h>
#endif

#if defined(Q_OS_ANDROID)
#  include <android/log.h>
#endif

#if defined(RDK)
#   define SD_JOURNAL_SUPPRESS_LOCATION
#   include <systemd/sd-journal.h>
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
// define a fake category for grouping milestone messages
Q_LOGGING_CATEGORY(milestone, "sky.milestone", QtInfoMsg)

// and another fake category for grouping production log messages
Q_LOGGING_CATEGORY(prodlogs, "sky.prodlogs", QtInfoMsg)

// and one more fake category for grouping rate limited production log messages
Q_LOGGING_CATEGORY(prodlogs_ratelimited, "sky.prodlogs.ratelimted", QtInfoMsg)

#else

// define a fake category for grouping milestone messages
Q_LOGGING_CATEGORY(milestone, "sky.milestone")

// and another fake category for grouping production log messages
Q_LOGGING_CATEGORY(prodlogs, "sky.prodlogs")

// and one more fake category for grouping rate limited production log messages
Q_LOGGING_CATEGORY(prodlogs_ratelimited, "sky.prodlogs.ratelimted")

#endif


#if (AI_BUILD_TYPE == AI_DEBUG)

	// the default log target on a debug build is the console
	static QAtomicInteger<LoggingTargets::Int> g_logTargets(LoggingTarget::Default);

	// the default log level on the debug build is all errors and milestones
	static QAtomicInteger<LoggingLevels::Int> g_logLevels(LoggingLevel::Fatal |
	                                                      LoggingLevel::Error |
	                                                      LoggingLevel::Warning |
	                                                      LoggingLevel::Milestone);
#elif (AI_BUILD_TYPE == AI_RELEASE)

	// on a release build there are no default targets (although EthanLog is
	// expected to be added later
	static QAtomicInteger<LoggingTargets::Int> g_logTargets(0);

	// on a release build all log levels are disabled
	static QAtomicInteger<LoggingLevels::Int> g_logLevels(0);

#else
#	error "Unknown AI_BUILD_TYPE, expected AI_DEBUG or AI_RELEASE"
#endif // (AI_BUILD_TYPE == AI_RELEASE)



#if !defined(Q_OS_ANDROID)

// borrowed from the ethanlog.h header
typedef void (*EthanLogPrototype)(int level, const char *filename,
                                  const char *function, int line,
                                  const char *format, ...);
static EthanLogPrototype g_ethanlogFunc = nullptr;

#define ETHAN_LOG_FATAL     (1)
#define ETHAN_LOG_ERROR     (2)
#define ETHAN_LOG_WARNING   (3)
#define ETHAN_LOG_INFO      (4)
#define ETHAN_LOG_DEBUG     (5)
#define ETHAN_LOG_MILESTONE (6)

// -----------------------------------------------------------------------------
/*!
	\internal

	Attempts to load the ethanlog library and get a pointer to the ethanlog(..)
	logging function.
 */
static bool setupEthanLogging()
{
	if (!qEnvironmentVariableIsSet("ETHAN_LOGGING_PIPE"))
		return false;

	g_ethanlogFunc = (EthanLogPrototype) QLibrary::resolve(QStringLiteral("ethanlog"),
	                                                       "ethanlog");

	return (g_ethanlogFunc != nullptr);
}

// -----------------------------------------------------------------------------
/*!
	Writes the log message out to either stdout or stderr depending on the log
	level specified. This function also prints out a timestamp along with
	log message details.

 */
static inline void logToConsole(LoggingLevel level, const char *file,
                                const char *func, int line, const char *msg,
                                size_t msgLength)
{
	struct timespec ts;

#if defined(Q_OS_LINUX) || defined(Q_OS_ANDROID)
	clock_gettime(CLOCK_MONOTONIC, &ts);

#elif defined(__APPLE__)
	kern_return_t retval;
	clock_serv_t cclock;
	mach_timespec_t mts;

	host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
	retval = clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);

	if (retval != KERN_SUCCESS) {
		ts.tv_sec = 0;
		ts.tv_nsec = 0;
	} else {
		ts.tv_sec = mts.tv_sec;
		ts.tv_nsec = mts.tv_nsec;
	}
#endif

	struct iovec iov[5];
	char tbuf[32];

	iov[0].iov_base = tbuf;
	iov[0].iov_len = snprintf(tbuf, sizeof(tbuf), "%.010lu.%.06lu ",
	                          ts.tv_sec, ts.tv_nsec / 1000);
	iov[0].iov_len = qMin<size_t>(iov[0].iov_len, sizeof(tbuf));

	switch (level) {
		case LoggingLevel::Fatal:
			iov[1].iov_base = (void*)"FTL: ";
			iov[1].iov_len = 5;
			break;
		case LoggingLevel::Error:
			iov[1].iov_base = (void*)"ERR: ";
			iov[1].iov_len = 5;
			break;
		case LoggingLevel::Warning:
			iov[1].iov_base = (void*)"WRN: ";
			iov[1].iov_len = 5;
			break;
		case LoggingLevel::Milestone:
			iov[1].iov_base = (void*)"MIL: ";
			iov[1].iov_len = 5;
			break;
		case LoggingLevel::Info:
			iov[1].iov_base = (void*)"NFO: ";
			iov[1].iov_len = 5;
			break;
		case LoggingLevel::Debug:
			iov[1].iov_base = (void*)"DBG: ";
			iov[1].iov_len = 5;
			break;
		default:
			iov[1].iov_base = (void*)": ";
			iov[1].iov_len = 2;
			break;
	}

	char fbuf[156];
	iov[2].iov_base = (void*)fbuf;
	iov[2].iov_len = snprintf(fbuf, sizeof(fbuf), "< M:%.*s F:%.*s L:%d > ",
	                          64, file ? : "?",
	                          64, func ? : "?",
	                          line);
	iov[2].iov_len = qMin<size_t>(iov[2].iov_len, sizeof(fbuf));

	iov[3].iov_base = const_cast<char*>(msg);
	iov[3].iov_len = msgLength;

	iov[4].iov_base = (void*)"\n";
	iov[4].iov_len = 1;

	writev(fileno(stderr), iov, 5);
}

// -----------------------------------------------------------------------------
/*!
	Writes the log message out to syslog.

 */
static inline void logToSysLog(LoggingLevel level,const char *file,
                               const char *func, int line, const char *msg,
                               size_t msgLength)
{
	Q_UNUSED(msgLength);

	int sysLogLevel = LOG_DEBUG;
	switch (level) {
		case LoggingLevel::Fatal:       sysLogLevel = LOG_ALERT;    break;
		case LoggingLevel::Error:       sysLogLevel = LOG_CRIT;     break;
		case LoggingLevel::Warning:     sysLogLevel = LOG_WARNING;  break;
		case LoggingLevel::Milestone:   sysLogLevel = LOG_NOTICE;   break;
		case LoggingLevel::Info:        sysLogLevel = LOG_INFO;     break;
		case LoggingLevel::Debug:       sysLogLevel = LOG_DEBUG;    break;
	}

	syslog(sysLogLevel, "< M:%.*s F:%.*s L:%d > %s",
	       64, file ? : "?",
	       64, func ? : "?",
	       line,
	       msg);
}

// -----------------------------------------------------------------------------
/*!
	Writes the log message out to ethanlog.

 */
static inline void logToEthanLog(LoggingLevel level,const char *file,
                                 const char *func, int line, const char *msg,
                                 size_t msgLength)
{
	Q_UNUSED(msgLength);

	int ethanLogLevel = ETHAN_LOG_DEBUG;
	switch (level) {
		case LoggingLevel::Fatal:       ethanLogLevel = ETHAN_LOG_FATAL;      break;
		case LoggingLevel::Error:       ethanLogLevel = ETHAN_LOG_ERROR;      break;
		case LoggingLevel::Warning:     ethanLogLevel = ETHAN_LOG_WARNING;    break;
		case LoggingLevel::Milestone:   ethanLogLevel = ETHAN_LOG_MILESTONE;  break;
		case LoggingLevel::Info:        ethanLogLevel = ETHAN_LOG_INFO;       break;
		case LoggingLevel::Debug:       ethanLogLevel = ETHAN_LOG_DEBUG;      break;
	}

	if (Q_LIKELY(g_ethanlogFunc != nullptr))
		g_ethanlogFunc(ethanLogLevel, file, func, line, "%s", msg);
}

#else // defined(Q_OS_ANDROID)

// -----------------------------------------------------------------------------
/*!
	Writes the log message out to the android log.

 */
static inline void logToAndroidLog(LoggingLevel level,const char *file,
                                   const char *func, int line, const char *msg,
                                   size_t msgLength)
{
	Q_UNUSED(msgLength);

	int androidLogLevel = ANDROID_LOG_DEBUG;
	switch (level) {
		case LoggingLevel::Fatal:       androidLogLevel = ANDROID_LOG_FATAL;  break;
		case LoggingLevel::Error:       androidLogLevel = ANDROID_LOG_ERROR;  break;
		case LoggingLevel::Warning:     androidLogLevel = ANDROID_LOG_WARN;   break;
		case LoggingLevel::Milestone:   androidLogLevel = ANDROID_LOG_INFO;   break;
		case LoggingLevel::Info:        androidLogLevel = ANDROID_LOG_INFO;   break;
		case LoggingLevel::Debug:       androidLogLevel = ANDROID_LOG_DEBUG;  break;
	}

	__android_log_print(androidLogLevel, "BleRcuQt", "< M:%.*s F:%.*s L:%d > %s",
	                    64, file ? : "?",
	                    64, func ? : "?",
	                    line,
	                    msg);
}

#endif // defined(Q_OS_ANDROID)

#if defined(RDK)

// -----------------------------------------------------------------------------
/*!
	Writes the log message out to the journald log.

 */
static inline void logToJournald(LoggingLevel level,const char *file,
                                 const char *func, int line, const char *msg,
                                 size_t msgLength)
{
	struct iovec iov[8];
	size_t n = 0;

	iov[n].iov_base = (void*)"SYSLOG_IDENTIFIER=BleRcuDaemon";
	iov[n].iov_len = 30;
	n++;

	switch (level) {
		case LoggingLevel::Fatal:
			iov[n].iov_base = (void*)"PRIORITY=2";
			break;
		case LoggingLevel::Error:
			iov[n].iov_base = (void*)"PRIORITY=3";
			break;
		case LoggingLevel::Warning:
			iov[n].iov_base = (void*)"PRIORITY=4";
			break;
		case LoggingLevel::Milestone:
			iov[n].iov_base = (void*)"PRIORITY=5";
			break;
		case LoggingLevel::Info:
			iov[n].iov_base = (void*)"PRIORITY=6";
			break;
		case LoggingLevel::Debug:
		default:
			iov[n].iov_base = (void*)"PRIORITY=7";
			break;
	}
	iov[n].iov_len = 10;
	n++;

	char msgBuf[128];
	iov[n].iov_base = (void*) msgBuf;
	iov[n].iov_len = snprintf(msgBuf, sizeof(msgBuf), "MESSAGE=%s", msg);
	iov[n].iov_len = qMin<size_t>(iov[n].iov_len, sizeof(msgBuf));
	n++;

	char fileBuf[64];
	if (file) {
		iov[n].iov_base = (void*) fileBuf;
		iov[n].iov_len = snprintf(fileBuf, sizeof(fileBuf), "CODE_FILE=%s", file);
		iov[n].iov_len = qMin<size_t>(iov[n].iov_len, sizeof(fileBuf));
		n++;
	}

	char funcBuf[64];
	if (func) {
		iov[n].iov_base = (void*) funcBuf;
		iov[n].iov_len = snprintf(funcBuf, sizeof(funcBuf), "CODE_FUNC=%s", func);
		iov[n].iov_len = qMin<size_t>(iov[n].iov_len, sizeof(funcBuf));
		n++;
	}

	char lineBuf[32];
	if (line > 0) {
		iov[n].iov_base = (void*) lineBuf;
		iov[n].iov_len = sprintf(lineBuf, "CODE_LINE=%d", line);
		n++;
	}


	int rc = sd_journal_sendv(iov, n);
	if (rc < 0)
		fprintf(stderr, "failed to write log message to journald (%d)", rc);
}

#endif // defined(RDK)

// -----------------------------------------------------------------------------
/*!
	\internal

	Uses a very simple token bucket algorithm to rate limit the log messages
	that go to the prod logs.

	There is one token per second added to the bucket, and each log message
	requires 150 tokens (2m30s of tokens).  The burst size is set to 3000 tokens
	which is 20 log messages.
 */
static bool canWriteProdLog()
{
	// the number of tokens required per message
	static constexpr qint64 tokensPerMessage = 150;

	// the max number of messages that may be sent in a burst
	static constexpr qint64 burstSize = (20 * tokensPerMessage);


	static QElapsedTimer tokenTimer;
	static qint64 tokens = 0;
	static int dropped = 0;

	// lock used just in case multiple threads are writing to the prod logs
	static QMutex lock;
	QMutexLocker locker(&lock);

	if (!tokenTimer.isValid()) {
		tokenTimer.start();
		tokens = burstSize;
	} else {
		// this rate limit will drift due to rounding, but we don't really
		// care for this basic logging case
		const qint64 secsElapsed = (tokenTimer.restart() / 1000);
		tokens = qMin<qint64>(burstSize, (tokens + secsElapsed));
	}

	// if no tokens then drop the message, store the drop count so can log that to
	if (tokens < tokensPerMessage) {
		dropped++;
		return false;
	}

	// remove the tokens
	tokens -= tokensPerMessage;

	// if we've dropped some log messages, then log the fact that it happened
	if (dropped > 0) {
		char buf[96];
		int len = snprintf(buf, sizeof(buf), "dropped %d prodlog messages due"
		                                     " to rate limiting", dropped);

		const LoggingTargets::Int targets = g_logTargets.load();

#if defined(Q_OS_ANDROID)
		if (targets & LoggingTarget::AndroidLog)
			logToAndroidLog(LoggingLevel::Warning, nullptr, nullptr, 0, buf, len);
#else // !defined(Q_OS_ANDROID)
		if (targets & LoggingTarget::EthanLog)
			logToEthanLog(LoggingLevel::Warning, nullptr, nullptr, 0, buf, len);
		if (targets & LoggingTarget::Console)
			logToConsole(LoggingLevel::Warning, nullptr, nullptr, 0, buf, len);
		if (targets & LoggingTarget::SysLog)
			logToSysLog(LoggingLevel::Warning, nullptr, nullptr, 0, buf, len);
#if defined(RDK)
		if (targets & LoggingTarget::Journald)
			logToJournald(LoggingLevel::Warning, nullptr, nullptr, 0, buf, len);
#endif // defined(RDK)
#endif // !defined(Q_OS_ANDROID)

		dropped = 0;
	}

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Simply returns just the name part of a file path.  Returns a nullptr if a
	\a filePath is invalid.

 */
static const char* getFileName(const char *filePath)
{
	if (!filePath)
		return nullptr;

	const char *fileName = strrchr(filePath, '/');
	if (!fileName)
		fileName = filePath;
	else
		fileName++;

	return fileName;
}

// -----------------------------------------------------------------------------
/*!
	Qt logging message handler, installed by the \a setupLogging function.

	This will redirect the message to one or more of the following output
	targets; console, syslog or the ethanlog pipe.

 */
static void messageOutput(QtMsgType type, const QMessageLogContext &context,
                          const QString &msg)
{
	// get the possible log targets
	const LoggingTargets::Int targets = g_logTargets.load();

	// quick check to see if we have any log targets
	if (Q_UNLIKELY(targets == 0))
		return;


	// we strip log context details on prod builds
#if (AI_BUILD_TYPE == AI_DEBUG)
	const char *fileName = getFileName(context.file);
	const char *funcName = context.function;
	const int lineNum = context.line;
#else
	const char *fileName = nullptr;
	const char *funcName = nullptr;
	const int lineNum = 0;
#endif

	LoggingLevel level;


	// prodlogs is a special case, that is always logged regardless of the
	// logging levels or build type (nb: this is deliberately not a string
	// compare, we are intentionally comparing the pointers)
	if (Q_UNLIKELY(context.category == prodlogs().categoryName())) {

		level = LoggingLevel::Milestone;

	} else if (Q_UNLIKELY(context.category == prodlogs_ratelimited().categoryName())) {

		// similar to prodlogs - however limited prod logs are rate limited so
		// we only allow a fixed number of log messages over a given time period
		if (!canWriteProdLog())
			return;

		level = LoggingLevel::Milestone;

	} else {

		// this is deliberately not a string compare, we are intentionally
		// comparing the pointers
		if (context.category == milestone().categoryName()) {
			level = LoggingLevel::Milestone;

		} else {
			// not in the milestone category so use the qt message type
			switch (type) {
				case QtFatalMsg:    level = LoggingLevel::Fatal;    break;
				case QtCriticalMsg: level = LoggingLevel::Error;    break;
				case QtWarningMsg:  level = LoggingLevel::Warning;  break;
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
				case QtInfoMsg:     level = LoggingLevel::Info;     break;
#endif
				case QtDebugMsg:    level = LoggingLevel::Debug;    break;
				default:            level = LoggingLevel::Debug;    break;
			}
		}

		// check this level should be logged
		const LoggingLevels::Int allowedLevels = g_logLevels.load();
		if (!(allowedLevels & level))
			return;
	}

	// convert the Qt string to an ascii string
	const QByteArray localMsg = msg.toLocal8Bit();

#if defined(Q_OS_ANDROID)

	if (targets & LoggingTarget::AndroidLog) {
		logToAndroidLog(level, fileName, funcName, lineNum,
		                localMsg.constData(), localMsg.length());
	}

#else // !defined(Q_OS_ANDROID)

	if (targets & LoggingTarget::EthanLog) {
		logToEthanLog(level, fileName, funcName, lineNum,
		              localMsg.constData(), localMsg.length());
	}

	if (targets & LoggingTarget::Console) {
		logToConsole(level, fileName, funcName, lineNum,
		             localMsg.constData(), localMsg.length());
	}

	if (targets & LoggingTarget::SysLog) {
		logToSysLog(level, fileName, funcName, lineNum,
		            localMsg.constData(), localMsg.length());
	}

#endif // !defined(Q_OS_ANDROID)

#if defined(RDK)

	if (targets & LoggingTarget::Journald) {
		logToJournald(level, fileName, funcName, lineNum,
		              localMsg.constData(), localMsg.length());
	}

#endif // defined(RDK)
}


// -----------------------------------------------------------------------------
/*!
	Sets up the Qt logging handler so that output is directed to one or more
	possible logging targets.

 */
void setupLogging(LoggingTargets targets, LoggingLevels levels)
{
	g_logTargets.store(targets);
	g_logLevels.store(levels);

#if !defined(Q_OS_ANDROID)

	// if logging to syslog
	if (g_logTargets & LoggingTarget::SysLog) {
		openlog("BleRcuDaemon", LOG_CONS | LOG_NDELAY, LOG_DAEMON);
	}

	// if ethanlog is desired we need to check we have the logging pipe and
	// we can access libethanlog.so
	if (g_logTargets & LoggingTarget::EthanLog) {
		if (!setupEthanLogging())
                    g_logTargets = (g_logTargets & ~LoggingTarget::EthanLog);
	}

#endif // !defined(Q_OS_ANDROID)

	// finally install our log handler
	qInstallMessageHandler(messageOutput);
}

// -----------------------------------------------------------------------------
/*!
	Sets the current log levels allowed.

	\warning This function is not thread safe, however do we really care ? it's
	just logging.
 */
void setLogLevels(LoggingLevels levels)
{
	g_logLevels.store(levels);
}

// -----------------------------------------------------------------------------
/*!
	Gets the current log levels allowed.

	\warning This function is not thread safe, however do we really care ? it's
	just logging.
 */
LoggingLevels getLogLevels()
{
	return LoggingLevels(g_logLevels.load());
}

// -----------------------------------------------------------------------------
/*!
	Sets the current log \a targets allowed.

 */
void setLogTargets(LoggingTargets targets)
{
	g_logTargets.store(targets);
}

// -----------------------------------------------------------------------------
/*!
	Gets the current log targets allowed.

 */
LoggingTargets getLogTargets()
{
	return LoggingTargets(g_logTargets.load());
}


// -----------------------------------------------------------------------------
/*!
	Utility function to convert an array of bytes to a string of hex digits.

 */
QString arrayToHex(const quint8 *data, int dataLen)
{
	static const char lookup[] = "0123456789abcdef";
	char buf[256];

	for (int i = 0; i < qMin<int>(dataLen, 126); i++) {
		buf[(i * 2) + 0] = lookup[ (data[i] >> 4) & 0xf ];
		buf[(i * 2) + 1] = lookup[ (data[i] >> 0) & 0xf ];
	}

	if (dataLen > 126) {
		buf[252] = '.';
		buf[253] = '.';
		buf[254] = '.';
		buf[255] = '\0';
		dataLen = 128;
	}

	return QString::fromLocal8Bit(buf, (dataLen * 2));
}

QString arrayToHex(const QByteArray &data)
{
	return arrayToHex(reinterpret_cast<const quint8*>(data.data()), data.length());
}

QString arrayToHex(const QVector<quint8> &data)
{
	return arrayToHex(data.data(), data.size());
}

QString arrayToHex(const QVarLengthArray<quint8> &data)
{
	return arrayToHex(data.data(), data.size());
}

#if QT_VERSION <= QT_VERSION_CHECK(5, 4, 0)
QString QString_asprintf(char const* format, ...)
{
    int len = 0;
    va_list argp;

    va_start(argp, format);
    len = 1 + vsnprintf(NULL, 0, format, argp);
    va_end(argp);

    char* tmpStr = new char[len + 1];

    va_start(argp, format);
    vsnprintf(tmpStr, len, format, argp);
    va_end(argp);

    const QString tmpQStr = QString(tmpStr);
    delete[] tmpStr;

    return tmpQStr; 
}
#endif

