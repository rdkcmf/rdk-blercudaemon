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
//  bleaddress.h
//  SkyBluetoothRcu
//

#ifndef BLEADDRESS_H
#define BLEADDRESS_H

#include <QObject>
#include <QString>
#include <QLatin1String>
#include <QByteArray>
#include <QHash>

#ifdef Q_OS_ANDROID
#  include <QtAndroid>
#  include <QAndroidJniEnvironment>
#endif


class BleAddress
{
public:
	enum AddressOrder {
		LSBOrder,
		MSBOrder,
	};

public:
	BleAddress();
	BleAddress(quint64 address);
	explicit BleAddress(const char *address);
	explicit BleAddress(const quint8 address[6], AddressOrder order = MSBOrder);
	explicit BleAddress(const QString &address);
	explicit BleAddress(QLatin1String address);
	BleAddress(const BleAddress &other);
	~BleAddress();

#ifdef Q_OS_ANDROID
	explicit BleAddress(JNIEnv *env, jstring address);
	explicit BleAddress(JNIEnv *env, jbyteArray address);
#endif

public:
	static void registerType();

public:
	BleAddress &operator=(const BleAddress &rhs);
	BleAddress &operator=(const QLatin1String &rhs);
	BleAddress &operator=(const QString &rhs);

	quint8 operator[](int index) const;


public:
	void clear();
	bool isNull() const;

	quint32 oui() const;

	QString toString() const;
	quint64 toUInt64() const;
	QVarLengthArray<quint8, 6> toArray() const;

#ifdef Q_OS_ANDROID
	jstring toJString(JNIEnv *env = nullptr) const;
	jbyteArray toJByteArray(JNIEnv *env = nullptr) const;
#endif

private:
	friend QDebug operator<<(QDebug dbg, const BleAddress &address);

	friend bool operator<(const BleAddress &bdaddr1, const BleAddress &bdaddr2);
	friend bool operator==(const BleAddress &bdaddr1, const BleAddress &bdaddr2);
	friend bool operator!=(const BleAddress &bdaddr1, const BleAddress &bdaddr2);
	friend uint qHash(const BleAddress &key, uint seed);

	const char* _toString(char buf[32]) const;
	quint64 _fromString(const char *address, int length) const;

private:
	quint64 m_address;
};

Q_DECLARE_METATYPE(BleAddress)

QDebug operator<<(QDebug dbg, const BleAddress &address);


inline bool operator<(const BleAddress &bdaddr1, const BleAddress &bdaddr2)
{
	return bdaddr1.m_address < bdaddr2.m_address;
}

inline bool operator==(const BleAddress &bdaddr1, const BleAddress &bdaddr2)
{
	return bdaddr1.m_address == bdaddr2.m_address;
}

inline bool operator!=(const BleAddress &bdaddr1, const BleAddress &bdaddr2)
{
	return bdaddr1.m_address != bdaddr2.m_address;
}

inline uint qHash(const BleAddress &key, uint seed)
{
	return qHash(key.m_address, seed);
}

#endif // !defined(BLEADDRESS_H)
