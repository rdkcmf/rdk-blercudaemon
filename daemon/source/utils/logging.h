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
//  logging.h
//  SkyBluetoothRcu
//

#ifndef LOGGING_H
#define LOGGING_H

#include <QFlags>
#include <QString>
#include <QVector>
#include <QByteArray>
#include <QVarLengthArray>

#include <QLoggingCategory>

// [Hack] added for Android studio which seems to have an issue finding the
// atomics for their code editor (not compile issue / just code highlighting
// issue)
#include <QtCore/qflags.h>
#include <QtCore/qbasicatomic.h>


Q_DECLARE_LOGGING_CATEGORY(milestone)
Q_DECLARE_LOGGING_CATEGORY(prodlogs)
Q_DECLARE_LOGGING_CATEGORY(prodlogs_ratelimited)

#if QT_VERSION <= QT_VERSION_CHECK(5, 4, 0)

#ifndef QT_MESSAGELOG_FILE
#define QT_MESSAGELOG_FILE          __FILE__
#define QT_MESSAGELOG_LINE          __LINE__
#define QT_MESSAGELOG_FUNC          __FUNCTION__

#define QtInfoMsg                   QtWarningMsg
#define qInfo                       qWarning
#endif

#define qProdLog \
	QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC, prodlogs().categoryName()).debug

#define qLimitedProdLog \
	QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC, prodlogs_ratelimited().categoryName()).debug

#define qMilestone \
	QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC, milestone().categoryName()).debug

#define qError \
	QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC).critical

#else
#define qProdLog \
	QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC, prodlogs().categoryName()).info

#define qLimitedProdLog \
	QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC, prodlogs_ratelimited().categoryName()).info

#define qMilestone \
	QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC, milestone().categoryName()).info

#define qError \
	QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC).critical
#endif

// the log levels we support
typedef enum {
	Fatal = 0x01,
	Error = 0x02,
	Warning = 0x04,
	Milestone = 0x08,
	Info = 0x10,
	Debug = 0x20
} LoggingLevel;

Q_DECLARE_FLAGS(LoggingLevels, LoggingLevel)
Q_DECLARE_OPERATORS_FOR_FLAGS(LoggingLevels)


// the log targets we support
typedef enum {
#if defined(Q_OS_ANDROID)
	AndroidLog = 0x1,
	Default = AndroidLog,
#else // !defined(Q_OS_ANDROID)
	Console = 0x1,
	Default = Console,
	SysLog = 0x2,
	EthanLog = 0x4,
#endif
#if defined(RDK)
	Journald = 0x8,
#endif
} LoggingTarget;

Q_DECLARE_FLAGS(LoggingTargets, LoggingTarget)
Q_DECLARE_OPERATORS_FOR_FLAGS(LoggingTargets)

extern void setupLogging(LoggingTargets targets, LoggingLevels levels);

extern void setLogLevels(LoggingLevels levels);
extern LoggingLevels getLogLevels();

extern void setLogTargets(LoggingTargets targets);
extern LoggingTargets getLogTargets();



extern QString arrayToHex(const quint8 *data, int dataLen);
extern QString arrayToHex(const QByteArray &data);
extern QString arrayToHex(const QVector<quint8> &data);
extern QString arrayToHex(const QVarLengthArray<quint8> &data);


#if QT_VERSION <= QT_VERSION_CHECK(5, 4, 0)
QString QString_asprintf(char const* format, ...);
#else
#define QString_asprintf(format, ...) QString::asprintf(format, __VA_ARGS__)
#endif


#endif // !defined(LOGGING_H)
