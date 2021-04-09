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
//  blercuerror.h
//  SkyBluetoothRcu
//

#ifndef BLERCUERROR_H
#define BLERCUERROR_H

#include <QString>
#include <QDebug>


class BleRcuError
{
public:
	enum ErrorType {
		NoError = 0,
		General,
		Rejected,
		Busy,
		IoDevice,
		InvalidArg,
		FileNotFound,
		BadFormat,
		InvalidHardware,
		NotImplemented,
		TimedOut,

#ifndef Q_QDOC
		// don't use this one!
		LastErrorType = BadFormat
#endif
	};

	BleRcuError();
	BleRcuError(ErrorType error);
	BleRcuError(ErrorType error, const QString &message);
	BleRcuError(ErrorType error, const char *message, ...)
		__attribute__ ((format (printf, 3, 4)));
	BleRcuError(const BleRcuError &other);
	BleRcuError(BleRcuError &&other);

	BleRcuError &operator=(const BleRcuError &other);
	BleRcuError &operator=(BleRcuError &&other);

	explicit operator bool() const Q_DECL_NOTHROW { return (m_code != NoError); }
	bool operator !() const Q_DECL_NOTHROW        { return (m_code == NoError); }

	void assign(ErrorType error, const QString &message = QString());

public:
	void swap(BleRcuError &other)
	{
		qSwap(m_valid,   other.m_valid);
		qSwap(m_code,    other.m_code);
		qSwap(m_message, other.m_message);
	}

	ErrorType type() const;
	QString name() const;
	QString message() const;
	bool isValid() const;

	static QString errorString(ErrorType error);

private:
	bool m_valid;
	ErrorType m_code;
	QString m_message;
};

QDebug operator<<(QDebug, const BleRcuError &);

Q_DECLARE_METATYPE(BleRcuError)

#endif // !defined(BLERCUERROR_H)
