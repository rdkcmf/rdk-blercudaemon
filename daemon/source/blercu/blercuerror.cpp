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
//  blercuerror.cpp
//  SkyBluetoothRcu
//

#include "blercuerror.h"


BleRcuError::BleRcuError()
	: m_valid(false)
{
}

BleRcuError::BleRcuError(ErrorType error)
	: m_valid(true)
	, m_code(error)
{
}

BleRcuError::BleRcuError(ErrorType error, const QString &message)
	: m_valid(true)
	, m_code(error)
	, m_message(message)
{
}

BleRcuError::BleRcuError(ErrorType error, const char *message, ...)
	: m_valid(true)
	, m_code(error)
{
	va_list vl;
	va_start(vl, message);
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
	m_message = QString::vasprintf(message, vl);
#else
	int len = 0;
	len = vsnprintf(NULL, 0, message, vl);
	va_end(vl);

	char* tmpStr = new char[len + 1];
	va_start(vl, message);
	vsnprintf(tmpStr, len, message, vl);
	m_message = QString(tmpStr);
	delete[] tmpStr;
#endif
	va_end(vl);
}

BleRcuError::BleRcuError(const BleRcuError &other)
	: m_valid(other.m_valid)
	, m_code(other.m_code)
	, m_message(other.m_message)
{
}

BleRcuError::BleRcuError(BleRcuError &&other)
	: m_valid(other.m_valid)
	, m_code(other.m_code)
	, m_message(std::move(other.m_message))
{
}

BleRcuError &BleRcuError::operator=(const BleRcuError &other)
{
	m_valid = other.m_valid;
	m_code = other.m_code;
	m_message = other.m_message;
	return *this;
}

BleRcuError &BleRcuError::operator=(BleRcuError &&other)
{
	swap(other);
	return *this;
}

void BleRcuError::assign(ErrorType error, const QString &message)
{
	m_valid = true;
	m_code = error;
	m_message = message;
}

BleRcuError::ErrorType BleRcuError::type() const
{
	return m_code;
}

QString BleRcuError::name() const
{
	return errorString(m_code);
}

QString BleRcuError::message() const
{
	return m_message;
}

bool BleRcuError::isValid() const
{
	return m_valid;
}

QString BleRcuError::errorString(ErrorType error)
{
	switch (error) {
		case NoError:
			return QStringLiteral("com.sky.Error.None");
		case General:
			return QStringLiteral("com.sky.Error.Failed");
		case Rejected:
			return QStringLiteral("com.sky.Error.Rejected");
		case Busy:
			return QStringLiteral("com.sky.Error.Busy");
		case IoDevice:
			return QStringLiteral("com.sky.Error.IODevice");
		case InvalidArg:
			return QStringLiteral("com.sky.Error.InvalidArgument");
		case FileNotFound:
			return QStringLiteral("com.sky.Error.FileNotFound");
		case BadFormat:
			return QStringLiteral("com.sky.Error.BadFormat");
		case InvalidHardware:
			return QStringLiteral("com.sky.Error.InvalidHardware");
		case NotImplemented:
			return QStringLiteral("com.sky.Error.NotImplemented");
		case TimedOut:
			return QStringLiteral("com.sky.Error.TimedOut");
		default:
			return QStringLiteral("com.sky.Error.Unknown");
	}
}

QDebug operator<<(QDebug dbg, const BleRcuError &err)
{
	QDebugStateSaver saver(dbg);
	dbg.nospace() << "BleRcuError(" << err.name() << ", " << err.message() << ')';
	return dbg;
}
