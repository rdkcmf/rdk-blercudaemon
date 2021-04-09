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
//  blercuinfrared1_adaptor.h
//  SkyBluetoothRcu
//

#ifndef BLERCUINFRARED1_ADAPTOR_H
#define BLERCUINFRARED1_ADAPTOR_H

#include "dbus/dbusabstractadaptor.h"
#include "blercu/bleservices/blercuinfraredservice.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QSharedPointer>

#include <QtDBus>


class BleRcuDevice;

class BleRcuInfrared1Adaptor : public DBusAbstractAdaptor
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "com.sky.blercu.Infrared1")
	Q_CLASSINFO("D-Bus Introspection", ""
	            "  <interface name=\"com.sky.blercu.Infrared1\">\n"
	            "    <method name=\"GetManufacturers\">\n"
	            "      <arg direction=\"in\" type=\"s\" name=\"search\"/>\n"
	            "      <arg direction=\"in\" type=\"u\" name=\"flags\"/>\n"
	            "      <arg direction=\"in\" type=\"x\" name=\"offset\"/>\n"
	            "      <arg direction=\"in\" type=\"x\" name=\"limit\"/>\n"
	            "      <arg direction=\"out\" type=\"t\" name=\"total_matches\"/>\n"
	            "      <arg direction=\"out\" type=\"as\" name=\"manufacturers\"/>\n"
	            "    </method>\n"
	            "    <method name=\"GetModels\">\n"
	            "      <arg direction=\"in\" type=\"s\" name=\"manufacturer\"/>\n"
	            "      <arg direction=\"in\" type=\"s\" name=\"search\"/>\n"
	            "      <arg direction=\"in\" type=\"u\" name=\"flags\"/>\n"
	            "      <arg direction=\"in\" type=\"x\" name=\"offset\"/>\n"
	            "      <arg direction=\"in\" type=\"x\" name=\"limit\"/>\n"
	            "      <arg direction=\"out\" type=\"t\" name=\"total_matches\"/>\n"
	            "      <arg direction=\"out\" type=\"as\" name=\"models\"/>\n"
	            "    </method>\n"
	            "    <method name=\"GetCodes\">\n"
	            "      <arg direction=\"in\" type=\"s\" name=\"manufacturer\"/>\n"
	            "      <arg direction=\"in\" type=\"s\" name=\"model\"/>\n"
	            "      <arg direction=\"in\" type=\"u\" name=\"flags\"/>\n"
	            "      <arg direction=\"out\" type=\"ai\" name=\"codes\"/>\n"
	            "      <annotation value=\"IrCodeList\" name=\"org.qtproject.QtDBus.QtTypeName.Out0\"/>\n"
	            "    </method>\n"
	            "    <method name=\"GetCodesFromEDID\">\n"
	            "      <arg direction=\"in\" type=\"ay\" name=\"edid\"/>\n"
	            "      <arg direction=\"out\" type=\"ai\" name=\"codes\"/>\n"
	            "      <annotation value=\"IrCodeList\" name=\"org.qtproject.QtDBus.QtTypeName.Out0\"/>\n"
	            "    </method>\n"
	            "  </interface>\n"
	            "")

public:
	BleRcuInfrared1Adaptor(const QSharedPointer<BleRcuDevice> &device,
	                       QObject *parent);
	virtual ~BleRcuInfrared1Adaptor();


public slots:
	void GetCodes(const QString &manufacturer, const QString &model,
	              quint32 flags, const QDBusMessage &message);

	void GetCodesFromEDID(const QByteArray &edid,
	                      const QDBusMessage &message);

	void GetManufacturers(const QString &search, quint32 flags,
	                      qint64 offset, qint64 limit,
	                      const QDBusMessage &message);

	void GetModels(const QString &manufacturer, const QString &search,
	               quint32 flags, qint64 offset, qint64 limit,
	               const QDBusMessage &message);

signals:

private:
	static QList<QVariant> convertSearchResults(const BleRcuInfraredService::SearchResults &results);
	static BleRcuInfraredService::SearchOptions flagsToSearchOptions(quint32 flags);

private:
	const QSharedPointer<BleRcuDevice> m_device;
};

#endif // !defined(BLERCUINFRARED1_ADAPTOR_H)
